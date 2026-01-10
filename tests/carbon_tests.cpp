#include <cstring>
#include "../include/PhantasmaAPI.h"
#include "../include/Adapters/PhantasmaAPI_openssl.h"
#include "../include/Numerics/Base16.h"
#include "../include/Numerics/BigInteger.h"
#include "../include/Cryptography/KeyPair.h"
#include "../include/Carbon/Carbon.h"
#include "../include/Carbon/DataBlockchain.h"
#include "../include/Carbon/Tx.h"
#include "../include/Carbon/Contracts/Token.h"
#include "../include/Carbon/Contracts/TokenSchemas.h"
#include "../include/Carbon/DataVm.h"
#include "../include/Utils/Timestamp.h"
#include "../include/VM/ScriptBuilder.h"
#include "../include/Blockchain/Transaction.h"
#include "../include/VM/VMObject.h"
#include "../include/Utils/BinaryWriter.h"
#include "../include/Utils/BinaryReader.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <limits>

using namespace phantasma;
using namespace phantasma::carbon;

struct TestContext
{
	int total = 0;
	int failed = 0;
};

static void Report(TestContext& ctx, bool ok, const std::string& name, const std::string& details = {})
{
	++ctx.total;
	if (!ok)
	{
		++ctx.failed;
		std::cerr << "FAIL: " << name;
		if (!details.empty())
		{
			std::cerr << " (" << details << ")";
		}
		std::cerr << std::endl;
	}
}

template <typename Fn>
static void ExpectNoThrow(TestContext& ctx, const std::string& name, Fn&& fn)
{
	try
	{
		fn();
		Report(ctx, true, name);
	}
	catch (const std::exception& ex)
	{
		Report(ctx, false, name, ex.what());
	}
	catch (...)
	{
		Report(ctx, false, name, "unexpected exception");
	}
}

template <typename Fn>
static void ExpectThrowContains(TestContext& ctx, const std::string& name, const std::string& needle, Fn&& fn)
{
#ifdef PHANTASMA_EXCEPTION_ENABLE
	try
	{
		fn();
		Report(ctx, false, name, "no exception");
	}
	catch (const std::exception& ex)
	{
		const std::string msg = ex.what();
		Report(ctx, msg.find(needle) != std::string::npos, name, msg);
	}
	catch (...)
	{
		Report(ctx, false, name, "unexpected exception");
	}
#else
	(void)fn; // no-op to silence unused warning when exceptions are disabled.
	(void)needle;
	Report(ctx, true, name + " (exceptions disabled)");
#endif
}

struct Row
{
	std::string kind;
	std::string value;
	std::string hex;
	std::string decOrig;
	std::string decBack;
};

static std::string ToUpper(const std::string& s)
{
	std::string out = s;
	std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return (char)toupper(c); });
	return out;
}

static ByteArray HexToBytes(const std::string& hex)
{
	return Base16::Decode(hex.c_str(), (int)hex.size());
}

static std::string BytesToHex(const ByteArray& bytes)
{
	const String s = Base16::Encode(bytes.empty() ? 0 : &bytes.front(), (int)bytes.size(), false);
	return std::string(s.begin(), s.end());
}

static ByteArray BytesFromView(const ByteView& view)
{
	if (!view.length)
	{
		return {};
	}
	return ByteArray(view.bytes, view.bytes + view.length);
}

static ByteArray ParseDecBytes(const std::string& dec)
{
	std::stringstream ss(dec);
	std::string token;
	ByteArray out;
	while (ss >> token)
	{
		const int value = std::stoi(token, nullptr, 10);
		if (value < 0 || value > 255)
		{
			throw std::runtime_error("invalid decimal byte in fixture");
		}
		out.push_back((Byte)value);
	}
	return out;
}

static int64_t ParseNum(const std::string& s)
{
	int base = 10;
	size_t idx = 0;
	if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
	{
		base = 16;
		idx = 2;
	}
	return std::stoll(s.substr(idx), nullptr, base);
}

static uint64_t ParseUNum(const std::string& s)
{
	int base = 10;
	size_t idx = 0;
	if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
	{
		base = 16;
		idx = 2;
	}
	return std::stoull(s.substr(idx), nullptr, base);
}

static intx ParseIntx(const std::string& s)
{
	bool err = false;
	intx v = intx::FromString(s.c_str(), (uint32_t)s.size(), 10, &err);
	if (err)
	{
		throw std::runtime_error("bad intx");
	}
	return v;
}

static int256 ToInt256(const BigInteger& bi)
{
	const ByteArray bytes = bi.ToSignedByteArray();
	return int256::FromBytes(ByteView{ bytes.data(), bytes.size() });
}

static std::vector<std::string> SplitCsv(const std::string& s)
{
	if (s.empty())
	{
		return {};
	}
	std::vector<std::string> out;
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, ','))
	{
		out.push_back(item);
	}
	return out;
}

static std::vector<ByteArray> ParseArrBytes2D(const std::string& s)
{
	// format: [[01,02],[03,04]]
	if (s.size() < 4 || s.front() != '[' || s[1] != '[' || s[s.size() - 2] != ']' || s[s.size() - 1] != ']')
	{
		return {};
	}
	const std::string inner = s.substr(2, s.size() - 4);
	std::vector<ByteArray> out;
	size_t start = 0;
	while (start < inner.size())
	{
		size_t end = inner.find("],[", start);
		const std::string seg = inner.substr(start, end == std::string::npos ? std::string::npos : end - start);
		std::string flat;
		for (char c : seg)
		{
			if (c != ',')
			{
				flat.push_back(c);
			}
		}
		out.push_back(HexToBytes(flat));
		if (end == std::string::npos)
		{
			break;
		}
		start = end + 3;
	}
	return out;
}

static std::vector<Row> LoadRows(const std::string& path)
{
	std::ifstream file(path);
	if (!file.is_open())
	{
		file.open("tests/" + path);
	}
	std::string line;
	std::vector<Row> rows;
	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}
		// strip UTF-8 BOM if present
		if (!line.empty() && (unsigned char)line[0] == 0xEF)
		{
			line = line.substr(3);
		}
		std::vector<std::string> cols;
		std::stringstream ss(line);
		std::string col;
		while (std::getline(ss, col, '\t'))
		{
			cols.push_back(col);
		}
		if (cols.size() == 5 && (cols[0] == "BI" || cols[0] == "INTX"))
		{
			rows.push_back(Row{ cols[0], cols[1], cols[2], cols[3], cols[4] });
		}
		else if (cols.size() == 3)
		{
			rows.push_back(Row{ cols[0], cols[1], cols[2], {}, {} });
		}
	}
	return rows;
}

static const std::string& SampleIcon()
{
	static const std::string icon = "data:image/svg+xml;base64,PHN2ZyB4bWxucz0naHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmcnIHZpZXdCb3g9JzAgMCAyNCAyNCc+PHBhdGggZmlsbD0nI0Y0NDMzNicgZD0nTTcgNGg1YTUgNSAwIDAxMCAxMEg5djZIN3pNOSA2djZoM2EzIDMgMCAwMDAtNnonLz48L3N2Zz4=";
	return icon;
}

