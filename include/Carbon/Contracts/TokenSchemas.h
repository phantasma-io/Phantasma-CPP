#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "../DataBlockchain.h"
#include "../../Numerics/Base16.h"

#ifdef PHANTASMA_RAPIDJSON
#include "rapidjson/document.h"
#endif

namespace phantasma::carbon {

struct TokenSchemas
{
	VmStructSchema seriesMetadata{};
	VmStructSchema rom{};
	VmStructSchema ram{};
};

inline void Write(const TokenSchemas& in, WriteView& w);

struct TokenSchemasOwned
{
	TokenSchemas view{};
	std::vector<VmNamedVariableSchema> seriesFields;
	std::vector<VmNamedVariableSchema> romFields;
	std::vector<VmNamedVariableSchema> ramFields;

	TokenSchemasOwned() = default;

	TokenSchemas View() const { return view; }
};

struct FieldType
{
	std::string name;
	VmType type = VmType::Dynamic;
};

struct MetadataValue
{
	enum class Kind
	{
		Null,
		String,
		Bytes,
		Bytes16,
		Bytes32,
		Bytes64,
		Int64,
		UInt64,
		Int256,
		UInt256,
		Struct,
		Array,
	};

	Kind kind = Kind::Null;
	std::string stringValue{};
	ByteArray bytesValue{};
	Bytes16 bytes16Value{};
	Bytes32 bytes32Value{};
	Bytes64 bytes64Value{};
	int64_t int64Value = 0;
	uint64_t uint64Value = 0;
	int256 int256Value{};
	uint256 uint256Value{};
	std::vector<MetadataValue> arrayValue{};
	std::vector<std::pair<std::string, MetadataValue>> structValue{};

	static MetadataValue FromString(const std::string& value)
	{
		MetadataValue out;
		out.kind = Kind::String;
		out.stringValue = value;
		return out;
	}

	static MetadataValue FromBytes(const ByteArray& value)
	{
		MetadataValue out;
		out.kind = Kind::Bytes;
		out.bytesValue = value;
		return out;
	}

	static MetadataValue FromBytes16(const Bytes16& value)
	{
		MetadataValue out;
		out.kind = Kind::Bytes16;
		out.bytes16Value = value;
		return out;
	}

	static MetadataValue FromBytes32(const Bytes32& value)
	{
		MetadataValue out;
		out.kind = Kind::Bytes32;
		out.bytes32Value = value;
		return out;
	}

	static MetadataValue FromBytes64(const Bytes64& value)
	{
		MetadataValue out;
		out.kind = Kind::Bytes64;
		out.bytes64Value = value;
		return out;
	}

	static MetadataValue FromInt64(int64_t value)
	{
		MetadataValue out;
		out.kind = Kind::Int64;
		out.int64Value = value;
		return out;
	}

	static MetadataValue FromUInt64(uint64_t value)
	{
		MetadataValue out;
		out.kind = Kind::UInt64;
		out.uint64Value = value;
		return out;
	}

	static MetadataValue FromInt256(const int256& value)
	{
		MetadataValue out;
		out.kind = Kind::Int256;
		out.int256Value = value;
		return out;
	}

	static MetadataValue FromUInt256(const uint256& value)
	{
		MetadataValue out;
		out.kind = Kind::UInt256;
		out.uint256Value = value;
		return out;
	}

	static MetadataValue FromStruct(const std::vector<std::pair<std::string, MetadataValue>>& value)
	{
		MetadataValue out;
		out.kind = Kind::Struct;
		out.structValue = value;
		return out;
	}

	static MetadataValue FromArray(const std::vector<MetadataValue>& value)
	{
		MetadataValue out;
		out.kind = Kind::Array;
		out.arrayValue = value;
		return out;
	}
};

struct MetadataField
{
	std::string name;
	MetadataValue value;
};

inline bool EqualsIgnoreCase(const std::string& a, const std::string& b)
{
	if (a.size() != b.size())
	{
		return false;
	}
	for (size_t i = 0; i != a.size(); ++i)
	{
		if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
		{
			return false;
		}
	}
	return true;
}

struct MetadataHelper
{
	inline static const std::vector<FieldType> SeriesDefaultMetadataFields = {
		FieldType{ StandardMeta::id.c_str(), VmType::Int256 },
		FieldType{ "mode", VmType::Int8 },
		FieldType{ "rom", VmType::Bytes },
	};

	inline static const std::vector<FieldType> NftDefaultMetadataFields = {
		FieldType{ StandardMeta::id.c_str(), VmType::Int256 },
		FieldType{ "rom", VmType::Bytes },
	};

