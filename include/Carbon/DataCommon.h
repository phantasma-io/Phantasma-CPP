#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif

#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>

#if defined(CARBON_HAS_SODIUM)
#define CARBON_HAS_SODIUM_IMPL (!!(CARBON_HAS_SODIUM))
#elif defined(__has_include)
# if __has_include(<sodium.h>)
#  define CARBON_HAS_SODIUM_IMPL 1
# endif
#endif
#ifndef CARBON_HAS_SODIUM_IMPL
#define CARBON_HAS_SODIUM_IMPL 0
#endif

#if CARBON_HAS_SODIUM_IMPL
#include <sodium.h>
#endif

#if !defined(CARBON_DEEP_TESTING) && !defined(CARBON_OPTIMIZED)
#define CARBON_DEEP_TESTING 1
#endif

#ifndef CARBON_DEEP_TESTING
//HACK - please leave CARBON_DEEP_TESTING on for now in any production systems!
//TODO - do enough formal testing to prove this isn't needed for correctness.
#pragma message("NOTE: Forcing CARBON_DEEP_TESTING on")
#define CARBON_DEEP_TESTING 1
#endif

#ifndef CARBON_DEEP_TESTING
//HACK - please leave CARBON_DEEP_TESTING on for now in any production systems!
//TODO - do enough formal testing to prove this isn't needed for correctness.
#pragma message("NOTE: Forcing CARBON_DEEP_TESTING on")
#define CARBON_DEEP_TESTING
#endif

namespace phantasma::carbon {

void OS_Assert(bool condition, const char* format, ...);

} // namespace phantasma::carbon

#if defined(__GNUC__) || defined(__clang__)
#define CARBON_UNREACHABLE                __builtin_unreachable()
#define CARBON_UNREACHABLE_IF(x) do{if(x){__builtin_unreachable();}}while(0)
#elif defined(_MSC_VER)
#define CARBON_UNREACHABLE       __assume(0)
#define CARBON_UNREACHABLE_IF(x) __assume(!(x))
#else
#define CARBON_UNREACHABLE       do{}while(0)
#define CARBON_UNREACHABLE_IF(x) do{}while(0)
#endif

#if defined(DEV) || defined(DEBUG)
#define CARBON_REL_UNREACHABLE       do{}while(0)
#define CARBON_REL_UNREACHABLE_IF(x) do{}while(0)
#else
#define CARBON_REL_UNREACHABLE       CARBON_UNREACHABLE
#define CARBON_REL_UNREACHABLE_IF(x) CARBON_UNREACHABLE_IF(x)
#endif

#define CARBON_JOIN_DETAIL(a, b) a##b
#define CARBON_STRINGIZE_DETAIL(x) #x
#define CARBON_JOIN(a, b) CARBON_JOIN_DETAIL(a, b)
#define CARBON_STRINGIZE(x) CARBON_STRINGIZE_DETAIL(x)

#define CARBON_JOIN_DEBUG_INFO( message, file, line ) message " in " file " @" CARBON_STRINGIZE(line)

#define CarbonAssert(c, ...) do{bool c_ = (c); if(!c_){::phantasma::carbon::OS_Assert(c_, CARBON_JOIN_DEBUG_INFO("\"" #c "\"", __FILE__, __LINE__ ) "\n" __VA_ARGS__);CARBON_REL_UNREACHABLE;} }while(0)

namespace phantasma::carbon {

//--------------------------------------------------------------
// The SDK supports overriding the use of std::vector with a custom type:
//--------------------------------------------------------------
#ifdef PHANTASMA_VECTOR
	typedef PHANTASMA_VECTOR<uint8_t>::size_type size_t;
#else
	typedef ::size_t size_t;
#endif

//--------------------------------------------------------------
// Length-prefixed strings
//--------------------------------------------------------------

struct SmallString
{
	uint8_t length;
	mutable char bytes[256];

	static SmallString Null() { return SmallString{ 0 }; }

	SmallString() = default;
	template<int N> constexpr SmallString(const char(&sz)[N]) : length(N - 1)  { memcpy(bytes, sz, N - 1); static_assert(N <= 256); }
	explicit SmallString(const char* sz, bool truncate = false);
	SmallString(const char* sz, size_t length);

