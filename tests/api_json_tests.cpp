#define PHANTASMA_IMPLEMENTATION
#include "test_cases.h"

namespace testcases {
using namespace testutil;

void RunApiJsonNumericFlexTests(TestContext& ctx)
{
	{
		bool err = false;
		const JSONDocument doc = R"({"result":"321"})";
		const Int32 value = json::LookupInt32(json::Parse(doc), PHANTASMA_LITERAL("result"), err);
		Report(ctx, !err && value == 321, "API JSON LookupInt32 accepts quoted numeric");
	}
	{
		bool err = false;
		const Int64 value = json::AsInt64(JSONValue{ "\"-42\"" }, err);
		Report(ctx, !err && value == -42, "API JSON AsInt64 accepts quoted numeric");
	}
	{
		bool err = false;
		const UInt64 value = json::AsUInt64(JSONValue{ "\"18446744073709551615\"" }, err);
		Report(ctx, !err && value == std::numeric_limits<UInt64>::max(), "API JSON AsUInt64 accepts quoted numeric");
	}
	{
		rpc::PhantasmaError err{};
		Int32 height = 0;
		const JSONDocument doc = R"({"result":"12345"})";
		const bool ok = rpc::PhantasmaJsonAPI::ParseGetBlockHeightResponse(json::Parse(doc), height, &err);
		Report(ctx, ok && err.code == 0 && height == 12345, "API GetBlockHeight parser accepts quoted numeric");
	}
}

} // namespace testcases
