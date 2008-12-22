#include <ncbi_pch.hpp>
#include "csamformatter.hpp"
#include "cguidefile.hpp"
#include "ialigner.hpp"
#include "chit.hpp"

USING_OLIGOFAR_SCOPES;

void CSamFormatter::FormatHeader( const string& version )
{
    m_out << "@HD\tVN:1.0\tCR:oligoFAR/" << version << "\n"; 
}


void CSamFormatter::FormatHit( const CHit* ) {} // no any hit formatting is supported

void CSamFormatter::FormatQueryHits( const CQuery* query )
{
    if( query == 0 ) return;
    if( const CHit * hit = query->GetTopHit() ) {
        typedef list<const CHit*> THitList;
        typedef vector<pair<bool, THitList> > TRankedHits;
        TRankedHits rankedHits;
        for( ; hit; ) {
            if( hit->IsNull() ) { 
                rankedHits.back().second.first = true; 
                break;
            }
            rankedHits.push_back( make_pair( false, THitList ) );
            rankedHits.back().second.push_back( hit );
            CHit * next = hit->GetNextHit();
            for( ; next && (!next->IsNull()) && next->GetTotalScore() == hit->GetTotalScore(); next = next->GetNextHit() )
                rankedHits.back.second.push_back( next );
        }
        int rank = 0;
        ITERATE( TRankedHits, hitsRank, rankedHits ) {
            int rankSize = hitsRank->first ? hitsRank->second.size() : 1000000;
            ITERATE( THitList, h, hitsRank->second ) {
                FormatHit( rank, rankSize, (*h), 0 );
                FormatHit( rank, rankSize, (*h), 1 );
            }
            ++rank;
        }
    } else {
        // format empty hit here
    }
    delete query;
}

void CSamFormatter::FormatHit( int rank, int rankSize, const CHit* hit, int pairmate )
{
    if( hit->GetComponentFlags() & (CHit::fComponent_1 << pairmate) == 0 ) return;
    int flags = 0;
    if( query->GetLength( !pairmate ) ) {
        flags |= 0x01;
        if( !hit->GetComponentFlags() != 3 ) flags |= 0x20; 
    }
    if( hit->GetComponentFlags() == 3 ) flags |= 0x02;
    if( rank ) flags |= 0x08;
    if( query->GetCoding() == CSeqCoding::eCoding_colorsp ) flags |= 0x10;

    m_out 
        << GetSubjectId( hit->GetSeqOrd() ) << "\t"
        << (min( hit->GetFrom(pairmate), hit->GetTo(pairmate) ) + 1) << "\t"
        << (hit->GetFrom(pairmate) < hit->GetTo(pairmate) ? '+':'-') << "\t"
        << flags << "\t"
        << "mapQ\t"
        << "CIGAR\t"
        << "reference\t"
        << "qual\t"
        << "tags\n";
}

