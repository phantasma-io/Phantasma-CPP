#pragma once
#include "Int256.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <inttypes.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
// tiny-bignum-c
extern "C"
{
#undef require
#define require(a,b) CarbonAssert(a, b)
#include "External/tiny-bignum-c/bn_impl.h"
}
static_assert(sizeof(bn) == sizeof(uint256));
static_assert(sizeof(bn) == sizeof(int256));
struct bn* B(const uint256& u) { return (bn*)&u; }
struct bn* B(const  int256& u) { return (bn*)&u; }
#ifdef THIS
#undef THIS
#endif
#define THIS ((bn*)this)

//------------------------------------------------------------------------------
// Read/Write helpers for uint256/int256/intx

inline bool Read(uint256& out, ReadView& reader)
{
	uint8_t header = 0;
	if (!reader.ReadBytes(header))
		return false;
	uint8_t fill = header & 0x80 ? 0xFF : 0x00;
	uint8_t length = header & 0x3F;
	if (length > 32 || (header & 0x40)) // non-standard header
		return false;
	uint8_t* dst = (uint8_t*)&out;
	if (length > 0 && !reader.ReadBytes(dst, length))
		return false;
	for (uint8_t* p = dst + length, *end = dst + 32; p != end; ++p)
		*p = fill;
	if ((dst[31] & 0x80) != (header & 0x80)) // non-standard header
		return false;
	return true;
}
inline void Write(const uint256& in, WriteView& writer)
{
	if (!in)
		return writer.WriteByte(0);
	const uint8_t fill = in.Signed().IsNegative() ? 0xFF : 0x00;
	const uint8_t* const src = (uint8_t*)&in;
	const uint32_t length = CarbonComputeSerializedLength(src, 32, fill);
	CarbonAssert(length <= 32);
	uint8_t header = (uint8_t)(length & 0x3F) | (fill & 0x80);
	writer.WriteByte(header);
	for (const uint8_t* p = src, *end = src + length; p != end; ++p)
		writer.WriteByte(*p);
}

inline bool Read(int256& out, ReadView& reader)
{
	return Read(out.Unsigned(), reader);
}
inline void Write(const int256& in, WriteView& writer)
{
	Write(in.Unsigned(), writer);
}

inline bool Read(intx& out, ReadView& reader)
{
	ReadView test{ reader };
	uint8_t header = 0;
	if (!test.ReadBytes(header))
		return false;
	if ((header & 0x3F) < 8) // not a valid intx
		return false;
	if ((header & 0x3F) == 8) // it's an 8 byte value
	{
		uint64_t value = 0;
		if (!test.ReadBytes(value))
			return false;
		// check if 64bit interpretation preserves the sign of the original value
		bool headerIsNegative = header & 0x80;
		bool valueIsNegative = ((int64_t)value) < 0;
		if (headerIsNegative == valueIsNegative)
		{
			bool ok = reader.Advance(9);
			CarbonAssert(ok); // we just read 9 bytes on the test copy of reader
			out.normal = value;
			out.isBig = false;
			return ok;
		}
		// else we need 65 bits to represent this value
		// what's happened is a sign extension mismatch. Either:
		//  -  bytes[8..31] == 0x00 && value & 0x80... == 0x80...
		//  -  bytes[8..31] == 0xff && value & 0x7F... == value
		// so store as bigint
	}
	bool ok = Read(out.big, reader);
	out.isBig = true;
	if (ok && out.big.Is8ByteSafe())
	{
		// this is actually tolerable but is a non-standard intx form
		ok &= reader.OnNonStandardDataFound(); // so readers in strict mode will treat it as a failure
	}
	return ok;
}
inline void Write(const intx& in, WriteView& writer)
{
	int64_t value;
	if (in.isBig)
	{
		if (!in.big.Signed().Is8ByteSafe()) // is it actually big
		{
			const uint256& big = in;
			Write(big, writer);
			return;
		}
		value = (int64_t)in.big.Signed();
	}
	else
		value = (int64_t)in.normal;

	uint8_t header = value < 0 ? 0x88 : 0x08;
	writer.WriteByte(header);
	Write8(value, writer);
}

//------------------------------------------------------------------------------
static constexpr char s_dictionary[] = "0123456789ABCDEF";

