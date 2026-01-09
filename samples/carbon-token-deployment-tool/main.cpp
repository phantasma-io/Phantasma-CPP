#define PHANTASMA_IMPLEMENTATION
#define CURL_STATICLIB

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <rapidjson/document.h>

#include "../../include/Adapters/PhantasmaAPI_rapidjson.h"
#include "../../include/Adapters/PhantasmaAPI_curl.h"
#include "../../include/PhantasmaAPI.h"
#include "../../include/Adapters/PhantasmaAPI_openssl.h"
#include "../../include/Utils/TextUtils.h"
#include "../../include/Utils/RpcUtils.h"
#include "../../include/Carbon/Alloc.h"
#include "../../include/Carbon/Contracts/Token.h"
#include "../../include/Carbon/Tx.h"
#include "../../include/Numerics/Base16.h"

using namespace phantasma;
using namespace phantasma::rpc;
using namespace phantasma::carbon;

struct Args
{
	std::unordered_map<std::string, std::string> values;
	std::unordered_set<std::string> flags;
};

struct Config
{
	std::string rpc;
	std::string nexus;
	std::string wif;
	std::string symbol;
	std::string tokenType;
	std::optional<uint64_t> carbonTokenId;
	std::optional<uint32_t> carbonSeriesId;
	std::optional<intx> tokenMaxSupply;
	std::optional<uint32_t> fungibleDecimals;
	std::string tokenSchemasRaw;
	std::string tokenMetadataRaw;
	std::string seriesMetadataRaw;
	std::string nftMetadataRaw;
	std::optional<uint64_t> createTokenMaxData;
	std::optional<uint64_t> createSeriesMaxData;
	std::optional<uint64_t> mintTokenMaxData;
	std::optional<uint64_t> gasFeeBase;
	std::optional<uint64_t> gasFeeCreateTokenBase;
	std::optional<uint64_t> gasFeeCreateTokenSymbol;
	std::optional<uint64_t> gasFeeCreateTokenSeries;
	std::optional<uint64_t> gasFeeMultiplier;
	bool dryRun = false;
};

static std::string Trim(const std::string& s)
{
	size_t a = 0;
	while (a < s.size() && isspace((unsigned char)s[a]))
	{
		++a;
	}
	size_t b = s.size();
	while (b > a && isspace((unsigned char)s[b - 1]))
	{
		--b;
	}
	return s.substr(a, b - a);
}

static std::string NormalizeRpcHost(const std::string& rpc)
{
	std::string host = Trim(rpc);
	// Allow TS/CS-style configs that include "/rpc"; the C++ API appends "/rpc" itself.
	while (!host.empty() && host.back() == '/')
	{
		host.pop_back();
	}
	const std::string suffix = "/rpc";
	if (host.size() >= suffix.size() && host.compare(host.size() - suffix.size(), suffix.size(), suffix) == 0)
	{
		host.erase(host.size() - suffix.size());
	}
	return host;
}

static Args ParseArgs(int argc, char** argv)
{
	Args out;
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		if (arg.rfind("--", 0) == 0)
		{
			arg = arg.substr(2);
			std::string key = arg;
			std::string value;
			const size_t eq = arg.find('=');
			if (eq != std::string::npos)
			{
				key = arg.substr(0, eq);
				value = arg.substr(eq + 1);
			}
			else if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0)
			{
				value = argv[i + 1];
				++i;
			}
			if (value.empty())
			{
				out.flags.insert(key);
			}
			else
			{
				out.values[key] = value;
			}
		}
		else if (arg == "-c" && i + 1 < argc)
		{
			out.values["config"] = argv[++i];
		}
	}
	return out;
}

