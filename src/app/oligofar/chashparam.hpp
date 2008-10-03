#ifndef OLIGOFAR_CHASHPARAM__HPP
#define OLIGOFAR_CHASHPARAM__HPP

#include "cbithacks.hpp"

BEGIN_OLIGOFAR_SCOPES

class CHashParam
{
public:
	enum EHashType {
		eHash_vector,
		eHash_arraymap,
		eHash_multimap
	};

    CHashParam( EHashType hashType, unsigned winLen );

    EHashType GetHashType() const { return m_hashType; }

    int GetOffset() const { return m_offset; }
    int GetWindowLength() const { return m_winLen; }
    int GetWordLength() const { return m_wordLen; }

    bool SingleHash() const { return m_winLen == m_wordLen; }
    bool DoubleHash() const { return m_winLen != m_wordLen; }

protected:
    EHashType m_hashType;
    Uint1 m_winLen;
    Uint1 m_wordLen;
    Uint1 m_offset;
};

inline CHashParam::CHashParam( EHashType hashType, unsigned winLen ) :
    m_hashType( hashType ),
    m_winLen( winLen ),
    m_wordLen( winLen ),
    m_offset( 0 )
{
    ASSERT( m_winLen < 31 );
    if( hashType == eHash_vector ) {
        if( m_winLen > 13 ) {
            m_wordLen = max( 12, ( m_winLen + 1 ) / 2 );
            m_offset = m_winLen - m_wordLen;
        }
    } else {
        if( m_winLen > 15 ) {
            m_wordLen = max( 15, (m_winLen + 1 ) / 2 );
            m_offset = m_winLen - m_wordLen;
        }
    }
}

END_OLIGOFAR_SCOPES

#endif
