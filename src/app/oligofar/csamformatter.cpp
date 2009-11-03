#include <ncbi_pch.hpp>
#include "oligofar-version.hpp"
#include "csamformatter.hpp"
#include "cguidefile.hpp"
#include "ialigner.hpp"
#include "chit.hpp"
#include <cmath>

USING_OLIGOFAR_SCOPES;

const double kHundredPct = 100.000000001;
const int kUnlimitedRankSize = 1000000;
// enum EFlags {
//     fFlag_readIsPaired = 0x01,
//     fFlag_hitIsPaired = 0x02,
//     fFlag_readIsUnmapped = 0x04,
//     fFlag_mateIsUnmapped = 0x08,
//     fFlag_readIsReversed = 0x10,
//     fFlag_mateIsReversed = 0x20,
//     fFlag_readIsTheFirst = 0x40,
//     fFlag_mateIsTheFirst = 0x80,
//     fFlag_hitIsNotTheBest = 0x100,
//     fFlag_readFailsPlatformChecks = 0x200,
//     fFlag_readIsPcrOrOpticalDupe = 0x400
// };

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
                if( hit->IsNull() ) { 
                    rankedHits.back().first = true; 
                    break;
                }
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
    if( hit->IsReverseStrand( pairmate ) ) flags |= fSeqReverse;
    if( rank ) flags |= fHitSuboptimal;
    if( query->GetLength( !pairmate ) ) {
        flags |= fPairedQuery;
        flags |= pairmate ? fSeqIsSecond : fSeqIsFirst;
        if( hit->GetComponentFlags() == 3 ) {
            flags |= fPairedHit;
            if( hit->IsReverseStrand( !pairmate ) ) 
                flags |= fMateReverse;
        } else {
            flags |= fMateUnmapped;
        }
    }

    int from1 = min( hit->GetFrom(pairmate), hit->GetTo(pairmate) ) + 1;
    int from2 = 0;
    if( hit->GetLength( !pairmate ) ) 
        from2 = min( hit->GetFrom(pairmate), hit->GetTo(pairmate) ) + 1;

    ostringstream iupac;
    query->PrintIupac( iupac, pairmate, hit->GetStrand( pairmate ) );

    m_out 
        << query->GetId() << "\t"
        << flags << "\t"
        << GetSubjectId( hit->GetSeqOrd() ) << "\t"
        << from1 << "\t"
        << 255 << "\t" // TODO: refine this formula: int( -10 * log( (kHundredPct - hit->GetScore(pairmate))/kHundredPct ) ) << "\t"
        << hit->GetTranscript( pairmate ).ToString() << "\t"
        << "=\t" // mate reference sequence name is always the same in oligoFAR
        << from2 << "\t"
        << ( from2 ? hit->GetFrom( !pairmate ) - hit->GetFrom( pairmate ) : 0 ) << "\t"
        << iupac.str() << "\t"
        << "*\t"; // TODO: replace with sequence and quality data
    // TODO: MD tag generation
    // TODO: CS tag generation
    // TODO: IH tag generation
    // TODO: NM tag generation
    m_out 
		<< "AS:i:" << int( 0.5 + hit->GetScore( pairmate ) ) << "\t"
        << "XR:i:" << rank << "\t" << "XN:i:" << rankSize << "\n";
}
