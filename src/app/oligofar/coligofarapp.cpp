#include <ncbi_pch.hpp>
#include "coligofarapp.hpp"
#include "csamformatter.hpp"
#include "coutputformatter.hpp"
#include "cprogressindicator.hpp"
#include "cscoringfactory.hpp"
#include "cbitmaskaccess.hpp"
#include "cseqscanner.hpp"
#include "cguidesam.hpp"
#include "cfeatmap.hpp"
#include "caligner.hpp"
#include "cfilter.hpp"
#include "cbatch.hpp"
#include "csnpdb.hpp"
#include "oligofar-version.hpp"
#include "coligofarcfg.hpp"

#include "string-util.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#ifndef _WIN32
#include <sys/resource.h>
#include <sysexits.h>
#else
#define strtoll( a, b, c ) strtoui64( a, b, c )
#define EX_USAGE 64
#endif

USING_OLIGOFAR_SCOPES;

COligoFarApp::COligoFarApp( int argc, char ** argv ) :
    CApp( argc, argv ),
    m_hashPass( 0 ),
    m_strands( 0x03 ),
    m_readsPerRun( 250000 ),
    m_minRun(0),
    m_maxRun( numeric_limits<int>::max() ),
    m_topCnt( 10 ),
    m_topPct( 99 ),
    m_minPctid( 60 ),
    m_identityScore(  1.0 ),
    m_mismatchScore( -1.0 ),
    m_gapOpeningScore( -3.0 ),
    m_gapExtentionScore( -1.5 ),
    m_extentionPenaltyDropoff( -100 ),
    m_qualityChannels( 0 ),
    m_qualityBase( 33 ),
    m_minBlockLength( 1000 ),
//    m_guideFilemaxMismatch( 0 ),
    m_memoryLimit( Uint8( sizeof(void*) == 4 ? 3 : 8 ) * int(kGigaByte) ),
    m_performTests( false ),
    m_colorSpace( false ),
    m_sodiumBisulfiteCuration( false ),
    m_outputSam( true ),
    m_printStatistics( false ),
    m_fastaParseIDs( false ),
#ifdef _WIN32
    //m_guideFile( "nul:" ),
    m_outputFile( "con:" ),
#else
    m_guideFile( "/dev/null" ),
    m_outputFile( "/dev/stdout" ),
#endif
    m_outputFlags( "z" ),
    m_geometry( "p" )
{
    m_passParam.push_back( CPassParam() );
#ifndef _WIN32
    ifstream meminfo( "/proc/meminfo" );
    string buff;
    if( !meminfo.fail() ) {
        m_memoryLimit = 0;

        while( getline( meminfo, buff ) ) {
            istringstream line(buff);
            string name, units;
            Uint8 value;
            line >> name >> value >> units;
            if( units.length() ) {
                switch( tolower( units[0] ) ) {
                case 'g': value *= kKiloByte;
                case 'm': value *= kKiloByte;
                case 'k': value *= kKiloByte;
                }
            }
            if( name == "MemFree:" || name == "Buffers:" || name == "Cached:" || name == "SwapCached:" ) {
                m_memoryLimit += value;
            }
        }
    }
#endif
    InitArgs();
}

int COligoFarApp::RevNo()
{
    return strtol( "$Rev$"+6, 0, 10 );
}

void COligoFarApp::Version( const char * )
{
    cout << GetProgramBasename() << " ver " OLIGOFAR_VERSION " (Rev:" << RevNo() << ") " NCBI_SIGNATURE << endl;
}

// letter   lower       letter  upper
// =============================================
// a        ambCnt      A       ambCnt
// b        snpdb       B       batchSz
// c        colorsp     C       /config/
// d        database    D       pairRange
// e        maxIndelW   E       masHashDist*
// f        windowStep  F       dustScore
// g        guidefile   G       gapScore
// h        help        H       hashBits
// i        input       I       idScore
// j        maxInsWid*  J       maxDelWid*   
// k        skipPos     K       indelPos*
// l        gilist      L       memLimit
// m        margin      M       mismScore
// n        maxMism     N       maxWindows
// o        output      O       outputFmt
// p        pctCutoff   P       phrapScore
// q        qualChn     Q       gapExtScore
// r        windowStart R       geometry
// s        strands     S       stride
// t        topPct      T       runTests
// u        topCnt      U       assertVer
// v        featfile    V       version
// w        win/word    W       POSIX 
// x        extDropOff  X       xDropoff
// y        onlySeqid   Y       bandHalfWidth
// z                    Z
// 0        baseQual    5
// 1        solexa1     6
// 2        solexa2     7
// 3                    8
// 4                    9

void COligoFarApp::Help( const char * arg )
{
    enum EFlags {
        fSynopsis = 0x01,
        fDetails  = 0x02,
        fExtended = 0x04
    };
    int flags = fSynopsis;
    if( arg ) {
        switch( *arg ) {
        case 'b': case 'B': break;
        case 'e': case 'E': flags = fExtended; break;
        case 'f': case 'F': flags = fDetails; break;
        }
    }
    if( flags & fSynopsis ) 
        PrintBriefHelp( cout );
    if( flags & fDetails ) 
        PrintFullHelp( cout );
}

