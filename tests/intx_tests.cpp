#include "test_cases.h"

namespace testcases {
using namespace testutil;

void RunIntXIs8ByteSafeTests(TestContext& ctx)
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

} // namespace testcases
