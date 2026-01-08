#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif

#include "DataCommon.h"

#include <cctype>
#include <inttypes.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

#define CARBON_MEMORY_CLEAR           1 // Fill all allocation with fixed bitpatterns before and after use
#define CARBON_MEMORY_BULK_ALLOCATION 1 // Pre-allocate larger buffers so multiple objects can share each allocation
#define CARBON_MEMORY_STD_ALLOCATOR   1 // Track allocations that are performed by the standard library

#define CARBON_MEMORY_DBG(x) x
#define CARBON_MEMORY_DBGC(x) ,x

namespace phantasma::carbon {

class Allocator;
class ReadView;
class WriteView;

// For internal use
inline void LogAlloc(size_t size, const char* tag, const void* owner);
inline void LogMove(size_t size, const char* tag, const void* ownerOld, const void* ownerNew);
inline void LogFree(size_t size, const char* tag, const void* owner);
inline bool LogLeaks();

template <typename T>
struct StdAllocator
{
	typedef T        value_type;
	typedef T*       pointer;
	typedef const T* const_pointer;
	typedef T&       reference;
	typedef const T& const_reference;
	typedef std::size_t    size_type;
	typedef std::ptrdiff_t difference_type;
	template<class Y> struct rebind { typedef StdAllocator<Y> other; };
	pointer address (reference value) const { return &value; }
	const_pointer address (const_reference value) const { return &value; }

	StdAllocator() = default;
	template<class Y> StdAllocator(const StdAllocator<Y>&) {}

	template<class Y> bool operator!=(const StdAllocator<Y>&) const { return false; }
	template<class Y> bool operator==(const StdAllocator<Y>&) const { return true; }

