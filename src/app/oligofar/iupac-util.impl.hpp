#ifndef OLIGOFAR_IUPAC_UTIL__IMPL__HPP
#define OLIGOFAR_IUPAC_UTIL__IMPL__HPP

#include "iupac-util.hpp"

inline string Ncbi2naToIupac( unsigned char w )
{
    const char * acgt = "ACGT";
    string ret;
    ret += acgt[0x03&(w>>0)];
    ret += acgt[0x03&(w>>2)];
    ret += acgt[0x03&(w>>4)];
    ret += acgt[0x03&(w>>6)];
    return ret;
}

inline string Ncbi4naToIupac( unsigned short w )
{
    const char * iupac = "-ACMGRSVTWYHKDBN";
    string ret;
    ret += iupac[0x0f&(w>>0x0)];
    ret += iupac[0x0f&(w>>0x4)];
    ret += iupac[0x0f&(w>>0x8)];
    ret += iupac[0x0f&(w>>0xc)];
    return ret;
}

inline string Ncbi2naToIupac( Uint4 window, unsigned windowLength )
{
    string ret( windowLength, '.' );
    for( unsigned i = 0; i < windowLength; ++i, (window >>= 2) )
        ret[i] = "ACGT"[window&0x3];
    return ret;
}

inline string Ncbi4naToIupac( Uint8 window, unsigned windowLength )
{
    string ret( windowLength, '.' );
    for( unsigned i = 0; i < windowLength; ++i, (window >>= 4) )
        ret[i] = "-ACMGRSVTWYHKDBN"[window&0xf];
    return ret;
}

inline unsigned char Iupacna2Ncbi4na( unsigned char c )
{
    return
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0x00-0x0f
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0x10-0x1f
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x00\x0f\x0f" // 0x20-0x2f
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0x30-0x3f
        "\x0f\x01\x0e\x02""\x0d\x0f\x0f\x04""\x0b\x0f\x0f\x0c""\x0f\x03\x0f\x0f" // 0x40-0x4f
        "\x0f\x0f\x05\x06""\x08\x08\x07\x09""\x0f\x0a\x0f\x0f""\x0f\x0f\x0f\x0f" // 0x50-0x5f
        "\x0f\x01\x0e\x02""\x0d\x0f\x0f\x04""\x0b\x0f\x0f\x0c""\x0f\x03\x0f\x0f" // 0x60-0x6f
        "\x0f\x0f\x05\x06""\x08\x08\x07\x09""\x0f\x0a\x0f\x0f""\x0f\x0f\x0f\x0f" // 0x70-0x7f
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0x80-0x8f
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0x90-0x9f
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0xa0-0xaf
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0xb0-0xbf
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0xc0-0xcf
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0xd0-0xdf
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0xe0-0xef
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0xf0-0xff
        [ c ];
}

inline unsigned char Iupacna2Ncbi4naCompl( unsigned char c )

{
    return
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0x00-0x0f
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0x10-0x1f
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x00\x0f\x0f" // 0x20-0x2f
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0x30-0x3f
        "\x0f\x08\x07\x04""\x0b\x0f\x0f\x02""\x0d\x0f\x0f\x03""\x0f\x0c\x0f\x0f" // 0x40-0x4f
        "\x0f\x0f\x0a\x06""\x01\x01\x0e\x09""\x0f\x05\x0f\x0f""\x0f\x0f\x0f\x0f" // 0x50-0x5f
        "\x0f\x08\x07\x04""\x0b\x0f\x0f\x02""\x0d\x0f\x0f\x03""\x0f\x0c\x0f\x0f" // 0x60-0x6f
        "\x0f\x0f\x0a\x06""\x01\x01\x0e\x09""\x0f\x05\x0f\x0f""\x0f\x0f\x0f\x0f" // 0x70-0x7f
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0x80-0x8f
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0x90-0x9f
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0xa0-0xaf
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0xb0-0xbf
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0xc0-0xcf
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0xd0-0xdf
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0xe0-0xef
        "\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f""\x0f\x0f\x0f\x0f" // 0xf0-0xff
        [ c ];
}

inline unsigned char Ncbi4na2Ncbi2na( unsigned char c )
{
	//        -   A   C         G                 T
	return "\x00\x00\x01\x00""\x02\x00\x01\x00""\x03\x00\x01\x00""\x02\x00\x01\x00"[c];
}