static Bytes32 ToBytes32(const ByteArray& bytes)
{
	Bytes32 out{};
	if (!bytes.empty())
	{
		out = View(bytes.data(), bytes.data() + bytes.size());
	}
	return out;
}

static VmNamedVariableSchema MakeSchema(const char* name, VmType type, const VmStructSchema* structSchema = nullptr)
{
	VmNamedVariableSchema schema{};
	schema.name = SmallString(name);
	schema.schema.type = type;
	if (structSchema)
	{
		schema.schema.structure = *structSchema;
	}
	return schema;
}

static VmStructSchema MakeStructSchema(VmNamedVariableSchema* fields, uint32_t count, bool allowDynamicExtras = false)
{
	return VmStructSchema::Sort(count, fields, allowDynamicExtras);
}

static ByteArray BuildConsensusSingleVoteScript()
{
	const std::string wif = "L5UEVHBjujaR1721aZM5Zm5ayjDyamMZS9W35RE9Y9giRkdf3dVx";
	const PhantasmaKeys keys = PhantasmaKeys::FromWIF(wif.c_str(), (int)wif.size());
	const int gasLimit = 10000;
	const int gasPrice = 210000;
	const char* subject = "system.nexus.protocol.version";

	ScriptBuilder sb;
	// Keep TS argument order (gasLimit, gasPrice) to match the shared vector.
	return sb
		.AllowGas(keys.GetAddress(), Address(), gasLimit, gasPrice)
		.CallContract("consensus", "SingleVote", keys.GetAddress().Text(), subject, 0)
		.SpendGas(keys.GetAddress())
		.EndScript();
}

static void EncodeTests(TestContext& ctx, const std::vector<Row>& rows)
{
	const std::vector<std::string> skipKinds = {
		"VMSTRUCT01", "VMSTRUCT02", "TX1", "TX2", "TX-CREATE-TOKEN", "TX-CREATE-TOKEN-SERIES", "TX-MINT-NON-FUNGIBLE"
	};
	for (const auto& row : rows)
	{
		if (std::find(skipKinds.begin(), skipKinds.end(), row.kind) != skipKinds.end())
		{
			continue;
		}

		ByteArray buffer;
		WriteView w(buffer);
		bool encoded = true;
		if (row.kind == "U8") Write1((uint8_t)ParseNum(row.value), w);
		else if (row.kind == "I16") Write2((int16_t)ParseNum(row.value), w);
		else if (row.kind == "I32") Write4((int32_t)ParseNum(row.value), w);
		else if (row.kind == "U32") Write4u((uint32_t)ParseUNum(row.value), w);
		else if (row.kind == "I64") Write8((int64_t)ParseNum(row.value), w);
		else if (row.kind == "U64") Write8u((uint64_t)ParseUNum(row.value), w);
		else if (row.kind == "FIX16") { auto b = HexToBytes(row.value); Write16(b.data(), b.size(), w); }
		else if (row.kind == "FIX32") { auto b = HexToBytes(row.value); Write32(b.data(), b.size(), w); }
		else if (row.kind == "FIX64") { auto b = HexToBytes(row.value); Write64(b.data(), b.size(), w); }
		else if (row.kind == "SZ") WriteSz(row.value, w);
		else if (row.kind == "ARRSZ") WriteArraySz(SplitCsv(row.value), w);
		else if (row.kind == "ARR8")
		{
			std::vector<int8_t> vals;
			for (const auto& s : SplitCsv(row.value)) vals.push_back((int8_t)ParseNum(s));
			WriteArray8(vals, w);
		}
		else if (row.kind == "ARR16")
		{
			std::vector<int16_t> vals;
			for (const auto& s : SplitCsv(row.value)) vals.push_back((int16_t)ParseNum(s));
			WriteArray16(vals, w);
		}
		else if (row.kind == "ARR32")
		{
			std::vector<int32_t> vals;
			for (const auto& s : SplitCsv(row.value)) vals.push_back((int32_t)ParseNum(s));
			WriteArray32(vals, w);
		}
		else if (row.kind == "ARR64")
		{
			std::vector<int64_t> vals;
			for (const auto& s : SplitCsv(row.value)) vals.push_back((int64_t)ParseNum(s));
			WriteArray64(vals, w);
		}
		else if (row.kind == "ARRU64")
		{
			std::vector<uint64_t> vals;
			for (const auto& s : SplitCsv(row.value)) vals.push_back(ParseUNum(s));
			WriteArray64u(vals, w);
		}
		else if (row.kind == "ARRBYTES-1D") { WriteArray(HexToBytes(row.value), w); }
		else if (row.kind == "ARRBYTES-2D")
		{
			WriteArrayOfArrays(ParseArrBytes2D(row.value), w);
		}
		else if (row.kind == "BI")
		{
			const intx ix = ParseIntx(row.value);
			Write(ix.Int256(), w);
		}
		else if (row.kind == "INTX")
		{
			const intx ix = ParseIntx(row.value);
			Write(ix, w);
		}
		else if (row.kind == "ARRBI")
		{
			std::vector<int256> vals;
			for (const auto& s : SplitCsv(row.value))
			{
				const intx ix = ParseIntx(s);
				vals.push_back(ix.Int256());
			}
			WriteArrayInt256(vals, w);
		}
		else
		{
			encoded = false;
		}

		if (!encoded)
		{
			Report(ctx, false, "encode " + row.kind, "unsupported kind");
			continue;
		}

		const std::string got = ToUpper(BytesToHex(buffer));
		const std::string expected = ToUpper(row.hex);
		Report(ctx, got == expected, "encode " + row.kind, got + " vs " + expected);
	}
}

