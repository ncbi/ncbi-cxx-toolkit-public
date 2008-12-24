#include <ncbi_pch.hpp>
#include "csamformatter.hpp"
#include "cguidefile.hpp"
#include "ialigner.hpp"
#include "chit.hpp"
#include <cmath>

USING_OLIGOFAR_SCOPES;

const double kHundredPct = 100.000000001;
const int kUnlimitedRankSize = 1000000;
enum EFlags {
    fFlag_readIsPaired = 0x01,
    fFlag_hitIsPaired = 0x02,
    fFlag_readIsUnmapped = 0x04,
    fFlag_mateIsUnmapped = 0x08,
    fFlag_readIsReversed = 0x10,
    fFlag_mateIsReversed = 0x20,
    fFlag_readIsTheFirst = 0x40,
    fFlag_mateIsTheFirst = 0x80,
    fFlag_hitIsNotTheBest = 0x100
};

void CSamFormatter::FormatHeader( const string& version )
{
    m_out 
        << "@HD\tVN:1.0\n" //\tSO:queryname\n" // it is not sort order, it is group order
        << "@PG\tID:oligoFAR\tVN:" << version << "\n"; 
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
    delete query;
}

void CSamFormatter::FormatHit( int rank, int rankSize, const CHit* hit, int pairmate )
{
    if( ( hit->GetComponentFlags() & (CHit::fComponent_1 << pairmate) ) == 0 ) return; // don't output unmapped pair mates
    const CQuery * query = hit->GetQuery();
    int flags = 0;
    if( hit->IsReverseStrand( pairmate ) ) flags |= fFlag_readIsReversed;
    if( rank ) flags |= fFlag_hitIsNotTheBest;
    if( query->GetLength( !pairmate ) ) {
        flags |= fFlag_readIsPaired;
        flags |= pairmate ? fFlag_mateIsTheFirst : fFlag_readIsTheFirst;
        if( hit->GetComponentFlags() == 3 ) {
            flags |= fFlag_hitIsPaired;
            if( hit->IsReverseStrand( !pairmate ) ) 
                flags |= fFlag_mateIsReversed;
        } else {
            flags |= fFlag_mateIsUnmapped;
        }
    }


    int from1 = min( hit->GetFrom(pairmate), hit->GetTo(pairmate) ) + 1;
    int from2 = 0;
    if( hit->GetLength( !pairmate ) ) 
        from2 = min( hit->GetFrom(pairmate), hit->GetTo(pairmate) ) + 1;

    string cigar = "=";
    if( hit->GetScore(pairmate) < 100 ) {
        cigar = "";
        const string& target = hit->GetTarget( pairmate );
        const char * seq = target.data();
        int length = target.length();
        const int aflags = CAlignerBase::fComputePicture | CAlignerBase::fComputeScore | CAlignerBase::fPictureSubjectStrand;
        m_aligner->SelectBasicScoreTables( hit->GetConvTbl( pairmate ) ); 
        m_aligner->SetBestPossibleQueryScore( query->GetBestScore( pairmate ) );
        if( hit->IsReverseStrand( pairmate ) == false ) {
            m_aligner->Align( query->GetCoding(),
                              query->GetData( pairmate ),
                              query->GetLength( pairmate ),
                              CSeqCoding::eCoding_ncbi8na,
                              seq, length, aflags );
                              
        } else {
            m_aligner->Align( query->GetCoding(),
                              query->GetData( pairmate ),
                              query->GetLength( pairmate ),
                              CSeqCoding::eCoding_ncbi8na,
                              seq + length - 1, 0 - length, aflags );
        }

        const CAlignerBase& abase = m_aligner->GetAlignerBase();
        const string& q = abase.GetQueryString();
        const string& s = abase.GetSubjectString();
        const string& a = abase.GetAlignmentString();
        unsigned l = 0, r = a.length() - 1;
        for( ; l < a.length(); ++l ) if( a[l] != ' ' ) break;
        for( ; r >= l; --r ) if( a[r] != ' ' ) break;
        if( l ) cigar += NStr::IntToString( l ) + "S";
        int n = 0;
        for( unsigned x = l; x <= r ; ++x ) {
            if( q[x] == '-' ) {
                int d = 1;
                while( x < r ) if( q[x + 1] == '-' ) { d++, x++; } else break;
                if( n ) cigar += NStr::IntToString( n ) + "M";
                cigar += NStr::IntToString( d ) + "D";
                n = 0;
            } else if( s[x] == '-' ) {
                int i = 1;
                while( x < r ) if( s[x + 1] == '-' ) { i++, x++; } else break;
                if( n ) cigar += NStr::IntToString( n ) + "M";
                cigar += NStr::IntToString( i ) + "I";
                n = 0;
            } else ++n;
        }
        if( n ) cigar += NStr::IntToString( n ) + "M";
        if( r < a.length() - 1 ) cigar += NStr::IntToString( a.length() - r - 1 ) + "S";
    }

    m_out 
        << query->GetId() << "\t"
        << flags << "\t"
        << GetSubjectId( hit->GetSeqOrd() ) << "\t"
        << from1 << "\t"
        << int( -10 * log( (kHundredPct - hit->GetScore(pairmate))/kHundredPct ) ) << "\t"
        << cigar << "\t"
        << "=\t" // mate reference sequence name is always the same in oligoFAR
        << from2 << "\t"
        << ( from2 ? hit->GetFrom( !pairmate ) - hit->GetFrom( pairmate ) : 0 ) << "\t"
        << "*\t*\t";

    m_out 
		<< "AS:i:" << int( 0.5 + hit->GetScore( pairmate ) ) << "\t"
        << "XR:i:" << rank << "\t" << "XN:i:" << rankSize << "\n";
}