inline unsigned char Iupacna2Ncbi2na( unsigned char c )
{
    return
        "\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00" // 0x00-0x0f
        "\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00" // 0x10-0x1f
        "\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00" // 0x20-0x2f
        "\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00" // 0x30-0x3f
        "\x00\x00\x01\x01""\x00\x00\x00\x02""\x00\x00\x00\x02""\x00\x00\x00\x00" // 0x40-0x4f
        "\x00\x00\x00\x01""\x03\x03\x00\x00""\x00\x01\x00\x00""\x00\x00\x00\x00" // 0x50-0x5f
        "\x00\x00\x01\x01""\x00\x00\x00\x02""\x00\x00\x00\x02""\x00\x00\x00\x00" // 0x60-0x6f
        "\x00\x00\x00\x01""\x03\x03\x00\x00""\x00\x01\x00\x00""\x00\x00\x00\x00" // 0x70-0x7f
        "\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00" // 0x80-0x8f
        "\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00" // 0x90-0x9f
        "\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00" // 0xa0-0xaf
        "\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00" // 0xb0-0xbf
        "\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00" // 0xc0-0xcf
        "\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00" // 0xd0-0xdf
        "\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00" // 0xe0-0xef
        "\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00""\x00\x00\x00\x00" // 0xf0-0xff
        [ c ];
}

inline unsigned char Iupacna2Ncbi2naCompl( unsigned char c )
{
    return
        "\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03" // 0x00-0x0f
        "\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03" // 0x10-0x1f
        "\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x00\x03\x03" // 0x20-0x2f
        "\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03" // 0x30-0x3f
        "\x03\x03\x00\x02""\x00\x03\x03\x01""\x00\x03\x03\x00""\x03\x02\x00\x03" // 0x40-0x4f
        "\x03\x03\x01\x01""\x00\x00\x01\x00""\x03\x00\x03\x03""\x03\x03\x03\x03" // 0x50-0x5f
        "\x03\x03\x00\x02""\x00\x03\x03\x01""\x00\x03\x03\x00""\x03\x02\x00\x03" // 0x60-0x6f
        "\x03\x03\x01\x01""\x00\x00\x01\x00""\x03\x00\x03\x03""\x03\x03\x03\x03" // 0x70-0x7f
        "\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03" // 0x80-0x8f
        "\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03" // 0x90-0x9f
        "\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03" // 0xa0-0xaf
        "\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03" // 0xb0-0xbf
        "\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03" // 0xc0-0xcf
        "\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03" // 0xd0-0xdf
        "\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03" // 0xe0-0xef
        "\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03""\x03\x03\x03\x03" // 0xf0-0xff
        [ c ];
}

inline char IupacnaComplement( char iupacna )
{
    return
        "................""................"
        "................""................"
        ".TVGH..CD..M.KN.""..YSAABW.R......"
        ".tvgh..cd..m.kn.""..ysaabw.r......"
        "................""................"
        "................""................"
        "................""................"
        "................""................"[int((unsigned char)iupacna)];
}

inline string IupacnaRevCompl( const string& iupacna )
{
	if( iupacna.size() == 0 ) return "";
	string ret( iupacna.size(), '.' );
	for( int i = int(iupacna.size()) - 1, j = 0; i >= 0; --i, ++j ) 
		ret[i] = IupacnaComplement( iupacna[j] );
	return ret;
}


inline unsigned char Ncbi4naCompl( unsigned char x )
{
    return "\x0\x8\x4\xc\x2\xa\x6\xe\x1\x9\x5\xd\x3\xb\x7\xf"[int(x)];
}

inline unsigned char Ncbi2naCompl( unsigned char x )
{
    return (~x)&0x03;
}

inline unsigned char Ncbipna2Ncbi4na( unsigned char a, unsigned char c,
                                      unsigned char g, unsigned char t, 
                                      unsigned char n, unsigned short score )
{
    return
        (unsigned(a) > score ? 0x01 : 0) |
        (unsigned(c) > score ? 0x02 : 0) |
        (unsigned(g) > score ? 0x04 : 0) |
        (unsigned(t) > score ? 0x08 : 0) ;
}

inline unsigned char Ncbipna2Ncbi4naCompl( unsigned char a, unsigned char c,
                                           unsigned char g, unsigned char t, 
                                           unsigned char n, unsigned short score )
{
    return
        (unsigned(a) > score ? 0x08 : 0) |
        (unsigned(c) > score ? 0x04 : 0) |
        (unsigned(g) > score ? 0x02 : 0) |
        (unsigned(t) > score ? 0x01 : 0) ;
}