	T* allocate( std::size_t n )
	{
		T* p = static_cast<T*>(::operator new(n * sizeof(T)));
		LogAlloc(n * sizeof(T), typeid(T).name(), p);
		return p;
	}
	void deallocate( T* p, std::size_t n )
	{
		LogFree(n * sizeof(T), typeid(T).name(), p);
		::operator delete(p);
	}
};

#ifdef PHANTASMA_VECTOR
template<class T>
using vector = PHANTASMA_VECTOR<T>;
#elif CARBON_MEMORY_STD_ALLOCATOR
template<class T>
using vector = std::vector<T, StdAllocator<T>>;
#else
template<class T>
using vector = std::vector<T>;
#endif

typedef vector<uint8_t> Bytes;

inline void CryptoRandomBuffer(uint8_t* buf, size_t size)
{
	if (size)
	{
#if CARBON_HAS_SODIUM_IMPL
		CarbonEnsureSodiumInit();
		randombytes_buf(buf, size);
#else
		(void)buf;
		CarbonAssert(false, "libsodium not available");
#endif
	}
}
inline void CryptoRandomBuffer(Bytes& b) { if(!b.empty()) CryptoRandomBuffer(&b.front(), b.size()); }
template<class Random> void RandomBuffer( Bytes& b, Random &r ) { if(!b.empty()) r.Buffer(&b.front(), b.size()); }

inline ByteView View(const Bytes& b) { return b.empty() ? ByteView{} : ByteView{ &b.front(), b.size() }; }
inline Bytes Clone(ByteView data) { Bytes b; if(data.length) { b.resize(data.length); memcpy(&b.front(), data.bytes, data.length); } return b; }
inline void Append(Bytes& b, ByteView data) { if(data.length) { auto s = b.size(); b.resize(s + data.length); memcpy(s + &b.front(), data.bytes, data.length); } }
template<class T> void Clone(T& output, const T& input, Allocator& a);

ByteView Compress( ByteView input, Bytes& buffer, int compressionLevel = 3 );
ByteView Decompress( ByteView input, Bytes& buffer );

const char* ToHex(ByteView k, Allocator& c);
const char* ToHex(ByteView k, vector<char>& c);
ByteView  FromHex(const char* sz, Allocator& c);
ByteView  FromHex(const char* sz, Bytes& c);

const char* ToHexBe(ByteView k, Allocator& c);
const char* ToHexBe(ByteView k, vector<char>& c);
ByteView  FromHexBe(const char* sz, Bytes& c);

char* VFormat( vector<char>& a, const char* fmt, va_list& v );
char* VFormat( Allocator& a, const char* fmt, va_list& v );
char* Format( Allocator& a, const char* fmt, ... );

// Helper for storing Bytes as a key in a map, but being able to do map-lookups using ByteViews
class BytesKey // holds either a ByteView or a Bytes. When copied, result is always Bytes.
{
	ByteView m_view;
	Bytes m_buffer;
	void operator=(const BytesKey&);
public:
	BytesKey() { m_view = {}; }
	explicit BytesKey(const ByteView& o) : m_view(o) {}
	explicit BytesKey(const Bytes& o) : m_buffer(o) {}
	BytesKey(const BytesKey& o) : m_buffer(o.m_buffer)
	{
		if(m_buffer.empty() && o.m_view.length)
		{
			m_buffer.resize(o.m_view.length);
			memcpy(&m_buffer.front(), o.m_view.bytes, o.m_view.length);
		}
	}
	BytesKey(BytesKey&& o) : m_buffer(o.m_buffer)
	{
		if(m_buffer.empty() && o.m_view.length)
		{
			m_buffer.resize(o.m_view.length);
			memcpy(&m_buffer.front(), o.m_view.bytes, o.m_view.length);
		}
	}
	bool operator==(const BytesKey& o) const { return (ByteView)(*this) == (ByteView)o; }
	bool operator< (const BytesKey& o) const { return (ByteView)(*this) <  (ByteView)o; }
	explicit operator ByteView() const { return m_buffer.empty() ? m_view : ByteView{ &m_buffer.front(), m_buffer.size() }; }
	explicit operator Bytes() const { return m_buffer.empty() ? Bytes(m_view.bytes, m_view.bytes + m_view.length) : m_buffer; }
};

struct BytesHash // for use with unordered_map, etc
{
	size_t operator()(const Bytes& s) const
	{
		return s.empty() ? 0 : (size_t)SipHash24_Random({&s.front(), s.size()});
	}
	size_t operator()(const ByteView& s) const
	{
		return s.length==0 ? 0 : (size_t)SipHash24_Random(s);
	}
	size_t operator()(const HashedByteView& s) const
	{
		return (size_t)s.hash;
	}
	size_t operator()(const BytesKey& s) const
	{
		return (*this)((ByteView)s);
	}
};
struct SmallStringHash // for use with SmallString
{
	size_t operator()(const SmallString& s) const
	{
		return !s.length ? 0 : (size_t)SipHash24_Random({(uint8_t*)s.bytes, s.length});
	}
};

struct AllocLog
{
	std::recursive_mutex m;

	struct Usage { size_t usage = 0; size_t peak = 0; size_t count = 0; };
	struct Owner { size_t size; SmallString tag; };
	std::unordered_map<SmallString, Usage, SmallStringHash> usage;
#ifdef DEV
	std::unordered_map<const void*, Owner> owners;
#endif
};

inline AllocLog& GetAllocLog()
{
	static AllocLog m;
	return m;
}

// Simple arena implementation. A linked-list of large stacks of bytes
class Allocator
{
	CARBON_MEMORY_DBG(const char* tag = "?");
public:
	explicit Allocator(const char* tag = "?") CARBON_MEMORY_DBG(:tag(tag)) { Clear(); }
	~Allocator() { Clear(); }
	Allocator(Allocator&& o) CARBON_MEMORY_DBG(:tag(o.tag)), m_head(std::move(o.m_head)) {}
	void Swap(Allocator& o) { m_head.swap(o.m_head); CARBON_MEMORY_DBG(std::swap(tag, o.tag)); }
	void Clear();

	template<class T> T* Alloc(ptrdiff_t count=1)
	{
		CarbonAssert(count >= 0);
		if(!count)
			return 0;
		T* result = Alloc<T>(*m_head, (carbon::size_t)count);
		if( !result )
			result = Alloc<T>(NewChunk((carbon::size_t)(sizeof(T) * count)), (carbon::size_t)count);
		CarbonAssert(result);
		return result;
	}