inline std::string uint256::ToString(uint32_t _base, const char* customDictionary) const
{
	const char* dictionary = customDictionary ? customDictionary : s_dictionary;
	CarbonAssert(_base > 1 && (_base <= 16 || customDictionary));
	CarbonAssert(strlen(dictionary) >= _base);
	if (bignum_is_zero(THIS))
		return "0";
	uint256 remainder;
	uint256 base(_base);
	uint256 temp = *this;
	std::string result;
	result.reserve(32);
	for (;;)
	{
		uint256 next;
		bignum_divmod(B(temp), B(base), B(next), B(remainder));
		uint32_t digit = remainder.bits[0];
		CarbonAssert(digit < _base);
		char c = dictionary[digit];
		result.append(1, c);
		if (!next)
			break;
		temp = next;
	}
	std::reverse(result.begin(), result.end());
	return result;
}

inline std::string int256::ToString(uint32_t base, const char* customDictionary, char negative) const
{
	if (!IsNegative())
		return Unsigned().ToString(base, customDictionary);
	std::string result;
	result.reserve(33);
	result.append(1, negative);
	result.append(TwosCompliment().Unsigned().ToString(base, customDictionary));
	return result;
}

inline intx intx::operator+(const intx& o) const
{
	const int256& a = *this, b = o;
	return a + b;
}
inline intx intx::operator-(const intx& o) const
{
	const int256& a = *this, b = o;
	return a - b;
}
inline bool intx::operator>(const intx& o) const
{
	const int256& a = *this, b = o;
	return a > b;
}
inline bool intx::operator==(const intx& o) const
{
	if (!isBig && !o.isBig)
		return normal == o.normal;
	const int256& a = *this, b = o;
	return a == b;
}

inline intx intx::FromString(const char* str, uint32_t strLength, uint32_t radix, bool* out_error)
{
	if (str && strLength == 0)
	{
		strLength = (int)strlen(str);
	}

	if (strLength < 20 && radix <= 10)
	{
		int64_t normal = strtoll(str, NULL, radix);
		if (normal != LLONG_MAX && normal != LLONG_MIN)
		{
			return intx(normal);
		}
	}
	return intx(uint256::FromString(str, strLength, radix, out_error));
}

inline uint256 uint256::FromString(const char* str, uint32_t strLength, uint32_t radix, bool* out_error)
{
	if (str && strLength == 0)
	{
		strLength = (int)strlen(str);
	}

	int sign = 0;

	if (strLength == 0 || str[0] == '\0' || (strLength == 1 && str[0] == '0'))
	{
		return uint256(0);
	}

	const char* first = str;
	const char* last = first + strLength - 1;
	while (*first == '\r' || *first == '\n')
		++first;
	while (last >= first && (*last == '\r' || *last == '\n'))
		--last;

	if (*first == '-')
	{
		++first;
		sign = -1;
	}
	else
	{
		sign = 1;
	}

	int length = (int)(last + 1 - first);

	uint256 bigRadix(10);
	uint256 result(0);
	uint256 bi(1);
	for (int i = 0; i < length; i++)
	{
		uint32_t val = toupper(last[-i]);
		val = ((val >= '0' && val <= '9') ? (val - '0') : ((val < 'A' || val > 'Z') ? 9999999 : (val - 'A' + 10)));
		if (val >= radix)
		{
			if (out_error)
				*out_error = true;
			return result;
		}

		result += bi * uint256(val);

		if (i + 1 < length)
			bi *= bigRadix;
	}

	if (sign < 0)
		return (-int256(result)).Unsigned();
	return result;
}

inline uint256& int256::Unsigned() { return (uint256&)*this; }
inline const uint256& int256::Unsigned() const { return (const uint256&)*this; }
inline int256& uint256::Signed() { return (int256&)*this; }
inline const int256& uint256::Signed() const { return (const int256&)*this; }

inline uint256::uint256(const uint256& o)
{
	bignum_assign(THIS, B(o));
}
inline uint256& uint256::operator=(const uint256& o)
{
	if (this != &o)
	{
		bignum_assign(THIS, B(o));
	}
	return *this;
}
inline int256::int256(const uint256& o)
{
	bignum_assign(THIS, B(o));
}
inline int256& int256::operator=(const int256& o)
{
	if (this != &o)
	{
		bignum_assign(THIS, B(o));
	}
	return *this;
}
inline uint256::uint256(uint64_t i)
{
	bignum_from_int(THIS, i);
}

