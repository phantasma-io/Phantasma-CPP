#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif

#include <cstdint>
#include <vector>

#include "../Cryptography/EdDSA/Ed25519Signature.h"
#include "../Cryptography/KeyPair.h"
#include "Carbon.h"
#include "DataCommon.h"
#include "DataVm.h"

namespace phantasma::carbon::Blockchain {

enum class TxRejection
{
	Valid = 0,
	DataFormat = 1,
	GasFees = 2,
	DataFees = 3,
	Witnesses = 4,
	Expired = 5,
	Contract = 6,
	Payload = 7,
};

struct ChainConfig
{
	uint8_t version = 0;
	uint8_t reserved1 = 0;
	uint8_t reserved2 = 0;
	uint8_t reserved3 = 0;
	uint32_t allowedTxTypes = 0;
	uint32_t expiryWindow = 0;
	uint32_t blockRateTarget = 0;
};

struct GasConfig
{
	uint8_t version = 0;
	uint8_t maxNameLength = 0;
	uint8_t maxTokenSymbolLength = 0;
	uint8_t feeShift = 0;
	uint32_t maxStructureSize = 0;
	uint64_t feeMultiplier = 0;
	uint64_t gasTokenId = 0;
	uint64_t dataTokenId = 0;
	uint64_t minimumGasOffer = 0;
	uint64_t dataEscrowPerRow = 0;
	uint64_t gasFeeTransfer = 0;
	uint64_t gasFeeQuery = 0;
	uint64_t gasFeeCreateTokenBase = 0;
	uint64_t gasFeeCreateTokenSymbol = 0;
	uint64_t gasFeeCreateTokenSeries = 0;
	uint64_t gasFeePerByte = 0;
	uint64_t gasFeeRegisterName = 0;
	uint64_t gasBurnRatioMul = 0;
	uint8_t gasBurnRatioShift = 0;
};

struct MsgCallArgs
{
	int32_t registerOffset = 0;
	ByteView args{};
};
struct MsgCallArgSections
{
	int32_t numArgSections_negative = 0;
	MsgCallArgs* argSections = nullptr;
};

struct TxMsgCall
{
	uint32_t moduleId = 0;
	uint32_t methodId = 0;
	ByteView args{};
	MsgCallArgSections sections{};
};

struct TxMsgCall_Multi
{
	uint32_t numCalls = 0;
	TxMsgCall* calls = nullptr;
};

struct TxMsgSpecialResolution
{
	uint64_t resolutionId = 0;
	TxMsgCall_Multi calls{};
};

struct TxMsgTransferFungible
{
	Bytes32 to;
	uint64_t tokenId = 0;
	uint64_t amount = 0;
};
struct TxMsgTransferFungible_GasPayer
{
	Bytes32 to;
	Bytes32 from;
	uint64_t tokenId = 0;
	uint64_t amount = 0;
};

struct TxMsgTransferNonFungible_Single
{
	Bytes32 to;
	uint64_t tokenId = 0;
	uint64_t instanceId = 0;
};
struct TxMsgTransferNonFungible_Single_GasPayer
{
	Bytes32 to;
	Bytes32 from;
	uint64_t tokenId = 0;
	uint64_t instanceId = 0;
};

struct TxMsgTransferNonFungible_Multi
{
	Bytes32 to;
	uint64_t tokenId = 0;
	uint32_t numInstanceIds = 0;
	const uint64_t* instanceIds = nullptr;
};
struct TxMsgTransferNonFungible_Multi_GasPayer
{
	Bytes32 to;
	Bytes32 from;
	uint64_t tokenId = 0;
	uint32_t numInstanceIds = 0;
	const uint64_t* instanceIds = nullptr;
};

struct TxMsgMintFungible
{
	uint64_t tokenId = 0;
	intx_pod amount{};
	Bytes32 to;
};
struct TxMsgBurnFungible
{
	uint64_t tokenId = 0;
	intx_pod amount{};
};
struct TxMsgBurnFungible_GasPayer
{
	uint64_t tokenId = 0;
	intx_pod amount{};
	Bytes32 from;
};
struct TxMsgMintNonFungible
{
	uint64_t tokenId = 0;
	Bytes32 to;
	uint32_t seriesId = 0;
	ByteView rom{};
	ByteView ram{};
};
struct TxMsgBurnNonFungible
{
	uint64_t tokenId = 0;
	uint64_t instanceId = 0;
};
struct TxMsgBurnNonFungible_GasPayer
{
	uint64_t tokenId = 0;
	Bytes32 from;
	uint64_t instanceId = 0;
};

struct TxMsgTrade
{
	uint32_t numTransferF = 0;
	TxMsgTransferFungible_GasPayer* transferF = nullptr;
	uint32_t numTransferN = 0;
	TxMsgTransferNonFungible_Single_GasPayer* transferN = nullptr;
	uint32_t numMintF = 0;
	TxMsgMintFungible* mintF = nullptr;
	uint32_t numBurnF = 0;
	TxMsgBurnFungible_GasPayer* burnF = nullptr;
	uint32_t numMintN = 0;
	TxMsgMintNonFungible* mintN = nullptr;
	uint32_t numBurnN = 0;
	TxMsgBurnNonFungible_GasPayer* burnN = nullptr;
};

struct TxMsgPhantasma
{
	SmallString nexus;
	SmallString chain;
	ByteView script{};
};

struct TxMsgPhantasma_Raw
{
	ByteView transaction{};
};

enum class TxTypes : uint8_t
{
	Call = 0,
	Call_Multi = 1,
	Trade = 2,
	TransferFungible = 3,
	TransferFungible_GasPayer = 4,
	TransferNonFungible_Single = 5,
	TransferNonFungible_Single_GasPayer = 6,
	TransferNonFungible_Multi = 7,
	TransferNonFungible_Multi_GasPayer = 8,
	MintFungible = 9,
	BurnFungible = 10,
	BurnFungible_GasPayer = 11,
	MintNonFungible = 12,
	BurnNonFungible = 13,
	BurnNonFungible_GasPayer = 14,
	Phantasma = 15,
	Phantasma_Raw = 16,
};

enum class TxState : uint8_t
{
	Unknown = 0xFF,
	Rejected = 0,
	Completed = 1,
	Aborted = 2,
	Pending = 3,
};

struct TxMsg
{
	constexpr static uint64_t NoMaxGas = (uint64_t)-1;
	constexpr static uint64_t NoMaxData = (uint64_t)-1;

