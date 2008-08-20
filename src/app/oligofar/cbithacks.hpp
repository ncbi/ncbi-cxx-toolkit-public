#ifndef OLIGOFAR_BITHACKS__HPP
#define OLIGOFAR_BITHACKS__HPP

#include "defs.hpp"
#include <sstream>
#include <iomanip>
#include <iterator>

BEGIN_OLIGOFAR_SCOPES

// Presumably optimized bit operations... but sometimes optimizer makes strange things

class CBitHacks
{
public:
    enum { kBitsPerByte = 8, kBitsPerBase_ncbi2na = 2 };
    
    template<class word> 
    static word WordFootprint( int wordsize );
    
//      template<class word> 
//      static int BitCount( word x );
    static int BitCount1( Uint1 x );
    static int BitCount2( Uint2 x );
    static int BitCount4( Uint4 x );
    static int BitCount8( Uint8 x );

//     static int BitCount( Uint1 );
//     static int BitCount( Uint2 );
//     static int BitCount( Uint4 );
//     static int BitCount( Uint8 );
    
    // following functions have explicit names for purpose - 
    // it makes simpler to explicitely say compiler what you mean
    
    // can be used as reverse complement for ncbi4na
    static Uint1 ReverseBits1( Uint1 x );
    static Uint2 ReverseBits2( Uint2 x );
    static Uint4 ReverseBits4( Uint4 x );
    static Uint8 ReverseBits8( Uint8 x );

    // can be used as reverse for ncbi2na, 
    // and ~ may be used as complement to ncbi2na
    static Uint1 ReverseBitPairs1( Uint1 x );
    static Uint2 ReverseBitPairs2( Uint2 x );
    static Uint4 ReverseBitPairs4( Uint4 x );
    static Uint8 ReverseBitPairs8( Uint8 x );

    template<class word, class output_iterator>
    static output_iterator AsBits( word m, int w, output_iterator );

    template<class word>
    static string AsBits( word m, int w = kBitsPerByte*sizeof(word) );

    template<class word>
    static word PackWord( word data, word mask );
    
    template<class word>
    static word DuplicateBits( word in );

    enum EMagic { // did not work correct in templates with -O9 :-[
        kPattern_02 = 0x5555555555555555LL,
        kPattern_04 = 0x3333333333333333LL,
        kPattern_08 = 0x0f0f0f0f0f0f0f0fLL,
        kPattern_16 = 0x00ff00ff00ff00ffLL,
        kPattern_32 = 0x0000ffff0000ffffLL,
        kPattern_64 = 0x00000000ffffffffLL,
        kPattern_ff = 0xffffffffffffffffLL
    };
protected:
    template<class word> static word x_SwapBits( word w, int shift, EMagic mask );
    template<class word> static word x_SwapBits( word w, int shift, word mask );
    template<class word> static word x_SwapBits( word w, int shift );
    template<class word> static word x_Count( word w, int c );
    template<class word> static word x_Mask( int c );
};

template <class word>
inline word CBitHacks::WordFootprint( int wordsize ) 
{ 
    return ~((~word(0)) << (wordsize));
}

template<class word> 
inline word CBitHacks::x_Mask( int c )
{
    return ~word(0) / ( ( word(1) << ( 1 << c ) ) + 1 );
}

template<class word> 
inline word CBitHacks::x_Count( word w, int c )
{
    return ( w & x_Mask<word>(c) ) + (w >> (1 << c)) & x_Mask<word>(c);
}

//         kPattern_01 = ~Uint8(0),                       // 0xffffffffffffffff
//         kPattern_02 = (~Uint8(0)) / 3,                 // 0x5555555555555555
//         kPattern_04 = (~Uint8(0)) / 5,                 // 0x3333333333333333
//         kPattern_08 = (~Uint8(0)) / 255 * 15,          // 0x0f0f0f0f0f0f0f0f
//         kPattern_16 = (~Uint8(0)) / 65535 * 255,       // 0x00ff00ff00ff00ff
//         kPattern_32 = (~Uint8(0)) / 4294967295 * 65535,// 0x0000ffff0000ffff
// template <class word>
// inline int CBitHacks::BitCount( word x ) 
// {
//     x -= ( x >> 1 ) & word(kPattern_02);                                     // 0+3=3
//     x = ( x & word(kPattern_04) ) + ( (x >> 2) & word(kPattern_04) );              // 3+5=8
//     x = ( x + (x >> 4) ) & word(kPattern_08);                                // 8+4=12
//     return (x * word(kPattern_16)) >> ((sizeof( word ) - 1) * word(kBitsPerByte)); // 12+2=14
// }

// inline int CBitHacks::BitCount( Uint1 x ) 
// {
//     x = x_Count( x, 0 );
//     x = x_Count( x, 1 );
//     x = x_Count( x, 2 );
//     return x;
// }

