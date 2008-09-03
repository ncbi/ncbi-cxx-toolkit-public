#ifndef OLIGOFAR_TSEQREF__HPP
#define OLIGOFAR_TSEQREF__HPP

#include "cseqcoding.hpp"

BEGIN_OLIGOFAR_SCOPES

template<class Base,int incr,CSeqCoding::ECoding coding>
class TSeqRef 
{
public:
	typedef TSeqRef<Base,incr,coding> TSelf;
	typedef Base TBase;
	TSeqRef( const char * ptr = 0 ) : m_ptr( ptr ) {}
    TSelf& Incr() { m_ptr += incr; return *this; }
    TSelf& Decr() { m_ptr -= incr; return *this; }
	TSelf& operator ++ () { m_ptr += incr; return *this; }
	TSelf  operator ++ (int) { TSelf other( *this ); ++*this; return other; }
	TSelf& operator -- () { m_ptr -= incr; return *this; }
	TSelf  operator -- (int) { TSelf other( *this ); --*this; return other; }
	TSelf& operator += (int cnt) { m_ptr += cnt * incr; return *this; } 
	TSelf& operator -= (int cnt) { m_ptr -= cnt * incr; return *this; }
    TSelf  operator +  (int cnt) const { return TSelf( m_ptr + cnt * incr ); }
    TSelf  operator -  (int cnt) const { return TSelf( m_ptr - cnt * incr ); }
    friend TSelf operator + ( int cnt, const TSelf& s ) { return TSelf( s.m_ptr + cnt * incr ); }
    friend TSelf operator - ( int cnt, const TSelf& s ) { return TSelf( s.m_ptr - cnt * incr ); }
    friend int operator - ( const TSelf& ref, const char * ptr ) { return ( ref.m_ptr - ptr ) / incr; }
    friend int operator - ( const TSelf& ref, const TSelf& oth ) { return ( ref.m_ptr - oth.m_ptr ) / incr; }
    friend int operator - ( const char * ptr, const TSelf& ref ) { return ( ptr - ref.m_ptr ) / incr; }
	bool operator == ( const char * ptr ) const { return m_ptr == ptr; }
	bool operator != ( const char * ptr ) const { return m_ptr != ptr; }
	bool operator <  ( const char * ptr ) const { return ( m_ptr - ptr ) / incr <  0; } 
	bool operator >  ( const char * ptr ) const { return ( m_ptr - ptr ) / incr >  0; } 
	bool operator <= ( const char * ptr ) const { return ( m_ptr - ptr ) / incr <= 0; } 
	bool operator >= ( const char * ptr ) const { return ( m_ptr - ptr ) / incr >= 0; } 
	static bool IsForward() { return incr > 0; }
	static bool IsReverse() { return incr < 0; }
    static int  GetIncrement() { return incr; }
	static CSeqCoding::EStrand GetStrand() { return incr < 0 ? CSeqCoding::eStrand_neg : CSeqCoding::eStrand_pos; }
	static CSeqCoding::ECoding GetCoding() { return coding; }
    Base operator [] ( int i ) const { return Base( m_ptr + i * incr, GetStrand() ); }
    Base operator * () const { return GetBase(); }
	Base GetBase() const { return Base( m_ptr, GetStrand() ); }
	Base GetBase( CSeqCoding::EStrand strand ) const { return Base( m_ptr, strand ); }
    TSeqRef<Base,-incr,coding> Reverse() const { return TSeqRef<Base,-incr,coding>( m_ptr ); }
    const char * GetPointer() const { return m_ptr; }
 	operator const char * () const { return m_ptr; }
// 	const char * operator [] ( int i ) const { return m_ptr + i * incr; }
// 	Base GetBase( CSeqCoding::EStrand s ) const { return Base( m_ptr, s == CSeqCoding::eStrand_pos ? incr < 0 : incr > 0 ); }
// 	Base GetBase( int i, CSeqCoding::EStrand s ) const { return Base( m_ptr + i * incr, s == CSeqCoding::eStrand_pos ? incr < 0 : incr > 0 ); }
protected:
	const char * m_ptr;
};


END_OLIGOFAR_SCOPES

#endif