static std::unordered_map<std::string, std::string> ParseToml(const std::string& path)
{
	std::unordered_map<std::string, std::string> out;
	std::ifstream file(path);
	if (!file.is_open())
	{
		return out;
	}
	std::string line;
	while (std::getline(file, line))
	{
		std::string trimmed = Trim(line);
		if (trimmed.empty() || trimmed[0] == '#')
		{
			continue;
		}
		const size_t eq = trimmed.find('=');
		if (eq == std::string::npos)
		{
			continue;
		}
		std::string key = Trim(trimmed.substr(0, eq));
		std::string value = Trim(trimmed.substr(eq + 1));
		if (value.rfind("\"\"\"", 0) == 0)
		{
			value = value.substr(3);
			std::string temp;
			while (std::getline(file, temp))
			{
				const size_t end = temp.find("\"\"\"");
				if (end != std::string::npos)
				{
					value.append("\n");
					value.append(temp.substr(0, end));
					break;
				}
				value.append("\n");
				value.append(temp);
			}
		}
		else if (!value.empty() && value.front() == '"' && value.back() == '"')
		{
			value = value.substr(1, value.size() - 2);
		}
		out[key] = value;
	}
	return out;
}

static std::string Pick(const Args& args, const std::unordered_map<std::string, std::string>& toml, const std::string& cliKey, const std::string& tomlKey)
{
	auto it = args.values.find(cliKey);
	if (it != args.values.end())
	{
		return it->second;
	}
	auto it2 = toml.find(tomlKey);
	if (it2 != toml.end())
	{
		return it2->second;
	}
	return {};
}

static bool HasFlag(const Args& args, const std::string& key)
{
	return args.flags.find(key) != args.flags.end();
}

static uint64_t ParseUint64(const std::string& text, const std::string& label)
{
	if (text.empty())
	{
		throw std::runtime_error("Missing numeric value for " + label);
	}
	uint64_t v = 0;
	std::stringstream ss(text);
	if (!(ss >> v))
	{
		throw std::runtime_error("Invalid numeric value for " + label);
	}
	return v;
}

static uint32_t ParseUint32(const std::string& text, const std::string& label)
{
	return (uint32_t)ParseUint64(text, label);
}

static intx ParseIntx(const std::string& text, const std::string& label)
{
	if (text.empty())
	{
		throw std::runtime_error("Missing numeric value for " + label);
	}
	uint32_t radix = 10;
	std::string num = text;
	if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
	{
		radix = 16;
		num = text.substr(2);
	}
	bool err = false;
	intx v = intx::FromString(num.c_str(), (uint32_t)num.size(), radix, &err);
	if (err)
	{
		throw std::runtime_error("Invalid numeric value for " + label);
	}
	return v;
}

static void ParseJson(const std::string& text, const std::string& label, rapidjson::Document& doc)
{
	doc.Parse<rapidjson::kParseDefaultFlags>(text.c_str());
	if (doc.HasParseError())
	{
		std::stringstream ss;
		ss << "Invalid JSON for " << label << " (parse error code " << doc.GetParseError() << ")";
		throw std::runtime_error(ss.str());
	}
}

static MetadataValue ParseMetadataValue(const rapidjson::Value& value, const std::string& path)
{
	if (value.IsString())
	{
		return MetadataValue::FromString(value.GetString());
	}
	if (value.IsBool())
	{
		throw std::runtime_error(path + " must be a string, number, object, or array");
	}
	if (value.IsInt64())
	{
		return MetadataValue::FromInt64(value.GetInt64());
	}
	if (value.IsUint64())
	{
		return MetadataValue::FromUInt64(value.GetUint64());
	}
	if (value.IsInt())
	{
		return MetadataValue::FromInt64(value.GetInt());
	}
	if (value.IsUint())
	{
		return MetadataValue::FromUInt64(value.GetUint());
	}
	if (value.IsDouble())
	{
		throw std::runtime_error(path + " must be an integer");
	}
	if (value.IsArray())
	{
		std::vector<MetadataValue> items;
		items.reserve(value.Size());
		for (rapidjson::SizeType i = 0; i < value.Size(); ++i)
		{
			const std::string childPath = path + "[" + std::to_string(i) + "]";
			items.push_back(ParseMetadataValue(value[i], childPath));
		}
		return MetadataValue::FromArray(items);
	}
	if (value.IsObject())
	{
		std::vector<std::pair<std::string, MetadataValue>> fields;
		for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it)
		{
			const std::string name = it->name.GetString();
			fields.push_back({ name, ParseMetadataValue(it->value, path + "." + name) });
		}
		return MetadataValue::FromStruct(fields);
	}

	throw std::runtime_error(path + " must be a string, number, object, or array");
}