// inline int CBitHacks::BitCount( Uint2 x ) 
// {
//     x = x_Count( x, 0 );
//     x = x_Count( x, 1 );
//     x = x_Count( x, 2 );
//     x = x_Count( x, 3 );
//     return x;
// }

// inline int CBitHacks::BitCount( Uint4 x ) 
// {
//     x = x_Count( x, 0 );
//     x = x_Count( x, 1 );
//     x = x_Count( x, 2 );
//     x = x_Count( x, 3 );
//     x = x_Count( x, 4 );
//     return x;
// }

// inline int CBitHacks::BitCount( Uint8 x ) 
// {
//     x = x_Count( x, 0 );
//     x = x_Count( x, 1 );
//     x = x_Count( x, 2 );
//     x = x_Count( x, 3 );
//     x = x_Count( x, 4 );
//     x = x_Count( x, 5 );
//     return x;
// }

inline int CBitHacks::BitCount1( Uint1 x ) 
{
    return 
        "\x0\x1\x1\x2""\x1\x2\x2\x3""\x1\x2\x2\x3""\x2\x3\x3\x4" // 0x00-0x0f
        "\x1\x2\x2\x3""\x2\x3\x3\x4""\x2\x3\x3\x4""\x3\x4\x4\x5" // 0x10-0x1f
        "\x1\x2\x2\x3""\x2\x3\x3\x4""\x2\x3\x3\x4""\x3\x4\x4\x5" // 0x20-0x2f
        "\x2\x3\x3\x4""\x3\x4\x4\x5""\x3\x4\x4\x5""\x4\x5\x5\x6" // 0x30-0x3f 
        "\x1\x2\x2\x3""\x2\x3\x3\x4""\x2\x3\x3\x4""\x3\x4\x4\x5" // 0x40-0x4f
        "\x2\x3\x3\x4""\x3\x4\x4\x5""\x3\x4\x4\x5""\x4\x5\x5\x6" // 0x50-0x5f 
        "\x2\x3\x3\x4""\x3\x4\x4\x5""\x3\x4\x4\x5""\x4\x5\x5\x6" // 0x60-0x6f 
        "\x3\x4\x4\x5""\x4\x5\x5\x6""\x4\x5\x5\x6""\x5\x6\x6\x7" // 0x70-0x7f 
        "\x1\x2\x2\x3""\x2\x3\x3\x4""\x2\x3\x3\x4""\x3\x4\x4\x5" // 0x80-0x8f
        "\x2\x3\x3\x4""\x3\x4\x4\x5""\x3\x4\x4\x5""\x4\x5\x5\x6" // 0x90-0x9f 
        "\x2\x3\x3\x4""\x3\x4\x4\x5""\x3\x4\x4\x5""\x4\x5\x5\x6" // 0xa0-0xaf 
        "\x3\x4\x4\x5""\x4\x5\x5\x6""\x4\x5\x5\x6""\x5\x6\x6\x7" // 0xb0-0xbf 
        "\x2\x3\x3\x4""\x3\x4\x4\x5""\x3\x4\x4\x5""\x4\x5\x5\x6" // 0xc0-0xcf 
        "\x3\x4\x4\x5""\x4\x5\x5\x6""\x4\x5\x5\x6""\x5\x6\x6\x7" // 0xd0-0xdf 
        "\x3\x4\x4\x5""\x4\x5\x5\x6""\x4\x5\x5\x6""\x5\x6\x6\x7" // 0xe0-0xef 
        "\x4\x5\x5\x6""\x5\x6\x6\x7""\x5\x6\x6\x7""\x6\x7\x7\x8" // 0xf0-0xff 
        [x];
}

inline int CBitHacks::BitCount2( Uint2 x )
{
    return BitCount1( x ) + BitCount1( x >> 8 );
}

inline int CBitHacks::BitCount4( Uint4 x )
{
    return BitCount2( x ) + BitCount2( x >> 16 );
}

inline int CBitHacks::BitCount8( Uint8 x )
{
    return BitCount4( x ) + BitCount4( x >> 32 );
}

template<class word> 
inline word CBitHacks::x_SwapBits( word w, int shift, EMagic mask )
{
    return x_SwapBits( w, shift, word(mask) ); 
}


template<class word> 
inline word CBitHacks::x_SwapBits( word w, int shift, word mask )
{
    return ((w >> shift) & mask) | ((w & mask) << shift);
}

template<class word> 
inline word CBitHacks::x_SwapBits( word w, int shift )
{
    return (w >> shift) | (w << shift);
}