	inline ByteView Clone(const ByteView& input)
	{
		if(!input.length)
			return {};
		ByteView result = Clone(*m_head, input);
		if( !result.length )
			result = Clone(NewChunk(input.length), input);
		CarbonAssert(result.length);
		return result;
	}
	inline char* Clone(const char* input)
	{
		ByteView data { (const uint8_t*)input, (size_t)strlen(input)+1 };
		ByteView cloned = Clone(data);
		return (char*)cloned.bytes;
	}

	template<class T>
	T* Clone(const T* src, size_t count)
	{
		T* dst = Alloc<T>((ptrdiff_t)count);
		if( dst && src && count )
		{
			std::copy(src, src + count, dst);
		}
		return dst;
	}

private:
	void operator=(const Allocator&) = delete; // non-copyable
	struct Chunk
	{
		~Chunk()
		{
#if CARBON_MEMORY_CLEAR
			if( a.get() )
				memset(a.get(), 0xcd, size);
#endif
			CARBON_MEMORY_DBG(LogFree(size, tag, this));
		}
		Chunk(Chunk&& c)
			: a(std::move(c.a))
			, size(c.size)
			, used(c.used)
			CARBON_MEMORY_DBGC(tag(c.tag))
		{ CARBON_MEMORY_DBG(LogMove(size, tag, &c, this)); }
		Chunk(std::unique_ptr<uint8_t[]>&& data, size_t size, std::unique_ptr<Chunk>&& p CARBON_MEMORY_DBGC(const char* tag))
			: a(std::move(data))
			, size(size)
			, used(0)
			, prev(std::move(p))
			CARBON_MEMORY_DBGC(tag(tag))
		{ CARBON_MEMORY_DBG(LogAlloc(size, tag, this)); }

		CARBON_MEMORY_DBG(uint64_t _header = 0x1234123412341234U);
		std::unique_ptr<uint8_t[]> a;
		const size_t size = 0;
		size_t used = 0;
		std::unique_ptr<Chunk> prev;
		CARBON_MEMORY_DBG(const char* tag);
	};
	std::unique_ptr<Chunk> m_head;

	Chunk& NewChunk(size_t required)
	{
		CarbonAssert(required);
#if !CARBON_MEMORY_BULK_ALLOCATION
		size_t chunkSize = required;
#else
		size_t chunkSize = ((required+65535)/65536)*65536; // round up to 64k chunk allocations
		//size_t chunkSize = ((required+4095)/4096)*4096; // round up to 4k chunk allocations
#endif
		uint8_t* buffer = new uint8_t[chunkSize];

#if CARBON_MEMORY_CLEAR
		memset(buffer, 0xff, chunkSize);
#endif
		m_head = std::make_unique<Chunk>( std::unique_ptr<uint8_t[]>(buffer), chunkSize, std::move(m_head) CARBON_MEMORY_DBGC(tag) );
		return *m_head;
	}

	template<class T> static T* Alloc(Chunk& chunk, size_t count=1)
	{
		CarbonAssert(count > 0);
#if !CARBON_MEMORY_BULK_ALLOCATION
		size_t required = sizeof(T) * count;
		if( chunk.used || chunk.used + required > chunk.size )
			return 0;
		chunk.used = chunk.size;
		return (T*)chunk.a.get();
#else
		const size_t prev = chunk.used;
		size_t required = sizeof(T) * count;
		size_t index = prev;
		uint8_t* const p = &chunk.a[prev];
		const uintptr_t original = (uintptr_t)p;
		const uintptr_t aligned = ((original+__alignof(T)-1) / __alignof(T)) * __alignof(T);
		if( original != aligned )
		{
			CarbonAssert(aligned > original);
			auto extra = (carbon::size_t)(aligned - original);
			required += extra;
			index += extra;
		}
		if( chunk.used + required > chunk.size )
			return 0;
		chunk.used += required;
		return (T*)&chunk.a[index];
#endif
	}

