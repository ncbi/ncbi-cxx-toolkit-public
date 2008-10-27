#include <ncbi_pch.hpp>
#include "cfilter.hpp"
#include "ialigner.hpp"
#include "cquery.hpp"
#include "coutputformatter.hpp"

USING_OLIGOFAR_SCOPES;

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
        if( m.IsReverseStrand() ) {
            ProcessMatch( score, pos, pos - abase.GetSubjectAlignedLength() + 1, true,  m.GetQuery(), m.GetPairmate() );
        } else {
            ProcessMatch( score, pos, pos + abase.GetSubjectAlignedLength() - 1, false, m.GetQuery(), m.GetPairmate() );
        }
	}
}

void CFilter::ProcessMatch( double score, int seqFrom, int seqTo, bool reverse, CQuery * query, int pairmate )
{
    ASSERT( query );
    ASSERT( m_ord >= 0 );
    if( query->IsPairedRead() == false ) {
        PurgeHit( new CHit( query, m_ord, pairmate, score, seqFrom, seqTo ) );
    } else { 
        int seqMin = reverse ? seqTo : seqFrom;
        int seqMax = reverse ? seqFrom : seqTo;
        
        int bottomPos = seqMax - m_maxDist + 1;
        int topPos = min( seqMax - m_minDist + 1, seqMin - 1 ); // just to be safe... reads can't be longer then 256
        // Here we try to find a pair for this match
        
        // First clear queue
        PurgeQueue( bottomPos - 1 );

        TPendingHits toAdd;
        TPendingHits::iterator phit =  m_pendingHits.begin();

        Uint2 matchFlags = (reverse << CHit::kRead1_reverse_bit) << pairmate;
        if( !pairmate ) matchFlags |= CHit::fOrder_reverse;

        bool found = false;

        for( ; phit != m_pendingHits.end() && phit->first <= topPos ; ++phit ) {
            if( phit->second->IsPurged() ) {
                cerr << DISPLAY( CBitHacks::AsBits( phit->second->GetGeometryNumber() ) ) << "\n"
                     << DISPLAY( CBitHacks::AsBits( phit->second->GetGeometryFlag() ) ) << "\n"
                     << DISPLAY( CBitHacks::AsBits( m_geometryFlags ) ) << "\n"
                     << DISPLAY( CBitHacks::AsBits( phit->second->ComputeChainedGeometryFlags() ) ) << "\n"
                     << DISPLAY( CBitHacks::AsBits( phit->second->ComputeExtentionGeometryFlags() ) ) << "\n"
                     << DISPLAY( phit->second->GetFrom() ) << DISPLAY( phit->second->GetTo() ) << "\n"
                     << DISPLAY( seqFrom ) << DISPLAY( seqTo ) << "\n"
                     << DISPLAY( bottomPos ) << DISPLAY( topPos ) << "\n"
                    ;
                abort();
            }
            
            // TODO: here: try to attach new hit to the existing hits, which may 
            // produce new hits which may be either purged or put in queue (for the latter 
            // they are to be added to toAdd queue and after the loop moved to main queue
            if( phit->second->GetQuery() != query ) continue;
            Uint2 geometry = 1 << (phit->second->GetGeometryNumber() | matchFlags);
            if( ( geometry & m_geometryFlags ) == 0 ) continue;

            switch( phit->second->GetComponentFlags() ) {
            case CHit::fPairedHit: 
                // Let's go here for combinatorial explosion for now...
                if( geometry & phit->second->ComputeExtentionGeometryFlags() & m_geometryFlags ) {
                    if( InRange( phit->second->GetFrom(), seqMax ) ) {
                        if( PairedHitSetPair( seqFrom, seqTo, reverse, pairmate, score, phit->second, toAdd, eExtend ) ) break;
                        found = true;
                    }
                }
                if( geometry & phit->second->ComputeChainedGeometryFlags() & m_geometryFlags ) {
                    if( InRange( phit->second->GetUpperHitMinPos(), seqMax ) ) {
                        found |= PairedHitSetPair( seqFrom, seqTo, reverse, pairmate, score, phit->second, toAdd, eChain );
                    }
                }
                break;
            case CHit::fComponent_1: 
                if( pairmate == 1 ) found |= SingleHitSetPair( seqFrom, seqTo, reverse, pairmate, score, phit->second, toAdd );
                continue;
            case CHit::fComponent_2: 
                if( pairmate == 0 ) found |= SingleHitSetPair( seqFrom, seqTo, reverse, pairmate, score, phit->second, toAdd );
                continue;
            default: THROW( logic_error, "Ooops here!" );
            }
        }
        // TODO: here: add toAdd hits to the new queue
        copy( toAdd.begin(), toAdd.end(), inserter( m_pendingHits, m_pendingHits.end() ) );

        // TODO: here: if it was not attached, put it in queue
        if( !found ) 
            m_pendingHits.insert( make_pair( seqMin, new CHit( query, m_ord, pairmate, score, seqFrom, seqTo ) ) );
    }
}

