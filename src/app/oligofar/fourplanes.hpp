#ifndef OLIGOFAR_FOURPLANES__HPP
#define OLIGOFAR_FOURPLANES__HPP

#include "types.hpp"
#include "cbithacks.hpp"
#include "cseqcoding.hpp"
#include "iupac-util.hpp"

BEGIN_OLIGOFAR_SCOPES
BEGIN_SCOPE( fourplanes )

// reverse rotation here

class CHashPlanes
{
public:
    enum { kBitsPerBase = 1 };

    CHashPlanes( int wordSize );

//    int GetBase( int i ) const;
    
    TPlaneMask GetFootprintMask() const { return m_footprintMask; }
    THashValue GetPlane( int base ) const { return m_planes[base&0x03] & m_footprintMask; }
//    THashValue GetPlanes( int base ) const;
    TPlaneMask GetAmbiguityMask() const { return m_ambiguityMask & m_footprintMask; }
    TAlternativesCount GetAlternativesCount() const { return m_alternativesCount; }
    int   GetAmbiguityCount() const { return m_ambiguityCount; }
    int   GetWordSize() const { return m_wordSize; }
    
    void AddIupacNa( char c );
    void AddBaseMask( int base );
    void Reset();
protected:
    TPlaneMask m_footprintMask;
    int        m_wordSize;
    TAlternativesCount m_alternativesCount;
    int        m_ambiguityCount;
    TPlaneMask m_ambiguityMask;
    TPlaneMask m_planes[4];
};

class CHashCode
{
public:
    enum { kBitsPerBase = 2 };

    CHashCode( int wordsize );

    void Reset();

    int GetOldestTriplet() const { return int( m_value & 0x3f ); }
    int GetNewestTriplet() const { return int( (m_value >> ((m_wordSize - 3)*int(kBitsPerBase))) & 0x3f ); }

    void AddIupacNa( char c );
    void AddBaseCode( int code );

    THashMask  GetFootprintMask() const { return m_footprintMask; }
    THashValue GetHashValue() const { return m_value & m_footprintMask; }

protected:
    THashValue m_value;
    THashValue m_wordSize;
    THashMask  m_footprintMask;
};

class CHashGenerator
{
public:
    friend class CHashIterator;
    
    CHashGenerator( int wordSize );

    void Reset();

    void AddIupacNa( char iupacna );
    void AddBaseMask( char ncbi8na );
    
    int   GetWordSize() const { return m_hashPlanes.GetWordSize(); }
    int   GetOldestTriplet() const { return m_hashCode.GetOldestTriplet(); }
    int   GetNewestTriplet() const { return m_hashCode.GetNewestTriplet(); }
    int   GetAmbiguityCount() const { return m_hashPlanes.GetAmbiguityCount(); }
    TAlternativesCount GetAlternativesCount() const { return m_hashPlanes.GetAlternativesCount(); }
    THashValue GetPlane( int base ) const { return m_hashPlanes.GetPlane(base); }
    THashValue GetHashValue() const { return m_hashCode.GetHashValue(); }

    const CHashPlanes& GetHashPlanes() const { return m_hashPlanes; }
    const CHashCode& GetHashCode() const { return m_hashCode; }
protected:
    CHashCode m_hashCode;
    CHashPlanes m_hashPlanes;
};

class CHashIterator
{
public:
    CHashIterator( const CHashGenerator& generator, 
                   TPlaneMask baseMask = ~TPlaneMask(0), 
                   THashMask codeMask = ~THashMask(0) );

    const CHashGenerator& GetGenerator() const { return m_generator; }
    
    int GetWordSize() const { return m_generator.GetWordSize(); }
    THashValue GetValue() const { return m_generator.m_hashCode.GetFootprintMask() & m_value; }
    
    const THashValue& operator  * () const { return m_value; }
    const THashValue* operator -> () const { return &m_value; }

    operator bool () const { return m_planeMask != 0; }