	TxTypes type = TxTypes::Call;
	int64_t expiry = 0;
	uint64_t maxGas = 0;
	uint64_t maxData = 0;
	Bytes32 gasFrom{};
	SmallString payload{};
	union {
		TxMsgCall call;
		TxMsgCall_Multi callMulti;
		TxMsgTransferFungible transferFt;
		TxMsgTransferFungible_GasPayer transferFtGasPayer;
		TxMsgTransferNonFungible_Single transferNftSingle;
		TxMsgTransferNonFungible_Single_GasPayer transferNftSingleGasPayer;
		TxMsgTransferNonFungible_Multi transferNftMulti;
		TxMsgTransferNonFungible_Multi_GasPayer transferNftMultiGasPayer;
		TxMsgMintFungible mintFungible;
		TxMsgBurnFungible burnFungible;
		TxMsgBurnFungible_GasPayer burnFungibleGasPayer;
		TxMsgMintNonFungible mintNonFungible;
		TxMsgBurnNonFungible burnNonFungible;
		TxMsgBurnNonFungible_GasPayer burnNonFungibleGasPayer;
		TxMsgTrade trade;
		TxMsgPhantasma phantasma;
		TxMsgPhantasma_Raw phantasmaRaw;
	};

	TxMsg()
		: type(TxTypes::Call)
		, expiry(0)
		, maxGas(0)
		, maxData(0)
		, gasFrom()
		, payload()
		, call{}
	{
	}
};

inline void Write(const Witness& in, WriteView& w)
{
	Write(in.address, w);
	Write(in.signature, w);
}

inline void Write(const Witnesses& in, WriteView& w)
{
	Write4((int32_t)in.numWitnesses, w);
	for (uint32_t i = 0; i != in.numWitnesses; ++i)
	{
		Write(in.witnesses[i], w);
	}
}

// Primitive serialization -----------------------------------------------------
inline void Write(const ChainConfig& in, WriteView& w)
{
	Write1(in.version, w);
	Write1(in.reserved1, w);
	Write1(in.reserved2, w);
	Write1(in.reserved3, w);
	Write4((int32_t)in.allowedTxTypes, w);
	Write4((int32_t)in.expiryWindow, w);
	Write4((int32_t)in.blockRateTarget, w);
}
inline void Write(const GasConfig& in, WriteView& w)
{
	Write1(in.version, w);
	Write1(in.maxNameLength, w);
	Write1(in.maxTokenSymbolLength, w);
	Write1(in.feeShift, w);
	Write4((int32_t)in.maxStructureSize, w);
	Write8u(in.feeMultiplier, w);
	Write8u(in.gasTokenId, w);
	Write8u(in.dataTokenId, w);
	Write8u(in.minimumGasOffer, w);
	Write8u(in.dataEscrowPerRow, w);
	Write8u(in.gasFeeTransfer, w);
	Write8u(in.gasFeeQuery, w);
	Write8u(in.gasFeeCreateTokenBase, w);
	Write8u(in.gasFeeCreateTokenSymbol, w);
	Write8u(in.gasFeeCreateTokenSeries, w);
	Write8u(in.gasFeePerByte, w);
	Write8u(in.gasFeeRegisterName, w);
	Write8u(in.gasBurnRatioMul, w);
	Write1(in.gasBurnRatioShift, w);
}

// Tx message serialization ---------------------------------------------------
inline bool Read(MsgCallArgs& out, ReadView& reader, Allocator& alloc)
{
	const Byte* mark = (const Byte*)reader.Mark();
	const int32_t value = Read4(reader);
	if (value >= 0)
	{
		reader.Rewind(mark);
		out.registerOffset = 0;
		return ReadArray(out.args, reader, alloc);
	}
	out.args = {};
	out.registerOffset = value;
	return true;
}

inline bool Read(MsgCallArgSections& out, ReadView& reader, Allocator& alloc)
{
	const int32_t count = Read4(reader);
	if (count >= 0)
	{
		return false;
	}
	out.numArgSections_negative = count;
	const uint32_t length = (uint32_t)(-count);
	if (length == 0)
	{
		out.argSections = nullptr;
		return true;
	}
	out.argSections = alloc.Alloc<MsgCallArgs>(length);
	for (uint32_t i = 0; i != length; ++i)
	{
		if (!Read(out.argSections[i], reader, alloc))
		{
			return false;
		}
	}
	return true;
}

inline bool Read(TxMsgCall& out, ReadView& reader, Allocator& alloc)
{
	out.sections = {};
	out.args = {};
	out.sections.argSections = nullptr;
	out.sections.numArgSections_negative = 0;

	out.moduleId = (uint32_t)Read4(reader);
	out.methodId = (uint32_t)Read4(reader);
	const Byte* mark = (const Byte*)reader.Mark();
	const int32_t len = Read4(reader);
	reader.Rewind(mark);
	if (len >= 0)
	{
		return ReadArray(out.args, reader, alloc);
	}
	return Read(out.sections, reader, alloc);
}

inline bool Read(TxMsgCall_Multi& out, ReadView& reader, Allocator& alloc)
{
	return ReadArray(out.numCalls, out.calls, reader, alloc, [&](TxMsgCall& call, ReadView& r) { return Read(call, r, alloc); });
}

inline bool Read(TxMsgSpecialResolution& out, ReadView& reader, Allocator& alloc)
{
	return Read(out.resolutionId, reader) && Read(out.calls, reader, alloc);
}

inline void Write(const TxMsgCall& in, WriteView& w)
{
	Write4((int32_t)in.moduleId, w);
	Write4((int32_t)in.methodId, w);
	if (in.sections.numArgSections_negative < 0)
	{
		Write4(in.sections.numArgSections_negative, w);
		const uint32_t count = (uint32_t)(-in.sections.numArgSections_negative);
		for (uint32_t i = 0; i != count; ++i)
		{
			const MsgCallArgs& section = in.sections.argSections[i];
			if (section.registerOffset < 0)
			{
				Write4(section.registerOffset, w);
			}
			else
			{
				Write4((int32_t)section.args.length, w);
				if (section.args.length)
				{
					WriteBytes(section.args.bytes, section.args.length, w);
				}
			}
		}
		return;
	}
	Write4((int32_t)in.args.length, w);
	if (in.args.length)
	{
		WriteBytes(in.args.bytes, in.args.length, w);
	}
}

inline void Write(const TxMsgCall_Multi& in, WriteView& w)
{
	Write4((int32_t)in.numCalls, w);
	for (uint32_t i = 0; i != in.numCalls; ++i)
	{
		Write(in.calls[i], w);
	}
}

inline void Write(const TxMsgSpecialResolution& in, WriteView& w)
{
	Write(in.resolutionId, w);
	Write(in.calls, w);
}

inline void Write(const TxMsgTransferFungible& in, WriteView& w)
{
	Write(in.to, w);
	Write8u(in.tokenId, w);
	Write8u(in.amount, w);
}

inline void Write(const TxMsgTransferFungible_GasPayer& in, WriteView& w)
{
	Write(in.to, w);
	Write(in.from, w);
	Write8u(in.tokenId, w);
	Write8u(in.amount, w);
}

inline void Write(const TxMsgTransferNonFungible_Single& in, WriteView& w)
{
	Write(in.to, w);
	Write8u(in.tokenId, w);
	Write8u(in.instanceId, w);
}

inline void Write(const TxMsgTransferNonFungible_Single_GasPayer& in, WriteView& w)
{
	Write(in.to, w);
	Write(in.from, w);
	Write8u(in.tokenId, w);
	Write8u(in.instanceId, w);
}

inline void Write(const TxMsgTransferNonFungible_Multi& in, WriteView& w)
{
	Write(in.to, w);
	Write8u(in.tokenId, w);
	Write4((int32_t)in.numInstanceIds, w);
	for (uint32_t i = 0; i != in.numInstanceIds; ++i)
	{
		Write8u(in.instanceIds[i], w);
	}
}

inline void Write(const TxMsgTransferNonFungible_Multi_GasPayer& in, WriteView& w)
{
	Write(in.to, w);
	Write(in.from, w);
	Write8u(in.tokenId, w);
	Write4((int32_t)in.numInstanceIds, w);
	for (uint32_t i = 0; i != in.numInstanceIds; ++i)
	{
		Write8u(in.instanceIds[i], w);
	}
}

inline void Write(const TxMsgMintFungible& in, WriteView& w)
{
	Write8u(in.tokenId, w);
	Write(in.to, w);
	Write(in.amount, w);
}

inline void Write(const TxMsgBurnFungible& in, WriteView& w)
{
	Write8u(in.tokenId, w);
	Write(in.amount, w);
}

inline void Write(const TxMsgBurnFungible_GasPayer& in, WriteView& w)
{
	Write8u(in.tokenId, w);
	Write(in.from, w);
	Write(in.amount, w);
}

inline void Write(const TxMsgMintNonFungible& in, WriteView& w)
{
	Write8u(in.tokenId, w);
	Write(in.to, w);
	Write4((int32_t)in.seriesId, w);
	WriteArray(ByteArray(in.rom.bytes, in.rom.bytes + in.rom.length), w);
	WriteArray(ByteArray(in.ram.bytes, in.ram.bytes + in.ram.length), w);
}

inline void Write(const TxMsgBurnNonFungible& in, WriteView& w)
{
	Write8u(in.tokenId, w);
	Write8u(in.instanceId, w);
}

inline void Write(const TxMsgBurnNonFungible_GasPayer& in, WriteView& w)
{
	Write8u(in.tokenId, w);
	Write(in.from, w);
	Write8u(in.instanceId, w);
}

inline void Write(const TxMsgTrade& in, WriteView& w)
{
	Write4((int32_t)in.numTransferF, w);
	for (uint32_t i = 0; i != in.numTransferF; ++i) Write(in.transferF[i], w);
	Write4((int32_t)in.numTransferN, w);
	for (uint32_t i = 0; i != in.numTransferN; ++i) Write(in.transferN[i], w);
	Write4((int32_t)in.numMintF, w);
	for (uint32_t i = 0; i != in.numMintF; ++i) Write(in.mintF[i], w);
	Write4((int32_t)in.numBurnF, w);
	for (uint32_t i = 0; i != in.numBurnF; ++i) Write(in.burnF[i], w);
	Write4((int32_t)in.numMintN, w);
	for (uint32_t i = 0; i != in.numMintN; ++i) Write(in.mintN[i], w);
	Write4((int32_t)in.numBurnN, w);
	for (uint32_t i = 0; i != in.numBurnN; ++i) Write(in.burnN[i], w);
}

inline void Write(const TxMsgPhantasma& in, WriteView& w)
{
	Write(in.nexus, w);
	Write(in.chain, w);
	WriteArray(ByteArray(in.script.bytes, in.script.bytes + in.script.length), w);
}

inline void Write(const TxMsgPhantasma_Raw& in, WriteView& w)
{
	WriteArray(ByteArray(in.transaction.bytes, in.transaction.bytes + in.transaction.length), w);
}

inline void Write(const TxMsg& msg, WriteView& w)
{
	Write1((uint8_t)msg.type, w);
	Write8(msg.expiry, w);
	Write8u(msg.maxGas, w);
	Write8u(msg.maxData, w);
	Write(msg.gasFrom, w);
	Write(msg.payload, w);

	switch (msg.type)
	{
	case TxTypes::Call: Write(msg.call, w); break;
	case TxTypes::Call_Multi: Write(msg.callMulti, w); break;
	case TxTypes::Trade: Write(msg.trade, w); break;
	case TxTypes::TransferFungible: Write(msg.transferFt, w); break;
	case TxTypes::TransferFungible_GasPayer: Write(msg.transferFtGasPayer, w); break;
	case TxTypes::TransferNonFungible_Single: Write(msg.transferNftSingle, w); break;
	case TxTypes::TransferNonFungible_Single_GasPayer: Write(msg.transferNftSingleGasPayer, w); break;
	case TxTypes::TransferNonFungible_Multi: Write(msg.transferNftMulti, w); break;
	case TxTypes::TransferNonFungible_Multi_GasPayer: Write(msg.transferNftMultiGasPayer, w); break;
	case TxTypes::MintFungible: Write(msg.mintFungible, w); break;
	case TxTypes::BurnFungible: Write(msg.burnFungible, w); break;
	case TxTypes::BurnFungible_GasPayer: Write(msg.burnFungibleGasPayer, w); break;
	case TxTypes::MintNonFungible: Write(msg.mintNonFungible, w); break;
	case TxTypes::BurnNonFungible: Write(msg.burnNonFungible, w); break;
	case TxTypes::BurnNonFungible_GasPayer: Write(msg.burnNonFungibleGasPayer, w); break;
	case TxTypes::Phantasma: Write(msg.phantasma, w); break;
	case TxTypes::Phantasma_Raw: Write(msg.phantasmaRaw, w); break;
	default: Throw::Assert(false, "Unsupported transaction type"); break;
	}
}

struct SignedTxMsg
{
	TxMsg msg;
	Witnesses witnesses{};
};

inline void Write(const SignedTxMsg& signedMsg, WriteView& w)
{
	Write(signedMsg.msg, w);
	const TxTypes type = signedMsg.msg.type;
	const Witnesses& witnessList = signedMsg.witnesses;

	switch (type)
	{
	case TxTypes::TransferFungible:
	case TxTypes::TransferNonFungible_Single:
	case TxTypes::TransferNonFungible_Multi:
	case TxTypes::MintFungible:
	case TxTypes::BurnFungible:
	case TxTypes::MintNonFungible:
	case TxTypes::BurnNonFungible:
		Throw::Assert(witnessList.numWitnesses == 1 && witnessList.witnesses && witnessList.witnesses[0].address == signedMsg.msg.gasFrom, "invalid witness");
		Write(witnessList.witnesses[0].signature, w);
		return;

	case TxTypes::TransferFungible_GasPayer:
	case TxTypes::TransferNonFungible_Single_GasPayer:
	case TxTypes::TransferNonFungible_Multi_GasPayer:
	case TxTypes::BurnFungible_GasPayer:
	case TxTypes::BurnNonFungible_GasPayer:
	{
		Bytes32 from;
		switch (type)
		{
		case TxTypes::TransferFungible_GasPayer: from = signedMsg.msg.transferFtGasPayer.from; break;
		case TxTypes::TransferNonFungible_Single_GasPayer: from = signedMsg.msg.transferNftSingleGasPayer.from; break;
		case TxTypes::TransferNonFungible_Multi_GasPayer: from = signedMsg.msg.transferNftMultiGasPayer.from; break;
		case TxTypes::BurnFungible_GasPayer: from = signedMsg.msg.burnFungibleGasPayer.from; break;
		case TxTypes::BurnNonFungible_GasPayer: from = signedMsg.msg.burnNonFungibleGasPayer.from; break;
		default: break;
		}
		Throw::Assert(witnessList.numWitnesses == 2 &&
			witnessList.witnesses &&
			witnessList.witnesses[0].address == signedMsg.msg.gasFrom &&
			witnessList.witnesses[1].address == from, "invalid witness");
		Write(witnessList.witnesses[0].signature, w);
		Write(witnessList.witnesses[1].signature, w);
		return;
	}

	case TxTypes::Call:
	case TxTypes::Call_Multi:
	case TxTypes::Trade:
	case TxTypes::Phantasma:
	{
		Write4((int32_t)witnessList.numWitnesses, w);
		for (uint32_t i = 0; i != witnessList.numWitnesses; ++i)
		{
			Write(witnessList.witnesses[i], w);
		}
		return;
	}

	case TxTypes::Phantasma_Raw:
		Throw::Assert(witnessList.numWitnesses == 0, "invalid witness");
		return;

	default:
		Throw::Assert(false, "Unsupported transaction type");
		return;
	}
}

// Helpers --------------------------------------------------------------------
inline ByteArray SerializeTx(const TxMsg& msg)
{
	ByteArray buffer;
	WriteView w(buffer);
	Write(msg, w);
	return buffer;
}

struct TxMsgSigner
{
	static ByteArray SignAndSerialize(const TxMsg& msg, const PhantasmaKeys& keys)
	{
		const ByteArray serializedMsg = SerializeTx(msg);
		const Ed25519Signature sig = Ed25519Signature::Generate(keys, serializedMsg);
		Bytes64 sigBytes(sig.Bytes(), Ed25519Signature::Length);

		Witness witness{ Bytes32(keys.GetPublicKey()), sigBytes };

		SignedTxMsg signedMsg;
		signedMsg.msg = msg;
		signedMsg.witnesses = Witnesses{ 1, &witness };

		ByteArray buffer;
		WriteView w(buffer);
		Write(signedMsg, w);
		return buffer;
	}
};

} // namespace phantasma::carbon::Blockchain
