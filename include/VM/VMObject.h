#pragma once

#include "../Numerics/Base64.h"
#include "../Numerics/BigInteger.h"
#include "../Utils/Timestamp.h"
#include "../Utils/BinaryWriter.h"
#include "../Utils/TextUtils.h"
#include "../Cryptography/Address.h"
#include "../Cryptography/Hash.h"
#if __cplusplus >= 201703L
#include <variant>
#endif
#include <cinttypes>
#include <codecvt>
#include <locale>

#ifndef PHANTASMA_PROFILE
#define PHANTASMA_PROFILE(name)
#endif

#ifndef CARBON_WARNING_PUSH
#define CARBON_WARNING_PUSH
#define CARBON_WARNING_POP
#define CARBON_WARNING_DISABLE_FALLTHROUGH
#endif

namespace phantasma {

inline String ToString(Int64 i)
{
	char buffer[64];
	sprintf(buffer, "%" PRId64, i);
	return String(buffer);
}

inline std::u16string Utf16UnitsFromVmString(const String& input)
{
	ByteArray utf8;
	CopyUTF8Bytes(input, utf8);
	std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
	const char* begin = utf8.empty() ? "" : (const char*)utf8.data();
	return converter.from_bytes(begin, begin + utf8.size());
}

inline String VmStringFromUtf16Units(const std::u16string& units)
{
	std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
	return FromUTF8(converter.to_bytes(units).c_str());
}

enum class VMType
{
	None,
	Struct,
	Bytes,
	Number,
	String,
	Timestamp,
	Bool,
	Enum,
	Object
};

inline const Char* VMTypeName(VMType type)
{
	switch( type )
	{
	case VMType::None:
		return PHANTASMA_LITERAL("None");
	case VMType::Struct:
		return PHANTASMA_LITERAL("Struct");
	case VMType::Bytes:
		return PHANTASMA_LITERAL("Bytes");
	case VMType::Number:
		return PHANTASMA_LITERAL("Number");
	case VMType::String:
		return PHANTASMA_LITERAL("String");
	case VMType::Timestamp:
		return PHANTASMA_LITERAL("Timestamp");
	case VMType::Bool:
		return PHANTASMA_LITERAL("Bool");
	case VMType::Enum:
		return PHANTASMA_LITERAL("Enum");
	case VMType::Object:
		return PHANTASMA_LITERAL("Object");
	default:
		return PHANTASMA_LITERAL("Unknown");
	}
}

class VMObject;
class ExecutionContext;
typedef PHANTASMA_PAIR<VMObject, VMObject> VMProperty;
typedef PHANTASMA_VECTOR<VMProperty> VMStructure;

#if __cplusplus >= 201703L
class VMObject
{
	typedef std::variant<bool, int, Timestamp, String, ByteArray, BigInteger, VMStructure, Address, Hash, ExecutionContext*> DataType;
	VMType m_type;
	DataType m_data;

  public:
	VMObject() : m_type(VMType::None) {}
	VMObject(const String& data) : m_type(VMType::String), m_data(data) {}
	VMObject(const Timestamp& data) : m_type(VMType::Timestamp), m_data(data) {}
	VMObject(const BigInteger& data) : m_type(VMType::Number), m_data(data) {}
	VMObject(const Address& data) : m_type(VMType::Object), m_data(data) {}
	VMObject(const Hash& data) : m_type(VMType::Object), m_data(data) {}
	VMObject(bool data) : m_type(VMType::Bool), m_data(data) {}
	VMObject(ExecutionContext& data) : m_type(VMType::Object), m_data(&data) {}
	VMObject(const VMObject& o)
	{
		m_type = o.m_type;
		m_data = o.m_data;
	}
	VMObject& operator=(const VMObject& o)
	{
		m_type = o.m_type;
		m_data = o.m_data;
		return *this;
	}
	VMObject(VMObject&& o)
	{
		m_type = o.m_type;
		m_data = std::move(o.m_data);
		o.m_type = VMType::None;
	}
	VMObject& operator=(VMObject&& o)
	{
		m_type = o.m_type;
		m_data = std::move(o.m_data);
		o.m_type = VMType::None;
		return *this;
	}
	static VMObject FromBytes(const ByteArray& b)
	{
		VMObject o;
		BinaryReader r(b);
		o.DeserializeData(r);
		return o;
	}

