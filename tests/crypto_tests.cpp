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
	Report(ctx, sig.VerifyIndex(message.data(), (int)message.size(), addresses, 1) == 0, "Ed25519 signature verify index");

	const Signature wrapped(sig);
	Report(ctx, wrapped.Verify(message.data(), (int)message.size(), address), "Signature verify");
	Report(ctx, wrapped.VerifyIndex(message.data(), (int)message.size(), addresses, 1) == 0, "Signature verify index");
	Report(ctx, 0 == std::memcmp(wrapped.GetEd25519Signature().Bytes(), sig.Bytes(), Ed25519Signature::Length), "Signature exposes Ed25519 bytes");

	const char badText[] = "hello worlds";
	const ByteArray badMessage((const Byte*)badText, (const Byte*)badText + sizeof(badText) - 1);
	const bool badOk = sig.Verify(badMessage.data(), (int)badMessage.size(), addresses, 1);
	Report(ctx, !badOk, "Ed25519 signature mismatch");
	Report(ctx, sig.VerifyIndex(badMessage.data(), (int)badMessage.size(), addresses, 1) == -1, "Ed25519 signature mismatch index");
	Report(ctx, wrapped.VerifyIndex(badMessage.data(), (int)badMessage.size(), addresses, 1) == -1, "Signature mismatch index");
}

} // namespace testcases
