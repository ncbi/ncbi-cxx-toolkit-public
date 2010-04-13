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


// If non-paired read - format either top hits, or empty (if required)
// If paired read:
// 1. if paired hits are present - format top paired hits
// 2. if there are no paired hits or if there were few of them and no "paired-only" flag is set, 
//    for each of components:
//    if hits are present, format up to xxx hits; with or without flag "the other read is mapped"
//    else format "empty hit" if necessary

void CSamFormatter::FormatQueryHits( const CQuery* query )
{
    if( query == 0 ) return;
    int maxToReport = m_topCount;
    int flags = 0;
    if( query->IsPairedRead() ) {
        flags |= fPairedQuery;
        int addFlags = 0;
        if( const CHit * hit = query->GetTopHit( 3 ) ) {
            maxToReport -= ReportHits( 3, query, hit, flags | fPairedHit, maxToReport );
            if( m_flags & fReportPairesOnly ) maxToReport = 0;
        } else addFlags |= fMateUnmapped;
        if( maxToReport > 0 ) {
            ReportHits( 1, query, query->GetTopHit( 1 ), ( query->GetTopHit( 2 ) ? 0 : addFlags ) | flags, maxToReport );
            ReportHits( 2, query, query->GetTopHit( 2 ), ( query->GetTopHit( 1 ) ? 0 : addFlags ) | flags, maxToReport );
        }
    } else {
        ReportHits( 1, query, query->GetTopHit( 1 ), 0, maxToReport );
        ReportHits( 2, query, query->GetTopHit( 2 ), 0, maxToReport );
    }
    delete query;
}

int CSamFormatter::ReportHits( int mask, const CQuery * query, const CHit * hit, int flags, int maxToReport ) // returns how many were reported
{
    if( hit == 0 ) {
        ASSERT( mask == 1 || mask == 2 );
        if( m_flags & fReportUnmapped ) flags |= fSeqUnmapped;
        else return 0;
        if( mask == 1 ) flags |= fSeqIsFirst; else flags |= fSeqIsSecond;
        ostringstream iupac;
        query->PrintIupac( iupac, mask == 1 ? 0 : 1, CSeqCoding::eStrand_pos );
        m_out 
            << query->GetId() << "\t"
            << flags << "\t" 
            << "*\t" // subjectId
            << "0\t" // pos
            << "0\t" // mapq
            << "*\t" // cigar
            << "*\t" // mrnm
            << "0\t" // mpos
            << "0\t" // isize
            << iupac << "\t"
            << "*\n"; // qual
        return 0;
    } else {
        typedef list<const CHit*> THitList;
        typedef vector<pair<bool, THitList> > TRankedHits;
        TRankedHits rankedHits;
        for( ; hit; ) {
            if( rankedHits.size() ) 
                rankedHits.back().first = true; // previous rank hit count is known
            if( hit->IsNull() ) break;
            rankedHits.push_back( make_pair( false, THitList() ) );
            rankedHits.back().second.push_back( hit );
            CHit * next = hit->GetNextHit();
            for( ; next && (!next->IsNull()) && next->GetTotalScore() == hit->GetTotalScore(); next = next->GetNextHit() )
                rankedHits.back().second.push_back( next );
            hit = next;
        }
        int rank = 0;
        int reported = 0;
        ITERATE( TRankedHits, hitsRank, rankedHits ) {
            int rankSize = hitsRank->first ? hitsRank->second.size() : kUnlimitedRankSize;
            ITERATE( THitList, h, hitsRank->second ) {
                FormatHit( rank, rankSize, (*h), 0, flags );
                FormatHit( rank, rankSize, (*h), 1, flags );
                ++reported;
            }
            ++rank;
        }
        return reported;
    }
}