inline unsigned char Ncbipna2Ncbi4na( const unsigned char * p, unsigned short score )
{
    return Ncbipna2Ncbi4na( p[0], p[1], p[2], p[3], p[4], score );
}

inline unsigned char Ncbipna2Ncbi4naCompl( const unsigned char * p, unsigned short score )
{
    return Ncbipna2Ncbi4naCompl( p[0], p[1], p[2], p[3], p[4], score );
}

inline unsigned char Ncbipna2Ncbi4naN( const unsigned char * p, unsigned short score )
{
    unsigned char x = Ncbipna2Ncbi4na( p[0], p[1], p[2], p[3], p[4], score );
    return x ? x : 0x0f;
}

inline unsigned char Ncbipna2Ncbi4naComplN( const unsigned char * p, unsigned short score )
{
    unsigned char x = Ncbipna2Ncbi4naCompl( p[0], p[1], p[2], p[3], p[4], score );
    return x ? x : 0x0f;
}

inline unsigned char Ncbiqna2Ncbi4na( const unsigned char * p, unsigned short score )
{
	if( (*p >> 2) > score ) return "\x1\x2\x4\x8"[(*p)&3];
	else return '\xf';
}

inline unsigned char Ncbiqna2Ncbi4naCompl( const unsigned char * p, unsigned short score )
{
	if( (*p >> 2) > score ) return "\x8\x4\x2\x1"[(*p)&3];
	else return '\xf';
}

template<class iterator>
inline iterator Solexa2Ncbipna( iterator dest, const string& line, int ) 
{
    istringstream in( line );
    while( !in.eof() ) {
        int a,c,g,t;
        in >> a >> c >> g >> t;
        if( in.fail() ) throw runtime_error( "Bad solexa line format: [" + line + "]" );
		double exp = max( max( a, c ), max( g, t ) ); 
        double base = 255 / std::pow( 2., exp ); // TODO: correct
        *dest++ = unsigned (base * std::pow(2., a));
        *dest++ = unsigned (base * std::pow(2., c));
        *dest++ = unsigned (base * std::pow(2., g));
        *dest++ = unsigned (base * std::pow(2., t));
        *dest++ = '\xff';
    }
    return dest;
}

template<class iterator>
inline iterator Iupacnaq2Ncbapna( iterator dest, const string& iupac, const string& qual, int base )
{
	for( const char * i = iupac.c_str(), * q = qual.c_str(); *i && *q ; ++i, ++q ) {
		if( *i == 'N' || *i == 'n' || *q < base + 2 ) {
			*dest++ = 255;
			*dest++ = 255;
			*dest++ = 255;
			*dest++ = 255;
		} else {
			int val = int( 255 * ( pow(10.0,-double(*q - base)/10)) );
			int x = Iupacna2Ncbi2na( *i )&0x03;
			*dest++ = (x--) ? val : 255;
			*dest++ = (x--) ? val : 255;
			*dest++ = (x--) ? val : 255;
			*dest++ = (x--) ? val : 255;
		}
		*dest++ = 255;
	}
	return dest;
}

inline double ComputeComplexity( unsigned hash, unsigned bases )
{
    unsigned char words[64];
    memset( words, 0, sizeof( words ) );
    for( unsigned b = bases - 2; b ; (--b), (hash >>= 2) )
        words[hash&0x3f]++;
    double c = 0;
    for( unsigned char * w = words; w != words + sizeof(words)/sizeof(*words); ++w )
        c += *w*(*w-1);
    return c / 2 / (bases - 3);
}

inline Uint8 Ncbipna2Ncbi4na( const unsigned char * data, unsigned windowLength, unsigned short score )
{
    Uint8 ret = 0;
    for( unsigned x = 0; x < windowLength; ++x ) 
        ret |= Uint8( Ncbipna2Ncbi4na( data + 5*x, score ) ) << (x*4);
    return ret;
}

inline Uint8 Ncbipna2Ncbi4naRevCompl( const unsigned char * data, unsigned windowLength, unsigned short score )
{
    Uint8 ret = 0;
    for( unsigned x = 0, y = windowLength - 1; x < windowLength; ++x, --y ) 
        ret |= Uint8( Ncbipna2Ncbi4naCompl( data + 5*x, score ) ) << (y*4);
    return ret;
}

