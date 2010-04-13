#ifndef OLIGOFAR_BITHACKS__HPP
#define OLIGOFAR_BITHACKS__HPP

#include "defs.hpp"
#include "uinth.hpp"
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
    static int BitCountH( const UintH x ) { return BitCount8( x.GetHi() ) + BitCount8( x.GetLo() ); }

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
    static UintH ReverseBitsH( UintH x ) { return UintH( ReverseBits8( x.GetLo() ), ReverseBits8( x.GetHi() ) ); }

    // can be used as reverse for ncbi2na, 
    // and ~ may be used as complement to ncbi2na
    static Uint1 ReverseBitPairs1( Uint1 x );
    static Uint2 ReverseBitPairs2( Uint2 x );
    static Uint4 ReverseBitPairs4( Uint4 x );
    static Uint8 ReverseBitPairs8( Uint8 x );
    static UintH ReverseBitPairsH( UintH x ) { return UintH( ReverseBitPairs8( x.GetLo() ), ReverseBitPairs8( x.GetHi() ) ); }

    // can be used as reverse for dibase colorspace represented as 4-channel 1-bit sets, 
    // and ~ may be used as complement to ncbi2na
    static Uint1 ReverseBitQuads1( Uint1 x );
    static Uint2 ReverseBitQuads2( Uint2 x );
    static Uint4 ReverseBitQuads4( Uint4 x );
    static Uint8 ReverseBitQuads8( Uint8 x );
    static UintH ReverseBitQuadsH( UintH x ) { return UintH( ReverseBitQuads8( x.GetLo() ), ReverseBitQuads8( x.GetHi() ) ); }

    // inserts bits 0..(bitsperpos-1) from "bits" at position offset*bitsperpos shifting higher (above offset) part of value left by bitsperpos
    template <class Uint, int bitsperpos, int bits> 
    static Uint InsertBits( const Uint& value, int offset );

    // deletes bitsperpos bits at position offset*bitsperpos, shifting higher (above offset+bitsperpos) part of value right by bitsperpos
    template <class Uint, int bitsperpos>
    static Uint DeleteBits( const Uint& value, int offset );

    template<class word, class output_iterator>
    static output_iterator AsBits( word m, int w, output_iterator );
    template<class output_iterator>
    static output_iterator AsBits( const UintH& m, int w, output_iterator o );

    template<class word>
    static string AsBits( word m, int w = kBitsPerByte*sizeof(word) );

    template<class word>
    static word PackWord( word data, word mask );

    template<class word>
    static int LargestBitPos( word data );
    
    template<class word>
    static word SmallestFootprint( word data );

    template<class word>
    static word DuplicateBits( word in );

#ifdef _WIN32
#define kPattern_02 0x5555555555555555ULL
#define kPattern_04 0x3333333333333333ULL
#define kPattern_08 0x0f0f0f0f0f0f0f0fULL
#define kPattern_16 0x00ff00ff00ff00ffULL
#define kPattern_32 0x0000ffff0000ffffULL
#define kPattern_64 0x00000000ffffffffULL
#define kPattern_ff 0xffffffffffffffffULL
#else
    enum EMagic { // did not work correct in templates with -O9 :-[
        kPattern_02 = 0x5555555555555555ULL,
        kPattern_04 = 0x3333333333333333ULL,
        kPattern_08 = 0x0f0f0f0f0f0f0f0fULL,
        kPattern_16 = 0x00ff00ff00ff00ffULL,
        kPattern_32 = 0x0000ffff0000ffffULL,
        kPattern_64 = 0x00000000ffffffffULL,
        kPattern_ff = 0xffffffffffffffffULL
    };
#endif

protected:
#ifndef _WIN32
    template<class word> static word x_SwapBits( word w, int shift, EMagic mask );
#endif
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
    return BitCount1( Uint1( x ) ) + BitCount1( x >> 8 );
}

inline int CBitHacks::BitCount4( Uint4 x )
{
    return BitCount2( Uint2( x ) ) + BitCount2( x >> 16 );
}

inline int CBitHacks::BitCount8( Uint8 x )
{
    return BitCount4( Uint4( x ) ) + BitCount4( Uint4( x >> 32 ) );
}

#ifndef _WIN32
template<class word> 
inline word CBitHacks::x_SwapBits( word w, int shift, EMagic mask )
{
    return x_SwapBits( w, shift, word(mask) ); 
}
#endif

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
    x = x_SwapBits<Uint1>( x, Uint1( 0x01 ), Uint1( kPattern_02 ) );
    x = x_SwapBits<Uint1>( x, Uint1( 0x02 ), Uint1( kPattern_04 ) );
    return x_SwapBits( x, 0x04 );
}

inline Uint2 CBitHacks::ReverseBits2( Uint2 x ) 
{
    x = x_SwapBits<Uint2>( x, Uint2( 0x01 ), Uint2( kPattern_02 ) );
    x = x_SwapBits<Uint2>( x, Uint2( 0x02 ), Uint2( kPattern_04 ) );
    x = x_SwapBits<Uint2>( x, Uint2( 0x04 ), Uint2( kPattern_08 ) );
    return x_SwapBits( x, 0x08 );
}

inline Uint4 CBitHacks::ReverseBits4( Uint4 x ) 
{
    x = x_SwapBits<Uint4>( x, Uint4( 0x01 ), Uint4( kPattern_02 ) );
    x = x_SwapBits<Uint4>( x, Uint4( 0x02 ), Uint4( kPattern_04 ) );
    x = x_SwapBits<Uint4>( x, Uint4( 0x04 ), Uint4( kPattern_08 ) );
    x = x_SwapBits<Uint4>( x, Uint4( 0x08 ), Uint4( kPattern_16 ) );
    return x_SwapBits( x, 0x10 );
}

