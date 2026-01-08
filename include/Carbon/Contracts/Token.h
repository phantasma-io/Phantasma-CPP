#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif

#include <cctype>
#include <map>
#include <string>
#include <vector>

#include "../../Numerics/Base64.h"
#include "../DataBlockchain.h"
#include "TokenSchemas.h"

namespace phantasma::carbon {

enum TokensConfigFlags
{
	TokensConfigFlags_None = 0,
	TokensConfigFlags_RequireMetadata = 1 << 0,
	TokensConfigFlags_RequireSymbol = 1 << 1,
	TokensConfigFlags_RequireNftMetaId = 1 << 2,
	TokensConfigFlags_RequireNftStandard = 1 << 3,
};
struct TokensConfig
{
	uint8_t flags = TokensConfigFlags_None;
};

enum TokenFlags
{
	TokenFlags_None = 0,
	TokenFlags_BigFungible = 1 << 0,
	TokenFlags_NonFungible = 1 << 1,
};

struct TokenInfo
{
	intx_pod maxSupply{};
	TokenFlags flags = TokenFlags_None;
	uint8_t decimals = 0;
	Bytes32 owner{};
	SmallString symbol{};
	ByteView metadata{};
	ByteView tokenSchemas{};
};
struct TokenInfo_FlagsOnly
{
	TokenFlags flags = TokenFlags_None;
};

struct GasConfigWithTokens
{
	Blockchain::GasConfig config{};
	TokenInfo gasToken{};
	TokenInfo dataToken{};
};

struct SeriesInfo
{
	uint32_t maxMint = 0;
	uint32_t maxSupply = 0;
	Bytes32 owner{};
	ByteView metadata{};
	VmStructSchema rom{};
	VmStructSchema ram{};
};

struct SeriesSupply
{
	uint32_t mintCount = 0;
	uint32_t currentSupply = 0;
};

enum NftInstanceFlags : uint8_t
{
	NftInstance_None = 0,
	NftInstance_HasMetaId = 1 << 0,
	NftInstance_HasRam = 1 << 1,
	NftInstance_Staked = 1 << 2,
};

struct NftState
{
	int64_t lastTransfer = 0;
	NftInstanceFlags flags = NftInstance_None;
	VmDynamicVariable metaId{};
};

struct NftInstance
{
	Bytes32 originator{};
	int64_t created = 0;
	NftInstanceFlags flags = NftInstance_None;
	ByteView rom{};
};

struct NftMintInfo
{
	uint32_t seriesId = 0;
	ByteView rom{};
	ByteView ram{};
};

struct NftInfo
{
	uint32_t seriesId = 0;
	uint32_t mintNumber = 0;
	Bytes32 originator{};
	int64_t created = 0;
	NftInstanceFlags flags = NftInstance_None;
	ByteView rom{};
	ByteView ram{};
	Bytes32 owner{};
};

struct NftSchema
{
	VmStructSchema tokenRom{};
	VmStructSchema seriesRom{};
	VmStructSchema tokenRam{};
	VmStructSchema seriesRam{};
	VmStructSchema seriesMetadataSchema{};
	ByteView seriesMetadataValue{};
	ByteView tokenMetadata{};
	SmallString tokenSymbol{};
};

struct NftImport
{
	uint32_t mintNumber = 0;
	Bytes32 originator{};
	int64_t created = 0;
	ByteView rom{};
	ByteView ram{};
	Bytes32 owner{};
};

struct SeriesImport
{
	uint64_t tokenId = 0;
	SeriesInfo info{};
	uint32_t numImports = 0;
	const NftImport* imports = nullptr;
};

// Serialization --------------------------------------------------------------
inline void Write(const TokensConfig& in, WriteView& w) { Write1(in.flags, w); }
inline void Write(const TokenInfo_FlagsOnly& in, WriteView& w) { Write1((uint8_t)in.flags, w); }
inline void Write(const TokenInfo& in, WriteView& w)
{
	Write(in.maxSupply, w);
	Write1((uint8_t)in.flags, w);
	Write1(in.decimals, w);
	Write(in.owner, w);
	Write(in.symbol, w);
	WriteArray(ByteArray(in.metadata.bytes, in.metadata.bytes + in.metadata.length), w);
	if ((in.flags & TokenFlags_NonFungible) != 0)
	{
		WriteArray(ByteArray(in.tokenSchemas.bytes, in.tokenSchemas.bytes + in.tokenSchemas.length), w);
	}
}
inline void Write(const TokenSchemas& in, WriteView& w)
{
	Write(in.seriesMetadata, w);
	Write(in.rom, w);
	Write(in.ram, w);
}
inline void Write(const SeriesInfo& in, WriteView& w)
{
	Write4((int32_t)in.maxMint, w);
	Write4((int32_t)in.maxSupply, w);
	Write(in.owner, w);
	WriteArray(ByteArray(in.metadata.bytes, in.metadata.bytes + in.metadata.length), w);
	Write(in.rom, w);
	Write(in.ram, w);
}

inline void Write(const SeriesImport& in, WriteView& w)
{
	Write8u(in.tokenId, w);
	Write(in.info, w);
	Write4((int32_t)in.numImports, w);
	for (uint32_t i = 0; i != in.numImports; ++i)
	{
		const NftImport& imp = in.imports[i];
		Write(imp.originator, w);
		Write4((int32_t)imp.mintNumber, w);
		Write8(imp.created, w);
		WriteArray(ByteArray(imp.rom.bytes, imp.rom.bytes + imp.rom.length), w);
		WriteArray(ByteArray(imp.ram.bytes, imp.ram.bytes + imp.ram.length), w);
		Write(imp.owner, w);
	}
}

inline void Write(const SeriesSupply& in, WriteView& w)
{
	Write4((int32_t)in.mintCount, w);
	Write4((int32_t)in.currentSupply, w);
}

inline void Write(const NftMintInfo& in, WriteView& w)
{
	Write4((int32_t)in.seriesId, w);
	WriteArray(ByteArray(in.rom.bytes, in.rom.bytes + in.rom.length), w);
	WriteArray(ByteArray(in.ram.bytes, in.ram.bytes + in.ram.length), w);
}

inline void Write(const NftInstance& in, WriteView& w)
{
	Write(in.originator, w);
	Write8(in.created, w);
	Write1((uint8_t)in.flags, w);
	WriteArray(ByteArray(in.rom.bytes, in.rom.bytes + in.rom.length), w);
}

inline void Write(const NftState& in, WriteView& w)
{
	Write8(in.lastTransfer, w);
	Write1((uint8_t)in.flags, w);
	Write(in.metaId, w);
}

inline void Write(const NftInfo& in, WriteView& w)
{
	Write4((int32_t)in.seriesId, w);
	Write4((int32_t)in.mintNumber, w);
	Write(in.originator, w);
	Write8(in.created, w);
	Write1((uint8_t)in.flags, w);
	WriteArray(ByteArray(in.rom.bytes, in.rom.bytes + in.rom.length), w);
	WriteArray(ByteArray(in.ram.bytes, in.ram.bytes + in.ram.length), w);
	Write(in.owner, w);
}

inline void Write(const NftSchema& in, WriteView& w)
{
	Write(in.tokenRom, w);
	Write(in.seriesRom, w);
	Write(in.tokenRam, w);
	Write(in.seriesRam, w);
	Write(in.seriesMetadataSchema, w);
	WriteArray(ByteArray(in.seriesMetadataValue.bytes, in.seriesMetadataValue.bytes + in.seriesMetadataValue.length), w);
	WriteArray(ByteArray(in.tokenMetadata.bytes, in.tokenMetadata.bytes + in.tokenMetadata.length), w);
	Write(in.tokenSymbol, w);
}

inline void Write(const GasConfigWithTokens& in, WriteView& w)
{
	Write(in.config, w);
	Write(in.gasToken, w);
	Write(in.dataToken, w);
}

struct TokenInfoOwned
{
	TokenInfo view{};
	ByteArray metadataStorage;
	ByteArray schemasStorage;

