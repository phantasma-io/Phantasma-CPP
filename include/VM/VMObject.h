#pragma once

#include "../Numerics/BigInteger.h"
#if __cplusplus >= 201703L
#include <variant>
#endif

namespace phantasma {

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
typedef PHANTASMA_VECTOR<PHANTASMA_PAIR<VMObject, VMObject>> VMStructure;

#if __cplusplus >= 201703L
class VMObject
{
	typedef std::variant<bool, int, Timestamp, String, ByteArray, BigInteger, VMStructure> Type;
	VMType type;
	Type data;

public:
	const VMType& GetType() const { return type; }
	template<class T>
	const T& Data() const { return std::get<T>(data); }
	template<class BinaryReader>
	bool DeserializeData(BinaryReader& reader)
	{
		VMType t = (VMType)reader.ReadByte();
		switch (t)
		{
		case VMType::Bool:
		{
			bool b = reader.ReadBool();
			data = b;
			break;
		}

		// NOTE object type information is lost during serialization, so we reconstruct it as byte array
		case VMType::Object:
			t = VMType::Bytes;
		case VMType::Bytes:
		{
			ByteArray bytes;
			reader.ReadByteArray(bytes);
			data = bytes;
			break;
		}

		case VMType::Number:
		{
			BigInteger n;
			reader.ReadBigInteger(n);
			data = n;
			break;
		}

		case VMType::Timestamp:
		{
			Timestamp n;
			reader.ReadUInt32(n.Value);
			data = n;
			break;
		}
		
		case VMType::String:
		{
			String s;
			reader.ReadVarString(s);
			data = s;
			break;
		}
		
		case VMType::Struct:
		{
			Int64 childCount;
			reader.ReadVarInt(childCount);
			VMStructure children;
			while (childCount --> 0)
			{
				VMObject key, val;
				key.DeserializeData(reader);
				val.DeserializeData(reader);
				children.push_back({key, val});
			}
			data = children;
			break;
		}

		case VMType::Enum:
		{
			Int64 i;
			reader.ReadVarInt(i);
			data = (int)i;
			break;
		}

		case VMType::None:
			break;
		
		default:
			//	throw new Exception($"invalid unserialize: type {this.Type}");
			return false;
		}
		type = t;
		return !reader.Error();
	}
};
#endif

}
