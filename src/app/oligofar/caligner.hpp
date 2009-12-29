#ifndef OLIGOFAR__CALIGNER__HPP
#define OLIGOFAR__CALIGNER__HPP

#include "iscore.hpp"
#include "ialigner.hpp"
#include "calignparam.hpp"
#include "cintron.hpp"
#include "ctranscript.hpp"

#include <iostream>

BEGIN_OLIGOFAR_SCOPES

class IScore;
class CScoreParam;
class CScoringFactory;
class CAligner : public IAligner, public CTrBase
{
public:
    CAligner( CScoringFactory * sf ) : 
        m_scoringFactory( sf ), 
        m_scoreParam( 0 ), 
        m_alignParam( 0 ),
        m_score( 0 ), 
        m_extentionPenaltyDropoff( -1000000 ),
        m_matrixSize( 0 ),
        m_matrixOffset( -1 ),
        m_queryBases( 0 ),
        m_queryBasesChecked( 0 ),
        m_scoreCalls( 0 ),
        m_totalCalls( 0 ),
        m_successAligns( 0 ),
        m_algoX0all( 0 ),
        m_algoX0end( 0 ),
        m_algoX0win( 0 ),
        m_algoSWend( 0 ),
        m_algoSWwin( 0 ),
        m_wordDistance( 0 )
        {}

    ~CAligner();

    virtual void SetWordDistance( int k ) { m_wordDistance = k; }
    virtual void SetQueryCoding( CSeqCoding::ECoding );
    virtual void SetSubjectCoding( CSeqCoding::ECoding );
    virtual void SetQueryStrand( CSeqCoding::EStrand );
    virtual void SetSubjectStrand( CSeqCoding::EStrand );

    virtual void SetQuery( const char * begin, int len );
    virtual void SetSubject( const char * begin, int len );
    virtual void SetSubjectAnchor( int first, int last ); // last included 
    virtual void SetQueryAnchor( int first, int last ); // last included

    virtual void SetSubjectGuideTranscript( const TTranscript& tr );

    virtual void SetPenaltyLimit( double limit );
    virtual void SetExtentionPenaltyDropoff( double limit );

    virtual bool Align();

    virtual double GetPenalty() const { return m_penalty; }
    virtual int GetSubjectFrom() const { return m_posS1; }
    virtual int GetSubjectTo() const { return m_posS2; }
    virtual int GetQueryFrom() const { return m_posQ1; }
    virtual int GetQueryTo() const { return m_posQ2; }

    virtual const TTranscript& GetSubjectTranscript() const { return m_transcript; }

    virtual CScoringFactory * GetScoringFactory() const { return m_scoringFactory; }

    const CAlignParam::TSpliceSet& GetSpliceSet() const { return m_alignParam->GetSpliceSet(); }
    int   GetSpliceCount() const { return GetSpliceSet().size(); }
    bool  NoSplicesDefined() const { return GetSpliceSet().empty(); }

    class CCell : public TTrItem
    {
    public:
        CCell( double score = 0, EEvent ev = eEvent_NULL, unsigned cnt = 1 ) : 
            TTrItem( ev, cnt ), m_score( score ) {}
        double GetScore() const { return m_score; }
    protected:
        double m_score;
    };
    void PrintStatistics( ostream& out = cerr );

protected:

    class CMaxGapLengths
    {
    public:
        CMaxGapLengths( double maxpenalty, const CScoreParam * sc, const CAlignParam * ap );
        bool AreIndelsAllowed() const { return m_maxInsLen|m_maxDelLen; }
        int GetMaxDelLength() const { return m_maxDelLen; }
        int GetMaxInsLength() const { return m_maxInsLen; }
    protected:
        int m_maxDelLen;
        int m_maxInsLen;
    };

    class CIndelLimits : public CMaxGapLengths
    {
    public:
        CIndelLimits( double pm, const CScoreParam * sc, const CAlignParam * ap );
        bool AreIndelsAllowed() const { return m_delBand|m_insBand; }
        int GetDelBand() const { return m_delBand; }
        int GetInsBand() const { return m_insBand; }
    protected:
        int m_delBand;
        int m_insBand;
    };