static void DecodeTests(TestContext& ctx, const std::vector<Row>& rows)
{
	for (const auto& row : rows)
	{
		const ByteArray bytes = HexToBytes(row.hex);
		ReadView r(bytes.empty() ? nullptr : (void*)bytes.data(), bytes.size());

		if (row.kind == "U8")
		{
			const uint8_t v = Read1(r);
			Report(ctx, std::to_string(v) == std::to_string(ParseNum(row.value)), "decode U8");
		}
		else if (row.kind == "I16")
		{
			const int16_t v = Read2(r);
			Report(ctx, std::to_string(v) == std::to_string(ParseNum(row.value)), "decode I16");
		}
		else if (row.kind == "I32")
		{
			const int32_t v = Read4(r);
			Report(ctx, std::to_string(v) == std::to_string(ParseNum(row.value)), "decode I32");
		}
		else if (row.kind == "U32")
		{
			const uint32_t v = Read4u(r);
			Report(ctx, std::to_string(v) == std::to_string(ParseUNum(row.value)), "decode U32");
		}
		else if (row.kind == "I64")
		{
			const int64_t v = Read8(r);
			Report(ctx, std::to_string(v) == std::to_string(ParseNum(row.value)), "decode I64");
		}
		else if (row.kind == "U64")
		{
			const uint64_t v = Read8u(r);
			Report(ctx, std::to_string(v) == std::to_string(ParseUNum(row.value)), "decode U64");
		}
		else if (row.kind == "FIX16")
		{
			const std::string got = ToUpper(BytesToHex(ReadExactly(16, r)));
			Report(ctx, got == ToUpper(row.value), "decode FIX16");
		}
		else if (row.kind == "FIX32")
		{
			const std::string got = ToUpper(BytesToHex(ReadExactly(32, r)));
			Report(ctx, got == ToUpper(row.value), "decode FIX32");
		}
		else if (row.kind == "FIX64")
		{
			const std::string got = ToUpper(BytesToHex(ReadExactly(64, r)));
			Report(ctx, got == ToUpper(row.value), "decode FIX64");
		}
		else if (row.kind == "SZ")
		{
			const std::string v = ReadSz(r);
			Report(ctx, v == row.value, "decode SZ");
		}
		else if (row.kind == "ARRSZ")
		{
			const auto v = ReadArraySz(r);
			Report(ctx, v == SplitCsv(row.value), "decode ARRSZ");
		}
		else if (row.kind == "ARR8")
		{
			const auto v = ReadArray8(r);
			std::vector<int8_t> expected;
			for (const auto& s : SplitCsv(row.value)) expected.push_back((int8_t)ParseNum(s));
			Report(ctx, v == expected, "decode ARR8");
		}
		else if (row.kind == "ARR16")
		{
			const auto v = ReadArray16(r);
			std::vector<int16_t> expected;
			for (const auto& s : SplitCsv(row.value)) expected.push_back((int16_t)ParseNum(s));
			Report(ctx, v == expected, "decode ARR16");
		}
		else if (row.kind == "ARR32")
		{
			const auto v = ReadArray32(r);
			std::vector<int32_t> expected;
			for (const auto& s : SplitCsv(row.value)) expected.push_back((int32_t)ParseNum(s));
			Report(ctx, v == expected, "decode ARR32");
		}
		else if (row.kind == "ARR64")
		{
			const auto v = ReadArray64(r);
			std::vector<int64_t> expected;
			for (const auto& s : SplitCsv(row.value)) expected.push_back((int64_t)ParseNum(s));
			Report(ctx, v == expected, "decode ARR64");
		}
		else if (row.kind == "ARRU64")
		{
			const auto v = ReadArray64u(r);
			std::vector<uint64_t> expected;
			for (const auto& s : SplitCsv(row.value)) expected.push_back(ParseUNum(s));
			Report(ctx, v == expected, "decode ARRU64");
		}
		else if (row.kind == "ARRBYTES-1D")
		{
			const std::string got = ToUpper(BytesToHex(ReadArray(r)));
			Report(ctx, got == ToUpper(row.value), "decode ARRBYTES-1D");
		}
		else if (row.kind == "ARRBYTES-2D")
		{
			const auto arrays = ReadArrayOfArrays(r);
			std::vector<std::string> got;
			for (const auto& a : arrays) got.push_back(BytesToHex(a));
			std::vector<std::string> expected;
			for (const auto& a : ParseArrBytes2D(row.value)) expected.push_back(BytesToHex(a));
			Report(ctx, got == expected, "decode ARRBYTES-2D");
		}
		else if (row.kind == "BI")
		{
			const int256 v = ReadInt256(r);
			const std::string expected = row.decBack.empty() ? row.value : row.decBack;
			Report(ctx, v.ToString() == expected, "decode BI", v.ToString() + " vs " + expected);
		}
		else if (row.kind == "INTX")
		{
			const intx v = ReadIntX(r);
			const std::string expected = row.decBack.empty() ? row.value : row.decBack;
			Report(ctx, v.ToString() == expected, "decode INTX", v.ToString() + " vs " + expected);
		}
		else if (row.kind == "ARRBI")
		{
			const auto vals = ReadArrayInt256(r);
			std::vector<std::string> got;
			for (const auto& b : vals)
			{
				got.push_back(b.ToString());
			}
			std::vector<std::string> expected;
			for (const auto& s : SplitCsv(row.value)) expected.push_back(s);
			Report(ctx, got == expected, "decode ARRBI");
		}
		else if (row.kind == "VMSTRUCT01")
		{
			const TokenSchemasOwned schemas = TokenSchemasBuilder::PrepareStandardTokenSchemas();
			const ByteArray built = CarbonSerialize(schemas.View());
			const std::string got = ToUpper(BytesToHex(built));
			Report(ctx, got == ToUpper(row.hex), "VMSTRUCT01", got + " vs " + ToUpper(row.hex));
		}
		else if (row.kind == "VMSTRUCT02")
		{
			const char desc[] = "My test token description";
			std::vector<VmNamedDynamicVariable> fields = {
				{ SmallString("description"), VmDynamicVariable(desc) },
				{ SmallString("icon"), VmDynamicVariable(SampleIcon().c_str()) },
				{ SmallString("name"), VmDynamicVariable("My test token!") },
				{ SmallString("url"), VmDynamicVariable("http://example.com") },
			};
			VmDynamicStruct meta = VmDynamicStruct::Sort((uint32_t)fields.size(), fields.data());
			ByteArray buf;
			WriteView w(buf);
			Write(meta, w);
			Report(ctx, ToUpper(BytesToHex(buf)) == ToUpper(row.hex), "VMSTRUCT02");
		}
		else if (row.kind == "TX1")
		{
			Blockchain::TxMsg msg{};
			msg.type = Blockchain::TxTypes::TransferFungible;
			msg.expiry = 1759711416000LL;
			msg.maxGas = 10000000;
			msg.maxData = 1000;
			msg.gasFrom = Bytes32();
			msg.payload = SmallString("test-payload");
			msg.transferFt = Blockchain::TxMsgTransferFungible{ Bytes32(), 1, 100000000 };

			ByteArray buf;
			WriteView w(buf);
			Write(msg, w);
			Report(ctx, ToUpper(BytesToHex(buf)) == ToUpper(row.hex), "TX1");
		}
		else if (row.kind == "TX2")
		{
			const std::string wifSender = "KwPpBSByydVKqStGHAnZzQofCqhDmD2bfRgc9BmZqM3ZmsdWJw4d";
			const std::string wifReceiver = "KwVG94yjfVg1YKFyRxAGtug93wdRbmLnqqrFV6Yd2CiA9KZDAp4H";

			const PhantasmaKeys sender = PhantasmaKeys::FromWIF(wifSender.c_str(), (int)wifSender.size());
			const PhantasmaKeys receiver = PhantasmaKeys::FromWIF(wifReceiver.c_str(), (int)wifReceiver.size());
			const ByteArray senderKey = sender.GetPublicKey();
			const ByteArray receiverKey = receiver.GetPublicKey();

			Blockchain::TxMsg msg{};
			msg.type = Blockchain::TxTypes::TransferFungible;
			msg.expiry = 1759711416000LL;
			msg.maxGas = 10000000;
			msg.maxData = 1000;
			msg.gasFrom = ToBytes32(senderKey);
			msg.payload = SmallString("test-payload");
			msg.transferFt = Blockchain::TxMsgTransferFungible{ ToBytes32(receiverKey), 1, 100000000 };

			const ByteArray signedTx = Blockchain::TxMsgSigner::SignAndSerialize(msg, sender);
			Report(ctx, ToUpper(BytesToHex(signedTx)) == ToUpper(row.hex), "TX2");
		}
		else if (row.kind == "TX-CREATE-TOKEN")
		{
			const std::string wif = "KwPpBSByydVKqStGHAnZzQofCqhDmD2bfRgc9BmZqM3ZmsdWJw4d";
			const PhantasmaKeys sender = PhantasmaKeys::FromWIF(wif.c_str(), (int)wif.size());
			const ByteArray senderKey = sender.GetPublicKey();
			const Bytes32 senderPub = ToBytes32(senderKey);

			std::vector<std::pair<std::string, std::string>> metaFields = {
				{ "name", "My test token!" },
				{ "icon", SampleIcon() },
				{ "url", "http://example.com" },
				{ "description", "My test token description" },
			};
			const ByteArray metadata = TokenMetadataBuilder::BuildAndSerialize(metaFields);
			const TokenSchemasOwned schemas = TokenSchemasBuilder::PrepareStandardTokenSchemas();
			const ByteArray schemasBytes = CarbonSerialize(schemas.View());

			TokenInfoOwned info = TokenInfoBuilder::Build("MYNFT", intx::FromString("0", 0, 10, nullptr), true, 0, senderPub, metadata, &schemasBytes);
			CreateTokenFeeOptions fees(10000, 10000000000ULL, 10000000000ULL, 10000);
			TxEnvelope tx = CreateTokenTxHelper::BuildTx(info.View(), senderPub, &fees, 100000000, 1759711416000LL);

			ByteArray buf;
			WriteView w(buf);
			Write(tx.msg, w);
			const std::string got = ToUpper(BytesToHex(buf));
			Report(ctx, got == ToUpper(row.hex), "TX-CREATE-TOKEN", got + " vs " + ToUpper(row.hex));
		}
		else if (row.kind == "TX-CREATE-TOKEN-SERIES")
		{
			const std::string wif = "KwPpBSByydVKqStGHAnZzQofCqhDmD2bfRgc9BmZqM3ZmsdWJw4d";
			const PhantasmaKeys sender = PhantasmaKeys::FromWIF(wif.c_str(), (int)wif.size());
			const ByteArray senderKey = sender.GetPublicKey();
			const Bytes32 senderPub = ToBytes32(senderKey);

			const BigInteger phantasmaSeriesId = BigInteger::Parse(String("115792089237316195423570985008687907853269984665640564039457584007913129639935"));
			const int256 seriesId = ToInt256(phantasmaSeriesId);
			const TokenSchemasOwned schemas = TokenSchemasBuilder::PrepareStandardTokenSchemas();
			const std::vector<MetadataField> seriesMetadata;
			SeriesInfoOwned series = SeriesInfoBuilder::Build(schemas.view.seriesMetadata, seriesId, 0, 0, senderPub, seriesMetadata);

			CreateSeriesFeeOptions fees(10000, 2500000000ULL, 10000);
			TxEnvelope tx = CreateTokenSeriesTxHelper::BuildTx((uint64_t)-1, series.View(), senderPub, &fees, 100000000, 1759711416000LL);

			ByteArray buf;
			WriteView w(buf);
			Write(tx.msg, w);
			Report(ctx, ToUpper(BytesToHex(buf)) == ToUpper(row.hex), "TX-CREATE-TOKEN-SERIES");
		}
		else if (row.kind == "TX-MINT-NON-FUNGIBLE")
		{
			const std::string wif = "KwPpBSByydVKqStGHAnZzQofCqhDmD2bfRgc9BmZqM3ZmsdWJw4d";
			const PhantasmaKeys sender = PhantasmaKeys::FromWIF(wif.c_str(), (int)wif.size());
			const ByteArray senderKey = sender.GetPublicKey();
			const Bytes32 senderPub = ToBytes32(senderKey);

			const BigInteger phantasmaNftId = BigInteger::Parse(String("115792089237316195423570985008687907853269984665640564039457584007913129639935"));
			const int256 nftId = ToInt256(phantasmaNftId);
			ByteArray phantasmaRomData = { (Byte)0x01, (Byte)0x42 };

			const TokenSchemasOwned schemas = TokenSchemasBuilder::PrepareStandardTokenSchemas();
			std::vector<MetadataField> nftMetadata = {
				MetadataField{ "name", MetadataValue::FromString("My NFT #1") },
				MetadataField{ "description", MetadataValue::FromString("This is my first NFT!") },
				MetadataField{ "imageURL", MetadataValue::FromString("images-assets.nasa.gov/image/PIA13227/PIA13227~orig.jpg") },
				MetadataField{ "infoURL", MetadataValue::FromString("https://images.nasa.gov/details/PIA13227") },
				MetadataField{ "royalties", MetadataValue::FromInt64(10000000) },
				MetadataField{ "rom", MetadataValue::FromBytes(phantasmaRomData) },
			};

			const ByteArray rom = NftRomBuilder::BuildAndSerialize(
				schemas.view.rom,
				nftId,
				nftMetadata);

			MintNftFeeOptions fees(10000, 1000);
			TxEnvelope tx = MintNonFungibleTxHelper::BuildTx(
				(uint64_t)-1,
				0xFFFFFFFFu,
				senderPub,
				senderPub,
				rom,
				ByteArray(),
				&fees,
				100000000,
				1759711416000LL);

			ByteArray buf;
			WriteView w(buf);
			Write(tx.msg, w);
			const std::string got = ToUpper(BytesToHex(buf));
			Report(ctx, got == ToUpper(row.hex), "TX-MINT-NON-FUNGIBLE", got + " vs " + ToUpper(row.hex));
		}
		else
		{
			Report(ctx, false, "decode " + row.kind, "unsupported kind");
		}
	}
}