inline Uint1 CBitHacks::ReverseBits1( Uint1 x ) 
{
    x = x_SwapBits( x, 0x01, kPattern_02 );
    x = x_SwapBits( x, 0x02, kPattern_04 );
    return x_SwapBits( x, 0x04 );
}

inline Uint2 CBitHacks::ReverseBits2( Uint2 x ) 
{
    x = x_SwapBits( x, 0x01, kPattern_02 );
    x = x_SwapBits( x, 0x02, kPattern_04 );
    x = x_SwapBits( x, 0x04, kPattern_08 );
    return x_SwapBits( x, 0x08 );
}

inline Uint4 CBitHacks::ReverseBits4( Uint4 x ) 
{
    x = x_SwapBits( x, 0x01, kPattern_02 );
    x = x_SwapBits( x, 0x02, kPattern_04 );
    x = x_SwapBits( x, 0x04, kPattern_08 );
    x = x_SwapBits( x, 0x08, kPattern_16 );
    return x_SwapBits( x, 0x10 );
}

inline Uint8 CBitHacks::ReverseBits8( Uint8 x ) 
{
    x = x_SwapBits( x, 0x01, kPattern_02 );
    x = x_SwapBits( x, 0x02, kPattern_04 );
    x = x_SwapBits( x, 0x04, kPattern_08 );
    x = x_SwapBits( x, 0x08, kPattern_16 );
    x = x_SwapBits( x, 0x10, kPattern_32 );
    return x_SwapBits( x, 0x20 );
}

inline Uint1 CBitHacks::ReverseBitPairs1( Uint1 x ) 
{
    x = x_SwapBits( x, 0x02, kPattern_04 );
    return x_SwapBits( x, 0x04 );
}

inline Uint2 CBitHacks::ReverseBitPairs2( Uint2 x ) 
{
    x = x_SwapBits( x, 0x02, kPattern_04 );
    x = x_SwapBits( x, 0x04, kPattern_08 );
    return x_SwapBits( x, 0x08 );
}

inline Uint4 CBitHacks::ReverseBitPairs4( Uint4 x ) 
{
    x = x_SwapBits( x, 0x02, kPattern_04 );
    x = x_SwapBits( x, 0x04, kPattern_08 );
    x = x_SwapBits( x, 0x08, kPattern_16 );
    return x_SwapBits( x, 0x10 );
}

inline Uint8 CBitHacks::ReverseBitPairs8( Uint8 x ) 
{
    x = x_SwapBits( x, 0x02, kPattern_04 );
    x = x_SwapBits( x, 0x04, kPattern_08 );
    x = x_SwapBits( x, 0x08, kPattern_16 );
    x = x_SwapBits( x, 0x10, kPattern_32 );
    return x_SwapBits( x, 0x20 );
}

template<class word, class output_iterator>
inline output_iterator CBitHacks::AsBits( word m,  int w, output_iterator dest ) 
{
    for( word k = word(1) << (w-1); w > 0; (--w), (k >>= 1) ) {
        *dest++ = (m & k) ? '1':'0';
    }
    return dest;
}

template<class word>
inline string CBitHacks::AsBits( word m, int w )
{
    ostringstream o;
    AsBits( m, w, ostream_iterator<char>( o ) );
    return o.str();
}

template<class word>
inline word CBitHacks::PackWord( word data, word mask ) // TODO: optimize
{
    word ret = 0;
    word bit = 1;
    
    while( data && ( bit <= mask ) ) {
        if( ( mask & bit ) == 0 ) {
            data >>= 1;
            mask >>= 1;
        } else {
            ret |= bit & data;
            bit <<= 1;
        }
    }
    return ret;
}

template<>
inline unsigned short CBitHacks::DuplicateBits<Uint2>( Uint2 in ) 
{
    in &= 0xff;
    return 
        (in & 0x01) * 0x01 * 0x03 |
        (in & 0x02) * 0x02 * 0x03 |
        (in & 0x04) * 0x04 * 0x03 |
        (in & 0x08) * 0x08 * 0x03 |
        (in & 0x10) * 0x10 * 0x03 |
        (in & 0x20) * 0x20 * 0x03 |
        (in & 0x40) * 0x40 * 0x03 |
        (in & 0x80) * 0x80 * 0x03;
}

template<>
inline Uint4 CBitHacks::DuplicateBits<Uint4>( Uint4 in ) 
{
    return DuplicateBits<Uint2>( in ) | ( ( (Uint4)DuplicateBits<Uint2>( in >> 8 ) ) << 16 );
}

template<>
inline Uint8 CBitHacks::DuplicateBits<Uint8>( Uint8 in ) 
{
    return DuplicateBits<Uint4>( in ) | ( ( (Uint8)DuplicateBits<Uint4>( in >> 16 ) ) << 32 );
}

END_OLIGOFAR_SCOPES

#endif
