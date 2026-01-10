#include "test_cases.h"

namespace testcases {
using namespace testutil;

namespace {

bool SchemaMatches(const VmStructSchema& actual, const VmStructSchema& expected)
{
	if (actual.numFields != expected.numFields)
	{
		return false;
	}
	if (actual.flags != expected.flags)
	{
		return false;
	}
	for (uint32_t i = 0; i < actual.numFields; ++i)
	{
		if (!(actual.fields[i].name == expected.fields[i].name))
		{
			return false;
		}
		if (actual.fields[i].schema.type != expected.fields[i].schema.type)
		{
			return false;
		}
	}
	return true;
}

const VmDynamicVariable* FindField(const VmDynamicStruct& structure, const char* name)
{
	return structure[SmallString(name)];
}

bool ExpectStringField(const VmDynamicStruct& structure, const char* name, const std::string& expected)
{
	const VmDynamicVariable* field = FindField(structure, name);
	if (!field || field->type != VmType::String || !field->data.string)
	{
		return false;
	}
	return std::string(field->data.string) == expected;
}

bool ExpectBytesField(const VmDynamicStruct& structure, const char* name, const ByteArray& expected)
{
	const VmDynamicVariable* field = FindField(structure, name);
	if (!field || field->type != VmType::Bytes)
	{
		return false;
	}
	return BytesFromView(field->data.bytes) == expected;
}

bool ExpectInt256Field(const VmDynamicStruct& structure, const char* name, const int256& expected)
{
	const VmDynamicVariable* field = FindField(structure, name);
	if (!field || field->type != VmType::Int256)
	{
		return false;
	}
	return field->data.int256.Signed().ToString() == expected.ToString();
}

bool ExpectInt8Field(const VmDynamicStruct& structure, const char* name, int8_t expected)
{
	const VmDynamicVariable* field = FindField(structure, name);
	if (!field || field->type != VmType::Int8)
	{
		return false;
	}
	return (int8_t)field->data.int8 == expected;
}

bool ExpectInt32Field(const VmDynamicStruct& structure, const char* name, int32_t expected)
{
	const VmDynamicVariable* field = FindField(structure, name);
	if (!field || field->type != VmType::Int32)
	{
		return false;
	}
	return (int32_t)field->data.int32 == expected;
}

bool ReadTokenSchemas(TokenSchemas& out, ReadView& reader, Allocator& alloc)
{
	return Read(out.seriesMetadata, reader, alloc)
		&& Read(out.rom, reader, alloc)
		&& Read(out.ram, reader, alloc);
}

// Test-only decoder: TokenInfo has no Read() overload, so decode the wire layout using existing primitives.
bool ReadTokenInfo(TokenInfo& out, ReadView& reader, Allocator& alloc)
{
	intx maxSupply;
	if (!Read(maxSupply, reader))
	{
		return false;
	}
	out.maxSupply = (const intx_pod&)maxSupply;
	out.flags = (TokenFlags)Read1(reader);
	out.decimals = Read1(reader);
	if (!Read(out.owner, reader) || !Read(out.symbol, reader))
	{
		return false;
	}
	if (!ReadArray(out.metadata, reader, alloc))
	{
		return false;
	}
	if ((out.flags & TokenFlags_NonFungible) != 0)
	{
		return ReadArray(out.tokenSchemas, reader, alloc);
	}
	out.tokenSchemas = {};
	return true;
}

// Test-only decoder: SeriesInfo has no Read() overload, so decode the wire layout using existing primitives.
bool ReadSeriesInfo(SeriesInfo& out, ReadView& reader, Allocator& alloc)
{
	out.maxMint = Read4u(reader);
	out.maxSupply = Read4u(reader);
	if (!Read(out.owner, reader))
	{
		return false;
	}
	if (!ReadArray(out.metadata, reader, alloc))
	{
		return false;
	}
	return Read(out.rom, reader, alloc) && Read(out.ram, reader, alloc);
}

// Test-only decoder: TxMsg has no Read() overload, so decode the wire layout using existing primitives.
bool ReadTxMsg(Blockchain::TxMsg& out, ReadView& reader, Allocator& alloc, ByteArray* romStorage, ByteArray* ramStorage)
{
	out.type = (Blockchain::TxTypes)Read1(reader);
	out.expiry = Read8(reader);
	out.maxGas = Read8u(reader);
	out.maxData = Read8u(reader);
	if (!Read(out.gasFrom, reader) || !Read(out.payload, reader))
	{
		return false;
	}

	switch (out.type)
	{
	case Blockchain::TxTypes::TransferFungible:
		if (!Read(out.transferFt.to, reader))
		{
			return false;
		}
		out.transferFt.tokenId = Read8u(reader);
		out.transferFt.amount = Read8u(reader);
		return true;
	case Blockchain::TxTypes::Call:
		return Read(out.call, reader, alloc);
	case Blockchain::TxTypes::MintNonFungible:
		if (!romStorage || !ramStorage)
		{
			return false;
		}
		out.mintNonFungible.tokenId = Read8u(reader);
		if (!Read(out.mintNonFungible.to, reader))
		{
			return false;
		}
		out.mintNonFungible.seriesId = Read4u(reader);
		*romStorage = ReadArray(reader);
		*ramStorage = ReadArray(reader);
		out.mintNonFungible.rom = ByteView{ romStorage->data(), romStorage->size() };
		out.mintNonFungible.ram = ByteView{ ramStorage->data(), ramStorage->size() };
		return true;
	default:
		return false;
	}
}

void EncodeTests(TestContext& ctx, const std::vector<Row>& rows)
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

void DecodeTests(TestContext& ctx, const std::vector<Row>& rows)
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
			Allocator alloc;
			TokenSchemas schemas{};
			const bool decoded = ReadTokenSchemas(schemas, r, alloc);
			const TokenSchemasOwned expected = TokenSchemasBuilder::PrepareStandardTokenSchemas();
			const bool fieldsOk = decoded
				&& SchemaMatches(schemas.seriesMetadata, expected.view.seriesMetadata)
				&& SchemaMatches(schemas.rom, expected.view.rom)
				&& SchemaMatches(schemas.ram, expected.view.ram);
			Report(ctx, fieldsOk, "decode VMSTRUCT01");
			if (fieldsOk)
			{
				const std::string got = ToUpper(BytesToHex(CarbonSerialize(schemas)));
				Report(ctx, got == ToUpper(row.hex), "decode VMSTRUCT01 re-encode", got + " vs " + ToUpper(row.hex));
			}
		}
		else if (row.kind == "VMSTRUCT02")
		{
			Allocator alloc;
			VmDynamicStruct meta{};
			const bool decoded = Read(meta, r, alloc);
			const bool fieldsOk = decoded
				&& meta.numFields == 4
				&& ExpectStringField(meta, "description", "My test token description")
				&& ExpectStringField(meta, "icon", SampleIcon())
				&& ExpectStringField(meta, "name", "My test token!")
				&& ExpectStringField(meta, "url", "http://example.com");
			Report(ctx, fieldsOk, "decode VMSTRUCT02");
			if (fieldsOk)
			{
				const std::string got = ToUpper(BytesToHex(CarbonSerialize(meta)));
				Report(ctx, got == ToUpper(row.hex), "decode VMSTRUCT02 re-encode", got + " vs " + ToUpper(row.hex));
			}
		}
		else if (row.kind == "TX1")
		{
			Allocator alloc;
			Blockchain::TxMsg msg{};
			const bool decoded = ReadTxMsg(msg, r, alloc, nullptr, nullptr);
			const bool fieldsOk = decoded
				&& msg.type == Blockchain::TxTypes::TransferFungible
				&& msg.expiry == 1759711416000LL
				&& msg.maxGas == 10000000
				&& msg.maxData == 1000
				&& msg.gasFrom == Bytes32()
				&& msg.payload == SmallString("test-payload")
				&& msg.transferFt.to == Bytes32()
				&& msg.transferFt.tokenId == 1
				&& msg.transferFt.amount == 100000000;
			Report(ctx, fieldsOk, "decode TX1");
			if (fieldsOk)
			{
				const std::string got = ToUpper(BytesToHex(Blockchain::SerializeTx(msg)));
				Report(ctx, got == ToUpper(row.hex), "decode TX1 re-encode", got + " vs " + ToUpper(row.hex));
			}
		}
		else if (row.kind == "TX2")
		{
			const std::string wifSender = "KwPpBSByydVKqStGHAnZzQofCqhDmD2bfRgc9BmZqM3ZmsdWJw4d";
			const std::string wifReceiver = "KwVG94yjfVg1YKFyRxAGtug93wdRbmLnqqrFV6Yd2CiA9KZDAp4H";

			const PhantasmaKeys sender = PhantasmaKeys::FromWIF(wifSender.c_str(), (int)wifSender.size());
			const PhantasmaKeys receiver = PhantasmaKeys::FromWIF(wifReceiver.c_str(), (int)wifReceiver.size());
			const ByteArray senderKey = sender.GetPublicKey();
			const ByteArray receiverKey = receiver.GetPublicKey();

			Allocator alloc;
			Blockchain::TxMsg msg{};
			const bool decoded = ReadTxMsg(msg, r, alloc, nullptr, nullptr);
			Bytes64 sig{};
			const bool sigOk = decoded && Read(sig, r);
			const bool fieldsOk = sigOk
				&& msg.type == Blockchain::TxTypes::TransferFungible
				&& msg.expiry == 1759711416000LL
				&& msg.maxGas == 10000000
				&& msg.maxData == 1000
				&& msg.gasFrom == ToBytes32(senderKey)
				&& msg.payload == SmallString("test-payload")
				&& msg.transferFt.to == ToBytes32(receiverKey)
				&& msg.transferFt.tokenId == 1
				&& msg.transferFt.amount == 100000000;
			Report(ctx, fieldsOk, "decode TX2");
			if (fieldsOk)
			{
				Witness witness{ ToBytes32(senderKey), sig };
				Witnesses witnesses{ 1, &witness };
				Blockchain::SignedTxMsg signedMsg{};
				signedMsg.msg = msg;
				signedMsg.witnesses = witnesses;
				const std::string got = ToUpper(BytesToHex(CarbonSerialize(signedMsg)));
				Report(ctx, got == ToUpper(row.hex), "decode TX2 re-encode", got + " vs " + ToUpper(row.hex));
			}
		}
		else if (row.kind == "TX-CREATE-TOKEN")
		{
			const std::string wif = "KwPpBSByydVKqStGHAnZzQofCqhDmD2bfRgc9BmZqM3ZmsdWJw4d";
			const PhantasmaKeys sender = PhantasmaKeys::FromWIF(wif.c_str(), (int)wif.size());
			const ByteArray senderKey = sender.GetPublicKey();
			const Bytes32 senderPub = ToBytes32(senderKey);

			Allocator alloc;
			Blockchain::TxMsg msg{};
			const bool decoded = ReadTxMsg(msg, r, alloc, nullptr, nullptr);
			const bool baseOk = decoded
				&& msg.type == Blockchain::TxTypes::Call
				&& msg.expiry == 1759711416000LL
				&& msg.maxData == 100000000
				&& msg.gasFrom == senderPub
				&& msg.payload == SmallString("");

			const bool callOk = baseOk
				&& msg.call.moduleId == (uint32_t)ModuleId::Token
				&& msg.call.methodId == (uint32_t)TokenContract_Methods::CreateToken
				&& msg.call.args.length > 0;

			bool fieldsOk = callOk;
			if (fieldsOk)
			{
				ReadView argsReader(const_cast<uint8_t*>(msg.call.args.bytes), msg.call.args.length);
				Allocator tokenAlloc;
				TokenInfo tokenInfo{};
				const bool tokenOk = ReadTokenInfo(tokenInfo, argsReader, tokenAlloc);

				const intx maxSupply = (const intx&)tokenInfo.maxSupply;
				fieldsOk = tokenOk
					&& tokenInfo.symbol == SmallString("MYNFT")
					&& tokenInfo.decimals == 0
					&& tokenInfo.flags == TokenFlags_NonFungible
					&& tokenInfo.owner == senderPub
					&& !maxSupply;

				VmDynamicStruct meta{};
				if (fieldsOk)
				{
					ReadView metaReader(const_cast<uint8_t*>(tokenInfo.metadata.bytes), tokenInfo.metadata.length);
					fieldsOk = Read(meta, metaReader, tokenAlloc)
						&& meta.numFields == 4
						&& ExpectStringField(meta, "name", "My test token!")
						&& ExpectStringField(meta, "icon", SampleIcon())
						&& ExpectStringField(meta, "url", "http://example.com")
						&& ExpectStringField(meta, "description", "My test token description");
				}

				if (fieldsOk)
				{
					TokenSchemas schemas{};
					ReadView schemaReader(const_cast<uint8_t*>(tokenInfo.tokenSchemas.bytes), tokenInfo.tokenSchemas.length);
					const TokenSchemasOwned expected = TokenSchemasBuilder::PrepareStandardTokenSchemas();
					fieldsOk = ReadTokenSchemas(schemas, schemaReader, tokenAlloc)
						&& SchemaMatches(schemas.seriesMetadata, expected.view.seriesMetadata)
						&& SchemaMatches(schemas.rom, expected.view.rom)
						&& SchemaMatches(schemas.ram, expected.view.ram);
				}

				if (fieldsOk)
				{
					CreateTokenFeeOptions fees(10000, 10000000000ULL, 10000000000ULL, 10000);
					fieldsOk = msg.maxGas == fees.CalculateMaxGas(tokenInfo.symbol);
				}
			}

			Report(ctx, fieldsOk, "decode TX-CREATE-TOKEN");
			if (fieldsOk)
			{
				const std::string got = ToUpper(BytesToHex(Blockchain::SerializeTx(msg)));
				Report(ctx, got == ToUpper(row.hex), "decode TX-CREATE-TOKEN re-encode", got + " vs " + ToUpper(row.hex));
			}
		}
		else if (row.kind == "TX-CREATE-TOKEN-SERIES")
		{
			const std::string wif = "KwPpBSByydVKqStGHAnZzQofCqhDmD2bfRgc9BmZqM3ZmsdWJw4d";
			const PhantasmaKeys sender = PhantasmaKeys::FromWIF(wif.c_str(), (int)wif.size());
			const ByteArray senderKey = sender.GetPublicKey();
			const Bytes32 senderPub = ToBytes32(senderKey);

			Allocator alloc;
			Blockchain::TxMsg msg{};
			const bool decoded = ReadTxMsg(msg, r, alloc, nullptr, nullptr);
			const bool baseOk = decoded
				&& msg.type == Blockchain::TxTypes::Call
				&& msg.expiry == 1759711416000LL
				&& msg.maxData == 100000000
				&& msg.gasFrom == senderPub
				&& msg.payload == SmallString("");

			const bool callOk = baseOk
				&& msg.call.moduleId == (uint32_t)ModuleId::Token
				&& msg.call.methodId == (uint32_t)TokenContract_Methods::CreateTokenSeries
				&& msg.call.args.length > 0;

			bool fieldsOk = callOk;
			if (fieldsOk)
			{
				ReadView argsReader(const_cast<uint8_t*>(msg.call.args.bytes), msg.call.args.length);
				const uint64_t tokenId = Read8u(argsReader);
				fieldsOk = tokenId == (uint64_t)-1;

				Allocator seriesAlloc;
				SeriesInfo seriesInfo{};
				if (fieldsOk)
				{
					fieldsOk = ReadSeriesInfo(seriesInfo, argsReader, seriesAlloc)
						&& seriesInfo.maxMint == 0
						&& seriesInfo.maxSupply == 0
						&& seriesInfo.owner == senderPub
						&& seriesInfo.rom.numFields == 0
						&& seriesInfo.ram.numFields == 0;
				}

				if (fieldsOk)
				{
					const TokenSchemasOwned schemas = TokenSchemasBuilder::PrepareStandardTokenSchemas();
					ReadView metaReader(const_cast<uint8_t*>(seriesInfo.metadata.bytes), seriesInfo.metadata.length);
					VmDynamicStruct meta{};
					fieldsOk = Read(meta, schemas.view.seriesMetadata, metaReader, seriesAlloc)
						&& meta.numFields == 3
						&& ExpectInt256Field(meta, StandardMeta::id.c_str(), ToInt256(BigInteger::Parse(String("115792089237316195423570985008687907853269984665640564039457584007913129639935"))))
						&& ExpectInt8Field(meta, "mode", 0)
						&& ExpectBytesField(meta, "rom", ByteArray{});
				}

				if (fieldsOk)
				{
					CreateSeriesFeeOptions fees(10000, 2500000000ULL, 10000);
					fieldsOk = msg.maxGas == fees.CalculateMaxGas();
				}
			}

			Report(ctx, fieldsOk, "decode TX-CREATE-TOKEN-SERIES");
			if (fieldsOk)
			{
				const std::string got = ToUpper(BytesToHex(Blockchain::SerializeTx(msg)));
				Report(ctx, got == ToUpper(row.hex), "decode TX-CREATE-TOKEN-SERIES re-encode", got + " vs " + ToUpper(row.hex));
			}
		}
		else if (row.kind == "TX-MINT-NON-FUNGIBLE")
		{
			const std::string wif = "KwPpBSByydVKqStGHAnZzQofCqhDmD2bfRgc9BmZqM3ZmsdWJw4d";
			const PhantasmaKeys sender = PhantasmaKeys::FromWIF(wif.c_str(), (int)wif.size());
			const ByteArray senderKey = sender.GetPublicKey();
			const Bytes32 senderPub = ToBytes32(senderKey);

			ByteArray romStorage;
			ByteArray ramStorage;
			Allocator alloc;
			Blockchain::TxMsg msg{};
			const bool decoded = ReadTxMsg(msg, r, alloc, &romStorage, &ramStorage);
			const bool baseOk = decoded
				&& msg.type == Blockchain::TxTypes::MintNonFungible
				&& msg.expiry == 1759711416000LL
				&& msg.maxData == 100000000
				&& msg.gasFrom == senderPub
				&& msg.payload == SmallString("");

			bool fieldsOk = baseOk;
			if (fieldsOk)
			{
				fieldsOk = msg.mintNonFungible.tokenId == (uint64_t)-1
					&& msg.mintNonFungible.seriesId == 0xFFFFFFFFu
					&& msg.mintNonFungible.to == senderPub
					&& msg.mintNonFungible.ram.length == 0;
			}

			if (fieldsOk)
			{
				const TokenSchemasOwned schemas = TokenSchemasBuilder::PrepareStandardTokenSchemas();
				ReadView romReader(const_cast<uint8_t*>(msg.mintNonFungible.rom.bytes), msg.mintNonFungible.rom.length);
				VmDynamicStruct rom{};
				fieldsOk = Read(rom, schemas.view.rom, romReader, alloc)
					&& ExpectInt256Field(rom, StandardMeta::id.c_str(), ToInt256(BigInteger::Parse(String("115792089237316195423570985008687907853269984665640564039457584007913129639935"))))
					&& ExpectBytesField(rom, "rom", ByteArray{ (Byte)0x01, (Byte)0x42 })
					&& ExpectStringField(rom, "name", "My NFT #1")
					&& ExpectStringField(rom, "description", "This is my first NFT!")
					&& ExpectStringField(rom, "imageURL", "images-assets.nasa.gov/image/PIA13227/PIA13227~orig.jpg")
					&& ExpectStringField(rom, "infoURL", "https://images.nasa.gov/details/PIA13227")
					&& ExpectInt32Field(rom, "royalties", 10000000);
			}

			if (fieldsOk)
			{
				MintNftFeeOptions fees(10000, 1000);
				fieldsOk = msg.maxGas == fees.CalculateMaxGas();
			}

			Report(ctx, fieldsOk, "decode TX-MINT-NON-FUNGIBLE");
			if (fieldsOk)
			{
				const std::string got = ToUpper(BytesToHex(Blockchain::SerializeTx(msg)));
				Report(ctx, got == ToUpper(row.hex), "decode TX-MINT-NON-FUNGIBLE re-encode", got + " vs " + ToUpper(row.hex));
			}
		}
		else
		{
			Report(ctx, false, "decode " + row.kind, "unsupported kind");
		}
	}
}

} // namespace

void RunCarbonVectorTests(TestContext& ctx)
{
	const auto rows = LoadRows("fixtures/carbon_vectors.tsv");
	EncodeTests(ctx, rows);
	DecodeTests(ctx, rows);
}

} // namespace testcases
