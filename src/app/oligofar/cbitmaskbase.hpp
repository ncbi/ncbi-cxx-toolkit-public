#ifndef OLIGOFAR__CBITMASKBASE__HPP
#define OLIGOFAR__CBITMASKBASE__HPP

#include "cbithacks.hpp"

BEGIN_OLIGOFAR_SCOPES

class CBitmaskBase
{
public:
    CBitmaskBase( int maxamb, int wsize, int wstep ) 
        : m_version( 0 ), m_size(0), m_data(0), m_maxAmb( maxamb ),
          m_wSize( wsize ), m_wStep( wstep ), m_wLength( wsize ), 
          m_wPattern( CBitHacks::WordFootprint<Uint4>( wsize ) ), 
          m_pattern2na( CBitHacks::WordFootprint<Uint8>( 2*wsize ) )
//          m_pattern4na( CBitHacks::WordFootprint<UintH>( 4*wsize ) ) 
    {}
    enum EVersion { eVersion_0_0_0, eVersion_0_1_0, eVersion_COUNT };

    const char * Signature(int ver = eVersion_0_0_0) const { 
        static char * versions[] = { 
            "oligoFAR.hash.word.bit.mask:0.0.0\0\0\0\0",
            "oligoFAR.hash.word.bit.mask:0.1.0\0\0\0\0",
        };
        return versions[ver];
    }
    Uint4 SignatureSize() const { return (strlen( Signature() ) + 1 + 3)&~3; }

    Uint4 GetMaxAmb() const { return m_maxAmb; }
    Uint4 GetWordSize() const { return m_wSize; }
    Uint4 GetWordStep() const { return m_wStep; }
    Uint4 GetWindowLength() const { return m_wLength; }
    Uint4 GetWindowPattern() const { return m_wPattern; }

    // should recompute word length and size; should not be called 
    void SetWindowPattern( Uint4 pattern );
protected:
    void x_SetWordSize( int size, bool allocate );

protected:
    int     m_version;
    Uint8   m_size;
    Uint4 * m_data;
    Uint4   m_maxAmb;
    Uint4   m_wSize;
    Uint4   m_wStep;
    Uint4   m_wLength;
    Uint4   m_wPattern;
    Uint8   m_pattern2na;
//    UintH   m_pattern4na;
};

inline void CBitmaskBase::x_SetWordSize( int wsize, bool allocate ) 
{
    m_wSize = wsize;
    m_size = ((UintH(1) << (2*wsize))/Uint8(32)).GetLo();
    if( allocate ) {
        delete [] m_data;   
        m_data = new Uint4[m_size];
        fill( m_data, m_data + m_size, 0 );
    }
}

inline void CBitmaskBase::SetWindowPattern( Uint4 pattern ) 
{
    while( (pattern&1) == 0 ) pattern>>=1;
    m_wPattern = pattern;
    m_wLength = CBitHacks::LargestBitPos( pattern ) + 1;
    if( m_wLength == Uint4( 0 ) )
        THROW( logic_error, "CBitmaskBase::SetWindowPattern(0) is invalid call " << hex << DISPLAY( pattern ) );
    int wsize = CBitHacks::BitCount4( pattern );
    /*
    if( m_version < eVersion_0_1_0 && (int)m_wLength != wsize ) 
        THROW( logic_error, "CBitmaskBase::SetWindowPattern() is not supported for version " << Signature(eVersion_0_0_0) << DISPLAY( m_wLength ) << DISPLAY( wsize ) << hex << DISPLAY( pattern ) );
        */
    if( (int)m_wLength != wsize && m_version < eVersion_0_1_0 ) m_version = eVersion_0_1_0;
    x_SetWordSize( wsize, false );
    m_pattern2na = CBitHacks::DuplicateBits<Uint8>( m_wPattern );
    //m_pattern4na = CBitHacks::DuplicateBits<UintH>( m_pattern2na );
    if( CBitHacks::PackWord<Uint8>(  0xcba7cba7cba7cba7,  0xcb8c3473cb8c3473 ) != 0xfd0bfd0b )
        THROW( logic_error, "Oops with CBitHacks::PackWord<Uint8>(  0xcba7cba7cba7cba7,  0xcb8c3473cb8c3473 ) != 0xfd0bfd0b 0x" 
                << hex << CBitHacks::PackWord<Uint8>(  0xcba7cba7cba7cba7,  0xcb8c3473cb8c3473 ) );
    //cerr << "CBitmaskBase::SetWindowPattern( " << hex << pattern << "): " << dec << DISPLAY( m_wLength ) << DISPLAY( m_wSize ) << hex << DISPLAY( m_wPattern ) << DISPLAY( m_pattern2na ) << "\n";
}

END_OLIGOFAR_SCOPES

#endif
