#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif

#include <cctype>
#include <string>
#include <vector>

#include "../DataBlockchain.h"

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

struct TokenSchemasBuilder
{
private:
	static bool EqualsIgnoreCase(const std::string& a, const std::string& b)
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
		const std::vector<FieldType> standard = {
			FieldType{ "name", VmType::String },
			FieldType{ "description", VmType::String },
			FieldType{ "imageURL", VmType::String },
			FieldType{ "infoURL", VmType::String },
			FieldType{ "royalties", VmType::Int32 },
		};

		for (const auto& f : standard)
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
		const std::vector<FieldType> seriesDefaults = {
			FieldType{ StandardMeta::id.c_str(), VmType::Int256 },
			FieldType{ "mode", VmType::Int8 },
			FieldType{ "rom", VmType::Bytes },
		};
		const std::vector<FieldType> nftDefaults = {
			FieldType{ StandardMeta::id.c_str(), VmType::Int256 },
			FieldType{ "rom", VmType::Bytes },
		};

		if (!VerifyMandatory(owned.view.seriesMetadata, seriesDefaults, outError))
		{
			return false;
		}
		if (!VerifyMandatory(owned.view.rom, nftDefaults, outError))
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
	static TokenSchemasOwned PrepareStandardTokenSchemas()
	{
		TokenSchemasOwned owned;
		owned.seriesFields = {
			VmNamedVariableSchema{ StandardMeta::id, VmVariableSchema{ VmType::Int256 } },
			VmNamedVariableSchema{ SmallString("mode"), VmVariableSchema{ VmType::Int8 } },
			VmNamedVariableSchema{ SmallString("rom"), VmVariableSchema{ VmType::Bytes } },
		};
		owned.romFields = {
			VmNamedVariableSchema{ StandardMeta::id, VmVariableSchema{ VmType::Int256 } },
			VmNamedVariableSchema{ SmallString("rom"), VmVariableSchema{ VmType::Bytes } },
			VmNamedVariableSchema{ StandardMeta::Token::Nft::name, VmVariableSchema{ VmType::String } },
			VmNamedVariableSchema{ StandardMeta::Token::Nft::description, VmVariableSchema{ VmType::String } },
			VmNamedVariableSchema{ StandardMeta::Token::Nft::imageURL, VmVariableSchema{ VmType::String } },
			VmNamedVariableSchema{ StandardMeta::Token::Nft::infoURL, VmVariableSchema{ VmType::String } },
			VmNamedVariableSchema{ StandardMeta::Token::Nft::royalties, VmVariableSchema{ VmType::Int32 } },
		};
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
};

} // namespace phantasma::carbon