	inline static const std::vector<FieldType> StandardMetadataFields = {
		FieldType{ "name", VmType::String },
		FieldType{ "description", VmType::String },
		FieldType{ "imageURL", VmType::String },
		FieldType{ "infoURL", VmType::String },
		FieldType{ "royalties", VmType::Int32 },
	};

	static const MetadataField* FindMetadataField(const std::vector<MetadataField>& fields, const std::string& name)
	{
		for (const auto& field : fields)
		{
			if (EqualsIgnoreCase(field.name, name))
			{
				return &field;
			}
		}
		return nullptr;
	}

	static ByteArray GetOptionalBytesField(const std::vector<MetadataField>& fields, const std::string& name)
	{
		const MetadataField* found = FindMetadataField(fields, name);
		if (!found)
		{
			return {};
		}
		return EnsureBytes(name, found->value);
	}

	static void PushMetadataField(
		const VmNamedVariableSchema& fieldSchema,
		std::vector<VmNamedDynamicVariable>& fields,
		const std::vector<MetadataField>& metadataFields,
		Allocator& alloc)
	{
		const MetadataField* found = nullptr;
		for (const auto& field : metadataFields)
		{
			if (field.name == fieldSchema.name.c_str())
			{
				found = &field;
				break;
			}
		}
		if (!found)
		{
			const MetadataField* caseMismatch = nullptr;
			for (const auto& field : metadataFields)
			{
				if (EqualsIgnoreCase(field.name, fieldSchema.name.c_str()))
				{
					caseMismatch = &field;
					break;
				}
			}
			if (caseMismatch)
			{
				PHANTASMA_EXCEPTION_MESSAGE(
					"Metadata field case mismatch",
					"Metadata field '" + std::string(fieldSchema.name.c_str()) +
					"' provided in incorrect case: '" + caseMismatch->name + "'");
			}

			PHANTASMA_EXCEPTION_MESSAGE(
				"Metadata field missing",
				"Metadata field '" + std::string(fieldSchema.name.c_str()) + "' is mandatory");
		}

		const VmDynamicVariable normalized = NormalizeMetadataValue(fieldSchema.schema, fieldSchema.name.c_str(), found->value, alloc);
		fields.push_back(VmNamedDynamicVariable{ fieldSchema.name, normalized });
	}

	static VmDynamicVariable NormalizeMetadataValue(
		const VmVariableSchema& schema,
		const std::string& fieldName,
		const MetadataValue& value,
		Allocator& alloc)
	{
		const uint8_t raw = (uint8_t)schema.type;
		const bool isArray = (raw & (uint8_t)VmType::Array) != 0;
		if (isArray)
		{
			if (value.kind != MetadataValue::Kind::Array)
			{
				PHANTASMA_EXCEPTION_MESSAGE(
					"Metadata field type mismatch",
					"Metadata field '" + fieldName + "' must be provided as an array");
			}
			const VmType baseType = (VmType)(raw & ~(uint8_t)VmType::Array);
			return NormalizeArrayValue(baseType, fieldName, value.arrayValue, schema.structure, alloc);
		}
		return NormalizeScalarValue(schema.type, fieldName, value, schema.structure, alloc);
	}

private:
	static std::string TrimWhitespace(const std::string& text)
	{
		size_t start = 0;
		while (start < text.size() && isspace((unsigned char)text[start]))
		{
			++start;
		}
		size_t end = text.size();
		while (end > start && isspace((unsigned char)text[end - 1]))
		{
			--end;
		}
		return text.substr(start, end - start);
	}

	static ByteArray DecodeHex(const std::string& fieldName, const std::string& hex)
	{
		std::string trimmed = TrimWhitespace(hex);
		if (trimmed.empty())
		{
			PHANTASMA_EXCEPTION_MESSAGE(
				"Metadata bytes invalid",
				"Metadata field '" + fieldName + "' must be a byte array or hex string");
		}
		if (trimmed.rfind("0x", 0) == 0 || trimmed.rfind("0X", 0) == 0)
		{
			trimmed = trimmed.substr(2);
		}
		if (trimmed.empty())
		{
			return {};
		}
		if ((trimmed.size() % 2) != 0)
		{
			PHANTASMA_EXCEPTION_MESSAGE(
				"Metadata bytes invalid",
				"Metadata field '" + fieldName + "' must be a byte array or hex string");
		}

		try
		{
			return Base16::Decode(trimmed.c_str(), (int)trimmed.size());
		}
		catch (...)
		{
			PHANTASMA_EXCEPTION_MESSAGE(
				"Metadata bytes invalid",
				"Metadata field '" + fieldName + "' must be a byte array or hex string");
		}
		return {};
	}