inline uint256 uint256::operator+ (const uint256& o) const
{
	uint256 r;
	bignum_add(THIS, B(o), B(r));
	return r;
}
inline uint256& uint256::operator+=(const uint256& o)
{
	bignum_add(THIS, B(o), THIS);
	return *this;
}
inline uint256 uint256::operator- (const uint256& o) const
{
	uint256 r;
	bignum_sub(THIS, B(o), B(r));
	return r;
}
inline uint256& uint256::operator-=(const uint256& o)
{
	bignum_sub(THIS, B(o), THIS);
	return *this;
}
inline uint256 uint256::operator* (const uint256& o) const
{
	uint256 r;
	bignum_mul(THIS, B(o), B(r));
	return r;
}
inline uint256& uint256::operator*=(const uint256& o)
{
	uint256 a = *this;
	bignum_mul(B(a), B(o), THIS);
	return *this;
}
inline uint256 uint256::operator/ (const uint256& o) const
{
	uint256 r;
	bignum_div(THIS, B(o), B(r));
	return r;
}
inline uint256& uint256::operator/=(const uint256& o)
{
	bignum_div(THIS, B(o), THIS);
	return *this;
}
inline uint256 uint256::operator% (const uint256& o) const
{
	uint256 r;
	bignum_mod(THIS, B(o), B(r));
	return r;
}
inline uint256& uint256::operator%=(const uint256& o)
{
	bignum_mod(THIS, B(o), THIS);
	return *this;
}

inline uint256 uint256::operator~() const
{
	uint256 r;
	bignum_not(THIS, B(r));
	return r;
}
inline uint256 uint256::operator& (const uint256& o) const
{
	uint256 r;
	bignum_and(THIS, B(o), B(r));
	return r;
}
inline uint256& uint256::operator&=(const uint256& o)
{
	bignum_and(THIS, B(o), THIS);
	return *this;
}
inline uint256 uint256::operator| (const uint256& o) const
{
	uint256 r;
	bignum_or(THIS, B(o), B(r));
	return r;
}
inline uint256& uint256::operator|=(const uint256& o)
{
	bignum_or(THIS, B(o), THIS);
	return *this;
}
inline uint256 uint256::operator^ (const uint256& o) const
{
	uint256 r;
	bignum_xor(THIS, B(o), B(r));
	return r;
}
inline uint256& uint256::operator^=(const uint256& o)
{
	bignum_xor(THIS, B(o), THIS);
	return *this;
}

inline uint256 uint256::operator<< (int nbits) const
{
	uint256 r;
	bignum_lshift(THIS, B(r), nbits);
	return r;
}
inline uint256& uint256::operator<<=(int nbits)
{
	bignum_lshift(THIS, THIS, nbits);
	return *this;
}
inline uint256 uint256::operator>> (int nbits) const
{
	uint256 r;
	bignum_rshift(THIS, B(r), nbits);
	return r;
}
inline uint256& uint256::operator>>=(int nbits)
{
	bignum_rshift(THIS, THIS, nbits);
	return *this;
}

inline int uint256::Compare(const uint256& o) const
{
	return bignum_cmp(THIS, B(o));
}
inline bool uint256::operator!() const
{
	return bignum_is_zero(THIS);
}
inline uint256::operator bool() const
{
	return !bignum_is_zero(THIS);
}
inline uint256::operator uint64_t() const
{
	if (*this >= uint256(UINT64_MAX))
		return UINT64_MAX;
	uint64_t result;
	memcpy(&result, this, 8);
	return result;
}
inline bool uint256::Is8ByteSafe() const
{
	bn* self = THIS;
	static_assert(sizeof(DTYPE) == 4);
	static_assert(BN_ARRAY_SIZE == 8);
	for (int i = 2; i != BN_ARRAY_SIZE; ++i)
		if (self->array[i])
			return false;
	int64_t test;
	memcpy(&test, this, 8);
	if (test < 0)
		return false; // only report 63byte unsigned values as safe!
	return true;
}

inline uint256& uint256::operator++()
{
	bignum_inc(THIS);
	return *this;
}
inline uint256 uint256::operator++(int)
{
	uint256 copy = *this;
	bignum_inc(THIS);
	return copy;
}
inline uint256& uint256::operator--()
{
	bignum_dec(THIS);
	return *this;
}
inline uint256 uint256::operator--(int)
{
	uint256 copy = *this;
	bignum_dec(THIS);
	return copy;
}

inline uint256 uint256::Pow(const uint256& e) const
{
	uint256 r;
	bignum_pow(THIS, B(e), B(r), 0xff);
	return r;
}
inline uint256 uint256::Sqrt() const
{
	uint256 r;
	bignum_isqrt(THIS, B(r));
	return r;
}

