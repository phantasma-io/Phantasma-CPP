#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

#include "Alloc.h"
#include "DataCommon.h"
#include "Int256.h"

namespace phantasma::carbon {

struct Throw
{
	static void If(bool condition, const char* message)
	{
		if (condition)
		{
			PHANTASMA_EXCEPTION(message);
		}
	}

	static void Assert(bool condition, const char* message = "assertion failed")
	{
		If(!condition, message);
	}
};

inline uint32_t CarbonComputeSerializedLength(const uint8_t* bytes, uint32_t size, uint8_t fill)
{
	uint32_t length = size;
	while (length > 1 && bytes[length - 1] == fill && ((bytes[length - 2] & 0x80) == (fill & 0x80)))
	{
		--length;
	}
	return length;
}

//------------------------------------------------------------------------------
// Write helpers (WriteView based)
//------------------------------------------------------------------------------
inline void WriteBytes(const Byte* data, size_t count, WriteView& w)
{
	if (count == 0)
	{
		return;
	}
	Throw::If(data == nullptr, "invalid input data");
	w.WriteBytes(data, count);
}
inline void WriteBytes(ByteView v, WriteView& w) { WriteBytes(v.bytes, v.length, w); }

inline void Write1(uint8_t v, WriteView& w) { w.WriteByte((Byte)v); }
inline void Write2(int16_t v, WriteView& w) { w.WriteBytes((const uint8_t*)&v, sizeof(v)); }
inline void Write4u(uint32_t v, WriteView& w) { Write4((int32_t)v, w); }
inline void Write8(int64_t v, WriteView& w) { w.WriteBytes((const uint8_t*)&v, sizeof(v)); }
inline void Write8u(uint64_t v, WriteView& w) { w.WriteBytes((const uint8_t*)&v, sizeof(v)); }

inline void WriteExactly(const Byte* data, size_t count, WriteView& w)
{
	Throw::If(data == nullptr && count > 0, "invalid input data");
	WriteBytes(data, count, w);
}

inline void Write16(const Byte* data, size_t length, WriteView& w)
{
	Throw::If(length != 16, "incorrect input size");
	WriteExactly(data, length, w);
}
inline void Write32(const Byte* data, size_t length, WriteView& w)
{
	Throw::If(length != 32, "incorrect input size");
	WriteExactly(data, length, w);
}
inline void Write64(const Byte* data, size_t length, WriteView& w)
{
	Throw::If(length != 64, "incorrect input size");
	WriteExactly(data, length, w);
}

inline void WriteSz(const std::string& s, WriteView& w)
{
	for (unsigned char c : s)
	{
		Throw::Assert(c != 0, "string contains zero byte");
	}
	WriteBytes((const Byte*)s.data(), s.size(), w);
	Write1(0, w);
}

inline void Write(ByteView v, WriteView& w)
{
	Write4((int32_t)v.length, w);
	WriteBytes(v, w);
}

inline void WriteArrayOfArrays(const std::vector<ByteArray>& arr, WriteView& w)
{
	Write4((int32_t)arr.size(), w);
	for (const auto& a : arr)
	{
		WriteArray(a, w);
	}
}

inline void WriteArray64u(const std::vector<uint64_t>& arr, WriteView& w)
{
	Write4((int32_t)arr.size(), w);
	for (uint64_t v : arr)
	{
		Write8u(v, w);
	}
}

inline void WriteArray64(const std::vector<int64_t>& arr, WriteView& w)
{
	Write4((int32_t)arr.size(), w);
	for (int64_t v : arr)
	{
		Write8(v, w);
	}
}

inline void WriteArray32(const std::vector<int32_t>& arr, WriteView& w)
{
	Write4((int32_t)arr.size(), w);
	for (int32_t v : arr)
	{
		Write4(v, w);
	}
}

inline void WriteArray16(const std::vector<int16_t>& arr, WriteView& w)
{
	Write4((int32_t)arr.size(), w);
	for (int16_t v : arr)
	{
		Write2(v, w);
	}
}

inline void WriteArray8(const std::vector<int8_t>& arr, WriteView& w)
{
	Write4((int32_t)arr.size(), w);
	for (int8_t v : arr)
	{
		Write1((uint8_t)v, w);
	}
}

inline void WriteArraySz(const std::vector<std::string>& arr, WriteView& w)
{
	Write4((int32_t)arr.size(), w);
	for (const auto& s : arr)
	{
		WriteSz(s, w);
	}
}

//------------------------------------------------------------------------------
// Read helpers (ReadView based)
//------------------------------------------------------------------------------
inline bool OnNonStandardDataFound(ReadView& r) { return r.OnNonStandardDataFound(); }

inline uint8_t Read1(ReadView& r)
{
	uint8_t v = 0;
	Throw::If(!r.ReadBytes(v), "end of stream reached");
	return v;
}

inline int16_t Read2(ReadView& r)
{
	int16_t v = 0;
	Throw::If(!r.ReadBytes(v), "end of stream reached");
	return v;
}

inline int32_t Read4(ReadView& r)
{
	int32_t v = 0;
	Throw::If(!r.ReadBytes(v), "end of stream reached");
	return v;
}
inline uint32_t Read4u(ReadView& r) { return (uint32_t)Read4(r); }

inline int64_t Read8(ReadView& r)
{
	int64_t v = 0;
	Throw::If(!r.ReadBytes(v), "end of stream reached");
	return v;
}
inline uint64_t Read8u(ReadView& r) { return (uint64_t)Read8(r); }

inline ByteArray ReadExactly(size_t count, ReadView& r)
{
	ByteArray out;
	if (count == 0)
	{
		return out;
	}
	out.resize(count);
	Throw::If(!r.ReadBytes(&out.front(), count), "end of stream reached");
	return out;
}

inline std::string ReadSz(ReadView& r)
{
	std::string out;
	while (true)
	{
		Throw::If(r.length == 0, "end of stream reached");
		const Byte b = r.bytes[0];
		r.Advance(1);
		if (b == 0)
		{
			break;
		}
		out.push_back((char)b);
	}
	return out;
}

inline std::vector<std::string> ReadArraySz(ReadView& r)
{
	const auto len = Read4(r);
	Throw::If(len < 0, "invalid array length");
	std::vector<std::string> out;
	out.reserve((size_t)len);
	for (int i = 0; i < len; ++i)
	{
		out.push_back(ReadSz(r));
	}
	return out;
}

inline ByteArray ReadArray(ReadView& r)
{
	const auto len = Read4(r);
	Throw::If(len < 0, "invalid array length");
	return ReadExactly((size_t)len, r);
}

inline std::vector<ByteArray> ReadArrayOfArrays(ReadView& r)
{
	const auto len = Read4(r);
	Throw::If(len < 0, "invalid array length");
	std::vector<ByteArray> out;
	out.reserve((size_t)len);
	for (int i = 0; i < len; ++i)
	{
		out.push_back(ReadArray(r));
	}
	return out;
}

inline std::vector<uint64_t> ReadArray64u(ReadView& r)
{
	const auto len = Read4(r);
	Throw::If(len < 0, "invalid array length");
	std::vector<uint64_t> out;
	out.reserve((size_t)len);
	for (int i = 0; i < len; ++i)
	{
		out.push_back(Read8u(r));
	}
	return out;
}

inline std::vector<int64_t> ReadArray64(ReadView& r)
{
	const auto len = Read4(r);
	Throw::If(len < 0, "invalid array length");
	std::vector<int64_t> out;
	out.reserve((size_t)len);
	for (int i = 0; i < len; ++i)
	{
		out.push_back(Read8(r));
	}
	return out;
}

inline std::vector<int32_t> ReadArray32(ReadView& r)
{
	const auto len = Read4(r);
	Throw::If(len < 0, "invalid array length");
	std::vector<int32_t> out;
	out.reserve((size_t)len);
	for (int i = 0; i < len; ++i)
	{
		out.push_back(Read4(r));
	}
	return out;
}

inline std::vector<int16_t> ReadArray16(ReadView& r)
{
	const auto len = Read4(r);
	Throw::If(len < 0, "invalid array length");
	std::vector<int16_t> out;
	out.reserve((size_t)len);
	for (int i = 0; i < len; ++i)
	{
		out.push_back(Read2(r));
	}
	return out;
}

inline std::vector<int8_t> ReadArray8(ReadView& r)
{
	const auto len = Read4(r);
	Throw::If(len < 0, "invalid array length");
	std::vector<int8_t> out;
	out.reserve((size_t)len);
	for (int i = 0; i < len; ++i)
	{
		out.push_back((int8_t)Read1(r));
	}
	return out;
}

//------------------------------------------------------------------------------
// Serialization entry points
//------------------------------------------------------------------------------
template<class T>
ByteArray CarbonSerialize(const T& blob)
{
	ByteArray buffer;
	WriteView w(buffer);
	Write(blob, w);
	return buffer;
}

template<class T>
T CarbonNew(const ByteArray& bytes, size_t offset = 0)
{
	ReadView r(bytes.empty() ? nullptr : (void*)(&bytes.front() + offset), bytes.size() - offset);
	T v;
	Read(v, r);
	return v;
}

//------------------------------------------------------------------------------
// Primitive Write/Read
//------------------------------------------------------------------------------
inline void Write(const SmallString& s, WriteView& w)
{
	Write1(s.length, w);
	if (s.length)
	{
		WriteBytes((const Byte*)s.bytes, s.length, w);
	}
}
inline bool Read(SmallString& s, ReadView& r)
{
	const uint8_t len = Read1(r);
	Throw::If(len > 255, "SmallString was too long");
	s.length = len;
	if (len)
	{
		Throw::If(!r.ReadBytes(s.bytes, len), "end of stream reached");
	}
	s.bytes[len] = 0;
	return true;
}

template<int N, typename B>
inline void Write(const BytesN<N, B>& v, WriteView& w)
{
	WriteBytes((const Byte*)v.bytes, N, w);
}
template<int N, typename B>
inline bool Read(BytesN<N, B>& v, ReadView& r)
{
	return r.ReadBytes(v.bytes, N);
}

//------------------------------------------------------------------------------
// Primitive overloads and adapters
//------------------------------------------------------------------------------
inline void Write(uint8_t v, WriteView& w) { Write1(v, w); }
inline void Write(int8_t v, WriteView& w) { Write1((uint8_t)v, w); }
inline void Write(uint16_t v, WriteView& w) { Write2((int16_t)v, w); }
inline void Write(int16_t v, WriteView& w) { Write2(v, w); }
inline void Write(uint32_t v, WriteView& w) { Write4((int32_t)v, w); }
inline void Write(int32_t v, WriteView& w) { Write4(v, w); }
inline void Write(uint64_t v, WriteView& w) { Write8u(v, w); }
inline void Write(int64_t v, WriteView& w) { Write8(v, w); }
inline void Write(const Bytes16& v, WriteView& w) { WriteExactly((const Byte*)v.bytes, Bytes16::length, w); }
inline void Write(const Bytes32& v, WriteView& w) { WriteExactly((const Byte*)v.bytes, Bytes32::length, w); }
inline void Write(const Bytes64& v, WriteView& w) { WriteExactly((const Byte*)v.bytes, Bytes64::length, w); }
inline void Write(ByteView v, WriteView&& w) { Write(v, w); }

inline bool Read(uint8_t& out, ReadView& r) { out = Read1(r); return true; }
inline bool Read(int8_t& out, ReadView& r) { out = (int8_t)Read1(r); return true; }
inline bool Read(uint16_t& out, ReadView& r) { out = (uint16_t)Read2(r); return true; }
inline bool Read(int16_t& out, ReadView& r) { out = Read2(r); return true; }
inline bool Read(uint32_t& out, ReadView& r) { out = (uint32_t)Read4(r); return true; }
inline bool Read(int32_t& out, ReadView& r) { out = Read4(r); return true; }
inline bool Read(uint64_t& out, ReadView& r) { out = (uint64_t)Read8(r); return true; }
inline bool Read(int64_t& out, ReadView& r) { out = Read8(r); return true; }
inline bool Read(Bytes16& out, ReadView& r) { return r.ReadBytes(out.bytes, Bytes16::length); }
inline bool Read(Bytes32& out, ReadView& r) { return r.ReadBytes(out.bytes, Bytes32::length); }
inline bool Read(Bytes64& out, ReadView& r) { return r.ReadBytes(out.bytes, Bytes64::length); }
inline bool Read(ByteView& out, ReadView& r)
{
	const auto len = Read4(r);
	Throw::If(len < 0, "invalid array length");
	out = ByteView{ r.bytes, (size_t)len };
	return r.Advance((size_t)len);
}
inline bool Read(ByteArray& out, ReadView& r)
{
	out = ReadArray(r);
	return true;
}

inline void WriteInt256(const int256& v, WriteView& w) { Write(v, w); }
inline void WriteIntX(const intx& v, WriteView& w) { Write(v, w); }
inline int256 ReadInt256(ReadView& r) { int256 v; Throw::If(!Read(v, r), "invalid int256 packing"); return v; }
inline intx ReadIntX(ReadView& r) { intx v; Throw::If(!Read(v, r), "invalid intx packing"); return v; }
inline void WriteArrayInt256(const std::vector<int256>& items, WriteView& w)
{
	Write4((int32_t)items.size(), w);
	for (const auto& i : items)
	{
		Write(i, w);
	}
}
inline std::vector<int256> ReadArrayInt256(ReadView& r)
{
	const auto len = Read4(r);
	Throw::If(len < 0, "invalid array length");
	std::vector<int256> out;
	out.reserve((size_t)len);
	for (int32_t i = 0; i < len; ++i)
	{
		out.push_back(ReadInt256(r));
	}
	return out;
}

#include "Int256Impl.h"

} // namespace phantasma::carbon

#include "DataVm.h"
#include "Contracts/Token.h"
#include "DataBlockchain.h"
#include "Tx.h"
