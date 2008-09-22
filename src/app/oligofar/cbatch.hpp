#ifndef OLIGOFAR_CBATCH__HPP
#define OLIGOFAR_CBATCH__HPP

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
    CBatch( int readCount, const string& fastaFile, CQueryHash& queryHash,
            CSeqVecProcessor& seqVecProcessor, COutputFormatter& formatter,
            const CScoreTbl& scoring ) ;

    void SetReadProgressIndicator( CProgressIndicator * p ) { m_readProgressIndicator = p; }
    int AddQuery( CQuery * query );
    void Purge();
    
    bool GetAllowShortWindow() const { return m_allowShortWindow; }
    void SetAllowShortWindow( bool a ) { m_allowShortWindow = a; }

    const TInputChunk& GetInputChunk() const { return m_inputChunk; }
    
protected:
    int x_EstimateMismatchCount( const CQuery*, bool matepair ) const;
    void x_LoadSeqIds();
    void x_Rehash( int mm );
protected:
    int m_readsPerRun;
    int m_hashedReads;
    int m_guidedReads;
	int m_ignoredReads;
	Uint8 m_hashEntries;
	const CScoreTbl& m_scoreTbl;
    double m_mismatchPenalty;
    bool m_allowShortWindow;
    string m_fastaFile;
    TInputChunk m_inputChunk;
    CQueryHash       & m_queryHash;
    CSeqVecProcessor & m_seqVecProcessor;
    COutputFormatter & m_formatter;
    CProgressIndicator * m_readProgressIndicator;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// implementation

inline CBatch::CBatch( int readCount, const string& fastaFile, CQueryHash& queryHash,
                                    CSeqVecProcessor& seqVecProcessor, COutputFormatter& formatter,
                                    const CScoreTbl& scoreTbl ) :
    m_readsPerRun( readCount ), m_hashedReads( 0 ), m_guidedReads( 0 ), m_ignoredReads( 0 ), m_hashEntries( 0 ),
	m_scoreTbl( scoreTbl ),
    m_mismatchPenalty( scoreTbl.GetIdentityScore() - max( scoreTbl.GetMismatchScore(), scoreTbl.GetGapOpeningScore() ) ),
    m_allowShortWindow( false ),
    m_fastaFile( fastaFile ), m_queryHash( queryHash ), m_seqVecProcessor( seqVecProcessor ),
    m_formatter( formatter ), m_readProgressIndicator( 0 )
{
    ASSERT( m_mismatchPenalty > 0 );
    m_inputChunk.reserve( readCount );
    m_queryHash.Reserve( readCount );
}

END_OLIGOFAR_SCOPES

#endif