	explicit operator bool() const { return !!length; }
	const char* c_str() const
	{
		if (!length)
			return "";
		bytes[length] = 0;
		return bytes;
	}
	bool operator==(const SmallString& o) const { return length == o.length && 0 == memcmp(bytes, o.bytes, length); }
	bool operator!=(const SmallString& o) const { return !(*this == o); }
	bool operator<(const SmallString& b) const { return strcmp(c_str(), b.c_str()) < 0; }
	bool operator>(const SmallString& b) const { return strcmp(c_str(), b.c_str()) > 0; }
};

//--------------------------------------------------------------
// Read-only range of bytes
//--------------------------------------------------------------

struct ByteView
{
	const uint8_t* bytes;
	size_t length;

	bool empty() const { return !length; }
	const uint8_t* begin() const { return bytes; }
	const uint8_t* end() const { return bytes + length; }
};

//--------------------------------------------------------------
// To pass around small fixed-size buffers easily
//--------------------------------------------------------------

template<int N, typename Byte = uint8_t> struct BytesN
{
	constexpr static int length = N;
	Byte bytes[N];

	BytesN() = default;
	explicit BytesN(ByteView rhs) { *this = rhs; }
	BytesN(const Byte* data, size_t len) { *this = ByteView{ data, len }; }
	explicit BytesN(const ByteArray& arr) { *this = ByteView{ arr.data(), arr.size() }; }

	explicit operator bool() const { return !!*this; }
	bool operator!() const { return *this == BytesN{}; }
	bool operator==(const BytesN& o) const { return 0 == memcmp(bytes, o.bytes, N); }
	bool operator!=(const BytesN& o) const { return 0 != memcmp(bytes, o.bytes, N); }
	bool operator==(const ByteView& o) const { return o.length == N && 0 == memcmp(bytes, o.bytes, N); }
	bool operator!=(const ByteView& o) const { return !(*this == o); }
	constexpr const Byte& operator[](int n) const { return bytes[n]; }
	constexpr       Byte& operator[](int n)       { return bytes[n]; }

	BytesN<N>& operator=(ByteView rhs)
	{
		for (size_t i = 0; i != N; ++i)
			bytes[i] = (i < rhs.length) ? rhs.bytes[i] : 0;
		return *this;
	}
	BytesN<N>& operator^=(BytesN<N> rhs)
	{
		for (size_t i = 0; i != N; ++i)
			bytes[i] ^= rhs.bytes[i];
		return *this;
	}
};

typedef BytesN<16> Bytes16;
typedef BytesN<32> Bytes32; // often used as public keys and IDs
typedef BytesN<64> Bytes64; // often used as signatures

//--------------------------------------------------------------
// To pass around small dynamically-size buffers easily
//--------------------------------------------------------------

template<uint32_t N, typename Byte = uint8_t> struct BytesUpToN
{
	constexpr static int max_length = N;
	Byte bytes[N];
	uint32_t length = 0;

	void resize(size_t l) { length = (uint32_t)l; }
	bool empty() const { return length == 0; }

	constexpr const Byte& operator[](int n) const { return bytes[n]; }
	constexpr       Byte& operator[](int n)       { return bytes[n]; }
};

//--------------------------------------------------------------
// Best practices for secrets in memory
//--------------------------------------------------------------
inline void OS_PinMemory(uint8_t*, size_t);
inline void OS_ClearPinned(uint8_t*, size_t);
inline void OS_FillRandom(uint8_t*, size_t);

template<int N, typename B = uint8_t> struct alignas(16) PrivateBytes : BytesN<N, B>
{
	PrivateBytes() { OS_PinMemory((uint8_t*)this->bytes, this->length); }
	~PrivateBytes() { OS_ClearPinned((uint8_t*)this->bytes, this->length); }
	PrivateBytes(const PrivateBytes& o) : PrivateBytes()
	{
		memcpy(this->bytes, o.bytes, this->length);
	}
	PrivateBytes& operator=(const PrivateBytes&) = default;

	void FillRandom() { OS_FillRandom((uint8_t*)this->bytes, this->length); }
private:
};

typedef PrivateBytes<32> Private32;
typedef PrivateBytes<64> Private64;

//--------------------------------------------------------------
// Signatures
//--------------------------------------------------------------

struct Witness
{
	Bytes32 address;
	Bytes64 signature;
};

struct Witnesses
{
	uint32_t numWitnesses;
	const Witness* witnesses;
};

inline bool operator==(const Witness& a, const Witness& b) { return a.address == b.address && a.signature == b.signature; }

//--------------------------------------------------------------
// When using ByteViews as keys in hash tables
//--------------------------------------------------------------

struct HashedByteView : public ByteView
{
	HashedByteView();
	HashedByteView(const HashedByteView&);
	HashedByteView(ByteView v);
	HashedByteView& operator=(const HashedByteView&);

