#ifndef PHANTASMA_EXCEPTION_ENABLE
#define PHANTASMA_EXCEPTION_ENABLE
#endif

#include "test_cases.h"

int main()
{
	testutil::TestContext ctx;
	testcases::RunGen2CSharpVmBigIntFixtureTests(ctx);

	if( ctx.failed == 0 )
	{
		std::cout << "All " << ctx.total << " VM bigint parity tests passed." << std::endl;
		return 0;
	}

	std::cerr << ctx.failed << " of " << ctx.total << " VM bigint parity tests failed." << std::endl;
	return 1;
}