    CHashIterator& operator ++ ();
    CHashIterator operator ++ (int) { CHashIterator x(*this); ++*this; return x; }

protected:
    const CHashGenerator& m_generator;
    THashValue m_value;
    THashMask  m_hashMask;
    TPlaneMask m_planeMask;
    TPlaneMask m_baseMask;
    int m_status;
    int m_position;

protected:
    
    bool x_NextBaseCallAtPosition();
    int  x_SearchFirstAmbiguousPosition();
    void x_ResetPosition();
};

////////////////////////////////////////////////////////////////////////
// implementations

////////////////////////////////////////////////////////////////////////
// CHashIterator

inline int CHashIterator::x_SearchFirstAmbiguousPosition()
{
    m_position = 0;
    if( TPlaneMask m = m_generator.m_hashPlanes.GetAmbiguityMask() & m_baseMask ) {
        while( ! (m_planeMask & m) ) {
            m_planeMask <<= CHashPlanes::kBitsPerBase;
            m_hashMask  <<= CHashCode::kBitsPerBase;
            ++m_position;
        }
    }
    return m_position;
}

inline CHashIterator::CHashIterator( const CHashGenerator& generator, TPlaneMask baseMask, THashMask codeMask ) :
    m_generator( generator ), 
    m_value( generator.m_hashCode.GetHashValue() & codeMask ),
    m_hashMask( 0x3 ),
    m_planeMask( 1 ),
    m_baseMask( baseMask )
{
    x_SearchFirstAmbiguousPosition();
    m_status = int( ( m_hashMask & m_value ) >> (2 * m_position) );
}

inline void CHashIterator::x_ResetPosition()
{
    m_hashMask = 0x3;
    m_planeMask = 1;
    x_SearchFirstAmbiguousPosition();
    m_status = int( (m_hashMask & m_generator.m_hashCode.GetHashValue()) >> (2 * m_position) );
}

inline bool CHashIterator::x_NextBaseCallAtPosition()
{
    static THashValue basePattern[4] = { 0x0000000000000000ULL, 0x5555555555555555ULL, 0xaaaaaaaaaaaaaaaaULL, 0xffffffffffffffffULL };
    
    while( ++m_status < 4 ) {
        if( m_planeMask & m_generator.m_hashPlanes.GetPlane( m_status ) ) { 
            m_value = (m_value & ~m_hashMask) | (basePattern[m_status] & m_hashMask);  // can be optimized
            return true;
        }
    }
    // restore base value at the position
    m_value = ( m_value & ~m_hashMask ) | ( m_generator.m_hashCode.GetHashValue() & m_hashMask ); // can be optimized
    return false;
}

inline CHashIterator& CHashIterator::operator ++ () 
{
    TPlaneMask ambMask = m_generator.m_hashPlanes.GetAmbiguityMask() & m_baseMask;
    if( ambMask == 0 ) { m_planeMask = 0; return *this; }

    // next value at this position
    if( x_NextBaseCallAtPosition() ) { return *this; }
    
    // next position
    do {
        m_planeMask <<= CHashPlanes::kBitsPerBase;
        m_hashMask  <<= CHashCode::kBitsPerBase;
        ++m_position;
        if( m_planeMask & ambMask ) {
            m_status = int( (m_value & m_hashMask) >> (2 * m_position) );
            if( x_NextBaseCallAtPosition() ) {
                x_ResetPosition();
                return *this;
            }
        }
    } while( ( m_planeMask <= ambMask ) && m_position < GetWordSize() );//while( ! ( m_planeMask & ambMask ) );

    m_planeMask = 0;
    return *this;
}

////////////////////////////////////////////////////////////////////////
// CHashGenerator

inline CHashGenerator::CHashGenerator( int wordSize ) : m_hashCode( wordSize ), m_hashPlanes( wordSize )
{}

inline void CHashGenerator::Reset()
{
    m_hashCode.Reset();
    m_hashPlanes.Reset();
}

