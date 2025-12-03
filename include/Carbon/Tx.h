#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif

#include "DataBlockchain.h"
#include "Modules.h"

namespace phantasma::carbon {

enum class ModuleId : uint32_t
{
	Internal = 0xFFFFFFFFu,
	Governance = 0u,
	Token = 1u,
	PhantasmaVm = 2u,
	Organization = 3u,
};

enum class TokenContract_Methods : uint32_t
{
	TransferFungible = 0,
	TransferNonFungible = 1,
	CreateToken = 2,
	MintFungible = 3,
	BurnFungible = 4,
	GetBalance = 5,
	CreateTokenSeries = 6,
	DeleteTokenSeries = 7,
	MintNonFungible = 8,
	BurnNonFungible = 9,
	GetInstances = 10,
	GetNonFungibleInfo = 11,
	GetNonFungibleInfoByRomId = 12,
	GetSeriesInfo = 13,
	GetSeriesInfoByMetaId = 14,
	GetTokenInfo = 15,
	GetTokenInfoBySymbol = 16,
	GetTokenSupply = 17,
	GetSeriesSupply = 18,
	GetTokenIdBySymbol = 19,
	GetBalances = 20,
	CreateMintedTokenSeries = 21,
};

struct TxEnvelope
{
	Blockchain::TxMsg msg{};
	std::vector<ByteArray> buffers;

	const Blockchain::TxMsg& View() const { return msg; }
};

struct FeeOptions
{
	uint64_t gasFeeBase;
	uint64_t feeMultiplier;

	explicit FeeOptions(uint64_t base = 10000, uint64_t multiplier = 1000)
		: gasFeeBase(base)
		, feeMultiplier(multiplier)
	{
	}

	virtual ~FeeOptions() = default;

	virtual uint64_t CalculateMaxGas() const
	{
		return gasFeeBase * feeMultiplier;
	}
};

struct CreateTokenFeeOptions : public FeeOptions
{
	uint64_t gasFeeCreateTokenBase;
	uint64_t gasFeeCreateTokenSymbol;

	CreateTokenFeeOptions(uint64_t base = 10000, uint64_t createTokenBase = 10000000000ULL, uint64_t createTokenSymbol = 10000000000ULL, uint64_t multiplier = 10000)
		: FeeOptions(base, multiplier)
		, gasFeeCreateTokenBase(createTokenBase)
		, gasFeeCreateTokenSymbol(createTokenSymbol)
	{
	}

	uint64_t CalculateMaxGas(const SmallString& symbol) const
	{
		const size_t len = symbol.length;
		uint64_t symbolPart = gasFeeCreateTokenSymbol;
		if (len > 0)
		{
			const size_t shift = len > 0 ? len - 1 : 0;
			if (shift < sizeof(uint64_t) * 8)
			{
				symbolPart >>= shift;
			}
		}
		return (gasFeeBase + gasFeeCreateTokenBase + symbolPart) * feeMultiplier;
	}
};

struct CreateSeriesFeeOptions : public FeeOptions
{
	uint64_t gasFeeCreateSeriesBase;

	CreateSeriesFeeOptions(uint64_t base = 10000, uint64_t createSeriesBase = 2500000000ULL, uint64_t multiplier = 10000)
		: FeeOptions(base, multiplier)
		, gasFeeCreateSeriesBase(createSeriesBase)
	{
	}

	uint64_t CalculateMaxGas() const
	{
		return (gasFeeBase + gasFeeCreateSeriesBase) * feeMultiplier;
	}
};

struct MintNftFeeOptions : public FeeOptions
{
	explicit MintNftFeeOptions(uint64_t base = 10000, uint64_t multiplier = 1000)
		: FeeOptions(base, multiplier)
	{
	}

