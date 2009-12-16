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

namespace {
class CMdFormatter
{
public:
    // queryS is query on the subject strand
    CMdFormatter( const char * target, const char * queryS, const TTrSequence& cigar ) :
        m_target( target ), m_query( queryS ), m_cigar( cigar ), m_keepCount(0) {}
    const string& GetString() const { return m_data; }
    bool Format();
    bool Ok() const { return m_data.length(); }
protected:
    void FormatAligned( const char * s, const char * t, int cnt );
    void FormatInsertion( const char * s, int cnt );
    void FormatDeletion( const char * t, int cnt );
    void FormatTerminal();
protected:
    string m_data;
    const char * m_target;
    const char * m_query;
    const TTrSequence& m_cigar;
    int m_keepCount;
};

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
            if( mask == 3 ) break; // If we have paired reads - ignore unpaired
        } else {
            // format empty hit here
        }
    }
    delete query;
}

bool CMdFormatter::Format()
{
    if( m_target == 0 || m_target[0] == 0 || m_target[1] == 0 ) return false; // tgt == "\xf" is possible

    const char * s = m_query;
    const char * t = m_target;
    const TTrSequence& tr = m_cigar;
    TTrSequence::const_iterator x = tr.begin();
    if( x != tr.end() ) {
        switch( x->GetEvent() ) {
        case CTrBase::eEvent_SoftMask: s += x->GetCount();
        case CTrBase::eEvent_HardMask: ++x; 
        default: break;
        }
    }
    for( ; x != tr.end(); ++x ) {
        switch( x->GetEvent() ) {
        case CTrBase::eEvent_Replaced:
        case CTrBase::eEvent_Changed:
        case CTrBase::eEvent_Match: 
            FormatAligned( s, t, x->GetCount() ); 
            s += x->GetCount(); 
            t += x->GetCount();
        case CTrBase::eEvent_SoftMask:
        case CTrBase::eEvent_HardMask:
        case CTrBase::eEvent_Padding: continue;
        default: break;
        case CTrBase::eEvent_Splice:
        case CTrBase::eEvent_Overlap: m_data.clear(); return false; // MD can't be produced
        }
        // Here we have I or D
        switch( x->GetEvent() ) {
            case CTrBase::eEvent_Insertion:
                FormatInsertion( s, x->GetCount() );
                s += x->GetCount();
                break;
            case CTrBase::eEvent_Deletion: 
                FormatDeletion( t, x->GetCount() );
                t += x->GetCount();
                break;
            default: THROW( logic_error, "Unexpected event " << x->GetEvent() << " @ " << __FILE__ << ":" << __LINE__ );
        }
    }
    FormatTerminal();
    return m_data.length() > 0;
}

void CMdFormatter::FormatAligned( const char * s, const char * t, int count )
{
    while( count-- ) {
        char query = *s++;
        char subj = CIupacnaBase( CNcbi8naBase( t++ ) );
        if( query == subj ) { ++m_keepCount; }
        else {
            FormatTerminal();
            switch( subj ) {
            case 'A': case 'C': case 'G': case 'T': m_data += subj; break;
            default: m_data += 'N';
            }
        }
    }
}

void CMdFormatter::FormatTerminal()
{
    if( m_keepCount ) { m_data += NStr::IntToString( m_keepCount ); m_keepCount = 0; }
}

void CMdFormatter::FormatInsertion( const char * s, int count )
{
    // Nothing. At all.
}

void CMdFormatter::FormatDeletion( const char * t, int count ) 
{
    FormatTerminal();
    m_data += "^";
    while( count-- ) {
        char ref = CIupacnaBase( CNcbi8naBase( t++ ) );
        switch( ref ) {
        case 'A': case 'C': case 'G': case 'T': m_data += ref; break;
        default: m_data += 'N';
        }
    }
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
    int insert = 0;
    if( hit->GetLength( !pairmate ) )  {
        from2 = min( hit->GetFrom(!pairmate), hit->GetTo(!pairmate) ) + 1;
        insert = hit->GetFrom( !pairmate ) - hit->GetFrom( pairmate );
    }

    ostringstream iupac;
    query->PrintIupac( iupac, pairmate, hit->GetStrand( pairmate ) );
    string iupacS = iupac.str();

    CMdFormatter md( hit->GetTarget( pairmate ), iupacS.c_str(), hit->GetTranscript( pairmate ) );

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
    // TODO: MD tag generation
    // TODO: CS tag generation
    // TODO: IH tag generation
    // TODO: NM tag generation
    m_out 
		<< "AS:i:" << int( 0.5 + hit->GetScore( pairmate ) ) << "\t"
        << "XR:i:" << rank << "\t" << "XN:i:" << rankSize << "\n";
}
}