	bool operator==(const VMObject& o) const
	{
		return m_type == o.m_type && (m_type == VMType::None || m_data == o.m_data);
	}
	bool operator!=(const VMObject& o) const { return !(*this == o); }

	bool IsEmpty() const { return m_type == VMType::None; }
	VMType Type() const { return m_type; }
	template<class T>
	const T& Data() const { return std::get<T>(m_data); }

	void SetValue(const ByteArray& val, VMType t) { return SetValue(val.empty() ? nullptr : &val.front(), (int)val.size(), t); }
	void SetValue(const Byte* val, int length, VMType t)
	{
		m_type = t;
		switch( m_type )
		{
		case VMType::Bytes:
			m_data = ByteArray(val, val + length);
			break;
		case VMType::Number:
			m_data = (val == 0 || length == 0) ? BigInteger(0) : BigInteger::FromSignedArray(val, length);
			break;
		case VMType::String:
			m_data = String((Char*)val, length);
			break;
		case VMType::Enum: {
			Int32 i = 0;
			memcpy(&i, val, PHANTASMA_MIN(length, 4));
			m_data = (int)i;
			break;
		}
		case VMType::Timestamp: {
			Int32 i = 0;
			memcpy(&i, val, PHANTASMA_MIN(length, 4));
			m_data = Timestamp(i);
			break;
		}
		case VMType::Bool:
			m_data = length ? !!val[0] : false;
			break;
		default:
			switch( length )
			{
			case Address::LengthInBytes:
				m_data = Address(val, length);
				break;
			case Hash::Length:
				m_data = Hash(val, length);
				break;
			default: {
				// Gen2 object payloads on this path are decoded by shape:
				// known fixed-size runtime objects (Address/Hash) first, otherwise
				// treat the bytes as a serialized VMObject blob.
				ByteArray bytes(val, val + length);
				BinaryReader reader(bytes);
				if( !DeserializeData(reader) )
				{
					PHANTASMA_EXCEPTION("Cannot decode interop object from bytes");
				}
				break;
			}
			}
			break;
		}
	}

	VMObject CastTo(VMType type)
	{
		if( m_type == type )
			return *this;
		switch( type )
		{
		case VMType::None:
			return VMObject();
		case VMType::String:
			return VMObject(AsString());
		case VMType::Timestamp:
			return VMObject(AsTimestamp());
		case VMType::Bool:
			return VMObject(AsBool());
		case VMType::Number:
			return VMObject(AsNumber());
		case VMType::Bytes: {
			// Gen2 C# parity: CAST Bytes delegates to AsByteArray() for String/Struct/Object payloads.
			// Legacy SATRN/DEX scripts depend on this conversion in runtime/storage flows.
			VMObject result;
			result.m_type = VMType::Bytes;
			result.m_data = AsByteArray();
			return result;
		}
		case VMType::Struct: {
			if( m_type == VMType::String )
			{
				const std::u16string utf16 = Utf16UnitsFromVmString(AsString());
				VMObject result;
				for( size_t i = 0; i < utf16.size(); ++i )
				{
					result.SetKey(VMObject(BigInteger((Int64)i)), VMObject(BigInteger((Int64)(uint16_t)utf16[i])));
				}
				return result;
			}
			if( m_type == VMType::Object )
			{
				// Gen2 CastViaReflection keeps known serializable runtime object shapes (Address/Hash)
				// as VMType::Object even when the requested target is Struct. The C++ VM only exposes
				// those known object shapes here, so preserve them instead of inventing a partial
				// reflection-based Struct view that would not match runtime behavior.
				return *this;
			}

			// Gen2 also exposes reflection-based Object->Struct conversion for arbitrary interop
			// objects, but the C++ port does not have an equivalent generic reflection surface.
			PHANTASMA_EXCEPTION("invalid cast");
			return {};
		}
		default:
			PHANTASMA_EXCEPTION("invalid cast");
			return {};
		}
	}