	TokenInfo View() const
	{
		TokenInfo copy = view;
		copy.metadata = ByteView{ metadataStorage.data(), metadataStorage.size() };
		copy.tokenSchemas = ByteView{ schemasStorage.data(), schemasStorage.size() };
		return copy;
	}
};

struct SeriesInfoOwned
{
	SeriesInfo view{};
	ByteArray metadataStorage;

	SeriesInfo View() const
	{
		SeriesInfo copy = view;
		copy.metadata = ByteView{ metadataStorage.data(), metadataStorage.size() };
		return copy;
	}
};

struct TokenMetadataBuilder
{
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

	static std::string TrimEndEquals(const std::string& text)
	{
		size_t end = text.size();
		while (end > 0 && text[end - 1] == '=')
		{
			--end;
		}
		return text.substr(0, end);
	}

	static bool IsBase64Char(char c)
	{
		return (c >= 'A' && c <= 'Z') ||
			(c >= 'a' && c <= 'z') ||
			(c >= '0' && c <= '9') ||
			c == '+' || c == '/' || c == '=';
	}

	static bool StartsWithCaseInsensitive(const std::string& text, const std::string& prefix)
	{
		if (text.size() < prefix.size())
		{
			return false;
		}
		for (size_t i = 0; i < prefix.size(); ++i)
		{
			char a = (char)tolower((unsigned char)text[i]);
			char b = (char)tolower((unsigned char)prefix[i]);
			if (a != b)
			{
				return false;
			}
		}
		return true;
	}

