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
	if( !CheckGeometry( target->m_from[0], target->m_to[0], target->m_from[1], target->m_to[1] ) ) {
		// to make sure logic works same way
        // TODO: actually, logic DOES NOT work same way - to be fixed
		THROW( logic_error, "Checking geometry: " << DISPLAY( target->m_from[0] ) << DISPLAY( target->m_to[0] ) << DISPLAY( target->m_from[1] ) << DISPLAY( target->m_to[1] ) << DISPLAY( pairmate ) );
	}
    return otherHit;
}

bool CFilter::CheckGeometry( int from1, int to1, int from2, int to2 ) const
{
	if( from1 < to1 ) {
		if( from2 < to2 ) {
			if( to1 <= from2 ) {
				if( m_geometryFlags & fFwd1_Fwd2 ) return InRange( from1, to2 );
			} else if( to2 <= from1 ) {
				if( m_geometryFlags & fFwd2_Fwd1 ) return InRange( from2, to1 );
			} else {
				if( m_geometryFlags & fForward ) return InRange( min( from1, from2 ), max( to1, to2 ) );
			}
		} else {
			if( to1 <= to2 ) {
				if( m_geometryFlags & fFwd1_Rev2 ) return InRange( from1, from2 );
			} else if( from2 <= from1 ) {
				if( m_geometryFlags & fRev2_Fwd1 ) return InRange( to2, to1 );
			} else {
				if( m_geometryFlags & fOppositeFwd1 ) return InRange( min( from1, to2 ), max( to1, from2 ) );
			}
		}
	} else {
		if( from2 < to2 ) {
			if( from1 <= from2 ) {
				if( m_geometryFlags & fRev1_Fwd2 ) return InRange( to1, to2 );
			} else if( to2 <= to1 ) {
				if( m_geometryFlags & fFwd2_Rev1 ) return InRange( from2, from1 );
			} else {
				if( m_geometryFlags & fOppositeRev1 ) return InRange( min( to1, from2 ), max( from1, to2 ) );
			}
		} else {
			if( from1 <= to2 ) {
				if( m_geometryFlags & fRev1_Rev2 ) return InRange( to1, from2 );
			} else if( from2 <= to1 ) {
				if( m_geometryFlags & fRev2_Rev1 ) return InRange( to2, from1 );
			} else {
				if( m_geometryFlags & fReverse ) return InRange( min( to1, to2 ), max( from1, from2 ) );
			}
		}
	}
	return false;
}

void CFilter::Match( const CHashAtom& m, const char * a, const char * A, int pos ) // always start at pos
{
	ASSERT( m_aligner );
	m_aligner->SetBestPossibleQueryScore( m.GetQuery()->GetBestScore( m.GetPairmate() ) );
    if( m.GetStrand() == '-' && m.GetQuery()->GetCoding() == CSeqCoding::eCoding_colorsp ) --pos;
    m_aligner->Align( m.GetQuery()->GetCoding(),
                      m.GetQuery()->GetData( m.GetPairmate() ),
                      m.GetQuery()->GetLength( m.GetPairmate() ),
                      CSeqCoding::eCoding_ncbi8na,
                      a + pos,
                      m.GetStrand() == '+' ? A - a - pos : -pos,
                      CAlignerBase::fComputeScore );
    const CAlignerBase& abase = m_aligner->GetAlignerBase();
    double score = abase.GetScore();
    if( score >= m_scoreCutoff ) {
        switch( m.GetStrand() ) {
        case '+': Match( score, pos, pos + abase.GetSubjectAlignedLength() - 1, m.GetQuery(), m.GetPairmate() ); break;
        case '-': Match( score, pos, pos - abase.GetSubjectAlignedLength() + 1, m.GetQuery(), m.GetPairmate() ); break;
        }
	}
}

CFilter::TPendingHits::iterator CFilter::x_ExpireOldHits( TPendingHits& pendingHits, int p ) 
{
    TPendingHits::iterator h = pendingHits.begin();
    if( pendingHits.size() ) {
        while( h != pendingHits.end() && h->first < p ) PurgeHit( h++->second );
        pendingHits.erase( pendingHits.begin(), h );
    }
	return h;
}

