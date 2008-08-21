#include <ncbi_pch.hpp>
#include "cfilter.hpp"
#include "ialigner.hpp"
#include "cquery.hpp"
#include "coutputformatter.hpp"

USING_OLIGOFAR_SCOPES;

CHit * CFilter::SetHit( CHit * target, int pairmate, double score, int from, int to, bool allowCombinations ) {
    ASSERT( ! target->IsNull() );
    ASSERT( target->m_seqOrd != ~0U );
    ASSERT( target->HasPairTo( pairmate ) );
    CHit * otherHit = 0;
    if( target->HasComponent( pairmate ) ) {
        if( allowCombinations ) {
            otherHit = new CHit( target->m_query, target->m_seqOrd, pairmate, score, from, to );
            ASSERT( otherHit == 0 || otherHit->IsNull() == false );
            if( target->HasPairTo( pairmate ) ) {
                ASSERT( otherHit );
                SetHit( otherHit, !pairmate, target->GetScore( !pairmate ), target->GetFrom( !pairmate ), target->GetTo( !pairmate ) );
            }
            return otherHit;
        } else { // allowCombinations
            if( target->GetScore( pairmate ) >= score ) {
                return new CHit( target->m_query, target->m_seqOrd, pairmate, score, from, to );
            } else {
                otherHit = new CHit( target->m_query, target->m_seqOrd, pairmate, 
                                     target->GetScore( pairmate ), target->GetFrom( pairmate ), target->GetTo( pairmate ) );
            }
        }
    }
    target->m_components |= target->GetComponentMask( pairmate );
    target->m_score[pairmate] = float( score );
    target->m_from[pairmate] = from;
    target->m_to[pairmate] = to;
    return otherHit;
}

void CFilter::Match( const CQueryHash::SHashAtom& m, const char * a, const char * A, int pos ) // always start at pos
{
	ASSERT( m_aligner );
    m_aligner->Align( m.query->GetCoding(),
                      m.query->GetData( m.pairmate ),
                      m.query->GetLength( m.pairmate ),
                      CSeqCoding::eCoding_ncbi8na,
                      a + pos,
                      m.strand == '+' ? A - a - pos : -pos,
                      CAlignerBase::fComputeScore );
    const CAlignerBase& abase = m_aligner->GetAlignerBase();
    double score = abase.GetScore();
    if( score >= m_scoreCutoff ) {
        switch( m.strand ) {
        case '+': Match( score, pos, pos + abase.GetSubjectAlignedLength() - 1, m.query, m.pairmate ); break;
        case '-': Match( score, pos, pos - abase.GetSubjectAlignedLength() + 1, m.query, m.pairmate ); break;
        }
	}
}

