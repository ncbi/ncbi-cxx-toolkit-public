#ifndef OLIGOFAR_UINTH__HPP
#define OLIGOFAR_UINTH__HPP

#include "defs.hpp"

BEGIN_OLIGOFAR_SCOPES

// 128-bit integer
class UintH 
{
public:
    UintH( Int1 lo ) : m_lo( lo ), m_hi( 0 ) {}
    UintH( Int2 lo ) : m_lo( lo ), m_hi( 0 ) {}
    UintH( Int4 lo ) : m_lo( lo ), m_hi( 0 ) {}
    UintH( Int8 lo ) : m_lo( lo ), m_hi( 0 ) {}
    UintH( Uint1 lo ) : m_lo( lo ), m_hi( 0 ) {}
    UintH( Uint2 lo ) : m_lo( lo ), m_hi( 0 ) {}
    UintH( Uint4 lo ) : m_lo( lo ), m_hi( 0 ) {}
    UintH( Uint8 lo = 0 ) : m_lo( lo ), m_hi( 0 ) {}
    UintH( const UintH& other ) : m_lo( other.GetLo() ), m_hi( other.GetHi() ) {}
    UintH( Uint8 hi, Uint8 lo ) : m_lo( lo ), m_hi( hi ) {} // HI first!!!

    UintH  operator ~  () const { return UintH( ~m_hi, ~m_lo ); }
    UintH& operator |= ( const UintH& other ) { m_lo |= other.m_lo; m_hi |= other.m_hi; return *this; }
    UintH& operator &= ( const UintH& other ) { m_lo &= other.m_lo; m_hi &= other.m_hi; return *this; }
    UintH& operator ^= ( const UintH& other ) { m_lo ^= other.m_lo; m_hi ^= other.m_hi; return *this; }

    UintH& operator <<= (unsigned i);
    UintH& operator >>= (unsigned i);

    UintH  operator -  () const { return ~(*this + ~UintH(0)); }
    UintH& operator -= ( const UintH& other ) { return *this += -other; }
    UintH& operator += ( const UintH& other ) { 
        int ha = int( m_lo >> 63 ); 
        int hb = int( other.m_lo >> 63 ); 
        int hc = int( (m_lo += other.m_lo) >> 63 );
        int carry = "\x0\x0\x1\x0\x1\x0\x1\x1"[(ha<<2)|(hb<<1)|(hc<<0)]; // TODO: check if carry flag computes right...
        m_hi += other.m_hi + carry;
        return *this;
    }

    UintH& operator *= ( Uint1 x ) { return *this = *this * UintH(x); }
    UintH& operator /= ( Uint1 x ) { return *this = *this / Uint8(x); } // for 0, 1, 2, 3, 4 only

    const Uint8& GetHi() const { return m_hi; }
    const Uint8& GetLo() const { return m_lo; }

    friend UintH operator << ( UintH a, int b ) { a <<= b; return a; }
    friend UintH operator >> ( UintH a, int b ) { a >>= b; return a; }
    friend UintH operator |  ( UintH a, const UintH& b ) { return a |= b; }
    friend UintH operator &  ( UintH a, const UintH& b ) { return a &= b; }
    friend UintH operator ^  ( UintH a, const UintH& b ) { return a ^= b; }
    friend UintH operator +  ( UintH a, const UintH& b ) { return a += b; }
    friend UintH operator -  ( UintH a, const UintH& b ) { return a -= b; }
    friend UintH operator *  ( const UintH& a, const UintH& b );
    friend UintH operator /  ( const UintH& a, const Uint8& b );

    friend ostream& operator << ( ostream& out, const UintH& h );

    bool operator == ( const UintH& other ) const { return m_lo == other.m_lo && m_hi == other.m_hi; return *this; }
    bool operator != ( const UintH& other ) const { return m_lo != other.m_lo || m_hi != other.m_hi; return *this; }

