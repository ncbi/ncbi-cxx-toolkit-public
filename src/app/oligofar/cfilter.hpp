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
		fFwd1_Rev2 = 0x01,
		fFwd2_Rev1 = 0x02,
		fRev1_Fwd2 = 0x04,
		fRev2_Fwd1 = 0x08,
		fFwd1_Fwd2 = 0x10,
		fFwd2_Fwd1 = 0x20,
		fRev1_Rev2 = 0x40,
		fRev2_Rev1 = 0x80,
		fInside = fFwd1_Rev2|fFwd2_Rev1,
		fOutside = fRev1_Fwd2|fRev2_Fwd1,
		fInOrder = fFwd1_Fwd2|fRev2_Rev1,
		fBackOrder = fFwd2_Fwd1|fRev1_Rev2,
		fSolexa = fInside,
		fSOLiD  = fInOrder, 
		fOpposite = fFwd1_Rev2|fFwd2_Rev1|fRev1_Fwd2|fRev2_Fwd1,
		fConsecutive = fFwd1_Fwd2|fFwd2_Fwd1|fRev1_Rev2|fRev2_Rev1,
		fAll  = 0xff,
		// following flags are needed for implementation
		fPutFwd1 = fFwd1_Rev2|fFwd1_Fwd2,
		fPutFwd2 = fFwd2_Rev1|fFwd2_Fwd1,
		fPutRev1 = fRev1_Fwd2|fRev1_Rev2,
		fPutRev2 = fRev2_Fwd1|fRev2_Rev1,
		fLookupInFwd = fFwd1_Rev2|fFwd2_Rev1|fFwd1_Fwd2|fFwd2_Fwd1,
		fLookupInRev = fRev1_Fwd2|fRev2_Fwd1|fRev1_Rev2|fRev2_Rev1,
		fLookupForFwd = fRev1_Fwd2|fRev2_Fwd1|fFwd1_Fwd2|fFwd2_Fwd1,
		fLookupForRev = fFwd1_Rev2|fFwd2_Rev1|fRev1_Rev2|fRev2_Rev1,
		fLookupFor1 = fFwd2_Rev1|fRev2_Fwd1|fFwd2_Fwd1|fRev2_Rev1,
		fLookupFor2 = fFwd1_Rev2|fRev1_Fwd2|fFwd1_Fwd2|fRev1_Rev2,
		fCheckFwd1 = fFwd1_Rev2|fRev2_Fwd1|fFwd1_Fwd2|fFwd2_Fwd1,
		fCheckFwd2 = fFwd2_Rev1|fRev1_Fwd2|fFwd1_Fwd2|fFwd2_Fwd1,
		fCheckRev1 = fFwd2_Rev1|fRev1_Fwd2|fRev1_Rev2|fRev2_Rev1,
		fCheckRev2 = fFwd1_Rev2|fRev2_Fwd1|fRev1_Rev2|fRev2_Rev1,
		fForward = fFwd1_Fwd2|fFwd2_Fwd1,
		fReverse = fRev1_Rev2|fRev2_Rev1,
		fOppositeFwd1 = fFwd1_Rev2|fRev2_Fwd1,
		fOppositeRev1 = fFwd2_Rev1|fRev1_Fwd2,
		fNONE = 0
	};

    CFilter() : m_seqIds( 0 ), m_aligner( 0 ), 
                m_ord( -1 ), m_begin( 0 ), m_end( 0 ), 
				m_geometryFlags( fSolexa ), 
                m_minDist( 100 ), m_maxDist( 300 ), 
                m_topCnt( 10 ), m_topPct( 10 ), 
                m_scoreCutoff( 80 ), m_outputFormatter( 0 ) {}

    void Match( double score, int seqFrom, int seqTo, CQuery * query, int pairmate );
    void Match( const CHashAtom& , const char * seqBegin, const char * seqEnd, int pos );

	void PurgeQueues();

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
	void SetGeometryFlags( int f ) { m_geometryFlags = f; }

	bool CheckGeometry( int from1, int to1, int from2, int to2 ) const;
	bool InRange( int dist ) const { return dist >= m_minDist && dist <= m_maxDist; }
	bool InRange( int from, int to ) const { return InRange( to - from + 1 ); }

    int GetMaxDist() const { return m_maxDist; }
    int GetMinDist() const { return m_minDist; }
    
    CHit * SetHit( CHit * target, int pairmate, double score, int from, int to, bool allowCombinations = false );

    IAligner * GetAligner() const { return m_aligner; }

protected:
	friend class CGuideFile;

    explicit CFilter( const CFilter& );
    CFilter& operator = ( const CFilter& );

    void PurgeHit( CHit *, bool setTarget = false );
	TPendingHits::iterator x_ExpireOldHits( TPendingHits& pendingHits, int pos ) ;
	bool x_LookupInQueue( TPendingHits::iterator begin, TPendingHits& pendingHits, bool fwd, int pairmate, int maxPos, double score, int seqFrom, int seqTo, CQuery * );

protected:
    CSeqIds * m_seqIds;
    IAligner * m_aligner;
    int m_ord;
    const char * m_begin;
    const char * m_end;
	int m_geometryFlags;
    int m_minDist;
    int m_maxDist;
    int m_topCnt;
    double m_topPct;
    double m_scoreCutoff;
    TPendingHits m_pendingHits[2];
	COutputFormatter * m_outputFormatter;
};

END_OLIGOFAR_SCOPES

#endif
