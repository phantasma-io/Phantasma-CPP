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

static ByteArray DecodeHex(const std::string& hex, const std::string& label)
{
	std::string trimmed = Trim(hex);
	if (trimmed.empty())
	{
		throw std::runtime_error("Invalid hex for " + label);
	}
	if (trimmed.rfind("0x", 0) == 0 || trimmed.rfind("0X", 0) == 0)
	{
		trimmed = trimmed.substr(2);
	}
	if (trimmed.empty())
	{
		return {};
	}
	if ((trimmed.size() % 2) != 0)
	{
		throw std::runtime_error("Invalid hex for " + label + " (length must be even)");
	}
	try
	{
		return Base16::Decode(trimmed.c_str(), (int)trimmed.size());
	}
	catch (...)
	{
		throw std::runtime_error("Invalid hex for " + label);
	}
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

static void CloneValue(const rapidjson::Value& src, rapidjson::Value& dst, rapidjson::Document::AllocatorType& alloc)
{
	if (src.IsObject())
	{
		dst.SetObject();
		for (auto it = src.MemberBegin(); it != src.MemberEnd(); ++it)
		{
			rapidjson::Value name(it->name.GetString(), it->name.GetStringLength(), alloc);
			rapidjson::Value value;
			CloneValue(it->value, value, alloc);
			dst.AddMember(name, value, alloc);
		}
		return;
	}

	if (src.IsArray())
	{
		dst.SetArray();
		for (auto it = src.Begin(); it != src.End(); ++it)
		{
			rapidjson::Value value;
			CloneValue(*it, value, alloc);
			dst.PushBack(value, alloc);
		}
		return;
	}

	if (src.IsString()) { dst.SetString(src.GetString(), src.GetStringLength(), alloc); return; }
	if (src.IsBool()) { dst.SetBool(src.GetBool()); return; }
	if (src.IsInt()) { dst.SetInt(src.GetInt()); return; }
	if (src.IsUint()) { dst.SetUint(src.GetUint()); return; }
	if (src.IsInt64()) { dst.SetInt64(src.GetInt64()); return; }
	if (src.IsUint64()) { dst.SetUint64(src.GetUint64()); return; }
	if (src.IsDouble()) { dst.SetDouble(src.GetDouble()); return; }
	dst.SetNull();
}

static void NormalizeMetadataJson(const std::string& text, const std::string& label, rapidjson::Document& out)
{
	out.SetObject();
	if (text.empty())
	{
		return;
	}

	rapidjson::Document parsed;
	ParseJson(text, label, parsed);
	auto& alloc = out.GetAllocator();

	if (parsed.IsObject())
	{
		for (auto it = parsed.MemberBegin(); it != parsed.MemberEnd(); ++it)
		{
			rapidjson::Value name(it->name.GetString(), it->name.GetStringLength(), alloc);
			rapidjson::Value value;
			CloneValue(it->value, value, alloc);
			out.AddMember(name, value, alloc);
		}
		return;
	}

	if (parsed.IsArray())
	{
		for (auto it = parsed.Begin(); it != parsed.End(); ++it)
		{
			const rapidjson::Value& entry = *it;
			if (!entry.IsObject() || !entry.HasMember("name") || !entry.HasMember("value") || !entry["name"].IsString())
			{
				throw std::runtime_error("Metadata array entry must be an object with name/value");
			}
			const char* key = entry["name"].GetString();
			rapidjson::Value name(key, (rapidjson::SizeType)strlen(key), alloc);
			rapidjson::Value value;
			CloneValue(entry["value"], value, alloc);
			out.AddMember(name, value, alloc);
		}
		return;
	}

	throw std::runtime_error("Metadata for " + label + " must be an object or array");
}

static TokenSchemasOwned ParseTokenSchemas(const std::string& text)
{
	rapidjson::Document doc;
	ParseJson(text, "token_schemas", doc);
	if (!doc.IsObject())
	{
		throw std::runtime_error("token_schemas must be a JSON object");
	}

	auto parseArray = [&](const char* key) -> std::vector<FieldType>
	{
		if (!doc.HasMember(key) || !doc[key].IsArray())
		{
			throw std::runtime_error(std::string(key) + " must be an array");
		}
		std::vector<FieldType> fields;
		for (auto it = doc[key].Begin(); it != doc[key].End(); ++it)
		{
			const rapidjson::Value& v = *it;
			if (!v.IsObject() || !v.HasMember("name") || !v.HasMember("type") || !v["name"].IsString() || !v["type"].IsString())
			{
				throw std::runtime_error(std::string(key) + " entries must contain name and type");
			}
			bool error = false;
			const VmType vmType = VmTypeFromString(v["type"].GetString(), &error);
			if (error)
			{
				throw std::runtime_error("Unknown VmType: " + std::string(v["type"].GetString()));
			}
			fields.push_back(FieldType{ v["name"].GetString(), vmType });
		}
		return fields;
	};

	std::vector<FieldType> seriesFields = parseArray("seriesMetadata");
	std::vector<FieldType> romFields = parseArray("rom");
	std::vector<FieldType> ramFields = parseArray("ram");

	return TokenSchemasBuilder::BuildFromFields(seriesFields, romFields, ramFields);
}

static intx GenerateRandomId()
{
	uint8_t bytes[32];
	CryptoRandomBuffer(bytes, sizeof(bytes));
	return intx::FromBytes(ByteView{ bytes, sizeof(bytes) }, false);
}

static std::string IdToString(const intx& id)
{
	return id.ToString();
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

static std::string ValueToString(const rapidjson::Value& v)
{
	if (v.IsString())
	{
		return v.GetString();
	}
	if (v.IsNumber())
	{
		std::ostringstream ss;
		if (v.IsUint64()) ss << v.GetUint64();
		else if (v.IsInt64()) ss << v.GetInt64();
		else if (v.IsUint()) ss << v.GetUint();
		else if (v.IsInt()) ss << v.GetInt();
		else ss << v.GetDouble();
		return ss.str();
	}
	throw std::runtime_error("Expected string or number value");
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

static bool EqualsIgnoreCaseStr(const std::string& a, const std::string& b)
{
	if (a.size() != b.size())
	{
		return false;
	}
	for (size_t i = 0; i != a.size(); ++i)
	{
		if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
		{
			return false;
		}
	}
	return true;
}

static const rapidjson::Value* FindField(const rapidjson::Value& container, const std::string& key)
{
	if (container.IsObject())
	{
		for (auto it = container.MemberBegin(); it != container.MemberEnd(); ++it)
		{
			const std::string name = it->name.GetString();
			if (name == key)
			{
				return &it->value;
			}
		}
		for (auto it = container.MemberBegin(); it != container.MemberEnd(); ++it)
		{
			const std::string name = it->name.GetString();
			if (EqualsIgnoreCaseStr(name, key))
			{
				throw std::runtime_error("Case mismatch for metadata field " + key + ", found " + name);
			}
		}
	}
	else if (container.IsArray())
	{
		// Search from the end so defaults appended later take precedence.
		for (auto it = container.End(); it != container.Begin();)
		{
			--it;
			const rapidjson::Value& entry = *it;
			if (!entry.IsObject() || !entry.HasMember("name") || !entry.HasMember("value") || !entry["name"].IsString())
			{
				continue;
			}
			const std::string name = entry["name"].GetString();
			if (name == key)
			{
				return &entry["value"];
			}
		}
		for (auto it = container.End(); it != container.Begin();)
		{
			--it;
			const rapidjson::Value& entry = *it;
			if (!entry.IsObject() || !entry.HasMember("name") || !entry.HasMember("value") || !entry["name"].IsString())
			{
				continue;
			}
			const std::string name = entry["name"].GetString();
			if (EqualsIgnoreCaseStr(name, key))
			{
				throw std::runtime_error("Case mismatch for metadata field " + key + ", found " + name);
			}
		}
	}
	return nullptr;
}

static VmDynamicVariable BuildValue(const VmVariableSchema& schema, const rapidjson::Value& value, Allocator& alloc);

static VmDynamicStruct BuildStruct(const VmStructSchema& schema, const rapidjson::Value& json, Allocator& alloc)
{
	if (!json.IsObject())
	{
		throw std::runtime_error("Metadata entry must be an object");
	}
	VmDynamicStruct s{};
	s.numFields = schema.numFields;
	s.fields = alloc.Alloc<VmNamedDynamicVariable>(schema.numFields);
	for (uint32_t i = 0; i != schema.numFields; ++i)
	{
		const SmallString& fname = schema.fields[i].name;
		const std::string key = fname.c_str();
		const rapidjson::Value* v = FindField(json, key);
		if (!v)
		{
			throw std::runtime_error("Missing metadata field " + key);
		}
		s.fields[i].name = fname;
		s.fields[i].value = BuildValue(schema.fields[i].schema, *v, alloc);
	}
	if (s.numFields > 1)
	{
		std::sort(s.fields, s.fields + s.numFields, NameLessThan<VmNamedDynamicVariable>{});
	}
	return s;
}

static ByteView CopyBytes(const ByteArray& source, Allocator& alloc)
{
	if (source.empty())
	{
		return ByteView{ nullptr, 0 };
	}
	Byte* dst = alloc.Alloc<Byte>(source.size());
	memcpy(dst, source.data(), source.size());
	return ByteView{ dst, source.size() };
}

static const char* CopyString(const std::string& src, Allocator& alloc)
{
	char* dst = alloc.Alloc<char>(src.size() + 1);
	memcpy(dst, src.c_str(), src.size());
	dst[src.size()] = 0;
	return dst;
}

static VmDynamicVariable BuildScalar(VmType type, const VmStructSchema* schema, const rapidjson::Value& value, Allocator& alloc)
{
	switch (type)
	{
	case VmType::String:
	{
		if (!value.IsString())
		{
			throw std::runtime_error("Expected string value");
		}
		const std::string text = value.GetString();
		VmDynamicVariable v(CopyString(text, alloc));
		return v;
	}
	case VmType::Int8:
	case VmType::Int16:
	case VmType::Int32:
	case VmType::Int64:
	{
		const bool isSigned = value.IsInt64();
		const bool isUnsigned = value.IsUint64();
		if (!isSigned && !isUnsigned)
		{
			throw std::runtime_error("Expected integer value");
		}
		if (type == VmType::Int8)
		{
			if (isSigned)
			{
				const int64_t num = value.GetInt64();
				if (num < -0x80 || num > 0x7F) throw std::runtime_error("Int8 out of range");
				return VmDynamicVariable((int8_t)num);
			}
			const uint64_t num = value.GetUint64();
			if (num > 0xFF) throw std::runtime_error("Int8 out of range");
			return VmDynamicVariable((uint8_t)num);
		}
		if (type == VmType::Int16)
		{
			if (isSigned)
			{
				const int64_t num = value.GetInt64();
				if (num < -0x8000 || num > 0x7FFF) throw std::runtime_error("Int16 out of range");
				return VmDynamicVariable((int16_t)num);
			}
			const uint64_t num = value.GetUint64();
			if (num > 0xFFFF) throw std::runtime_error("Int16 out of range");
			return VmDynamicVariable((uint16_t)num);
		}
		if (type == VmType::Int32)
		{
			if (isSigned)
			{
				const int64_t num = value.GetInt64();
				if (num < INT32_MIN || num > INT32_MAX) throw std::runtime_error("Int32 out of range");
				return VmDynamicVariable((int32_t)num);
			}
			const uint64_t num = value.GetUint64();
			if (num > 0xFFFFFFFFull) throw std::runtime_error("Int32 out of range");
			return VmDynamicVariable((uint32_t)num);
		}
		if (isSigned)
		{
			return VmDynamicVariable((int64_t)value.GetInt64());
		}
		return VmDynamicVariable((uint64_t)value.GetUint64());
	}
	case VmType::Int256:
	{
		if (!value.IsString() && !value.IsNumber())
		{
			throw std::runtime_error("Expected numeric string for Int256");
		}
		const std::string text = value.IsString() ? value.GetString() : ValueToString(value);
		const intx v = ParseIntx(text, "metadata");
		return VmDynamicVariable(v.Int256());
	}
	case VmType::Bytes:
	{
		if (!value.IsString())
		{
			throw std::runtime_error("Bytes fields must be hex strings");
		}
		const ByteArray decoded = DecodeHex(value.GetString(), "metadata bytes");
		VmDynamicVariable v;
		v.type = VmType::Bytes;
		v.arrayLength = 1;
		v.data.bytes = CopyBytes(decoded, alloc);
		return v;
	}
	case VmType::Bytes16:
	case VmType::Bytes32:
	case VmType::Bytes64:
	{
		if (!value.IsString())
		{
			throw std::runtime_error("Fixed bytes fields must be hex strings");
		}
		const ByteArray decoded = DecodeHex(value.GetString(), "metadata bytes");
		const size_t expected = type == VmType::Bytes16 ? 16 : (type == VmType::Bytes32 ? 32 : 64);
		if (decoded.size() != expected)
		{
			throw std::runtime_error("Fixed bytes length mismatch");
		}
		if (type == VmType::Bytes16) return VmDynamicVariable(Bytes16(View(decoded)));
		if (type == VmType::Bytes32) return VmDynamicVariable(Bytes32(View(decoded)));
		return VmDynamicVariable(Bytes64(View(decoded)));
	}
	case VmType::Struct:
	{
		if (!schema)
		{
			throw std::runtime_error("Struct schema missing");
		}
		return VmDynamicVariable(BuildStruct(*schema, value, alloc));
	}
	default:
		break;
	}
	throw std::runtime_error("Unsupported VmType in metadata");
}

static VmDynamicVariable BuildArray(VmType baseType, const VmStructSchema* schema, const rapidjson::Value& value, Allocator& alloc)
{
	if (!value.IsArray())
	{
		throw std::runtime_error("Expected array value");
	}
	const uint32_t count = (uint32_t)value.Size();
	VmDynamicVariable out;
	out.type = (VmType)((uint8_t)baseType | (uint8_t)VmType::Array);
	out.arrayLength = count;

	switch (baseType)
	{
	case VmType::Int8:
	{
		uint8_t* arr = alloc.Alloc<uint8_t>(count);
		for (uint32_t i = 0; i != count; ++i)
		{
			VmDynamicVariable v = BuildScalar(baseType, schema, value[i], alloc);
			arr[i] = v.data.int8;
		}
		out.data.int8Array = arr;
		return out;
	}
	case VmType::Int16:
	{
		uint16_t* arr = alloc.Alloc<uint16_t>(count);
		for (uint32_t i = 0; i != count; ++i)
		{
			VmDynamicVariable v = BuildScalar(baseType, schema, value[i], alloc);
			arr[i] = v.data.int16;
		}
		out.data.int16Array = arr;
		return out;
	}
	case VmType::Int32:
	{
		uint32_t* arr = alloc.Alloc<uint32_t>(count);
		for (uint32_t i = 0; i != count; ++i)
		{
			VmDynamicVariable v = BuildScalar(baseType, schema, value[i], alloc);
			arr[i] = v.data.int32;
		}
		out.data.int32Array = arr;
		return out;
	}
	case VmType::Int64:
	{
		uint64_t* arr = alloc.Alloc<uint64_t>(count);
		for (uint32_t i = 0; i != count; ++i)
		{
			VmDynamicVariable v = BuildScalar(baseType, schema, value[i], alloc);
			arr[i] = v.data.int64;
		}
		out.data.int64Array = arr;
		return out;
	}
	case VmType::Int256:
	{
		uint256* arr = alloc.Alloc<uint256>(count);
		for (uint32_t i = 0; i != count; ++i)
		{
			VmDynamicVariable v = BuildScalar(baseType, schema, value[i], alloc);
			arr[i] = v.data.int256;
		}
		out.data.int256Array = arr;
		return out;
	}
	case VmType::Bytes:
	{
		ByteView* arr = alloc.Alloc<ByteView>(count);
		for (uint32_t i = 0; i != count; ++i)
		{
			VmDynamicVariable v = BuildScalar(baseType, schema, value[i], alloc);
			arr[i] = v.data.bytes;
		}
		out.data.bytesArray = arr;
		return out;
	}
	case VmType::Bytes16:
	{
		Bytes16* arr = alloc.Alloc<Bytes16>(count);
		for (uint32_t i = 0; i != count; ++i)
		{
			VmDynamicVariable v = BuildScalar(baseType, schema, value[i], alloc);
			arr[i] = v.data.bytes16;
		}
		out.data.bytes16Array = arr;
		return out;
	}
	case VmType::Bytes32:
	{
		Bytes32* arr = alloc.Alloc<Bytes32>(count);
		for (uint32_t i = 0; i != count; ++i)
		{
			VmDynamicVariable v = BuildScalar(baseType, schema, value[i], alloc);
			arr[i] = v.data.bytes32;
		}
		out.data.bytes32Array = arr;
		return out;
	}
	case VmType::Bytes64:
	{
		Bytes64* arr = alloc.Alloc<Bytes64>(count);
		for (uint32_t i = 0; i != count; ++i)
		{
			VmDynamicVariable v = BuildScalar(baseType, schema, value[i], alloc);
			arr[i] = v.data.bytes64;
		}
		out.data.bytes64Array = arr;
		return out;
	}
	case VmType::String:
	{
		const char** arr = alloc.Alloc<const char*>(count);
		for (uint32_t i = 0; i != count; ++i)
		{
			VmDynamicVariable v = BuildScalar(baseType, schema, value[i], alloc);
			arr[i] = v.data.string;
		}
		out.data.stringArray = arr;
		return out;
	}
	case VmType::Struct:
	{
		if (!schema)
		{
			throw std::runtime_error("Struct schema missing");
		}
		VmDynamicStruct* arr = alloc.Alloc<VmDynamicStruct>(count);
		for (uint32_t i = 0; i != count; ++i)
		{
			arr[i] = BuildStruct(*schema, value[i], alloc);
		}
		out.data.structureArray = { *schema, arr };
		return out;
	}
	default:
		break;
	}
	throw std::runtime_error("Unsupported array metadata type");
}

static VmDynamicVariable BuildValue(const VmVariableSchema& schema, const rapidjson::Value& value, Allocator& alloc)
{
	const uint8_t raw = (uint8_t)schema.type;
	const bool isArray = (raw & (uint8_t)VmType::Array) != 0;
	if (isArray)
	{
		VmType base = (VmType)(raw & ~(uint8_t)VmType::Array);
		return BuildArray(base, schema.structure.numFields ? &schema.structure : nullptr, value, alloc);
	}
	return BuildScalar(schema.type, schema.structure.numFields ? &schema.structure : nullptr, value, alloc);
}

static ByteArray SerializeMetadata(const VmStructSchema& schema, const rapidjson::Value& json)
{
	Allocator alloc;
	VmDynamicStruct s = BuildStruct(schema, json, alloc);
	phantasma::carbon::vector<Byte> buffer;
	WriteView w(buffer);
	Write(s, schema, w);
	return ByteArray(buffer.begin(), buffer.end());
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
	Ensure(cfg.gasFeeBase.has_value(), "gas_fee_base is required");
	Ensure(cfg.gasFeeCreateTokenBase.has_value(), "gas_fee_create_token_base is required");
	Ensure(cfg.gasFeeCreateTokenSymbol.has_value(), "gas_fee_create_token_symbol is required");
	Ensure(cfg.gasFeeMultiplier.has_value(), "gas_fee_multiplier is required");
	Ensure(cfg.createTokenMaxData.has_value(), "create_token_max_data is required");
	Ensure(!cfg.tokenMetadataRaw.empty(), "token_metadata is required");

	const std::string tokenType = cfg.tokenType.empty() ? "nft" : cfg.tokenType;
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
	const ByteArray signedBytes = SignAndSerialize(tx, keys);
	const std::string txHex = BytesToHex(signedBytes);

	if (cfg.dryRun)
	{
		std::cout << "[dry-run] Prepared tx: " << txHex << std::endl;
		return;
	}

	CurlClient http(cfg.rpc);
	PhantasmaAPI api(http);
	PhantasmaError err;
	const String hash = api.SendCarbonTransaction(txHex.c_str(), &err);
	Ensure(err.code == 0, "Failed to send transaction: " + err.message);
	std::cout << "txHash: " << hash << std::endl;

	std::string result;
	if (WaitForTx(api, hash.c_str(), result))
	{
		const uint32_t carbonId = CreateTokenTxHelper::ParseResult(result);
		std::cout << "Deployed carbon token ID: " << carbonId << std::endl;
	}
}

static ByteArray ExtractRomFromMetadata(const rapidjson::Value& meta)
{
	if (!meta.IsObject() || !meta.HasMember("rom") || !meta["rom"].IsString())
	{
		return {};
	}
	return DecodeHex(meta["rom"].GetString(), "rom");
}

static void InjectSeriesDefaults(rapidjson::Document& meta, const intx& seriesId, const ByteArray& romBytes)
{
	auto& alloc = meta.GetAllocator();
	const std::string idStr = IdToString(seriesId);
	meta.RemoveMember("mode");
	meta.RemoveMember(StandardMeta::id.c_str());
	meta.RemoveMember("rom");
	rapidjson::Value idName(StandardMeta::id.c_str(), alloc);
	rapidjson::Value idVal(idStr.c_str(), alloc);
	meta.AddMember(idName, idVal, alloc);

	rapidjson::Value modeName("mode", alloc);
	rapidjson::Value modeVal(romBytes.empty() ? 0 : 1);
	meta.AddMember(modeName, modeVal, alloc);

	rapidjson::Value romName("rom", alloc);
	const std::string romHex = romBytes.empty() ? "0x" : BytesToHex(romBytes);
	rapidjson::Value romVal(romHex.c_str(), alloc);
	meta.AddMember(romName, romVal, alloc);
}

static void InjectNftDefaults(rapidjson::Document& meta, const intx& nftId, const ByteArray& romBytes)
{
	auto& alloc = meta.GetAllocator();
	const std::string idStr = IdToString(nftId);
	meta.RemoveMember(StandardMeta::id.c_str());
	meta.RemoveMember("rom");
	rapidjson::Value idName(StandardMeta::id.c_str(), alloc);
	rapidjson::Value idVal(idStr.c_str(), alloc);
	meta.AddMember(idName, idVal, alloc);

	rapidjson::Value romName("rom", alloc);
	const std::string romHex = romBytes.empty() ? "0x" : BytesToHex(romBytes);
	rapidjson::Value romVal(romHex.c_str(), alloc);
	meta.AddMember(romName, romVal, alloc);
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

	rapidjson::Document metaDoc;
	NormalizeMetadataJson(cfg.seriesMetadataRaw, "series_metadata", metaDoc);
	const ByteArray romBytes = ExtractRomFromMetadata(metaDoc);
	const intx seriesId = GenerateRandomId();
	InjectSeriesDefaults(metaDoc, seriesId, romBytes);

	const ByteArray metadataBytes = SerializeMetadata(schemasOwned.view.seriesMetadata, metaDoc);

	const SeriesInfoOwned seriesInfoOwned = SeriesInfoBuilder::Build(seriesId.Int256(), 0, 0, owner, &metadataBytes);

	const CreateSeriesFeeOptions feeOptions(
		cfg.gasFeeBase.value(),
		cfg.gasFeeCreateTokenSeries.value(),
		cfg.gasFeeMultiplier.value());

	const TxEnvelope tx = CreateTokenSeriesTxHelper::BuildTx(cfg.carbonTokenId.value(), seriesInfoOwned.View(), owner, &feeOptions, cfg.createSeriesMaxData.value());
	const ByteArray signedBytes = SignAndSerialize(tx, keys);
	const std::string txHex = BytesToHex(signedBytes);

	if (cfg.dryRun)
	{
		std::cout << "[dry-run] Prepared tx: " << txHex << std::endl;
		return;
	}

	CurlClient http(cfg.rpc);
	PhantasmaAPI api(http);
	PhantasmaError err;
	const String hash = api.SendCarbonTransaction(txHex.c_str(), &err);
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

	rapidjson::Document metaDoc;
	NormalizeMetadataJson(cfg.nftMetadataRaw, "nft_metadata", metaDoc);
	const ByteArray romBytes = ExtractRomFromMetadata(metaDoc);
	const intx nftId = GenerateRandomId();
	InjectNftDefaults(metaDoc, nftId, romBytes);

	const ByteArray rom = SerializeMetadata(schemasOwned.view.rom, metaDoc);

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

	const ByteArray signedBytes = SignAndSerialize(tx, keys);
	const std::string txHex = BytesToHex(signedBytes);

	if (cfg.dryRun)
	{
		std::cout << "[dry-run] Prepared tx: " << txHex << std::endl;
		return;
	}

	CurlClient http(cfg.rpc);
	PhantasmaAPI api(http);
	PhantasmaError err;
	const String hash = api.SendCarbonTransaction(txHex.c_str(), &err);
	Ensure(err.code == 0, "Failed to send transaction: " + err.message);
	std::cout << "txHash: " << hash << std::endl;

	std::string result;
	if (WaitForTx(api, hash.c_str(), result))
	{
		const auto addresses = MintNonFungibleTxHelper::ParseResult(cfg.carbonTokenId.value(), result);
		if (!addresses.empty())
		{
			const String addr = Base16::Encode(addresses[0].bytes, (int)Bytes32::length, false);
			std::cout << "Deployed NFT with phantasma ID " << IdToString(nftId)
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