inline Uint8 CBitHacks::ReverseBits8( Uint8 x ) 
{
    x = x_SwapBits<Uint8>( x, 0x01, kPattern_02 );
    x = x_SwapBits<Uint8>( x, 0x02, kPattern_04 );
    x = x_SwapBits<Uint8>( x, 0x04, kPattern_08 );
    x = x_SwapBits<Uint8>( x, 0x08, kPattern_16 );
    x = x_SwapBits<Uint8>( x, 0x10, kPattern_32 );
    return x_SwapBits( x, 0x20 );
}

inline Uint1 CBitHacks::ReverseBitPairs1( Uint1 x ) 
{
    x = x_SwapBits<Uint1>( x, Uint1( 0x02 ), Uint1( kPattern_04 ) );
    return x_SwapBits( x, 0x04 );
}

inline Uint2 CBitHacks::ReverseBitPairs2( Uint2 x ) 
{
    x = x_SwapBits<Uint2>( x, Uint2( 0x02 ), Uint2( kPattern_04 ) );
    x = x_SwapBits<Uint2>( x, Uint2( 0x04 ), Uint2( kPattern_08 ) );
    return x_SwapBits( x, 0x08 );
}

inline Uint4 CBitHacks::ReverseBitPairs4( Uint4 x ) 
{
    x = x_SwapBits<Uint4>( x, Uint4( 0x02 ), Uint4( kPattern_04 ) );
    x = x_SwapBits<Uint4>( x, Uint4( 0x04 ), Uint4( kPattern_08 ) );
    x = x_SwapBits<Uint4>( x, Uint4( 0x08 ), Uint4( kPattern_16 ) );
    return x_SwapBits( x, 0x10 );
}

inline Uint8 CBitHacks::ReverseBitPairs8( Uint8 x ) 
{
    x = x_SwapBits<Uint8>( x, 0x02, kPattern_04 );
    x = x_SwapBits<Uint8>( x, 0x04, kPattern_08 );
    x = x_SwapBits<Uint8>( x, 0x08, kPattern_16 );
    x = x_SwapBits<Uint8>( x, 0x10, kPattern_32 );
    return x_SwapBits( x, 0x20 );
}

inline Uint1 CBitHacks::ReverseBitQuads1( Uint1 x ) 
{
    return x_SwapBits( x, 0x04 );
}

inline Uint2 CBitHacks::ReverseBitQuads2( Uint2 x ) 
{
    x = x_SwapBits<Uint2>( x, Uint2( 0x04 ), Uint2( kPattern_08 ) );
    return x_SwapBits( x, 0x08 );
}

inline Uint4 CBitHacks::ReverseBitQuads4( Uint4 x ) 
{
    x = x_SwapBits<Uint4>( x, Uint4( 0x04 ), Uint4( kPattern_08 ) );
    x = x_SwapBits<Uint4>( x, Uint4( 0x08 ), Uint4( kPattern_16 ) );
    return x_SwapBits( x, 0x10 );
}

inline Uint8 CBitHacks::ReverseBitQuads8( Uint8 x ) 
{
    x = x_SwapBits<Uint8>( x, 0x04, kPattern_08 );
    x = x_SwapBits<Uint8>( x, 0x08, kPattern_16 );
    x = x_SwapBits<Uint8>( x, 0x10, kPattern_32 );
    return x_SwapBits( x, 0x20 );
}

template <class Uint, int bitsperpos, int bits>
inline Uint CBitHacks::InsertBits( const Uint& value, int offset ) 
{
    Uint mask( WordFootprint<Uint>( bitsperpos * offset ) );
    Uint right( value & mask );
    Uint insert( Uint( bits ) << ( bitsperpos * offset ) );
    Uint left( ( (~mask) & value ) << bitsperpos );
    return left | right | insert;
}

template <class Uint, int bitsperpos>
inline Uint CBitHacks::DeleteBits( const Uint& value, int offset ) 
{
    Uint mask( WordFootprint<Uint>( bitsperpos * offset ) );
    return ( value & mask ) | ( (~mask) & ( value >> bitsperpos ) );
}

template<class word, class output_iterator>
inline output_iterator CBitHacks::AsBits( word m,  int w, output_iterator dest ) 
{
    for( word k = word(1) << (w-1); w > 0; (--w), (k >>= 1) ) {
        *dest++ = (m & k) ? '1':'0';
    }
    return dest;
}

template<class output_iterator>
inline output_iterator CBitHacks::AsBits( const UintH& m,  int w, output_iterator dest ) 
{
    if( w < 64 ) return AsBits( m.GetLo(), w, dest );
    else return AsBits( m.GetLo(), 64, AsBits( m.GetHi(), w - 64, dest ) );
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

template<class word>
inline word CBitHacks::SmallestFootprint( word data ) // TODO: optimize
{
    if( data == 0 ) return 0;
    word mask = 0;
    while( ( data & mask ) != data ) {
        (mask <<= 1) |= 1;
    }
    return mask;
}

template<class word>
inline int CBitHacks::LargestBitPos( word data ) // TODO: optimize
{
    if( data == 0 ) return -1;
    word mask = 0;
    int ret = -1;
    while( ( data & mask ) != data ) {
        (mask <<= 1) |= 1;
        ++ret;
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
    return DuplicateBits<Uint4>( Uint4( in ) ) | ( ( (Uint8)DuplicateBits<Uint4>( Uint4( in >> 16 ) ) ) << 32 );
}


END_OLIGOFAR_SCOPES

#endif