inline Uint8 Ncbiqna2Ncbi4na( const unsigned char * data, unsigned windowLength, unsigned short score )
{
    Uint8 ret = 0;
    for( unsigned x = 0; x < windowLength; ++x ) 
        ret |= Uint8( Ncbiqna2Ncbi4na( data + x, score ) ) << (x*4);
    return ret;
}

inline Uint8 Ncbiqna2Ncbi4naRevCompl( const unsigned char * data, unsigned windowLength, unsigned short score )
{
    Uint8 ret = 0;
    for( unsigned x = 0, y = windowLength - 1; x < windowLength; ++x, --y ) 
        ret |= Uint8( Ncbiqna2Ncbi4naCompl( data + x, score ) ) << (y*4);
    return ret;
}

inline Uint8 Ncbi8na2Ncbi4na( const unsigned char * data, unsigned windowLength )
{
    Uint8 ret = 0;
    for( unsigned x = 0; x < windowLength; ++x ) 
        ret |= Uint8( data[x] ) << (x*4);
    return ret;
}

inline Uint8 Ncbi8na2Ncbi4naRevCompl( const unsigned char * data, unsigned windowLength )
{
    Uint8 ret = 0;
    for( unsigned x = 0, t = windowLength - 1; x < windowLength; ++x, --t ) 
        ret |= Uint8( Ncbi4naCompl( data[t] ) ) << (x*4);
    return ret;
}

inline Uint8 Iupacna2Ncbi4na( const unsigned char * data, unsigned windowLength )
{
    Uint8 ret = 0;
    for( unsigned x = 0; x < windowLength; ++x ) 
        ret |= Uint8( Iupacna2Ncbi4na( data[x] ) ) << (x*4);
    return ret;
}

inline Uint8 Iupacna2Ncbi4naRevCompl( const unsigned char * data, unsigned windowLength )
{
    Uint8 ret = 0;
    for( unsigned x = 0, t = windowLength - 1; x < windowLength; ++x, --t ) 
        ret |= Uint8( Iupacna2Ncbi4naCompl( data[t] ) ) << (x*4);
    return ret;
}

inline Uint4 Iupacna2Ncbi2na( const unsigned char * window, unsigned windowLength )
{
    Uint4 ret = 0;
    for( unsigned x = 0; x < windowLength; ++x ) ret |= Uint4( Iupacna2Ncbi2na( window[x] ) ) << (x*2);
    return ret;
}

inline Uint4 Iupacna2Ncbi2naRevCompl( const unsigned char * data, unsigned windowLength )
{
    Uint4 ret = 0;
    for( unsigned x = 0, t = windowLength - 1; x < windowLength; ++x, --t ) 
        ret |= Uint4( Iupacna2Ncbi2naCompl( data[t] ) ) << (x*2);
    return ret;
}

inline Uint8 Ncbi4naReverse( Uint8 window, unsigned windowSize )
{
    return CBitHacks::ReverseBitQuads8( window ) >> (64 - 4 * windowSize);
}

inline Uint8 Ncbi4naRevCompl( Uint8 window, unsigned windowSize )
{
    return CBitHacks::ReverseBits8( window ) >> (64 - 4 * windowSize);
}

inline Uint4 Ncbi2naRevCompl( Uint4 window, unsigned windowSize )
{
    return (~CBitHacks::ReverseBitPairs4( window )) >> (32 - 2 * windowSize);
}

inline bool Ncbi4naIsAmbiguous( unsigned t )
{
    return "\x0\x0\x0\x1\x0\x1\x1\x1\x0\x1\x1\x1\x1\x1\x1\x1"[t&0x0f] != 0;
}

inline unsigned Ncbi4naAlternativeCount( unsigned t )
{
    return "\x0\x1\x1\x2\x1\x2\x2\x3\x1\x2\x2\x3\x2\x3\x3\x4"[t&0x0f];
}

inline Uint8 Ncbi4naAlternativeCount( Uint8 t, unsigned windowSize )
{
    Uint8 total = 1;
    for( unsigned i = 0; i < windowSize; ++i ) {
        if( unsigned c = Ncbi4naAlternativeCount( unsigned( t >> (i*4) ) ) ) total *= c;
    }
    return total;
}

#endif
