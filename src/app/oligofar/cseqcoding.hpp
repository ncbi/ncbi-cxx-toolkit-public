#ifndef OLIGOFAR_CSEQCODING__HPP
#define OLIGOFAR_CSEQCODING__HPP

#include "defs.hpp"
#include <cmath>

BEGIN_OLIGOFAR_SCOPES

class CIupacnaBase;
class CNcbipnaBase;
class CNcbiqnaBase;
class CNcbi8naBase;
class CNcbi4naBase;
class CNcbi2naBase;

class CSeqCoding
{
public:
    enum ECoding {
        eCoding_iupacna,
        eCoding_ncbipna,
        eCoding_ncbi8na,
        eCoding_ncbi4na,
        eCoding_ncbi2na,
		eCoding_ncbiqna
    };
	enum EStrand { 
		eStrand_pos = 0, 
		eStrand_neg = 1 
	};
	// NB: following function should be rewritten if values of EStrand constants are different from 0 (+) and 1 (-)
	friend EStrand operator ^ ( EStrand a, EStrand b ) { return EStrand( int(a) ^ int(b) ); }
};

class CIupacnaBase : public CSeqCoding
{ 
public: 
    CIupacnaBase( const char * c, EStrand strand ) : m_base( *c ) { if( strand == eStrand_neg ) m_base = s_complement[(int)m_base]; }
    CIupacnaBase( const char * c ) : m_base( *c ) {}
    CIupacnaBase( char c ) : m_base( c ) {}
    CIupacnaBase( const CNcbi8naBase& );
    CIupacnaBase( const CNcbi4naBase& );
    CIupacnaBase( const CNcbi2naBase& );
    CIupacnaBase( const CNcbiqnaBase& , int cutoff = 5 );
    CIupacnaBase( const CNcbipnaBase& , int mask = 0xf000 );
    CIupacnaBase Complement() const { return s_complement[(int)m_base]; }
	CIupacnaBase Get( EStrand strand ) const { return strand == eStrand_neg ? Complement() : *this; }
    operator char () const { return m_base; }
	static CIupacnaBase Any() { return CIupacnaBase('N'); }
protected: 
    char m_base;
    static char s_complement[];
};

class CNcbipnaBase : public CSeqCoding
{
public:
    CNcbipnaBase( const char * base, EStrand strand ) { if( strand == eStrand_neg ) { copy( base + 4, base, m_base ); m_base[4] = base[4]; } else copy( base, base+5, m_base ); }
    CNcbipnaBase( const char * base ) { copy( base, base+5, m_base ); }
    CNcbipnaBase( const unsigned char * base ) { copy( base, base+5, m_base ); }
    CNcbipnaBase( const CIupacnaBase& b );
    CNcbipnaBase( const CNcbi8naBase& b ) { x_Init( b ); }
    CNcbipnaBase( const CNcbi4naBase& b );
    CNcbipnaBase( const CNcbi2naBase& );
    CNcbipnaBase( const CNcbiqnaBase& );
    operator const char * () const { return m_base; }
    unsigned char operator [] (int i) const { return m_base[i]; }
	CNcbipnaBase Complement() const { return CNcbipnaBase( m_base, eStrand_neg ); }
	CNcbipnaBase Get( EStrand strand ) const { return strand == eStrand_neg ? Complement() : *this; }
protected:
    void x_Init( const CNcbi8naBase&  );
    char m_base[5]; // pointer will take 4 bytes in 32-bit architecture - so would save almost nothing
};