	static ByteArray EnsureBytes(const std::string& fieldName, const MetadataValue& value)
	{
		switch (value.kind)
		{
		case MetadataValue::Kind::Bytes:
			return value.bytesValue;
		case MetadataValue::Kind::String:
			return DecodeHex(fieldName, value.stringValue);
		default:
			break;
		}
		PHANTASMA_EXCEPTION_MESSAGE(
			"Metadata bytes invalid",
			"Metadata field '" + fieldName + "' must be a byte array or hex string");
		return {};
	}

	static ByteArray EnsureFixedBytes(const std::string& fieldName, const MetadataValue& value, size_t expectedLength)
	{
		ByteArray bytes = EnsureBytes(fieldName, value);
		if (bytes.size() != expectedLength)
		{
			PHANTASMA_EXCEPTION_MESSAGE(
				"Metadata bytes invalid",
				"Metadata field '" + fieldName + "' must be exactly " + std::to_string(expectedLength) + " bytes");
		}
		return bytes;
	}

	static Bytes16 EnsureBytes16(const std::string& fieldName, const MetadataValue& value)
	{
		if (value.kind == MetadataValue::Kind::Bytes16)
		{
			return value.bytes16Value;
		}
		return Bytes16(EnsureFixedBytes(fieldName, value, Bytes16::length));
	}

	static Bytes32 EnsureBytes32(const std::string& fieldName, const MetadataValue& value)
	{
		if (value.kind == MetadataValue::Kind::Bytes32)
		{
			return value.bytes32Value;
		}
		return Bytes32(EnsureFixedBytes(fieldName, value, Bytes32::length));
	}

	static Bytes64 EnsureBytes64(const std::string& fieldName, const MetadataValue& value)
	{
		if (value.kind == MetadataValue::Kind::Bytes64)
		{
			return value.bytes64Value;
		}
		return Bytes64(EnsureFixedBytes(fieldName, value, Bytes64::length));
	}

	static std::string EnsureNonEmptyString(const std::string& fieldName, const MetadataValue& value)
	{
		if (value.kind != MetadataValue::Kind::String)
		{
			PHANTASMA_EXCEPTION_MESSAGE(
				"Metadata string invalid",
				"Metadata field '" + fieldName + "' must be a string");
		}
		std::string trimmed = TrimWhitespace(value.stringValue);
		if (trimmed.empty())
		{
			PHANTASMA_EXCEPTION_MESSAGE(
				"Metadata string invalid",
				"Metadata field '" + fieldName + "' is mandatory");
		}
		return trimmed;
	}

	static uint64_t EnsureIntegerInRange(
		const std::string& fieldName,
		const MetadataValue& value,
		int64_t min,
		int64_t max,
		uint64_t unsignedMax,
		const char* label)
	{
		if (value.kind == MetadataValue::Kind::Int64)
		{
			const int64_t v = value.int64Value;
			if (v < min || v > max)
			{
				PHANTASMA_EXCEPTION_MESSAGE(
					"Metadata integer invalid",
					"Metadata field '" + fieldName + "' must be between " + std::to_string(min) +
					" and " + std::to_string(max) + " or between 0 and " + std::to_string(unsignedMax) +
					" (" + label + ")");
			}
			return (uint64_t)v;
		}
		if (value.kind == MetadataValue::Kind::UInt64)
		{
			const uint64_t v = value.uint64Value;
			if (v > unsignedMax)
			{
				PHANTASMA_EXCEPTION_MESSAGE(
					"Metadata integer invalid",
					"Metadata field '" + fieldName + "' must be between " + std::to_string(min) +
					" and " + std::to_string(max) + " or between 0 and " + std::to_string(unsignedMax) +
					" (" + label + ")");
			}
			return v;
		}

		PHANTASMA_EXCEPTION_MESSAGE(
			"Metadata integer invalid",
			"Metadata field '" + fieldName + "' must be a number");
		return 0;
	}