void COligoFarApp::PrintArgValue( int opt, ostream& out, int pass ) 
{
    switch( opt ) {
        case 'i': out << m_readFile; break;
        case 'd': out << m_fastaFile; break;
        case 'b': out << m_snpdbFile; break;
        case 'g': out << m_guideFile; break;
        case 'v': out << m_featFile; break;
        case 'l': out << m_gilistFile; break;
        case '1': out << m_read1qualityFile; break;
        case '2': out << m_read2qualityFile; break;
        case 'o': out << m_outputFile; break;
        case 'y': out << Join( ", ", m_seqIds ); break;
        case 'c': out << (m_colorSpace?"yes":"no"); break;
        case 'q': out << m_qualityChannels; break;
        //case '0': out << "ASCII(" << m_qualityBase << ")"; if( isprint( m_qualityBase ) ) out << " or +" << char( m_qualityBase ); break;
        case '0': if( isprint( m_qualityBase ) ) out << "+" << char( m_qualityBase ); else out << m_qualityBase; break;
        case 'O': out << m_outputFlags; break;
        case 'B': out << m_readsPerRun; break;
        case kLongOpt_batchRange: out << m_minRun << "-" << m_maxRun; break;
        case kLongOpt_NaHSO3: out << (m_sodiumBisulfiteCuration?"yes":"no"); break;
        case 's': out << m_strands; break;
        case kLongOpt_hashBitMask: out << m_hashBitMask; break;
        case 'w': out << m_passParam[pass].GetHashParam().GetWindowSize() << "/" << m_passParam[pass].GetHashParam().GetWordSize(); break;
        case 'k': copy( m_passParam[pass].GetHashParam().GetSkipPositions().begin(), 
                        m_passParam[pass].GetHashParam().GetSkipPositions().end(), 
                        ostream_iterator<int>( out, "," ) ); break;
        case 'f': out << m_passParam[pass].GetHashParam().GetWindowStep(); break;
        case 'r': out << m_passParam[pass].GetHashParam().GetWindowStart(); break;
        case 'S': out << m_passParam[pass].GetHashParam().GetStrideSize(); break;
        case 'H': out << m_passParam[pass].GetHashParam().GetHashBits(); break;
        case 'N': out << m_passParam[pass].GetHashParam().GetWindowCount(); break;
        case 'a': out << m_passParam[pass].GetHashParam().GetHashMaxAmb(); break;
        case 'A': out << m_passParam[pass].GetMaxSubjAmb(); break;
        case 'P': out << m_passParam[pass].GetPhrapCutoff(); break;
        case 'F': out << m_passParam[pass].GetHashParam().GetMaxSimplicity(); break;
        case 'n': out << m_passParam[pass].GetHashParam().GetHashMismatches(); break;
        case 'e': out << max( m_passParam[pass].GetHashParam().GetHashInsertions(), m_passParam[pass].GetHashParam().GetHashDeletions() ); break;
        case 'j': out << m_passParam[pass].GetHashParam().GetHashInsertions(); break;
        case 'J': out << m_passParam[pass].GetHashParam().GetHashDeletions(); break;
        case 'E': out << m_passParam[pass].GetHashParam().GetHashMaxDistance(); break;
        case 'K': out << m_passParam[pass].GetHashParam().GetHashIndelPosition(); break;
        case 'X': out << m_passParam[pass].GetAlignParam().GetMaxIndelLength(); break;
        case 'Y': out << m_passParam[pass].GetAlignParam().GetMaxIndelCount(); break;
        case kLongOpt_maxInsertion: out << m_passParam[pass].GetAlignParam().GetMaxInsertionLength(); break;
        case kLongOpt_maxDeletion: out << m_passParam[pass].GetAlignParam().GetMaxDeletionLength(); break;
        case kLongOpt_maxInsertions: out << m_passParam[pass].GetAlignParam().GetMaxInsertionsCount(); break;
        case kLongOpt_maxDeletions: out << m_passParam[pass].GetAlignParam().GetMaxDeletionsCount(); break;
        case kLongOpt_addSplice: out << ReportSplices( pass ); break;
        case 'D': out << m_passParam[pass].GetMinPair() << "-" << m_passParam[pass].GetMaxPair(); break;
        case 'm': out << m_passParam[pass].GetPairMargin(); break;
        case 'I': out << m_identityScore; break;
        case 'M': out << m_mismatchScore; break;
        case 'G': out << m_gapOpeningScore; break;
        case 'Q': out << m_gapExtentionScore; break;
        case 'x': out << m_extentionPenaltyDropoff; break;
        case 'p': out << m_minPctid; break;
        case 'u': out << m_topCnt; break;
        case 't': out << m_topPct; break;
        case 'R': out << m_geometry; break;
        case 'h': break;
        case 'V': break;
        case 'U': out << OLIGOFAR_VERSION; break;
        case 'L': out << m_memoryLimit; break;
        case 'T': out << (m_performTests?"yes":"no"); break;
        case kLongOpt_min_block_length: out << m_minBlockLength; break;
        case kLongOpt_printStatistics: out << (m_printStatistics?"yes":"no"); break;
        case kLongOpt_fastaParseIDs: out << (m_fastaParseIDs?"yes":"no"); break;
    }
}
/*
            << "\nRelative orientation flags recognized:\n"
            << "     p|centripetal|inside|pcr|solexa       reads are oriented so that vectors 5'->3' pointing to each other\n"
            << "     f|centrifugal|outside                 reads are oriented so that vectors 5'->3' are pointing outside\n"
            << "     i|incr|incremental|solid              reads are on same strand, first preceeds second on this strand\n"
            << "     d|decr|decremental                    reads are on same strand, first succeeds second on this strand\n"
            << "\nOutput flags (for -O):\n"
            << "     -   reset all flags\n"
            << "     h   report all hits before ranking\n"
            << "     u   report unmapped reads\n"
            << "     x   indicate that there are more reads of this rank\n"
            << "     m   indicate that there are more reads of lower ranks\n"
            << "     t   indicate that there were no more hits\n"
            << "     d   report differences between query and subject\n"
            << "     e   print empty line after all hits of the read are reported\n"
            << "     r   print raw scores rather then relative scores\n"
            << "     z   output in SAM 1.2 format (other flags are unsupported in this format)\n"
            << "     Z   output in native format\n"
            << "     p   print unpaired hits only if there are no paired\n"
            << "Read file data options may be used only in combinations:\n"
            << "     1. with column file:\n"
            << "        -q0 -i input.col -c no \n"
            << "        -q1 -i input.col -c no \n"
            << "        -q0 -i input.col -c yes\n"
            << "     2. with fasta or fastq files:\n"
            << "        -q0 -1 reads1.fa  [-2 reads2.fa]  -c yes|no\n"
            << "        -q1 -1 reads1.faq [-2 reads2.faq] -c no\n"
            << "     3. with Solexa 4-channel data\n"
            << "        -q4 -i input.id -1 reads1.prb [-2 reads2.prb] -c no\n"
            << "\nNB: although -L flag is optional, it is strongly recommended to use it!\n"
            ;
    }
    if( flags & fExtended ) 
        cout << "\nExtended options:\n"
             << "   --min-block-length=bases   Length for subject sequence to be scanned at once [" << m_minBlockLength << "]\n"
             << "   --print-statistics         Print statistics upon completion (for debugging and profiling purposes) [" << (m_printStatistics?"on":"off") << "]\n"
             ;
}

struct option COligoFarApp::s_options[] = {
    {"help", 2, 0, 'h'},
    {"version", 0, 0, 'V'},
    {"config-file", 1, 0, 'C'},
    {"pass0", 0, 0, kLongOpt_pass0},
    {"pass1", 0, 0, kLongOpt_pass1},

    {"window-size", 1, 0, 'w'},
    {"window-skip",1,0,'k'},
    {"window-step",1,0,'f'},
    {"window-start",1,0,'r'},
    {"index-bits", 1, 0, 'H'},
    {"max-windows",1,0,'N'},
    {"max-mism", 1, 0, 'n'},
    {"max-indel", 1, 0, 'e'},
    {"max-ins", 1, 0, 'j'},
    {"max-del", 1, 0, 'J'},
    {"max-hash-dist", 1, 0, 'E'},
    {"input-max-amb", 1, 0, 'a'},
    {"fasta-max-amb", 1, 0, 'A'},
    {"phrap-score", 1, 0, 'P'},
    {"max-simplicity", 1, 0, 'F'},
    {"indel-pos", 1, 0, 'K'},
    {"indel-dropoff", 1, 0, 'X'},
    {"band-half-width", 1, 0, 'Y'},
    {"longest-ins", 1, 0, kLongOpt_maxInsertion},
    {"longest-del", 1, 0, kLongOpt_maxDeletion},
    {"max-inserted", 1, 0, kLongOpt_maxInsertions},
    {"max-deleted", 1, 0,  kLongOpt_maxDeletions},
    {"add-splice", 1, 0, kLongOpt_addSplice},
    {"pair-distance", 1, 0, 'D'},
    {"pair-margin", 1, 0, 'm'},

    {"assert-version", 1, 0, 'U'},
    {"colorspace", 1, 0, 'c'},
    {"NaHSO3", 1, 0, kLongOpt_NaHSO3},
    {"input-file", 1, 0, 'i'},
    {"fasta-file", 1, 0, 'd'},
    {"snpdb-file", 1, 0, 'b'},
    {"guide-file", 1, 0, 'g'},
    {"feat-file", 1, 0, 'v'},
    {"output-file", 1, 0, 'o'},
    {"output-flags", 1, 0, 'O'},
    {"only-seqid", 1, 0, 'y'},
    {"gi-list", 1, 0, 'l'},
    {"strands", 1, 0, 's'},
    {"hash-bitmap-file", 1, 0, kLongOpt_hashBitMask },
    {"batch-size", 1, 0, 'B'},
    {"batch-range", 1, 0, kLongOpt_batchRange },
//        {"guide-max-mism", 1, 0, 'x'},
    {"min-pctid", 1, 0, 'p'},
    {"top-count", 1, 0, 'u'},
    {"top-percent", 1, 0, 't'},
    {"read-1-file", 1, 0, '1'},
    {"read-2-file", 1, 0, '2'},
    {"quality-channels", 1, 0, 'q'},
    {"quality-base", 1, 0, '0'},
    {"geometry", 1, 0, 'R'},
    {"identity-score", 1, 0, 'I'},
    {"mismatch-score", 1, 0, 'M'},
    {"gap-opening-score", 1, 0, 'G'},
    {"gap-extention-score", 1, 0, 'Q'},
    {"extention-dropoff", 1, 0, 'x'},
    {"memory-limit", 1, 0, 'L'},
    {"test-suite", 1, 0, 'T'},
    {"min-block-length", 1, 0, kLongOpt_min_block_length },
    {"print-statistics", 0, 0, kLongOpt_printStatistics },
    {0,0,0,0}
};
*/
const option * COligoFarApp::GetLongOptions() const
{
    return this->COligofarCfg::GetLongOptions();
}

