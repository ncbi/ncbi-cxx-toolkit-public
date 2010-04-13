#include <ncbi_pch.hpp>
#include "cfilter.hpp"
#include "ialigner.hpp"
#include "caligner.hpp"
#include "cquery.hpp"
#include "ioutputformatter.hpp"
#include "cscoringfactory.hpp"
#include "ialigner.hpp"
#include <sstream>

USING_OLIGOFAR_SCOPES;

static const bool gsx_allowReverseAlignment = true;

Uint8 CFilter::s_longestQueue = 0;

void CFilter::SetAligner( IAligner * a ) 
{
    m_aligner = a;
    m_scoringFactory = a ? a->GetScoringFactory() : 0;
}

void CFilter::Match( const CHashAtom& m, const char * a, const char * A, int pos1, int pos2 ) // always start at pos
{
    ASSERT( m_aligner );
    ASSERT( m_scoringFactory );
    if( int conv = m.GetConv() ) {
        if( conv & CHashAtom::fFlags_convTC ) {
            CScoringFactory::EConvertionTable tbl = m.IsForwardStrand() ? CScoringFactory::eConvTC : CScoringFactory::eConvTC ;
            m_scoringFactory->SetConvertionTable( tbl );
            MatchConv( m, a, A, pos1, pos2 );
        }
        if( conv & CHashAtom::fFlags_convAG ) {
            CScoringFactory::EConvertionTable tbl = m.IsReverseStrand() ? CScoringFactory::eConvAG : CScoringFactory::eConvAG ;
            m_scoringFactory->SetConvertionTable( tbl );
            MatchConv( m, a, A, pos1, pos2 );
        }
    } else {
        m_scoringFactory->SetConvertionTable( CScoringFactory::eNoConv );
        MatchConv( m, a, A, pos1, pos2 );
    }
}

double CFilter::ComputePenaltyLimit( const CHashAtom& m, double bestScore ) const
{
    // SCORE -- REWRITE
    double worstScore = bestScore * m_scoreCutoff / 100;
    if( m_topPct > m_scoreCutoff ) {
        // int p = m.GetQuery()->IsPairedRead() ? 1 : 0;
#if DEVELOPMENT_VER
        double p = m.GetQuery()->ComputeBestScore( m_scoringFactory->GetScoreParam(), ! m.GetPairmate() );
#else
        double p = m.GetQuery()->GetBestScore( ! m.GetPairmate() );
#endif
        if( CHit * hp = m.GetQuery()->GetTopHit( 3 ) ) 
            worstScore = max( worstScore, (hp->GetTotalScore() * m_topPct/100/*00*/ - p) /* * bestScore */ );
        if( CHit * hs = m.GetQuery()->GetTopHit( m.GetPairmask() ) ) 
            worstScore = max( worstScore, (hs->GetTotalScore() * m_topPct/100/*00*/ - p) /* * bestScore */ );
    }
    double penaltyLimit = worstScore - bestScore; 
    if( penaltyLimit > 0 ) {
        THROW( runtime_error, "Bad penalty limit" << DISPLAY( penaltyLimit ) << DISPLAY( bestScore ) << DISPLAY( worstScore ) << DISPLAY( m_scoreCutoff ) << DISPLAY( m_topPct ) );
    }
    return penaltyLimit;
}