	static VmDynamicVariable NormalizeScalarValue(
		VmType type,
		const std::string& fieldName,
		const MetadataValue& value,
		const VmStructSchema& structSchema,
		Allocator& alloc)
	{
		switch (type)
		{
		case VmType::String:
		{
			const std::string text = EnsureNonEmptyString(fieldName, value);
			return VmDynamicVariable(alloc.Clone(text.c_str()));
		}
		case VmType::Int8:
		{
			const uint64_t raw = EnsureIntegerInRange(fieldName, value, -0x80, 0x7f, 0xff, "Int8");
			return VmDynamicVariable((uint8_t)raw);
		}
		case VmType::Int16:
		{
			const uint64_t raw = EnsureIntegerInRange(fieldName, value, -0x8000, 0x7fff, 0xffff, "Int16");
			return VmDynamicVariable((uint16_t)raw);
		}
		case VmType::Int32:
		{
			const uint64_t raw = EnsureIntegerInRange(fieldName, value, -0x80000000LL, 0x7fffffffLL, 0xffffffffULL, "Int32");
			return VmDynamicVariable((uint32_t)raw);
		}
		case VmType::Int64:
		{
			const uint64_t raw = EnsureIntegerInRange(
				fieldName,
				value,
				std::numeric_limits<int64_t>::min(),
				std::numeric_limits<int64_t>::max(),
				std::numeric_limits<uint64_t>::max(),
				"Int64");
			return VmDynamicVariable((uint64_t)raw);
		}
		case VmType::Int256:
		{
			if (value.kind == MetadataValue::Kind::Int256)
			{
				return VmDynamicVariable(value.int256Value);
			}
			if (value.kind == MetadataValue::Kind::UInt256)
			{
				return VmDynamicVariable(value.uint256Value);
			}
			if (value.kind == MetadataValue::Kind::Int64)
			{
				return VmDynamicVariable(int256(value.int64Value));
			}
			if (value.kind == MetadataValue::Kind::UInt64)
			{
				return VmDynamicVariable(uint256(value.uint64Value));
			}
			PHANTASMA_EXCEPTION_MESSAGE(
				"Metadata integer invalid",
				"Metadata field '" + fieldName + "' must be a number (Int256)");
			break;
		}
		case VmType::Bytes:
		{
			const ByteArray bytes = EnsureBytes(fieldName, value);
			const ByteView view = alloc.Clone(ByteView{ bytes.data(), bytes.size() });
			return VmDynamicVariable(view);
		}
		case VmType::Bytes16:
			return VmDynamicVariable(EnsureBytes16(fieldName, value));
		case VmType::Bytes32:
			return VmDynamicVariable(EnsureBytes32(fieldName, value));
		case VmType::Bytes64:
			return VmDynamicVariable(EnsureBytes64(fieldName, value));
		case VmType::Struct:
		{
			const VmDynamicStruct str = NormalizeStructValue(fieldName, structSchema, value, alloc);
			return VmDynamicVariable(str);
		}
		default:
			break;
		}

		PHANTASMA_EXCEPTION_MESSAGE(
			"Metadata field unsupported",
			"Metadata field '" + fieldName + "' has unsupported type");
		return VmDynamicVariable();
	}

