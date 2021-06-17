#ifndef OLIGOFAR_CBATCH__HPP
#define OLIGOFAR_CBATCH__HPP

#include "cfilter.hpp"
#include "cqueryhash.hpp"
#include "cpassparam.hpp"
#include "cscoringfactory.hpp"
#include "cseqvecprocessor.hpp"

BEGIN_OLIGOFAR_SCOPES

class IAligner;
class IOutputFormatter;
class CProgressIndicator;
class CSeqScanner;

class CBatch
{
public:
    typedef vector<CQuery*> TInputChunk;
    typedef vector<CPassParam> TPassParamSet;

    void SetReadCount( int rc ) { m_readsPerRun = rc; if( m_queryHash ) m_queryHash->SetExpectedReadCount( rc ); m_inputChunk.reserve( rc ); }
    void SetFastaFile( const string& fname ) { m_fastaFile = fname; }
    void SetFilter( CFilter * f ) { m_filter = f; }
    void SetQueryHash( CQueryHash * qh ) { m_queryHash = qh; if( qh ) qh->SetExpectedReadCount( m_readsPerRun ); }
    void SetSeqScanner( CSeqScanner * ss ) { m_seqScanner = ss; }
    void SetSeqVecProcessor( CSeqVecProcessor * sp ) { m_seqVecProcessor = sp; }
    void SetScoringFactory( CScoringFactory * sf ) { m_scoringFactory = sf; }
    void SetOutputFormatter( IOutputFormatter * sf ) { m_formatter = sf; }
    void SetReadProgressIndicator( CProgressIndicator * p ) { m_readProgressIndicator = p; }
    void SetRange( int minBatch, int maxBatch ) { m_minBatch = minBatch; m_maxBatch = maxBatch; }
    
    ~CBatch();
    CBatch() : 
        m_passParam( 0 ), 
        m_readsPerRun( 0 ), 
        m_minBatch( 0 ),
        m_maxBatch( numeric_limits<int>::max() ),
        m_batchOrdinal( 0 ),
        m_hashedReads( 0 ), 
        m_guidedReads( 0 ), 
        m_ignoredReads( 0 ),
        m_hashEntries( 0 ), 
        m_filter( 0 ), 
        m_queryHash( 0 ), 
        m_seqVecProcessor( 0 ), 
        m_seqScanner( 0 ),
        m_scoringFactory( 0 ),
        m_formatter( 0 ),
        m_readProgressIndicator( 0 ), 
        m_pass( -1 ) {}

    void Start();
    int  AddQuery( CQuery * query );
    void Purge();
    bool Done() const { return m_batchOrdinal > m_maxBatch; }
    
    bool SinglePass() const { return m_passParam && m_passParam->size() < 2; }

    const TInputChunk& GetInputChunk() const { return m_inputChunk; }

    void SetPassParam( const TPassParamSet* pps ) { m_passParam = pps; }
    const CPassParam& GetPassParam( int pass ) const { return (*m_passParam)[pass]; }

protected:
    int x_EstimateMismatchCount( const CQuery*, bool matepair ) const;
    void x_LoadSeqIds();
    void x_Rehash( unsigned pass );
    void SetPass( unsigned );
protected:
    const TPassParamSet * m_passParam;
    int m_readsPerRun;
    int m_minBatch;
    int m_maxBatch;
    int m_batchOrdinal;
    int m_hashedReads;
    int m_guidedReads;
    int m_ignoredReads;
    Uint8 m_hashEntries;
    string m_fastaFile;
    TInputChunk m_inputChunk;
    CFilter            * m_filter;
    CQueryHash         * m_queryHash;
    CSeqVecProcessor   * m_seqVecProcessor;
    CSeqScanner        * m_seqScanner;
    CScoringFactory    * m_scoringFactory;
    IOutputFormatter   * m_formatter;
    CProgressIndicator * m_readProgressIndicator;
    int m_pass;
};

END_OLIGOFAR_SCOPES

#endif