void CSamFormatter::FormatHit( int rank, int rankSize, const CHit* hit, int pairmate, int flags )
{
    if( !m_headerPrinted ) {
        FormatHeader( OLIGOFAR_VERSION );
        m_headerPrinted = true;
    }
    if( ( hit->GetComponentFlags() & (CHit::fComponent_1 << pairmate) ) == 0 ) return; // don't output unmapped pair mates
    if( hit->GetCachedSam( pairmate )[0] ) {
        const char * c = hit->GetCachedSam( pairmate );
        const char * e = c + strlen(c);
        while( c < e && strchr( "\t\n\r", e[-1] ) ) --e;
        m_out.write( c, e-c );
        m_out << "\tXS:Z:guide\n";
        return;
    }

    const CQuery * query = hit->GetQuery();
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
    if( query->GetCoding() != CSeqCoding::eCoding_colorsp ) {
        query->PrintIupac( iupac, pairmate, hit->GetStrand( pairmate ) );
    } else {
        PrintCorrectedIupac( iupac, query->GetData( pairmate ), query->GetLength( pairmate ), hit->GetTranscript( pairmate ), hit->GetTarget( pairmate ), hit->GetStrand( pairmate ) );
    }
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
    if( query->GetCoding() == CSeqCoding::eCoding_colorsp ) {
        m_out << "CS:Z:";
        const char * c = query->GetData( pairmate ) - 1;
        int l =  query->GetDataSize( pairmate );
        for( int i = 0; i <= l; ++i ) { // yes, <=
            char k = c[i];
            if( i ) k = CColorTwoBase( k ).GetColor(); 
            m_out << CIupacnaBase( CColorTwoBase( k ) );
        }
        m_out << "\t";
    }
    // TODO: IH tag generation
    // TODO: NM tag generation
    m_out 
		<< "AS:i:" << int( 0.5 + hit->GetScore( pairmate ) ) << "\t"
        << "XR:i:" << rank;
    if( rankSize != kUnlimitedRankSize ) m_out << "\t" << "XN:i:" << rankSize;
    m_out << "\n";
}

//////////////////////////////////////////////////////////////////////////////
// Corrects errors in colorspace read to match subject according alignment
// Useful for MD:Z: generation
void CSamFormatter::PrintCorrectedIupac( ostream& o, const char * seqdata, int seqlength, const TTrSequence& tr, const char * target, CSeqCoding::EStrand strand )
{
    int inc = 1;
    const char * a = seqdata; const char * A = seqdata + seqlength - 1;
    CIupacnaBase status = CIupacnaBase( CColorTwoBase( seqdata ) );
    if( strand == CSeqCoding::eStrand_neg ) {
        for( int i = 1; i < seqlength; ++i )
            status = CIupacnaBase( status, CColorTwoBase( seqdata[i] ) );
        inc = -1;
        seqdata += seqlength - 1;
    }
    TTrSequence::const_iterator t = tr.begin();
    int count = t->GetCount();
    int i = 0;
    try {
        for( i = 0; i < seqlength; ) {
            switch( t->GetEvent() ) {
            case CTrBase::eEvent_Match: 
                o << CIupacnaBase( CNcbi8naBase( *target ) ); 
                if( ++i < seqlength) status = CIupacnaBase( CNcbi8naBase( *target ), CColorTwoBase( seqdata += inc ) ); 
                --count; ++target;  // ??? not sure if it should not go before if( ++i .. ) line
                break;
            case CTrBase::eEvent_SoftMask: 
                --count;
                if( ++i < seqlength) status = CIupacnaBase( status, CColorTwoBase( seqdata += inc ) ); 
                break;
            case CTrBase::eEvent_Replaced:
            case CTrBase::eEvent_Changed:
                o << (char)tolower( status );
                --count; ++target; 
                if( ++i < seqlength) status = CIupacnaBase( status, CColorTwoBase( seqdata += inc ) ); 
                break;
            case CTrBase::eEvent_Insertion:
            case CTrBase::eEvent_Overlap:
                o << (char)tolower( status );
                --count;  
                if( ++i < seqlength) status = CIupacnaBase( status, CColorTwoBase( seqdata += inc ) ); 
                break;
            case CTrBase::eEvent_Deletion:
            case CTrBase::eEvent_Splice:
                --count; ++target; 
                break;
            case CTrBase::eEvent_Padding: --count; break;
            default: THROW( logic_error, "Unexpected event " << CTrBase::Event2Char( t->GetEvent() ) << " in colorspace alignment with CIGAR " << tr.ToString() );
            }
            if( count == 0 ) if( ++t != tr.end() ) count = t->GetCount();
        }
    } catch( exception& e ) {
        cerr << "\n" << CTrBase::Event2Char( t->GetEvent() ) << t->GetCount() << DISPLAY( count ) << DISPLAY( tr.ToString() ) << DISPLAY( strand ) << "\n";
        cerr << DISPLAY( seqdata - a ) << DISPLAY( A - a ) << DISPLAY( inc ) << "\n";
        cerr << DISPLAY( CIupacnaBase( CNcbi8naBase( *target ) ) ) << DISPLAY( CIupacnaBase( status ) ) << "\n";
        throw;
    }
}

// END