	static VmDynamicVariable NormalizeArrayValue(
		VmType type,
		const std::string& fieldName,
		const std::vector<MetadataValue>& values,
		const VmStructSchema& structSchema,
		Allocator& alloc)
	{
		const uint32_t count = (uint32_t)values.size();
		VmDynamicVariable out;
		out.type = (VmType)((uint8_t)VmType::Array | (uint8_t)type);
		out.arrayLength = count;

		switch (type)
		{
		case VmType::String:
		{
			const char** arr = alloc.Alloc<const char*>(count);
			for (uint32_t i = 0; i != count; ++i)
			{
				const std::string text = EnsureNonEmptyString(fieldName + "[" + std::to_string(i) + "]", values[i]);
				arr[i] = alloc.Clone(text.c_str());
			}
			out.data.stringArray = arr;
			return out;
		}
		case VmType::Int8:
		{
			uint8_t* arr = alloc.Alloc<uint8_t>(count);
			for (uint32_t i = 0; i != count; ++i)
			{
				const uint64_t raw = EnsureIntegerInRange(fieldName + "[" + std::to_string(i) + "]", values[i], -0x80, 0x7f, 0xff, "Int8");
				arr[i] = (uint8_t)raw;
			}
			out.data.int8Array = arr;
			return out;
		}
		case VmType::Int16:
		{
			uint16_t* arr = alloc.Alloc<uint16_t>(count);
			for (uint32_t i = 0; i != count; ++i)
			{
				const uint64_t raw = EnsureIntegerInRange(fieldName + "[" + std::to_string(i) + "]", values[i], -0x8000, 0x7fff, 0xffff, "Int16");
				arr[i] = (uint16_t)raw;
			}
			out.data.int16Array = arr;
			return out;
		}
		case VmType::Int32:
		{
			uint32_t* arr = alloc.Alloc<uint32_t>(count);
			for (uint32_t i = 0; i != count; ++i)
			{
				const uint64_t raw = EnsureIntegerInRange(fieldName + "[" + std::to_string(i) + "]", values[i], -0x80000000LL, 0x7fffffffLL, 0xffffffffULL, "Int32");
				arr[i] = (uint32_t)raw;
			}
			out.data.int32Array = arr;
			return out;
		}
		case VmType::Int64:
		{
			uint64_t* arr = alloc.Alloc<uint64_t>(count);
			for (uint32_t i = 0; i != count; ++i)
			{
				const uint64_t raw = EnsureIntegerInRange(
					fieldName + "[" + std::to_string(i) + "]",
					values[i],
					std::numeric_limits<int64_t>::min(),
					std::numeric_limits<int64_t>::max(),
					std::numeric_limits<uint64_t>::max(),
					"Int64");
				arr[i] = raw;
			}
			out.data.int64Array = arr;
			return out;
		}
		case VmType::Int256:
		{
			uint256* arr = alloc.Alloc<uint256>(count);
			for (uint32_t i = 0; i != count; ++i)
			{
				const MetadataValue& v = values[i];
				if (v.kind == MetadataValue::Kind::UInt256)
				{
					arr[i] = v.uint256Value;
				}
				else if (v.kind == MetadataValue::Kind::Int256)
				{
					arr[i] = v.int256Value.Unsigned();
				}
				else if (v.kind == MetadataValue::Kind::Int64)
				{
					arr[i] = int256(v.int64Value).Unsigned();
				}
				else if (v.kind == MetadataValue::Kind::UInt64)
				{
					arr[i] = uint256(v.uint64Value);
				}
				else
				{
					PHANTASMA_EXCEPTION_MESSAGE(
						"Metadata integer invalid",
						"Metadata field '" + fieldName + "[" + std::to_string(i) + "]' must be a number (Int256)");
				}
			}
			out.data.int256Array = arr;
			return out;
		}
		case VmType::Bytes:
		{
			ByteView* arr = alloc.Alloc<ByteView>(count);
			for (uint32_t i = 0; i != count; ++i)
			{
				const ByteArray bytes = EnsureBytes(fieldName + "[" + std::to_string(i) + "]", values[i]);
				arr[i] = alloc.Clone(ByteView{ bytes.data(), bytes.size() });
			}
			out.data.bytesArray = arr;
			return out;
		}
		case VmType::Bytes16:
		{
			Bytes16* arr = alloc.Alloc<Bytes16>(count);
			for (uint32_t i = 0; i != count; ++i)
			{
				arr[i] = EnsureBytes16(fieldName + "[" + std::to_string(i) + "]", values[i]);
			}
			out.data.bytes16Array = arr;
			return out;
		}
		case VmType::Bytes32:
		{
			Bytes32* arr = alloc.Alloc<Bytes32>(count);
			for (uint32_t i = 0; i != count; ++i)
			{
				arr[i] = EnsureBytes32(fieldName + "[" + std::to_string(i) + "]", values[i]);
			}
			out.data.bytes32Array = arr;
			return out;
		}
		case VmType::Bytes64:
		{
			Bytes64* arr = alloc.Alloc<Bytes64>(count);
			for (uint32_t i = 0; i != count; ++i)
			{
				arr[i] = EnsureBytes64(fieldName + "[" + std::to_string(i) + "]", values[i]);
			}
			out.data.bytes64Array = arr;
			return out;
		}
		case VmType::Struct:
		{
			VmDynamicStruct* arr = alloc.Alloc<VmDynamicStruct>(count);
			for (uint32_t i = 0; i != count; ++i)
			{
				arr[i] = NormalizeStructValue(fieldName + "[" + std::to_string(i) + "]", structSchema, values[i], alloc);
			}
			out.data.structureArray = { structSchema, arr };
			return out;
		}
		default:
			break;
		}

		PHANTASMA_EXCEPTION_MESSAGE(
			"Metadata field unsupported",
			"Metadata field '" + fieldName + "' has unsupported array type");
		return VmDynamicVariable();
	}