	ExecutionContext* AsExecutionContext() const
	{
		return m_type == VMType::Object && std::holds_alternative<ExecutionContext*>(m_data) ? Data<ExecutionContext*>() : nullptr;
	}
	bool AsBool() const
	{
		// Gen2 C# only treats Bool, Number, and single-byte Bytes as valid bool inputs.
		// Legacy truthy conversions for String/Struct/Object drift VM opcode semantics.
		if( m_type == VMType::Bytes )
		{
			const ByteArray& bytes = Data<ByteArray>();
			if( bytes.size() == 1 )
				return !!bytes[0];
		}
		switch( m_type )
		{
		case VMType::Bool:
			return Data<bool>();
		case VMType::Number:
			return !AsNumber().IsZero();
		default: {
			String msg = PHANTASMA_LITERAL("Invalid cast: expected bool, got ");
			msg += VMTypeName(m_type);
			PHANTASMA_EXCEPTION_MESSAGE("invalid cast", msg);
			return false;
		}
		}
	}
	int AsEnum() const
	{
		if( m_type == VMType::Enum )
			return Data<int>();
		return (int)AsNumber();
	}
	String AsString() const
	{
		switch( m_type )
		{
		case VMType::None:
			return String("");
		case VMType::String:
			return Data<String>();
		case VMType::Timestamp:
			return phantasma::ToString(Data<Timestamp>().Value);
		case VMType::Bool:
			return Data<bool>() ? "true" : "false";
		case VMType::Number:
			return Data<BigInteger>().ToString();
		case VMType::Enum:
			return phantasma::ToString(Data<int>());
		case VMType::Bytes: {
			const ByteArray& bytes = Data<ByteArray>();
			return bytes.empty() ? String() : String((const Char*)&bytes[0], bytes.size());
		}
		case VMType::Struct: {
			VMType arrayType = GetArrayType();
			if( arrayType == VMType::Number ) // convert array of unicode numbers into a string
			{
				const VMStructure& children = Data<VMStructure>();
				std::u16string text;
				text.reserve(children.size());
				for( int i = 0; i < (int)children.size(); ++i )
				{
					const VMObject key(BigInteger((Int64)i));
					const VMObject value = GetKey(key);
					text.push_back((char16_t)(uint16_t)(Int64)value.AsNumber());
				}
				return VmStringFromUtf16Units(text);
			}
			else
			{
				// Non-character Struct values stringify via serialized VMObject bytes encoded as Base64.
				// This looks unusual, but it is the observable Gen2 behavior for generic struct payloads.
				BinaryWriter writer;
				SerializeData(writer);
				return Base64::Encode(writer.ToArray());
			}
		}
		case VMType::Object:
			if( std::holds_alternative<Address>(m_data) )
				return Data<Address>().ToString();
			if( std::holds_alternative<Hash>(m_data) )
				return Data<Hash>().ToString();
			break;
		default:
			break;
		}
		PHANTASMA_EXCEPTION("invalid cast");
		return {};
	}
	String ToString() const
	{
		String ret;
		PHANTASMA_TRY
		{
			ret = AsString();
		}
		PHANTASMA_CATCH_ALL()
		{
			ret = "[VMObject]";
		}
		return ret;
	}
	Timestamp AsTimestamp() const
	{
		if( m_type == VMType::Timestamp )
			return Data<Timestamp>();
		PHANTASMA_EXCEPTION("invalid cast");
		return Timestamp();
	}
	Hash AsHash() const
	{
		switch( m_type )
		{
		case VMType::String: {
			const String& s = Data<String>();
			return Hash::Parse(s);
		}
			CARBON_WARNING_PUSH
			CARBON_WARNING_DISABLE_FALLTHROUGH
		case VMType::Object:
			if( std::holds_alternative<Hash>(m_data) )
				return Data<Hash>();
		case VMType::Bytes:
			CARBON_WARNING_POP
			{
				const ByteArray& bytes = Data<ByteArray>();
				if( bytes.size() == Hash::Length )
					return Hash(bytes);
				if( bytes.size() == Hash::Length + 1 )
				{
					ByteArray temp;
					temp.resize(Address::LengthInBytes);
					memcpy(&temp[0], &bytes[1], Address::LengthInBytes);
					return Hash(temp);
				}
				PHANTASMA_EXCEPTION("Invalid hash size, expected {Hash.Length} got {bytes.Length}");
			}
		default:
			PHANTASMA_EXCEPTION("invalid cast");
			return {};
		}
	}
	Address AsAddress() const
	{
		switch( m_type )
		{
		case VMType::String: {
			PHANTASMA_PROFILE(StringToAddress);
			const String& s = Data<String>();
			if( Address::IsValidAddress(s) )
				return Address::FromText(s);
			PHANTASMA_EXCEPTION("invalid cast");
			return {};
		}
			CARBON_WARNING_PUSH
			CARBON_WARNING_DISABLE_FALLTHROUGH
		case VMType::Object:
			if( std::holds_alternative<Address>(m_data) )
				return Data<Address>();
		case VMType::Bytes:
			CARBON_WARNING_POP
			{
				const ByteArray& bytes = Data<ByteArray>();
				if( bytes.size() == Address::LengthInBytes )
					return Address(bytes);
				if( bytes.size() == Address::LengthInBytes + 1 )
				{
					ByteArray temp;
					temp.resize(Address::LengthInBytes);
					memcpy(&temp[0], &bytes[1], Address::LengthInBytes);
					return Address(temp);
				}
				PHANTASMA_EXCEPTION("Invalid address size, expected {Address.LengthInBytes} got {bytes.Length}");
			}
		default:
			PHANTASMA_EXCEPTION("invalid cast");
			return {};
		}
	}
	BigInteger AsNumber() const
	{
		switch( m_type )
		{
		case VMType::Number:
			return Data<BigInteger>();
		case VMType::None:
			return BigInteger(0);
		case VMType::Timestamp:
			return BigInteger(Data<Timestamp>().Value);
		case VMType::Enum:
			return BigInteger(Data<int>());
		case VMType::Bool:
			return BigInteger(Data<bool>() ? 1 : 0);
		case VMType::String: {
			PHANTASMA_PROFILE(StringToNumber);
			BigInteger result;
			if( BigInteger::_TryParse(Data<String>(), result) )
				return result;
			StringBuilder sb;
			sb << PHANTASMA_LITERAL("Cannot convert String '");
			sb << Data<String>();
			sb << PHANTASMA_LITERAL("' to BigInteger.");
			PHANTASMA_EXCEPTION_MESSAGE("string to bigint", sb.str());
			return {};
		}
		case VMType::Bytes: {
			const ByteArray& bytes = Data<ByteArray>();
			return BigInteger(bytes);
		}
		case VMType::Object:
			if( std::holds_alternative<Hash>(m_data) )
				return Data<Hash>();
			break;
		default:
			break;
		}
		StringBuilder sb;
		sb << PHANTASMA_LITERAL("Invalid cast: expected number, got ");
		sb << VMTypeName(m_type);
		PHANTASMA_EXCEPTION_MESSAGE("invalid cast", sb.str());
		return {};
	}
	ByteArray AsByteArray() const
	{
		switch( m_type )
		{
		case VMType::None:
			break;
		case VMType::Bytes:
			return Data<ByteArray>();
		case VMType::Number:
			return Data<BigInteger>().ToSignedByteArray();
		case VMType::Bool: // return ByteArray(1, (Byte)(Data<bool>() ? 1 : 0));
		{
			ByteArray temp;
			temp.resize(1);
			temp[0] = (Byte)(Data<bool>() ? 1 : 0);
			return temp;
		}
		case VMType::Timestamp: {
			Int32 data = Data<Timestamp>().Value;
			return ByteArray((const Byte*)&data, (const Byte*)(&data + 1));
		}
		case VMType::Enum: {
			Int32 data = Data<int>();
			return ByteArray((const Byte*)&data, (const Byte*)(&data + 1));
		}
		case VMType::String: {
			const String& data = Data<String>();
			return ByteArray((const Byte*)data.c_str(), (const Byte*)data.c_str() + data.length());
		}
		case VMType::Struct: {
			// Gen2 emits SerializeData() bytes for Struct->Bytes, including VMType prefix.
			// contracts.cpp::Read(VmDynamicStruct, ...) contains a compatibility probe for this exact layout.
			BinaryWriter writer;
			SerializeData(writer);
			return writer.ToArray();
		}
		case VMType::Object: {
			// Keep parity with C# VMObject.AsByteArray for serializable object payloads
			// used by storage interop (Address/Hash are the required runtime shapes here).
			if( std::holds_alternative<Address>(m_data) )
			{
				const Byte* data = Data<Address>().ToByteArray();
				return ByteArray(data, data + Address::LengthInBytes);
			}
			if( std::holds_alternative<Hash>(m_data) )
			{
				const Byte* data = Data<Hash>().ToByteArray();
				return ByteArray(data, data + Hash::Length);
			}
			PHANTASMA_EXCEPTION("Complex object type can't be serialized to byte array");
			return {};
		}
		default:
			break;
		}
		StringBuilder sb;
		sb << PHANTASMA_LITERAL("Invalid cast: expected bytes, got ");
		sb << VMTypeName(m_type);
		PHANTASMA_EXCEPTION_MESSAGE("invalid cast", sb.str());
		return {};
	}

