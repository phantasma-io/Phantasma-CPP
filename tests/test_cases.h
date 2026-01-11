#pragma once

#include "test_common.h"

namespace testcases {

void RunCarbonVectorTests(testutil::TestContext& ctx);
void RunCallSectionsTests(testutil::TestContext& ctx);
void RunVmObjectTests(testutil::TestContext& ctx);
void RunVmDynamicVariableTests(testutil::TestContext& ctx);
void RunMetadataHelperTests(testutil::TestContext& ctx);
void RunTokenMetadataIconTests(testutil::TestContext& ctx);
void RunAddressTests(testutil::TestContext& ctx);
void RunScriptBuilderTransactionTests(testutil::TestContext& ctx);
void RunBigIntSerializationTests(testutil::TestContext& ctx);
void RunBigIntOperationFixtureTests(testutil::TestContext& ctx);
void RunIntXIs8ByteSafeTests(testutil::TestContext& ctx);
void RunTokenBuilderValidationTests(testutil::TestContext& ctx);
void RunEncodingRoundtripTests(testutil::TestContext& ctx);
void RunKeyPairTests(testutil::TestContext& ctx);
void RunCarbonTxExtraTests(testutil::TestContext& ctx);

} // namespace testcases
