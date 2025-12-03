#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "Carbon.h"
#include "Alloc.h"
#include "DataCommon.h"

namespace phantasma::carbon {

struct StandardMeta
{
	struct Chain
	{
		inline static const SmallString address{ "_a" };
		inline static const SmallString name{ "_n" };
		inline static const SmallString nexus{ "_x" };
		inline static const SmallString tokenomics{ "_t" };
	};

	struct Token
	{
		inline static const SmallString staking_org_id{ "_soi" };
		inline static const SmallString staking_org_threshold{ "_sot" };
		inline static const SmallString staking_reward_token{ "_srt" };
		inline static const SmallString staking_reward_period{ "_srp" };
		inline static const SmallString staking_reward_mul{ "_srm" };
		inline static const SmallString staking_reward_div{ "_srd" };
		inline static const SmallString staking_lock{ "_sl" };
		inline static const SmallString staking_booster_token{ "_sbt" };
		inline static const SmallString staking_booster_mul{ "_sbm" };
		inline static const SmallString staking_booster_div{ "_sbd" };
		inline static const SmallString staking_booster_limit{ "_sbl" };
		inline static const SmallString phantasma_script{ "_phs" };
		inline static const SmallString phantasma_abi{ "_phb" };
		inline static const SmallString pre_burn{ "_brn" };
		inline static const SmallString inflation_period{ "_ip" };
		inline static const SmallString inflation_amounts{ "_ia" };
		inline static const SmallString inflation_initial_begin{ "_iib" };
		inline static const SmallString inflation_initial_end{ "_iie" };

		struct Nft
		{
			inline static const SmallString name{ "name" };
			inline static const SmallString description{ "description" };

			inline static const SmallString url{ "url" };
			inline static const SmallString icon{ "icon" };
			inline static const SmallString version{ "version" };

			inline static const SmallString imageURL{ "imageURL" };
			inline static const SmallString infoURL{ "infoURL" };
			inline static const SmallString royalties{ "royalties" };
		};
	};

	inline static const SmallString id{ "_i" };
};

enum class VmType : uint8_t
{
	Dynamic = 0,
	Array = 1,
	Bytes = 1 << 1,
	Struct = 2 << 1,
	Int8 = 3 << 1,
	Int16 = 4 << 1,
	Int32 = 5 << 1,
	Int64 = 6 << 1,
	Int256 = 7 << 1,
	Bytes16 = 8 << 1,
	Bytes32 = 9 << 1,
	Bytes64 = 10 << 1,
	String = 11 << 1,

	Array_Bytes = Array | Bytes,
	Array_Struct = Array | Struct,
	Array_Int8 = Array | Int8,
	Array_Int16 = Array | Int16,
	Array_Int32 = Array | Int32,
	Array_Int64 = Array | Int64,
	Array_Int256 = Array | Int256,
	Array_Bytes16 = Array | Bytes16,
	Array_Bytes32 = Array | Bytes32,
	Array_Bytes64 = Array | Bytes64,
	Array_String = Array | String,
};

inline VmType operator|(VmType a, VmType b)
{
	return (VmType)((uint8_t)a | (uint8_t)b);
}
inline VmType operator&(VmType a, VmType b)
{
	return (VmType)((uint8_t)a & (uint8_t)b);
}

template<class T>
struct NameLessThan
{
	bool operator()(const T& a, const T& b) const { return a.name < b.name; }
};

struct VmNamedVariableSchema;
struct VmStructSchema
{
	enum SchemaFlags
	{
		Flag_None = 0,
		Flag_DynamicExtras = 1 << 0,
		Flag_IsSorted = 1 << 1,
	};

	uint32_t numFields = 0;
	const VmNamedVariableSchema* fields = nullptr;
	SchemaFlags flags = Flag_None;

	const VmNamedVariableSchema* operator[](const SmallString&) const;

	static bool IsSorted(uint32_t numFields, VmNamedVariableSchema* fields);
	static VmStructSchema Sort(uint32_t numFields, VmNamedVariableSchema* fields, bool allowDynamicExtras);
	template<int N>
	static VmStructSchema Sort(VmNamedVariableSchema(&arr)[N], bool allowDynamicExtras) { return Sort(N, arr, allowDynamicExtras); }
};

struct VmVariableSchema
{
	VmType type = VmType::Dynamic;
	VmStructSchema structure{};
};

struct VmNamedVariableSchema
{
	SmallString name;
	VmVariableSchema schema;
};

struct VmNamedDynamicVariable;
struct VmDynamicVariable;

struct VmDynamicStruct
{
	uint32_t numFields = 0; // fields *must* be sorted by name
	VmNamedDynamicVariable* fields = nullptr;

	const VmDynamicVariable* operator[](const SmallString&) const;
	VmDynamicVariable* operator[](const SmallString&);

	void Erase(const VmDynamicVariable*);

