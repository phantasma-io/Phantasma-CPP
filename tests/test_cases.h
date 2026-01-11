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
void RunBigIntBitwiseFixtureTests(testutil::TestContext& ctx);
void RunBigIntPowFixtureTests(testutil::TestContext& ctx);
void RunBigIntModPowFixtureTests(testutil::TestContext& ctx);
void RunBigIntModInverseFixtureTests(testutil::TestContext& ctx);
void RunBigIntParseFormatTests(testutil::TestContext& ctx);
void RunBigIntByteArrayTests(testutil::TestContext& ctx);
void RunBigIntBitHelperTests(testutil::TestContext& ctx);
void RunBigIntConstructorTests(testutil::TestContext& ctx);
void RunBigIntOperatorTests(testutil::TestContext& ctx);
void RunSecureBigIntTests(testutil::TestContext& ctx);
void RunBigIntMultiWordTests(testutil::TestContext& ctx);
void RunIntXIs8ByteSafeTests(testutil::TestContext& ctx);
void RunTokenBuilderValidationTests(testutil::TestContext& ctx);
void RunEncodingRoundtripTests(testutil::TestContext& ctx);
void RunKeyPairTests(testutil::TestContext& ctx);
void RunCarbonTxExtraTests(testutil::TestContext& ctx);

} // namespace testcases
