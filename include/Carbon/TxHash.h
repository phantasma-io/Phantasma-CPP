#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif

#include "DataBlockchain.h"

namespace phantasma::carbon {

inline bool ComputeCarbonTxId(const Blockchain::TxMsg& msg, Bytes32& outTxId)
{
#if !CARBON_HAS_SODIUM_IMPL
	(void)msg;
	(void)outTxId;
	return false;
#else
	const ByteArray serialized = Blockchain::SerializeTx(msg);
	if (serialized.empty())
		return false;
	return crypto_generichash(outTxId.bytes, Bytes32::length, serialized.data(), serialized.size(), nullptr, 0) == 0;
#endif
}

} // namespace phantasma::carbon