const char * COligoFarApp::GetOptString() const
{
    return this->COligofarCfg::GetOptString();
}

int COligoFarApp::ParseArg( int opt, const char * arg, int longindex )
{
    switch( opt ) {
    case kLongOpt_hashBitMask: m_hashBitMask = arg; break;
    case kLongOpt_min_block_length: m_minBlockLength = NStr::StringToInt( arg ); break;
    case kLongOpt_printStatistics: m_printStatistics = true; break;
    case kLongOpt_NaHSO3: m_sodiumBisulfiteCuration = *arg == '+' ? true : *arg == '-' ? false : NStr::StringToBool( arg ); break;
    case kLongOpt_pass0: m_hashPass = 0; break;
    case kLongOpt_pass1: if( m_passParam.size() < 2 ) m_passParam.push_back( m_passParam.back() ); m_hashPass = 1; break;
    case kLongOpt_maxInsertion: m_passParam[m_hashPass].SetAlignParam().SetMaxInsertionLength( abs( NStr::StringToInt( arg ) ) ); break;
    case kLongOpt_maxDeletion:  m_passParam[m_hashPass].SetAlignParam().SetMaxDeletionLength( abs( NStr::StringToInt( arg ) ) ); break;
    case kLongOpt_maxInsertions: m_passParam[m_hashPass].SetAlignParam().SetMaxInsertionCount( abs( NStr::StringToInt( arg ) ) ); break;
    case kLongOpt_maxDeletions:  m_passParam[m_hashPass].SetAlignParam().SetMaxDeletionCount( abs( NStr::StringToInt( arg ) ) ); break;
    case kLongOpt_addSplice: AddSplice( arg ); break;
    case kLongOpt_batchRange: ParseRange( m_minRun, m_maxRun, arg, "-" ); break;
    case kLongOpt_fastaParseIDs: m_fastaParseIDs = *arg == '+' ? true : *arg == '-' ? false : NStr::StringToBool( arg ); break;
    case 'U': if( strcmp( arg, OLIGOFAR_VERSION ) ) THROW( runtime_error, "Expected oligofar version " << arg << ", called " OLIGOFAR_VERSION ); break;
    case 'C': ParseConfig( arg ); break;
    case kLongOpt_writeConfig: WriteConfig( arg ); break;
    case 'k': 
        do {
            list<string> x;
            Split( arg, ",", back_inserter( x ) );
            CHashParam::TSkipPositions& sp = m_passParam[m_hashPass].SetHashParam().SetSkipPositions();
            ITERATE( list<string>, t, x ) sp.insert( sp.end(), NStr::StringToInt( *t ) );
        } while(0);
        break;
    case 'w': 
        do {
            int win, word;
            ParseRange( win, word, arg, "/" ); 
            m_passParam[m_hashPass].SetHashParam().SetWindowSize( win );
            m_passParam[m_hashPass].SetHashParam().SetWordSize( word );
        } while(0);
        break;
    case 'N': m_passParam[m_hashPass].SetHashParam().SetWindowCount( NStr::StringToInt( arg ) ); break;
    case 'f': m_passParam[m_hashPass].SetHashParam().SetWindowStep( abs( NStr::StringToInt( arg ) ) ); break;
    case 'r': m_passParam[m_hashPass].SetHashParam().SetWindowStart( abs( NStr::StringToInt( arg ) - 1 ) ); break;
    case 'H': m_passParam[m_hashPass].SetHashParam().SetHashBits( NStr::StringToInt( arg ) ); break;
    case 'S': m_passParam[m_hashPass].SetHashParam().SetStrideSize( NStr::StringToInt( arg ) ); break;
    case 'n': m_passParam[m_hashPass].SetHashParam().SetHashMismatches( NStr::StringToInt( arg ) ); break;
    case 'e': m_passParam[m_hashPass].SetHashParam().SetHashIndels( NStr::StringToInt( arg ) ); break;
    case 'j': m_passParam[m_hashPass].SetHashParam().SetHashInsertions( NStr::StringToInt( arg ) ); break;
    case 'J': m_passParam[m_hashPass].SetHashParam().SetHashDeletions( NStr::StringToInt( arg ) ); break;
    case 'E': m_passParam[m_hashPass].SetHashParam().SetMaxHashDistance( NStr::StringToInt( arg ) ); break;
    case 'K': m_passParam[m_hashPass].SetHashParam().SetHashIndelPosition( NStr::StringToInt( arg ) ); break;
    case 'a': m_passParam[m_hashPass].SetHashParam().SetHashMaxAmb( strtol( arg, 0, 10 ) ); break;
    case 'A': m_passParam[m_hashPass].SetMaxSubjAmb() = strtol( arg, 0, 10 ); break;
    case 'c': m_colorSpace = *arg == '+' ? true : *arg == '-' ? false : NStr::StringToBool( arg ); break;
    case 'i': m_readFile = arg; break;
    case 'd': m_fastaFile = arg; break;
    case 'b': m_snpdbFile = arg; break;
    case 'g': m_guideFile = arg; break;
    case 'v': m_featFile = arg; break;
    case 'o': m_outputFile = arg; break;
    case 'O': m_outputFlags += arg; if( const char * m = strrchr( m_outputFlags.c_str(), '-' ) ) m_outputFlags = m + 1; break;
    case 'l': m_gilistFile = arg; break;
    case 'y': m_seqIds.push_back( CSeq_id( arg ).AsFastaString() ); break;
    case 's': m_strands = strtol( arg, 0, 10 ); break;
    case 'B': m_readsPerRun = strtol( arg, 0, 10 ); break;
//    case 'x': m_guideFilemaxMismatch = strtol( arg, 0, 10 ); break;
    case 'p': m_minPctid = NStr::StringToDouble( arg ); break;
    case 'u': m_topCnt = strtol( arg, 0, 10 ); break;
    case 't': m_topPct = max( 0.0, min( 99.99999, NStr::StringToDouble( arg ) ) ); break;
    case '1': m_read1qualityFile = arg; break;
    case '2': m_read2qualityFile = arg; break;
    case 'q': m_qualityChannels = NStr::StringToInt( arg ); break;
    case '0': m_qualityBase = ( arg[0] && arg[0] == '+' ) ? arg[1] : NStr::StringToInt( arg ); break;
    case 'D': ParseRange( m_passParam[m_hashPass].SetMinPair(), m_passParam[m_hashPass].SetMaxPair(), arg ); break;
    case 'm': m_passParam[m_hashPass].SetPairMargin() = strtol( arg, 0, 10 ); break;
    case 'P': m_passParam[m_hashPass].SetPhrapCutoff() = strtol( arg, 0, 10 ); break;
    case 'R': m_geometry = arg; break;
    case 'F': m_passParam[m_hashPass].SetHashParam().SetMaxSimplicity( NStr::StringToDouble( arg ) ); break;
    case 'X': m_passParam[m_hashPass].SetAlignParam().SetMaxIndelLength( abs( strtol( arg, 0, 10 ) ) ); break;
    case 'Y': m_passParam[m_hashPass].SetAlignParam().SetMaxIndelCount( abs( strtol( arg, 0, 10 ) ) ); break;
    case 'I': m_identityScore = fabs( NStr::StringToDouble( arg ) ); break;
    case 'M': m_mismatchScore = -fabs( NStr::StringToDouble( arg ) ); break;
    case 'G': m_gapOpeningScore = -fabs( NStr::StringToDouble( arg ) ); break;
    case 'Q': m_gapExtentionScore = -fabs( NStr::StringToDouble( arg ) ); break;
    case 'x': m_extentionPenaltyDropoff = -fabs( NStr::StringToDouble( arg ) ); break; 
    case 'L':
#ifndef _WIN32
        do {
            char * t = 0;
            m_memoryLimit = strtoll( arg, &t, 10 );
            if( t ) {
                switch( tolower(*t) ) {
                case 'g': m_memoryLimit *= 1024;
                case 'm': m_memoryLimit *= 1024;
                case 'k': m_memoryLimit *= 1024;
                }
            }
        } while(0);
#else
        cerr << "[" << GetProgramBasename() << "] Warning: -L is ignored in win32\n";
#endif
        break;
    case 'T': m_performTests = (*arg == '+') ? true : (*arg == '-') ? false : NStr::StringToBool( arg ); break;
    default: return CApp::ParseArg( opt, arg, longindex );
    }
    return 0;
}

