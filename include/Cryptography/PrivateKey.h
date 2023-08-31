#pragma once

#include "../Security/SecureMemory.h"

namespace phantasma {

class PrivateKey
{
public:
	constexpr static int Length = 32;

	PrivateKey()
		: m_data(Length, 0)
		, m_isNull(true)
	{
	}
	PrivateKey( const Byte* privateKey, int privateKeyLength )
		: m_data(Length, privateKeyLength == Length ? privateKey : 0)
	{
		m_isNull = true;
		if(privateKeyLength != Length)
		{
			PHANTASMA_EXCEPTION("privateKey should have length 32");
			return;
		}

		for(int i=0; i!= Length; ++i)
		{
			if(privateKey[i] != 0)
			{
				m_isNull = false;
				break;
			}
		}
	}

	SecureByteReader Read() const { return m_data.Read(); }
	SecureByteWriter Write()      { return m_data.Write(); }
	bool IsNull() const { return m_isNull; }
private:
	SecureByteArray m_data;
	bool m_isNull;
};
}
