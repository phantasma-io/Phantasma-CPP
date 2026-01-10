#include "test_cases.h"

namespace testcases {
using namespace testutil;

static ByteArray BuildTokenMetadata()
{
	const std::string png = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR4nGMAAQAABQABDQottAAAAABJRU5ErkJggg==";
	std::vector<std::pair<std::string, std::string>> fields = {
		{ "name", "My test token!" },
		{ "description", "My test token description" },
		{ "icon", png },
		{ "url", "http://example.com" },
	};
	return TokenMetadataBuilder::BuildAndSerialize(fields);
}

void RunTokenBuilderValidationTests(TestContext& ctx)
{
	const ByteArray metadata = BuildTokenMetadata();
	const Bytes32 creator{};
	const intx maxSupply = ParseIntx("0");

	ExpectThrowContains(ctx, "TokenInfoBuilder empty symbol", "Empty string is invalid", [&]() {
		TokenInfoBuilder::Build("", maxSupply, false, 0, creator, metadata, nullptr);
	});
	ExpectThrowContains(ctx, "TokenInfoBuilder long symbol", "Too long", [&]() {
		TokenInfoBuilder::Build(std::string(256, 'A'), maxSupply, false, 0, creator, metadata, nullptr);
	});
	ExpectThrowContains(ctx, "TokenInfoBuilder invalid symbol", "Anything outside A-Z", [&]() {
		TokenInfoBuilder::Build("AB1", maxSupply, false, 0, creator, metadata, nullptr);
	});
	ExpectThrowContains(ctx, "TokenInfoBuilder metadata required", "metadata is required", [&]() {
		const ByteArray empty;
		TokenInfoBuilder::Build("ABC", maxSupply, false, 0, creator, empty, nullptr);
	});

	const intx bigSupply = ParseIntx("9223372036854775808");
	const ByteArray tokenSchemas = TokenSchemasBuilder::BuildAndSerialize(nullptr);
	ExpectThrowContains(ctx, "TokenInfoBuilder NFT supply Int64", "NFT maximum supply must fit into Int64", [&]() {
		TokenInfoBuilder::Build("NFT", bigSupply, true, 0, creator, metadata, &tokenSchemas);
	});
	ExpectThrowContains(ctx, "TokenInfoBuilder NFT schemas required", "tokenSchemas is required", [&]() {
		TokenInfoBuilder::Build("NFT", maxSupply, true, 0, creator, metadata, nullptr);
	});
	ExpectNoThrow(ctx, "TokenInfoBuilder valid fungible", [&]() {
		TokenInfoBuilder::Build("FUNGIBLE", maxSupply, false, 8, creator, metadata, nullptr);
	});

	ExpectThrowContains(ctx, "SeriesInfoBuilder metadata required", "series metadata is required", [&]() {
		const int256 seriesId = int256(1);
		SeriesInfoBuilder::Build(seriesId, 1, 1, creator, nullptr);
	});

	ExpectThrowContains(ctx, "TokenSchemasBuilder missing metadata", "Mandatory metadata field not found: name", [&]() {
		TokenSchemasBuilder::BuildFromFields({}, {}, {});
	});
	ExpectThrowContains(ctx, "TokenSchemasBuilder type mismatch", "Type mismatch for field name", [&]() {
		std::vector<FieldType> seriesFields = { FieldType{ "name", VmType::Int32 } };
		TokenSchemasBuilder::BuildFromFields(seriesFields, {}, {});
	});
	ExpectThrowContains(ctx, "TokenSchemasBuilder case mismatch", "Case mismatch for field name", [&]() {
		std::vector<FieldType> seriesFields = { FieldType{ "Name", VmType::String } };
		TokenSchemasBuilder::BuildFromFields(seriesFields, {}, {});
	});
}

} // namespace testcases
