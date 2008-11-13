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
    bool reverseStrand = m.IsReverseStrand();
    if( reverseStrand && m.GetQuery()->GetCoding() == CSeqCoding::eCoding_colorsp ) --pos;
    m_aligner->Align( m.GetQuery()->GetCoding(),
                      m.GetQuery()->GetData( m.GetPairmate() ),
                      m.GetQuery()->GetLength( m.GetPairmate() ),
                      CSeqCoding::eCoding_ncbi8na,
                      a + pos,
                      reverseStrand ? -pos : A - a - pos,
                      CAlignerBase::fComputeScore );
    const CAlignerBase& abase = m_aligner->GetAlignerBase();
    double score = abase.GetScore();
    if( score >= m_scoreCutoff ) {
        if( reverseStrand ) {
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
        Uint4 componentGeometry = reverse | ( pairmate << 1 );
        ASSERT( (m_geometry & ~3) == 0 );
        ASSERT( (componentGeometry & ~3) == 0 );

        enum EAction { eAction_Append = 'A', eAction_Lookup = 'L' };
        switch( "ALALLALALLAAAALL"[ (componentGeometry << 2) | m_geometry ] ) {
        default: THROW( logic_error, "Invalid action here!" );
        case eAction_Append:
            m_pendingHits.insert( make_pair( reverse ? seqTo : seqFrom, new CHit( query, m_ord, pairmate, score, seqFrom, seqTo ) ) );
            break;
        case eAction_Lookup:
            do {
                TPendingHits toAdd;
                if( ! LookupInQueue( score, seqFrom, seqTo, reverse, query, pairmate, toAdd, true ) )
                    PurgeHit( new CHit( query, m_ord, pairmate, score, seqFrom, seqTo ) );
                copy( toAdd.begin(), toAdd.end(), inserter( m_pendingHits, m_pendingHits.end() ) );
            } while(0);
            break;
            break;
        }
    }
}

bool CFilter::LookupInQueue( double score, int seqFrom, int seqTo, bool reverse, CQuery * query, int pairmate, TPendingHits& toAdd, bool canPurgeQueue )
{
    int seqMin = reverse ? seqTo : seqFrom;
    int seqMax = reverse ? seqFrom : seqTo;
    int bottomPos = seqMax - m_maxDist + 1;
    int topPos = min( seqMax - m_minDist + 1, seqMin - 1 ); // just to be safe... reads can't be longer then 256
    // First clear queue
    if( canPurgeQueue )
        PurgeQueue( bottomPos - 1 - m_reserveDist );
    
    TPendingHits::iterator phit =  m_pendingHits.begin();
    
    bool found = false;

    for( ; phit != m_pendingHits.end() && phit->first <  bottomPos ; ++phit );
    for( ; phit != m_pendingHits.end() && phit->first <= topPos ; ++phit ) {
        CHit * hit = phit->second;
        if( hit->IsPurged() ) 
            THROW( logic_error, "Attempt to use purged hit here!"
                 << DISPLAY( CBitHacks::AsBits( hit->GetGeometry() ) ) << "\n"
                 << DISPLAY( CBitHacks::AsBits( m_geometry ) ) << "\n"
                 << DISPLAY( hit->GetFrom() ) << DISPLAY( hit->GetTo() ) << "\n"
                 << DISPLAY( seqFrom ) << DISPLAY( seqTo ) << "\n"
                 << DISPLAY( bottomPos ) << DISPLAY( topPos ) << "\n" );
        
        if( hit->GetQuery() != query ) continue;
        if( !hit->HasComponent( !pairmate ) ) continue;
        if( !hit->HasComponent( pairmate ) ) {
            hit->SetPairmate( pairmate, score, seqFrom, seqTo );
            found = true;
        } else {
            int pos = phit->first;
            toAdd.insert( make_pair( pos, pairmate ? 
                         new CHit( query, m_ord, hit->GetScore(0), hit->GetFrom(0), hit->GetTo(0), score, seqFrom, seqTo ) :
                         new CHit( query, m_ord, score, seqFrom, seqTo, hit->GetScore(1), hit->GetFrom(1), hit->GetTo(1) ) ) );
            found = true;
            while( phit != m_pendingHits.end() && phit->first == pos ) ++phit; // to prevent multiplication of existing hits
        }
    }
    return found;
}

void CFilter::PurgeQueue( int bottomPos )
{
    TPendingHits::iterator phit = m_pendingHits.begin();
    for( ; phit != m_pendingHits.end() && phit->first < bottomPos ; ++phit ) {
        PurgeHit( phit->second );
    }
    m_pendingHits.erase( m_pendingHits.begin(), phit );
}

void CFilter::PurgeQueueToTheEnd()
{
    set<CHit*> purged;
    for( TPendingHits::const_iterator i = m_pendingHits.begin(); i != m_pendingHits.end(); ++i ) {
        PurgeHit( i->second, true );
    }
   	m_pendingHits.clear();
}

bool CFilter::CheckGeometry( int from1, int to1, int from2, int to2 ) const
{
    int geo = CHit::ComputeGeometry( from1, to1, from2, to2 );
    switch( geo ) {
    case CHit::fGeometry_Fwd1_Fwd2: return InRange( from1, to2 );
    case CHit::fGeometry_Fwd1_Rev2: return InRange( from1, from2 );
    case CHit::fGeometry_Rev1_Fwd2: return InRange( to1, to2 );
    case CHit::fGeometry_Rev1_Rev2: return InRange( to1, from2 );
    case CHit::fGeometry_Fwd2_Fwd1: return InRange( from2, to1 );
    case CHit::fGeometry_Fwd2_Rev1: return InRange( from2, from1 );
    case CHit::fGeometry_Rev2_Fwd1: return InRange( to2, to1 );
    case CHit::fGeometry_Rev2_Rev1: return InRange( to2, from1 );
    default: return false;
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
            if( tih->Equals( hit ) ) { delete hit; return; } // uniqueing hits
            CHit::C_NextCtl( hit ).SetNext( tih );
            q->m_topHit = hit;
			cscore = ( tscore = hscore ) * m_topPct/100;
        } else {
			// trying to insert hit somewhere in the list
            bool weak = true;
            for( ; weak && tih->GetNextHit() && topcnt > 0; (tih = tih->GetNextHit()), --topcnt ) {
                if( tih->GetNextHit()->IsNull() || tih->GetNextHit()->GetTotalScore() < hscore ) {
                    if( tih->Equals( hit ) ) { delete hit; return; }
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


