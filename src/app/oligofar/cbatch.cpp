#include <ncbi_pch.hpp>
#include "cbatch.hpp"
#include "ialigner.hpp"
#include "cprogressindicator.hpp"

USING_OLIGOFAR_SCOPES;

CBatch::~CBatch() { for( int i = 0; i < 2; ++i ) if( m_ownAligner[i] ) delete m_aligner[i]; }

void CBatch::Start() 
{
    int mode = SinglePass() ? 1 : 0;
    m_filter.SetAligner( m_aligner[mode] );
    m_queryHash.SetHashType( m_hashType[mode], m_windowLength[mode] );
    m_queryHash.SetMaxMism( m_queryHash.GetMinimalMaxMism() );
}

int CBatch::AddQuery( CQuery * query )
{
    int cnt = 0;
    m_inputChunk.push_back( query ); // do not hash if we have guided hit
    if( query->GetTopHit() == 0 ) {
        /*
        m_hashEntries += (cnt = m_queryHash.AddQuery( query ));
        if( cnt ) {
            ++m_hashedReads;
            m_inputChunk.push_back( query );
        } else {
			++m_ignoredReads;
			delete query;
		}
        */
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
			//	int( (100.0 - hit->GetScore( matepair )) / (100.0 * m_mismatchPenalty) );
        } else return kNoHitEquivalent;
    } else return 0; // no read - no mismatches, n'est pas?
}

void CBatch::x_Rehash( int m, int pass ) 
{
    int toRehash = 0;
    if( pass > 0 ) {
        ITERATE( TInputChunk, i, m_inputChunk ) {
            ASSERT( *i );
            if( x_EstimateMismatchCount( *i, 0 ) > m || x_EstimateMismatchCount( *i, 1 ) > m ) {
                (*i)->ClearHits();
                ++toRehash;
            }
        }
    } else {
        toRehash = m_inputChunk.size() - m_guidedReads;
    }
    
    int mode = SinglePass() || pass > 0 ? 1 : 0;
    m_filter.SetAligner( m_aligner[mode] );
    m_queryHash.SetHashType( m_hashType[ (toRehash < m_readsPerRun * m_fraction) ? mode : 0 ], m_windowLength[mode] );

    ostringstream msg0;
    msg0 << "pass" << pass << " " << m_queryHash.GetHashTypeName() 
        << " n=" << m_queryHash.GetMaxMism() 
        << " w=" << m_queryHash.GetWindowLength() << "/" << m_queryHash.GetWordLength() 
        << " m >= " << m;
    CProgressIndicator p( "Hashing " + msg0.str() );

    int entries = 0;
    m_hashedReads = 0;
    ITERATE( TInputChunk, i, m_inputChunk ) {
        ASSERT( *i );
        if( (*i)->GetTopHit() == 0 ) {
            entries += m_queryHash.AddQuery( *i );
            m_hashedReads++;
            p.Increment();
        }
    }
    /*
    ostringstream msg;
    msg << "pass" << pass << " " << m_queryHash.GetHashTypeName() << " hash for reads with best matches having " << m 
        << " mismatches or more using max-mism=" << m_queryHash.GetMaxMism() 
        << " and window-size=" << m_queryHash.GetWindowLength();
        */
    p.SetMessage( "Created " + NStr::IntToString( entries ) + " entries for " + msg0.str() );
    p.Summary();
}

void CBatch::Purge()
{
    if( CProgressIndicator * p = m_readProgressIndicator) p->Format( CProgressIndicator::e_final );

    x_Rehash( m_queryHash.GetMinimalMaxMism(), 0 );
    for( int pass = 0; m_hashedReads; ++pass ) {

        m_queryHash.Freeze();
        m_seqVecProcessor.Process( m_fastaFile );
        m_queryHash.Clear();

        if( SinglePass() ) break;

        bool rehash = false;
        int mismCutoff = m_queryHash.GetMaxMism() + 1;
        
        if( m_queryHash.GetWindowLength() != m_windowLength[1] ) {
            rehash = true;
        }
        if( m_queryHash.GetMaxMism() < m_queryHash.GetAbsoluteMaxMism() ) {
            rehash = true;
            m_queryHash.SetMaxMism( m_queryHash.GetMaxMism() + 1 );
        }
        if( rehash ) x_Rehash( mismCutoff, pass + 1 );
        else break;
    }

    if( m_guidedReads && m_formatter.NeedSeqids() ) x_LoadSeqIds();

    cerr << "Purging " << CHit::GetCount() << " hits for " << CQuery::GetCount() << " entries: ";
    for_each( m_inputChunk.begin(), m_inputChunk.end(), m_formatter );
    m_inputChunk.clear();

    m_filter.SetAligner( m_aligner[0] );
    m_queryHash.SetHashType( m_hashType[0], m_windowLength[0] );
    m_queryHash.SetMaxMism( m_queryHash.GetMinimalMaxMism() );
    
    cerr << "OK, " << CHit::GetCount() << "/" << CQuery::GetCount() << " left\n";

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