//------------------------------------------------------------------------------
inline bool int256::IsNegative() const
{
	static_assert(sizeof(int256::bits) == 32);
	static_assert(sizeof(int256::bits) / sizeof(int256::bits[0]) == 8);
	static_assert(sizeof(int256::bits[0]) == 4);
	return bits[7] & 0x80000000;
}

inline void int256::TwosComplimentInPlace()
{
	bignum_not(THIS, THIS);
	bignum_inc(THIS);
}
inline int256 int256::TwosCompliment() const
{
	int256 r;
	bignum_not(THIS, B(r));
	bignum_inc(B(r));
	return r;
}
inline int256 int256::Abs() const
{
	return IsNegative() ? TwosCompliment() : *this;
}
inline int256 int256::operator-() const
{
	return TwosCompliment();
}

inline int256::int256(const int256& o)
{
	bignum_assign(THIS, B(o));
}
inline int256::int256(int64_t i)
{
	memcpy(bits, &i, 8);
	memset(bits + 2, i < 0 ? 0xFF : 0x00, 32 - 8);
}

inline int256 int256::operator+ (const int256& o) const
{
	int256 r;
	bignum_add(THIS, B(o), B(r));
	return r;
}
inline int256& int256::operator+=(const int256& o)
{
	bignum_add(THIS, B(o), THIS);
	return *this;
}
inline int256 int256::operator- (const int256& o) const
{
	int256 r;
	bignum_sub(THIS, B(o), B(r));
	return r;
}
inline int256& int256::operator-=(const int256& o)
{
	bignum_sub(THIS, B(o), THIS);
	return *this;
}
inline int256 int256::operator* (const int256& o) const
{
	bool n1 = IsNegative();
	bool n2 = o.IsNegative();
	int256 r;
	if (!n1 && !n2)
		bignum_mul(THIS, B(o), B(r));
	else if (n1 && n2)
	{
		int256 a = TwosCompliment();
		int256 b = o.TwosCompliment();
		bignum_mul(B(a), B(b), B(r));
	}
	else if (n1)
	{
		int256 a = TwosCompliment();
		bignum_mul(B(a), B(o), B(r));
		r.TwosComplimentInPlace();
	}
	else
	{
		CarbonAssert(n2);
		int256 b = o.TwosCompliment();
		bignum_mul(THIS, B(b), B(r));
		r.TwosComplimentInPlace();
	}
	return r;
}
inline int256& int256::operator*=(const int256& o)
{
	return *this = (*this * o);
}
inline int256 int256::operator/ (const int256& o) const
{
	bool n1 = IsNegative();
	bool n2 = o.IsNegative();
	int256 r;
	if (!n1 && !n2)
		bignum_div(THIS, B(o), B(r));
	else if (n1 && n2)
	{
		int256 a = TwosCompliment();
		int256 b = o.TwosCompliment();
		bignum_div(B(a), B(b), B(r));
	}
	else if (n1)
	{
		int256 a = TwosCompliment();
		bignum_div(B(a), B(o), B(r));
		r.TwosComplimentInPlace();
	}
	else
	{
		CarbonAssert(n2);
		int256 b = o.TwosCompliment();
		bignum_div(THIS, B(b), B(r));
		r.TwosComplimentInPlace();
	}
	return r;
}
inline int256& int256::operator/=(const int256& o)
{
	return *this = (*this / o);
}

inline int256 int256::operator~() const
{
	int256 r;
	bignum_not(THIS, B(r));
	return r;
}
inline int256 int256::operator& (const int256& o) const
{
	int256 r;
	bignum_and(THIS, B(o), B(r));
	return r;
}
inline int256& int256::operator&=(const int256& o)
{
	bignum_and(THIS, B(o), THIS);
	return *this;
}
inline int256 int256::operator| (const int256& o) const
{
	int256 r;
	bignum_or(THIS, B(o), B(r));
	return r;
}
inline int256& int256::operator|=(const int256& o)
{
	bignum_or(THIS, B(o), THIS);
	return *this;
}
inline int256 int256::operator^ (const int256& o) const
{
	int256 r;
	bignum_xor(THIS, B(o), B(r));
	return r;
}
inline int256& int256::operator^=(const int256& o)
{
	bignum_xor(THIS, B(o), THIS);
	return *this;
}

