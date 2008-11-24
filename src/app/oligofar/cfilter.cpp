#include <ncbi_pch.hpp>
#include "cfilter.hpp"
#include "ialigner.hpp"
#include "cquery.hpp"
#include "coutputformatter.hpp"

USING_OLIGOFAR_SCOPES;

static const bool gsx_allowReverseAlignment = true;

void CFilter::Match( const CHashAtom& m, const char * a, const char * A, int pos ) // always start at pos
{
    ASSERT( m_aligner );
    if( int conv = m.GetConv() ) {
        if( conv & CHashAtom::fFlags_convTC ) {
            int tbl = m.IsForwardStrand() ? CScoreTbl::eSel_ConvTC : CScoreTbl::eSel_ConvTC ;
            m_aligner->SelectBasicScoreTables( tbl );
            MatchConv( m, a, A, pos );
        }
        if( conv & CHashAtom::fFlags_convAG ) {
            int tbl = m.IsForwardStrand() ? CScoreTbl::eSel_ConvAG : CScoreTbl::eSel_ConvAG ;
            m_aligner->SelectBasicScoreTables( tbl );
            MatchConv( m, a, A, pos );
        }
    } else {
        m_aligner->SelectBasicScoreTables( CScoreTbl::eSel_NoConv );
        MatchConv( m, a, A, pos );
    }
}

void CFilter::MatchConv( const CHashAtom& m, const char * a, const char * A, int pos ) // always start at pos
{
    double bestScore = m.GetQuery()->GetBestScore( m.GetPairmate() );
    m_aligner->SetBestPossibleQueryScore( bestScore );
    bool reverseStrand = m.IsReverseStrand();
    if( reverseStrand && m.GetQuery()->GetCoding() == CSeqCoding::eCoding_colorsp ) --pos;
    const CAlignerBase& abase = m_aligner->GetAlignerBase();
    if( gsx_allowReverseAlignment && m.GetOffset() >= 3 ) {
        int one = reverseStrand ? -1 : 1;
        int off = m.GetOffset() * one;
        const CAlignerBase& abase = m_aligner->GetAlignerBase();
        m_aligner->Align( m.GetQuery()->GetCoding(),
                          m.GetQuery()->GetData( m.GetPairmate() ) + m.GetOffset(),
                          - m.GetOffset() - 1,
                          CSeqCoding::eCoding_ncbi8na,
                          a + pos + off,
                          reverseStrand ? A - a - pos : -pos,
                          CAlignerBase::fComputeScore );
        double score = abase.GetRawScore();
        int from = pos + off - one * (abase.GetSubjectAlignedLength() - 1);

        m_aligner->Align( m.GetQuery()->GetCoding(),
                          m.GetQuery()->GetData( m.GetPairmate() ) + m.GetOffset() + 1,
                          m.GetQuery()->GetLength( m.GetPairmate() ) - m.GetOffset() - 1,
                          CSeqCoding::eCoding_ncbi8na,
                          a + pos + off + one,
                          reverseStrand ? -pos : A - a - pos,
                          CAlignerBase::fComputeScore );
        score += abase.GetRawScore();
        int to = pos + off + one * (abase.GetSubjectAlignedLength());

        score *= 100.0/bestScore;
        if( score >= m_scoreCutoff ) { ProcessMatch( score, from, to, reverseStrand,  m.GetQuery(), m.GetPairmate() ); }
    } else {
        m_aligner->Align( m.GetQuery()->GetCoding(),
                          m.GetQuery()->GetData( m.GetPairmate() ),
                          m.GetQuery()->GetLength( m.GetPairmate() ),
                          CSeqCoding::eCoding_ncbi8na,
                          a + pos,
                          reverseStrand ? -pos : A - a - pos,
                          CAlignerBase::fComputeScore );
        double score = abase.GetScore();
        if( score >= m_scoreCutoff ) {
            if( reverseStrand ) {
                ProcessMatch( score, pos, pos - abase.GetSubjectAlignedLength() + 1, true,  m.GetQuery(), m.GetPairmate() );
            } else {
                ProcessMatch( score, pos, pos + abase.GetSubjectAlignedLength() - 1, false, m.GetQuery(), m.GetPairmate() );
            }
        }
    }
}