	static void ValidateIcon(const std::string& icon)
	{
		const std::string trimmed = TrimWhitespace(icon);
		if (trimmed.empty())
		{
			PHANTASMA_EXCEPTION("Token metadata icon must be a base64-encoded data URI (PNG, JPEG, or WebP)");
		}

		if (!StartsWithCaseInsensitive(trimmed, "data:image/"))
		{
			PHANTASMA_EXCEPTION("Token metadata icon must be a base64-encoded data URI (PNG, JPEG, or WebP)");
		}
		const auto comma = trimmed.find(',');
		if (comma == std::string::npos)
		{
			PHANTASMA_EXCEPTION("Token metadata icon must be a base64-encoded data URI (PNG, JPEG, or WebP)");
		}
		const std::string mimePart = trimmed.substr(0, comma);
		if (!StartsWithCaseInsensitive(mimePart, "data:image/png;base64") &&
			!StartsWithCaseInsensitive(mimePart, "data:image/jpeg;base64") &&
			!StartsWithCaseInsensitive(mimePart, "data:image/webp;base64"))
		{
			PHANTASMA_EXCEPTION("Token metadata icon must be a base64-encoded data URI (PNG, JPEG, or WebP)");
		}

		std::string payload = TrimWhitespace(trimmed.substr(comma + 1));
		if (payload.empty())
		{
			PHANTASMA_EXCEPTION("Token metadata icon must include a non-empty base64 payload");
		}

		if ((payload.size() % 4) != 0)
		{
			PHANTASMA_EXCEPTION("Token metadata icon payload is not valid base64");
		}
		for (const char c : payload)
		{
			if (!IsBase64Char(c))
			{
				PHANTASMA_EXCEPTION("Token metadata icon payload is not valid base64");
			}
		}

		const ByteArray decoded = Base64::Decode(payload.c_str(), (int)payload.size());
		if (decoded.empty())
		{
			PHANTASMA_EXCEPTION("Token metadata icon must include a non-empty base64 payload");
		}

		const String encoded = Base64::Encode(decoded);
		const std::string encodedStr(encoded.begin(), encoded.end());
		if (TrimEndEquals(encodedStr) != TrimEndEquals(payload))
		{
			PHANTASMA_EXCEPTION("Token metadata icon payload is not valid base64");
		}
	}

public:
	static ByteArray BuildAndSerialize(const std::vector<std::pair<std::string, std::string>>& metaFields)
	{
		const std::vector<std::string> required = { "name", "icon", "url", "description" };
		std::map<std::string, std::string> lookup;
		for (const auto& kv : metaFields)
		{
			lookup[kv.first] = kv.second;
		}
		if (lookup.size() < required.size())
		{
			PHANTASMA_EXCEPTION("Token metadata is mandatory");
		}
		std::vector<std::string> missing;
		for (const auto& field : required)
		{
			auto it = lookup.find(field);
			if (it == lookup.end() || TrimWhitespace(it->second).empty())
			{
				missing.push_back(field);
			}
		}
		if (!missing.empty())
		{
			std::string list;
			for (size_t i = 0; i < missing.size(); ++i)
			{
				if (i > 0) list += ", ";
				list += missing[i];
			}
			PHANTASMA_EXCEPTION("Token metadata is missing required fields: " + list);
		}
		ValidateIcon(lookup.at("icon"));

		std::vector<std::string> storage;
		storage.reserve(lookup.size());
		std::vector<VmNamedDynamicVariable> fields;
		fields.reserve(lookup.size());
		for (const auto& kv : lookup)
		{
			storage.push_back(kv.second);
			fields.push_back(VmNamedDynamicVariable{ SmallString(kv.first.c_str(), kv.first.size()), VmDynamicVariable(storage.back().c_str()) });
		}

		VmDynamicStruct meta = VmDynamicStruct::Sort((uint32_t)fields.size(), fields.data());
		ByteArray buffer;
		WriteView w(buffer);
		Write(meta, w);
		return buffer;
	}
};

struct TokenSeriesMetadataBuilder
{
	static ByteArray BuildAndSerialize(
		const VmStructSchema& seriesMetadataSchema,
		const uint256& phantasmaSeriesId,
		const std::vector<MetadataField>& metadata)
	{
		Allocator alloc;
		const ByteArray sharedRom = MetadataHelper::GetOptionalBytesField(metadata, "rom");

		std::vector<VmNamedDynamicVariable> fields = {
			VmNamedDynamicVariable{ StandardMeta::id, VmDynamicVariable(phantasmaSeriesId) },
			VmNamedDynamicVariable{ SmallString("mode"), VmDynamicVariable((uint8_t)(sharedRom.empty() ? 0 : 1)) },
			VmNamedDynamicVariable{ SmallString("rom"), VmDynamicVariable(ByteView{ sharedRom.data(), sharedRom.size() }) },
		};

		for (uint32_t i = 0; i != seriesMetadataSchema.numFields; ++i)
		{
			const VmNamedVariableSchema& schemaField = seriesMetadataSchema.fields[i];
			bool isDefault = false;
			for (const auto& field : MetadataHelper::SeriesDefaultMetadataFields)
			{
				if (field.name == schemaField.name.c_str())
				{
					isDefault = true;
					break;
				}
			}
			if (isDefault)
			{
				continue;
			}

			MetadataHelper::PushMetadataField(schemaField, fields, metadata, alloc);
		}

		VmDynamicStruct meta = VmDynamicStruct::Sort((uint32_t)fields.size(), fields.data());
		ByteArray buffer;
		WriteView w(buffer);
		Write(meta, seriesMetadataSchema, w);
		return buffer;
	}

