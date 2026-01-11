#include "test_cases.h"

int main()
{
	testutil::TestContext ctx;
	testcases::RunCarbonVectorTests(ctx);
	testcases::RunMetadataHelperTests(ctx);
	testcases::RunTokenMetadataIconTests(ctx);
	testcases::RunTokenBuilderValidationTests(ctx);
	testcases::RunAddressTests(ctx);
	testcases::RunKeyPairTests(ctx);
	testcases::RunEncodingRoundtripTests(ctx);
	testcases::RunVmDynamicVariableTests(ctx);
	testcases::RunVmObjectTests(ctx);
	testcases::RunScriptBuilderTransactionTests(ctx);
	testcases::RunCarbonTxExtraTests(ctx);
	testcases::RunBigIntSerializationTests(ctx);
	testcases::RunBigIntOperationFixtureTests(ctx);
	testcases::RunBigIntBitwiseFixtureTests(ctx);
	testcases::RunBigIntPowFixtureTests(ctx);
	testcases::RunBigIntModPowFixtureTests(ctx);
	testcases::RunBigIntModInverseFixtureTests(ctx);
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