static std::vector<MetadataField> ParseMetadataFields(const std::string& text, const std::string& label)
{
	rapidjson::Document doc;
	ParseJson(text, label, doc);

	std::vector<MetadataField> fields;
	if (doc.IsObject())
	{
		for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it)
		{
			const std::string name = it->name.GetString();
			if (name.empty())
			{
				throw std::runtime_error(label + ": metadata field name cannot be empty");
			}
			fields.push_back(MetadataField{ name, ParseMetadataValue(it->value, label + "." + name) });
		}
		return fields;
	}

	if (doc.IsArray())
	{
		fields.reserve(doc.Size());
		for (rapidjson::SizeType i = 0; i < doc.Size(); ++i)
		{
			const rapidjson::Value& entry = doc[i];
			if (!entry.IsObject() || !entry.HasMember("name") || !entry.HasMember("value") || !entry["name"].IsString())
			{
				throw std::runtime_error(label + "[" + std::to_string(i) + "] must be an object with name/value");
			}
			const std::string name = entry["name"].GetString();
			if (name.empty())
			{
				throw std::runtime_error(label + "[" + std::to_string(i) + "]: metadata field name cannot be empty");
			}
			fields.push_back(MetadataField{ name, ParseMetadataValue(entry["value"], label + "[" + std::to_string(i) + "]." + name) });
		}
		return fields;
	}

	throw std::runtime_error(label + " must be a JSON object or array");
}

static TokenSchemasOwned ParseTokenSchemas(const std::string& text)
{
#ifdef PHANTASMA_RAPIDJSON
	return TokenSchemasBuilder::FromJson(text);
#else
	(void)text;
	throw std::runtime_error("token_schemas parsing requires PHANTASMA_RAPIDJSON");
#endif
}

static std::string IdToStringUnsigned(const uint256& id)
{
	return intx(id).ToStringUnsigned();
}

static std::string BytesToHex(const ByteArray& b)
{
	if (b.empty())
	{
		return {};
	}
	const String s = Base16::Encode(&b.front(), (int)b.size(), false);
	return std::string(s.begin(), s.end());
}

static std::vector<std::pair<std::string, std::string>> ParseTokenMetadata(const std::string& text)
{
	rapidjson::Document doc;
	ParseJson(text, "token_metadata", doc);
	if (!doc.IsObject())
	{
		throw std::runtime_error("token_metadata must be a JSON object");
	}
	std::vector<std::pair<std::string, std::string>> meta;
	for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it)
	{
		if (!it->value.IsString())
		{
			throw std::runtime_error("Token metadata values must be strings");
		}
		meta.push_back({ it->name.GetString(), it->value.GetString() });
	}
	return meta;
}

