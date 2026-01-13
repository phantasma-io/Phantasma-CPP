#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif

#include "../Cryptography/Address.h"
#include "Carbon.h"

namespace phantasma::carbon {

inline Bytes32 FromPhantasmaSystemAddress(const phantasma::Address& a)
{
	Throw::Assert(a.IsSystem(), "not system address");
	const phantasma::Byte* bytes = a.ToByteArray();
	Throw::Assert(bytes != nullptr, "missing address bytes");
	static_assert(Bytes32::length == 32 && phantasma::Address::LengthInBytes == 34);
	Bytes32 result;
	memcpy(result.bytes, bytes + 2, Bytes32::length);
	return result;
}

inline Bytes32 FromPhantasmaAddress(const phantasma::Address& a)
{
	if (a.IsUser())
	{
		const phantasma::Byte* bytes = a.ToByteArray();
		Throw::Assert(bytes != nullptr, "missing address bytes");
		static_assert(Bytes32::length == 32 && phantasma::Address::LengthInBytes == 34);
		Bytes32 result;
		memcpy(result.bytes, bytes + 2, Bytes32::length);
		return result;
	}
	if (a.IsSystem())
	{
		return FromPhantasmaSystemAddress(a);
	}
	PHANTASMA_EXCEPTION("unsupported address");
	return Bytes32{};
}

} // namespace phantasma::carbon
