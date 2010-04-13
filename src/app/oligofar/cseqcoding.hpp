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
class CColorTwoBase;

class CSeqCoding
{
public:
    enum ECoding {
        eCoding_iupacna,
        eCoding_ncbipna,
        eCoding_ncbi8na,
        eCoding_ncbi4na,
        eCoding_ncbi2na,
		eCoding_ncbiqna,
		eCoding_colorsp
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
    CIupacnaBase( const CNcbipnaBase& , int score = 127 );
    CIupacnaBase( const CColorTwoBase& b );
    CIupacnaBase( const CIupacnaBase& b, const CColorTwoBase& c );
    CIupacnaBase Complement() const { return s_complement[(int)m_base]; }
	CIupacnaBase Get( EStrand strand ) const { return strand == eStrand_neg ? Complement() : *this; }
    operator char () const { return m_base; }
	static CIupacnaBase Any() { return CIupacnaBase('N'); }
    static int BytesPerBase() { return 1; }
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
    CNcbipnaBase( const CColorTwoBase& b ) { THROW( logic_error, "CNcbipnaBase( CColorTwoBase ) should not be called" ); }
    operator char () const { THROW( logic_error, "CNcbiqnaBase::operator char () should not be called!" ); }
    operator const char * () const { return m_base; }
    unsigned char operator [] (int i) const { return m_base[i]; }
	CNcbipnaBase Complement() const { return CNcbipnaBase( m_base, eStrand_neg ); }
	CNcbipnaBase Get( EStrand strand ) const { return strand == eStrand_neg ? Complement() : *this; }
    static int BytesPerBase() { return 5; }
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
    CNcbiqnaBase( const CColorTwoBase& b ) { THROW( logic_error, "CNcbiqnaBase( CColorTwoBase ) should not be called" ); }
	CNcbiqnaBase Complement() const { return m_base ^ '\x03'; }
	CNcbiqnaBase Get( EStrand strand ) const { return ( strand == eStrand_neg ) ? Complement() : *this; }
    operator char () const { return m_base; }
	int GetPhrapScore() const { return ((unsigned char)m_base) >> 2; }
	static char AdjustScore( int score ); 
	static CNcbiqnaBase Any() { return CNcbiqnaBase( 0 ); }
    static int BytesPerBase() { return 1; }
protected:
	char m_base;
};

class CNcbi8naBase : public CSeqCoding
{
public:
    enum EBase { fBase_A = 0x01, fBase_C = 0x02, fBase_G = 0x04, fBase_T = 0x08 };
    CNcbi8naBase( const char * c, EStrand strand ) : m_base( *c ) { if( strand == eStrand_neg ) m_base = s_complement[(int)m_base]; }
	CNcbi8naBase( const char * c ) : m_base( *c ) {}
    CNcbi8naBase( char c, EStrand strand = eStrand_pos );
    CNcbi8naBase( int c, EStrand strand = eStrand_pos );
    CNcbi8naBase( unsigned int c, EStrand strand  = eStrand_pos );
    CNcbi8naBase( const CIupacnaBase& b );
    CNcbi8naBase( const CNcbipnaBase& b, int score = 127 );
    CNcbi8naBase( const CNcbiqnaBase& b, int cutoff = 5 );
    CNcbi8naBase( const CNcbi8naBase& b, int ) : m_base( b ) {}
    CNcbi8naBase( const CNcbi4naBase& b );
    CNcbi8naBase( const CNcbi2naBase& b );
    CNcbi8naBase( const CColorTwoBase& b ) { THROW( logic_error, "CNcbi8naBase( CColorTwoBase ) should not be called" ); }
    CNcbi8naBase( const CNcbi8naBase& b, const CColorTwoBase& c );
    CNcbi8naBase Complement() const;
	CNcbi8naBase Get( EStrand strand ) const { return ( strand == eStrand_neg ) ? Complement() : *this; }
    bool IsAmbiguous() const { return GetAltCount() > 1; }
    int GetAltCount() const { return s_altCount[(int)m_base]; }
    operator char () const { return m_base; }
	static CNcbi8naBase Any() { return CNcbi8naBase( '\x0f' ); }
    static int BytesPerBase() { return 1; }
	CNcbi2naBase GetSmallestNcbi2na() const;
protected:
    char m_base;
    static char s_fromIupacna[];
	static char s_complement[];
	static char s_altCount[];
	static char s_smallestNcbi2na[];
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
    CNcbi2naBase( const CNcbipnaBase& b, int score = 127 );
    CNcbi2naBase( const CIupacnaBase& b );
    CNcbi2naBase( const CNcbi8naBase& b );
    CNcbi2naBase( const CNcbi4naBase& b );
    CNcbi2naBase( const CNcbiqnaBase& b );
    operator char () const { return m_base; }
    CNcbi2naBase Complement() const { return '\x03' ^ m_base; }
	CNcbi2naBase Get( EStrand strand ) const { return ( strand == eStrand_neg ) ? Complement() : *this; }
protected:
    char m_base;
    static char s_iupacnaTable[];
    static char s_ncbi8naTable[];
};

class CColorTwoBase : public CSeqCoding
{
public:
	enum EColorHi { eColor_BLUE = 0x00, eColor_GREEN = 0x10, eColor_YELLOW = 0x20, eColor_RED = 30 };
	enum EFromASCII { eFromASCII };
	CColorTwoBase( char c ) : m_base( c ) {}
	CColorTwoBase( char c, EStrand ) : m_base( c ) {}
	CColorTwoBase( const char * c ) : m_base( *c ) {}
	CColorTwoBase( const char * c, EStrand ) : m_base( *c ) {}
	CColorTwoBase( EFromASCII, char c ) { x_Init( c ); }
	CColorTwoBase( CNcbi2naBase b ) : m_base( CNcbi8naBase( b ) ) {}
	CColorTwoBase( CNcbiqnaBase b ) : m_base( CNcbi8naBase( b ) ) {}
	CColorTwoBase( CNcbi8naBase b ) : m_base( b ) {}
	CColorTwoBase( CNcbipnaBase b ) : m_base( CNcbi8naBase(b) ) {}
	CColorTwoBase( CNcbi2naBase prev, CNcbi2naBase b ) :
		m_base( s_dibase2code[(prev << 2) | b] | CColorTwoBase(b) ) {}
	CColorTwoBase( CNcbiqnaBase prev, CNcbiqnaBase b ) :
		m_base( s_dibase2code[(prev&0x30 << 2) | b&0x30] | CColorTwoBase(b) ) {}
	CColorTwoBase( CNcbi2naBase prev, CNcbi8naBase b ) :
		m_base( s_dibase2code[(prev << 2) | b.GetSmallestNcbi2na()] | CColorTwoBase(b) ) {}
	CColorTwoBase( CColorTwoBase prev, CColorTwoBase b ) { THROW( logic_error, "CColorTwoBase( CColorTwoBase, CColorTwoBase ) should never be called" ); }
	CColorTwoBase( CNcbi8naBase prev, CNcbi8naBase b ) :
		m_base( s_dibase2code[(prev.GetSmallestNcbi2na() << 2) | b.GetSmallestNcbi2na()] | CColorTwoBase(b) ) {}
	CColorTwoBase( CNcbipnaBase prev, CNcbipnaBase b ) :
		m_base( s_dibase2code[(CNcbi8naBase(prev).GetSmallestNcbi2na() << 2) | CNcbi8naBase(b).GetSmallestNcbi2na()] | CColorTwoBase(b) ) {}
//     template<class base>
//     CColorTwoBase( base a, base b ) : m_base( s_dibase2code[(CNcbi8naBase( a ).GetSmallestNcbi2na()<<2) | CNcbi8naBase( b ).GetSmallestNcbi2na()] ) {}
	operator char () const { return m_base; }
	CNcbi8naBase GetBaseCalls() const { return m_base & 0xf; }
	CColorTwoBase Complement() const { return GetBaseCalls().Complement() | GetColor(); }
	int GetColor() const { return m_base & 0x30; }
	int GetColorOrd() const { return GetColor() >> 4; }
	char AsAscii( bool tryBaseCalls = true ) const { return tryBaseCalls && GetBaseCalls() ? (char)CIupacnaBase( GetBaseCalls() ) : ('0' + GetColorOrd()); }
    static int BytesPerBase() { return 1; }
protected:
	void x_Init( char c );
protected:
	static char s_dibase2code[];
	char m_base; // ..cc4444
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
inline CIupacnaBase::CIupacnaBase( const CNcbi2naBase& b ) : m_base("ACGT"[b&0x03]) {}
inline CIupacnaBase::CIupacnaBase( const CNcbipnaBase& b, int score ) : m_base(0) { m_base = CIupacnaBase( CNcbi8naBase( b, score ) ); }
inline CIupacnaBase::CIupacnaBase( const CNcbiqnaBase& b, int cutoff ) : m_base( b.GetPhrapScore() > cutoff ? "ACGT"[b&3] : 'N' ) {}
inline CIupacnaBase::CIupacnaBase( const CColorTwoBase& b ) { m_base = b.GetBaseCalls() ? char(CIupacnaBase( b.GetBaseCalls() )) : char(( b.GetColorOrd() + '0' )); }
inline CIupacnaBase::CIupacnaBase( const CIupacnaBase& b, const CColorTwoBase& c ) 
{ m_base = "ACGTCATGGTACTGCA"[(CNcbi2naBase(b) << 2)|c.GetColorOrd()]; }

////////////////////////////////////////////////////////////////////////

inline CNcbipnaBase::CNcbipnaBase( const CIupacnaBase& b ) 
{ 
    x_Init( CNcbi8naBase( b ) ); 
}

inline CNcbipnaBase::CNcbipnaBase( const CNcbi2naBase& b ) 
{
    fill( m_base, m_base + 5, 0 );
    m_base[b] = m_base[4] = '\xff';
}

inline CNcbipnaBase::CNcbipnaBase( const CNcbi4naBase& b ) 
{ 
    x_Init( b ); 
}

inline void CNcbipnaBase::x_Init( const CNcbi8naBase& b ) 
{ 
    fill( m_base, m_base + 5, 0 );
    if( b & 0x01 ) m_base[0] = '\xff';
    if( b & 0x02 ) m_base[1] = '\xff';
    if( b & 0x03 ) m_base[2] = '\xff';
    if( b & 0x04 ) m_base[3] = '\xff';
    m_base[4] = b ? '\xff' : 0;
}

inline CNcbipnaBase::CNcbipnaBase( const CNcbiqnaBase& b ) 
{
	Uint1 val = Uint1( 255 * ( std::pow(10.0, b.GetPhrapScore() / 10 ) ) );
	int x = CNcbi2naBase( b );
	m_base[0] = x-- ? val : '\xff';
	m_base[1] = x-- ? val : '\xff';
	m_base[2] = x-- ? val : '\xff';
	m_base[3] = x-- ? val : '\xff';
	m_base[4] = '\xff';
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
	else m_base = CNcbi2naBase( b ) | (AdjustScore(score) << 2);
}

inline CNcbiqnaBase::CNcbiqnaBase( const CNcbi4naBase& b, unsigned score )
{
	if( b.GetAltCount() > 1 ) m_base = 0;
	else m_base = CNcbi2naBase( b ) | (AdjustScore(score) << 2);
}

inline CNcbiqnaBase::CNcbiqnaBase( const CNcbi2naBase& b, unsigned score )
{
	m_base = b | (AdjustScore(score) << 2);
}

inline CNcbiqnaBase::CNcbiqnaBase( const CIupacnaBase& b, unsigned score )
{
	CNcbi8naBase _b( b );
	if( _b.GetAltCount() > 1 ) m_base = 0;
	else m_base = CNcbi2naBase( b ) | (AdjustScore(score) << 2);
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

inline CNcbi8naBase::CNcbi8naBase( const CNcbipnaBase& b, int score ) : m_base(0) 
{
    if( b[0] > score ) m_base |= 0x01;
    if( b[1] > score ) m_base |= 0x02;
    if( b[2] > score ) m_base |= 0x04;
    if( b[3] > score ) m_base |= 0x08;
    if( (!m_base) && ( b[4] > score ) ) m_base = 0x0f;
}

inline CNcbi8naBase::CNcbi8naBase( const CIupacnaBase& b ) : m_base( s_fromIupacna[b] ) 
{
    if( m_base == '\xf0' ) THROW( runtime_error, "Invalid IUPACNA base ASCII[" << int(b) << "] " << b );
}

inline CNcbi8naBase::CNcbi8naBase( const CNcbi8naBase& b, const CColorTwoBase& c ) 
{ m_base = "\x1\x2\x4\x8\x2\x1\x8\x4\x4\x8\x1\x2\x8\x4\x2\x1"[(CNcbi2naBase(b) << 2)|c.GetColorOrd()]; }

inline CNcbi8naBase::CNcbi8naBase( const CNcbiqnaBase& b, int cutoff ) : m_base( b.GetPhrapScore() > cutoff ? CNcbi8naBase( CNcbi2naBase( b&3 ) ) : Any() ) {}
inline CNcbi8naBase CNcbi8naBase::Complement() const { return s_complement[(int)(unsigned char)m_base]; }
inline CNcbi2naBase CNcbi8naBase::GetSmallestNcbi2na() const { return CNcbi2naBase( s_smallestNcbi2na[short((unsigned char)m_base)] ); }

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

inline CNcbi2naBase::CNcbi2naBase( const CNcbiqnaBase& b ) : m_base( b & 3 ) {}

////////////////////////////////////////////////////////////////////////

inline void CColorTwoBase::x_Init( char c )
{ 
	static const char * t = "0123ACGT";
	const char * x = strchr( t, toupper( c ) );
	if( x == 0 || *x == 0 ) THROW( runtime_error, "Invalid colorspace base " << c );
	m_base = x - t;
	if( m_base < 4 ) (m_base <<= 4);
	else m_base = CNcbi8naBase( CNcbi2naBase( m_base & 0x03 ) ); 
}

END_OLIGOFAR_SCOPES

#endif