void COligoFarApp::WriteConfig( const char * arg )
{
    if( arg ) {
        ofstream out( arg );
        PrintConfig( out );
    }
    else PrintConfig( cout );
    m_done = true;
}

void COligoFarApp::ParseConfig( const string& cfg ) 
{
    ifstream in( cfg.c_str() );
    if( !in.good() ) THROW( runtime_error, "Failed to read config file " << cfg );
    CNcbiRegistry reg( in );
    ParseConfig( &reg );
}

bool COligoFarApp::HasConfigEntry( IRegistry * reg, const string& section, const string& name ) const
{
    return reg->HasEntry( section, name ) && reg->Get( section, name ).length();
}

void COligoFarApp::ParseConfig( IRegistry * reg )
{
    int hp = m_hashPass;
    ITERATE( TOptionList, opt, m_optionList ) {
        if( opt->GetSection() == eSection_ignore ) continue;
        if( opt->GetSection() == eSection_passopt ) {
            bool hasIt = false;
            if( HasConfigEntry( reg, "oligofar-pass0", opt->GetName() ) ) {
                m_hashPass = 0;
                ParseArg( opt->GetOpt(), reg->Get( "oligofar-pass0", opt->GetName() ).c_str(), -1 );
                hasIt = true;
            }
            if( HasConfigEntry( reg, "oligofar-pass1", opt->GetName() ) ) {
                if( m_passParam.size() < 2 ) m_passParam.push_back( m_passParam.back() ); m_hashPass = 1;
                ParseArg( opt->GetOpt(), reg->Get( "oligofar-pass1", opt->GetName() ).c_str(), -1 );
                hasIt = true;
            }
            if( (!hasIt) && HasConfigEntry( reg, "oligofar", opt->GetName() ) ) {
                m_hashPass = 0;
                ParseArg( opt->GetOpt(), reg->Get( "oligofar", opt->GetName() ).c_str(), -1 );
                if( m_passParam.size() > 1 ) { // affects both passes
                    m_hashPass = 1;
                    ParseArg( opt->GetOpt(), reg->Get( "oligofar", opt->GetName() ).c_str(), -1 );
                }
            }
        } else {
            if( HasConfigEntry( reg, "oligofar", opt->GetName() )  ) {
                ParseArg( opt->GetOpt(), reg->Get( "oligofar", opt->GetName() ).c_str(), -1 );
            }
        }
    }
    m_hashPass = hp; // restore command-line pass status
}