	static VmDynamicStruct NormalizeStructValue(
		const std::string& fieldName,
		const VmStructSchema& structSchema,
		const MetadataValue& value,
		Allocator& alloc)
	{
		if (structSchema.numFields == 0)
		{
			PHANTASMA_EXCEPTION_MESSAGE(
				"Metadata struct invalid",
				"Metadata field '" + fieldName + "' is missing struct schema");
		}

		std::vector<std::pair<std::string, MetadataValue>> provided;
		if (value.kind == MetadataValue::Kind::Struct)
		{
			provided = value.structValue;
		}
		else if (value.kind == MetadataValue::Kind::Array)
		{
			for (const auto& item : value.arrayValue)
			{
				if (item.kind != MetadataValue::Kind::Struct)
				{
					PHANTASMA_EXCEPTION_MESSAGE(
						"Metadata struct invalid",
						"Metadata field '" + fieldName + "' must be provided as an object or array of fields");
				}
				const auto nameIt = std::find_if(
					item.structValue.begin(),
					item.structValue.end(),
					[](const std::pair<std::string, MetadataValue>& f) { return f.first == "name"; });
				const auto valueIt = std::find_if(
					item.structValue.begin(),
					item.structValue.end(),
					[](const std::pair<std::string, MetadataValue>& f) { return f.first == "value"; });
				if (nameIt == item.structValue.end() || valueIt == item.structValue.end())
				{
					PHANTASMA_EXCEPTION_MESSAGE(
						"Metadata struct invalid",
						"Metadata field '" + fieldName + "' must be provided as an object or array of fields");
				}
				if (nameIt->second.kind != MetadataValue::Kind::String)
				{
					PHANTASMA_EXCEPTION_MESSAGE(
						"Metadata struct invalid",
						"Metadata field '" + fieldName + "' must be provided as an object or array of fields");
				}
				provided.push_back({ nameIt->second.stringValue, valueIt->second });
			}
		}
		else
		{
			PHANTASMA_EXCEPTION_MESSAGE(
				"Metadata struct invalid",
				"Metadata field '" + fieldName + "' must be provided as an object or array of fields");
		}

		std::vector<VmNamedDynamicVariable> fields;
		fields.reserve(structSchema.numFields);

		for (uint32_t i = 0; i != structSchema.numFields; ++i)
		{
			const VmNamedVariableSchema& childSchema = structSchema.fields[i];
			const std::string childName = childSchema.name.c_str();
			const auto exact = std::find_if(
				provided.begin(),
				provided.end(),
				[&](const std::pair<std::string, MetadataValue>& f) { return f.first == childName; });
			if (exact == provided.end())
			{
				const auto caseMismatch = std::find_if(
					provided.begin(),
					provided.end(),
					[&](const std::pair<std::string, MetadataValue>& f) { return EqualsIgnoreCase(f.first, childName); });
				if (caseMismatch != provided.end())
				{
					PHANTASMA_EXCEPTION_MESSAGE(
						"Metadata struct invalid",
						"Metadata field '" + childName + "' provided in incorrect case inside '" + fieldName +
						"': '" + caseMismatch->first + "'");
				}
				PHANTASMA_EXCEPTION_MESSAGE(
					"Metadata struct invalid",
					"Metadata field '" + fieldName + "." + childName + "' is mandatory");
			}

			const VmDynamicVariable normalized = NormalizeMetadataValue(
				childSchema.schema,
				fieldName + "." + childName,
				exact->second,
				alloc);
			fields.push_back(VmNamedDynamicVariable{ childSchema.name, normalized });
		}

		for (const auto& providedField : provided)
		{
			const bool known = std::any_of(
				structSchema.fields,
				structSchema.fields + structSchema.numFields,
				[&](const VmNamedVariableSchema& s) { return EqualsIgnoreCase(s.name.c_str(), providedField.first); });
			if (!known)
			{
				PHANTASMA_EXCEPTION_MESSAGE(
					"Metadata struct invalid",
					"Metadata field '" + fieldName + "' received unknown property '" + providedField.first + "'");
			}
		}

		VmNamedDynamicVariable* storage = alloc.Alloc<VmNamedDynamicVariable>(fields.size());
		for (size_t i = 0; i != fields.size(); ++i)
		{
			storage[i] = fields[i];
		}
		return VmDynamicStruct::Sort((uint32_t)fields.size(), storage);
	}
};

struct IdHelper
{
	static uint256 GetRandomPhantasmaId()
	{
		uint8_t bytes[32];
		CryptoRandomBuffer(bytes, sizeof(bytes));
		return uint256::FromBytes(ByteView{ bytes, sizeof(bytes) });
	}
};

struct TokenSchemasBuilder
{
private:
	static bool ContainsField(const VmStructSchema& schema, const std::string& name, VmType type, std::string& outError)
	{
		for (uint32_t i = 0; i != schema.numFields; ++i)
		{
			const std::string candidate = schema.fields[i].name.c_str();
			if (candidate == name)
			{
				if (schema.fields[i].schema.type != type)
				{
					outError = "Type mismatch for field " + name;
					return false;
				}
				return true;
			}
			if (EqualsIgnoreCase(candidate, name))
			{
				outError = "Case mismatch for field " + name + ", expected " + candidate;
				return false;
			}
		}
		return false;
	}

	static bool VerifyMandatory(const VmStructSchema& schema, const std::vector<FieldType>& mandatory, std::string& outError)
	{
		for (const auto& f : mandatory)
		{
			if (!ContainsField(schema, f.name, f.type, outError))
			{
				if (outError.empty())
				{
					outError = "Mandatory metadata field not found: " + f.name;
				}
				return false;
			}
		}
		return true;
	}