void CFilter::MatchConv( const CHashAtom& m, const char * a, const char * A, int pos1, int pos2 ) // always start at pos
{
    bool reverseStrand = m.IsReverseStrand();
    if( reverseStrand && m.GetQuery()->GetCoding() == CSeqCoding::eCoding_colorsp ) { --pos1; --pos2; }

    /*========================================================================*
     * Determine which parts of the read should align where 
     *========================================================================*/

    int offset = m.GetOffset(); // offset
    int length = m.GetQuery()->GetLength( m.GetPairmate() );
    int width = pos2 - pos1 + 1; // window width
    if( offset + length < width ) return; // does not fit

    /*========================================================================*
     * Determine worst score allowed and penalty limit
     *========================================================================*/

    // this is the value for which and below there is no reason to continue alignment
#if DEVELOPMENT_VER
    double bestScore = m.GetQuery()->ComputeBestScore( m_scoringFactory->GetScoreParam(), m.GetPairmate() );
#else
    double bestScore = m.GetQuery()->GetBestScore( m.GetPairmate() );
#endif
    double penaltyLimit = ComputePenaltyLimit( m, bestScore );

    /*========================================================================*
     * Filter out too bad windows 
     * (TODO: this may be too restrictive if we use quality scores)
     *========================================================================*/

    if( m_prefilter && m.HasDiffs() ) {
        const CScoreParam * sp = m_scoringFactory->GetScoreParam();
        double wordPenalty = 
            /// Following line is removed for purpose: it would unnecessarily filter away "weak" mismatches (MismScore) and dovetails (IdntScore)
            /// - ( m.GetMismatches() * ( sp->GetMismatchPenalty() + sp->GetIdentityScore() ) ) 
            - ( m.GetInsertions() ? ( sp->GetGapBasePenalty() + m.GetInsertions() * ( sp->GetGapExtentionPenalty() ) ) : 0 )
            - ( m.GetDeletions()  ? ( sp->GetGapBasePenalty() + m.GetDeletions()  * ( sp->GetGapExtentionPenalty() + sp->GetIdentityScore() ) ) : 0 )
            ;
        if( wordPenalty < penaltyLimit ) return;
    }

    /*========================================================================*
     * Here aligning code should start
     *========================================================================*/

    m_aligner->SetQuery( m.GetQuery()->GetData( m.GetPairmate() ), length );
    m_aligner->SetQueryCoding( m.GetQuery()->GetCoding() );

    // NB: m.Insertions mean insertion on subject side, while output insertion means that query has extra base,
    // therefore here (in this function) we have opposite meanings of insertions and deletions to compute 
    // positions and penalties

    int qpos2 = offset + width - 1 - m.GetInsertions() + m.GetDeletions();

    m_aligner->SetQueryAnchor( offset, qpos2 );
    m_aligner->SetSubjectAnchor( pos1, pos2 );
    m_aligner->SetSubjectStrand( m.IsReverseStrand() ? CSeqCoding::eStrand_neg : CSeqCoding::eStrand_pos );
    m_aligner->SetPenaltyLimit( penaltyLimit );
    m_aligner->SetWordDistance( m.GetMismatches() + m.GetInsertions() + m.GetDeletions() );

    if( m_aligner->Align() ) {
        double score = bestScore + m_aligner->GetPenalty();

        // SCORE -- OK
        ProcessMatch( score, // / bestScore * 100, 
            m_aligner->GetSubjectFrom(), m_aligner->GetSubjectTo(), 
            reverseStrand,  m.GetQuery(), m.GetPairmate() ); 
    }
}