class CNcbiqnaBase : public CSeqCoding
{
public:
	CNcbiqnaBase( const char * base, EStrand strand ): m_base( *base ) { if( strand == eStrand_neg ) m_base = m_base ^ '\x03'; }
	CNcbiqnaBase( const char * c ) : m_base( *c ) {}
	CNcbiqnaBase( char c ) : m_base( c ) {}
	CNcbiqnaBase( int c ) : m_base( c ) {}
	CNcbiqnaBase( const CNcbipnaBase& b );
	CNcbiqnaBase( const CNcbi8naBase& b, unsigned score = 63 );
	CNcbiqnaBase( const CNcbi4naBase& b, unsigned score = 63 );
	CNcbiqnaBase( const CNcbi2naBase& b, unsigned score = 63 );
	CNcbiqnaBase( const CIupacnaBase& b, unsigned score = 63 );
	CNcbiqnaBase Complement() const { return m_base ^ '\x03'; }
	CNcbiqnaBase Get( EStrand strand ) const { return ( strand == eStrand_neg ) ? Complement() : *this; }
    operator char () const { return m_base; }
	int GetPhrapScore() const { return m_base >> 2; }
	static char AdjustScore( int score ); 
	static CNcbiqnaBase Any() { return CNcbiqnaBase( 0 ); }
protected:
	char m_base;
};

class CNcbi8naBase : public CSeqCoding
{
public:
    CNcbi8naBase( const char * c, EStrand strand ) : m_base( *c ) { if( strand == eStrand_neg ) m_base = s_complement[(int)m_base]; }
	CNcbi8naBase( const char * c ) : m_base( *c ) {}
    CNcbi8naBase( char c, EStrand strand = eStrand_pos );
    CNcbi8naBase( int c, EStrand strand = eStrand_pos );
    CNcbi8naBase( unsigned int c, EStrand strand  = eStrand_pos );
    CNcbi8naBase( const CIupacnaBase& b );
    CNcbi8naBase( const CNcbipnaBase& b, int mask = 0xf000 );
    CNcbi8naBase( const CNcbi4naBase& b );
    CNcbi8naBase( const CNcbi2naBase& b );
    CNcbi8naBase( const CNcbiqnaBase& b, int cutoff = 5 );
    CNcbi8naBase Complement() const;
	CNcbi8naBase Get( EStrand strand ) const { return ( strand == eStrand_neg ) ? Complement() : *this; }
    int GetAltCount() const { return s_altCount[(int)m_base]; }
    operator char () const { return m_base; }
	static CNcbi8naBase Any() { return CNcbi8naBase( '\x0f' ); }
protected:
    char m_base;
    static char s_fromIupacna[];
	static char s_complement[];
	static char s_altCount[];
};

class CNcbi4naBase : public CNcbi8naBase 
{
public:
	CNcbi4naBase( const CNcbi8naBase& b ) : CNcbi8naBase( b ) {}
};

class CNcbi2naBase : public CSeqCoding
{
public:
    CNcbi2naBase( const char * c, EStrand strand ) : m_base( *c ) { if( strand == eStrand_neg ) m_base = '\x03' ^ m_base; }
	CNcbi2naBase( const char * c ) : m_base( *c ) {}
    CNcbi2naBase( char c ) : m_base( c ) {}
    CNcbi2naBase( int c ) : m_base( c ) {}
    CNcbi2naBase( unsigned int c ) : m_base( c ) {}
    CNcbi2naBase( const CNcbipnaBase& b, int mask = 0xf000 );
    CNcbi2naBase( const CIupacnaBase& b );
    CNcbi2naBase( const CNcbi8naBase& b );
    CNcbi2naBase( const CNcbi4naBase& b );
    CNcbi2naBase( const CNcbiqnaBase& b, int cutoff = 5 );
    operator char () const { return m_base; }
    CNcbi2naBase Complement() const { return '\x03' ^ m_base; }
	CNcbi2naBase Get( EStrand strand ) const { return ( strand == eStrand_neg ) ? Complement() : *this; }
protected:
    char m_base;
    static char s_iupacnaTable[];
    static char s_ncbi8naTable[];
};

////////////////////////////////////////////////////////////////////////

template<class BaseA, class BaseB>
inline int ComputeScore( const BaseA& a, const BaseB& b )
{
    return bool( CNcbi8naBase( a ) & CNcbi8naBase( b ) );
}

inline int ComputeScore( const CNcbi8naBase& a, const CNcbipnaBase& b )
{
    return 
        (( a & 0x01 ? b[0] : 0 ) + 
         ( a & 0x02 ? b[1] : 0 ) +
         ( a & 0x04 ? b[2] : 0 ) +
         ( a & 0x08 ? b[3] : 0 )) / (b[0] + b[1] + b[2] + b[3]);
}

