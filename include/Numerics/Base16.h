#pragma once
#ifndef PHANTASMA_API_INCLUDED
#error "Configure and include PhantasmaAPI.h first"
#endif 

#include <ctype.h>
#include "../Utils/ByteArrayUtils.h"

namespace phantasma {
namespace Base16 {

constexpr Char Alphabet[] = PHANTASMA_LITERAL("0123456789ABCDEF");
inline int AlphabetIndexOf( Char in )
{
	for( const Char* c = Alphabet; *c; ++c )
		if( *c == in )
			return (int)(c - Alphabet);
	return -1;
}

inline int RequiredCharacters( int numBytes )//does not include a null terminator
{
	return numBytes * 2;
}

inline bool IsValid(const Char* sz, int inputLength = 0)
{
	if (!sz || inputLength < 0)
	{
		PHANTASMA_EXCEPTION("invalid usage");
		return false;
	}

	if (inputLength == 0)
	{
		inputLength = (int)PHANTASMA_STRLEN(sz);
		if (inputLength == 0)
		{
			return false;
		}
	}

	if (inputLength >= 2 && sz[0] == '0' && (sz[1] == 'x' || sz[1] == 'X'))
	{
		sz += 2;
		inputLength -= 2;
		if (inputLength == 0)
		{
			return false;
		}
	}

	// if (inputLength % 2 == 1)
	// {
	// 	return 0;
	// }

	int length = (inputLength + 1) / 2;
	int lengthDown = inputLength / 2;

	if(lengthDown != length)
	{
		int B = AlphabetIndexOf((Char)toupper(sz[0]));
		if(B < 0)
		{
			PHANTASMA_EXCEPTION("invalid character");
			return false;
		}
		sz++;
	}

	for (int i = 0; i < lengthDown; i++)
	{
		int A = AlphabetIndexOf((Char)toupper(sz[i * 2 + 0]));
		int B = AlphabetIndexOf((Char)toupper(sz[i * 2 + 1]));

		if (A < 0 || B < 0)
		{
			PHANTASMA_EXCEPTION("invalid character");
			return false;
		}
	}

	return true;
}
inline bool IsValid(const String& str)
{
	return IsValid(str.c_str(), (int)str.length());
}

inline int Decode(Byte* output, int outputLength, const Char* sz, int inputLength=0)
{
	if(!sz || inputLength < 0 || outputLength < 0)
	{
		PHANTASMA_EXCEPTION("invalid usage");
		return 0;
	}

	if(inputLength == 0)
	{
		inputLength = (int)PHANTASMA_STRLEN(sz);
	}

	if( inputLength == 0 )
	{
		PHANTASMA_EXCEPTION("string cannot be empty");
		return 0;
	}

	if (inputLength >= 2 && sz[0] == '0' && (sz[1] == 'x' || sz[1] == 'X'))
	{
		sz += 2;
		inputLength -= 2;
		if( inputLength == 0 )
		{
			PHANTASMA_EXCEPTION("string cannot be empty");
			return 0;
		}
	}

	// if( inputLength % 2 == 1 )
	// {
	// 	PHANTASMA_EXCEPTION("string length must be even");
	// 	return 0;
	// }

	int length = (inputLength + 1) / 2;
	int lengthDown = inputLength / 2;

	if( !output )
		return length;
	
	if(lengthDown != length)
	{
		length = PHANTASMA_MIN(length, outputLength);
		lengthDown = length-1;

		int B = AlphabetIndexOf((Char)toupper(sz[0]));
		if(B < 0)
		{
			PHANTASMA_EXCEPTION("invalid character");
			return 0;
		}
		output[0] = (Byte)B;

		sz++;
		output++;
	}
	else
	{
		lengthDown = length = PHANTASMA_MIN(length, outputLength);
	}

	for (int i = 0; i < lengthDown; i++)
	{
		int A = AlphabetIndexOf((Char)toupper(sz[i * 2 + 0]));
		int B = AlphabetIndexOf((Char)toupper(sz[i * 2 + 1]));

		if(A < 0 || B < 0)
		{
			PHANTASMA_EXCEPTION("invalid character");
			return i;
		}

		output[i] = (Byte)(A * 16 + B);
	}

	return length;
}
inline ByteArray Decode(const Char* input, int inputLength=0)
{
	if(inputLength == 0)
	{
		inputLength = (int)PHANTASMA_STRLEN(input);
	}
	int length = Decode( 0, 0, input, inputLength );
	ByteArray result;
	if( length > 0 )
	{
		result.resize(length);
		int decoded = Decode( &result.front(), length, input, inputLength );
		if( decoded != length )
			return ByteArray{};
	}
	return result;
}
inline ByteArray Decode(const String& input)
{
	return Decode(input.c_str(), (int)input.length());
}

inline String Encode(const Byte* input, int length, bool lowercase=false)
{
	if(!input || length <= 0)
	{
		PHANTASMA_EXCEPTION("invalid argument");
		return String();
	}

	PHANTASMA_VECTOR<Char> c;
	c.resize(length * 2 + 1);
	int b;
	for(int i = 0; i < length; i++)
	{
		b = input[i] >> 4;
		c[i * 2] = (Char)(55 + b + (((b - 10) >> 31) & -7));
		b = input[i] & 0xF;
		c[i * 2 + 1] = (Char)(55 + b + (((b - 10) >> 31) & -7));
	}
	c[c.size()-1] = '\0';

	if(lowercase)
	{
		for( Char* i = &c.front(); *i != '\0'; ++i )
			*i = (Char)tolower(*i);
	}

	return String(&c.front());
}

inline String Encode(const ByteArray& input)
{
	if(input.empty())
	{
		PHANTASMA_EXCEPTION("invalid argument");
		return String();
	}
	return Encode(&input.front(), (int)input.size());
}

}}