void CFilter::ProcessMatch( double score, int seqFrom, int seqTo, bool reverse, CQuery * query, int pairmate )
{
    ASSERT( query );
    ASSERT( m_ord >= 0 );
    if( reverse ) swap( seqFrom, seqTo );
    const TTranscript& transcript = m_aligner->GetSubjectTranscript();

    if( query->IsPairedRead() == false ) {
        PurgeHit( new CHit( query, m_ord, pairmate, score, seqFrom, seqTo, m_scoringFactory->GetConvertionTable(), transcript ) );
    } else { 
        Uint4 componentGeometry = Uint4( reverse ) | ( pairmate << 1 );
        ASSERT( (m_geometry & ~3) == 0 );
        ASSERT( (componentGeometry & ~3) == 0 );

        enum EAction { eAction_Append = 'A', eAction_Lookup = 'L' };
        switch( "ALALLALALLAAAALL"[ (componentGeometry << 2) | m_geometry ] ) {
        default: THROW( logic_error, "Invalid action here!" );
        case eAction_Append:
            m_pendingHits.insert( make_pair( reverse ? seqTo : seqFrom, new CHit( query, m_ord, pairmate, score, seqFrom, seqTo, m_scoringFactory->GetConvertionTable(), transcript ) ) );
            break;
        case eAction_Lookup:
            do {
                TPendingHits toAdd;
                if( ! LookupInQueue( score, seqFrom, seqTo, reverse, query, pairmate, toAdd, true, transcript ) ) {
                    PurgeHit( new CHit( query, m_ord, pairmate, score, seqFrom, seqTo, m_scoringFactory->GetConvertionTable(), transcript ) );
                }
                copy( toAdd.begin(), toAdd.end(), inserter( m_pendingHits, m_pendingHits.end() ) );
            } while(0);
            break;
        }
    }
}

bool CFilter::LookupInQueue( double score, int seqFrom, int seqTo, bool reverse, CQuery * query, int pairmate, TPendingHits& toAdd, bool canPurgeQueue, const TTranscript& transcript )
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
            if( pairmate ) {
                if( ! CheckGeometry( hit->GetFrom(0), hit->GetTo(0), seqFrom, seqTo ) ) continue;
            } else {
                if( ! CheckGeometry( seqFrom, seqTo, hit->GetFrom(1), hit->GetTo(1) ) ) continue;
            }
            hit->SetPairmate( pairmate, score, seqFrom, seqTo, m_scoringFactory->GetConvertionTable(), transcript );
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
            if( pairmate ) {
                if( ! CheckGeometry( hit->GetFrom(0), hit->GetTo(0), seqFrom, seqTo ) ) continue;
            } else {
                if( ! CheckGeometry( seqFrom, seqTo, hit->GetFrom(1), hit->GetTo(1) ) ) continue;
            }
            CHit * nhit = pairmate ? 
                new CHit( query, m_ord, hit->GetScore(0), hit->GetFrom(0), hit->GetTo(0), hit->GetConvTbl(0), score, seqFrom, seqTo, m_scoringFactory->GetConvertionTable(), hit->GetTranscript(0), transcript ) :
                new CHit( query, m_ord, score, seqFrom, seqTo, m_scoringFactory->GetConvertionTable(), hit->GetScore(1), hit->GetFrom(1), hit->GetTo(1), hit->GetConvTbl(1), transcript, hit->GetTranscript(1) );
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
                for( TPendingHits::iterator nhit = phit; ++nhit != m_pendingHits.end() ; )
                    if( phit->first == nhit->first ) phit = nhit;
                // while( phit != m_pendingHits.end() && phit->first == pos ) ++phit; // to prevent multiplication of existing hits
                // if( phit == m_pendingHits.end() ) break;
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
    /*
    cerr << "\n\x1b[31;43m\x1b[K" << oid << "\t";
    ITERATE( TSeqIds, i, id ) {
        cerr << " [" << (*i)->AsFastaString() << "]";
    }
    cerr << "\x1b[0m\n";
    */
    m_ord = m_seqIds->Register( id, oid );
}

