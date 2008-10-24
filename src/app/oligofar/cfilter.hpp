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
		fFwd1_Rev2 = CHit::fGeometry_Fwd1_Rev2, 
		fFwd2_Rev1 = CHit::fGeometry_Fwd2_Rev1, 
		fRev1_Fwd2 = CHit::fGeometry_Rev1_Fwd2, 
		fRev2_Fwd1 = CHit::fGeometry_Rev2_Fwd1, 
		fFwd1_Fwd2 = CHit::fGeometry_Fwd1_Fwd2, 
		fFwd2_Fwd1 = CHit::fGeometry_Fwd2_Fwd1, 
		fRev1_Rev2 = CHit::fGeometry_Rev1_Rev2, 
		fRev2_Rev1 = CHit::fGeometry_Rev2_Rev1, 
		fInside = fFwd1_Rev2|fFwd2_Rev1,
		fOutside = fRev1_Fwd2|fRev2_Fwd1,
		fInOrder = fFwd1_Fwd2|fRev2_Rev1,
		fBackOrder = fFwd2_Fwd1|fRev1_Rev2,
		fSolexa = fInside,
		fSOLiD  = fInOrder, 
		fOpposite = fFwd1_Rev2|fFwd2_Rev1|fRev1_Fwd2|fRev2_Fwd1,
		fConsecutive = fFwd1_Fwd2|fFwd2_Fwd1|fRev1_Rev2|fRev2_Rev1,
		fAll  = fOpposite|fConsecutive,
		fNONE = 0
	};

    CFilter() : m_seqIds( 0 ), m_aligner( 0 ), 
                m_ord( -1 ), m_begin( 0 ), m_end( 0 ), 
				m_geometryFlags( fSolexa ), 
                m_minDist( 100 ), m_maxDist( 300 ), 
                m_topCnt( 10 ), m_topPct( 10 ), 
                m_scoreCutoff( 80 ), m_outputFormatter( 0 ) {}

    void Match( const CHashAtom& , const char * seqBegin, const char * seqEnd, int pos );

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
	void SetGeometryFlags( unsigned f ) { m_geometryFlags = f; }

    int GetMaxDist() const { return m_maxDist; }
    int GetMinDist() const { return m_minDist; }
    
    IAligner * GetAligner() const { return m_aligner; }

    bool CheckGeometry( int from1, int to1, int from2, int to2 ) const {
        return m_geometryFlags & CHit::ComputeGeometry( from1, to1, from2, to2 );
    }

    bool InRange( int from, int to ) const { ASSERT( to > from ); return InRange( to - from + 1 ); }
    bool InRange( int length ) const { ASSERT( length > 0 ); return length >= m_minDist && length <= m_maxDist ; }

	void PurgeQueueToTheEnd();

private:
    explicit CFilter( const CFilter& );
    CFilter& operator = ( const CFilter& );

protected:
	friend class CGuideFile;

    enum EHitUpdateMode {
        eExtend = 0x01,
        eChain = 0x02
    };

    void PurgeHit( CHit *, bool setTarget = false );
    void ProcessMatch( double score, int from, int to, bool reverse, CQuery * query, int pairmate );

    bool SingleHitSetPair( int seqFrom, int seqTo, bool reverse, int pairmate, double score, CHit * hit, TPendingHits& toAdd );
    bool PairedHitSetPair( int seqFrom, int seqTo, bool reverse, int pairmate, double score, CHit * hit, TPendingHits& toAdd, EHitUpdateMode );
    void PurgeQueue( int bottomPos );

protected:
    CSeqIds * m_seqIds;
    IAligner * m_aligner;
    int m_ord;
    const char * m_begin;
    const char * m_end;
	unsigned m_geometryFlags;
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