void CFilter::PurgeQueue( int bottomPos )
{
    // TODO: get back to optimized purging, no assertions
    TPendingHits::iterator phit = m_pendingHits.begin();
    set<CHit*> purged;
    for( ; phit != m_pendingHits.end() && phit->first < bottomPos ; ++phit ) {
        // decide should this hit be purged
        if( phit->second->GetComponentFlags() != CHit::fPairedHit ) purged.insert( phit->second ); //PurgeHit( phit->second );
        else if( phit->second->GetUpperHitMinPos() < bottomPos ) purged.insert( phit->second ); // PurgeHit( phit->second );
        else if( ( phit->second->ComputeChainedGeometryFlags() & m_geometryFlags ) == 0 ) purged.insert( phit->second ); //PurgeHit( phit->second );
    }
    m_pendingHits.erase( m_pendingHits.begin(), phit );
    ITERATE( TPendingHits, h, m_pendingHits ) {
        if( purged.find( h->second ) != purged.end() ) {
            cerr << h->second->GetQuery()->GetId() << "\t"
                 << h->second->GetFrom() << "\t"
                 << h->second->GetTo() << "\n";
            purged.erase( h->second );
        } 
    }
    ITERATE( set<CHit*>, h, purged ) PurgeHit( *h );        
}

void CFilter::PurgeQueueToTheEnd()
{
    // TODO: get back to optimized purging, no assertions
    set<CHit*> purged;
    for( TPendingHits::const_iterator i = m_pendingHits.begin(); i != m_pendingHits.end(); ++i ) {
        //PurgeHit( i->second, true );
        purged.insert( i->second );
    }
    ITERATE( set<CHit*>, i, purged ) PurgeHit( *i ); 
   	m_pendingHits.clear();
}

bool CFilter::SingleHitSetPair( int seqFrom, int seqTo, bool reverse, int pairmate, double score, CHit * hit, TPendingHits& toAdd )
{
    int maxPos = reverse ? seqFrom : seqTo;
    int minPos = reverse ? seqTo : seqFrom;
    if( InRange( hit->GetFrom(), maxPos ) && minPos > hit->GetFrom() && maxPos > hit->GetTo() ) {
        hit->SetPairmate( pairmate, score, seqFrom, seqTo );
        if( hit->ComputeChainedGeometryFlags() & m_geometryFlags )
            toAdd.insert( make_pair( minPos, hit ) );
        return true;
    }
    else return false;
}

