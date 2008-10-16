#include <ncbi_pch.hpp>
#include "cbatch.hpp"
#include "ialigner.hpp"
#include "cprogressindicator.hpp"

USING_OLIGOFAR_SCOPES;

CBatch::~CBatch() { for( int i = 0; i < 2; ++i ) if( m_ownAligner[i] ) delete m_aligner[i]; }

void CBatch::Start() 
{
    SetPass(0);
}

IAligner * CBatch::GetAligner() const
{
    EAlignerType t = eAligner_noIndel;
    if( m_pass > 0 ) t = eAligner_regular;
    else if( m_hashParam[m_pass].GetHashIndels() ) t = eAligner_regular;
    return m_aligner[t];
}

void CBatch::SetPass( unsigned pass )
{
    ASSERT( pass < m_hashParam.size() );
    m_pass = pass;
    m_filter.SetAligner( GetAligner() );
    m_queryHash.SetWindowSize( m_hashParam[pass].GetWindowSize() );
    m_queryHash.SetStrideSize( m_hashParam[pass].GetStrideSize() );
    m_queryHash.SetWordSize( m_hashParam[pass].GetWordSize() );
    m_queryHash.SetIndexBits( m_hashParam[pass].GetHashBits() );
    m_queryHash.SetMaxMismatches( m_hashParam[pass].GetHashMismatches() );
    m_queryHash.SetAllowIndel( m_hashParam[pass].GetHashIndels() );
    m_queryHash.ComputeHasherWindowLength();
}

int CBatch::AddQuery( CQuery * query )
{
    int cnt = 0;
    m_inputChunk.push_back( query ); // do not hash if we have guided hit
    if( query->GetTopHit() == 0 ) {
    } else {
        ++m_guidedReads;
    }
    int hasheableReads = m_inputChunk.size() - m_guidedReads;
    if( hasheableReads && hasheableReads % m_readsPerRun == 0 || 
        int( m_inputChunk.size() ) >= 2*m_readsPerRun ) Purge();
    return cnt;
}

int CBatch::x_EstimateMismatchCount( const CQuery* query, bool matepair ) const
{
    const int kNoHitEquivalent = 1000;
    ASSERT( query != 0 );
    const CHit * hit = query->GetTopHit();
    if( hit == 0 ) return kNoHitEquivalent;
    if( query->HasComponent( matepair ) ) {
        if( hit->HasComponent( matepair ) ) {
            return int( query->GetBestScore( matepair ) * ( 1.0 - hit->GetScore( matepair ) / 100.0 ) / m_mismatchPenalty );
        } else return kNoHitEquivalent;
    } else return 0; // no read - no mismatches, n'est pas?
}

void CBatch::x_Rehash( unsigned pass ) 
{
    SetPass( pass );
    int m = pass ? m_hashParam[m_pass - 1].GetHashMismatches() : 0;
//     int toRehash = 0;
//     CProgressIndicator p0( "Counting reads with
//     ITERATE( TInputChunk, i, m_inputChunk ) {
//         ASSERT( *i );
//         if( x_EstimateMismatchCount( *i, 0 ) > m || x_EstimateMismatchCount( *i, 1 ) > m ) {
//             (*i)->ClearHits();
//             ++toRehash;
//         }
//     }
    
    ostringstream msg0;
    msg0 << "pass" << pass 
         << " n=" << m_queryHash.GetMaxMismatches() 
         << " e=" << ( m_queryHash.GetAllowIndel() ? 1 : 0 )
         << " w=" << m_queryHash.GetWindowSize() << "/" << m_queryHash.GetWordSize() << " (" << m_queryHash.GetIndexBits() << "b)"
         << " S=" << m_queryHash.GetStrideSize() 
         << " m > " << m;
    CProgressIndicator p( "Hashing " + msg0.str() );

    int entries = 0;
    m_hashedReads = 0;
    ITERATE( TInputChunk, i, m_inputChunk ) {
        ASSERT( *i );
        if( (*i)->GetTopHit() == 0 || x_EstimateMismatchCount( *i, 0 ) > m || x_EstimateMismatchCount( *i, 1 ) > m ) {
            (*i)->ClearHits();
            entries += m_queryHash.AddQuery( *i );
            m_hashedReads++;
            p.Increment();
        }
    }
    p.SetMessage( "Created " + NStr::IntToString( entries ) + " entries for " + msg0.str() );
    p.Summary();
}

void CBatch::Purge()
{
    if( CProgressIndicator * p = m_readProgressIndicator) p->Format( CProgressIndicator::e_final );

    ASSERT( m_hashParam.size() );
    for( m_pass = 0; m_pass < m_hashParam.size(); ++m_pass ) {
        x_Rehash( m_pass );
        if( m_hashedReads ) m_seqVecProcessor.Process( m_fastaFile );
        m_queryHash.Clear();
    }

    if( m_guidedReads && m_formatter.NeedSeqids() ) x_LoadSeqIds();

    CProgressIndicator p( "Purging " + NStr::Int8ToString( CHit::GetCount() ) + " hits for " + NStr::Int8ToString( CQuery::GetCount() ) + " entries" );
    ITERATE( TInputChunk, i, m_inputChunk ) {
        m_formatter( *i );
        p.Increment();
    }
    p.Summary();

    m_inputChunk.clear();
    m_hashedReads = 0;
    m_guidedReads = 0;
	m_ignoredReads = 0;
	m_hashEntries = 0;
}

void CBatch::x_LoadSeqIds()
{
	cerr << "Loading seq ids for guided hits..." << flush;
    m_queryHash.Clear();
    m_seqVecProcessor.Process( m_fastaFile );
	cerr << "\b\b\b: OK\n";
}
