#include "test_cases.h"

namespace testcases {
using namespace testutil;

void RunMetadataHelperTests(TestContext& ctx)
{
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("royalties", VmType::Int32);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "royalties", MetadataValue::FromInt64(42) } };
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const bool ok = fields.size() == 1 &&
			fields[0].value.type == VmType::Int32 &&
			(int32_t)fields[0].value.data.int32 == 42;
		Report(ctx, ok, "MetadataHelper Int32 accepts");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("royalties", VmType::Int32);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "royalties", MetadataValue::FromString("forty-two") } };
		ExpectThrowContains(ctx, "MetadataHelper Int32 non-number", "must be a number", [&]() {
			MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		});
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("royalties", VmType::Int32);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "royalties", MetadataValue::FromUInt64(0x100000000ULL) } };
		ExpectThrowContains(ctx, "MetadataHelper Int32 range", "between -2147483648", [&]() {
			MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		});
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("payload", VmType::Bytes);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "payload", MetadataValue::FromString("0a0b") } };
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const ByteArray got = BytesFromView(fields[0].value.data.bytes);
		const bool ok = fields[0].value.type == VmType::Bytes && got == ByteArray({ (Byte)0x0A, (Byte)0x0B });
		Report(ctx, ok, "MetadataHelper Bytes hex");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("payload", VmType::Bytes);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "payload", MetadataValue::FromString("0x0a0b") } };
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const ByteArray got = BytesFromView(fields[0].value.data.bytes);
		const bool ok = fields[0].value.type == VmType::Bytes && got == ByteArray({ (Byte)0x0A, (Byte)0x0B });
		Report(ctx, ok, "MetadataHelper Bytes hex 0x");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("level", VmType::Int8);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "level", MetadataValue::FromInt64(200) } };
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const bool ok = fields[0].value.type == VmType::Int8 && fields[0].value.data.int8 == 200;
		Report(ctx, ok, "MetadataHelper Int8 unsigned");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("checksum", VmType::Int16);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "checksum", MetadataValue::FromInt64(65535) } };
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const bool ok = fields[0].value.type == VmType::Int16 && fields[0].value.data.int16 == 65535;
		Report(ctx, ok, "MetadataHelper Int16 unsigned");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("payload", VmType::Bytes);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "payload", MetadataValue::FromString("xyz") } };
		ExpectThrowContains(ctx, "MetadataHelper Bytes invalid hex", "byte array or hex string", [&]() {
			MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		});
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("supply", VmType::Int64);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = { { "supply", MetadataValue::FromUInt64(std::numeric_limits<uint64_t>::max()) } };
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const bool ok = fields[0].value.type == VmType::Int64 &&
			fields[0].value.data.int64 == std::numeric_limits<uint64_t>::max();
		Report(ctx, ok, "MetadataHelper Int64 unsigned");
	}
	{
		Allocator alloc;
		VmNamedVariableSchema nestedFields[] = {
			MakeSchema("innerName", VmType::String),
			MakeSchema("innerValue", VmType::Int32),
		};
		const VmStructSchema nestedSchema = MakeStructSchema(nestedFields, 2, false);
		const VmNamedVariableSchema schema = MakeSchema("details", VmType::Struct, &nestedSchema);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "details", MetadataValue::FromStruct({
				{ "innerName", MetadataValue::FromString("demo") },
				{ "innerValue", MetadataValue::FromInt64(5) },
			}) }
		};
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const VmDynamicStruct& nested = fields[0].value.data.structure;
		const VmDynamicVariable* innerName = nested[SmallString("innerName")];
		const VmDynamicVariable* innerValue = nested[SmallString("innerValue")];
		const bool ok = innerName && innerValue &&
			innerName->type == VmType::String &&
			std::string(innerName->data.string) == "demo" &&
			innerValue->type == VmType::Int32 &&
			(int32_t)innerValue->data.int32 == 5;
		Report(ctx, ok, "MetadataHelper Struct nested");
	}
	{
		Allocator alloc;
		VmNamedVariableSchema nestedFields[] = {
			MakeSchema("innerName", VmType::String),
		};
		const VmStructSchema nestedSchema = MakeStructSchema(nestedFields, 1, false);
		const VmNamedVariableSchema schema = MakeSchema("details", VmType::Struct, &nestedSchema);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "details", MetadataValue::FromStruct({
				{ "innerName", MetadataValue::FromString("demo") },
				{ "extra", MetadataValue::FromString("oops") },
			}) }
		};
		ExpectThrowContains(ctx, "MetadataHelper Struct unknown", "received unknown property", [&]() {
			MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		});
	}
	{
		Allocator alloc;
		VmNamedVariableSchema nestedFields[] = {
			MakeSchema("innerName", VmType::String),
		};
		const VmStructSchema nestedSchema = MakeStructSchema(nestedFields, 1, false);
		const VmNamedVariableSchema schema = MakeSchema("details", VmType::Struct, &nestedSchema);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "details", MetadataValue::FromStruct({}) }
		};
		ExpectThrowContains(ctx, "MetadataHelper Struct missing", "is mandatory", [&]() {
			MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		});
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("tags", VmType::Array_String);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "tags", MetadataValue::FromArray({
				MetadataValue::FromString("alpha"),
				MetadataValue::FromString("beta"),
			}) }
		};
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const VmDynamicVariable& value = fields[0].value;
		const bool ok = value.type == VmType::Array_String &&
			value.arrayLength == 2 &&
			std::string(value.data.stringArray[0]) == "alpha" &&
			std::string(value.data.stringArray[1]) == "beta";
		Report(ctx, ok, "MetadataHelper Array string");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("deltas", VmType::Array_Int8);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "deltas", MetadataValue::FromArray({
				MetadataValue::FromInt64(1),
				MetadataValue::FromInt64(-1),
				MetadataValue::FromInt64(5),
			}) }
		};
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const VmDynamicVariable& value = fields[0].value;
		const bool ok = value.type == VmType::Array_Int8 &&
			value.arrayLength == 3 &&
			value.data.int8Array &&
			value.data.int8Array[0] == 1 &&
			value.data.int8Array[1] == 255 &&
			value.data.int8Array[2] == 5;
		Report(ctx, ok, "MetadataHelper Array Int8");
	}
	{
		Allocator alloc;
		VmNamedVariableSchema elementFields[] = {
			MakeSchema("name", VmType::String),
		};
		const VmStructSchema elementSchema = MakeStructSchema(elementFields, 1, false);
		const VmNamedVariableSchema schema = MakeSchema("items", VmType::Array_Struct, &elementSchema);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "items", MetadataValue::FromArray({
				MetadataValue::FromStruct({ { "name", MetadataValue::FromString("one") } }),
				MetadataValue::FromStruct({ { "name", MetadataValue::FromString("two") } }),
			}) }
		};
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const VmDynamicVariable& value = fields[0].value;
		const VmStructArray& arrayValue = value.data.structureArray;
		const VmDynamicVariable* firstName = arrayValue.structs[0][SmallString("name")];
		const VmDynamicVariable* secondName = arrayValue.structs[1][SmallString("name")];
		const bool ok = value.type == VmType::Array_Struct &&
			value.arrayLength == 2 &&
			arrayValue.schema.numFields == 1 &&
			std::string(arrayValue.schema.fields[0].name.c_str()) == "name" &&
			firstName && secondName &&
			std::string(firstName->data.string) == "one" &&
			std::string(secondName->data.string) == "two";
		Report(ctx, ok, "MetadataHelper Array struct");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("hash", VmType::Bytes16);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "hash", MetadataValue::FromString("00112233445566778899aabbccddeeff") }
		};
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const Bytes16 expected(HexToBytes("00112233445566778899aabbccddeeff"));
		const bool ok = fields[0].value.type == VmType::Bytes16 && fields[0].value.data.bytes16 == expected;
		Report(ctx, ok, "MetadataHelper Bytes16");
	}
	{
		Allocator alloc;
		const VmNamedVariableSchema schema = MakeSchema("roots", VmType::Array_Bytes32);
		std::vector<VmNamedDynamicVariable> fields;
		std::vector<MetadataField> metadata = {
			{ "roots", MetadataValue::FromArray({
				MetadataValue::FromString("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"),
				MetadataValue::FromString("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"),
			}) }
		};
		MetadataHelper::PushMetadataField(schema, fields, metadata, alloc);
		const Bytes32 expectedA(HexToBytes("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"));
		const Bytes32 expectedB(HexToBytes("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));
		const VmDynamicVariable& value = fields[0].value;
		const bool ok = value.type == VmType::Array_Bytes32 &&
			value.arrayLength == 2 &&
			value.data.bytes32Array &&
			value.data.bytes32Array[0] == expectedA &&
			value.data.bytes32Array[1] == expectedB;
		Report(ctx, ok, "MetadataHelper Array Bytes32");
	}
}

void RunTokenMetadataIconTests(TestContext& ctx)
{
	const std::string png = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR4nGMAAQAABQABDQottAAAAABJRU5ErkJggg==";
	const std::string webp = "data:image/webp;base64,UklGRg==";
	const std::string svg = "data:image/svg+xml;base64,PHN2ZyB4bWxucz0naHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmcnIHZpZXdCb3g9JzAgMCAyNCAyNCc+PHBhdGggZmlsbD0nI0Y0NDMzNicgZD0nTTcgNGg1YTUgNSAwIDAxMCAxMEg5djZIN3pNOSA2djZoM2EzIDMgMCAwMDAtNnonLz48L3N2Zz4=";
	const std::string legacySvg = "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'%3E%3Cpath fill='%23F44336' d='M7 4h5a5 5 0 010 10H9v6H7zM9 6v6h3a3 3 0 000-6z'/%3E%3C/svg%3E";
	const std::string gif = "data:image/gif;base64,R0lGODlhAQABAIAAAAAAAAAAACH5BAAAAAAALAAAAAABAAEAAAICRAEAOw==";
	const std::string emptyPayload = "data:image/png;base64,";
	const std::string invalidPayload = "data:image/jpeg;base64,@@@";

	auto buildFields = [&](const std::string& icon) {
		return std::vector<std::pair<std::string, std::string>>{
			{ "name", "My test token!" },
			{ "icon", icon },
			{ "url", "http://example.com" },
			{ "description", "My test token description" },
		};
	};

	ExpectNoThrow(ctx, "TokenMetadata icon PNG", [&]() {
		TokenMetadataBuilder::BuildAndSerialize(buildFields(png));
	});
	ExpectNoThrow(ctx, "TokenMetadata icon JPEG", [&]() {
		const std::string jpegPayload = "/9j/";
		TokenMetadataBuilder::BuildAndSerialize(buildFields("data:image/jpeg;base64," + jpegPayload));
	});
	ExpectNoThrow(ctx, "TokenMetadata icon WebP", [&]() {
		TokenMetadataBuilder::BuildAndSerialize(buildFields(webp));
	});
	ExpectThrowContains(ctx, "TokenMetadata icon SVG", "base64-encoded data URI", [&]() {
		TokenMetadataBuilder::BuildAndSerialize(buildFields(svg));
	});
	ExpectThrowContains(ctx, "TokenMetadata icon legacy svg", "base64-encoded data URI", [&]() {
		TokenMetadataBuilder::BuildAndSerialize(buildFields(legacySvg));
	});
	ExpectThrowContains(ctx, "TokenMetadata icon GIF", "base64-encoded data URI", [&]() {
		TokenMetadataBuilder::BuildAndSerialize(buildFields(gif));
	});
	ExpectThrowContains(ctx, "TokenMetadata icon empty", "non-empty base64 payload", [&]() {
		TokenMetadataBuilder::BuildAndSerialize(buildFields(emptyPayload));
	});
	ExpectThrowContains(ctx, "TokenMetadata icon invalid base64", "payload is not valid base64", [&]() {
		TokenMetadataBuilder::BuildAndSerialize(buildFields(invalidPayload));
	});
}

} // namespace testcases