	static ByteArray BuildAndSerialize(
		const VmStructSchema& seriesMetadataSchema,
		const int256& phantasmaSeriesId,
		const std::vector<MetadataField>& metadata)
	{
		return BuildAndSerialize(seriesMetadataSchema, phantasmaSeriesId.Unsigned(), metadata);
	}
};

struct SeriesInfoBuilder
{
	static SeriesInfoOwned Build(
		const VmStructSchema& seriesMetadataSchema,
		const uint256& phantasmaSeriesId,
		uint32_t maxMint,
		uint32_t maxSupply,
		const Bytes32& ownerPublicKey,
		const std::vector<MetadataField>& metadata)
	{
		SeriesInfoOwned owned;
		owned.view.maxMint = maxMint;
		owned.view.maxSupply = maxSupply;
		owned.view.owner = ownerPublicKey;
		owned.metadataStorage = TokenSeriesMetadataBuilder::BuildAndSerialize(seriesMetadataSchema, phantasmaSeriesId, metadata);
		owned.view.rom = VmStructSchema{};
		owned.view.ram = VmStructSchema{};
		return owned;
	}

	static SeriesInfoOwned Build(
		const VmStructSchema& seriesMetadataSchema,
		const int256& phantasmaSeriesId,
		uint32_t maxMint,
		uint32_t maxSupply,
		const Bytes32& ownerPublicKey,
		const std::vector<MetadataField>& metadata)
	{
		return Build(seriesMetadataSchema, phantasmaSeriesId.Unsigned(), maxMint, maxSupply, ownerPublicKey, metadata);
	}

