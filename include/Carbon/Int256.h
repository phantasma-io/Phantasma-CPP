#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "DataCommon.h"

namespace phantasma::carbon {

class ReadView;
class WriteView;

struct int256;
struct uint256;
struct intx;

bool Read(uint256& out, ReadView& reader);
bool Read(int256& out, ReadView& reader);
bool Read(intx& out, ReadView& reader);
void Write(const uint256& in, WriteView& writer);
void Write(const int256& in, WriteView& writer);
void Write(const intx& in, WriteView& writer);

struct uint256
{
	uint256() = default;
	uint256(const uint256&);
	explicit uint256(const int256&);
	explicit uint256(uint64_t);

	      int256& Signed();
	const int256& Signed() const;

	ByteView Bytes() const;

	static uint256 FromBytes(const ByteView&);
	static uint256 FromString(const char*, uint32_t length = 0, uint32_t base = 10, bool* out_error = 0);
	std::string ToString(uint32_t base = 10, const char* customDictionary = 0) const;

	uint256  operator+ (const uint256&) const;
	uint256& operator+=(const uint256&);
	uint256  operator- (const uint256&) const;
	uint256& operator-=(const uint256&);
	uint256  operator* (const uint256&) const;
	uint256& operator*=(const uint256&);
	uint256  operator/ (const uint256&) const;
	uint256& operator/=(const uint256&);
	uint256  operator% (const uint256&) const;
	uint256& operator%=(const uint256&);

	uint256  operator~ () const;
	uint256  operator& (const uint256&) const;
	uint256& operator&=(const uint256&);
	uint256  operator| (const uint256&) const;
	uint256& operator|=(const uint256&);
	uint256  operator^ (const uint256&) const;
	uint256& operator^=(const uint256&);
	uint256  operator<< (int nbits) const;
	uint256& operator<<=(int nbits);
	uint256  operator>> (int nbits) const;
	uint256& operator>>=(int nbits);

	int Compare(const uint256&) const;
	bool operator< (const uint256& o) const { return Compare(o) <  0; }
	bool operator<=(const uint256& o) const { return Compare(o) <= 0; }
	bool operator> (const uint256& o) const { return Compare(o) >  0; }
	bool operator>=(const uint256& o) const { return Compare(o) >= 0; }
	bool operator==(const uint256& o) const { return Compare(o) == 0; }
	bool operator!=(const uint256& o) const { return Compare(o) != 0; }
	bool operator!() const;
	explicit operator bool() const;
	explicit operator uint64_t() const;
	bool Is8ByteSafe() const;

	uint256& operator++();
	uint256  operator++(int);
	uint256& operator--();
	uint256  operator--(int);

	uint256 Pow(const uint256& e) const;
	uint256 Sqrt() const;
private:
	uint32_t bits[8];
};

struct int256
{
	int256() = default;
	int256(const int256&);
	explicit int256(const uint256&);
	explicit int256(int64_t);

	      uint256& Unsigned();
	const uint256& Unsigned() const;

	ByteView Bytes() const;

	static int256 FromBytes(const ByteView&);
	static int256 FromString(const char*, int length = 0, uint32_t base = 10);
	std::string ToString(uint32_t base = 10, const char* customDictionary = 0, char negative = '-') const;

	int256  Abs() const;
	int256  operator- () const;
	int256  operator+ (const int256&) const;
	int256& operator+=(const int256&);
	int256  operator- (const int256&) const;
	int256& operator-=(const int256&);
	int256  operator* (const int256&) const;
	int256& operator*=(const int256&);
	int256  operator/ (const int256&) const;
	int256& operator/=(const int256&);
	int256  operator% (const int256&) const;
	int256& operator%=(const int256&);

	int256  operator~ () const;
	int256  operator& (const int256&) const;
	int256& operator&=(const int256&);
	int256  operator| (const int256&) const;
	int256& operator|=(const int256&);
	int256  operator^ (const int256&) const;
	int256& operator^=(const int256&);
	int256  operator<< (int nbits) const;
	int256& operator<<=(int nbits);
	int256  operator>> (int nbits) const;
	int256& operator>>=(int nbits);

	int Compare(const int256&) const;
	bool operator< (const int256& o) const { return Compare(o) <  0; }
	bool operator<=(const int256& o) const { return Compare(o) <= 0; }
	bool operator> (const int256& o) const { return Compare(o) >  0; }
	bool operator>=(const int256& o) const { return Compare(o) >= 0; }
	bool operator==(const int256& o) const { return Compare(o) == 0; }
	bool operator!=(const int256& o) const { return Compare(o) != 0; }
	bool operator!() const;
	explicit operator bool() const;
	explicit operator int64_t() const;
	bool Is8ByteSafe() const;
	bool IsNegative() const;

	int256& operator++();
	int256  operator++(int);
	int256& operator--();
	int256  operator--(int);
private:
	uint32_t bits[8];

	inline void TwosComplimentInPlace();
	inline int256 TwosCompliment() const;
};

struct intx_pod;
struct intx
{
	intx() = default;
	intx(const intx& o) { memcpy(this, &o, sizeof(intx)); }
	intx(const int256& o) : isBig(true) { big = o.Unsigned(); }
	intx(const uint256& o) : isBig(true) { big = o; }
	intx(     int o) : isBig(false) { normal = (uint64_t)(int64_t)o; }
	intx(uint64_t o) : isBig(false) { normal = o; }
	intx( int64_t o) : isBig(false) { normal = (uint64_t)o; }

	static intx FromBytes(const ByteView&, bool isSigned = true);
	static intx Zero() { return intx((uint64_t)0); }

	static intx FromString(const char*, uint32_t length = 0, uint32_t base = 10, bool* out_error = 0);
	std::string ToString() const;
	std::string ToStringUnsigned() const;

	union {
		mutable uint256 big;
		uint64_t normal;
	};
	mutable bool isBig;
	bool operator !() const { return isBig ? !big : !normal; }
	explicit operator bool() const { return isBig ? (bool)big : (bool)normal; }
	explicit operator uint64_t() const { return isBig ? (uint64_t)big : normal; }
	explicit operator  int64_t() const { return isBig ? (int64_t)big.Signed() : (int64_t)normal; }
	operator const uint256&() const { return Uint256(); }
	operator const  int256&() const { return Int256(); }

	const uint256& Uint256() const { MakeBig(); return big; }
	const  int256&  Int256() const { MakeBig(); return big.Signed(); }

	//intx& operator+=(const intx& o);
	//intx& operator-=(const intx& o);
	intx operator+(const intx& o) const;
	intx operator-(const intx& o) const;
	bool operator>(const intx& o) const;
	bool operator==(const intx& o) const;

	operator       intx_pod&()       { return *(intx_pod*)this; }
	operator const intx_pod&() const { return *(intx_pod*)this; }
private:
	void MakeBig() const
	{
		if(isBig)
			return;
		isBig = true;
		if( ((int64_t)normal) < 0 )
			big.Signed() = int256((int64_t)normal);
		else
			big = uint256(normal);
	}
};
struct intx_pod
{
	uint64_t data[(sizeof(intx)+7)/8];
	operator       intx&()       { return *(intx*)this; }
	operator const intx&() const { return *(intx*)this; }
	      intx& x()       { return *(intx*)this; }
	const intx& x() const { return *(intx*)this; }
};
static_assert( sizeof(intx) == sizeof(intx_pod) );
static_assert( alignof(intx) == alignof(intx_pod) );

}