static Config LoadConfig(const Args& args)
{
	std::string configPath = Pick(args, {}, "config", "config");
	if (configPath.empty())
	{
		configPath = "config.toml";
	}
	const auto toml = ParseToml(configPath);

	Config cfg{};
	cfg.rpc = Pick(args, toml, "rpc", "rpc");
	if (cfg.rpc.empty())
	{
		cfg.rpc = "https://testnet.phantasma.info/rpc";
	}
	cfg.rpc = NormalizeRpcHost(cfg.rpc);
	cfg.nexus = Pick(args, toml, "nexus", "nexus");
	cfg.wif = Pick(args, toml, "wif", "wif");
	cfg.symbol = Pick(args, toml, "symbol", "symbol");
	cfg.tokenType = Pick(args, toml, "token-type", "token_type");
	std::string tokenMax = Pick(args, toml, "token-max-supply", "token_max_supply");
	if (tokenMax.empty())
	{
		tokenMax = Pick(args, toml, "fungible-max-supply", "fungible_max_supply");
	}
	if (!tokenMax.empty()) cfg.tokenMaxSupply = ParseIntx(tokenMax, "token_max_supply");
	std::string decimals = Pick(args, toml, "fungible-decimals", "fungible_decimals");
	if (!decimals.empty()) cfg.fungibleDecimals = ParseUint32(decimals, "fungible_decimals");
	std::string carbonId = Pick(args, toml, "carbon-token-id", "carbon_token_id");
	if (!carbonId.empty()) cfg.carbonTokenId = ParseUint64(carbonId, "carbon_token_id");
	std::string seriesId = Pick(args, toml, "carbon-token-series-id", "carbon_token_series_id");
	if (!seriesId.empty()) cfg.carbonSeriesId = ParseUint32(seriesId, "carbon_token_series_id");
	cfg.tokenSchemasRaw = Pick(args, toml, "token-schemas", "token_schemas");
	cfg.tokenMetadataRaw = Pick(args, toml, "token-metadata", "token_metadata");
	cfg.seriesMetadataRaw = Pick(args, toml, "series-metadata", "series_metadata");
	cfg.nftMetadataRaw = Pick(args, toml, "nft-metadata", "nft_metadata");
	std::string createTokenMax = Pick(args, toml, "create-token-max-data", "create_token_max_data");
	if (!createTokenMax.empty()) cfg.createTokenMaxData = ParseUint64(createTokenMax, "create_token_max_data");
	std::string createSeriesMax = Pick(args, toml, "create-token-series-max-data", "create_token_series_max_data");
	if (!createSeriesMax.empty()) cfg.createSeriesMaxData = ParseUint64(createSeriesMax, "create_token_series_max_data");
	std::string mintMax = Pick(args, toml, "mint-token-max-data", "mint_token_max_data");
	if (!mintMax.empty()) cfg.mintTokenMaxData = ParseUint64(mintMax, "mint_token_max_data");
	std::string gasBase = Pick(args, toml, "gas-fee-base", "gas_fee_base");
	if (!gasBase.empty()) cfg.gasFeeBase = ParseUint64(gasBase, "gas_fee_base");
	std::string gasCreateTokenBase = Pick(args, toml, "gas-fee-create-token-base", "gas_fee_create_token_base");
	if (!gasCreateTokenBase.empty()) cfg.gasFeeCreateTokenBase = ParseUint64(gasCreateTokenBase, "gas_fee_create_token_base");
	std::string gasCreateTokenSymbol = Pick(args, toml, "gas-fee-create-token-symbol", "gas_fee_create_token_symbol");
	if (!gasCreateTokenSymbol.empty()) cfg.gasFeeCreateTokenSymbol = ParseUint64(gasCreateTokenSymbol, "gas_fee_create_token_symbol");
	std::string gasCreateSeries = Pick(args, toml, "gas-fee-create-token-series", "gas_fee_create_token_series");
	if (!gasCreateSeries.empty()) cfg.gasFeeCreateTokenSeries = ParseUint64(gasCreateSeries, "gas_fee_create_token_series");
	std::string gasMult = Pick(args, toml, "gas-fee-multiplier", "gas_fee_multiplier");
	if (!gasMult.empty()) cfg.gasFeeMultiplier = ParseUint64(gasMult, "gas_fee_multiplier");
	cfg.dryRun = HasFlag(args, "dry-run") || (toml.find("dry_run") != toml.end() && toml.at("dry_run") == "true");

	return cfg;
}

static bool WaitForTx(PhantasmaAPI& api, const std::string& hash, std::string& outResult)
{
	const auto start = std::chrono::steady_clock::now();
	const auto timeout = std::chrono::seconds(30);
	while (std::chrono::steady_clock::now() - start < timeout)
	{
		rpc::Transaction tx;
		PhantasmaError err;
		const TransactionState state = CheckConfirmation(api, hash.c_str(), tx, err);
		if (state == TransactionState::Confirmed)
		{
			outResult = tx.result;
			return true;
		}
		if (state == TransactionState::Rejected)
		{
			const std::string stateText = tx.state.empty() ? "Rejected" : std::string(tx.state.begin(), tx.state.end());
			const std::string resultText = tx.result.empty()
				? std::string(err.message.begin(), err.message.end())
				: std::string(tx.result.begin(), tx.result.end());
			const std::string debugText = tx.debugComment.empty()
				? std::string()
				: std::string(tx.debugComment.begin(), tx.debugComment.end());
			std::cout << "Transaction failed: " << stateText
				<< " result: '" << resultText
				<< "' debugComment: '" << debugText << "'" << std::endl;
			return false;
		}
		if (state == TransactionState::Unknown)
		{
			std::cout << "Polling error (retrying): " << err.message << std::endl;
		}
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}
	std::cout << "Timed out while waiting for tx confirmation" << std::endl;
	return false;
}