	static SeriesInfoOwned Build(const int256& phantasmaSeriesId, uint32_t maxMint, uint32_t maxSupply, const Bytes32& ownerPublicKey, const ByteArray* metadata = nullptr)
	{
		if (!metadata)
		{
			PHANTASMA_EXCEPTION("series metadata is required");
		}
		SeriesInfoOwned owned;
		owned.view.maxMint = maxMint;
		owned.view.maxSupply = maxSupply;
		owned.view.owner = ownerPublicKey;
		owned.metadataStorage = *metadata;
		owned.view.rom = VmStructSchema{};
		owned.view.ram = VmStructSchema{};
		return owned;
	}
};

struct NftRomBuilder
{
	static ByteArray BuildAndSerialize(
		const VmStructSchema& nftRomSchema,
		const uint256& phantasmaNftId,
		const std::vector<MetadataField>& metadata)
	{
		Allocator alloc;
		const ByteArray rom = MetadataHelper::GetOptionalBytesField(metadata, "rom");

		std::vector<VmNamedDynamicVariable> fields = {
			VmNamedDynamicVariable{ StandardMeta::id, VmDynamicVariable(phantasmaNftId) },
			VmNamedDynamicVariable{ SmallString("rom"), VmDynamicVariable(ByteView{ rom.data(), rom.size() }) },
		};

		for (uint32_t i = 0; i != nftRomSchema.numFields; ++i)
		{
			const VmNamedVariableSchema& schemaField = nftRomSchema.fields[i];
			bool isDefault = false;
			for (const auto& field : MetadataHelper::NftDefaultMetadataFields)
			{
				if (field.name == schemaField.name.c_str())
				{
					isDefault = true;
					break;
				}
			}
			if (isDefault)
			{
				continue;
			}

			MetadataHelper::PushMetadataField(schemaField, fields, metadata, alloc);
		}

		VmDynamicStruct romStruct = VmDynamicStruct::Sort((uint32_t)fields.size(), fields.data());
		ByteArray buffer;
		WriteView w(buffer);
		Write(romStruct, nftRomSchema, w);
		return buffer;
	}

	static ByteArray BuildAndSerialize(
		const VmStructSchema& nftRomSchema,
		const int256& phantasmaNftId,
		const std::vector<MetadataField>& metadata)
	{
		return BuildAndSerialize(nftRomSchema, phantasmaNftId.Unsigned(), metadata);
	}

	static ByteArray BuildAndSerialize(
		const int256& phantasmaNftId,
		const std::string& name,
		const std::string& description,
		const std::string& imageURL,
		const std::string& infoURL,
		uint32_t royalties,
		const ByteArray& rom,
		const TokenSchemas* tokenSchemas)
	{
		const TokenSchemasOwned tsOwned = tokenSchemas
			? TokenSchemasOwned{ *tokenSchemas, {}, {}, {} }
			: TokenSchemasBuilder::PrepareStandardTokenSchemas();

		std::vector<MetadataField> metadata = {
			MetadataField{ "name", MetadataValue::FromString(name) },
			MetadataField{ "description", MetadataValue::FromString(description) },
			MetadataField{ "imageURL", MetadataValue::FromString(imageURL) },
			MetadataField{ "infoURL", MetadataValue::FromString(infoURL) },
			MetadataField{ "royalties", MetadataValue::FromInt64((int64_t)royalties) },
			MetadataField{ "rom", MetadataValue::FromBytes(rom) },
		};

		return BuildAndSerialize(tsOwned.view.rom, phantasmaNftId, metadata);
	}
};

struct TokenInfoBuilder
{
	static TokenInfoOwned Build(const std::string& symbol, const intx& maxSupply, bool isNFT, uint8_t decimals, const Bytes32& creatorPublicKey, const ByteArray& metadata, const ByteArray* tokenSchemas = nullptr)
	{
		if (symbol.empty())
		{
			PHANTASMA_EXCEPTION("Symbol validation error: Empty string is invalid");
		}
		if (symbol.size() > 255)
		{
			PHANTASMA_EXCEPTION("Symbol validation error: Too long");
		}
		for (char c : symbol)
		{
			if (c < 'A' || c > 'Z')
			{
				PHANTASMA_EXCEPTION("Symbol validation error: Anything outside A-Z is forbidden (digits, accents, etc.)");
			}
		}

		if (metadata.empty())
		{
			PHANTASMA_EXCEPTION("metadata is required");
		}

		const bool isInt64Safe = maxSupply.Int256().Is8ByteSafe();

		TokenInfoOwned owned;
		owned.view.maxSupply = (const intx_pod&)maxSupply;
		owned.view.flags = TokenFlags_None;
		if (isNFT)
		{
			if (!isInt64Safe)
			{
				PHANTASMA_EXCEPTION("NFT maximum supply must fit into Int64");
			}
			owned.view.flags = TokenFlags_NonFungible;
		}
		else if (!isInt64Safe)
		{
			owned.view.flags = TokenFlags_BigFungible;
		}
		owned.view.decimals = decimals;
		owned.view.owner = creatorPublicKey;
		owned.view.symbol = SmallString(symbol.c_str(), symbol.size());
		owned.metadataStorage = metadata;
		if (isNFT)
		{
			if (!tokenSchemas)
			{
				PHANTASMA_EXCEPTION("tokenSchemas is required for NFTs");
			}
			owned.schemasStorage = *tokenSchemas;
		}
		return owned;
	}
};

} // namespace phantasma::carbon
