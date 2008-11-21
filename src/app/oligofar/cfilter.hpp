#ifndef OLIGOFAR_CFILTER__HPP
#define OLIGOFAR_CFILTER__HPP

#include "chit.hpp"
#include "cseqids.hpp"
#include "cqueryhash.hpp"
#include "cseqvecprocessor.hpp"

BEGIN_OLIGOFAR_SCOPES

class IAligner;
class COutputFormatter;
class CFilter : public CSeqVecProcessor::ICallback
{
public:
    typedef multimap<int,CHit*> TPendingHits;

    enum EGeometryFlags {
        eFF = CHit::fGeometry_Fwd1_Fwd2,
        eRF = CHit::fGeometry_Rev1_Fwd2,
        eFR = CHit::fGeometry_Fwd1_Rev2,
        eRR = CHit::fGeometry_Rev1_Rev2,
        eCentripetal = eFR,
        eCentrifugal = eRF,
        eIncremental = eFF,
        eDecremental = eRR,
        eSolexa = eCentripetal,
        eSOLiD  = eIncremental,
        eBAD = -1
    };

    // Note: it is essential that geometry does not allow mixed cases. 
    // Each individual component hit should either be added to 
    // a positional queue, or lookup in the queue for the hit 
    // should be performed. 

    CFilter() : m_seqIds( 0 ), m_aligner( 0 ), 
                m_ord( -1 ), m_begin( 0 ), m_end( 0 ), 
                m_geometry( eSolexa ), 
                m_minDist( 100 ), m_maxDist( 300 ), m_reserveDist( 255 ),
                m_topCnt( 10 ), m_topPct( 10 ), 
                m_scoreCutoff( 80 ), m_outputFormatter( 0 ) {}

    void Match( const CHashAtom& , const char * seqBegin, const char * seqEnd, int pos );
    void PurgeQueueToTheEnd();

    virtual void SequenceBegin( const TSeqIds&, int oid );
    virtual void SequenceBuffer( CSeqBuffer* );
    virtual void SequenceEnd();

    void SetSeqIds( CSeqIds* seqIds ) { m_seqIds = seqIds; }
    void SetAligner( IAligner* aligner ) { m_aligner = aligner; }
    void SetOutputFormatter( COutputFormatter * f ) { m_outputFormatter = f; }

    void SetMaxDist( int d ) { m_maxDist = d; }
    void SetMinDist( int d ) { m_minDist = d; }
    void SetReserveDist( int d ) { m_reserveDist = d; }
    void SetTopPct( double pct ) { m_topPct = pct; }
    void SetTopCnt( int cnt ) { m_topCnt = cnt; }
    void SetScorePctCutoff( double cutoff ) { m_scoreCutoff = cutoff; }
    void SetGeometry( unsigned g ) { m_geometry = g; }

    bool CheckGeometry( int from1, int to1, int from2, int to2 ) const;
    bool InRange( int a, int b ) const { return InRange( b - a + 1 ); }
    bool InRange( int d ) const { return d <= m_maxDist && d >= m_minDist; }

    int GetMaxDist() const { return m_maxDist; }
    int GetMinDist() const { return m_minDist; }
    
    IAligner * GetAligner() const { return m_aligner; }

private:
    explicit CFilter( const CFilter& );
    CFilter& operator = ( const CFilter& );

protected:
    friend class CGuideFile;

    void PurgeHit( CHit *, bool setTarget = false );
    void ProcessMatch( double score, int from, int to, bool reverse, CQuery * query, int pairmate );
    bool LookupInQueue( double score, int from, int to, bool reverse, CQuery * query, int pairmate, TPendingHits& toAdd, bool canPurgeQueue );
    void PurgeQueue( int bottomPos );
    void MatchConv( const CHashAtom& , const char * seqBegin, const char * seqEnd, int pos );

protected:
    CSeqIds * m_seqIds;
    IAligner * m_aligner;
    int m_ord;
    const char * m_begin;
    const char * m_end;
    unsigned m_geometry;
    int m_minDist;
    int m_maxDist;
    int m_reserveDist; // extra distance to keep hits in queue: should take into account stride, window count....
    int m_topCnt;
    double m_topPct;
    double m_scoreCutoff;
    TPendingHits m_pendingHits;
    COutputFormatter * m_outputFormatter;
};

END_OLIGOFAR_SCOPES

#endif