static void Ensure(bool condition, const std::string& message)
{
	if (!condition)
	{
		throw std::runtime_error(message);
	}
}

static void RunCreateToken(const Config& cfg)
{
	Ensure(!cfg.rpc.empty(), "rpc is required");
	Ensure(!cfg.nexus.empty(), "nexus is required");
	Ensure(!cfg.wif.empty(), "wif is required");
	Ensure(!cfg.symbol.empty(), "symbol is required");
	Ensure(!cfg.tokenType.empty(), "token_type is required");
	Ensure(cfg.gasFeeBase.has_value(), "gas_fee_base is required");
	Ensure(cfg.gasFeeCreateTokenBase.has_value(), "gas_fee_create_token_base is required");
	Ensure(cfg.gasFeeCreateTokenSymbol.has_value(), "gas_fee_create_token_symbol is required");
	Ensure(cfg.gasFeeMultiplier.has_value(), "gas_fee_multiplier is required");
	Ensure(cfg.createTokenMaxData.has_value(), "create_token_max_data is required");
	Ensure(!cfg.tokenMetadataRaw.empty(), "token_metadata is required");

	std::string tokenType = cfg.tokenType;
	std::transform(tokenType.begin(), tokenType.end(), tokenType.begin(), [](unsigned char c) { return (char)tolower(c); });
	Ensure(tokenType == "fungible" || tokenType == "nft", "token_type must be 'fungible' or 'nft'");
	const bool isFungible = tokenType == "fungible";

	if (isFungible)
	{
		Ensure(cfg.tokenMaxSupply.has_value(), "token_max_supply is required for fungible tokens");
		Ensure(cfg.fungibleDecimals.has_value(), "fungible_decimals is required for fungible tokens");
		Ensure(cfg.fungibleDecimals.value() <= 255, "fungible_decimals must be <= 255");
	}
	if (cfg.tokenMaxSupply.has_value() && cfg.tokenMaxSupply->Int256().IsNegative())
	{
		throw std::runtime_error("token_max_supply must be non-negative");
	}

	PhantasmaKeys keys = PhantasmaKeys::FromWIF(cfg.wif.c_str(), (int)cfg.wif.size());
	const Bytes32 owner(keys.GetPublicKey());

	const String ownerText = keys.ToString();
	std::cout << "Deploying token with owner " << std::string(ownerText.begin(), ownerText.end()) << std::endl;

	TokenSchemasOwned schemasOwned;
	if (!isFungible)
	{
		Ensure(!cfg.tokenSchemasRaw.empty(), "token_schemas is required for NFT tokens");
		schemasOwned = ParseTokenSchemas(cfg.tokenSchemasRaw);
	}

	const intx maxSupply = cfg.tokenMaxSupply.has_value() ? cfg.tokenMaxSupply.value() : intx::Zero();
	const uint8_t decimals = isFungible ? (uint8_t)cfg.fungibleDecimals.value() : 0;

	std::vector<std::pair<std::string, std::string>> tokenMetadata = ParseTokenMetadata(cfg.tokenMetadataRaw);
	const ByteArray tokenMetadataBytes = TokenMetadataBuilder::BuildAndSerialize(tokenMetadata);
	ByteArray schemasBytes;
	if (!isFungible)
	{
		schemasBytes = TokenSchemasBuilder::BuildAndSerialize(&schemasOwned.view);
	}

	TokenInfoOwned tokenInfoOwned = TokenInfoBuilder::Build(
		cfg.symbol,
		maxSupply,
		!isFungible,
		decimals,
		owner,
		tokenMetadataBytes,
		isFungible ? nullptr : &schemasBytes);

	const CreateTokenFeeOptions feeOptions(
		cfg.gasFeeBase.value(),
		cfg.gasFeeCreateTokenBase.value(),
		cfg.gasFeeCreateTokenSymbol.value(),
		cfg.gasFeeMultiplier.value());

	const TxEnvelope tx = CreateTokenTxHelper::BuildTx(tokenInfoOwned.View(), owner, &feeOptions, cfg.createTokenMaxData.value());

	if (cfg.dryRun)
	{
		const ByteArray signedBytes = SignAndSerialize(tx, keys);
		const std::string txHex = BytesToHex(signedBytes);
		std::cout << "[dry-run] Prepared tx: " << txHex << std::endl;
		return;
	}

	CurlClient http(cfg.rpc);
	PhantasmaAPI api(http);
	PhantasmaError err;
	const String hash = SignAndSendCarbonTransaction(api, tx.msg, keys, &err);
	Ensure(err.code == 0, "Failed to send transaction: " + err.message);
	std::cout << "txHash: " << hash << std::endl;

	std::string result;
	if (WaitForTx(api, hash.c_str(), result))
	{
		const uint32_t carbonId = CreateTokenTxHelper::ParseResult(result);
		std::cout << "Deployed carbon token ID: " << carbonId << std::endl;
	}
}

