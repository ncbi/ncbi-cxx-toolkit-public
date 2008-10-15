#ifndef OLIGOFAR_DUST__HPP
#define OLIGOFAR_DUST__HPP

#include "cbithacks.hpp"
#include "assert.h"

BEGIN_OLIGOFAR_SCOPES

class CComplexityMeasure
{
public:
    CComplexityMeasure( int windowSize = 2 );
    operator double() const { return Get(); }
    void Add( int tripletId );
    void Del( int tripletId );
    double Get() const {  return m_windowSize > 3 ? m_score / 2 / ( m_windowSize - 3 ) : 0; }
    int GetWindowSize() const { return m_windowSize; }
    void Reset();
protected:
    double m_score;
    int m_windowSize;
    int m_tripletCounts[64];
};

////////////////////////////////////////////////////////////////////////

inline CComplexityMeasure::CComplexityMeasure( int windowSize ) 
    : m_score( windowSize > 1 ? (windowSize - 2)*(windowSize - 3) : 0 ), m_windowSize( windowSize )
{
    fill( m_tripletCounts, m_tripletCounts + 64, 0 );
    if( m_windowSize > 2 ) m_tripletCounts[0] = m_windowSize - 2;
}

inline void CComplexityMeasure::Add( int tripletId ) 
{
    assert( tripletId >= 0 );
    assert( tripletId < 64 );
    
    if( m_windowSize >= 2 ) {
        int & tripletCount = m_tripletCounts[tripletId];
        m_score -= tripletCount * ( tripletCount - 1 );
        m_score += tripletCount * ( tripletCount + 1 );
        tripletCount ++;
    }
    m_windowSize ++;
}

inline void CComplexityMeasure::Del( int tripletId )
{
    if( m_windowSize > 2 ) {
        int & tripletCount = m_tripletCounts[tripletId];
        if( tripletCount <= 0 ) 
			throw logic_error( "CComplexityMeasure::Del(int tripletId): Deleting non-existing triplet!" );
        tripletCount --;
        m_score -= tripletCount * ( tripletCount + 1 );
        m_score += tripletCount * ( tripletCount - 1 );
    }
    m_windowSize -- ;
}

inline void CComplexityMeasure::Reset()
{
    m_windowSize = 2;
    m_score = 0;
    fill( m_tripletCounts, m_tripletCounts + 64, 0 );
}

END_OLIGOFAR_SCOPES

#endif