	static bool VerifyStandardMetadata(const VmStructSchema* first, const VmStructSchema* second, std::string& outError)
	{
		for (const auto& f : MetadataHelper::StandardMetadataFields)
		{
			bool found = false;
			std::string tempError;
			if (first && ContainsField(*first, f.name, f.type, tempError))
			{
				found = true;
			}
			else if (!tempError.empty())
			{
				outError = tempError;
				return false;
			}
			if (!found && second && ContainsField(*second, f.name, f.type, tempError))
			{
				found = true;
			}
			else if (!found && !tempError.empty())
			{
				outError = tempError;
				return false;
			}
			if (!found)
			{
				outError = "Mandatory metadata field not found: " + f.name;
				return false;
			}
		}
		return true;
	}

	static bool AddField(std::vector<VmNamedVariableSchema>& dest, const FieldType& f, std::string& outError)
	{
		if (f.name.empty())
		{
			outError = "Field name cannot be empty";
			return false;
		}
		for (const auto& existing : dest)
		{
			const std::string candidate = existing.name.c_str();
			if (candidate == f.name)
			{
				outError = "Duplicate field name: " + f.name;
				return false;
			}
			if (EqualsIgnoreCase(candidate, f.name))
			{
				outError = "Case mismatch for field " + f.name + ", expected " + candidate;
				return false;
			}
		}
		dest.push_back(VmNamedVariableSchema{ SmallString(f.name.c_str(), f.name.size()), VmVariableSchema{ f.type } });
		return true;
	}

	static bool Verify(const TokenSchemasOwned& owned, std::string& outError)
	{
		if (!VerifyMandatory(owned.view.seriesMetadata, MetadataHelper::SeriesDefaultMetadataFields, outError))
		{
			return false;
		}
		if (!VerifyMandatory(owned.view.rom, MetadataHelper::NftDefaultMetadataFields, outError))
		{
			return false;
		}
		if (!VerifyStandardMetadata(&owned.view.seriesMetadata, &owned.view.rom, outError))
		{
			return false;
		}
		return true;
	}

public:
	static TokenSchemasOwned PrepareStandardTokenSchemas(bool sharedMetadata = false)
	{
		TokenSchemasOwned owned;
		owned.seriesFields = {
			VmNamedVariableSchema{ StandardMeta::id, VmVariableSchema{ VmType::Int256 } },
			VmNamedVariableSchema{ SmallString("mode"), VmVariableSchema{ VmType::Int8 } },
			VmNamedVariableSchema{ SmallString("rom"), VmVariableSchema{ VmType::Bytes } },
		};
		if (sharedMetadata)
		{
			for (const auto& f : MetadataHelper::StandardMetadataFields)
			{
				owned.seriesFields.push_back(VmNamedVariableSchema{ SmallString(f.name.c_str(), f.name.size()), VmVariableSchema{ f.type } });
			}
		}
		owned.romFields = {
			VmNamedVariableSchema{ StandardMeta::id, VmVariableSchema{ VmType::Int256 } },
			VmNamedVariableSchema{ SmallString("rom"), VmVariableSchema{ VmType::Bytes } },
		};
		if (!sharedMetadata)
		{
			for (const auto& f : MetadataHelper::StandardMetadataFields)
			{
				owned.romFields.push_back(VmNamedVariableSchema{ SmallString(f.name.c_str(), f.name.size()), VmVariableSchema{ f.type } });
			}
		}
		owned.ramFields.clear();

		owned.view.seriesMetadata = VmStructSchema{ (uint32_t)owned.seriesFields.size(), owned.seriesFields.data(), VmStructSchema::Flag_None };
		owned.view.rom = VmStructSchema{ (uint32_t)owned.romFields.size(), owned.romFields.data(), VmStructSchema::Flag_None };
		owned.view.ram = VmStructSchema{ (uint32_t)owned.ramFields.size(), owned.ramFields.data(), VmStructSchema::Flag_DynamicExtras };
		return owned;
	}