	inline static ByteView Clone(Chunk& chunk, const ByteView& input)
	{
		if( chunk.used + input.length > chunk.size )
			return {};
		size_t prev = chunk.used;
		chunk.used += input.length;
		uint8_t* p = &chunk.a[prev];
		memcpy(p, input.bytes, input.length);
		return { p, input.length };
	}
public:
	bool Dbg_IsOwner(const void* obj) const
	{
		const Chunk* p = m_head.get();
		while(p)
		{
			const uint8_t* begin = p->a.get();
			const uint8_t* end   = begin + p->used;
			if( obj >= begin && obj < end )
				return true;
			p = p->prev.get();
		}
		return false;
	}
	void Dbg_Nuke()
	{
#if CARBON_MEMORY_CLEAR
		Chunk* p = m_head.get();
		while(p)
		{
			uint8_t* begin = p->a.get();
			memset(begin, 0xcc, p->size);
			p = p->prev.get();
		}
#endif
	}
};

inline void Allocator::Clear()
{
	Dbg_Nuke();
	if( !m_head )
		NewChunk(1);
	else
	{
		m_head->used = 0;
		Chunk* p = m_head.get();
		std::unique_ptr<Chunk> temp;
		while(p)
		{
			Chunk* prev = p->prev.get();
			CarbonAssert( prev != m_head.get() );
			temp = std::move(p->prev);
			p = prev;
		}
		CarbonAssert(!m_head->prev);
	}
}

class ReadView : public ByteView
{
	Allocator* allocator = 0;
	bool fail = false;
	bool nonStandard = false;
public:
	enum Flags
	{
		Copy    = 0,    // By default, all input data will be re-allocated to extend its lifetime
		InPlace = 1<<0, // But this flag allows obtaining a pointer into the source byte-view
		Relaxed = 1<<1, // Allows non-standard serialization forms to be used instead of complete strictness
	};
	const Flags flags;
	bool AllowNonStandard() const { return flags & Relaxed; }
	bool StrictMode() const { return !AllowNonStandard(); }
	bool AllowInPlace() const { return flags & InPlace; }
	bool ExtendLifetimes() const { return !AllowInPlace(); }

	ReadView(void* v, size_t s)                        :          flags(InPlace) { bytes = (uint8_t*)v; length = s; }
	ReadView(void* v, size_t s, Allocator& a, Flags f) : allocator(&a), flags(f) { bytes = (uint8_t*)v; length = s; }
	ReadView(const Bytes& v,    Allocator& a, Flags f) : allocator(&a), flags(f) { static_cast<ByteView&>(*this) = View(v); }
	ReadView(const ByteView& v, Allocator& a, Flags f) : allocator(&a), flags(f) { static_cast<ByteView&>(*this) = v; }
	explicit ReadView(const ReadView& o) : allocator(o.allocator), fail(o.fail), flags(o.flags) { length = o.length; bytes = o.bytes; }
	void operator=(const ReadView&) = delete;
	bool Failure() const { return fail; }
	bool Finished() const { return length==0; }
	bool OnNonStandardDataFound() { nonStandard |= true; fail |= StrictMode(); return AllowNonStandard(); }
	bool WasNonStandardDataFound() const { return nonStandard; }
	bool Advance(size_t o, ByteView& out)
	{
		out.bytes = bytes;
		bool ok = Advance(o);
		out.length = (size_t)(bytes - out.bytes);
		return ok;
	}
	bool Advance(size_t o)
	{
		fail |= ( o > length );
		if( fail )
			return false;
		bytes += o;
		length -= o;
		return true;
	}
	const void* Mark() const { return bytes; }
	void Rewind(const void* p)
	{
		ptrdiff_t diff = bytes - (uint8_t*)p;
		CarbonAssert(diff >= 0);
		length += (size_t)diff;
		bytes -= (size_t)diff;
	}
	bool ReadBytes(void* dst, size_t o)
	{
		const void* src = bytes;
		bool ok = Advance(o);
		if( ok )
			memcpy(dst, src, o);
		return ok;
	}
	template<class T>
	bool ReadBytes(T& v) { return ReadBytes(&v, sizeof(v)); }

	template<class T> T* Alloc(size_t count=1) { CarbonAssert(allocator); return allocator->Alloc<T>(count); }
	ByteView Clone(const ByteView& input) { CarbonAssert(allocator); return allocator->Clone(input); }
};

template<class T> ReadView ReadViewInPlace(const T& b, Allocator& a) { return {View(b), a, ReadView::InPlace}; }
template<class T> ReadView ReadViewCopy(   const T& b, Allocator& a) { return {View(b), a, ReadView::Copy}; }

class WriteView
{
	Bytes& m;
public:
	WriteView(Bytes& buf) : m(buf) {};
	void WriteByte(uint8_t b) { m.push_back(b); }
	void WriteBytes(ByteView b) { WriteBytes(b.bytes, b.length); }
	void WriteBytes(const uint8_t* b, size_t s)
	{
		if( !s )
			return;
		size_t offset = m.size();
		m.resize(offset + s);
		memcpy(&m.front()+offset, b, s);
	}
	void Reserve(size_t s) { m.reserve(m.size() + s); }