    class CDiagonalScores
    {
    public:
        CDiagonalScores( double id ) : m_aq( 0 ), m_wq( 0 ), m_vq( 0 ), m_id( id ) {}
        CDiagonalScores& Add( double score ) { m_aq += m_id; m_wq += (m_vq = score); return *this; }
        double GetLastScore() const { return m_vq; }
        double GetLastPenalty() const { return m_id - m_vq; }
        double GetAccumulatedBestScore() const { return m_aq; }
        double GetAccumulatedPenalty() const { return m_aq - m_wq; }
        double GetAccumulatedScore() const { return m_wq; }
        bool ExceedsPenalty( double abslimit ) const { return GetAccumulatedPenalty() > abslimit; }
        bool LastIsMatch() const { return m_vq > 0; }
        EEvent GetLastEvent() const { return LastIsMatch() ? eEvent_Match : eEvent_Replaced; }
        ostream& Print( ostream& o ) const {
            return o << "CDiagonalScores"
                << DISPLAY( m_aq ) << DISPLAY( m_wq ) << DISPLAY( m_vq ) << DISPLAY( m_aq - m_wq ) << DISPLAY( GetLastEvent() );
        }
    protected:
        double m_aq;
        double m_wq;
        double m_vq;
        const double m_id;
    };
    
    enum EAlignModeFlags {
        eAlign_startAfterSplice = 0x01,
        eAlign_endOnSplice = 0x02
    };

    void MakeLocalSpliceSet( CIntron::TSet&, int qdir, int qpos ) const;
    double Score( const char * q, const char * s ) const;

    // AlignWindow() may adjust alignment positions - which is useful especially in the case of indel of 
    // length 2 the window boundary (otherwise neither AlignWindow nor AlignDiag then can see the all indel, 
    // which leads to double accounting for gap base score)
    bool AlignWindow( const char * & q, const char * & Q, const char * & s, const char * & S, TTranscript& t );
    bool AlignWinTwoDiag( const char * & q, const char * & Q, const char * & s, const char * & S, int splice, int smin, int smax, TTranscript& t );
    bool AlignDiag( const char * q, const char * Q, const char * s, const char * S, bool reverseStrand );
    bool AlignTail( int qlen, int qdir, const char * q, const char * s, int& qe, int& se, TTranscript& t, int qpos ); 
    bool AlignTailDiag( int qlen, int qdir, double pmax, const char * qseq, const char * sseq, int& ge, int& se, TTranscript& t, int qpos );

    void AdjustMatrixSize();
    void AdjustWindowBoundary( const char * & q, const char * & Q, const char * & s, const char * & S, TTranscript& tw );
    const CCell& GetMatrix( int q, int s ) const;
    const CCell& SetMatrix( int q, int s, double v, CCell::EEvent e, int c = 1 );

    template <typename Iterator>
    void  AppendTranscript( Iterator b, Iterator e ) { while( b != e ) AppendTranscriptItem( *b++ ); }

    void  AppendTranscriptItem( const TTrItem& u ); // { ASSERT( u.GetEvent() != eEvent_NULL ); m_transcript.AppendItem( u ); }
    void  AppendTranscriptItem( EEvent ev, unsigned cnt = 1 ) { AppendTranscriptItem( TTrItem( ev, cnt ) ); }

    ostream& PrintQ( ostream& o, const char * q, const char * Q ) const {
        for( Q += m_qryInc ; q != Q ; q += m_qryInc ) o << CIupacnaBase( CNcbi8naBase( *q ) );
        return o;
    }
    ostream& PrintS( ostream& o, const char * s, const char * S ) const {
        for( S += m_sbjInc ; s != S ; s += m_sbjInc ) o << CIupacnaBase( CNcbi8naBase( *s ) );
        return o;
    }

    void x_ColorspaceUpdatePenalty();

protected:
    CScoringFactory * m_scoringFactory;
    const CScoreParam * m_scoreParam;
    const CAlignParam * m_alignParam;
    IScore * m_score;

