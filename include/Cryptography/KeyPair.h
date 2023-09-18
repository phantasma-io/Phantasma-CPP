﻿#pragma once

#include "Address.h"
#include "Entropy.h"
#include "PrivateKey.h"
#include "EdDSA/Ed25519Signature.h"
#include "../Security/SecureMemory.h"

namespace phantasma {

class PhantasmaKeys
{
	PrivateKey privateKey;
	ByteArray  publicKey;
	Address    address;
public:
	[[deprecated]]
	const PrivateKey& PrivateKey() const { return privateKey; }
	[[deprecated]]
	const ByteArray&  PublicKey()  const { return publicKey; }
	[[deprecated]]
	const Address&    Address()    const { return address; }

	const class PrivateKey& GetPrivateKey() const { return privateKey; }
	const ByteArray&        GetPublicKey()  const { return publicKey; }
	const class Address&    GetAddress()    const { return address; }

	const Byte*  PublicKeyBytes()  const { return publicKey.size() ? &publicKey.front() : 0; }
	int          PublicKeyLength() const { return (int)publicKey.size(); }

	PhantasmaKeys()
		: privateKey()
		, address()
	{
	}
	PhantasmaKeys( const Byte* privateKey, int privateKeyLength )
		: privateKey(privateKey, privateKeyLength)
		, publicKey( Ed25519::PublicKeyFromSeed( privateKey, privateKeyLength ) )
		, address( Address::FromKey(*this) )
	{
	}

	PhantasmaKeys& operator=( const PhantasmaKeys& other )
	{
		privateKey = other.privateKey;
		publicKey = other.publicKey;
		address = other.address;
		return *this;
	}

	String ToString() const
	{
		return address.Text();
	}

	bool IsNull() const { return privateKey.IsNull(); }

	static PhantasmaKeys Generate()
	{
		PinnedBytes<PrivateKey::Length> privateKey;
		Entropy::GetRandomBytes( privateKey.bytes, PrivateKey::Length );
		return { privateKey.bytes, PrivateKey::Length };
	}

	static PhantasmaKeys FromWIF(const SecureString& wif)
	{
		return FromWIF(wif.c_str(), wif.length());
	}
	static PhantasmaKeys FromWIF(const Char* wif, int wifStringLength=0)
	{
		PinnedBytes<34> data;
		if( !DecodeWIF(data, wif, wifStringLength) )
		{
			PHANTASMA_EXCEPTION( "Invalid WIF format" );
			Byte nullKey[PrivateKey::Length] = {};
			return PhantasmaKeys( nullKey, PrivateKey::Length );
		}
		return { &data.bytes[1], 32 };
	}

	static bool DecodeWIF(PinnedBytes<34>& out, const Char* wif, int wifStringLength=0)
	{
		if( wifStringLength == 0 )
		{
			wifStringLength = (int)PHANTASMA_STRLEN(wif);
		}
		if( !wif || wif[0] == '\0' || wifStringLength <= 0 )
		{
			return false;
		}
		int size = Base58::CheckDecodeSecure(out.bytes, 34, wif, wifStringLength);
		if( size != 34 || out.bytes[0] != 0x80 || out.bytes[33] != 0x01 )
		{
			return false;
		}
		return true;
	}

	SecureString ToWIF() const
	{
		static_assert( PrivateKey::Length == 32, "uh oh" );
		PinnedBytes<34> temp;
		Byte* data = temp.bytes;
		data[0] = 0x80;
		data[33] = 0x01;
		SecureByteReader read = privateKey.Read();
		PHANTASMA_COPY(read.Bytes(), read.Bytes()+32, data+1);
		return Base58::CheckEncodeSecure(data, 34);
	}

	Ed25519Signature Sign( const ByteArray& message ) const
	{
		return Ed25519Signature::Generate(*this, message);
	}
};


inline Address Address::FromWIF(const Char* wif, int wifStringLength)
{
	return PhantasmaKeys::FromWIF(wif, wifStringLength).GetAddress();
}

}