inline int ComputeScore( const CNcbipnaBase& b, const CNcbi8naBase& a )
{
    return 
        (( a & 0x01 ? b[0] : 0 ) + 
         ( a & 0x02 ? b[1] : 0 ) +
         ( a & 0x04 ? b[2] : 0 ) +
         ( a & 0x08 ? b[3] : 0 )) / (b[0] + b[1] + b[2] + b[3]);
}

inline int operator * ( const CNcbi2naBase& a, const CNcbi2naBase& b )
{
    return a == b;
}

////////////////////////////////////////////////////////////////////////

inline CIupacnaBase::CIupacnaBase( const CNcbi8naBase& b ) : m_base("-ACMGRSVTWYHKDBN"[b&0x0f]) {}
inline CIupacnaBase::CIupacnaBase( const CNcbi4naBase& b ) : m_base("-ACMGRSVTWYHKDBN"[b&0x0f]) {}
inline CIupacnaBase::CIupacnaBase( const CNcbi2naBase& b ) : m_base("-ACGT"[b&0x03]) {}
inline CIupacnaBase::CIupacnaBase( const CNcbipnaBase& b, int mask ) : m_base(0) 
{
    m_base = CIupacnaBase( CNcbi8naBase( b, mask ) );
}
inline CIupacnaBase::CIupacnaBase( const CNcbiqnaBase& b, int cutoff ) : m_base( b.GetPhrapScore() > cutoff ? (char)CNcbi2naBase(b) : 'N' ) {}
////////////////////////////////////////////////////////////////////////

inline CNcbipnaBase::CNcbipnaBase( const CIupacnaBase& b ) 
{ 
    x_Init( CNcbi8naBase( b ) ); 
}

inline CNcbipnaBase::CNcbipnaBase( const CNcbi2naBase& b ) 
{
    fill( m_base, m_base + 5, 0 );
    m_base[b] = m_base[4] = 255;
}

inline CNcbipnaBase::CNcbipnaBase( const CNcbi4naBase& b ) 
{ 
    x_Init( b ); 
}

inline void CNcbipnaBase::x_Init( const CNcbi8naBase& b ) 
{ 
    fill( m_base, m_base + 5, 0 );
    if( b & 0x01 ) m_base[0] = 255;
    if( b & 0x02 ) m_base[1] = 255;
    if( b & 0x03 ) m_base[2] = 255;
    if( b & 0x04 ) m_base[3] = 255;
    m_base[4] = b ? 255 : 0;
}

inline CNcbipnaBase::CNcbipnaBase( const CNcbiqnaBase& b ) 
{
	int val = int( 255 * ( std::pow(10.0,b.GetPhrapScore()/10)) );
	int x = CNcbi2naBase( b );
	m_base[0] = x-- ? val : 255;
	m_base[1] = x-- ? val : 255;
	m_base[2] = x-- ? val : 255;
	m_base[3] = x-- ? val : 255;
	m_base[4] = 255;
}

////////////////////////////////////////////////////////////////////////

inline char CNcbiqnaBase::AdjustScore( int score ) 
{
	if( score < 0 ) score = 0;
	if( score > 63 ) score = 63;
	return score;
}

inline CNcbiqnaBase::CNcbiqnaBase( const CNcbipnaBase& b ) : m_base(0)
{
	int max = 0, next = 0;
	for( int i = 0; i < 4; ++i ) {
		if( b[i] > max ) {
		   	m_base = i;	
			next = max;
			max = b[i];
		} else if( b[i] > next ) next = b[i];
	}
	if( next == 0 ) m_base |= 0xfc;
	else {
		double pscore = 10*std::log10( double( max ) / next );
		m_base |= AdjustScore( int(pscore) ) << 2;
	}
}
inline CNcbiqnaBase::CNcbiqnaBase( const CNcbi8naBase& b, unsigned score ) 
{
	if( b.GetAltCount() > 1 ) m_base = 0;
	else m_base = CNcbi2naBase( b ) | AdjustScore(score << 2);
}