    CSeqCoding::ECoding m_qryCoding;
    CSeqCoding::ECoding m_sbjCoding;
    CSeqCoding::EStrand m_qryStrand;
    CSeqCoding::EStrand m_sbjStrand;
    int m_anchorQ1;
    int m_anchorQ2;
    int m_anchorS1;
    int m_anchorS2;
    int m_qryInc;
    int m_sbjInc;
    const char * m_qry;
    const char * m_sbj;
    int m_qryLength;
    int m_sbjLength;
    double m_extentionPenaltyDropoff;
    double m_penaltyLimit;
    double m_penalty;

    int m_posQ1;
    int m_posQ2;
    int m_posS1;
    int m_posS2;

    vector<CCell> m_matrix;
    int m_matrixSize;
    int m_matrixOffset;

    Uint8 m_queryBases;
    Uint8 m_queryBasesChecked;
    mutable Uint8 m_scoreCalls;
    Uint8 m_totalCalls;
    Uint8 m_successAligns;
    Uint4 m_algoX0all;
    Uint4 m_algoX0end;
    Uint4 m_algoX0win;
    Uint4 m_algoSWend;
    Uint4 m_algoSWwin;

    int m_wordDistance;

    TTranscript m_transcript;
    TTranscript m_guideTranscript;
    static map<double,Uint8> CAligner::s_maxPenalty;
};

inline void CAligner::AppendTranscriptItem( const TTrItem& u ) 
{ 
    ASSERT( u.GetEvent() != eEvent_NULL ); 
    if( m_transcript.size() ) {
        if( m_transcript.back().GetEvent() == eEvent_SoftMask && u.GetEvent() == eEvent_Insertion ) {
            m_penalty += m_scoreParam->GetGapBasePenalty() + ( m_scoreParam->GetGapExtentionPenalty() + m_scoreParam->GetIdentityScore() - m_scoreParam->GetMismatchPenalty() ) * u.GetCount();
            m_transcript.AppendItem( TTrItem( eEvent_SoftMask, u.GetCount() ) );
            return;
        } else if( u.GetEvent() == eEvent_SoftMask && m_transcript.back().GetEvent() == eEvent_Insertion ) {
            m_penalty += m_scoreParam->GetGapBasePenalty() + ( m_scoreParam->GetGapExtentionPenalty() + m_scoreParam->GetIdentityScore() - m_scoreParam->GetMismatchPenalty() ) * m_transcript.back().GetCount();
            m_transcript.back() = TTrItem( eEvent_SoftMask, m_transcript.back().GetCount() );
        }
    }
    m_transcript.AppendItem( u ); 
}


inline void CAligner::SetPenaltyLimit( double l ) { m_penaltyLimit = l; }
inline void CAligner::SetExtentionPenaltyDropoff( double x ) { m_extentionPenaltyDropoff = abs( x ); }
inline void CAligner::SetQueryStrand( CSeqCoding::EStrand s ) { m_qryStrand = s; }
inline void CAligner::SetQueryCoding( CSeqCoding::ECoding c ) { m_qryCoding = c; }
inline void CAligner::SetSubjectStrand( CSeqCoding::EStrand s ) { m_sbjStrand = s; }
inline void CAligner::SetSubjectCoding( CSeqCoding::ECoding c ) { m_sbjCoding = c; }
inline void CAligner::SetQuery( const char * b, int l ) { m_qry = b; m_qryLength = l; } 
inline void CAligner::SetQueryAnchor( int f, int l ) { m_anchorQ1 = f; m_anchorQ2 = l; m_guideTranscript.clear(); }
inline void CAligner::SetSubject( const char * b, int l ) { m_sbj = b; m_sbjLength = l; } 
inline void CAligner::SetSubjectAnchor( int f, int l ) { m_anchorS1 = f; m_anchorS2 = l; m_guideTranscript.clear(); }
inline void CAligner::SetSubjectGuideTranscript( const TTranscript& tr ) 
{
    pair<int,int> lengths = tr.ComputeLengths();
    if( lengths.second == abs(m_anchorS2 - m_anchorS1 + 1) && lengths.first == abs(m_anchorS2 - m_anchorQ1 + 1) ) {
        // clip masked regions
        ITERATE( TTranscript, t, tr ) {
            switch( t->GetEvent() ) {
                case eEvent_SoftMask:
                case eEvent_HardMask: 
                case eEvent_NULL: break;
                default: m_guideTranscript.AppendItem( *t ); break;
            }
        }
    }
}

