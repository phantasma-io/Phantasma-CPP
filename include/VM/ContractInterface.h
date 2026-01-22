#pragma once

#include "Opcodes.h"
#include <functional>

namespace phantasma {

struct ContractParameter
{
	String name;//varstring
	VMType type;//byte
};
struct ContractMethod
{
	String name;//varstring
	VMType returnType;//byte
	Int32 offset;
	PHANTASMA_VECTOR<ContractParameter> parameters;

	bool DeserializeData(BinaryReader& reader)
	{
		reader.ReadVarString(name);
		returnType = (VMType)reader.ReadByte();
		reader.Read(offset);
		Byte numParameters = reader.ReadByte();
		parameters.resize(numParameters);
		for( int i=0; i<numParameters; ++i )
		{
			reader.ReadVarString(parameters[i].name);
			parameters[i].type = (VMType)reader.ReadByte();
		}
		return !reader.Error();
	}
	void SerializeData(BinaryWriter& writer) const
	{
		writer.WriteVarString(name);
		writer.Write((Byte)returnType);
		writer.Write((Int32)offset);
		writer.Write((Byte)parameters.size());
		for( int i=0, end=(int)parameters.size(); i!=end; ++i )
		{
			writer.WriteVarString(parameters[i].name);
			writer.Write((Byte)parameters[i].type);
		}
	}
};
struct ContractEvent
{
	Byte value;
	String name;//varstring
	VMType returnType;//byte
	ByteArray description;//ReadByteArray

	bool DeserializeData(BinaryReader& reader)
	{
		value = reader.ReadByte();
		reader.ReadVarString(name);
		returnType = (VMType)reader.ReadByte();
		reader.ReadByteArray(description);
		return !reader.Error();
	}
	void SerializeData(BinaryWriter& writer) const
	{
		writer.Write((Byte)value);
		writer.WriteVarString(name);
		writer.Write((Byte)returnType);
		writer.WriteByteArray(description);
	}
};

struct ContractInterface
{
	PHANTASMA_VECTOR<ContractMethod> methods;
	PHANTASMA_VECTOR<ContractEvent> events;

	const ContractMethod* FindMethod(const String& name) const
	{
		auto f = std::find_if(methods.begin(), methods.end(), [&name](const auto& x) { return x.name == name; });
		if( f == methods.end() )
			return 0;
		return &*f;
	}

	bool Implements( const ContractMethod& method ) const
	{
		const ContractMethod* thisMethod = FindMethod(method.name);
		if( !thisMethod )
			return false;
		if( thisMethod->parameters.size() != method.parameters.size() )
			return false;
		for( size_t i = 0, end = method.parameters.size(); i != end; i++ )
		{
			if( thisMethod->parameters[i].type != method.parameters[i].type )
				return false;
		}
		return true;
	}
	bool Implements( const ContractEvent& evt ) const
	{
		for( auto entry : events )
		{
			if( entry.name == evt.name && entry.value == evt.value && entry.returnType == evt.returnType )
				return true;
		}
		return false;
	}
	bool Implements( const ContractInterface& other ) const
	{
		for( auto method : other.methods )
		{
			if( !Implements(method) )
				return false;
		}
		for( auto evt : other.events )
		{
			if( !Implements(evt) )
				return false;
		}
		return true;
	}

	static ContractInterface FromBytes(const ByteArray& bytes)
	{
		ContractInterface abiObject;
		BinaryReader reader(bytes);
		if( !abiObject.DeserializeData(reader) )
		{
			PHANTASMA_EXCEPTION("invalid abi");
			return {};
		}
		return abiObject;
	}

	bool DeserializeData(BinaryReader& reader)
	{
		Byte numMethods = reader.ReadByte();
		methods.resize(numMethods);
		for( int i=0; i<numMethods; ++i )
		{
			if( !methods[i].DeserializeData(reader) )
				return false;
		}
		Byte numEvents = reader.ReadByte();
		events.resize(numEvents);
		for( int i=0; i<numEvents; ++i )
		{
			if( !events[i].DeserializeData(reader) )
				return false;
		}
		return !reader.Error();
	}
	void SerializeData(BinaryWriter& writer) const
	{
		writer.Write((Byte)methods.size());
		for( int i=0, end=(int)methods.size(); i!=end; ++i )
		{
			methods[i].SerializeData(writer);
		}
		writer.Write((Byte)events.size());
		for( int i=0, end=(int)events.size(); i!=end; ++i )
		{
			events[i].SerializeData(writer);
		}
	}
};

}