inline CNcbiqnaBase::CNcbiqnaBase( const CNcbi4naBase& b, unsigned score )
{
	if( b.GetAltCount() > 1 ) m_base = 0;
	else m_base = CNcbi2naBase( b ) | AdjustScore(score << 2);
}

inline CNcbiqnaBase::CNcbiqnaBase( const CNcbi2naBase& b, unsigned score )
{
	m_base = b | AdjustScore(score << 2);
}

inline CNcbiqnaBase::CNcbiqnaBase( const CIupacnaBase& b, unsigned score )
{
	CNcbi8naBase _b( b );
	if( _b.GetAltCount() > 1 ) m_base = 0;
	else m_base = CNcbi2naBase( b ) | AdjustScore(score << 2);
}

////////////////////////////////////////////////////////////////////////

inline CNcbi8naBase::CNcbi8naBase( char c, EStrand strand ) : m_base( c ) 
{ 
	if( strand == eStrand_neg ) m_base = s_complement[int(m_base)]; 
}

inline CNcbi8naBase::CNcbi8naBase( int c, EStrand strand ) : m_base( c ) 
{ 
	if( strand == eStrand_neg ) m_base = s_complement[int(m_base)]; 
}

inline CNcbi8naBase::CNcbi8naBase( unsigned int c, EStrand strand ) : m_base( c ) 
{ 
	if( strand == eStrand_neg ) m_base = s_complement[int(m_base)]; 
}

inline CNcbi8naBase::CNcbi8naBase( const CNcbi4naBase& b ) : m_base( b ) {}
inline CNcbi8naBase::CNcbi8naBase( const CNcbi2naBase& b ) : m_base( "\x01\x02\x04\x08"[(int)b] ) {}

inline CNcbi8naBase::CNcbi8naBase( const CNcbipnaBase& b, int mask ) : m_base(0) 
{
    if( b[0] & mask ) m_base |= 0x01;
    if( b[1] & mask ) m_base |= 0x02;
    if( b[2] & mask ) m_base |= 0x04;
    if( b[3] & mask ) m_base |= 0x08;
    if( (!m_base) && ( b[4] & mask ) ) m_base = 0x0f;
}

inline CNcbi8naBase::CNcbi8naBase( const CIupacnaBase& b ) : m_base( s_fromIupacna[b] ) 
{
    if( m_base == '\xf0' ) THROW( runtime_error, "Invalid IUPACNA base ASCII[" << int(b) << "] " << b );
}

inline CNcbi8naBase CNcbi8naBase::Complement() const 
{ 
    return s_complement[(int)m_base]; 
}

inline CNcbi8naBase::CNcbi8naBase( const CNcbiqnaBase& b, int cutoff ) : m_base( b.GetPhrapScore() > cutoff ? (char)CNcbi2naBase(b) : 'N' ) {}

////////////////////////////////////////////////////////////////////////

inline CNcbi2naBase::CNcbi2naBase( const CIupacnaBase& b ) : m_base( s_iupacnaTable[b] ) 
{
    if( m_base == '\xf0' ) THROW( runtime_error, "Invalid IUPACNA base ASCII[" << int(b) << "] " << b );
}

inline CNcbi2naBase::CNcbi2naBase( const CNcbi8naBase& b ) : m_base( s_ncbi8naTable[b] ) 
{
    if( m_base == '\xf0' ) THROW( runtime_error, "Invalid NCBI8NA base 0x" << setw(2) << hex << setfill('0') << int(b) << " IUPACNA " << CIupacnaBase(b) );
}

inline CNcbi2naBase::CNcbi2naBase( const CNcbi4naBase& b ) : m_base( s_ncbi8naTable[b] ) 
{
    if( m_base == '\xf0' ) THROW( runtime_error, "Invalid NCBI8NA base 0x" << setw(2) << hex << setfill('0') << int(b) << " IUPACNA " << CIupacnaBase(b) );
}

inline CNcbi2naBase::CNcbi2naBase( const CNcbiqnaBase& b, int cutoff ) : m_base( b.GetPhrapScore() > cutoff ? (char)CNcbi2naBase(b) : 'N' ) {}

////////////////////////////////////////////////////////////////////////

END_OLIGOFAR_SCOPES

#endif