	// this method checks if the VMObject is an array by checking the following rules
	// a) must be a struct
	// b) all keys of the struct must be numeric indexes from 0 to count-1
	// c) all element values must have same type
	VMType GetArrayType() const
	{
		if( m_type != VMType::Struct )
			return VMType::None;

		VMType result = VMType::None;
		const VMStructure& children = Data<VMStructure>();
		for( int i = 0; i < (int)children.size(); ++i )
		{
			const VMObject key(BigInteger((Int64)i));
			const VMProperty* entry = nullptr;
			for( const VMProperty& p : children )
			{
				if( p.first == key )
				{
					entry = &p;
					break;
				}
			}

			if( !entry )
				return VMType::None;

			const VMType valueType = entry->second.Type();
			if( result == VMType::None )
			{
				result = valueType;
			}
			else if( valueType != result )
			{
				return VMType::None;
			}
		}

		return result;
	}

	void SetKey(const VMObject& key, const VMObject& value)
	{
		if( key.m_type == VMType::None )
			PHANTASMA_EXCEPTION("invalid key type");

		if( m_type == VMType::None )
		{
			m_type = VMType::Struct;
			m_data = VMStructure();
		}
		if( m_type == VMType::Struct )
		{
			VMStructure& data = std::get<VMStructure>(m_data);
			bool found = false;
			for( VMProperty& p : data )
			{
				if( p.first == key )
				{
					p.second = value;
					found = true;
					break;
				}
			}
			if( !found )
				data.push_back({ key, value });
		}
		else
			PHANTASMA_EXCEPTION("Invalid cast: expected struct");
	}
	VMObject GetKey(const VMObject& key) const
	{
		if( key.m_type == VMType::None )
			PHANTASMA_EXCEPTION("invalid key type");

		if( m_type == VMType::Struct )
		{
			const VMStructure& data = Data<VMStructure>();
			for( const VMProperty& p : data )
			{
				if( p.first == key )
					return p.second;
			}
		}
		else
			PHANTASMA_EXCEPTION("Invalid cast: expected struct");
		return VMObject();
	}