static void CallSectionsTests(TestContext& ctx)
{
	Allocator alloc;
	Blockchain::MsgCallArgs sections[2];
	sections[0].registerOffset = -1;
	sections[0].args = {};
	const ByteArray argsData{ (Byte)0x0A, (Byte)0x0B };
	sections[1].registerOffset = 0;
	sections[1].args = ByteView{ argsData.data(), argsData.size() };

	Blockchain::TxMsgCall call{};
	call.moduleId = 1;
	call.methodId = 2;
	call.sections.numArgSections_negative = -2;
	call.sections.argSections = sections;

	ByteArray buf;
	WriteView w(buf);
	Write(call, w);
	const std::string expected = "0100000002000000FEFFFFFFFFFFFFFF020000000A0B";
	const std::string got = ToUpper(BytesToHex(buf));
	Report(ctx, got == expected, "CALL-ARG-SECTIONS-ENCODE", got + " vs " + expected);

	ReadView r(buf.empty() ? nullptr : (void*)buf.data(), buf.size());
	Blockchain::TxMsgCall decoded{};
	const bool decodedOk =
		Read(decoded, r, alloc) &&
		decoded.sections.numArgSections_negative == -2 &&
		decoded.args.length == 0 &&
		decoded.moduleId == call.moduleId &&
		decoded.methodId == call.methodId &&
		decoded.sections.argSections &&
		decoded.sections.argSections[0].registerOffset == -1 &&
		decoded.sections.argSections[0].args.length == 0 &&
		decoded.sections.argSections[1].registerOffset == 0 &&
		ByteArray(decoded.sections.argSections[1].args.bytes, decoded.sections.argSections[1].args.bytes + decoded.sections.argSections[1].args.length) == ByteArray({ (Byte)0x0A, (Byte)0x0B });
	Report(ctx, decodedOk, "CALL-ARG-SECTIONS-DECODE");
}