	uint64_t CalculateMaxGas() const
	{
		return gasFeeBase * feeMultiplier;
	}
};

struct CreateTokenTxHelper
{
	static TxEnvelope BuildTx(const TokenInfo& tokenInfo, const Bytes32& creatorPublicKey, const CreateTokenFeeOptions* feeOptions = nullptr, uint64_t maxData = 0, int64_t expiry = 0)
	{
		const CreateTokenFeeOptions fees = feeOptions ? *feeOptions : CreateTokenFeeOptions();
		const uint64_t maxGas = fees.CalculateMaxGas(tokenInfo.symbol);

		TxEnvelope env;
		env.buffers.push_back(CarbonSerialize(tokenInfo));

		env.msg.type = Blockchain::TxTypes::Call;
		env.msg.expiry = expiry;
		env.msg.maxGas = maxGas;
		env.msg.maxData = maxData;
		env.msg.gasFrom = creatorPublicKey;
		env.msg.payload = SmallString();
		env.msg.call = Blockchain::TxMsgCall{
			(uint32_t)ModuleId::Token,
			(uint32_t)TokenContract_Methods::CreateToken,
			ByteView{ env.buffers.back().data(), env.buffers.back().size() },
			{}
		};
		return env;
	}
};

struct CreateTokenSeriesTxHelper
{
	static TxEnvelope BuildTx(uint64_t tokenId, const SeriesInfo& seriesInfo, const Bytes32& creatorPublicKey, const CreateSeriesFeeOptions* feeOptions = nullptr, uint64_t maxData = 0, int64_t expiry = 0)
	{
		const CreateSeriesFeeOptions fees = feeOptions ? *feeOptions : CreateSeriesFeeOptions();
		const uint64_t maxGas = fees.CalculateMaxGas();

		TxEnvelope env;

		ByteArray argsBuffer;
		WriteView argsWriter(argsBuffer);
		Write8u(tokenId, argsWriter);
		Write(seriesInfo, argsWriter);
		env.buffers.push_back(argsBuffer);

		env.msg.type = Blockchain::TxTypes::Call;
		env.msg.expiry = expiry;
		env.msg.maxGas = maxGas;
		env.msg.maxData = maxData;
		env.msg.gasFrom = creatorPublicKey;
		env.msg.payload = SmallString();
		env.msg.call = Blockchain::TxMsgCall{
			(uint32_t)ModuleId::Token,
			(uint32_t)TokenContract_Methods::CreateTokenSeries,
			ByteView{ env.buffers.back().data(), env.buffers.back().size() },
			{}
		};
		return env;
	}
};

struct MintNonFungibleTxHelper
{
	static TxEnvelope BuildTx(
		uint64_t tokenId,
		uint32_t seriesId,
		const Bytes32& senderPublicKey,
		const Bytes32& receiverPublicKey,
		const ByteArray& rom,
		const ByteArray& ram,
		const MintNftFeeOptions* feeOptions = nullptr,
		uint64_t maxData = 0,
		int64_t expiry = 0)
	{
		const MintNftFeeOptions fees = feeOptions ? *feeOptions : MintNftFeeOptions();
		const uint64_t maxGas = fees.CalculateMaxGas();

		TxEnvelope env;
		env.msg.type = Blockchain::TxTypes::MintNonFungible;
		env.msg.expiry = expiry;
		env.msg.maxGas = maxGas;
		env.msg.maxData = maxData;
		env.msg.gasFrom = senderPublicKey;
		env.msg.payload = SmallString();
		env.msg.mintNonFungible = Blockchain::TxMsgMintNonFungible{
			tokenId,
			receiverPublicKey,
			seriesId,
			ByteView{ rom.data(), rom.size() },
			ByteView{ ram.data(), ram.size() }
		};
		return env;
	}
};

inline ByteArray SignAndSerialize(const TxEnvelope& env, const PhantasmaKeys& keys)
{
	return Blockchain::TxMsgSigner::SignAndSerialize(env.msg, keys);
}

} // namespace phantasma::carbon
