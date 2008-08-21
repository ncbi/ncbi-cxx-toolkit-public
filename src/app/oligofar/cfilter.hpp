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
    CFilter() : m_seqIds( 0 ), m_aligner( 0 ), 
                m_ord( -1 ), m_begin( 0 ), m_end( 0 ), 
                m_minDist( 100 ), m_maxDist( 300 ), 
                m_topCnt( 10 ), m_topPct( 10 ), 
                m_scoreCutoff( 80 ), m_outputFormatter( 0 ) {}

    void Match( double score, int seqFrom, int seqTo, CQuery * query, int pairmate );
    void Match( const CQueryHash::SHashAtom& , const char * seqBegin, const char * seqEnd, int pos );

    virtual void SequenceBegin( const TSeqIds&, int oid );
    virtual void SequenceBuffer( CSeqBuffer* );
    virtual void SequenceEnd();

    void SetSeqIds( CSeqIds* seqIds ) { m_seqIds = seqIds; }
    void SetAligner( IAligner* aligner ) { m_aligner = aligner; }
	void SetOutputFormatter( COutputFormatter * f ) { m_outputFormatter = f; }

    void SetMaxDist( int d ) { m_maxDist = d; }
    void SetMinDist( int d ) { m_minDist = d; }
    void SetTopPct( double pct ) { m_topPct = pct; }
    void SetTopCnt( int cnt ) { m_topCnt = cnt; }
    void SetScorePctCutoff( double cutoff ) { m_scoreCutoff = cutoff; }

    int GetMaxDist() const { return m_maxDist; }
    int GetMinDist() const { return m_minDist; }
    
    CHit * SetHit( CHit * target, int pairmate, double score, int from, int to, bool allowCombinations = false );

protected:
	friend class CGuideFile;
    void PurgeHit( CHit * );
protected:
    CSeqIds * m_seqIds;
    IAligner * m_aligner;
    int m_ord;
    const char * m_begin;
    const char * m_end;
    int m_minDist;
    int m_maxDist;
    int m_topCnt;
    double m_topPct;
    double m_scoreCutoff;
    TPendingHits m_pendingHits;
	COutputFormatter * m_outputFormatter;
};

END_OLIGOFAR_SCOPES

#endif