#if __cplusplus >= 201703L
static void VmObjectTests(TestContext& ctx)
{
	{
		BinaryWriter w;
		w.Write((uint8_t)VMType::Bool);
		w.Write((uint8_t)1);
		const ByteArray bytes = w.ToArray();
		BinaryReader r(bytes);
		VMObject obj;
		const bool ok = obj.DeserializeData(r) && obj.GetType() == VMType::Bool && obj.Data<bool>();
		Report(ctx, ok, "VMObject Bool");
	}
	{
		BinaryWriter w;
		w.Write((uint8_t)VMType::Bytes);
		const ByteArray payload = { (Byte)0x01, (Byte)0x02 };
		w.WriteByteArray(payload);
		const ByteArray bytes = w.ToArray();
		BinaryReader r(bytes);
		VMObject obj;
		const bool ok = obj.DeserializeData(r) &&
			obj.GetType() == VMType::Bytes &&
			obj.Data<ByteArray>() == payload;
		Report(ctx, ok, "VMObject Bytes");
	}
	{
		BinaryWriter w;
		w.Write((uint8_t)VMType::String);
		w.WriteVarString(PHANTASMA_LITERAL("hello world"));
		const ByteArray bytes = w.ToArray();
		BinaryReader r(bytes);
		VMObject obj;
		const bool ok = obj.DeserializeData(r) &&
			obj.GetType() == VMType::String &&
			obj.Data<String>() == PHANTASMA_LITERAL("hello world");
		Report(ctx, ok, "VMObject String");
	}
	{
		BinaryWriter w;
		w.Write((uint8_t)VMType::Number);
		w.WriteBigInteger(BigInteger(123));
		const ByteArray bytes = w.ToArray();
		BinaryReader r(bytes);
		VMObject obj;
		const bool ok = obj.DeserializeData(r) &&
			obj.GetType() == VMType::Number &&
			obj.Data<BigInteger>().ToString() == PHANTASMA_LITERAL("123");
		Report(ctx, ok, "VMObject Number");
	}
	{
		BinaryWriter w;
		w.Write((uint8_t)VMType::Struct);
		w.WriteVarInt(1);
		w.Write((uint8_t)VMType::String);
		w.WriteVarString(PHANTASMA_LITERAL("name"));
		w.Write((uint8_t)VMType::Number);
		w.WriteBigInteger(BigInteger(7));
		const ByteArray bytes = w.ToArray();
		BinaryReader r(bytes);
		VMObject obj;
		bool ok = obj.DeserializeData(r) && obj.GetType() == VMType::Struct;
		if (ok)
		{
			const VMStructure& data = obj.Data<VMStructure>();
			ok = data.size() == 1 &&
				data[0].first.GetType() == VMType::String &&
				data[0].first.Data<String>() == PHANTASMA_LITERAL("name") &&
				data[0].second.GetType() == VMType::Number &&
				data[0].second.Data<BigInteger>().ToString() == PHANTASMA_LITERAL("7");
		}
		Report(ctx, ok, "VMObject Struct");
	}
}
#endif

static void VmDynamicVariableTests(TestContext& ctx)
{
	auto roundtrip = [&](const std::string& name, VmType type, const VmDynamicVariable& input, auto&& check) {
		try
		{
			ByteArray buffer;
			WriteView w(buffer);
			Write(type, input, nullptr, w);
			Allocator alloc;
			ReadView r(buffer.empty() ? nullptr : (void*)buffer.data(), buffer.size());
			VmDynamicVariable out{};
			const bool ok = Read(type, out, nullptr, r, alloc) && check(out);
			Report(ctx, ok, name);
		}
		catch (const std::exception& ex)
		{
			Report(ctx, false, name, ex.what());
		}
		catch (...)
		{
			Report(ctx, false, name, "unexpected exception");
		}
	};

	for (const uint8_t value : { (uint8_t)0, (uint8_t)1, (uint8_t)255 })
	{
		const VmDynamicVariable input(value);
		roundtrip("VmDynamic Int8 " + std::to_string(value), VmType::Int8, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Int8 && out.data.int8 == value;
		});
	}
	for (const int16_t value : { (int16_t)0, (int16_t)1, (int16_t)-32768, (int16_t)32767 })
	{
		const VmDynamicVariable input(value);
		roundtrip("VmDynamic Int16 " + std::to_string(value), VmType::Int16, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Int16 && (int16_t)out.data.int16 == value;
		});
	}
	for (const int32_t value : { (int32_t)0, (int32_t)1, (int32_t)-2147483648, (int32_t)2147483647 })
	{
		const VmDynamicVariable input(value);
		roundtrip("VmDynamic Int32 " + std::to_string(value), VmType::Int32, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Int32 && (int32_t)out.data.int32 == value;
		});
	}
	for (const uint64_t value : { (uint64_t)0, (uint64_t)1, std::numeric_limits<uint64_t>::max() })
	{
		const VmDynamicVariable input(value);
		roundtrip("VmDynamic Int64 " + std::to_string((unsigned long long)value), VmType::Int64, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Int64 && out.data.int64 == value;
		});
	}
	{
		const intx valueX = intx::FromString("1234567890123456789012345678901234567890", 0, 10, nullptr);
		const int256 value = valueX.Int256();
		const VmDynamicVariable input(value);
		roundtrip("VmDynamic Int256", VmType::Int256, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Int256 && out.data.int256.Signed().ToString() == value.ToString();
		});
	}
	{
		ByteArray bytes(32);
		for (size_t i = 0; i < bytes.size(); ++i)
		{
			bytes[i] = (Byte)i;
		}
		const VmDynamicVariable input(ByteView{ bytes.data(), bytes.size() });
		roundtrip("VmDynamic Bytes", VmType::Bytes, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Bytes && BytesFromView(out.data.bytes) == bytes;
		});
	}
	{
		const char* text = "hello world";
		const VmDynamicVariable input(text);
		roundtrip("VmDynamic String", VmType::String, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::String && std::string(out.data.string) == text;
		});
	}
	{
		Bytes16 bytes{};
		for (int i = 0; i < Bytes16::length; ++i)
		{
			bytes.bytes[i] = (Byte)i;
		}
		const VmDynamicVariable input(bytes);
		roundtrip("VmDynamic Bytes16", VmType::Bytes16, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Bytes16 && out.data.bytes16 == bytes;
		});
	}
	{
		Bytes32 bytes{};
		for (int i = 0; i < Bytes32::length; ++i)
		{
			bytes.bytes[i] = (Byte)i;
		}
		const VmDynamicVariable input(bytes);
		roundtrip("VmDynamic Bytes32", VmType::Bytes32, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Bytes32 && out.data.bytes32 == bytes;
		});
	}
	{
		Bytes64 bytes{};
		for (int i = 0; i < Bytes64::length; ++i)
		{
			bytes.bytes[i] = (Byte)i;
		}
		const VmDynamicVariable input(bytes);
		roundtrip("VmDynamic Bytes64", VmType::Bytes64, input, [&](const VmDynamicVariable& out) {
			return out.type == VmType::Bytes64 && out.data.bytes64 == bytes;
		});
	}
}