int COligoFarApp::RunTestSuite()
{
    if( m_performTests ) {
        if( int rc = TestSuite() ) {
            cerr << "[" << GetProgramBasename() << "] internal tests failed: " << rc << endl;
            return rc;
        } else {
            cerr << "[" << GetProgramBasename() << "] internal tests succeeded!\n";
        }
    }
    return 0;
}

int COligoFarApp::SetLimits()
{
#ifndef _WIN32
    if( m_memoryLimit ) {
        struct rlimit rl;
        rl.rlim_cur = m_memoryLimit;
        rl.rlim_max = RLIM_INFINITY;
        cerr << "[" << GetProgramBasename() << "] Setting memory limit to " << m_memoryLimit << ": ";
        errno = 0;
        int rc = setrlimit( RLIMIT_AS, &rl );
        cerr << "\b\b failed: " << strerror( errno ) << endl;
        return rc; 
    } else 
        cerr << "[" << GetProgramBasename() << "] Memory limit is not set.\n";
#else
    cerr << "[" << GetProgramBasename() << "] Setting memory limit is not implemented for win32 yet, ignored.\n";
#endif
    return 0;
}

int COligoFarApp::Execute()
{
    if( int rc = RunTestSuite() ) return rc;
    //if( int rc = SetLimits() ) return rc;
    SetLimits();
    return ProcessData();
}