	template<class BinaryReader>
	bool DeserializeData(BinaryReader& reader)
	{
		VMType t = (VMType)reader.ReadByte();
		switch( t )
		{
		case VMType::Bool: {
			bool b = reader.ReadBool();
			m_data = b;
			break;
		}

		case VMType::Object: {
			ByteArray bytes;
			reader.ReadByteArray(bytes);
			// Gen2 preserves Address payloads on roundtrip, but all other object payloads
			// come back as raw bytes because the object type itself is erased on the wire.
			if( bytes.size() == Address::LengthInBytes + 1 )
			{
				BinaryReader nested(bytes);
				Address address;
				address.UnserializeData(nested);
				if( !nested.Error() )
				{
					m_data = address;
					break;
				}
			}
			t = VMType::Bytes;
			m_data = bytes;
			break;
		}
		case VMType::Bytes: {
			ByteArray bytes;
			reader.ReadByteArray(bytes);
			m_data = bytes;
			break;
		}

		case VMType::Number: {
			BigInteger n;
			reader.ReadBigInteger(n);
			m_data = n;
			break;
		}

		case VMType::Timestamp: {
			Timestamp n;
			reader.ReadUInt32(n.Value);
			m_data = n;
			break;
		}

		case VMType::String: {
			String s;
			reader.ReadVarString(s);
			m_data = s;
			CarbonAssert(s.length() == strlen(s.c_str()));
			break;
		}

		case VMType::Struct: {
			Int64 childCount;
			reader.ReadVarInt(childCount);
			VMStructure children;
			while( childCount-- > 0 )
			{
				VMObject key, val;
				key.DeserializeData(reader);
				val.DeserializeData(reader);
				children.push_back({ key, val });
			}
			m_data = children;
			break;
		}

		case VMType::Enum: {
			Int64 i;
			reader.ReadVarInt(i);
			m_data = (int)i;
			break;
		}

		case VMType::None:
			break;

		default:
			//	throw new Exception($"invalid unserialize: type {this.Type}");
			return false;
		}
		m_type = t;
		return !reader.Error();
	}

