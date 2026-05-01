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
	{
		// Verify current Carbon NFT identity fields are preserved from TokenData responses.
		rpc::PhantasmaError err{};
		rpc::TokenData token{};
		const JSONDocument doc = R"({"result":{"id":"1001","series":"55","carbonTokenId":"4","carbonSeriesId":"7","carbonNftAddress":"ABCDEF","mint":"42","chainName":"main","ownerAddress":"P-owner","creatorAddress":"P-creator","ram":"","rom":"CAFE","status":"Active","infusion":[],"properties":[{"key":"Name","value":"Crown #42"}]}})";
		const bool ok = rpc::PhantasmaJsonAPI::ParseGetTokenDataResponse(json::Parse(doc), token, &err);
		Report(ctx,
		    ok && err.code == 0 && token.ID == "1001" && token.series == "55" && token.carbonTokenId == "4" &&
		        token.carbonSeriesId == "7" && token.carbonNftAddress == "ABCDEF" && token.properties.size() == 1 &&
		        token.properties[0].Key == "Name",
		    "API GetTokenData parser preserves Carbon NFT identity");
	}
	{
		// Verify the current TokenSeries response shape parses without legacy-only fields.
		rpc::PhantasmaError err{};
		rpc::TokenSeries series{};
		const JSONDocument doc = R"({"result":{"seriesId":"55","carbonTokenId":"4","carbonSeriesId":"7","ownerAddress":"P-owner","maxMint":"100","mintCount":"42","currentSupply":"41","maxSupply":"100","metadata":[{"key":"Name","value":"Series 55"}]}})";
		const bool ok = rpc::PhantasmaJsonAPI::ParseGetTokenSeriesByIdResponse(json::Parse(doc), series, &err);
		Report(ctx,
		    ok && err.code == 0 && series.seriesId == "55" && series.carbonTokenId == "4" &&
		        series.carbonSeriesId == "7" && series.ownerAddress == "P-owner" && series.maxMint == "100" &&
		        series.mintCount == "42" && series.currentSupply == "41" && series.maxSupply == "100" &&
		        series.mode == rpc::TokenSeriesMode::Unique && series.metadata.size() == 1 &&
		        series.metadata[0].Value == "Series 55",
		    "API GetTokenSeriesById parser accepts current Carbon series shape");
	}
	{
		// Verify Carbon transaction metadata is not dropped from Transaction responses.
		rpc::PhantasmaError err{};
		rpc::Transaction tx{};
		const JSONDocument doc = R"({"result":{"hash":"HASH","chainAddress":"CHAIN","timestamp":123,"blockHeight":456,"blockHash":"BLOCK","script":"","payload":"CAFE","carbonTxType":1,"carbonTxData":"BEEF","events":[],"result":"","fee":"0","signatures":[],"expiration":789,"state":"Halt","sender":"P-sender","gasPayer":"P-gas","gasTarget":"P-target","gasPrice":"1","gasLimit":"1000"}})";
		const bool ok = rpc::PhantasmaJsonAPI::ParseGetTransactionResponse(json::Parse(doc), tx, &err);
		Report(ctx,
		    ok && err.code == 0 && tx.hash == "HASH" && tx.payload == "CAFE" && tx.carbonTxType == 1 &&
		        tx.carbonTxData == "BEEF" && tx.gasPayer == "P-gas" && tx.expiration == 789,
		    "API GetTransaction parser preserves Carbon transaction metadata");
	}
}

} // namespace testcases