static void MetadataHelperTests(TestContext& ctx)
{
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("royalties", VmType::Int32);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "royalties", MetadataValue::FromInt64(42) } };
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const bool ok = fields.size() == 1 &&
			fields[0].value.type == VmType::Int32 &&
			(int32_t)fields[0].value.data.int32 == 42;
		Report(ctx, ok, "MetadataHelper Int32 accepts");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("royalties", VmType::Int32);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "royalties", MetadataValue::FromString("forty-two") } };
		ExpectThrowContains(ctx, "MetadataHelper Int32 non-number", "must be a number", [&]() {
			MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		});
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("royalties", VmType::Int32);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "royalties", MetadataValue::FromUInt64(0x100000000ULL) } };
		ExpectThrowContains(ctx, "MetadataHelper Int32 range", "between -2147483648", [&]() {
			MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		});
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("payload", VmType::Bytes);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "payload", MetadataValue::FromString("0a0b") } };
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const ByteArray got = BytesFromView(fields[0].value.data.bytes);
		const bool ok = fields[0].value.type == VmType::Bytes && got == ByteArray({ (Byte)0x0A, (Byte)0x0B });
		Report(ctx, ok, "MetadataHelper Bytes hex");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("payload", VmType::Bytes);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "payload", MetadataValue::FromString("0x0a0b") } };
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const ByteArray got = BytesFromView(fields[0].value.data.bytes);
		const bool ok = fields[0].value.type == VmType::Bytes && got == ByteArray({ (Byte)0x0A, (Byte)0x0B });
		Report(ctx, ok, "MetadataHelper Bytes hex 0x");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("level", VmType::Int8);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "level", MetadataValue::FromInt64(200) } };
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const bool ok = fields[0].value.type == VmType::Int8 && fields[0].value.data.int8 == 200;
		Report(ctx, ok, "MetadataHelper Int8 unsigned");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("checksum", VmType::Int16);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "checksum", MetadataValue::FromInt64(65535) } };
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const bool ok = fields[0].value.type == VmType::Int16 && fields[0].value.data.int16 == 65535;
		Report(ctx, ok, "MetadataHelper Int16 unsigned");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("payload", VmType::Bytes);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "payload", MetadataValue::FromString("xyz") } };
		ExpectThrowContains(ctx, "MetadataHelper Bytes invalid hex", "byte array or hex string", [&]() {
			MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		});
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("supply", VmType::Int64);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "supply", MetadataValue::FromUInt64(std::numeric_limits<uint64_t>::max()) } };
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const bool ok = fields[0].value.type == VmType::Int64 &&
			fields[0].value.data.int64 == std::numeric_limits<uint64_t>::max();
		Report(ctx, ok, "MetadataHelper Int64 unsigned");
	}
	{
		Allocator alloc;
		VmNamedVariableSchema nestedFields[] = {
			MakeSchema("innerName", VmType::String),
			MakeSchema("innerValue", VmType::Int32),
		};
		const VmStructSchema nestedSchema = MakeStructSchema(nestedFields, 2, false);
		const VmNamedVariableSchema schema = MakeSchema("details", VmType::Struct, &nestedSchema);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "details", MetadataValue::FromStruct({
				{ "innerName", MetadataValue::FromString("demo") },
				{ "innerValue", MetadataValue::FromInt64(5) },
			}) }
		};
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const VmDynamicStruct& nested = fields[0].value.data.structure;
		const VmDynamicVariable* innerName = nested[SmallString("innerName")];
		const VmDynamicVariable* innerValue = nested[SmallString("innerValue")];
		const bool ok = innerName && innerValue &&
			innerName->type == VmType::String &&
			std::string(innerName->data.string) == "demo" &&
			innerValue->type == VmType::Int32 &&
			(int32_t)innerValue->data.int32 == 5;
		Report(ctx, ok, "MetadataHelper Struct nested");
	}
	{
		Allocator alloc;
		VmNamedVariableSchema nestedFields[] = {
			MakeSchema("innerName", VmType::String),
		};
		const VmStructSchema nestedSchema = MakeStructSchema(nestedFields, 1, false);
		const VmNamedVariableSchema schema = MakeSchema("details", VmType::Struct, &nestedSchema);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "details", MetadataValue::FromStruct({
				{ "innerName", MetadataValue::FromString("demo") },
				{ "extra", MetadataValue::FromString("oops") },
			}) }
		};
		ExpectThrowContains(ctx, "MetadataHelper Struct unknown", "received unknown property", [&]() {
			MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		});
	}
	{
		Allocator alloc;
		VmNamedVariableSchema nestedFields[] = {
			MakeSchema("innerName", VmType::String),
		};
		const VmStructSchema nestedSchema = MakeStructSchema(nestedFields, 1, false);
		const VmNamedVariableSchema schema = MakeSchema("details", VmType::Struct, &nestedSchema);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "details", MetadataValue::FromStruct({}) }
		};
		ExpectThrowContains(ctx, "MetadataHelper Struct missing", "is mandatory", [&]() {
			MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		});
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("tags", VmType::Array_String);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "tags", MetadataValue::FromArray({
				MetadataValue::FromString("alpha"),
				MetadataValue::FromString("beta"),
			}) }
		};
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const VmDynamicVariable& value = fields[0].value;
		const bool ok = value.type == VmType::Array_String &&
			value.arrayLength == 2 &&
			std::string(value.data.stringArray[0]) == "alpha" &&
			std::string(value.data.stringArray[1]) == "beta";
		Report(ctx, ok, "MetadataHelper Array string");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("deltas", VmType::Array_Int8);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "deltas", MetadataValue::FromArray({
				MetadataValue::FromInt64(1),
				MetadataValue::FromInt64(-1),
				MetadataValue::FromInt64(5),
			}) }
		};
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const VmDynamicVariable& value = fields[0].value;
		const bool ok = value.type == VmType::Array_Int8 &&
			value.arrayLength == 3 &&
			value.data.int8Array &&
			value.data.int8Array[0] == 1 &&
			value.data.int8Array[1] == 255 &&
			value.data.int8Array[2] == 5;
		Report(ctx, ok, "MetadataHelper Array Int8");
	}
	{
		Allocator alloc;
		VmNamedVariableSchema elementFields[] = {
			MakeSchema("name", VmType::String),
		};
		const VmStructSchema elementSchema = MakeStructSchema(elementFields, 1, false);
		const VmNamedVariableSchema schema = MakeSchema("items", VmType::Array_Struct, &elementSchema);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "items", MetadataValue::FromArray({
				MetadataValue::FromStruct({ { "name", MetadataValue::FromString("one") } }),
				MetadataValue::FromStruct({ { "name", MetadataValue::FromString("two") } }),
			}) }
		};
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const VmDynamicVariable& value = fields[0].value;
		const VmStructArray& arrayValue = value.data.structureArray;
		const VmDynamicVariable* firstName = arrayValue.structs[0][SmallString("name")];
		const VmDynamicVariable* secondName = arrayValue.structs[1][SmallString("name")];
		const bool ok = value.type == VmType::Array_Struct &&
			value.arrayLength == 2 &&
			arrayValue.schema.numFields == 1 &&
			std::string(arrayValue.schema.fields[0].name.c_str()) == "name" &&
			firstName && secondName &&
			std::string(firstName->data.string) == "one" &&
			std::string(secondName->data.string) == "two";
		Report(ctx, ok, "MetadataHelper Array struct");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("hash", VmType::Bytes16);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "hash", MetadataValue::FromString("00112233445566778899aabbccddeeff") }
		};
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const Bytes16 expected(HexToBytes("00112233445566778899aabbccddeeff"));
		const bool ok = fields[0].value.type == VmType::Bytes16 && fields[0].value.data.bytes16 == expected;
		Report(ctx, ok, "MetadataHelper Bytes16");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("roots", VmType::Array_Bytes32);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "roots", MetadataValue::FromArray({
				MetadataValue::FromString("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"),
				MetadataValue::FromString("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"),
			}) }
		};
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const Bytes32 expectedA(HexToBytes("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"));
		const Bytes32 expectedB(HexToBytes("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));
		const VmDynamicVariable& value = fields[0].value;
		const bool ok = value.type == VmType::Array_Bytes32 &&
			value.arrayLength == 2 &&
			value.data.bytes32Array &&
			value.data.bytes32Array[0] == expectedA &&
			value.data.bytes32Array[1] == expectedB;
		Report(ctx, ok, "MetadataHelper Array Bytes32");
	}
}