static void RunCreateSeries(const Config& cfg)
{
	Ensure(!cfg.rpc.empty(), "rpc is required");
	Ensure(!cfg.nexus.empty(), "nexus is required");
	Ensure(!cfg.wif.empty(), "wif is required");
	Ensure(cfg.carbonTokenId.has_value(), "carbon_token_id is required");
	Ensure(cfg.gasFeeBase.has_value(), "gas_fee_base is required");
	Ensure(cfg.gasFeeCreateTokenSeries.has_value(), "gas_fee_create_token_series is required");
	Ensure(cfg.gasFeeMultiplier.has_value(), "gas_fee_multiplier is required");
	Ensure(cfg.createSeriesMaxData.has_value(), "create_token_series_max_data is required");
	Ensure(!cfg.tokenSchemasRaw.empty(), "token_schemas is required");
	Ensure(!cfg.seriesMetadataRaw.empty(), "series_metadata is required");

	PhantasmaKeys keys = PhantasmaKeys::FromWIF(cfg.wif.c_str(), (int)cfg.wif.size());
	const Bytes32 owner(keys.GetPublicKey());

	TokenSchemasOwned schemasOwned = ParseTokenSchemas(cfg.tokenSchemasRaw);
	const std::vector<MetadataField> seriesMetadata = ParseMetadataFields(cfg.seriesMetadataRaw, "series_metadata");
	const uint256 seriesId = IdHelper::GetRandomPhantasmaId();

	std::cout << "Creating new series '" << IdToStringUnsigned(seriesId) << "'" << std::endl;

	const SeriesInfoOwned seriesInfoOwned = SeriesInfoBuilder::Build(schemasOwned.view.seriesMetadata, seriesId, 0, 0, owner, seriesMetadata);

	const CreateSeriesFeeOptions feeOptions(
		cfg.gasFeeBase.value(),
		cfg.gasFeeCreateTokenSeries.value(),
		cfg.gasFeeMultiplier.value());

	const TxEnvelope tx = CreateTokenSeriesTxHelper::BuildTx(cfg.carbonTokenId.value(), seriesInfoOwned.View(), owner, &feeOptions, cfg.createSeriesMaxData.value());

	if (cfg.dryRun)
	{
		const ByteArray signedBytes = SignAndSerialize(tx, keys);
		const std::string txHex = BytesToHex(signedBytes);
		std::cout << "[dry-run] Prepared tx: " << txHex << std::endl;
		return;
	}

	CurlClient http(cfg.rpc);
	PhantasmaAPI api(http);
	PhantasmaError err;
	const String hash = SignAndSendCarbonTransaction(api, tx.msg, keys, &err);
	Ensure(err.code == 0, "Failed to send transaction: " + err.message);
	std::cout << "txHash: " << hash << std::endl;

	std::string result;
	if (WaitForTx(api, hash.c_str(), result))
	{
		const uint32_t carbonSeriesId = CreateTokenSeriesTxHelper::ParseResult(result);
		std::cout << "Deployed carbon series ID: " << carbonSeriesId << std::endl;
	}
}