inline void CHashGenerator::AddIupacNa( char iupacna )
{
    m_hashCode.AddIupacNa( iupacna );
    m_hashPlanes.AddIupacNa( iupacna );
}

inline void CHashGenerator::AddBaseMask( char ncbi8na )
{
    m_hashCode.AddBaseCode( Ncbi4na2Ncbi2na( ncbi8na ) );
    m_hashPlanes.AddBaseMask( ncbi8na );
}

////////////////////////////////////////////////////////////////////////
// CHashPlanes

inline CHashPlanes::CHashPlanes( int wordSize ) : 
    m_footprintMask( CBitHacks::WordFootprint<TPlaneMask>( wordSize*int(kBitsPerBase) ) ), 
    m_wordSize( wordSize ),
    m_alternativesCount( TAlternativesCount(1) << (2 * m_wordSize) ), 
    m_ambiguityCount( wordSize ), 
    m_ambiguityMask( m_footprintMask ) 
{ 
    fill( m_planes, m_planes + 4, m_footprintMask ); 
}

// inline int CHashPlanes::GetBase( int i ) const 
// {
//     TPlaneMask m(1 << i);
//     int ret = 0;
//     for( int x = 3; x >= 0; ++x ) { ret <<= 1; if( m_planes[x]&m ) ret |= 1; }
//     return ret;
// }

// inline THashValue CHashPlanes::GetPlanes( int base ) const 
// {
//     THashValue ret = 0;
//     int b = base;
//     for( int i = 0; i < 4; ++i, b >>= 1 ) if( b & 1 ) ret |= m_planes[i];
//     return ret;
// }
            
inline void CHashPlanes::AddIupacNa( char c ) 
{ 
    AddBaseMask( Iupacna2Ncbi4na(c) ); 
}

inline void CHashPlanes::AddBaseMask( int base ) 
{
    if( base == 0 ) return;
    int b = base;
    int c = 0;
    int d = 0;
    TPlaneMask p = 1 << (m_wordSize - 1);
//    TPlaneMask P = ~p;
    for( int i = 0; i < 4; ++i, b >>= 1 ) {
        if( m_planes[i] & 1 ) d++;
        m_planes[i] >>= 1;
        if( b & 1 ) {
            m_planes[i] |= p;
            c++;
//         } else {
//             m_planes[i] &= P;
        }
    }
    if( d ) m_alternativesCount /= d;
    if( c ) m_alternativesCount *= c;
    if( m_ambiguityMask & 1 ) { m_ambiguityCount--; }
    m_ambiguityMask >>= 1;
    if( Ncbi4naIsAmbiguous( base ) ) { m_ambiguityMask |= p; m_ambiguityCount++; }
//    else { m_ambiguityMask &= P; }
}

inline void CHashPlanes::Reset()
{
    fill( m_planes, m_planes + 4, m_footprintMask ); 
    m_ambiguityMask = m_footprintMask;
    m_alternativesCount = TAlternativesCount(1) << (2 * m_wordSize);
    m_ambiguityCount = m_wordSize;
}

////////////////////////////////////////////////////////////////////////
// CHashCode

inline CHashCode::CHashCode( int wordSize ) :  
    m_value( 0 ), m_wordSize( wordSize ),
    m_footprintMask( CBitHacks::WordFootprint<THashMask>( wordSize * int(kBitsPerBase) ) )
{}

inline void CHashCode::AddIupacNa( char c )
{
    int x = Iupacna2Ncbi2na(c);
    if( x != 0xf ) AddBaseCode( x );
}

inline void CHashCode::AddBaseCode( int code ) 
{
    m_value >>= kBitsPerBase;
    m_value |= (code & 0x3) << ((m_wordSize - 1)*int(kBitsPerBase));
}

inline void CHashCode::Reset()
{
    m_value = 0;
}

END_SCOPE( fourplanes )
END_OLIGOFAR_SCOPES

#endif
