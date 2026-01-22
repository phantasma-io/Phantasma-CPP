#pragma once

#include "../Numerics/BigInteger.h"
#include "../Utils/Timestamp.h"
#include "../Cryptography/Address.h"
#include "../Cryptography/Hash.h"
#if __cplusplus >= 201703L
#include <variant>
#endif
#include <cinttypes>

namespace phantasma {

inline String ToString(Int64 i)
{
	char buffer[64];
	sprintf(buffer, "%" PRId64, i);
	return String(buffer);
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
	template<class T> const T& Data() const { return std::get<T>(m_data); }

	void SetValue(const ByteArray& val, VMType t) { return SetValue(val.empty() ? nullptr : &val.front(), (int)val.size(), t); }
	void SetValue(const Byte* val, int length, VMType t)
	{
		m_type = t;
		switch( m_type )
		{
		case VMType::Bytes:
			m_data = ByteArray(val, val+length);
			break;
		case VMType::Number:
			m_data = (val == 0 || length == 0) ? BigInteger(0) : BigInteger::FromSignedArray(val, length);
			break;
		case VMType::String:
			m_data = String((Char*)val, length);
			break;
		case VMType::Enum:
		{
			Int32 i = 0;
			memcpy(&i, val, PHANTASMA_MIN(length, 4));
			m_data = (int)i;
			break;
		}
		case VMType::Timestamp:
		{
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
			default:
			{
				ByteArray bytes(val, val+length);
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
		case VMType::None:      return VMObject();
		case VMType::String:    return VMObject(AsString());
		case VMType::Timestamp: return VMObject(AsTimestamp());
		case VMType::Bool:      return VMObject(AsBool());
		case VMType::Number:    return VMObject(AsNumber());
		//	case VMType::Bytes:
		//	{
		//		var result = new VMObject();
		//		result.SetValue(srcObj.AsByteArray()); // TODO does this work for all types?
		//		return result;
		//	}
		//	case VMType::Struct:
		//		switch (srcObj.Type)
		//		{
		//			// this allow casting a string into char array (represented as unicode numbers)
		//		case VMType::String:
		//		{
		//			var str = srcObj.AsString();
		//			var chars = str.ToCharArray().Select(x => new BigInteger((uint)x)).ToArray();
		//			return VMObject.FromArray(chars);
		//		}
		//
		//		case VMType::Object: return CastViaReflection(srcObj.Data, 0);
		//
		//		default: throw new Exception($"invalid cast: {srcObj.Type} to {type}");
		//		}
		//
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
		switch( m_type )
		{
		case VMType::None:      return false;
		case VMType::String:    return !Data<String>().empty();
		case VMType::Timestamp: return !!Data<Timestamp>().Value;
		case VMType::Bool:      return Data<bool>();
		case VMType::Number: return !Data<BigInteger>().IsZero();
		case VMType::Enum:   return !!Data<int>();
		case VMType::Struct: return true;
		case VMType::Object: return true;
		case VMType::Bytes:
		{
			const ByteArray& bytes = Data<ByteArray>();
			return !bytes.empty() && !!bytes[0];
		}
		default:
			PHANTASMA_EXCEPTION("invalid cast");
			return false;
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
		case VMType::None:      return String("");
		case VMType::String:    return Data<String>();
		case VMType::Timestamp: return phantasma::ToString(Data<Timestamp>().Value);
		case VMType::Bool:      return Data<bool>() ? "true" : "false";
		case VMType::Number:    return Data<BigInteger>().ToString();
		case VMType::Enum:      return phantasma::ToString(Data<int>());
		case VMType::Bytes:
		{
			const ByteArray& bytes = Data<ByteArray>();
			return bytes.empty() ? String() : String((const PHANTASMA_CHAR*)&bytes[0], bytes.size());
		}
		case VMType::Struct:
		{
			VMType arrayType = GetArrayType();
			if( arrayType == VMType::Number ) // convert array of unicode numbers into a string
			{
				//	var children = GetChildren();
				//	var sb = new StringBuilder();
				//
				//	for (int i = 0; i < children.Count; i++)
				//	{
				//		var key = VMObject.FromObject(i);
				//		var val = children[key];
				//
				//		var ch = (char)((uint)val.AsNumber());
				//
				//		sb.Append(ch);
				//	}
				//
				//	return sb.ToString();
			}
			else
			{
				//	using (var stream = new MemoryStream())
				//	{
				//		using (var writer = new BinaryWriter(stream))
				//		{
				//			SerializeData(writer);
				//		}
				//		return Convert.ToBase64String(stream.ToArray());
				//	}
			}
			break;
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
		PHANTASMA_TRY {
			ret = AsString();
		} PHANTASMA_CATCH_ALL() {
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
		case VMType::String:
		{
			const String& s = Data<String>();
			return Hash::Parse(s);
		}
		case VMType::Object:
			if( std::holds_alternative<Hash>(m_data) )
				return Data<Hash>();
		case VMType::Bytes:
		{
			const ByteArray& bytes = Data<ByteArray>();
			if (bytes.size() == Hash::Length)
				return Hash(bytes);
			if (bytes.size() == Hash::Length + 1)
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
		case VMType::String:
		{
			const String& s = Data<String>();
			if( Address::IsValidAddress(s) )
				return Address::FromText(s);
		}
		case VMType::Object:
			if( std::holds_alternative<Address>(m_data) )
				return Data<Address>();
		case VMType::Bytes:
		{
			const ByteArray& bytes = Data<ByteArray>();
			if (bytes.size() == Address::LengthInBytes)
				return Address(bytes);
			if (bytes.size() == Address::LengthInBytes + 1)
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
		case VMType::Number:    return Data<BigInteger>();
		case VMType::None:      return BigInteger(0);
		case VMType::Timestamp: return BigInteger(Data<Timestamp>().Value);
		case VMType::Enum:      return BigInteger(Data<int>());
		case VMType::Bool:      return BigInteger(Data<bool>() ? 1 : 0);
		case VMType::String:
		{
			BigInteger result;
			if( BigInteger::_TryParse(Data<String>(), result) )
				return result;
			break;
		}
		case VMType::Bytes:
		{
			const ByteArray& bytes = Data<ByteArray>();
			return BigInteger(bytes);
		}
		default:
			break;
		}
		PHANTASMA_EXCEPTION("invalid cast");
		return {};
	}
	ByteArray AsByteArray() const
	{
		switch( m_type )
		{
		case VMType::None:      return ByteArray();
		case VMType::Bytes:     return Data<ByteArray>();
		case VMType::Number:    return Data<BigInteger>().ToSignedByteArray();
		case VMType::Bool:     // return ByteArray(1, (Byte)(Data<bool>() ? 1 : 0));
		{ ByteArray temp; temp.resize(1); temp[0] = (Byte)(Data<bool>() ? 1 : 0); return temp; }
		case VMType::Timestamp:
		{
			Int32 data = Data<Timestamp>().Value;
			return ByteArray((const Byte*)&data, (const Byte*)(&data + 1));
		}
		case VMType::Enum:
		{
			Int32 data = Data<int>();
			return ByteArray((const Byte*)&data, (const Byte*)(&data + 1));
		}
		case VMType::String:
		{
			const String& data = Data<String>();
			return ByteArray((const Byte*)data.c_str(), (const Byte*)data.c_str() + data.length());
		}
		//case VMType::Struct:
			//todo return this.Serialize();
		//case VMType::Object:
		//{
		//	var serializable = Data as ISerializable;
		//	if (serializable != null)
		//	{
		//		var bytes = serializable.Serialize().Skip(1).ToArray();
		//		return bytes;
		//	}
		//
		//	throw new Exception("Complex object type can't be");
		//}
		//	break;
		default:
			PHANTASMA_EXCEPTION("invalid cast");
			return {};
		}
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
	//	var children = GetChildren();
	//
	//	for( int i = 0; i < children.Count; i++ )
	//	{
	//		var key = VMObject.FromObject(i);
	//
	//		if( !children.ContainsKey(key) )
	//		{
	//			return VMType::None;
	//		}
	//
	//		var val = children[key];
	//
	//		if( result == VMType::None )
	//		{
	//			result = val.Type;
	//		}
	//		else if( val.Type != result )
	//		{
	//			return VMType::None;
	//		}
	//	}

		return result;
	}

	void SetKey(const VMObject& key, const VMObject& value)
	{
		if(key.m_type == VMType::None)
			PHANTASMA_EXCEPTION("invalid key type");

		if(m_type == VMType::None)
		{
			m_type = VMType::Struct;
			m_data = VMStructure();
		}
		if(m_type == VMType::Struct)
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
				data.push_back({key, value});
		}
		else
			PHANTASMA_EXCEPTION("Invalid cast: expected struct");
	}
	VMObject GetKey(const VMObject& key) const
	{
		if(key.m_type == VMType::None)
			PHANTASMA_EXCEPTION("invalid key type");

		if(m_type == VMType::Struct)
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
		case VMType::Bool:
		{
			bool b = reader.ReadBool();
			m_data = b;
			break;
		}

		// NOTE object type information is lost during serialization, so we reconstruct it as byte array
		case VMType::Object:
			t = VMType::Bytes;
		case VMType::Bytes:
		{
			ByteArray bytes;
			reader.ReadByteArray(bytes);
			m_data = bytes;
			break;
		}

		case VMType::Number:
		{
			BigInteger n;
			reader.ReadBigInteger(n);
			m_data = n;
			break;
		}

		case VMType::Timestamp:
		{
			Timestamp n;
			reader.ReadUInt32(n.Value);
			m_data = n;
			break;
		}

		case VMType::String:
		{
			String s;
			reader.ReadVarString(s);
			m_data = s;
			CarbonAssert(s.length() == strlen(s.c_str()));
			break;
		}

		case VMType::Struct:
		{
			Int64 childCount;
			reader.ReadVarInt(childCount);
			VMStructure children;
			while( childCount--> 0 )
			{
				VMObject key, val;
				key.DeserializeData(reader);
				val.DeserializeData(reader);
				children.push_back({ key, val });
			}
			m_data = children;
			break;
		}

		case VMType::Enum:
		{
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
		case VMType::Bool:
		{
			writer.Write((Byte)(Data<bool>()?1:0));
			break;
		}

		case VMType::Object:
		{
			ByteArray bytes = AsByteArray();
			writer.WriteByteArray(bytes);
			break;
		}
		case VMType::Bytes:
		{
			const ByteArray& bytes = Data<ByteArray>();
			writer.WriteByteArray(bytes);
			break;
		}

		case VMType::Number:
		{
			writer.WriteBigInteger(Data<BigInteger>());
			break;
		}

		case VMType::Timestamp:
		{
			writer.Write((UInt32)Data<Timestamp>().Value);
 			break;
		}

		case VMType::String:
		{
			writer.WriteVarString(Data<String>());
			break;
		}

		case VMType::Struct:
		{
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

		case VMType::Enum:
		{
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

}
