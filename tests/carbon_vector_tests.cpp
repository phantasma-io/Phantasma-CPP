#include "test_cases.h"

namespace testcases {
using namespace testutil;

namespace {

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

} // namespace

void RunCarbonVectorTests(TestContext& ctx)
{
	const auto rows = LoadRows("fixtures/carbon_vectors.tsv");
	EncodeTests(ctx, rows);
	DecodeTests(ctx, rows);
}

} // namespace testcases
