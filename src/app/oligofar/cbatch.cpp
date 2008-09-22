#include <ncbi_pch.hpp>
#include "cbatch.hpp"
#include "cprogressindicator.hpp"

USING_OLIGOFAR_SCOPES;

int CBatch::AddQuery( CQuery * query )
{
    int cnt = 0;
    if( query->GetTopHit() == 0 ) {
        m_hashEntries += (cnt = m_queryHash.AddQuery( query ));
        if( cnt ) {
            ++m_hashedReads;
            m_inputChunk.push_back( query );
        } else {
			++m_ignoredReads;
			delete query;
		}
    } else {
        m_inputChunk.push_back( query ); // do not hash if we have guided hit
        ++m_guidedReads;
    }
    if( m_hashedReads && m_hashedReads % m_readsPerRun == 0 || 
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

void CBatch::x_Rehash( int m ) 
{
    CProgressIndicator p("Rehashing reads with non-perfect matches using max-mism=" + NStr::IntToString(m_queryHash.GetMaxMism()) + 
                         " and winlen=" + NStr::IntToString( m_queryHash.GetCurrentWindowLength() ) );
    int entries = 0;
    ITERATE( TInputChunk, i, m_inputChunk ) {
        ASSERT( *i );
        if( x_EstimateMismatchCount( *i, 0 ) > m || x_EstimateMismatchCount( *i, 1 ) > m ) {
            (*i)->ClearHits();
            entries += m_queryHash.AddQuery( *i );
            p.Increment();
        } else {
            m_hashedReads--;
        }
    }
    p.SetMessage( "Rehashed " + NStr::IntToString( entries ) + " entries for non-perfectly matched reads using max-mism=" + NStr::IntToString(m_queryHash.GetMaxMism()) + 
                  " and winlen=" + NStr::IntToString( m_queryHash.GetCurrentWindowLength() ) );
    p.Summary();
}

void CBatch::Purge()
{
	cerr << "Queries: guided " << m_guidedReads << ", hashed " << m_hashedReads << ", ignored: " << m_ignoredReads << ", entries: " << m_hashEntries << "\n";
    if( CProgressIndicator * p = m_readProgressIndicator) p->Format( CProgressIndicator::e_final );
    while( m_hashedReads ) {
        m_queryHash.Freeze();
        m_seqVecProcessor.Process( m_fastaFile );
        m_queryHash.Clear();

        bool rehash = false;
        if( m_allowShortWindow && m_queryHash.GetOffset() && ( m_queryHash.GetOneHash() == false )) {
            rehash = true;
            m_queryHash.SetOneHash( true );
        }
        if( m_queryHash.GetMaxMism() < m_queryHash.GetAbsoluteMaxMism() ) {
            rehash = true;
            m_queryHash.SetMaxMism( m_queryHash.GetMaxMism() + 1 );
        }
        if( rehash ) x_Rehash( m_queryHash.GetMaxMism() );
        else break;
    }

    if( m_guidedReads && m_formatter.NeedSeqids() ) x_LoadSeqIds();

    cerr << "Purging " << CHit::GetCount() << " hits for " << CQuery::GetCount() << " entries: ";
    for_each( m_inputChunk.begin(), m_inputChunk.end(), m_formatter );
    m_inputChunk.clear();
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