bool CFilter::PairedHitSetPair( int seqFrom, int seqTo, bool reverse, int pairmate, double score, CHit * hit, TPendingHits& toAdd, EHitUpdateMode mode )
{
    if( score <= hit->GetScore( pairmate ) ) return false;
    int badPos = min( hit->GetFrom( pairmate ), hit->GetTo( pairmate ) );
    CHit * newHit = new CHit( hit->GetQuery(), m_ord, pairmate, hit->GetScore( pairmate ), hit->GetFrom( pairmate ), hit->GetTo( pairmate ) );
    try {
        hit->SetPairmate( pairmate, score, seqFrom, seqTo );
    } catch(...) {
        cerr 
            << DISPLAY( hit->GetFrom(0) ) 
            << DISPLAY( hit->GetTo(0) ) 
            << DISPLAY( hit->GetFrom(1) )
            << DISPLAY( hit->GetTo(1) ) 
            << endl
            << DISPLAY( newHit->GetFrom(0) ) 
            << DISPLAY( newHit->GetTo(0) ) 
            << DISPLAY( newHit->GetFrom(1) )
            << DISPLAY( newHit->GetTo(1) ) 
            << endl
            << DISPLAY( seqFrom ) 
            << DISPLAY( seqTo )
            << DISPLAY( pairmate ) << endl;
        throw;
    }
    bool toBeAdded = bool( newHit->ComputeExtentionGeometryFlags() | m_geometryFlags );
    bool added = false;
    for( TPendingHits::iterator i = m_pendingHits.find( badPos ); i != m_pendingHits.end() && i->first == badPos ; ++i ) {
        if( i->second == hit ) {
            i->second = toBeAdded ? newHit : 0;
            added = true;
        }
    }
    if( toBeAdded && !added ) toAdd.insert( make_pair( badPos, newHit ) );
    if( hit->ComputeExtentionGeometryFlags() & m_geometryFlags )
        toAdd.insert( make_pair( min( seqFrom, seqTo ), hit ) );
    return true;
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
    PurgeQueueToTheEnd();
    m_begin = m_end = 0;
}

void CFilter::PurgeHit( CHit * hit, bool setTarget )
{
    if( hit == 0 ) return;
    CQuery * q = hit->GetQuery();
    ASSERT( q );
    ASSERT( hit->IsPurged() == false );
    ASSERT( hit->GetNextHit() == 0 );
    ASSERT( hit->GetComponentFlags() );
    CHit::C_NextCtl( hit ).SetPurged();
    if( CHit * tih = q->GetTopHit() ) {
        double hscore = hit->GetTotalScore();
        double tscore = tih->GetTotalScore();
        double cscore = tscore * m_topPct/100;
        if( cscore > hscore ) { delete hit; return; }
        int topcnt = m_topCnt;
        if( tscore <= hscore ) {
            CHit::C_NextCtl( hit ).SetNext( tih );
            q->m_topHit = hit;
			cscore = ( tscore = hscore ) * m_topPct/100;
        } else {
			// trying to insert hit somewhere in the list
            bool weak = true;
            for( ; weak && tih->GetNextHit() && topcnt > 0; (tih = tih->GetNextHit()), --topcnt ) {
                if( tih->GetNextHit()->IsNull() || tih->GetNextHit()->GetTotalScore() < hscore ) {
                    // keep chain linked
                    CHit::C_NextCtl( hit ).SetNext( tih->GetNextHit() );
                    CHit::C_NextCtl( tih ).SetNext( hit );
                    weak = false;
                }
            }
			// oops - the hit is too weak, ignore it 
            if( weak ) {
                delete hit;
                return;
            }
        }
        for( tih = hit; topcnt > 0 && tih->GetNextHit() && tih->GetNextHit()->GetTotalScore() >= cscore; tih = tih->GetNextHit(), --topcnt );
        if( tih->GetNextHit() && !tih->GetNextHit()->IsNull() ) {
            CHit::C_NextCtl( tih ).SetNext( topcnt ? new CHit( q ) : 0 ); // if we clipped by score, we need to restore terminator
        }
    } else {
        q->m_topHit = hit;
        CHit::C_NextCtl( hit ).SetNext( new CHit( q ) );
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