static void RunMintNft(const Config& cfg)
{
	Ensure(!cfg.rpc.empty(), "rpc is required");
	Ensure(!cfg.nexus.empty(), "nexus is required");
	Ensure(!cfg.wif.empty(), "wif is required");
	Ensure(cfg.carbonTokenId.has_value(), "carbon_token_id is required");
	Ensure(cfg.carbonSeriesId.has_value(), "carbon_token_series_id is required");
	Ensure(cfg.gasFeeBase.has_value(), "gas_fee_base is required");
	Ensure(cfg.gasFeeMultiplier.has_value(), "gas_fee_multiplier is required");
	Ensure(cfg.mintTokenMaxData.has_value(), "mint_token_max_data is required");
	Ensure(!cfg.tokenSchemasRaw.empty(), "token_schemas is required");
	Ensure(!cfg.nftMetadataRaw.empty(), "nft_metadata is required");

	TokenSchemasOwned schemasOwned = ParseTokenSchemas(cfg.tokenSchemasRaw);

	PhantasmaKeys keys = PhantasmaKeys::FromWIF(cfg.wif.c_str(), (int)cfg.wif.size());
	const Bytes32 owner(keys.GetPublicKey());

	const std::vector<MetadataField> nftMetadata = ParseMetadataFields(cfg.nftMetadataRaw, "nft_metadata");
	const uint256 nftId = IdHelper::GetRandomPhantasmaId();

	std::cout << "Minting NFT with phantasma ID " << IdToStringUnsigned(nftId) << std::endl;

	const ByteArray rom = NftRomBuilder::BuildAndSerialize(schemasOwned.view.rom, nftId, nftMetadata);

	const MintNftFeeOptions feeOptions(cfg.gasFeeBase.value(), cfg.gasFeeMultiplier.value());
	const TxEnvelope tx = MintNonFungibleTxHelper::BuildTx(
		cfg.carbonTokenId.value(),
		cfg.carbonSeriesId.value(),
		owner,
		owner,
		rom,
		ByteArray{},
		&feeOptions,
		cfg.mintTokenMaxData.value());

	if (cfg.dryRun)
	{
		const ByteArray signedBytes = SignAndSerialize(tx, keys);
		const std::string txHex = BytesToHex(signedBytes);
		std::cout << "[dry-run] Prepared tx: " << txHex << std::endl;
		return;
	}

	CurlClient http(cfg.rpc);
	PhantasmaAPI api(http);
	PhantasmaError err;
	const String hash = SignAndSendCarbonTransaction(api, tx.msg, keys, &err);
	Ensure(err.code == 0, "Failed to send transaction: " + err.message);
	std::cout << "txHash: " << hash << std::endl;

	std::string result;
	if (WaitForTx(api, hash.c_str(), result))
	{
		const auto addresses = MintNonFungibleTxHelper::ParseResult(cfg.carbonTokenId.value(), result);
		if (!addresses.empty())
		{
			const String addr = Base16::Encode(addresses[0].bytes, (int)Bytes32::length, false);
			std::cout << "Deployed NFT with phantasma ID " << IdToStringUnsigned(nftId)
				<< " and carbon NFT address " << std::string(addr.begin(), addr.end()) << std::endl;
		}
	}
}

int main(int argc, char** argv)
{
	try
	{
		const Args args = ParseArgs(argc, argv);
		const Config cfg = LoadConfig(args);

		if (HasFlag(args, "create-token"))
		{
			RunCreateToken(cfg);
			return 0;
		}
		if (HasFlag(args, "create-series"))
		{
			RunCreateSeries(cfg);
			return 0;
		}
		if (HasFlag(args, "mint-nft"))
		{
			RunMintNft(cfg);
			return 0;
		}

		std::cout << "Usage: carbon-token-deployment-tool-cpp [--config path] --create-token|--create-series|--mint-nft [options]" << std::endl;
		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}
}