	const void* Mark() const { return (void*)(intptr_t)m.size(); } // return handle of where the write cursor is up to

	// Views are invalidated on next write operation!
	ByteView View() const { return carbon::View(m); } // view of everything written
	ByteView View(const void* marker) const // view from previous mark call to now
	{
		ByteView v = carbon::View(m);
		auto begin = (size_t)(intptr_t)marker;
		auto end = m.size();
		CarbonAssert( end >= begin );
		return { v.bytes + begin, end-begin };
	}
	ByteView View(const void* markerBegin, const void* markerEnd) const // view between two previous mark calls
	{
		ByteView v = carbon::View(m);
		auto begin = (size_t)(intptr_t)markerBegin;
		auto end = (size_t)(intptr_t)markerEnd;
		CarbonAssert( end >= begin );
		return { v.bytes + begin, end-begin };
	}
};

inline void Write4(uint32_t in, WriteView& writer) { writer.WriteBytes((const uint8_t*)&in, sizeof(in)); }
inline void Write4( int32_t in, WriteView& writer) { writer.WriteBytes((const uint8_t*)&in, sizeof(in)); }
inline void Write4(   float in, WriteView& writer) { writer.WriteBytes((const uint8_t*)&in, sizeof(in)); }
inline bool Read4(uint32_t& out, ReadView& reader) { return reader.ReadBytes(out); }
inline bool Read4( int32_t& out, ReadView& reader) { return reader.ReadBytes(out); }

inline void WriteArray(ByteView v, WriteView& writer)
{
	Write4((uint32_t)v.length, writer);
	writer.WriteBytes(v);
}
inline void WriteArray(uint32_t length, const Bytes* items, WriteView& writer)
{
	CarbonAssert(length < 0xFFFFFFFFU);
	Write4(length, writer);
	for( uint32_t i=0; i!=length; ++i )
		WriteArray(View(items[i]), writer);
}
template<class T> void WriteArray(const vector<T>& vec, WriteView& writer)
{
	WriteArray((uint32_t)vec.size(), vec.empty() ? nullptr : &vec.front(), writer);
}
inline void WriteArray(const Bytes& vec, WriteView& writer)
{
	WriteArray(View(vec), writer);
}
template<class ReadViewT> bool ReadArray(Bytes& vec, ReadViewT& reader)
{
	uint32_t length = 0;
	if( !Read4(length, reader) )
		return false;
	const uint8_t* bytes = reader.bytes;
	if( !reader.Advance(length) )
		return false;
	vec.resize(length);
	if( length )
		memcpy(&vec.front(), bytes, length);
	return true;
}

template<class T> void Clone(T& output, const T& input, Allocator& a)
{
	Bytes temp;
	Write(input, WriteView(temp));
	bool ok = Read(output, ReadViewCopy(temp, a));
	CarbonAssert(ok);
}

inline ByteView Compress( ByteView input, Bytes& buffer, int /*compressionLevel*/ )
{
	(void)input;
	(void)buffer;
	CarbonAssert(false, "Compression not available in header-only Carbon build");
	return {};
}
inline ByteView Decompress( ByteView input, Bytes& buffer )
{
	(void)input;
	(void)buffer;
	CarbonAssert(false, "Compression not available in header-only Carbon build");
	return {};
}

inline const char* ToHexBe(ByteView k, Allocator& a)
{
	carbon::size_t len = k.length * 2;
	char* c = a.Alloc<char>(len + 1);
	for(carbon::size_t j = 0; j < k.length; j++)
	{
		carbon::size_t i = (k.length-1) - j;
		int b = k.bytes[i] >> 4;
		c[j * 2] = (char)(55 + b + (((b - 10) >> 31) & -7));
		b = k.bytes[i] & 0xF;
		c[j * 2 + 1] = (char)(55 + b + (((b - 10) >> 31) & -7));
	}
	c[len] = '\0';
	return c;
}
inline const char* ToHexBe(ByteView k, vector<char>& c)
{
	c.resize(k.length * 2 + 1);
	auto* out = &c.front();
	for(carbon::size_t j = 0; j < k.length; j++)
	{
		carbon::size_t i = (k.length-1) - j;
		int b = k.bytes[i] >> 4;
		out[j * 2] = (char)(55 + b + (((b - 10) >> 31) & -7));
		b = k.bytes[i] & 0xF;
		out[j * 2 + 1] = (char)(55 + b + (((b - 10) >> 31) & -7));
	}
	out[c.size()-1] = '\0';
	return out;
}
inline ByteView FromHexBe(const char* sz, Bytes& c)
{
	CarbonAssert(sz);
	if( sz[0] == '0' && (sz[1] == 'x' || sz[1] == 'X') )
		sz += 2;
	const int len = (int)strlen(sz) / 2;
	c.resize(len);
	for(int i = 0; i < len; i++)
	{
		auto idx = (size_t)i;
		int A = 0;
		int B = 0;
		const char a = (char)toupper(sz[i * 2 + 0]);
		const char b = (char)toupper(sz[i * 2 + 1]);
		if( a >= '0' && a <= '9' ) A = a - '0';
		else if( a >= 'A' && a <= 'F' ) A = a - 'A' + 10;
		if( b >= '0' && b <= '9' ) B = b - '0';
		else if( b >= 'A' && b <= 'F' ) B = b - 'A' + 10;
		c[len-(idx+1)] = (uint8_t)(A * 16 + B);
	}
	return { &c.front(), c.size() };
}

inline const char* ToHex(ByteView k, Allocator& a)
{
	carbon::size_t len = k.length * 2;
	char* c = a.Alloc<char>(len + 1);
	for(carbon::size_t i = 0; i < k.length; i++)
	{
		int b = k.bytes[i] >> 4;
		c[i * 2] = (char)(55 + b + (((b - 10) >> 31) & -7));
		b = k.bytes[i] & 0xF;
		c[i * 2 + 1] = (char)(55 + b + (((b - 10) >> 31) & -7));
	}
	c[len] = '\0';
	return c;
}
inline const char* ToHex(ByteView k, vector<char>& c)
{
	c.resize(k.length * 2 + 1);
	auto* out = &c.front();
	for(carbon::size_t i = 0; i < k.length; i++)
	{
		int b = k.bytes[i] >> 4;
		out[i * 2] = (char)(55 + b + (((b - 10) >> 31) & -7));
		b = k.bytes[i] & 0xF;
		out[i * 2 + 1] = (char)(55 + b + (((b - 10) >> 31) & -7));
	}
	out[c.size()-1] = '\0';
	return out;
}
inline ByteView FromHex(const char* sz, Bytes& c)
{
	CarbonAssert(sz);
	if( sz[0] == '0' && (sz[1] == 'x' || sz[1] == 'X') )
		sz += 2;
	const auto len = (carbon::size_t)strlen(sz) / 2;
	c.resize(len);
	for(carbon::size_t i = 0; i < len; i++)
	{
		int A = 0;
		int B = 0;
		const char a = (char)toupper(sz[i * 2 + 0]);
		const char b = (char)toupper(sz[i * 2 + 1]);
		if( a >= '0' && a <= '9' ) A = a - '0';
		else if( a >= 'A' && a <= 'F' ) A = a - 'A' + 10;
		if( b >= '0' && b <= '9' ) B = b - '0';
		else if( b >= 'A' && b <= 'F' ) B = b - 'A' + 10;
		c[i] = (uint8_t)(A * 16 + B);
	}
	return { &c.front(), c.size() };
}
inline ByteView FromHex(const char* sz, Allocator& a)
{
	CarbonAssert(sz);
	if( sz[0] == '0' && (sz[1] == 'x' || sz[1] == 'X') )
		sz += 2;
	const auto len = (carbon::size_t)strlen(sz) / 2;
	uint8_t* c = a.Alloc<uint8_t>(len);
	for(carbon::size_t i = 0; i < len; i++)
	{
		int A = 0;
		int B = 0;
		const char a = (char)toupper(sz[i * 2 + 0]);
		const char b = (char)toupper(sz[i * 2 + 1]);
		if( a >= '0' && a <= '9' ) A = a - '0';
		else if( a >= 'A' && a <= 'F' ) A = a - 'A' + 10;
		if( b >= '0' && b <= '9' ) B = b - '0';
		else if( b >= 'A' && b <= 'F' ) B = b - 'A' + 10;
		c[i] = (uint8_t)(A * 16 + B);
	}
	return { c, len };
}

#ifndef _MSC_VER
inline int _vscprintf (const char * format, va_list pargs)
{
	va_list argcopy;
	va_copy(argcopy, pargs);
	const int retval = vsnprintf(NULL, 0, format, argcopy);
	va_end(argcopy);
	return retval;
}
#endif

inline char* VFormat( vector<char>& a, const char* fmt, va_list& args )
{
	int length = _vscprintf( fmt, args ) + 1; // +1 for terminating '\0'
	a.resize(length);
	char* buffer = length ? &a.front() : 0;
	vsnprintf(buffer, length, fmt, args);
	return buffer;
}
inline char* VFormat( Allocator& a, const char* fmt, va_list& v )
{
	int length = _vscprintf( fmt, v ) + 1; // +1 for terminating '\0'
	char* buffer = a.Alloc<char>(length);
	vsnprintf(buffer, length, fmt, v);
	return buffer;
}
inline char* Format( Allocator& a, const char* fmt, ... )
{
	va_list	v;
	va_start(v, fmt);
	char* buffer = VFormat(a, fmt, v);
	va_end( v );
	return buffer;
}

inline void LogAlloc(size_t size, const char* tag, const void* owner)
{
	SmallString str( tag, true );
	AllocLog& s = GetAllocLog();
	std::lock_guard lk(s.m);
	auto& pair = s.usage[str];
	pair.count ++;
	pair.usage += size;
	pair.peak = Max(pair.peak, pair.usage);
#ifdef DEV
	CarbonAssert(s.owners.insert({owner, {size, str}}).second);
#else
	// no-op: silence unused parameter warnings when DEV tracking is off
	(void)owner;
#endif
}
inline void LogMove(size_t size, const char* tag, const void* ownerOld, const void* ownerNew)
{
#ifdef DEV
	SmallString str( tag, true );
	AllocLog& s = GetAllocLog();
	std::lock_guard lk(s.m);
	auto f = s.owners.find(ownerOld);
	CarbonAssert(f != s.owners.end());
	CarbonAssert(f->second.size == size);
	CarbonAssert(f->second.tag == str);
	s.owners.erase(f);
	CarbonAssert(s.owners.insert({ownerNew, {size, str}}).second);
#else
	// no-op: silence unused parameter warnings when DEV tracking is off
	(void)size; (void)tag; (void)ownerOld; (void)ownerNew;
#endif
}
inline void LogFree(size_t size, const char* tag, const void* owner)
{
	SmallString str( tag, true );
	AllocLog& s = GetAllocLog();
	std::lock_guard lk(s.m);
	auto& pair = s.usage[str];
	CarbonAssert( pair.usage >= size );
	CarbonAssert( pair.count >= 1 );
	pair.count --;
	pair.usage -= size;
#ifdef DEV
	auto f = s.owners.find(owner);
	CarbonAssert(f != s.owners.end());
	CarbonAssert(f->second.size == size);
	CarbonAssert(f->second.tag == str);
	s.owners.erase(f);
#else
	// no-op: silence unused parameter warnings when DEV tracking is off
	(void)owner;
#endif
}
inline bool LogLeaks()
{
	bool leaky = false;
	AllocLog& s = GetAllocLog();
	std::lock_guard lk(s.m);
	for( const auto& i : s.usage )
	{
		bool ok = i.second.count == 0 && i.second.usage == 0;
		if( ok )
			continue;
		leaky = true;
		printf("LEAK: %s, %" PRIu64 " bytes, %" PRIu64 " allocations\n", i.first.c_str(), (uint64_t)i.second.usage, (uint64_t)i.second.count);
	}
#ifdef DEV
	leaky |= !s.owners.empty();
#endif
	return leaky;
}

} // namespace phantasma::carbon
