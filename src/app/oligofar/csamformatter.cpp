#include <ncbi_pch.hpp>
#include "oligofar-version.hpp"
#include "csamformatter.hpp"
#include "csammdformatter.hpp"
#include "cguidefile.hpp"
#include "ialigner.hpp"
#include "chit.hpp"
#include <cmath>

USING_OLIGOFAR_SCOPES;

const double kHundredPct = 100.000000001;
const int kUnlimitedRankSize = 10000000;

void CSamFormatter::FormatHeader( const string& version )
{
    m_out 
        << "@HD\tVN:1.2\tGO:query\n" //\tSO:queryname\n" // it is not sort order, it is group order
        << "@PG\tID:oligoFAR\tVN:" << version << "\n"; 
}

void CSamFormatter::FormatHit( const CHit* ) {} // no any hit formatting is supported

void CSamFormatter::FormatQueryHits( const CQuery* query )
{
    if( query == 0 ) return;
    for( int mask = 3; mask > 0; --mask ) {
        if( const CHit * hit = query->GetTopHit( mask ) ) {
            typedef list<const CHit*> THitList;
            typedef vector<pair<bool, THitList> > TRankedHits;
            TRankedHits rankedHits;
            for( ; hit; ) {
                //if( hit->IsNull() ) { 
                    if( rankedHits.size() ) 
                        rankedHits.back().first = true; // previous rank hit count is known
                    if( hit->IsNull() ) break;
                //    break;
                //}
                rankedHits.push_back( make_pair( false, THitList() ) );
                rankedHits.back().second.push_back( hit );
                CHit * next = hit->GetNextHit();
                for( ; next && (!next->IsNull()) && next->GetTotalScore() == hit->GetTotalScore(); next = next->GetNextHit() )
                    rankedHits.back().second.push_back( next );
                hit = next;
            }
            int rank = 0;
            ITERATE( TRankedHits, hitsRank, rankedHits ) {
                int rankSize = hitsRank->first ? hitsRank->second.size() : kUnlimitedRankSize;
                ITERATE( THitList, h, hitsRank->second ) {
                    FormatHit( rank, rankSize, (*h), 0 );
                    FormatHit( rank, rankSize, (*h), 1 );
                }
                ++rank;
            }
            if( ( !m_formatUnpaired ) && ( mask == 3 ) ) break; // If we have paired reads - ignore unpaired
        } else {
            // format empty hit here
        }
    }
    delete query;
}

void CSamFormatter::FormatHit( int rank, int rankSize, const CHit* hit, int pairmate )
{
    if( !m_headerPrinted ) {
        FormatHeader( OLIGOFAR_VERSION );
        m_headerPrinted = true;
    }
    if( ( hit->GetComponentFlags() & (CHit::fComponent_1 << pairmate) ) == 0 ) return; // don't output unmapped pair mates
    const CQuery * query = hit->GetQuery();
    int flags = 0;
    if( hit->IsReverseStrand( pairmate ) ) { flags |= fSeqReverse; }
    if( rank ) { flags |= fHitSuboptimal; }
    if( query->GetLength( !pairmate ) ) {
        flags |= fPairedQuery; 
        flags |= pairmate ? fSeqIsSecond : fSeqIsFirst;
        if( hit->GetComponentMask() == 3 ) {
            flags |= fPairedHit;
            if( hit->IsReverseStrand( !pairmate ) ) {
                flags |= fMateReverse;
            }
        } else if( hit->GetQuery()->GetTopHit( 1 << (1&!pairmate) ) == 0 ) { // only if there are no hits for the pairmate
            flags |= fMateUnmapped;
        }
    }

    int from1 = min( hit->GetFrom(pairmate), hit->GetTo(pairmate) ) + 1;
    int from2 = 0;
    int insert = 0;
    if( hit->GetLength( !pairmate ) )  {
        from2 = min( hit->GetFrom(!pairmate), hit->GetTo(!pairmate) ) + 1;
        insert = hit->GetFrom( !pairmate ) - hit->GetFrom( pairmate );
    }

    ostringstream iupac;
    query->PrintIupac( iupac, pairmate, hit->GetStrand( pairmate ) );
    string iupacS = iupac.str();

    CSamMdFormatter md( hit->GetTarget( pairmate ), iupacS.c_str(), hit->GetTranscript( pairmate ) );

    m_out 
        << query->GetId() << "\t"
        << flags << "\t"
        << GetSubjectId( hit->GetSeqOrd() ) << "\t"
        << from1 << "\t"
        << 100/rankSize << "\t" // TODO: refine this formula: int( -10 * log( (kHundredPct - hit->GetScore(pairmate))/kHundredPct ) ) << "\t"
        << hit->GetTranscript( pairmate ).ToString() << "\t"
        << "=\t" // mate reference sequence name is always the same in oligoFAR
        << from2 << "\t"
        << insert << "\t"
        << iupacS << "\t"
        << "*\t"; // TODO: replace with sequence and quality data
    if( md.Format() ) m_out << "MD:Z:" << md.GetString() << "\t";
    // TODO: CS tag generation
    // TODO: IH tag generation
    // TODO: NM tag generation
    m_out 
		<< "AS:i:" << int( 0.5 + hit->GetScore( pairmate ) ) << "\t"
        << "XR:i:" << rank;
    if( rankSize != kUnlimitedRankSize ) m_out << "\t" << "XN:i:" << rankSize;
    m_out << "\n";
}
// END
