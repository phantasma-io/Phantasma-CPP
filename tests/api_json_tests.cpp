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
	{
		// Verify the current RPC shape includes the chain before the block hash.
		JSONBuilder request;
		rpc::PhantasmaJsonAPI::MakeGetBlockTransactionCountByHashRequest(request, "custom", "ABCDEF");
		const std::string json = request.s.str();
		Report(ctx,
		    json.find("\"method\": \"getBlockTransactionCountByHash\"") != std::string::npos &&
		        json.find("\"params\": [\"custom\", \"ABCDEF\"]") != std::string::npos,
		    "API GetBlockTransactionCountByHash request includes chain parameter");
	}
	{
		// Verify transaction lookup by block hash uses the chain-aware parameter order.
		JSONBuilder request;
		rpc::PhantasmaJsonAPI::MakeGetTransactionByBlockHashAndIndexRequest(request, "custom", "ABCDEF", 7);
		const std::string json = request.s.str();
		Report(ctx,
		    json.find("\"method\": \"getTransactionByBlockHashAndIndex\"") != std::string::npos &&
		        json.find("\"params\": [\"custom\", \"ABCDEF\", 7]") != std::string::npos,
		    "API GetTransactionByBlockHashAndIndex request includes chain parameter");
	}
	{
		// Verify Carbon token-series lookup sends both Phantasma and Carbon identifiers.
		JSONBuilder request;
		rpc::PhantasmaJsonAPI::MakeGetTokenSeriesByIdRequest(request, "CROWN", 17, "series-alpha", 3);
		const std::string json = request.s.str();
		Report(ctx,
		    json.find("\"method\": \"getTokenSeriesById\"") != std::string::npos &&
		        json.find("\"params\": [\"CROWN\", 17, \"series-alpha\", 3]") != std::string::npos,
		    "API GetTokenSeriesById request uses both token and series identifiers");
	}
	{
		// Verify build metadata parsing for the getVersion RPC helper.
		rpc::PhantasmaError err{};
		rpc::BuildInfoResult build{};
		const JSONDocument doc = R"({"result":{"version":"1.2.3","commit":"abcdef","buildTimeUtc":"2026-04-28T00:00:00Z"}})";
		const bool ok = rpc::PhantasmaJsonAPI::ParseGetVersionResponse(json::Parse(doc), build, &err);
		Report(ctx,
		    ok && err.code == 0 && build.version == "1.2.3" && build.commit == "abcdef" && build.buildTimeUtc == "2026-04-28T00:00:00Z",
		    "API GetVersion parser reads build metadata");
	}
	{
		// Verify VM gas/config parsing for the getPhantasmaVmConfig RPC helper.
		rpc::PhantasmaError err{};
		rpc::PhantasmaVmConfig config{};
		const JSONDocument doc = R"({"result":{"isStored":true,"featureLevel":2,"gasConstructor":"1","gasNexus":"2","gasOrganization":"3","gasAccount":"4","gasLeaderboard":"5","gasStandard":"6","gasOracle":"7","fuelPerContractDeploy":"8"}})";
		const bool ok = rpc::PhantasmaJsonAPI::ParseGetPhantasmaVmConfigResponse(json::Parse(doc), config, &err);
		Report(ctx,
		    ok && err.code == 0 && config.isStored && config.featureLevel == 2 && config.gasConstructor == "1" &&
		        config.gasNexus == "2" && config.gasOrganization == "3" && config.gasAccount == "4" &&
		        config.gasLeaderboard == "5" && config.gasStandard == "6" && config.gasOracle == "7" &&
		        config.fuelPerContractDeploy == "8",
		    "API GetPhantasmaVmConfig parser reads VM config");
	}
}

} // namespace testcases
