#ifndef OLIGOFAR_CHASHPARAM__HPP
#define OLIGOFAR_CHASHPARAM__HPP

#include "cbithacks.hpp"

BEGIN_OLIGOFAR_SCOPES

class CHashParam
{
public:
    CHashParam( unsigned winLen, unsigned wordLen, unsigned winMask );

    int GetOffset() const { return m_offset; }

    int GetWindowLength() const { return m_winLen; }
    int GetWordLength(int i) const { return m_wordLen[i]; }
    int GetPackedSize(int i) const { return m_wordLen[i]; } // TODO: change when packing is implemented

    template<int bitsPerBase, class uint>
    uint PackWord(int i, uint word) const { return word; } // TODO: implement packing

protected:
    Uint4 m_winMask;
    Uint1 m_winLen;
    Uint1 m_offset;
    Uint1 m_wordLen[2];
    Uint1 m_wordSize[2];
    Uint4 m_wordMask[2];
    Uint4 m_wordGaps[2];
};

inline CHashParam::CHashParam( unsigned winLen, unsigned wordLen, unsigned winMask ) :
    m_winMask( winMask & ((1 << m_winLen) - 1) ),
    m_winLen( winLen ),
    m_offset( m_winLen - min( winLen, wordLen ) )
{
    if( m_offset ) {
        m_wordLen[0] = m_wordLen[1] = wordLen;
        m_wordMask[0] = m_winMask & ((1 << m_wordLen[0]) - 1);
        m_wordMask[1] = (m_winMask >> m_offset) & ((1 << m_wordLen[1]) - 1);
        m_wordSize[0] = CBitHacks::BitCount4( m_wordMask[0] );
        m_wordSize[1] = CBitHacks::BitCount4( m_wordMask[1] );
        // word lengths should be same to make simple hash 
        // value computation for reverse strand
    } else {
        m_wordLen[0] = m_winLen;
        m_wordLen[1] = 0;
        m_wordSize[0] = CBitHacks::BitCount4( m_winMask );
        m_wordSize[1] = 0;
        m_wordMask[0] = m_winMask;
        m_wordMask[1] = 0;
    }
    m_wordGaps[0] = (~m_wordMask[0]) & ((1 << m_wordLen[0]) - 1);
    m_wordGaps[1] = (~m_wordMask[1]) & ((1 << m_wordLen[1]) - 1);
}

END_OLIGOFAR_SCOPES

#endif