static void TokenMetadataIconTests(TestContext& ctx)
{
	const std::string png = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR4nGMAAQAABQABDQottAAAAABJRU5ErkJggg==";
	const std::string webp = "data:image/webp;base64,UklGRg==";
	const std::string svg = "data:image/svg+xml;base64,PHN2ZyB4bWxucz0naHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmcnIHZpZXdCb3g9JzAgMCAyNCAyNCc+PHBhdGggZmlsbD0nI0Y0NDMzNicgZD0nTTcgNGg1YTUgNSAwIDAxMCAxMEg5djZIN3pNOSA2djZoM2EzIDMgMCAwMDAtNnonLz48L3N2Zz4=";
	const std::string legacySvg = "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'%3E%3Cpath fill='%23F44336' d='M7 4h5a5 5 0 010 10H9v6H7zM9 6v6h3a3 3 0 000-6z'/%3E%3C/svg%3E";
	const std::string gif = "data:image/gif;base64,R0lGODlhAQABAIAAAAAAAAAAACH5BAAAAAAALAAAAAABAAEAAAICRAEAOw==";
	const std::string emptyPayload = "data:image/png;base64,";
	const std::string invalidPayload = "data:image/jpeg;base64,@@@";

	auto buildFields = [&](const std::string& icon) {
		return std::vector<std::pair<std::string, std::string>>{
			{ "name", "My test token!" },
			{ "icon", icon },
			{ "url", "http://example.com" },
			{ "description", "My test token description" },
		};
	};

	ExpectNoThrow(ctx, "TokenMetadata icon PNG", [&]() {
		TokenMetadataBuilder::BuildAndSerialize(buildFields(png));
	});
	ExpectNoThrow(ctx, "TokenMetadata icon JPEG", [&]() {
		const std::string jpegPayload = "/9j/";
		TokenMetadataBuilder::BuildAndSerialize(buildFields("data:image/jpeg;base64," + jpegPayload));
	});
	ExpectNoThrow(ctx, "TokenMetadata icon WebP", [&]() {
		TokenMetadataBuilder::BuildAndSerialize(buildFields(webp));
	});
	ExpectThrowContains(ctx, "TokenMetadata icon SVG", "base64-encoded data URI", [&]() {
		TokenMetadataBuilder::BuildAndSerialize(buildFields(svg));
	});
	ExpectThrowContains(ctx, "TokenMetadata icon legacy svg", "base64-encoded data URI", [&]() {
		TokenMetadataBuilder::BuildAndSerialize(buildFields(legacySvg));
	});
	ExpectThrowContains(ctx, "TokenMetadata icon GIF", "base64-encoded data URI", [&]() {
		TokenMetadataBuilder::BuildAndSerialize(buildFields(gif));
	});
	ExpectThrowContains(ctx, "TokenMetadata icon empty", "non-empty base64 payload", [&]() {
		TokenMetadataBuilder::BuildAndSerialize(buildFields(emptyPayload));
	});
	ExpectThrowContains(ctx, "TokenMetadata icon invalid base64", "payload is not valid base64", [&]() {
		TokenMetadataBuilder::BuildAndSerialize(buildFields(invalidPayload));
	});
}

static void AddressTests(TestContext& ctx)
{
	const std::string wif = "L5UEVHBjujaR1721aZM5Zm5ayjDyamMZS9W35RE9Y9giRkdf3dVx";
	const std::string expected = "P2KFEyFevpQfSaW8G4VjSmhWUZXR4QrG9YQR1HbMpTUCpCL";
	const PhantasmaKeys keys = PhantasmaKeys::FromWIF(wif.c_str(), (int)wif.size());
	const bool fromWifOk = std::string(keys.GetAddress().Text().c_str()) == expected;
	Report(ctx, fromWifOk, "Address from WIF");
	const Address addr = Address::FromText(expected.c_str(), (int)expected.size());
	const bool fromTextOk = std::string(addr.Text().c_str()) == expected;
	Report(ctx, fromTextOk, "Address from text");
	const Address fromKey = Address::FromKey(keys);
	const bool fromKeyOk = std::string(fromKey.Text().c_str()) == expected;
	Report(ctx, fromKeyOk, "Address from key");
	const Address nullAddr;
	Report(ctx, nullAddr.IsNull(), "Address null");
	const bool nullLabelOk = nullAddr.ToString() == PHANTASMA_LITERAL("[Null address]");
	Report(ctx, nullLabelOk, "Address null label");
}