int COligoFarApp::GetOutputFlags() 
{
    int oflags = 0;
    for( const char * f = m_outputFlags.c_str(); *f; ++f ) {
        switch( *f ) {
        case '-': oflags = 0; m_outputSam = false; break;
        case 'z': m_outputSam = true; break;
        case 'Z': m_outputSam = false; break;
        case 'e': oflags |= COutputFormatter::fReportEmptyLines; break;
        case 'u': oflags |= COutputFormatter::fReportUnmapped; break;
        case 'm': oflags |= COutputFormatter::fReportMany; break;
        case 'x': oflags |= COutputFormatter::fReportMore; break;
        case 't': oflags |= COutputFormatter::fReportTerminator; break;
        case 'd': oflags |= COutputFormatter::fReportDifferences; break;
        case 'h': oflags |= COutputFormatter::fReportAllHits; break;
        case 'r': oflags |= COutputFormatter::fReportRawScore; break;
        case 'p': oflags |= COutputFormatter::fReportPairesOnly; break;
        default: cerr << "[" << GetProgramBasename() << "] Warning: unknown format flag `" << *f << "'\n"; break;
        }
    }
    return oflags;
}

void COligoFarApp::SetupGeometries( map<string,int>& geometries )
{
    geometries.insert( make_pair("p",CFilter::eCentripetal) );
    geometries.insert( make_pair("centripetal",CFilter::eCentripetal) );
    geometries.insert( make_pair("inside",CFilter::eCentripetal) );
    geometries.insert( make_pair("pcr",CFilter::eCentripetal) );
    geometries.insert( make_pair("solexa",CFilter::eCentripetal) );
    geometries.insert( make_pair("f",CFilter::eCentrifugal) );
    geometries.insert( make_pair("centrifugal",CFilter::eCentrifugal) );
    geometries.insert( make_pair("outside",CFilter::eCentrifugal) );
    geometries.insert( make_pair("i",CFilter::eIncremental) );
    geometries.insert( make_pair("incr",CFilter::eIncremental) );
    geometries.insert( make_pair("incremental",CFilter::eIncremental) );
    geometries.insert( make_pair("solid",CFilter::eIncremental) );
    geometries.insert( make_pair("d",CFilter::eDecremental) );
    geometries.insert( make_pair("decr",CFilter::eDecremental) );
    geometries.insert( make_pair("decremental",CFilter::eDecremental) );
}

string COligoFarApp::ReportSplices( int i ) const 
{
    ostringstream o;
    o << Join( ",", m_passParam[i].GetAlignParam().GetSpliceSet().begin(), m_passParam[i].GetAlignParam().GetSpliceSet().end() );
    return o.str();
}