    bool operator <  ( const UintH& other ) const { return m_hi < other.m_hi || m_hi == other.m_hi && m_lo <  other.m_lo; }
    bool operator >  ( const UintH& other ) const { return m_hi > other.m_hi || m_hi == other.m_hi && m_lo >  other.m_lo; }
    bool operator <= ( const UintH& other ) const { return m_hi < other.m_hi || m_hi == other.m_hi && m_lo <= other.m_lo; }
    bool operator >= ( const UintH& other ) const { return m_hi > other.m_hi || m_hi == other.m_hi && m_lo >= other.m_lo; }

    bool operator !  () const { return !operator bool(); }

    operator bool () const { return m_lo || m_hi; }

    operator Uint8 () const { return (Uint8)m_lo; }
    operator Uint4 () const { return (Uint4)m_lo; }
    operator Uint2 () const { return (Uint2)m_lo; }
    operator Uint1 () const { return (Uint1)m_lo; }
    
    operator Int8 () const { return (Int8)m_lo; }
    operator Int4 () const { return (Int4)m_lo; }
    operator Int2 () const { return (Int2)m_lo; }
    operator Int1 () const { return (Int1)m_lo; }

protected:
    Uint8 m_lo;
    Uint8 m_hi;
};

inline UintH& UintH::operator <<= (unsigned i) 
{ 
    if( i >= 64 ) {
        m_hi = ( m_lo << (i-64) ); 
        m_lo = 0;
    } else if( i ) {
        ( m_hi <<= i ) |= ( m_lo >> ( 64 - i ) ); 
        m_lo <<= i; 
    }
    return *this; 
}

inline UintH& UintH::operator >>= (unsigned i) 
{ 
    if( i >= 64 ) {
        m_lo = ( m_hi >> (i-64) );
        m_hi = 0;
    } else if( i ) {
        ( m_lo >>= i ) |= ( m_hi << ( 64 - i ) ); 
        m_hi >>= i; 
    }
    return *this; 
}

inline UintH operator * ( const UintH& x, const UintH& y ) 
{
    Uint8 xa = x.GetLo() & 0xffffffff;
    Uint8 xb = x.GetLo() >> 32;
    Uint8 xc = x.GetHi() & 0xffffffff;
    Uint8 xd = x.GetHi() >> 32;
    Uint8 ya = y.GetLo() & 0xffffffff;
    Uint8 yb = y.GetLo() >> 32;
    Uint8 yc = y.GetHi() & 0xffffffff;
    Uint8 yd = y.GetHi() >> 32;
    
    Uint8 a = xa * ya;
    Uint8 b = xa * yb + xb * ya;
    Uint8 c = xa * yc + xb * yb + xc * ya;
    Uint8 d = xa * yd + xb * yc + xc * yb + xd * ya;
    
    return UintH( ( b >> 32 ) + c + ( d << 32 ), a + ( b << 32 ) );
}

// inline UintH& UintH::operator *= ( Uint1 x ) 
// {
//     UintH t( 0 );
//     for( int o = 0; x; ++o, (x >>= 1) )
//         if( x ) t += (*this << o);
//     return *this = t;
// }

inline UintH operator / ( const UintH& n, const Uint8& d )
{
    Uint8 remainder = 0;
    UintH quotient = n;

    for(int i = 0; i < 128; i++ ) {
        Uint8 sbit = ~((~Uint8(0))>>1) & quotient.GetHi();
        quotient <<= 1;
        remainder <<= 1;
        if (sbit) remainder |= 1;
        if(remainder >= d) {
            remainder -= d;
            quotient.m_lo |= 1;
        }
    }
    
    return quotient;
}

inline ostream& operator << ( ostream& out, const UintH& h ) 
{
    out << "[" << hex << setw(8) << setfill('0') << ((h.GetHi() >> 32)&0xffffffff)
        << "." << hex << setw(8) << setfill('0') << ((h.GetHi() >>  0)&0xffffffff) 
        << "." << hex << setw(8) << setfill('0') << ((h.GetLo() >> 32)&0xffffffff) 
        << "." << hex << setw(8) << setfill('0') << ((h.GetLo() >>  0)&0xffffffff) 
        << "]" << dec;
    return out;
}

END_OLIGOFAR_SCOPES

#endif