void CFilter::ProcessMatch( double score, int seqFrom, int seqTo, bool reverse, CQuery * query, int pairmate )
{
    ASSERT( query );
    ASSERT( m_ord >= 0 );
    if( query->IsPairedRead() == false ) {
        PurgeHit( new CHit( query, m_ord, pairmate, score, seqFrom, seqTo, m_aligner->GetBasicScoreTables() ) );
    } else { 
        Uint4 componentGeometry = Uint4( reverse ) | ( pairmate << 1 );
        ASSERT( (m_geometry & ~3) == 0 );
        ASSERT( (componentGeometry & ~3) == 0 );

        enum EAction { eAction_Append = 'A', eAction_Lookup = 'L' };
        switch( "ALALLALALLAAAALL"[ (componentGeometry << 2) | m_geometry ] ) {
        default: THROW( logic_error, "Invalid action here!" );
        case eAction_Append:
            m_pendingHits.insert( make_pair( reverse ? seqTo : seqFrom, new CHit( query, m_ord, pairmate, score, seqFrom, seqTo, m_aligner->GetBasicScoreTables() ) ) );
            break;
        case eAction_Lookup:
            do {
                TPendingHits toAdd;
                if( ! LookupInQueue( score, seqFrom, seqTo, reverse, query, pairmate, toAdd, true ) )
                    PurgeHit( new CHit( query, m_ord, pairmate, score, seqFrom, seqTo, m_aligner->GetBasicScoreTables() ) );
                copy( toAdd.begin(), toAdd.end(), inserter( m_pendingHits, m_pendingHits.end() ) );
            } while(0);
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
            hit->SetPairmate( pairmate, score, seqFrom, seqTo, m_aligner->GetBasicScoreTables() );
            if( hit->IsOverlap() || hit->GetFilterGeometry() != m_geometry ) {
                cerr << "\n\033[32mUpdated hit: " << ( hit->IsOverlap() ? "reads overlap" : "wrong geometry" ) << " {\x1b[032m";
                hit->PrintDebug( cerr );
                cerr << "\x1b[32m}\033[0m\n";
                cerr << DISPLAY( hit->GetFilterGeometry() ) << DISPLAY( m_geometry ) << endl;
                cerr << hit->GetLength(0) << ", " << hit->GetLength(1) << "\n";
                ASSERT( hit->GetComponentFlags() == CHit::fPairedHit );
            }
            found = true;
        } else if( hit->IsReverseStrand( pairmate ) == reverse ) { // the strict criterion on mutual orientation suggests this
            int pos = phit->first;
            CHit * nhit = pairmate ? 
                new CHit( query, m_ord, hit->GetScore(0), hit->GetFrom(0), hit->GetTo(0), hit->GetConvTbl(0), score, seqFrom, seqTo, m_aligner->GetBasicScoreTables() ) :
                new CHit( query, m_ord, score, seqFrom, seqTo, m_aligner->GetBasicScoreTables(), hit->GetScore(1), hit->GetFrom(1), hit->GetTo(1), hit->GetConvTbl(1) );
            if( nhit->IsOverlap() || nhit->GetFilterGeometry() != m_geometry ) {
                cerr << "\n\033[35mDEBUG: Ignoring " << ( hit->IsOverlap() ? "reads overlap" : "wrong geometry" ) << " {\x1b[36m";
                nhit->PrintDebug( cerr );
                cerr << "\x1b[35m}\033[0m\n";
                cerr << "\n\033[35m                       old hit {\x1b[36m";
                hit->PrintDebug( cerr );
                cerr << "\x1b[35m}\033[0m\n";
                cerr << DISPLAY( nhit->GetFilterGeometry() ) << DISPLAY( m_geometry ) << DISPLAY( pairmate ) << endl;
                cerr << "scoreCutoff = " << m_scoreCutoff << "\n";
                if( pairmate ) {
                    cerr << hit->GetFrom(0) << "->" << hit->GetTo(0) << "\t" << seqFrom << "=>" << seqTo << "\n";
                } else {
                    cerr << seqFrom << "=>" << seqTo << "\t" << hit->GetFrom(1) << "->" << hit->GetTo(1) << "\n";
                }
                cerr << "\n";
                delete nhit;
            } else {
                toAdd.insert( make_pair( pos, nhit ) );
                found = true;
                while( phit != m_pendingHits.end() && phit->first == pos ) ++phit; // to prevent multiplication of existing hits
                if( phit == m_pendingHits.end() ) break;
            }
        } // else it is ignored
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

inline ostream& operator << ( ostream& o, CHit * h ) 
{
    o << "\x033[32mhit\x033[33m{\x033[34m";
    h->PrintDebug( o );
    o << "\x033[33m}\x033[0m";
    return o;
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
        int topcnt = m_topCnt - 1;

        if( tih->ClustersWithSameQ( hit ) ) {
            if( tscore <= hscore ) {
                q->m_topHit = hit;
                CHit::C_NextCtl( hit ).SetNext( tih->GetNextHit() );
                CHit::C_NextCtl( tih ).SetNext( 0 );
                delete tih;
                tih = 0;
            } else {
                delete hit; return; 
            }
        } else if( tscore < hscore ) {
            CHit::C_NextCtl( hit ).SetNext( tih );
            q->m_topHit = hit;
            cscore = ( tscore = hscore ) * m_topPct/100;
        } else {
            // trying to insert hit somewhere in the list
            bool weak = true;
            for( ; weak && tih->GetNextHit() && topcnt > 0; (tih = tih->GetNextHit()), --topcnt ) {
                CHit * nih = tih->GetNextHit();
                if( nih->ClustersWithSameQ( hit ) ) {
                    if( nih->GetTotalScore() >= hit->GetTotalScore() ) {
                        delete hit;
                    } else {
                        CHit::C_NextCtl( tih ).SetNext( hit );
                        CHit::C_NextCtl( hit ).SetNext( nih->GetNextHit() );
                        CHit::C_NextCtl( nih ).SetNext( 0 );
                        delete nih;
                    }
                    return;
                } else if( nih->IsNull() || nih->GetTotalScore() < hscore ) {
                    // keep chain linked
                    CHit::C_NextCtl( hit ).SetNext( nih );
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
        for( tih = hit; topcnt > 0 && tih->GetNextHit() && tih->GetNextHit()->GetTotalScore() >= cscore; tih = tih->GetNextHit(), --topcnt ) { 
            CHit * nih = tih->GetNextHit();
            if( nih->ClustersWithSameQ( hit ) ) {
                CHit::C_NextCtl( tih ).SetNext( nih->GetNextHit() );
                CHit::C_NextCtl( nih ).SetNext( 0 );
                delete nih;
                ++topcnt;
            }
        }
        if( tih->GetNextHit() && !tih->GetNextHit()->IsNull() ) {
            delete tih->GetNextHit();
            CHit::C_NextCtl( tih ).SetNext( topcnt > 0 ? new CHit( q ) : 0 ); // if we clipped by score, we need to restore terminator
        }
    } else {
        q->m_topHit = hit;
        ASSERT( hit->GetNextHit() == 0 );
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