	template<class BinaryWriter>
	bool SerializeData(BinaryWriter& writer) const
	{
		writer.Write((Byte)m_type);
		switch( m_type )
		{
		case VMType::Bool: {
			writer.Write((Byte)(Data<bool>() ? 1 : 0));
			break;
		}

		case VMType::Object: {
			BinaryWriter nested;
			// Gen2 object serialization writes the serialized payload as a byte array.
			// That preserves Address object roundtrip semantics and degrades Hash to Bytes.
			if( std::holds_alternative<Address>(m_data) )
			{
				Data<Address>().SerializeData(nested);
			}
			else if( std::holds_alternative<Hash>(m_data) )
			{
				Data<Hash>().SerializeData(nested);
			}
			else
			{
				PHANTASMA_EXCEPTION("Objects of type ExecutionContext cannot be serialized");
				return false;
			}
			writer.WriteByteArray(nested.ToArray());
			break;
		}
		case VMType::Bytes: {
			const ByteArray& bytes = Data<ByteArray>();
			writer.WriteByteArray(bytes);
			break;
		}

		case VMType::Number: {
			writer.WriteBigInteger(Data<BigInteger>());
			break;
		}

		case VMType::Timestamp: {
			writer.Write((UInt32)Data<Timestamp>().Value);
			break;
		}

		case VMType::String: {
			writer.WriteVarString(Data<String>());
			break;
		}

		case VMType::Struct: {
			const VMStructure& s = Data<VMStructure>();
			Int64 childCount = (Int64)s.size();
			writer.WriteVarInt(childCount);
			for( const auto& p : s )
			{
				p.first.SerializeData(writer);
				p.second.SerializeData(writer);
			}
			break;
		}

		case VMType::Enum: {
			writer.WriteVarInt(Data<int>());
			break;
		}

		case VMType::None:
			break;

		default:
			//	throw new Exception($"invalid unserialize: type {this.Type}");
			return false;
		}
		return true;
	}
};

inline bool operator==(const VMStructure& a, const VMStructure& b)
{
	if( a.size() != b.size() )
		return false;

	for( const VMProperty& p : a )
	{
		const VMProperty* found = 0;
		for( const VMProperty& q : b )
		{
			if( p.first == q.first )
			{
				found = &q;
				break;
			}
		}
		if( !found )
			return false;
		if( p.second != found->second )
			return false;
	}
	return true;
}
#endif

} // namespace phantasma
