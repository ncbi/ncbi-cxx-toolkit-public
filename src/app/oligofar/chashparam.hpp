#ifndef OLIGOFAR_CHASHPARAM__HPP
#define OLIGOFAR_CHASHPARAM__HPP

#include "array_set.hpp"

BEGIN_OLIGOFAR_SCOPES

class CHashParam 
{
public:
    typedef array_set<Uint1> TSkipPositions;
    
    CHashParam() : 
        m_windowSize( 22 ), 
        m_wordSize( 11 ), 
        m_strideSize( 1 ), 
        m_hashBits( 22 ),
        m_hashMismatches( 1 ), 
        m_hashIndels( 0 ) {}
    
    int  GetWindowSize() const { return m_windowSize; }
    int  GetWordSize() const { return m_wordSize; }
    int  GetStrideSize() const { return m_strideSize; }
    int  GetHashBits() const { return m_hashBits; }
    int  GetHashMismatches() const { return m_hashMismatches; }
    bool GetHashIndels() const { return m_hashIndels; }

    void SetWindowSize( int x ) { m_windowSize = x; }
    void SetWordSize( int x ) { m_wordSize = x; }
    void SetStrideSize( int x ) { m_strideSize = x; }
    void SetHashBits( int x ) { m_hashBits = x; }
    void SetHashMismatches( int x ) { m_hashMismatches = x; }
    void SetHashIndels( bool x ) { m_hashIndels = x; }

    bool ValidateParam( const TSkipPositions& pos, string& msg ) const { /* TODO: implement */ return true; }

protected:
    int  m_windowSize;
    int  m_wordSize;
    int  m_strideSize;
    int  m_hashBits;
    int  m_hashMismatches;
    bool m_hashIndels;
};


END_OLIGOFAR_SCOPES

#endif
