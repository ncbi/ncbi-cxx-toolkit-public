#ifndef OLIGOFAR__CALIGNPARAM__HPP
#define OLIGOFAR__CALIGNPARAM__HPP

#include "cintron.hpp"

BEGIN_OLIGOFAR_SCOPES

class CAlignParam
{
public:
    typedef CIntron::TSet  TSpliceSet;

    CAlignParam( 
        int maxInsLen = 2,
        int maxDelLen = 2,
        int maxInsCnt = 3,
        int maxDelCnt = 3 ) :
        m_maxInsLen( min( maxInsLen, maxInsCnt ) ),
        m_maxDelLen( min( maxDelLen, maxDelCnt ) ),
        m_maxInsCnt( maxInsCnt ),
        m_maxDelCnt( maxDelCnt ),
        m_splicesSize( 0 ),
        m_splicesBack( 0 ) {}

    int  GetMaxInsertionLength() const { return m_maxInsLen; }
    int  GetMaxInsertionsCount() const { return m_maxInsCnt; }
    int  GetMaxDeletionLength() const { return m_maxDelLen; }
    int  GetMaxDeletionsCount() const { return m_maxDelCnt; }
    int  GetMaxIndelCount() const { return max( GetMaxInsertionsCount(), GetMaxDeletionsCount() ); }
    int  GetMaxIndelLength() const { return max( GetMaxInsertionLength(), GetMaxDeletionLength() ); }

    int  GetMaxSplicesSize() const { return m_splicesSize; }
    int  GetMaxSpliceOverlap() const { return -m_splicesBack; }
    
    void SetMaxInsertionLength( int x )  { m_maxInsLen = x; }
    void SetMaxDeletionLength( int x ) { m_maxDelLen = x; }
    void SetMaxInsertionCount( int x ) { m_maxInsCnt = x; }
    void SetMaxDeletionCount( int x ) { m_maxDelCnt = x; }
    void SetMaxIndelLength( int x ) { SetMaxInsertionLength(x); SetMaxDeletionLength(x); }
    void SetMaxIndelCount( int x ) { SetMaxInsertionCount(x); SetMaxDeletionCount(x); }

    const TSpliceSet& GetSpliceSet() const { return m_spliceSet; }
    void SetSpliceSet( const TSpliceSet& ss );
    void AddSplice( const CIntron& i );

protected:
    int    m_maxInsLen;
    int    m_maxDelLen;
    int    m_maxInsCnt;
    int    m_maxDelCnt;
    int    m_splicesSize; // accumulated size of all introns
    int    m_splicesBack; // maximal overlap (negative value)
    TSpliceSet m_spliceSet;
};

inline void CAlignParam::SetSpliceSet( const TSpliceSet& ss )
{
    m_spliceSet.clear(); 
    m_splicesSize = 0;
    m_splicesBack = 0;
    ITERATE( TSpliceSet, i, ss ) AddSplice( *i );
}

inline void CAlignParam::AddSplice( const CIntron& i )
{
    if( i.GetMin() == 0 && i.GetMax() == 0 ) return;
    if( i.GetMax() > 0 ) m_splicesSize += i.GetMax();
    m_spliceSet.insert( i );
    ASSERT( i.GetMin() <= i.GetMax() );
    if( i.GetMin() < m_splicesBack ) m_splicesBack = i.GetMin();
}

END_OLIGOFAR_SCOPES

#endif