	template<int N>
	static VmDynamicStruct Sort(VmNamedDynamicVariable(&arr)[N]) { return Sort(N, arr); }
	static VmDynamicStruct Sort(uint32_t numFields, VmNamedDynamicVariable* fields);
	static bool IsSorted(uint32_t numFields, VmNamedDynamicVariable* fields);

	static VmDynamicStruct Merge(const VmDynamicStruct& old, const VmDynamicStruct& updates, Allocator& a);
};

template<int N>
inline VmDynamicStruct Bake(VmNamedDynamicVariable(&arr)[N]) { return VmDynamicStruct::Sort(N, arr); }
inline VmDynamicStruct Bake(uint32_t numFields, VmNamedDynamicVariable* fields) { return VmDynamicStruct::Sort(numFields, fields); }

struct VmStructArray
{
	VmStructSchema schema{};
	const VmDynamicStruct* structs = nullptr;
};

struct VmDynamicVariable
{
	VmType type = VmType::Dynamic;
	union {
		VmDynamicVariable* dynamic;
		ByteView bytes;
		VmDynamicStruct structure;
		uint8_t  int8;
		uint16_t int16;
		uint32_t int32;
		uint64_t int64;
		uint256  int256;
		Bytes16  bytes16;
		Bytes32  bytes32;
		Bytes64  bytes64;
		const char* string;
		VmDynamicVariable* dynamicArray;
		ByteView* bytesArray;
		VmStructArray structureArray;
		uint8_t*  int8Array;
		uint16_t* int16Array;
		uint32_t* int32Array;
		uint64_t* int64Array;
		uint256*  int256Array;
		Bytes16*  bytes16Array;
		Bytes32*  bytes32Array;
		Bytes64*  bytes64Array;
		const char** stringArray;
	} data{};
	uint32_t arrayLength = 0;

	VmDynamicVariable() = default;
	VmDynamicVariable(const VmDynamicVariable&);
	VmDynamicVariable& operator=(const VmDynamicVariable&);

	VmDynamicVariable(ByteView v) : type(VmType::Bytes), arrayLength(1) { data.bytes = v; }
	VmDynamicVariable(const VmDynamicStruct& v) : type(VmType::Struct), arrayLength(1) { data.structure = v; }
	VmDynamicVariable(uint8_t v) : type(VmType::Int8), arrayLength(1) { data.int8 = v; }
	VmDynamicVariable(int8_t v) : type(VmType::Int8), arrayLength(1) { data.int8 = (uint8_t)v; }
	VmDynamicVariable(uint16_t v) : type(VmType::Int16), arrayLength(1) { data.int16 = v; }
	VmDynamicVariable(int16_t v) : type(VmType::Int16), arrayLength(1) { data.int16 = (uint16_t)v; }
	VmDynamicVariable(uint32_t v) : type(VmType::Int32), arrayLength(1) { data.int32 = v; }
	VmDynamicVariable(int32_t v) : type(VmType::Int32), arrayLength(1) { data.int32 = (uint32_t)v; }
	VmDynamicVariable(uint64_t v) : type(VmType::Int64), arrayLength(1) { data.int64 = v; }
	VmDynamicVariable(int64_t v) : type(VmType::Int64), arrayLength(1) { data.int64 = (uint64_t)v; }
	VmDynamicVariable(const int256& v) : type(VmType::Int256), arrayLength(1) { data.int256 = v.Unsigned(); }
	VmDynamicVariable(const uint256& v) : type(VmType::Int256), arrayLength(1) { data.int256 = v; }
	VmDynamicVariable(const Bytes16& v) : type(VmType::Bytes16), arrayLength(1) { data.bytes16 = v; }
	VmDynamicVariable(const Bytes32& v) : type(VmType::Bytes32), arrayLength(1) { data.bytes32 = v; }
	VmDynamicVariable(const Bytes64& v) : type(VmType::Bytes64), arrayLength(1) { data.bytes64 = v; }
	VmDynamicVariable(const char* v) : type(VmType::String), arrayLength(1) { data.string = v; }
	template<int N> VmDynamicVariable(const Bytes32(&v)[N]) : type(VmType::Array_Bytes32), arrayLength(N) { data.bytes32Array = v; }
	template<int N> VmDynamicVariable(const VmDynamicStruct(&v)[N]) : type(VmType::Array_Struct), arrayLength(N) { data.structureArray = { {}, v }; }
	VmDynamicVariable& operator=(VmDynamicVariable&&) = default;
	VmDynamicVariable(VmDynamicVariable&&) = default;