	static TokenSchemasOwned BuildFromFields(const std::vector<FieldType>& seriesFields, const std::vector<FieldType>& romFields, const std::vector<FieldType>& ramFields)
	{
		TokenSchemasOwned owned;
		std::string error;

		{
			const std::vector<FieldType> defaults = {
				FieldType{ StandardMeta::id.c_str(), VmType::Int256 },
				FieldType{ "mode", VmType::Int8 },
				FieldType{ "rom", VmType::Bytes },
			};
			for (const auto& f : defaults)
			{
				if (!AddField(owned.seriesFields, f, error))
				{
					PHANTASMA_EXCEPTION_MESSAGE("Invalid token schema", error);
				}
			}
			for (const auto& f : seriesFields)
			{
				if (!AddField(owned.seriesFields, f, error))
				{
					PHANTASMA_EXCEPTION_MESSAGE("Invalid token schema", error);
				}
			}
			owned.view.seriesMetadata = VmStructSchema::Sort((uint32_t)owned.seriesFields.size(), owned.seriesFields.data(), false);
		}

		{
			const std::vector<FieldType> defaults = {
				FieldType{ StandardMeta::id.c_str(), VmType::Int256 },
				FieldType{ "rom", VmType::Bytes },
			};
			for (const auto& f : defaults)
			{
				if (!AddField(owned.romFields, f, error))
				{
					PHANTASMA_EXCEPTION_MESSAGE("Invalid token schema", error);
				}
			}
			for (const auto& f : romFields)
			{
				if (!AddField(owned.romFields, f, error))
				{
					PHANTASMA_EXCEPTION_MESSAGE("Invalid token schema", error);
				}
			}
			owned.view.rom = VmStructSchema::Sort((uint32_t)owned.romFields.size(), owned.romFields.data(), false);
		}

		{
			for (const auto& f : ramFields)
			{
				if (!AddField(owned.ramFields, f, error))
				{
					PHANTASMA_EXCEPTION_MESSAGE("Invalid token schema", error);
				}
			}
			const bool allowExtras = owned.ramFields.empty();
			owned.view.ram = VmStructSchema::Sort((uint32_t)owned.ramFields.size(), owned.ramFields.data(), allowExtras);
		}

		if (!Verify(owned, error))
		{
			PHANTASMA_EXCEPTION_MESSAGE("Invalid token schema", error);
		}
		return owned;
	}

	static ByteArray BuildAndSerialize(const TokenSchemas* tokenSchemas)
	{
		TokenSchemasOwned owned = tokenSchemas ? TokenSchemasOwned() : PrepareStandardTokenSchemas();
		ByteArray buffer;
		WriteView w(buffer);
		if (tokenSchemas)
		{
			Write(*tokenSchemas, w);
		}
		else
		{
			Write(owned.View(), w);
		}
		return buffer;
	}

	static ByteArray Serialize(const TokenSchemas& tokenSchemas)
	{
		ByteArray buffer;
		WriteView w(buffer);
		Write(tokenSchemas, w);
		return buffer;
	}

	static std::string SerializeHex(const TokenSchemas& tokenSchemas)
	{
		const ByteArray bytes = Serialize(tokenSchemas);
		if (bytes.empty())
		{
			return {};
		}
		const String encoded = Base16::Encode(&bytes.front(), (int)bytes.size(), false);
		return std::string(encoded.begin(), encoded.end());
	}

#ifdef PHANTASMA_RAPIDJSON
	static TokenSchemasOwned FromJson(const std::string& json)
	{
		rapidjson::Document doc;
		doc.Parse<rapidjson::kParseDefaultFlags>(json.c_str());
		if (doc.HasParseError() || !doc.IsObject())
		{
			PHANTASMA_EXCEPTION_MESSAGE("TokenSchemas json invalid", "token_schemas must be a JSON object");
		}

		auto parseArray = [&](const char* key) -> std::vector<FieldType>
		{
			if (!doc.HasMember(key) || !doc[key].IsArray())
			{
				PHANTASMA_EXCEPTION_MESSAGE("TokenSchemas json invalid", std::string(key) + " must be an array");
			}
			std::vector<FieldType> fields;
			for (auto it = doc[key].Begin(); it != doc[key].End(); ++it)
			{
				const rapidjson::Value& v = *it;
				if (!v.IsObject() || !v.HasMember("name") || !v.HasMember("type") || !v["name"].IsString() || !v["type"].IsString())
				{
					PHANTASMA_EXCEPTION_MESSAGE("TokenSchemas json invalid", std::string(key) + " entries must contain name and type");
				}
				bool error = false;
				const VmType vmType = VmTypeFromString(v["type"].GetString(), &error);
				if (error)
				{
					PHANTASMA_EXCEPTION_MESSAGE("TokenSchemas json invalid", "Unknown VmType: " + std::string(v["type"].GetString()));
				}
				fields.push_back(FieldType{ v["name"].GetString(), vmType });
			}
			return fields;
		};

		const std::vector<FieldType> seriesFields = parseArray("seriesMetadata");
		const std::vector<FieldType> romFields = parseArray("rom");
		const std::vector<FieldType> ramFields = parseArray("ram");

		return BuildFromFields(seriesFields, romFields, ramFields);
	}
#endif
};

} // namespace phantasma::carbon