void COligoFarApp::AddSplice( const char* arg ) 
{
    for( const char * comma = strchr( arg, ',' ); arg; ) {
        m_passParam[m_hashPass].SetAlignParam().AddSplice( CIntron( arg ) );
        if( comma ) {
            arg = comma + 1;
            comma = strchr( arg, ',' );
        } else break;
    }
}

int COligoFarApp::ProcessData()
{
    for( unsigned p = 0; p < m_passParam.size(); ++p ) {
        string msg;
        if( ! m_passParam[p].GetHashParam().ValidateParam( msg ) ) {
            cerr << "Incompatible set of hash parameters for pass" << p 
                 << ( m_passParam[p].GetHashParam().GetSkipPositions().size() ? " with skip ppositions:" : ":" ) 
                 << msg << "\n";
            return EX_USAGE;
        }
    }

    if( m_readFile == "-" ) m_readFile = "/dev/stdin";
    if( m_fastaFile == "-" ) m_fastaFile = "/dev/stdin";
    if( m_guideFile == "-" ) m_guideFile = "/dev/stdin";
    if( m_outputFile == "-" ) m_outputFile = "/dev/stdout";

    CReaderFactory rfactory;
    rfactory.SetColorspace( m_colorSpace );
    rfactory.SetQualityChannels( m_qualityChannels );
    rfactory.SetReadIdFile( m_readFile );
    rfactory.SetReadDataFile1( m_read1qualityFile );
    rfactory.SetReadDataFile2( m_read2qualityFile );
    auto_ptr<IShortReader> qreader( rfactory.CreateReader() );
    
    if( qreader.get() == 0 ) {
        cerr << "Failed to understand input specifications: incompatible set of parameters (see oligofar -h).\n";
        return EX_USAGE;
    }

    ofstream o( m_outputFile.c_str() );
    if( m_readFile == "/dev/stdin" ) cerr << "* Notice: expecting read tab-separated data on STDIN\n";
    if( m_guideFile == "/dev/stdin" ) cerr << "* Notice: expecting guide data on STDIN\n";
    if( m_fastaFile == "/dev/stdin" ) cerr << "* Notice: expecting reference sequence data on STDIN\n";

    map<string,int> geometries;
    SetupGeometries( geometries );

    if( geometries.find( m_geometry ) == geometries.end() ) {
        THROW( runtime_error, "Unknown geometry `" << m_geometry << "'" );
    }

    for( vector<CPassParam>::iterator p = m_passParam.begin(); p != m_passParam.end(); ++p ) {
        p->SetHashParam().Validate( cerr, "[" + string( GetProgramBasename() ) + "] " );
    }

    CSeqIds seqIds;
    CFilter filter;
    CSeqScanner seqScanner;
    CSeqVecProcessor seqVecProcessor;
    seqVecProcessor.SetFastaReaderParseId( m_fastaParseIDs );

    CFeatMap featMap;
    if( m_featFile.length() ) {
        featMap.SetSeqIds( &seqIds );
        featMap.ReadFile( m_featFile );
        seqScanner.SetFeatMap( &featMap );
    }

    CScoreParam scoreParam( m_identityScore, m_mismatchScore, m_gapOpeningScore, m_gapExtentionScore );
    auto_ptr<AOutputFormatter> formatter( 0 );
    int oflags = GetOutputFlags();
    if( m_fastaParseIDs == false ) oflags |= AOutputFormatter::fReportSeqidsAsis;
    if( m_outputSam ) formatter.reset( new CSamFormatter( o, seqIds ) );
    else {
        COutputFormatter * of = new COutputFormatter( o, scoreParam, seqIds );
        formatter.reset( of );
        of->SetTopPct( m_topPct );
    }
    formatter->AssignFlags( oflags );
    formatter->SetTopCount( m_topCnt );
    
    // o-lala!!! here something should be changed....

    CScoringFactory scoringFactory( &scoreParam, &m_passParam[0].GetAlignParam() );
    CAligner aligner( &scoringFactory );
    CQueryHash queryHash( m_passParam[0].GetHashParam().GetHashMismatches(), m_passParam[0].GetHashParam().GetHashMaxAmb() ); 
    CGuideSamFile guideFile( m_guideFile, filter, seqIds, scoreParam );
    CBatch batch;
    
    batch.SetReadCount( m_readsPerRun );
    batch.SetFastaFile( m_fastaFile );
    batch.SetQueryHash( &queryHash );
    batch.SetSeqVecProcessor( &seqVecProcessor );
    batch.SetSeqScanner( &seqScanner );
    batch.SetFilter( &filter );
    batch.SetOutputFormatter( formatter.get() );
    batch.SetScoringFactory( &scoringFactory );
    batch.SetPassParam( &m_passParam );
    batch.SetRange( m_minRun, m_maxRun );

    CSnpDb snpDb( CSnpDb::eSeqId_integer );
    if( m_snpdbFile.length() ) {
        snpDb.Open( m_snpdbFile, CBDB_File::eReadOnly );
        seqScanner.SetSnpDb( &snpDb );
    }
    
//     guideFile.SetMismatchPenalty( scoreParam );
//     guideFile.SetMaxMismatch( m_guideFilemaxMismatch );

    formatter->SetGuideFile( guideFile );

    filter.SetGeometry( geometries[m_geometry] );
    filter.SetAligner( &aligner );
    filter.SetSeqIds( &seqIds );
    filter.SetTopPct( m_topPct );
    filter.SetTopCnt( m_topCnt );
    filter.SetScorePctCutoff( m_minPctid );
    filter.SetOutputFormatter( formatter.get() );

    if( m_minPctid > m_topPct ) {
        cerr << "[" << GetProgramBasename() << "] Warning: top% is greater then %cutoff ("
             << m_topPct << " < " << m_minPctid << ")\n";
    }

    aligner.SetSubjectCoding( CSeqCoding::eCoding_ncbi8na );
    aligner.SetQueryStrand( CSeqCoding::eStrand_pos );
    aligner.SetExtentionPenaltyDropoff( m_extentionPenaltyDropoff );

//    for( TSkipPositions::iterator i = m_skipPositions.begin(); i != m_skipPositions.end(); ++i ) --*i; // 1-based to 0-based
    auto_ptr<CBitmaskAccess> bitmaskAccess( 0 );
    if( m_hashBitMask.size() ) bitmaskAccess.reset( new CBitmaskAccess( m_hashBitMask ) );

    queryHash.SetStrands( m_strands );
    queryHash.SetNaHSO3mode( m_sodiumBisulfiteCuration );
    queryHash.SetHashWordBitmask( bitmaskAccess.get() );

    seqScanner.SetFilter( &filter );
    seqScanner.SetQueryHash( &queryHash );
    seqScanner.SetSeqIds( &seqIds );
    seqScanner.SetMinBlockLength( m_minBlockLength );
    seqScanner.SetInputChunk( batch.GetInputChunk() );

    seqVecProcessor.SetTargetCoding( m_colorSpace ? CSeqCoding::eCoding_colorsp : CSeqCoding::eCoding_ncbi8na );
    seqVecProcessor.AddCallback( 1, &filter );
    seqVecProcessor.AddCallback( 0, &seqScanner );

    if( !ValidateSplices( queryHash ) ) return EX_USAGE;

    if( m_seqIds.size() ) seqVecProcessor.SetSeqIdList( m_seqIds );
    if( m_gilistFile.length() ) seqVecProcessor.SetGiListFile( m_gilistFile );

    Uint8 queriesTotal = 0;
    Uint8 entriesTotal = 0;

    CProgressIndicator p( "Reading input data", "lines" );
    batch.SetReadProgressIndicator( &p );
    batch.Start();

    for( int count = 0; (!batch.Done()) && qreader->NextRead(); ++count ) {

        CQuery * query = new CQuery( 
                qreader->GetSeqCoding(), 
                qreader->GetReadId(), 
                qreader->GetReadData(0), 
                qreader->GetReadData(1), 
                m_qualityBase );
        /*
        ITERATE( TSkipPositions, k, m_skipPositions ) {
            query->MarkPositionAmbiguous( 0, *k - 1 );
            query->MarkPositionAmbiguous( 1, *k - 1 );
        }
        */
        query->ComputeBestScore( &scoreParam, 0 );
        query->ComputeBestScore( &scoreParam, 1 );
        while( guideFile.NextHit( queriesTotal, query ) ); // add guided hits
        entriesTotal += batch.AddQuery( query );
        queriesTotal ++;
        p.Increment();
    }
    batch.Purge();
    batch.SetReadProgressIndicator( 0 );
    p.Summary();
    cerr << "Queries processed: " << queriesTotal << " (" << entriesTotal << " hash entries)\n";

    if( m_printStatistics ) {
        cerr << "Memory usage:\n"
             << "  hits  left: " << CHit::GetCount() << "\n"
             << "queries left: " << CQuery::GetCount() << "\n";

        aligner.PrintStatistics( cerr );
    }

    return 0;
}

bool COligoFarApp::ValidateSplices( CQueryHash& queryHash )
{
    /*
    int wlen =    queryHash.ComputeHasherWindowLength();
    int wstep =   queryHash.GetHasherWindowStep();
    int wcnt =    queryHash.GetMaxWindowCount();
    int wstart =  queryHash.GetWindowStart();
    CIntron::TSet::const_iterator sp = m_intronSet.begin();
    for( int wp = wstart, wn = 0; wn != wcnt && sp != m_intronSet.end(); ++wn, (wp += wstep) ) {
        while( sp->GetPos() < wp ) if( ++sp == m_intronSet.end() ) return true;
        int sc = 0;
        for( CIntron::TSet::const_iterator so = sp; so != m_intronSet.end(); ++so ) {
            if( so->GetPos() >= wp + wlen ) break;
            ++sc;
        }
        if( sc > 1 ) {
            cerr << "[" << GetProgramBasename() << "] ERROR: window " << wn 
                << " [" << wp << ".." << (wp + wlen - 1) 
                << "] has more then one (" << sc << ") splices defined, which oligoFAR can't handle. "
                << "Please consider using different window size, window step, window start or splice definitions.";
            return false;
        }
    }
    */
    return true;
}