static void ScriptBuilderTransactionTests(TestContext& ctx)
{
	const std::string expectedScriptHex =
		"0D00030350340303000D000302102703000D000223220000000000000000000000000000000000000000000000000000000000000000000003000D000223220100AA53BE71FC41BC0889B694F4D6D03F7906A3D9A21705943CAF9632EEAFBB489503000D000408416C6C6F7747617303000D0004036761732D00012E010D0003010003000D00041D73797374656D2E6E657875732E70726F746F636F6C2E76657273696F6E03000D00042F50324B464579466576705166536157384734566A536D6857555A585234517247395951523148624D7054554370434C03000D00040A53696E676C65566F746503000D000409636F6E73656E7375732D00012E010D000223220100AA53BE71FC41BC0889B694F4D6D03F7906A3D9A21705943CAF9632EEAFBB489503000D0004085370656E6447617303000D0004036761732D00012E010B";
	const std::string expectedSignedTxHex =
		"07746573746E6574046D61696EFD42010D00030350340303000D000302102703000D000223220000000000000000000000000000000000000000000000000000000000000000000003000D000223220100AA53BE71FC41BC0889B694F4D6D03F7906A3D9A21705943CAF9632EEAFBB489503000D000408416C6C6F7747617303000D0004036761732D00012E010D0003010003000D00041D73797374656D2E6E657875732E70726F746F636F6C2E76657273696F6E03000D00042F50324B464579466576705166536157384734566A536D6857555A585234517247395951523148624D7054554370434C03000D00040A53696E676C65566F746503000D000409636F6E73656E7375732D00012E010D000223220100AA53BE71FC41BC0889B694F4D6D03F7906A3D9A21705943CAF9632EEAFBB489503000D0004085370656E6447617303000D0004036761732D00012E010BD202964909436F6E73656E737573010140F1C0410D49A5EDF0945B0EE9FAFDF6CA1FC315118D545E07824BEF1BA1F00881C29419648FD0B8200A356D21FAF45C60F4B77279D931CE4D732F5896E93BFE0D";
	const std::string knownTxHex =
		"07746573746E6574046D61696E03010203D2029649077061796C6F61640101404C033859A20A4FC2E469B3741FB05ACEDFEC24BFE92E07633680488665D79F916773FF40D0E81C4468E1C1487E6E1E6EEFDA5C5D7C53C15C4FB349C2349A1802";

	const ByteArray script = BuildConsensusSingleVoteScript();
	const std::string scriptHex = ToUpper(BytesToHex(script));
	const std::string expectedScript = ToUpper(expectedScriptHex);
	Report(ctx, scriptHex == expectedScript, "ScriptBuilder vector", scriptHex + " vs " + expectedScript);

	const std::string wif = "L5UEVHBjujaR1721aZM5Zm5ayjDyamMZS9W35RE9Y9giRkdf3dVx";
	const PhantasmaKeys keys = PhantasmaKeys::FromWIF(wif.c_str(), (int)wif.size());
	const Timestamp expiration(1234567890);
	const char payloadText[] = "Consensus";
	const ByteArray payload(payloadText, payloadText + sizeof(payloadText) - 1);

	Transaction tx("testnet", "main", script, expiration, payload);
	tx.Sign(keys);
	const ByteArray signedTx = tx.ToByteArray(true);
	const std::string signedHex = ToUpper(BytesToHex(signedTx));
	const std::string expectedSigned = ToUpper(expectedSignedTxHex);
	Report(ctx, signedHex == expectedSigned, "Transaction signed vector", signedHex + " vs " + expectedSigned);

	const ByteArray knownBytes = HexToBytes(knownTxHex);
	BinaryReader reader(knownBytes);
	const Transaction knownTx = Transaction::Unserialize(reader);
	const bool knownOk =
		knownTx.NexusName() == PHANTASMA_LITERAL("testnet") &&
		knownTx.ChainName() == PHANTASMA_LITERAL("main") &&
		ToUpper(BytesToHex(knownTx.Script())) == "010203" &&
		ToUpper(BytesToHex(knownTx.Payload())) == "7061796C6F6164" &&
		knownTx.Expiration().Value == 1234567890u &&
		knownTx.Signatures().size() == 1;
	Report(ctx, knownOk, "Transaction unserialize");
}

static void BigIntSerializationTests(TestContext& ctx)
{
	std::ifstream file("tests/fixtures/phantasma_bigint_vectors.tsv");
	if (!file.is_open())
	{
		file.open("fixtures/phantasma_bigint_vectors.tsv");
	}
	if (!file.is_open())
	{
		Report(ctx, false, "BigInt fixture", "missing phantasma_bigint_vectors.tsv");
		return;
	}

	std::string line;
	bool header = true;
	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}
		if (header)
		{
			header = false;
			continue;
		}
		std::vector<std::string> cols;
		std::stringstream ss(line);
		std::string col;
		while (std::getline(ss, col, '\t'))
		{
			cols.push_back(col);
		}
		if (cols.size() < 3)
		{
			continue;
		}

		const std::string& number = cols[0];
		const ByteArray expectedCsharp = ParseDecBytes(cols[2]);
		const BigInteger n = BigInteger::Parse(String(number.c_str()));

		BinaryWriter writer;
		writer.WriteBigInteger(n);
		const ByteArray serialized = writer.ToArray();
		BinaryReader reader(serialized);
		BigInteger roundtripPha;
		reader.ReadBigInteger(roundtripPha);
		Report(ctx, roundtripPha.ToString() == String(number.c_str()), "BigInt PHA roundtrip " + number);

		ScriptBuilder sb;
		sb.EmitLoad(0, n);
		const ByteArray script = sb.ToScript();
		BinaryReader scriptReader(script);
		Byte opcode = 0;
		Byte reg = 0;
		Byte type = 0;
		scriptReader.Read(opcode);
		scriptReader.Read(reg);
		scriptReader.Read(type);
		(void)reg;
		Int64 len = 0;
		scriptReader.ReadVarInt(len);
		ByteArray csharpBytes;
		scriptReader.Read(csharpBytes, (int)len);
		Report(ctx, opcode == (Byte)Opcode::LOAD, "BigInt C# opcode " + number);
		Report(ctx, type == (Byte)VMType::Number, "BigInt C# type " + number);
		Report(ctx, csharpBytes == expectedCsharp, "BigInt C# " + number);
	}
}

static void IntXIs8ByteSafeTests(TestContext& ctx)
{
	const intx zero((int64_t)0);
	const intx minI64((int64_t)std::numeric_limits<int64_t>::min());
	const intx maxI64((int64_t)std::numeric_limits<int64_t>::max());
	Report(ctx, zero.Int256().Is8ByteSafe(), "IntX safe zero");
	Report(ctx, minI64.Int256().Is8ByteSafe(), "IntX safe min");
	Report(ctx, maxI64.Int256().Is8ByteSafe(), "IntX safe max");

	const intx tooLarge = intx::FromString("9223372036854775808", 0, 10, nullptr);
	Report(ctx, !tooLarge.Int256().Is8ByteSafe(), "IntX unsafe max+1");
	const intx tooSmall = intx::FromString("-9223372036854775809", 0, 10, nullptr);
	Report(ctx, !tooSmall.Int256().Is8ByteSafe(), "IntX unsafe min-1");

	const intx bigBacked(uint256::FromString("42", 0, 10, nullptr));
	Report(ctx, bigBacked.Int256().Is8ByteSafe(), "IntX safe big-backed");
}

int main()
{
	TestContext ctx;
	const auto rows = LoadRows("fixtures/carbon_vectors.tsv");
	EncodeTests(ctx, rows);
	DecodeTests(ctx, rows);
	MetadataHelperTests(ctx);
	TokenMetadataIconTests(ctx);
	AddressTests(ctx);
	VmDynamicVariableTests(ctx);
#if __cplusplus >= 201703L
	VmObjectTests(ctx);
#endif
	ScriptBuilderTransactionTests(ctx);
	BigIntSerializationTests(ctx);
	IntXIs8ByteSafeTests(ctx);
	CallSectionsTests(ctx);

	if (ctx.failed == 0)
	{
		std::cout << "All " << ctx.total << " tests passed." << std::endl;
		return 0;
	}

	std::cerr << ctx.failed << " of " << ctx.total << " tests failed." << std::endl;
	return 1;
}