void CFilter::Match( double score, int seqFrom, int seqTo, CQuery * query, int pairmate )
{
    int p = seqFrom - m_maxDist;
    
    ASSERT( query );
    ASSERT( m_ord >= 0 );

    // expire old hits
    TPendingHits::iterator h = m_pendingHits.begin();
    if( m_pendingHits.size() ) {
        while( h != m_pendingHits.end() && h->first < p ) PurgeHit( h++->second );
        m_pendingHits.erase( m_pendingHits.begin(), h );
    }
    

    if( query->IsPairedRead() == false ) {
        PurgeHit( new CHit( query, m_ord, pairmate, score, seqFrom, seqTo ) );
        return;
    } 

    if( seqFrom > seqTo ) { 
        int P = seqFrom - m_minDist;
        
        // find pair in queue
        bool found = false;
        TPendingHits toAdd;
        for( ; h != m_pendingHits.end() && h->first <= P ; ++h ) {
            CHit * hh = h->second;

            if( query != hh->GetQuery() ) continue;
            if( !hh->HasPairTo( pairmate ) ) continue;
            if( hh->IsRevCompl( !pairmate ) ) continue;

            if( CHit * hit = SetHit( h->second, pairmate, score, seqFrom, seqTo ) ) {
                if( hit->GetComponents() == 3 ) // this may create combinatorial explosion
                    toAdd.insert( make_pair( h->first, hit ) ); 
                else {
                    ASSERT( hit->HasComponent( pairmate ) );
                    ASSERT( hit->HasComponent( !pairmate ) == false );
                    ASSERT( hit->GetFrom( pairmate ) > hit->GetTo( pairmate ) );
                    PurgeHit( hit ); // this is in case when combinatorial explosio is forbidden
                }
            }
            found = true;
       }
        copy( toAdd.begin(), toAdd.end(), inserter( m_pendingHits, m_pendingHits.end() ) );
        if( !found ) {
            PurgeHit( new CHit( query, m_ord, pairmate, score, seqFrom, seqTo ) );
        }
    } else {
        // positive strand of paired read - add the hit to queue
        CHit * hh = new CHit( query, m_ord, pairmate, score, seqFrom, seqTo );
        ASSERT( hh->IsRevCompl( pairmate ) == false );
        ASSERT( hh->GetComponents() == CHit::GetComponentMask( pairmate ) );
        m_pendingHits.insert( make_pair( seqFrom, hh ) );
    } 
}

void CFilter::SequenceBegin( const TSeqIds& id, int oid ) 
{
	ASSERT( oid >= 0 );
	ASSERT( m_seqIds );
    m_ord = m_seqIds->Register( id, oid );
}

void CFilter::SequenceBuffer( CSeqBuffer* buffer ) 
{
    m_begin = buffer->GetBeginPtr();
    m_end = buffer->GetEndPtr();
}

void CFilter::SequenceEnd()
{
    for( TPendingHits::const_iterator i = m_pendingHits.begin(); i != m_pendingHits.end(); ++i ) {
        PurgeHit( i->second );
    }
    m_pendingHits.clear();
    m_begin = m_end = 0;
}

void CFilter::PurgeHit( CHit * hit )
{
    if( hit == 0 ) return;
    CQuery * q = hit->GetQuery();
    ASSERT( q );
    ASSERT( hit->m_next == 0 );
    ASSERT( hit->GetComponents() );
    if( CHit * tih = q->GetTopHit() ) {
        double hscore = hit->GetTotalScore();
        double tscore = tih->GetTotalScore();
        double cscore = tscore * m_topPct/100;
        if( cscore > hscore ) { delete hit; return; }
        int topcnt = m_topCnt;
        if( tscore <= hscore ) {
            hit->m_next = tih;
            q->m_topHit = hit;
			cscore = ( tscore = hscore ) * m_topPct/100;
        } else {
			// trying to insert hit somewhere in the list
            bool weak = true;
            for( ; weak && tih->m_next && topcnt > 0; (tih = tih->m_next), --topcnt ) {
                if( tih->m_next->IsNull() || tih->m_next->GetTotalScore() < hscore ) {
                    hit->m_next = tih->m_next;
                    tih->m_next = hit;
                    weak = false;
                }
            }
			// oops - the hit is too weak, ignore it 
            if( weak ) {
                delete hit;
                return;
            }
        }
        for( tih = hit; topcnt > 0 && tih->m_next && tih->m_next->GetTotalScore() >= cscore; tih = tih->m_next, --topcnt );
        if( tih->m_next && !tih->m_next->IsNull() ) {
            delete tih->m_next;
            tih->m_next = ( topcnt ? new CHit( q ) : 0 ); // if we clipped by score, we need to restore terminator
        }
    } else {
        q->m_topHit = hit;
        hit->m_next = new CHit( q );
    }
	if( m_outputFormatter && m_outputFormatter->ShouldFormatAllHits() ) {
		hit->SetTarget( 0, m_begin, m_end );
		hit->SetTarget( 1, m_begin, m_end );
		m_outputFormatter->FormatHit( hit );
	}
}