bool CFilter::x_LookupInQueue( TPendingHits::iterator h, TPendingHits& pendingHits, bool fwd, int pairmate, int maxPos, double score, int seqFrom, int seqTo, CQuery * query )
{
   	TPendingHits toAdd;
	bool found = false;
   	for( ; h != pendingHits.end() && h->first <= maxPos ; ++h ) {
		CHit * hh = h->second;

	    if( query != hh->GetQuery() ) continue;
    	if( !hh->HasPairTo( pairmate ) ) continue;
		// TODO: one more check here on strand
		if( ( ( fwd == true  ) && ( m_geometryFlags & fLookupForFwd ) ) ||
			( ( fwd == false ) && ( m_geometryFlags & fLookupForRev ) ) ) {

            if( CHit * hit = SetHit( h->second, pairmate, score, seqFrom, seqTo ) ) {
                if( hit->GetComponents() == 3 ) // this may create combinatorial explosion
                    toAdd.insert( make_pair( h->first, hit ) ); 
                else {
                    PurgeHit( hit ); // this is in case when combinatorial explosion is forbidden
                }
            }
            found = true;
		}
	}
    copy( toAdd.begin(), toAdd.end(), inserter( pendingHits, pendingHits.end() ) );
	return found;
}

void CFilter::Match( double score, int seqFrom, int seqTo, CQuery * query, int pairmate )
{
    int p = seqFrom - m_maxDist;
    
    ASSERT( query );
    ASSERT( m_ord >= 0 );

    // expire old hits
	TPendingHits::iterator h0 = x_ExpireOldHits( m_pendingHits[0], p );
	TPendingHits::iterator h1 = x_ExpireOldHits( m_pendingHits[1], p );
    
    if( query->IsPairedRead() == false ) {
        PurgeHit( new CHit( query, m_ord, pairmate, score, seqFrom, seqTo ) );
        return;
    } 

	bool fwd = seqFrom < seqTo;
	bool found = false;
	///bool added = false;

	int lookupFlags = pairmate == 0 ? fwd ? fRev2_Fwd1|fFwd2_Fwd1 : fFwd2_Rev1|fRev2_Rev1 : fwd ? fRev1_Fwd2|fFwd1_Fwd2 : fFwd1_Rev2|fRev1_Rev2;
	int appendFlags = pairmate == 0 ? fwd ? fFwd1_Rev2|fFwd1_Fwd2 : fRev1_Fwd2|fRev1_Rev2 : fwd ? fFwd2_Rev1|fFwd2_Fwd1 : fRev2_Fwd1|fRev2_Rev1;
	
	if( int flags = m_geometryFlags & lookupFlags ) {
        int maxPos = seqFrom - m_minDist;

		if( flags & fLookupInFwd ) {
			found |= x_LookupInQueue( h0, m_pendingHits[0], fwd, pairmate, maxPos, score, seqFrom, seqTo, query );
		}
		if( flags & fLookupInRev ) {
			found |= x_LookupInQueue( h1, m_pendingHits[1], fwd, pairmate, maxPos, score, seqFrom, seqTo, query );
		}
	} 

	// NB: logic here is a bit limited... it allows transitive hits, e.g. Fwd1...Fwd2...Fwd1 even with disallowed combinatorial explosion; 
	// otherwise we may have a problem that Fwd2...Fwd1 part will be missed even if it is stronger then Fwd1...Fwd2 part of the triplet.
	if( m_geometryFlags & appendFlags ) {
       	CHit * hh = new CHit( query, m_ord, pairmate, score, seqFrom, seqTo );
		if( fwd ) m_pendingHits[0].insert( make_pair( seqFrom, hh ) );
		else      m_pendingHits[1].insert( make_pair( seqTo,   hh ) );
	} else if( !found ) {
        PurgeHit( new CHit( query, m_ord, pairmate, score, seqFrom, seqTo ) );
    }

	/*
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
	*/
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

void CFilter::PurgeQueues()
{
	for( unsigned int q = 0; q < sizeof( m_pendingHits )/sizeof( m_pendingHits[0] ); ++q ) {
	    for( TPendingHits::const_iterator i = m_pendingHits[q].begin(); i != m_pendingHits[q].end(); ++i ) {
    	    PurgeHit( i->second, true );
		}
    }
}

void CFilter::SequenceEnd()
{
	for( unsigned int q = 0; q < sizeof( m_pendingHits )/sizeof( m_pendingHits[0] ); ++q ) {
    	m_pendingHits[q].clear();
	}
    m_begin = m_end = 0;
}

void CFilter::PurgeHit( CHit * hit, bool setTarget )
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
	bool fmt = m_outputFormatter && m_outputFormatter->ShouldFormatAllHits();
	if( fmt || setTarget ) {
		hit->SetTarget( 0, m_begin, m_end );
		hit->SetTarget( 1, m_begin, m_end );
	}
	if( fmt ) {
		m_outputFormatter->FormatHit( hit );
	}
}