void CFilter::SequenceBuffer( CSeqBuffer* buffer ) 
{
    m_begin = buffer->GetBeginPtr();
    m_end = buffer->GetEndPtr();
    ASSERT( m_aligner );
    m_aligner->SetSubject( m_begin, m_end - m_begin );
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

// TODO: clip by score unpaired reads if we have added the paired one on top (?)
void CFilter::PurgeHit( CHit * hit, bool setTarget )
{
    if( hit == 0 ) return;
    CQuery * q = hit->GetQuery();
    ASSERT( q );
    ASSERT( hit->IsPurged() == false );
    ASSERT( hit->GetNextHit() == 0 );
    ASSERT( hit->GetComponentFlags() );
    CHit::C_NextCtl( hit ).SetPurged();
    ASSERT( hit->IsPurged() == true );
    if( CHit * top = q->GetTopHit( hit->GetComponentMask() ) ) {
        ASSERT( top->IsNull() || top->IsPurged() );
        double hitscore = hit->GetTotalScore();
        double topscore = top->GetTotalScore();
        double cutscore = topscore * m_topPct/100;
        if( cutscore > hitscore ) { delete hit; return; } // chain was not changed - leave
        int topcnt = m_topCnt - 1;

        if( top->ClustersWithSameQ( hit ) ) {
            if( topscore <= hitscore ) {
                q->SetTopHit( hit );
                CHit::C_NextCtl( hit ).SetNext( top->GetNextHit() );
                CHit::C_NextCtl( top ).SetNext( 0 );
                delete top;
                top = 0;
                cutscore = ( topscore = hitscore ) * m_topPct/100;
            } else {
                delete hit; return; // chain was not changed - leave
            }
        } else if( topscore <= hitscore ) { 
            // NB: it HAS to be <=, otherwise else brunch should be rewritten to take special care of `=' case, otherwise NULL will not be deleted when it should
            CHit::C_NextCtl( hit ).SetNext( top );
            q->SetTopHit( hit );
            cutscore = ( topscore = hitscore ) * m_topPct/100;
        } else { // topscore > hitscore
            // trying to insert hit somewhere in the list
            bool weak = true;
            for( ; weak && top->GetNextHit() && topcnt > 0; (top = top->GetNextHit()), --topcnt ) {
                CHit * nxt = top->GetNextHit();
                if( (!nxt->IsNull()) && nxt->ClustersWithSameQ( hit ) ) { // it does check components
                    if( nxt->GetTotalScore() >= hitscore ) {
                        delete hit;
                    } else {
                        CHit::C_NextCtl( top ).SetNext( hit );
                        CHit::C_NextCtl( hit ).SetNext( nxt->GetNextHit() );
                        CHit::C_NextCtl( nxt ).SetNext( 0 );
                        delete nxt;
                    }
                    return; // we either did not change chain, or have changed middle of the chain - no farther changes needed
                } else if( nxt->IsNull() || nxt->GetTotalScore() < hitscore ) {
                    // keep chain linked
                    CHit::C_NextCtl( hit ).SetNext( nxt );
                    CHit::C_NextCtl( top ).SetNext( hit );
                    weak = false;
                }
            }
            // oops - the hit is too weak, we did nto find a place to insert it - so ignore it,
            // and since chain have not changed - exit
            if( weak ) {
                delete hit;
                return;
            }
        }
        // here we have updated chain, and accordingly we have updated topcnt and cutscore
        CHit * cur = hit;
        for( ; topcnt > 0 ; cur = cur->GetNextHit() ) { 
            CHit * nxt = cur->GetNextHit();
            if( nxt == 0 ) break;
            if( nxt->IsNull() ) { ASSERT( nxt->GetNextHit() == 0 ); break; }
            --topcnt;
            if( nxt->GetTotalScore() < cutscore ) break;
            if( nxt->ClustersWithSameQ( hit ) ) {
                CHit::C_NextCtl( cur ).SetNext( nxt->GetNextHit() );
                CHit::C_NextCtl( nxt ).SetNext( 0 );
                delete nxt;
                nxt = cur->GetNextHit();
                ++topcnt;
                if( nxt == 0 ) break;
            }
        }
        // we have reached the end of chain as it should be clipped
        if( cur->GetNextHit() && !cur->GetNextHit()->IsNull() ) {
            delete cur->GetNextHit();
            CHit::C_NextCtl( cur ).SetNext( topcnt > 0 ? new CHit( q ) : 0 ); // if we clipped by score, we need to restore terminator
        }
    } else {
        q->SetTopHit( hit );
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