	bool IsNull() const { return type == VmType::Dynamic && !data.dynamic; }
};

struct VmNamedDynamicVariable
{
	SmallString name;
	VmDynamicVariable value;
};

// Forward declarations -------------------------------------------------------
inline bool Read(VmDynamicVariable& out, ReadView& reader, Allocator& alloc);
inline bool Read(VmType type, VmDynamicVariable& out, const VmStructSchema* schema, ReadView& reader, Allocator& alloc);
inline bool Read(VmDynamicVariable& out, const VmVariableSchema& schema, ReadView& reader, Allocator& alloc);
inline bool Read(VmDynamicStruct& out, const VmStructSchema& schema, ReadView& reader, Allocator& alloc);
inline bool Read(VmNamedDynamicVariable& out, ReadView& reader, Allocator& alloc);

inline bool Write(const VmDynamicVariable& in, const VmVariableSchema& schema, WriteView& writer);
inline bool Write(VmType type, const VmDynamicVariable& in, const VmStructSchema* schema, WriteView& writer);
inline void Write(const VmDynamicVariable& in, WriteView& writer);
inline bool Write(const VmDynamicStruct& in, const VmStructSchema& schema, WriteView& writer);
inline void Write(const VmNamedDynamicVariable& in, WriteView& writer);

// Helpers ---------------------------------------------------------------------

template<class T>
inline void WriteArray(uint32_t length, const T* items, WriteView& writer)
{
	Write4((int32_t)length, writer);
	for (uint32_t i = 0; i != length; ++i)
	{
		Write(items[i], writer);
	}
}

inline void WriteArray(uint32_t length, const ByteView* items, WriteView& writer)
{
	Write4((int32_t)length, writer);
	for (uint32_t i = 0; i != length; ++i)
	{
		WriteArray(items[i], writer);
	}
}

inline void WriteArraySz(uint32_t length, const char* const* items, WriteView& writer)
{
	Write4((int32_t)length, writer);
	for (uint32_t i = 0; i != length; ++i)
	{
		WriteSz(items[i] ? std::string(items[i]) : std::string(), writer);
	}
}

inline void WriteArray(uint32_t length, const uint8_t* items, WriteView& writer)
{
	Write4((int32_t)length, writer);
	for (uint32_t i = 0; i != length; ++i) Write1(items[i], writer);
}
inline void WriteArray(uint32_t length, const uint16_t* items, WriteView& writer)
{
	Write4((int32_t)length, writer);
	for (uint32_t i = 0; i != length; ++i) Write2((int16_t)items[i], writer);
}
inline void WriteArray(uint32_t length, const uint32_t* items, WriteView& writer)
{
	Write4((int32_t)length, writer);
	for (uint32_t i = 0; i != length; ++i) Write4((int32_t)items[i], writer);
}
inline void WriteArray(uint32_t length, const uint64_t* items, WriteView& writer)
{
	Write4((int32_t)length, writer);
	for (uint32_t i = 0; i != length; ++i) Write8u(items[i], writer);
}
inline void WriteArray(uint32_t length, const int256* items, WriteView& writer)
{
	Write4((int32_t)length, writer);
	for (uint32_t i = 0; i != length; ++i) WriteInt256(items[i], writer);
}
inline void WriteArray(uint32_t length, const Bytes16* items, WriteView& writer)
{
	Write4((int32_t)length, writer);
	for (uint32_t i = 0; i != length; ++i) WriteExactly((const Byte*)items[i].bytes, Bytes16::length, writer);
}
inline void WriteArray(uint32_t length, const Bytes32* items, WriteView& writer)
{
	Write4((int32_t)length, writer);
	for (uint32_t i = 0; i != length; ++i) WriteExactly((const Byte*)items[i].bytes, Bytes32::length, writer);
}
inline void WriteArray(uint32_t length, const Bytes64* items, WriteView& writer)
{
	Write4((int32_t)length, writer);
	for (uint32_t i = 0; i != length; ++i) WriteExactly((const Byte*)items[i].bytes, Bytes64::length, writer);
}

inline bool ReadArray(ByteView& out, ReadView& reader, Allocator& alloc)
{
	const int32_t len = Read4(reader);
	if (len < 0)
	{
		return false;
	}
	if (len == 0)
	{
		out = {};
		return true;
	}
	ByteArray bytes = ReadExactly((size_t)len, reader);
	Byte* storage = alloc.Clone(bytes.data(), bytes.size());
	out = ByteView{ storage, bytes.size() };
	return true;
}

inline bool ReadArray(uint32_t& length, ByteView*& items, ReadView& reader, Allocator& alloc)
{
	const int32_t len = Read4(reader);
	if (len < 0)
	{
		return false;
	}
	length = (uint32_t)len;
	if (length == 0)
	{
		items = nullptr;
		return true;
	}
	ByteView* arr = alloc.Alloc<ByteView>(length);
	items = arr;
	for (uint32_t i = 0; i != length; ++i)
	{
		if (!ReadArray(arr[i], reader, alloc))
		{
			return false;
		}
	}
	return true;
}

template<class T, class ReaderFunc>
inline bool ReadArray(uint32_t& length, T*& items, ReadView& reader, Allocator& alloc, ReaderFunc fn)
{
	const int32_t len = Read4(reader);
	if (len < 0)
	{
		return false;
	}
	length = (uint32_t)len;
	if (length == 0)
	{
		items = nullptr;
		return true;
	}
	T* arr = alloc.Alloc<T>(length);
	items = arr;
	for (uint32_t i = 0; i != length; ++i)
	{
		if (!fn(arr[i], reader))
		{
			return false;
		}
	}
	return true;
}

inline bool ReadSz(const char*& out, ReadView& reader, Allocator& alloc)
{
	std::string s = ReadSz(reader);
	const size_t len = s.size();
	char* dst = alloc.Alloc<char>(len + 1);
	memcpy(dst, s.c_str(), len + 1);
	out = dst;
	return true;
}

inline bool ReadArraySz(uint32_t& length, const char**& items, ReadView& reader, Allocator& alloc)
{
	const int32_t len = Read4(reader);
	if (len < 0)
	{
		return false;
	}
	length = (uint32_t)len;
	if (length == 0)
	{
		items = nullptr;
		return true;
	}
	const char** arr = alloc.Alloc<const char*>(length);
	items = arr;
	for (uint32_t i = 0; i != length; ++i)
	{
		if (!ReadSz(arr[i], reader, alloc))
		{
			return false;
		}
	}
	return true;
}

// Serialization --------------------------------------------------------------

inline bool Read(VmType& out, ReadView& reader)
{
	out = (VmType)Read1(reader);
	return true;
}
inline void Write(VmType in, WriteView& writer) { Write1((uint8_t)in, writer); }

inline bool Read(VmNamedVariableSchema& out, ReadView& reader, Allocator& alloc);
inline void Write(const VmNamedVariableSchema& in, WriteView& writer);
inline bool Read(VmNamedDynamicVariable& out, ReadView& reader, Allocator& alloc);
inline void Write(const VmNamedDynamicVariable& in, WriteView& writer);

inline bool Read(VmStructSchema& out, ReadView& reader, Allocator& alloc)
{
	const int32_t len = Read4(reader);
	if (len < 0)
	{
		return false;
	}
	out.numFields = (uint32_t)len;
	out.fields = out.numFields ? alloc.Alloc<VmNamedVariableSchema>(out.numFields) : nullptr;
	for (uint32_t i = 0; i != out.numFields; ++i)
	{
		if (!Read(const_cast<VmNamedVariableSchema&>(out.fields[i]), reader, alloc))
		{
			return false;
		}
	}
	out.flags = (VmStructSchema::SchemaFlags)Read1(reader);
	return true;
}

inline void Write(const VmStructSchema& in, WriteView& writer)
{
	Write4((int32_t)in.numFields, writer);
	for (uint32_t i = 0; i != in.numFields; ++i)
	{
		Write(in.fields[i], writer);
	}
	Write1((uint8_t)in.flags, writer);
}

inline bool Read(VmVariableSchema& out, ReadView& reader, Allocator& alloc)
{
	if (!Read(out.type, reader))
	{
		return false;
	}
	if (out.type == VmType::Struct || out.type == VmType::Array_Struct)
	{
		return Read(out.structure, reader, alloc);
	}
	return true;
}

inline void Write(const VmVariableSchema& in, WriteView& writer)
{
	Write(in.type, writer);
	if (in.type == VmType::Struct || in.type == VmType::Array_Struct)
	{
		Write(in.structure, writer);
	}
}

inline bool Read(VmNamedVariableSchema& out, ReadView& reader, Allocator& alloc)
{
	Read(out.name, reader);
	return Read(out.schema, reader, alloc);
}

inline void Write(const VmNamedVariableSchema& in, WriteView& writer)
{
	Write(in.name, writer);
	Write(in.schema, writer);
}

inline bool Read(VmDynamicVariable& out, ReadView& reader, Allocator& alloc);
inline bool Read(VmDynamicStruct& out, ReadView& reader, Allocator& alloc)
{
	const int32_t len = Read4(reader);
	if (len < 0)
	{
		return false;
	}
	out.numFields = (uint32_t)len;
	out.fields = out.numFields ? alloc.Alloc<VmNamedDynamicVariable>(out.numFields) : nullptr;
	for (uint32_t i = 0; i != out.numFields; ++i)
	{
		if (!Read(out.fields[i], reader, alloc))
		{
			return false;
		}
	}
	if (out.numFields > 1)
	{
		std::sort(out.fields, out.fields + out.numFields, NameLessThan<VmNamedDynamicVariable>{});
	}
	return true;
}

inline void Write(const VmNamedDynamicVariable& in, WriteView& writer)
{
	Write(in.name, writer);
	Write(in.value, writer);
}

inline void Write(const VmDynamicStruct& in, WriteView& writer)
{
	Write4((int32_t)in.numFields, writer);
	if (in.numFields == 0 || !in.fields)
	{
		return;
	}
	if (in.numFields > 1 && !VmDynamicStruct::IsSorted(in.numFields, in.fields))
	{
		std::vector<VmNamedDynamicVariable> sorted(in.fields, in.fields + in.numFields);
		std::sort(sorted.begin(), sorted.end(), NameLessThan<VmNamedDynamicVariable>{});
		for (const auto& f : sorted)
		{
			Write(f, writer);
		}
		return;
	}
	for (uint32_t i = 0; i != in.numFields; ++i)
	{
		Write(in.fields[i], writer);
	}
}

inline bool Write(const VmDynamicStruct& in, const VmStructSchema& schema, WriteView& writer)
{
	bool ok = true;
	uint32_t fieldsFound = 0;
	for (uint32_t i = 0; i != schema.numFields; ++i)
	{
		const SmallString& name = schema.fields[i].name;
		const VmDynamicVariable* value = in[name];
		if (value)
		{
			Write(*value, schema.fields[i].schema, writer);
			++fieldsFound;
		}
		else
		{
			VmDynamicVariable defaultValue;
			memset(&defaultValue, 0, sizeof(defaultValue));
			defaultValue.type = schema.fields[i].schema.type;
			Write(defaultValue, schema.fields[i].schema, writer);
			ok = false;
		}
	}
	if (0 == (schema.flags & VmStructSchema::Flag_DynamicExtras))
	{
		return ok;
	}
	if (fieldsFound == schema.numFields && schema.numFields == in.numFields)
	{
		Write4(0, writer);
		return ok;
	}
	std::vector<VmNamedDynamicVariable> extras;
	for (uint32_t i = 0; i != in.numFields; ++i)
	{
		const auto it = std::find_if(schema.fields, schema.fields + schema.numFields, [&](const VmNamedVariableSchema& s) { return s.name == in.fields[i].name; });
		if (it == schema.fields + schema.numFields)
		{
			extras.push_back(in.fields[i]);
		}
	}
	Write4((int32_t)extras.size(), writer);
	for (const auto& e : extras)
	{
		Write(e, writer);
	}
	return ok;
}

inline bool Read(VmDynamicVariable& out, ReadView& reader, Allocator& alloc)
{
	if (!Read(out.type, reader))
	{
		return false;
	}
	return Read(out.type, out, nullptr, reader, alloc);
}

inline void Write(const VmDynamicVariable& in, WriteView& writer)
{
	Write(in.type, writer);
	Write(in.type, in, nullptr, writer);
}

inline const VmNamedVariableSchema* VmStructSchema::operator[](const SmallString& name) const
{
	if (flags & Flag_IsSorted)
	{
		VmNamedVariableSchema dummy{ name };
		auto it = std::lower_bound(fields, fields + numFields, dummy, NameLessThan<VmNamedVariableSchema>{});
		if (it != fields + numFields && it->name == name)
		{
			return it;
		}
	}
	else
	{
		for (uint32_t i = 0; i != numFields; ++i)
		{
			if (fields[i].name == name)
			{
				return &fields[i];
			}
		}
	}
	return nullptr;
}

inline bool VmStructSchema::IsSorted(uint32_t num, VmNamedVariableSchema* f)
{
	for (uint32_t i = 1; i < num; ++i)
	{
		if (f[i - 1].name > f[i].name)
		{
			return false;
		}
	}
	return true;
}

inline VmStructSchema VmStructSchema::Sort(uint32_t num, VmNamedVariableSchema* f, bool allowDynamicExtras)
{
	if (num > 1 && !IsSorted(num, f))
	{
		std::sort(f, f + num, NameLessThan<VmNamedVariableSchema>{});
	}
	VmStructSchema s;
	s.numFields = num;
	s.fields = f;
	s.flags = (SchemaFlags)(Flag_IsSorted | (allowDynamicExtras ? Flag_DynamicExtras : Flag_None));
	return s;
}

inline const VmDynamicVariable* VmDynamicStruct::operator[](const SmallString& name) const
{
	VmNamedDynamicVariable dummy{ name };
	const VmNamedDynamicVariable* lower = std::lower_bound(fields, fields + numFields, dummy, NameLessThan<VmNamedDynamicVariable>{});
	if (lower != fields + numFields && lower->name == name)
	{
		return &lower->value;
	}
	return nullptr;
}
inline VmDynamicVariable* VmDynamicStruct::operator[](const SmallString& name)
{
	VmNamedDynamicVariable dummy{ name };
	VmNamedDynamicVariable* lower = std::lower_bound(fields, fields + numFields, dummy, NameLessThan<VmNamedDynamicVariable>{});
	if (lower != fields + numFields && lower->name == name)
	{
		return &lower->value;
	}
	return nullptr;
}

inline void VmDynamicStruct::Erase(const VmDynamicVariable* p)
{
	const VmNamedDynamicVariable* item = (VmNamedDynamicVariable*)(((const char*)p) - offsetof(VmNamedDynamicVariable, value));
	ptrdiff_t index = item - fields;
	for (++index; index < (ptrdiff_t)numFields; ++index)
	{
		fields[index - 1] = fields[index];
	}
	numFields--;
}

inline VmDynamicStruct VmDynamicStruct::Sort(uint32_t num, VmNamedDynamicVariable* f)
{
	if (num > 1 && !IsSorted(num, f))
	{
		std::sort(f, f + num, NameLessThan<VmNamedDynamicVariable>{});
	}
	VmDynamicStruct s;
	s.numFields = num;
	s.fields = f;
	return s;
}

inline bool VmDynamicStruct::IsSorted(uint32_t num, VmNamedDynamicVariable* f)
{
	for (uint32_t i = 1; i < num; ++i)
	{
		if (f[i - 1].name > f[i].name)
		{
			return false;
		}
	}
	return true;
}

inline VmDynamicStruct VmDynamicStruct::Merge(const VmDynamicStruct& old, const VmDynamicStruct& updates, Allocator& a)
{
	uint32_t numFields = 0;
	VmNamedDynamicVariable* fields = a.Alloc<VmNamedDynamicVariable>(old.numFields + updates.numFields);

	for (uint32_t i = 0; i != old.numFields; ++i)
	{
		const VmDynamicVariable* replacement = updates[old.fields[i].name];
		if (!replacement)
		{
			fields[numFields++] = old.fields[i];
		}
	}
	for (uint32_t i = 0; i != updates.numFields; ++i)
	{
		if (!updates.fields[i].value.IsNull())
		{
			fields[numFields++] = updates.fields[i];
		}
	}
	return VmDynamicStruct::Sort(numFields, fields);
}

inline VmDynamicVariable::VmDynamicVariable(const VmDynamicVariable& o)
{
	memcpy(this, &o, sizeof(VmDynamicVariable));
}
inline VmDynamicVariable& VmDynamicVariable::operator=(const VmDynamicVariable& o)
{
	if (this != &o)
	{
		memcpy(this, &o, sizeof(VmDynamicVariable));
	}
	return *this;
}

inline bool Read(VmDynamicVariable& out, const VmVariableSchema& schema, ReadView& reader, Allocator& alloc)
{
	out.type = schema.type;
	return Read(schema.type, out, schema.structure.numFields ? &schema.structure : nullptr, reader, alloc);
}

inline bool Write(const VmDynamicVariable& in, const VmVariableSchema& schema, WriteView& writer)
{
	return Write(schema.type, in, schema.structure.numFields ? &schema.structure : nullptr, writer);
}

inline bool Read(VmType type, VmDynamicVariable& out, const VmStructSchema* schema, ReadView& reader, Allocator& alloc)
{
	out.type = type;
	switch (type)
	{
	case VmType::Dynamic:
		out.data.dynamic = alloc.Alloc<VmDynamicVariable>(1);
		return Read(*out.data.dynamic, reader, alloc);
	case VmType::Bytes:
		return ReadArray(out.data.bytes, reader, alloc);
	case VmType::Struct:
		if (schema)
		{
			return Read(out.data.structure, *schema, reader, alloc);
		}
		return Read(out.data.structure, reader, alloc);
	case VmType::Int8: out.data.int8 = Read1(reader); return true;
	case VmType::Int16: out.data.int16 = (uint16_t)Read2(reader); return true;
	case VmType::Int32: out.data.int32 = (uint32_t)Read4(reader); return true;
	case VmType::Int64: out.data.int64 = Read8u(reader); return true;
	case VmType::Int256:
	{
		int256 temp;
		Throw::If(!Read(temp, reader), "invalid int256");
		out.data.int256 = temp.Unsigned();
		return true;
	}
	case VmType::Bytes16: Throw::If(!reader.ReadBytes(out.data.bytes16.bytes, Bytes16::length), "end of stream reached"); return true;
	case VmType::Bytes32: Throw::If(!reader.ReadBytes(out.data.bytes32.bytes, Bytes32::length), "end of stream reached"); return true;
	case VmType::Bytes64: Throw::If(!reader.ReadBytes(out.data.bytes64.bytes, Bytes64::length), "end of stream reached"); return true;
	case VmType::String: return ReadSz(out.data.string, reader, alloc);
	default:
		break;
	}

	const uint8_t rawType = (uint8_t)type;
	if ((rawType & (uint8_t)VmType::Array) == 0)
	{
		return false;
	}
	switch (rawType & ~((uint8_t)VmType::Array))
	{
	case (uint8_t)VmType::Dynamic:
		return ReadArray(out.arrayLength, out.data.dynamicArray, reader, alloc, [&](VmDynamicVariable& v, ReadView& r) { return Read(v, r, alloc); });
	case (uint8_t)VmType::Bytes:
		return ReadArray(out.arrayLength, out.data.bytesArray, reader, alloc);
	case (uint8_t)VmType::Struct:
	{
		const int32_t len = Read4(reader);
		if (len < 0)
		{
			return false;
		}
		out.arrayLength = (uint32_t)len;
		if (out.arrayLength == 0)
		{
			out.data.structureArray.structs = nullptr;
			return true;
		}
		VmStructSchema readSchema;
		const VmStructSchema* schemaPtr = schema;
		if (!schema)
		{
			if (!Read(readSchema, reader, alloc))
			{
				return false;
			}
			schemaPtr = &readSchema;
		}
		VmDynamicStruct* structs = alloc.Alloc<VmDynamicStruct>(out.arrayLength);
		out.data.structureArray.schema = schemaPtr ? *schemaPtr : VmStructSchema{};
		out.data.structureArray.structs = structs;
		for (uint32_t i = 0; i != out.arrayLength; ++i)
		{
			if (schemaPtr)
			{
				if (!Read(structs[i], *schemaPtr, reader, alloc))
				{
					return false;
				}
			}
			else if (!Read(structs[i], reader, alloc))
			{
				return false;
			}
		}
		return true;
	}
	case (uint8_t)VmType::Int8:
		return ReadArray(out.arrayLength, out.data.int8Array, reader, alloc, [](uint8_t& v, ReadView& r) { v = Read1(r); return true; });
	case (uint8_t)VmType::Int16:
		return ReadArray(out.arrayLength, out.data.int16Array, reader, alloc, [](uint16_t& v, ReadView& r) { v = (uint16_t)Read2(r); return true; });
	case (uint8_t)VmType::Int32:
		return ReadArray(out.arrayLength, out.data.int32Array, reader, alloc, [](uint32_t& v, ReadView& r) { v = (uint32_t)Read4(r); return true; });
	case (uint8_t)VmType::Int64:
		return ReadArray(out.arrayLength, out.data.int64Array, reader, alloc, [](uint64_t& v, ReadView& r) { v = Read8u(r); return true; });
	case (uint8_t)VmType::Int256:
		return ReadArray(out.arrayLength, out.data.int256Array, reader, alloc, [](uint256& v, ReadView& r) { int256 temp; bool ok = Read(temp, r); v = temp.Unsigned(); return ok; });
	case (uint8_t)VmType::Bytes16:
		return ReadArray(out.arrayLength, out.data.bytes16Array, reader, alloc, [](Bytes16& v, ReadView& r) { return r.ReadBytes(v.bytes, Bytes16::length); });
	case (uint8_t)VmType::Bytes32:
		return ReadArray(out.arrayLength, out.data.bytes32Array, reader, alloc, [](Bytes32& v, ReadView& r) { return r.ReadBytes(v.bytes, Bytes32::length); });
	case (uint8_t)VmType::Bytes64:
		return ReadArray(out.arrayLength, out.data.bytes64Array, reader, alloc, [](Bytes64& v, ReadView& r) { return r.ReadBytes(v.bytes, Bytes64::length); });
	case (uint8_t)VmType::String:
		return ReadArraySz(out.arrayLength, out.data.stringArray, reader, alloc);
	default:
		return false;
	}
}

inline bool Write(VmType type, const VmDynamicVariable& in, const VmStructSchema* schema, WriteView& writer)
{
	if (in.type != type)
	{
		VmDynamicVariable error;
		memset(&error, 0, sizeof(VmDynamicVariable));
		error.type = type;
		Write(type, error, schema, writer);
		return false;
	}

	switch (type)
	{
	case VmType::Dynamic:
		if (!in.data.dynamic)
		{
			Write((VmType)(VmType::Array | VmType::Dynamic), writer);
			WriteArray(0, in.data.dynamicArray, writer);
			return true;
		}
		Write(*in.data.dynamic, writer);
		return true;
	case VmType::Bytes:
		WriteArray(in.data.bytes, writer);
		return true;
	case VmType::Struct:
		if (schema)
		{
			return Write(in.data.structure, *schema, writer);
		}
		Write(in.data.structure, writer);
		return true;
	case VmType::Int8: Write1(in.data.int8, writer); return true;
	case VmType::Int16: Write2((int16_t)in.data.int16, writer); return true;
	case VmType::Int32: Write4((int32_t)in.data.int32, writer); return true;
	case VmType::Int64: Write8((int64_t)in.data.int64, writer); return true;
	case VmType::Int256: WriteInt256(in.data.int256.Signed(), writer); return true;
	case VmType::Bytes16: WriteExactly((const Byte*)in.data.bytes16.bytes, Bytes16::length, writer); return true;
	case VmType::Bytes32: WriteExactly((const Byte*)in.data.bytes32.bytes, Bytes32::length, writer); return true;
	case VmType::Bytes64: WriteExactly((const Byte*)in.data.bytes64.bytes, Bytes64::length, writer); return true;
	case VmType::String: WriteSz(in.data.string ? std::string(in.data.string) : std::string(), writer); return true;
	default:
		break;
	}

	const uint8_t rawType = (uint8_t)type;
	if ((rawType & (uint8_t)VmType::Array) == 0)
	{
		return false;
	}
	switch (rawType & ~((uint8_t)VmType::Array))
	{
	case (uint8_t)VmType::Dynamic:
		WriteArray(in.arrayLength, in.data.dynamicArray, writer);
		return true;
	case (uint8_t)VmType::Bytes:
		WriteArray(in.arrayLength, in.data.bytesArray, writer);
		return true;
	case (uint8_t)VmType::Struct:
	{
		Write4((int32_t)in.arrayLength, writer);
		const VmStructSchema* schemaPtr = schema;
		if (!schemaPtr && in.data.structureArray.schema.numFields)
		{
			schemaPtr = &in.data.structureArray.schema;
		}
		if (!schema && schemaPtr)
		{
			Write(*schemaPtr, writer);
		}
		for (uint32_t i = 0; i != in.arrayLength; ++i)
		{
			if (schemaPtr)
			{
				Write(in.data.structureArray.structs[i], *schemaPtr, writer);
			}
			else
			{
				Write(in.data.structureArray.structs[i], writer);
			}
		}
		return true;
	}
	case (uint8_t)VmType::Int8:
		WriteArray(in.arrayLength, in.data.int8Array, writer);
		return true;
	case (uint8_t)VmType::Int16:
		WriteArray(in.arrayLength, in.data.int16Array, writer);
		return true;
	case (uint8_t)VmType::Int32:
		WriteArray(in.arrayLength, in.data.int32Array, writer);
		return true;
	case (uint8_t)VmType::Int64:
		WriteArray(in.arrayLength, in.data.int64Array, writer);
		return true;
	case (uint8_t)VmType::Int256:
		WriteArray(in.arrayLength, in.data.int256Array, writer);
		return true;
	case (uint8_t)VmType::Bytes16:
		WriteArray(in.arrayLength, in.data.bytes16Array, writer);
		return true;
	case (uint8_t)VmType::Bytes32:
		WriteArray(in.arrayLength, in.data.bytes32Array, writer);
		return true;
	case (uint8_t)VmType::Bytes64:
		WriteArray(in.arrayLength, in.data.bytes64Array, writer);
		return true;
	case (uint8_t)VmType::String:
		WriteArraySz(in.arrayLength, in.data.stringArray, writer);
		return true;
	default:
		return false;
	}
}

inline bool Read(VmDynamicStruct& out, const VmStructSchema& schema, ReadView& reader, Allocator& alloc)
{
	if (schema.numFields == 0)
	{
		out.numFields = 0;
		out.fields = nullptr;
		return true;
	}
	out.numFields = schema.numFields;
	out.fields = alloc.Alloc<VmNamedDynamicVariable>(schema.numFields);
	for (uint32_t i = 0; i != schema.numFields; ++i)
	{
		out.fields[i].name = schema.fields[i].name;
		if (!Read(out.fields[i].value, schema.fields[i].schema, reader, alloc))
		{
			return false;
		}
	}
	if (0 == (schema.flags & VmStructSchema::Flag_DynamicExtras))
	{
		if (0 == (schema.flags & VmStructSchema::Flag_IsSorted) && out.numFields > 1)
		{
			std::sort(out.fields, out.fields + out.numFields, NameLessThan<VmNamedDynamicVariable>{});
		}
		return true;
	}
	const int32_t extrasLen = Read4(reader);
	if (extrasLen < 0)
	{
		return false;
	}
	const uint32_t extrasLength = (uint32_t)extrasLen;
	if (extrasLength == 0)
	{
		if (out.numFields > 1)
		{
			std::sort(out.fields, out.fields + out.numFields, NameLessThan<VmNamedDynamicVariable>{});
		}
		return true;
	}
	VmNamedDynamicVariable* combined = alloc.Alloc<VmNamedDynamicVariable>(schema.numFields + extrasLength);
	memcpy(combined, out.fields, sizeof(VmNamedDynamicVariable) * schema.numFields);
	for (uint32_t i = 0; i != extrasLength; ++i)
	{
		if (!Read(combined[schema.numFields + i], reader, alloc))
		{
			return false;
		}
	}
	out.fields = combined;
	out.numFields = schema.numFields + extrasLength;
	if (out.numFields > 1)
	{
		std::sort(out.fields, out.fields + out.numFields, NameLessThan<VmNamedDynamicVariable>{});
	}
	return true;
}

inline bool Read(VmNamedDynamicVariable& out, ReadView& reader, Allocator& alloc)
{
	Read(out.name, reader);
	return Read(out.value, reader, alloc);
}

inline bool Read(VmDynamicVariable& out, const VmStructSchema& schema, ReadView& reader, Allocator& alloc)
{
	out.type = VmType::Struct;
	out.data.structure = {};
	return Read(out.data.structure, schema, reader, alloc);
}
} // namespace phantasma::carbon