inline const CAligner::CCell& CAligner::GetMatrix( int q, int s ) const 
{ 
    return m_matrix[ (q - m_matrixOffset)*m_matrixSize + ++s ]; 
}

inline const CAligner::CCell& CAligner::SetMatrix( int q, int s, double v, CCell::EEvent e, int cnt ) 
{ 
//#define OLIGOFAR_ALIGN_CHECK_MATRIX_BOUNDARIES 0
#if !defined(OLIGOFAR_ALIGN_CHECK_MATRIX_BOUNDARIES)
#define OLIGOFAR_ALIGN_CHECK_MATRIX_BOUNDARIES 0
#endif
#if OLIGOFAR_ALIGN_CHECK_MATRIX_BOUNDARIES
    ASSERT( q >= -1 );
    ASSERT( s >= -1 );
    ASSERT( q < m_matrixSize - 1 ); 
    ASSERT( s < m_matrixSize - 1 ); 
#endif
    return m_matrix[ (q - m_matrixOffset)*m_matrixSize + ++s ] = CCell( v, e, cnt ); 
}

inline double CAligner::Score( const char * q, const char * s ) const 
{
    ++m_scoreCalls;
    return m_score->Score( q, s );
}

/* ========================================================================
 * There are two parameters which control how many values of matrix should 
 * be computed and how meny should be queried.
 *
 * Score of the single gap is smaller or equal to the score of multiple 
 * gaps. But if we have limit on xdropof, the limit may be still controlled 
 * by how many gaps of the longest width may be introduced.
 * ======================================================================== */

inline CAligner::CMaxGapLengths::CMaxGapLengths( double pm, const CScoreParam * sc, const CAlignParam * ap ) 
{
    double id = sc->GetIdentityScore();
    double go = sc->GetGapBasePenalty();
    double ge = sc->GetGapExtentionPenalty();
    int maxDel = int( (pm - go)/ge );
    int maxIns = int((pm + id - go)/(id + ge)); 
    m_maxDelLen = max( 0, min( ap->GetMaxDeletionLength(), maxDel ) );
    m_maxInsLen = max( 0, min( ap->GetMaxInsertionLength(), min( maxIns, maxDel ) ) ); // m_maxDelLen can't be longer then xd
}

inline CAligner::CIndelLimits::CIndelLimits( double pm, const CScoreParam * sc, const CAlignParam * ap ) : CMaxGapLengths( pm, sc, ap )
{
    double id = sc->GetIdentityScore();
    double go = sc->GetGapBasePenalty();
    double ge = sc->GetGapExtentionPenalty();

    int maxDelCnt = int( pm / (go + m_maxDelLen*ge) );
    int maxInsCnt = int( pm / (go + m_maxInsLen*ge + id) );

    int extraDelLen = max( 0, int( (pm - maxDelCnt*(go + m_maxDelLen*ge) - go)/ge ) );
    int extraInsLen = max( 0, int( (pm - maxInsCnt*(go + m_maxInsLen*ge) - go - id)/(ge + id) ) );

    m_delBand = min( ap->GetMaxDeletionsCount(), max( m_maxDelLen, maxDelCnt * m_maxDelLen + extraDelLen ) );
    m_insBand = min( ap->GetMaxInsertionsCount(), max( m_maxInsLen, maxInsCnt * m_maxInsLen + extraInsLen ) );
}

inline std::ostream& operator << ( std::ostream& o, const CAligner::CCell& cell ) 
{
    return o << "[" << cell.GetCount() << cell.GetCigar() << ";" << cell.GetScore() << "]";
}

END_OLIGOFAR_SCOPES

#endif