	uint64_t hash;
};

//--------------------------------------------------------------
// When using Bytes32 as keys in hash tables
//--------------------------------------------------------------

template<int N, typename Byte = uint8_t>
struct BytesHasher
{
	size_t operator()(const BytesN<N, Byte>& s) const
	{
		size_t result;
		memcpy(&result, s.bytes, sizeof(size_t));
		return result;
	}
};

typedef BytesHasher<32> Bytes32Hasher;

//--------------------------------------------------------------
// Operators
//--------------------------------------------------------------
template<class T> T Min(T a, T b) { return a < b ? a : b; }
template<class T> T Max(T a, T b) { return a > b ? a : b; }

inline bool operator==(const ByteView& l, const ByteView& r)
{
	return l.length == r.length && (!r.length || 0 == memcmp(l.bytes, r.bytes, r.length));
}
inline bool operator!=(const ByteView& l, const ByteView& r)
{
	return !(l == r);
}
inline bool operator<(const ByteView& a, const ByteView& b) // lexicographic order
{
	const uint8_t* const ka = a.bytes;
	const uint8_t* const kb = b.bytes;
	const size_t minLength = Min(a.length, b.length);
	for (size_t i = 0; i != minLength; ++i)
	{
		if (ka[i] < kb[i])
			return true;
		if (ka[i] > kb[i])
			return false;
	}
	return a.length < b.length;
}
inline bool operator>=(const ByteView& a, const ByteView& b)
{
	return !(a < b);
}
inline bool operator==(const HashedByteView& l, const ByteView& r)
{
	return static_cast<const ByteView&>(l) == r;
}
inline bool operator==(const HashedByteView& l, const HashedByteView& r)
{
	return l.hash == r.hash && static_cast<const ByteView&>(l) == static_cast<const ByteView&>(r);
}
inline bool operator!=(const HashedByteView& l, const HashedByteView& r)
{
	return !(l == r);
}
inline bool operator<(const HashedByteView& l, const HashedByteView& r) // lexicographic order
{
	return static_cast<const ByteView&>(l) < static_cast<const ByteView&>(r);
}
inline bool operator<(const HashedByteView& l, const ByteView& r) // lexicographic order
{
	return static_cast<const ByteView&>(l) < r;
}

template<int N> bool operator<(const BytesN<N>& a, const BytesN<N>& b) // lexicographic order
{
	for (size_t i = 0; i != N; ++i)
	{
		if (a.bytes[i] < b.bytes[i])
			return true;
		if (a.bytes[i] > b.bytes[i])
			return false;
	}
	return false;
}
template<int N> bool operator>=(const BytesN<N>& a, const BytesN<N>& b)
{
	return !(a < b);
}

//--------------------------------------------------------------
// Casting other types to bytes views
//--------------------------------------------------------------
inline                      ByteView View(const void* b, const void* e) { return ByteView{ (const uint8_t*)b, (size_t)(((const char*)e) - ((const char*)b)) }; } // C-style byte range
inline                      ByteView View(const ByteView& b) { return b; } // for convenience in templates, support casting a view to a view...
inline                      ByteView View(const SmallString& s) { return ByteView{ (uint8_t*)s.bytes, s.length }; } // the contents of the string
template<int N>             ByteView View(const BytesN<N>& b) { return ByteView{ b.bytes, N }; } // fixed-size byte arrays
template<uint32_t N>        ByteView View(const BytesUpToN<N>& b) { return ByteView{ b.bytes, b.length }; } // variable-size byte arrays
template<class Type, int N> ByteView View(Type(&b)[N]) { return ByteView{ (uint8_t*)b, sizeof(Type) * N }; } // compile-time fixed arrays
inline                      ByteView View(const uint8_t& b) { return ByteView{ &b, 1 }; } // a byte!
inline                      ByteView View(const uint32_t& b) { return ByteView{ (uint8_t*)&b, 4 }; } // integers
inline                      ByteView View(const int32_t& b) { return ByteView{ (uint8_t*)&b, 4 }; }
inline                      ByteView View(const uint64_t& b) { return ByteView{ (uint8_t*)&b, 8 }; }
inline                      ByteView View(const int64_t& b) { return ByteView{ (uint8_t*)&b, 8 }; }

//--------------------------------------------------------------
// Hashing helpers
//--------------------------------------------------------------
inline void CarbonEnsureSodiumInit()
{
	static std::once_flag once;
	std::call_once(once, [&]()
	{
#if CARBON_HAS_SODIUM_IMPL
		int err = sodium_init();
		CarbonAssert(err == 0);
#else
		CarbonAssert(false, "libsodium not available");
#endif
	});
}

uint64_t SipHash24(ByteView in, const uint64_t key[2]);
uint64_t SipHash24_Random(ByteView in);

inline void OS_PinMemory(uint8_t* bytes, size_t length)
{
	if (!bytes || !length)
		return;
	CarbonEnsureSodiumInit();
#if CARBON_HAS_SODIUM_IMPL
	sodium_mlock(bytes, length);
#else
	CarbonAssert(false, "libsodium not available");
#endif
}
inline void OS_ClearPinned(uint8_t* bytes, size_t length)
{
	if (!bytes || !length)
		return;
	CarbonEnsureSodiumInit();
#if CARBON_HAS_SODIUM_IMPL
	sodium_memzero(bytes, length);
	sodium_munlock(bytes, length);
#else
	CarbonAssert(false, "libsodium not available");
#endif
}
inline void OS_FillRandom(uint8_t* bytes, size_t length)
{
	if (!bytes || !length)
		return;
	CarbonEnsureSodiumInit();
#if CARBON_HAS_SODIUM_IMPL
	randombytes_buf(bytes, length);
#else
	CarbonAssert(false, "libsodium not available");
#endif
}

inline SmallString::SmallString(const char* sz, bool truncate)
{
	if (!sz)
	{
		length = 0;
		return;
	}
	size_t len = strlen(sz);
	if (truncate && len > 255)
	{
		len = 255;
	}
	CarbonAssert(len <= 255, "SmallString was too long");
	length = (uint8_t)len;
	if (length)
	{
		memcpy(bytes, sz, length);
	}
	bytes[length] = 0;
}
inline SmallString::SmallString(const char* sz, size_t len)
{
	CarbonAssert(len <= 255, "SmallString was too long");
	length = (uint8_t)len;
	if (length)
	{
		memcpy(bytes, sz, length);
	}
	bytes[length] = 0;
}

inline HashedByteView::HashedByteView()
{
	bytes = 0;
	length = 0;
	hash = SipHash24_Random(*this);
}
inline HashedByteView::HashedByteView(const HashedByteView& o)
{
	bytes = o.bytes;
	length = o.length;
	hash = o.hash;
}
inline HashedByteView::HashedByteView(ByteView v)
{
	bytes = v.bytes;
	length = v.length;
	hash = SipHash24_Random(v);
}
inline HashedByteView& HashedByteView::operator=(const HashedByteView& o)
{
	bytes = o.bytes;
	length = o.length;
	hash = o.hash;
	return *this;
}

inline uint64_t SipHash24(ByteView in, const uint64_t key[2])
{
	CarbonEnsureSodiumInit();
#if CARBON_HAS_SODIUM_IMPL
	union
	{
		uint64_t v;
		unsigned char b[8];
	} out;
	crypto_shorthash_siphash24(out.b, in.bytes, in.length, (unsigned char*)key);
	return out.v;
#else
	CarbonAssert(false, "libsodium not available");
	(void)in; (void)key;
	return 0;
#endif
}
inline uint64_t SipHash24_Random(ByteView in)
{
	CarbonEnsureSodiumInit();
#if CARBON_HAS_SODIUM_IMPL
	static uint64_t keys[2] = {};
	static std::once_flag once;
	std::call_once(once, [&]() { randombytes_buf(keys, sizeof(keys)); });
	return SipHash24(in, keys);
#else
	(void)in;
	CarbonAssert(false, "libsodium not available");
	return 0;
#endif
}

inline void OS_Assert(bool condition, const char* format, ...)
{
	if (condition)
		return;
	char buffer[512];
	va_list args;
	va_start(args, format);
	const int written = vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	const char* message = written >= 0 ? buffer : format;
	PHANTASMA_EXCEPTION(message);
}

} // namespace phantasma::carbon