inline int256 int256::operator<< (int nbits) const
{
	int256 r;
	bignum_lshift(THIS, B(r), nbits);
	return r;
}
inline int256& int256::operator<<=(int nbits)
{
	bignum_lshift(THIS, THIS, nbits);
	return *this;
}

inline int int256::Compare(const int256& o) const
{
	bool n1 = IsNegative();
	bool n2 = o.IsNegative();
	if (n1 && !n2)
		return -1;
	if (!n1 && n2)
		return 1;
	if (n1 && n2)
		return -bignum_cmp(B(TwosCompliment()), B(o.TwosCompliment()));
	return bignum_cmp(THIS, B(o));
}
inline bool int256::operator!() const
{
	return bignum_is_zero(THIS);
}
inline int256::operator bool() const
{
	return !bignum_is_zero(THIS);
}
inline int256::operator int64_t() const
{
	int64_t result;
	if (*this <= int256(INT64_MIN))
		result = INT64_MIN;
	if (*this >= int256(INT64_MAX))
		result = INT64_MAX;
	else
		memcpy(&result, bits, 8);
	return result;
}
inline bool int256::Is8ByteSafe() const
{
	bn* self = THIS;
	static_assert(sizeof(DTYPE) == 4);
	static_assert(BN_ARRAY_SIZE == 8);
	uint32_t expected = IsNegative() ? 0xFFFFFFFFU : 0x0;
	for (int i = 2; i != BN_ARRAY_SIZE; ++i)
		if (self->array[i] != expected)
			return false;
	if (!IsNegative())
	{
		int64_t test;
		memcpy(&test, this, 8);
		if (test < 0)
			return false; // only report 63byte unsigned values as safe!
	}
	else
	{
		int64_t test;
		memcpy(&test, this, 8);
		if (test >= 0)
			return false;
	}
	return true;
}

inline int256& int256::operator++()
{
	bignum_inc(THIS);
	return *this;
}
inline int256 int256::operator++(int)
{
	int256 copy = *this;
	bignum_inc(THIS);
	return copy;
}
inline int256& int256::operator--()
{
	bignum_dec(THIS);
	return *this;
}
inline int256 int256::operator--(int)
{
	int256 copy = *this;
	bignum_dec(THIS);
	return copy;
}

//------------------------------------------------------------------------------

inline uint256 uint256::FromBytes(const ByteView& data)
{
	if (!data.length)
		return uint256(0);
	CarbonAssert(data.length <= 32 || (data.length == 33 && data.bytes[32] == 0x00));
	uint256 i;
	uint8_t* dst = (uint8_t*)&i;
	if (data.length < 32)
		memset(dst, 0, 32);
	memcpy(dst, data.bytes, data.length);
	return i;
}
inline int256 int256::FromBytes(const ByteView& data)
{
	if (!data.length)
		return int256(0);
	CarbonAssert(data.length <= 32 || (data.length == 33 && ((data.bytes[32] == 0x00 && !(data.bytes[31] & 0x80)) || (data.bytes[32] == 0xFF && (data.bytes[31] & 0x80)))));
	uint8_t fill = (data.bytes[data.length - 1] & 0x80) ? 0xFF : 0;
	int256 i;
	uint8_t* dst = (uint8_t*)&i;
	if (data.length < 32)
		memset(dst, fill, 32);
	memcpy(dst, data.bytes, data.length);
	return i;
}
inline intx intx::FromBytes(const ByteView& data, bool isSigned)
{
	if (!data.length)
		return intx::Zero();
	if (data.length <= 8)
	{
		int64_t i = (isSigned && (data.bytes[data.length - 1] & 0x80)) ? ~int64_t(0) : 0;
		memcpy(&i, data.bytes, data.length);
		return intx(i);
	}
	if (data.length <= 33)
	{
		if (isSigned)
			return int256::FromBytes(data);
		else
			return uint256::FromBytes(data);
	}
	else
	{
		CarbonAssert(false);
		int256 i;
		uint8_t* dst = (uint8_t*)&i;
		memset(dst, 0xFF, 32);
		return i;
	}
}

inline std::string intx::ToString() const
{
	if (isBig)
		return big.Signed().ToString();
	char buf[128];
	snprintf(buf, ARRAY_SIZE(buf), "%" PRId64, normal);
	return buf;
}

inline std::string intx::ToStringUnsigned() const
{
	if (isBig)
		return big.ToString();
	char buf[128];
	snprintf(buf, ARRAY_SIZE(buf), "%" PRIu64, normal);
	return buf;
}

//------------------------------------------------------------------------------
#undef THIS
