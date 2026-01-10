#include "test_cases.h"

namespace testcases {
using namespace testutil;

void RunCarbonTxExtraTests(TestContext& ctx)
{
	const std::string senderWif = "KwPpBSByydVKqStGHAnZzQofCqhDmD2bfRgc9BmZqM3ZmsdWJw4d";
	const std::string receiverWif = "KwVG94yjfVg1YKFyRxAGtug93wdRbmLnqqrFV6Yd2CiA9KZDAp4H";
	const PhantasmaKeys sender = PhantasmaKeys::FromWIF(senderWif.c_str(), (int)senderWif.size());
	const PhantasmaKeys receiver = PhantasmaKeys::FromWIF(receiverWif.c_str(), (int)receiverWif.size());
	const Bytes32 senderPub = ToBytes32(sender.GetPublicKey());
	const Bytes32 receiverPub = ToBytes32(receiver.GetPublicKey());

	const int64_t expiry = 1759711416000;
	const uint64_t maxGas = 10000000;
	const uint64_t maxData = 1000;
	const SmallString payload("test-payload");
	const intx amount = ParseIntx("100000000");

	{
		Blockchain::TxMsg msg;
		msg.type = Blockchain::TxTypes::TransferFungible_GasPayer;
		msg.expiry = expiry;
		msg.maxGas = maxGas;
		msg.maxData = maxData;
		msg.gasFrom = senderPub;
		msg.payload = payload;
		msg.transferFtGasPayer = Blockchain::TxMsgTransferFungible_GasPayer{ receiverPub, senderPub, 1, 100000000 };

		const std::string expected =
			"04C04EF9B6990100008096980000000000E803000000000000F94A8E45BDF1E37A8466B951849E92D1BAF870F49D1D04CD204D0BC9FE4308960C746573742D7061796C6F6164D4C5061B81C4682B27A0CFC6459CD9D7892EB60A43F73DD1060B6C478AA7C3D8F94A8E45BDF1E37A8466B951849E92D1BAF870F49D1D04CD204D0BC9FE430896010000000000000000E1F50500000000";
		const std::string got = ToUpper(BytesToHex(CarbonSerialize(msg)));
		Report(ctx, got == expected, "TxMsg TransferFungible GasPayer vector", got + " vs " + expected);
	}

	{
		Blockchain::TxMsg msg;
		msg.type = Blockchain::TxTypes::BurnFungible_GasPayer;
		msg.expiry = expiry;
		msg.maxGas = maxGas;
		msg.maxData = maxData;
		msg.gasFrom = senderPub;
		msg.payload = payload;
		msg.burnFungibleGasPayer = Blockchain::TxMsgBurnFungible_GasPayer{ 1, (const intx_pod&)amount, senderPub };

		const std::string expected =
			"0BC04EF9B6990100008096980000000000E803000000000000F94A8E45BDF1E37A8466B951849E92D1BAF870F49D1D04CD204D0BC9FE4308960C746573742D7061796C6F61640100000000000000F94A8E45BDF1E37A8466B951849E92D1BAF870F49D1D04CD204D0BC9FE4308960800E1F50500000000";
		const std::string got = ToUpper(BytesToHex(CarbonSerialize(msg)));
		Report(ctx, got == expected, "TxMsg BurnFungible GasPayer vector", got + " vs " + expected);
	}

	{
		Blockchain::TxMsg msg;
		msg.type = Blockchain::TxTypes::MintFungible;
		msg.expiry = expiry;
		msg.maxGas = maxGas;
		msg.maxData = maxData;
		msg.gasFrom = senderPub;
		msg.payload = payload;
		msg.mintFungible = Blockchain::TxMsgMintFungible{ 1, (const intx_pod&)amount, receiverPub };

		const std::string expected =
			"09C04EF9B6990100008096980000000000E803000000000000F94A8E45BDF1E37A8466B951849E92D1BAF870F49D1D04CD204D0BC9FE4308960C746573742D7061796C6F61640100000000000000D4C5061B81C4682B27A0CFC6459CD9D7892EB60A43F73DD1060B6C478AA7C3D80800E1F50500000000";
		const std::string got = ToUpper(BytesToHex(CarbonSerialize(msg)));
		Report(ctx, got == expected, "TxMsg MintFungible vector", got + " vs " + expected);
	}
}

} // namespace testcases
