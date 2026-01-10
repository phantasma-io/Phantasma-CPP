#include "test_cases.h"

int main()
{
	testutil::TestContext ctx;
	testcases::RunCarbonVectorTests(ctx);
	testcases::RunMetadataHelperTests(ctx);
	testcases::RunTokenMetadataIconTests(ctx);
	testcases::RunAddressTests(ctx);
	testcases::RunVmDynamicVariableTests(ctx);
	testcases::RunVmObjectTests(ctx);
	testcases::RunScriptBuilderTransactionTests(ctx);
	testcases::RunBigIntSerializationTests(ctx);
	testcases::RunIntXIs8ByteSafeTests(ctx);
	testcases::RunCallSectionsTests(ctx);

	if (ctx.failed == 0)
	{
		std::cout << "All " << ctx.total << " tests passed." << std::endl;
		return 0;
	}

	std::cerr << ctx.failed << " of " << ctx.total << " tests failed." << std::endl;
	return 1;
}
