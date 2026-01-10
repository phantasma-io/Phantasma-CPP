#include "test_cases.h"

#include "../include/Numerics/Base58.h"
#include "../include/Numerics/Base64.h"

namespace testcases {
using namespace testutil;

void RunEncodingRoundtripTests(TestContext& ctx)
{
	const ByteArray data = { 0x00, 0x00, 0x01, 0x02, 0x03, 0xFF, 0x10, 0x20 };

	{
		const std::string hex = BytesToHex(data);
		const ByteArray decoded = Base16::Decode(hex.c_str(), (int)hex.size());
		Report(ctx, decoded == data, "Base16 roundtrip");
	}
	{
		const auto encoded = Base58::Encode(data.empty() ? nullptr : data.data(), (int)data.size());
		const ByteArray decoded = Base58::Decode(encoded);
		Report(ctx, decoded == data, "Base58 roundtrip");
	}
	{
		const auto encoded = Base64::Encode(data);
		const ByteArray decoded = Base64::Decode(encoded);
		Report(ctx, decoded == data, "Base64 roundtrip");
	}
}

} // namespace testcases
