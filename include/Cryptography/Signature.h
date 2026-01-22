#pragma once

#include "EdDSA/Ed25519Signature.h"
#include "../Utils/Serializable.h"

namespace phantasma {

class Signature : public Serializable
{
public:
	Signature()
		: m_kind(SignatureKind::None)
	{
	}
	template<class T>
	explicit Signature(const T& sig)
		: m_kind(T::Kind)
		, m_signature(sig)
	{
	}
	Signature(const Signature& other)
		: m_kind(other.m_kind)
		, m_signature(other.m_kind, other.m_signature)
	{
	}
	Signature& operator=(const Signature& o)
	{
		m_kind = o.m_kind;
		switch(m_kind)
		{
		default: PHANTASMA_ASSERT(false); break;
		case SignatureKind::Ed25519: m_signature.ed25519 = o.m_signature.ed25519; break;
		case SignatureKind::Ring:    m_signature.ring    = o.m_signature.ring;	  break;
		}
		return *this;
	}

	SignatureKind Kind() const { return m_kind; }

	bool operator==( const Signature& o ) const  { return Equals(o);  }

	bool Verify( const Byte* message, int messageLength, const Address& address ) const
	{
		return _Verify(message, messageLength, &address, 1) >= 0;
	}
	bool Verify( const Byte* message, int messageLength, const Address* addresses, int numAddresses ) const
	{
		return _Verify(message, messageLength, addresses, numAddresses) >= 0;
	}
	int VerifyIndex( const Byte* message, int messageLength, const Address* addresses, int numAddresses ) const
	{
		return _Verify(message, messageLength, addresses, numAddresses);
	}

	const Ed25519Signature& GetEd25519Signature() const
	{
		return m_signature.ed25519;
	}

private:
	struct RingSignature { BigInteger Y0, S; constexpr static int Length = 42; bool operator==( const RingSignature& o ) const {return false;} int Verify(...)const{return -1;} template<class T>void SerializeData(T&) const {} }; //todo

	SignatureKind m_kind;
	//I'm implementing polymorphism via a union/switch so that Signatures can be copied around by value and stored in arrays without dynamic memory management
	union Variant
	{
		Variant() {}
		Variant(SignatureKind k, const Variant& v) 
		{
			switch(k)
			{
			default: PHANTASMA_ASSERT(false); break;
			case SignatureKind::Ed25519: new(&ed25519)Ed25519Signature(v.ed25519); break;
			case SignatureKind::Ring:    new(&ring)   RingSignature(   v.ring);    break;
			}
		}
		Variant(const Ed25519Signature& sig) : ed25519(sig) {}
		Variant(const RingSignature&    sig) : ring(   sig) {}
		~Variant(){}

		Ed25519Signature ed25519;
		RingSignature    ring;
	} m_signature;

	bool Equals( const Signature& o ) const 
	{
		if( m_kind == o.m_kind )
		{
			switch(m_kind)
			{
			default: PHANTASMA_ASSERT(false); break;
			case SignatureKind::Ed25519: return m_signature.ed25519 == o.m_signature.ed25519;
			case SignatureKind::Ring:    return m_signature.ring    == o.m_signature.ring;
			}
		}
		return false; 
	}

	int _Verify( const Byte* message, int messageLength, const Address* addresses, int numAddresses ) const
	{
		switch(m_kind)
		{
		default: PHANTASMA_ASSERT(false); break;
		case SignatureKind::Ed25519: return m_signature.ed25519.Verify(message, messageLength, addresses, numAddresses);
		case SignatureKind::Ring:    return m_signature.ring   .Verify(message, messageLength, addresses, numAddresses);
		}
		return -1;
	}
public:
	~Signature()
	{
		switch(m_kind)
		{
		default: break;
		case SignatureKind::Ed25519: m_signature.ed25519.~Ed25519Signature(); break;
		case SignatureKind::Ring:    m_signature.ring.   ~RingSignature();    break;
		}
	}


	template<class BinaryWriter>
	void SerializeData(BinaryWriter& writer) const
	{
		writer.Write((Byte)m_kind);
		switch(m_kind)
		{
		default: PHANTASMA_ASSERT(false); break;
		case SignatureKind::Ed25519: m_signature.ed25519.SerializeData(writer); break;
		case SignatureKind::Ring:    m_signature.ring   .SerializeData(writer); break;
		}
	}

	template<class BinaryReader>
	void UnserializeData(BinaryReader& reader)
	{
		SignatureKind kind = (SignatureKind)reader.ReadByte();
		switch(kind)
		{
			case SignatureKind::Ed25519:
			{
				Ed25519Signature temp;
				temp.UnserializeData(reader);
				this->~Signature();
				new(this)Signature{temp};
				break;
			}
			default:
				PHANTASMA_EXCEPTION("todo");
				break;
			//{
			//	RingSignature temp;
			//	temp.UnserializeData(reader);
			//	return Signature(temp);
			//}
		}
	}
};

inline void BinaryReader::ReadSignature(Signature& hash)
{
	ReadSerializable(hash);
}

}
