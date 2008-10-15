#ifndef OLIGOFAR_CBATCH__HPP
#define OLIGOFAR_CBATCH__HPP

#include "cfilter.hpp"
#include "cscoretbl.hpp"
#include "cqueryhash.hpp"
#include "chashparam.hpp"
#include "cseqvecprocessor.hpp"
#include "coutputformatter.hpp"

BEGIN_OLIGOFAR_SCOPES

class CProgressIndicator;
class CBatch
{
public:
    typedef vector<CQuery*> TInputChunk;
    typedef vector<CHashParam> THashParamSet;

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

    template<class iterator>
    void SetHashParam( iterator a, iterator b ) { m_hashParam.clear(); copy( a, b, back_inserter( m_hashParam ) ); }
    template<class container>
    void SetHashParam( const container& c ) { SetHashParam( c.begin(), c.end() ); }

    enum EAlignerType {
        eAligner_noIndel = 0,
        eAligner_regular = 1
    };

    void SetAligner( EAlignerType atype, IAligner * aligner, bool own ) { m_aligner[atype] = aligner; m_ownAligner[atype] = own; ASSERT( m_aligner[atype] ); }
    IAligner * GetAligner( EAlignerType t ) const { return m_aligner[t]; }
    IAligner * GetAligner() const;

    bool SinglePass() const { return m_hashParam.size() < 2; }

protected:
    int x_EstimateMismatchCount( const CQuery*, bool matepair ) const;
    void x_LoadSeqIds();
    void x_Rehash( unsigned pass );
    void SetPass( unsigned );
protected:
    THashParamSet m_hashParam;
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
    IAligner * m_aligner[2];
    bool m_ownAligner[2];
    unsigned m_pass;
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
    m_queryHash.SetExpectedReadCount( readCount );
    m_aligner[0] = m_aligner[1] = 0;
    m_ownAligner[0] = m_ownAligner[1] = 0;
}

END_OLIGOFAR_SCOPES

#endif
