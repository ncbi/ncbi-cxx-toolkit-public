#ifndef OLIGOFAR_CBATCH__HPP
#define OLIGOFAR_CBATCH__HPP

#include "cfilter.hpp"
#include "cscoretbl.hpp"
#include "cqueryhash.hpp"
#include "cseqvecprocessor.hpp"
#include "coutputformatter.hpp"

BEGIN_OLIGOFAR_SCOPES

class CProgressIndicator;
class CBatch
{
public:
    typedef vector<CQuery*> TInputChunk;

    // scoring is used to estimate number of mismathes for more iterative hashing
    CBatch( int readCount, const string& fastaFile, 
            CQueryHash& queryHash, CSeqVecProcessor& seqVecProcessor, 
            CFilter& filter, COutputFormatter& formatter,
            const CScoreTbl& scoring ) ;
    ~CBatch();

    void SetReadProgressIndicator( CProgressIndicator * p ) { m_readProgressIndicator = p; }

    void Start();
    int  AddQuery( CQuery * query );
    void Purge();
    
    const TInputChunk& GetInputChunk() const { return m_inputChunk; }

    void SetInitialWindowLength( int l ) { m_windowLength[1] = min( m_windowLength[1], m_windowLength[0] = l ); }
    void SetRegularWindowLength( int l ) { m_windowLength[0] = max( m_windowLength[0], m_windowLength[1] = l ); }
    void SetInitialHashType( CQueryHash::EHashType t ) { m_hashType[0] = t; }
    void SetRegularHashType( CQueryHash::EHashType t ) { m_hashType[1] = t; }
    void SetHashTypeChangeFraction( double fraction ) { m_fraction = fraction; }
    void SetInitialAligner( IAligner * aligner, bool own ) { m_aligner[0] = aligner; m_ownAligner[0] = own; ASSERT( m_aligner[0] ); }
    void SetRegularAligner( IAligner * aligner, bool own ) { m_aligner[1] = aligner; m_ownAligner[1] = own; ASSERT( m_aligner[1] ); }

    IAligner * GetRegularAligner() const { return m_aligner[1]; }
    IAligner * GetInitialAligner() const { return m_aligner[0]; }

    bool SinglePass() const { 
        return m_windowLength[0] == m_windowLength[1] && 
            m_queryHash.GetMinimalMaxMism() == m_queryHash.GetAbsoluteMaxMism();
    }

protected:
    int x_EstimateMismatchCount( const CQuery*, bool matepair ) const;
    void x_LoadSeqIds();
    void x_Rehash( int mm, int pass );
protected:
    int m_readsPerRun;
    int m_hashedReads;
    int m_guidedReads;
	int m_ignoredReads;
	Uint8 m_hashEntries;
	const CScoreTbl& m_scoreTbl;
    double m_mismatchPenalty;
    string m_fastaFile;
    TInputChunk m_inputChunk;
    CFilter          & m_filter;
    CQueryHash       & m_queryHash;
    CSeqVecProcessor & m_seqVecProcessor;
    COutputFormatter & m_formatter;
    CProgressIndicator * m_readProgressIndicator;
    int m_windowLength[2];
    CQueryHash::EHashType m_hashType[2];
    IAligner * m_aligner[2];
    bool m_ownAligner[2];
    double m_fraction;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// implementation

inline CBatch::CBatch( int readCount, const string& fastaFile, 
        CQueryHash& queryHash, CSeqVecProcessor& seqVecProcessor, CFilter& filter, 
        COutputFormatter& formatter, const CScoreTbl& scoreTbl ) :
    m_readsPerRun( readCount ), m_hashedReads( 0 ), m_guidedReads( 0 ), m_ignoredReads( 0 ), m_hashEntries( 0 ),
	m_scoreTbl( scoreTbl ),
    m_mismatchPenalty( scoreTbl.GetIdentityScore() - max( scoreTbl.GetMismatchScore(), scoreTbl.GetGapOpeningScore() ) ),
    m_fastaFile( fastaFile ), m_filter( filter ), m_queryHash( queryHash ), 
    m_seqVecProcessor( seqVecProcessor ), m_formatter( formatter ), 
    m_readProgressIndicator( 0 )
{
    ASSERT( m_mismatchPenalty > 0 );
    m_inputChunk.reserve( readCount );
    m_queryHash.Reserve( readCount );
}

END_OLIGOFAR_SCOPES

#endif
