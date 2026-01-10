#include "test_cases.h"

namespace testcases {
using namespace testutil;

void RunKeyPairTests(TestContext& ctx)
{
	const std::string wif = "KwPpBSByydVKqStGHAnZzQofCqhDmD2bfRgc9BmZqM3ZmsdWJw4d";
	const PhantasmaKeys keys = PhantasmaKeys::FromWIF(wif.c_str(), (int)wif.size());
	const std::string wifBack = std::string(keys.ToWIF().c_str());
	Report(ctx, wifBack == wif, "PhantasmaKeys WIF roundtrip", wifBack);

	const char msgText[] = "hello world";
	const ByteArray message((const Byte*)msgText, (const Byte*)msgText + sizeof(msgText) - 1);
	const Ed25519Signature sig = keys.Sign(message);
	const Address address = keys.GetAddress();
	const Address addresses[] = { address };
	const bool ok = sig.Verify(message.data(), (int)message.size(), addresses, 1);
	Report(ctx, ok, "Ed25519 signature verify");

	const char badText[] = "hello worlds";
	const ByteArray badMessage((const Byte*)badText, (const Byte*)badText + sizeof(badText) - 1);
	const bool badOk = sig.Verify(badMessage.data(), (int)badMessage.size(), addresses, 1);
	Report(ctx, !badOk, "Ed25519 signature mismatch");
}

} // namespace testcases
