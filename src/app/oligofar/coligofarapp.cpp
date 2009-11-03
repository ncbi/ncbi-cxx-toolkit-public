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
        cout << "usage: [-hV] [--help[=full|brief|extended]] [-U version]\n"
             << "  [short-read-options] [-0 qbase] [-d genomedb] [-b snpdb] [-g guidefile]\n"
             << "  [-v featfile] [-l gilist|-y seqID] [--hash-bitmap-file=file]\n"
             << "  [-o output] [-O -eumxtdhz] [-B batchsz] [-s 1|2|3] [-k skipPos]\n"
             << "  [--pass0 hash-options] [--pass1 hash-options]\n"
             << "  [-a maxamb] [-A maxamb] [-P phrap] [-F dust] [-X xdropoff] [-Y bandhw]\n"
             << "  [-I idscore] [-M mismscore] [-G gapcore] [-Q gapextscore]\n"
             << "  [-D minPair[-maxPair]] [-m margin] [-R geometry]\n"
             << "  [-p cutoff] [-x dropoff] [-u topcnt] [-t toppct] [-L memlimit] [-T +|-]\n"
             << "  [--NaHSO3=yes|no]\n"
             << "where hash-options are:\n"
             << "  [-w win[/word]] [-N wcnt] [-f wstep] [-r wstart] [-S stride] [-H bits]\n"
             << "  [-n mism] [-e gaps] [-j ins] [-J del] [-E dist]\n"
             << "  [--add-splice=pos([min:]max)] [--longest-del=val] [--longest-ins=val]\n"
             << "  [--max-inserted=val] [--max-deleted=val]\n"
             << "and short-read-options are:\n"
             << "  [-i reads.col] [-1 reads1] [-2 reads2] [-q 0|1|4] [-c yes|no]\n"
             << "for more details and to see effective option values run:\n"
             << "  oligofar [options] --help=full\n";

    if( flags & fDetails ) {
        cout 
            << "\nFile options:\n" 
            << "   --input-file=file         -i file       short reads tab-separated input file [" << m_readFile << "]\n"
            << "   --fasta-file=file         -d file       database (fasta or basename of blastdb) file [" << m_fastaFile << "]\n"
            << "   --snpdb-file=file         -b file       snp database subject file [" << m_snpdbFile << "]\n"
            << "   --guide-file=file         -g file       guide file (output of sr-search in SAM 1.2 format [" << m_guideFile << "]\n"
            << "   --feat-file=file          -v file       limit scanning to features listed in this file [" << m_featFile << "]\n"
            << "   --gi-list=file            -l file       gi list to use for the blast db [" << m_gilistFile << "]\n"
            << "   --read-1-file=file        -1 file       read 1 4-channel quality file (requires -i), fasta or fastq file [" << m_read1qualityFile << "]\n"
            << "   --read-2-file=file        -2 file       read 2 4-channel quality file (requires -i), fasta or fastq file [" << m_read2qualityFile << "]\n"
            << "   --output-file=output      -o output     set output file [" << m_outputFile << "]\n"
            << "   --only-seqid=seqId        -y seqId      make database scan only seqIds indicated here [" << Join( ", ", m_seqIds ) << "]\n"
//            << "   --guide-max-mism=count    -x count      set maximal number of mismatches for hits in guide file [" << m_guideFilemaxMismatch << "]\n"
            << "   --colorspace=+|-          -c +|-       *reads are set in dibase colorspace [" << (m_colorSpace?"yes":"no") << "]\n"
            << "   --quality-channels=cnt    -q 0|1|4      number of channels in input file quality columns [" << m_qualityChannels << "]\n"
            << "   --quality-base=value      -0 value      base quality number (ASCII value for character representing phrap score 0) [" << m_qualityBase << "]\n"
            << "   --quality-base=+char      -0 +char      base quality char (character representing phrap score 0) [+" << char(m_qualityBase) << "]\n"
            << "   --output-flags=flags      -O flags      add output flags (-huxmtdaez) [" << m_outputFlags << "]\n"
            << "   --batch-size=count        -B count      how many short seqs to map at once [" << m_readsPerRun << "]\n"
            << "   --batch-range=min[-max]                 which batches to run [" << m_minRun << "-" << m_maxRun << "]\n"
            << "   --NaHSO3=+|-                            subject sequences sodium bisulfite curation [" << (m_sodiumBisulfiteCuration?"yes":"no") << "]\n"
//            << "  -C config     --config-file=file         take parameters from config file section `oligofar' and continue parsing commandline\n"
            << "\nGeneral hashing and scanning options:\n"
            << "   --strands=1|2|3           -s 1|2|3      hash and lookup for strands (bitmask: 1 for +, 2 for -, 3 for both) [" << m_strands << "]\n"
            << "   --hash-bitmap-file=file                 hash bitmap file [" << m_hashBitMask << "]\n"
            ;
    
        cout
            << "\nPass-specific hashing and scanning options:\n";
        for( unsigned i = 0; i < max( size_t(2), m_passParam.size() ); ++i ) {
            cout 
                << "   --pass" << i << "                                 following options will be used for pass " << i;
            if( i >= m_passParam.size() ) cout << " [off]\n";
            else {
                set<int> skipPos;
                copy( m_passParam[i].GetHashParam().GetSkipPositions().begin(), m_passParam[i].GetHashParam().GetSkipPositions().end(), inserter( skipPos, skipPos.end() ) );
                cout 
                    << ":\n"
                    << "   --window-size=win[/word]  -w win[/word] hash using window size and word size [" << m_passParam[i].GetHashParam().GetWindowSize() << "/" << m_passParam[i].GetHashParam().GetWordSize() << "]\n"
                    << "   --window-skip=pos[,...]   -k pos[,...]  skip read positions when hashing (1-based) [" << Join( ",", skipPos ) << "]\n"
                    << "   --window-step=bases       -f bases      step between windows to hash (0 - consecutive) [" << m_passParam[i].GetHashParam().GetWindowStep() << "]\n"
                    << "   --window-start=bases      -r bases      start position for first window to hash (1 - default) [" << (m_passParam[i].GetHashParam().GetWindowStart() + 1) << "]\n"
                    << "   --stride-size=stride      -S stride     hash with given stride size [" << m_passParam[i].GetHashParam().GetStrideSize() << "]\n"
                    << "   --index-bits=bits         -H bits       set number of bits for index part of hash table [" << m_passParam[i].GetHashParam().GetHashBits() << "]\n"   
                    << "   --max-windows=count       -N count      hash using maximum number of windows [" << m_passParam[i].GetHashParam().GetWindowCount() << "]\n"
                    << "   --input-max-amb=amb       -a amb        maximal number of ambiguities in hash window [" << m_passParam[i].GetHashParam().GetHashMaxAmb() << "]\n"
                    << "   --fasta-max-amb=amb       -A amb        maximal number of ambiguities in fasta window [" << m_passParam[i].GetMaxSubjAmb() << "]\n"
                    << "   --phrap-cutoff=score      -P score      set maximal phrap score to consider base as ambiguous [" << m_passParam[i].GetPhrapCutoff() << "]\n"
                    << "   --max-simplicity=val      -F simpl      low complexity filter cutoff for hash window [" << m_passParam[i].GetHashParam().GetMaxSimplicity() << "]\n"
                    << "   --max-mism=mismatch       -n mismatch   hash allowing up to given number of mismatches (0-3) [" << m_passParam[i].GetHashParam().GetHashMismatches() << "]\n"
                    << "   --max-indel=len           -e len        hash allowing indel of up to given length (0-2) [" << max( m_passParam[i].GetHashParam().GetHashInsertions(), m_passParam[i].GetHashParam().GetHashDeletions() ) << "]\n"
                    << "   --max-ins=len             -j len        hash allowing insertion of up to given length (0-2) [" << m_passParam[i].GetHashParam().GetHashInsertions() << "]\n"
                    << "   --max-del=len             -J len        hash allowing deletion  of up to given length (0-2) [" << m_passParam[i].GetHashParam().GetHashDeletions() << "]\n"
                    << "   --max-hash-dist=cnt       -E cnt        hash allowing up to given total number of mismatches and indels (0-5) [" << m_passParam[i].GetHashParam().GetHashMaxDistance() << "]\n"
                    << "   --indel-pos=pos           -K pos        hash allowing indels only at this position [" << m_passParam[i].GetHashParam().GetHashIndelPosition() << "]\n"
                    << "   --indel-dropoff=value     -X value      set longest indel for alignment [" << m_passParam[i].GetAlignParam().GetMaxIndelLength() << "]\n"
                    << "   --band-half-width=value   -Y value      set maximal number of consecutive indels of same type for alignment [" << m_passParam[i].GetAlignParam().GetMaxIndelCount() << "]\n"
                    << "   --longest-ins=value                     set maximal length for insertions to be reliably found [" << m_passParam[i].GetAlignParam().GetMaxInsertionLength() << "]\n"
                    << "   --longest-del=value                     set maximal length for deletions to be reliably found [" << m_passParam[i].GetAlignParam().GetMaxDeletionLength() << "]\n"
                    << "   --max-inserted=value                    set maximal number of inserted bases to be allowed [" << m_passParam[i].GetAlignParam().GetMaxInsertionsCount() << "]\n"
                    << "   --max-deleted=value                     set maximal number of deleted bases to be allowed [" << m_passParam[i].GetAlignParam().GetMaxDeletionsCount() << "]\n"
                    << "   --add-splice=pos([min:]max)             add non-penalized splice site for alignment [" << ReportSplices( i ) << "]\n"
                    << "   --pair-distance=min[-max] -D min[-max]  pair distance [" << m_passParam[i].GetMinPair() << "-" << m_passParam[i].GetMaxPair() << "]\n"
                    << "   --pair-margin=len         -m dist       pair distance margin [" << m_passParam[i].GetPairMargin() << "]\n"
                    ;
            }
        }
        cout
            << "\nAlignment and scoring options:\n"
            << "   --identity-score=score    -I score      set identity score [" << m_identityScore << "]\n"
            << "   --mismatch-score=score    -M score      set mismatch score [" << m_mismatchScore << "]\n"
            << "   --gap-opening-score=score -G score      set gap opening score [" << m_gapOpeningScore << "]\n"
            << "   --gap-extention-score=val -Q score      set gap extention score [" << m_gapExtentionScore << "]\n"
            << "   --extention-dropoff       -x score      the worst penalty possible when extending alignment [" << m_extentionPenaltyDropoff << "]\n"
            << "\nFiltering and ranking options:\n"
            << "   --min-pctid=pctid         -p pctid      set global percent identity cutoff [" << m_minPctid << "]\n"
            << "   --top-count=val           -u topcnt     maximal number of top hits per read [" << m_topCnt << "]\n"
            << "   --top-percent=val         -t toppct     maximal score of hit (in % to best) to be reported [" << m_topPct << "]\n"
            << "   --geometry=value          -R value      restrictions on relative hit orientation and order for paired hits [" << (m_geometry) << "]\n"
            << "\nOther options:\n"
            << "   --help=[brief|full|ext]   -h            print help with current parameter values and exit after parsing cmdline\n"
            << "   --version                 -V            print version and exit after parsing cmdline\n"
            << "   --assert-version=version  -U version    make sure that the oligofar version is what expected [" OLIGOFAR_VERSION "]\n"
            << "   --memory-limit=value      -L value      set rlimit for the program (k|M|G suffix is allowed) [" << m_memoryLimit << "]\n"
            << "   --test-suite=+|-          -T +|-        turn test suite on/off [" << (m_performTests?"on":"off") << "]\n"
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
            << "     z   output in SAM 0.1.1 format (other flags are unsupported in this format)\n"
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
             ;
}

const option * COligoFarApp::GetLongOptions() const
{
    static struct option opt[] = {
        {"help", 2, 0, 'h'},
        {"version", 0, 0, 'V'},
        {"assert-version", 1, 0, 'U'},
        {"window-size", 1, 0, 'w'},
        {"window-skip",1,0,'k'},
        {"window-step",1,0,'f'},
        {"window-start",1,0,'r'},
        {"max-windows",1,0,'N'},
        {"max-mism", 1, 0, 'n'},
        {"max-indel", 1, 0, 'e'},
        {"max-ins", 1, 0, 'j'},
        {"max-del", 1, 0, 'J'},
        {"max-hash-dist", 1, 0, 'E'},
        {"indel-pos", 1, 0, 'K'},
        {"input-max-amb", 1, 0, 'a'},
        {"fasta-max-amb", 1, 0, 'A'},
        {"colorspace", 1, 0, 'c'},
        {"NaHSO3", 1, 0, kLongOpt_NaHSO3},
        {"input-file", 1, 0, 'i'},
        {"fasta-file", 1, 0, 'd'},
        {"snpdb-file", 1, 0, 'b'},
        {"guide-file", 1, 0, 'g'},
        {"feat-file", 1, 0, 'v'},
        {"output-file", 1, 0, 'o'},
        {"output-flags", 1, 0, 'O'},
//        {"config-file", 1, 0, 'C'},
        {"only-seqid", 1, 0, 'y'},
        {"gi-list", 1, 0, 'l'},
        {"strands", 1, 0, 's'},
        {"hash-bitmap-file", 1, 0, kLongOpt_hashBitMask },
        {"batch-size", 1, 0, 'B'},
        {"batch-range", 1, 0, kLongOpt_batchRange },
        {"guide-max-mism", 1, 0, 'x'},
        {"min-pctid", 1, 0, 'p'},
        {"top-count", 1, 0, 'u'},
        {"top-percent", 1, 0, 't'},
        {"read-1-file", 1, 0, '1'},
        {"read-2-file", 1, 0, '2'},
        {"quality-channels", 1, 0, 'q'},
        {"quality-base", 1, 0, '0'},
        {"phrap-score", 1, 0, 'P'},
        {"pair-distance", 1, 0, 'D'},
        {"pair-margin", 1, 0, 'm'},
        {"geometry", 1, 0, 'R'},
        {"max-simplicity", 1, 0, 'F'},
        {"identity-score", 1, 0, 'I'},
        {"mismatch-score", 1, 0, 'M'},
        {"gap-opening-score", 1, 0, 'G'},
        {"gap-extention-score", 1, 0, 'Q'},
        {"extention-dropoff", 1, 0, 'x'},
        {"indel-dropoff", 1, 0, 'X'},
        {"band-half-width", 1, 0, 'Y'},
        {"longest-ins", 1, 0, kLongOpt_maxInsertion},
        {"longest-del", 1, 0, kLongOpt_maxDeletion},
        {"max-inserted", 1, 0, kLongOpt_maxInsertions},
        {"max-deleted", 1, 0,  kLongOpt_maxDeletions},
        {"add-splice", 1, 0, kLongOpt_addSplice},
        {"memory-limit", 1, 0, 'L'},
        {"test-suite", 1, 0, 'T'},
        {"index-bits", 1, 0, 'H'},
        {"pass0", 0, 0, kLongOpt_pass0},
        {"pass1", 0, 0, kLongOpt_pass1},
        {"min-block-length", 1, 0, kLongOpt_min_block_length },
        {0,0,0,0}
    };
    return opt;
}

const char * COligoFarApp::GetOptString() const
{
    return "U:H:S:w:N:f:r:k:n:e:E:j:J:K:a:A:c:i:d:b:v:g:o:O:l:y:s:B:x:p:u:t:1:2:q:0:P:m:D:R:F:I:M:G:Q:X:Y:L:T:";
}

int COligoFarApp::ParseArg( int opt, const char * arg, int longindex )
{
    switch( opt ) {
    case kLongOpt_hashBitMask: m_hashBitMask = arg; break;
    case kLongOpt_min_block_length: m_minBlockLength = NStr::StringToInt( arg ); break;
    case kLongOpt_NaHSO3: m_sodiumBisulfiteCuration = *arg == '+' ? true : *arg == '-' ? false : NStr::StringToBool( arg ); break;
    case kLongOpt_pass0: m_hashPass = 0; break;
    case kLongOpt_pass1: if( m_passParam.size() < 2 ) m_passParam.push_back( m_passParam.back() ); m_hashPass = 1; break;
    case kLongOpt_maxInsertion: m_passParam[m_hashPass].SetAlignParam().SetMaxInsertionLength( abs( NStr::StringToInt( arg ) ) ); break;
    case kLongOpt_maxDeletion:  m_passParam[m_hashPass].SetAlignParam().SetMaxDeletionLength( abs( NStr::StringToInt( arg ) ) ); break;
    case kLongOpt_maxInsertions: m_passParam[m_hashPass].SetAlignParam().SetMaxInsertionCount( abs( NStr::StringToInt( arg ) ) ); break;
    case kLongOpt_maxDeletions:  m_passParam[m_hashPass].SetAlignParam().SetMaxDeletionCount( abs( NStr::StringToInt( arg ) ) ); break;
    case kLongOpt_addSplice: AddSplice( arg ); break;
    case kLongOpt_batchRange: ParseRange( m_minRun, m_maxRun, arg, "-" ); break;
    case 'U': if( strcmp( arg, OLIGOFAR_VERSION ) ) THROW( runtime_error, "Expected oligofar version " << arg << ", called " OLIGOFAR_VERSION ); break;
//    case 'C': ParseConfig( arg ); break;
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
    case 't': m_topPct = NStr::StringToDouble( arg ); break;
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

void COligoFarApp::ParseConfig( const string& cfg ) 
{
    ifstream in( cfg.c_str() );
    if( !in.good() ) THROW( runtime_error, "Failed to read config file " << cfg );
    CNcbiRegistry reg( in );
    ParseConfig( &reg );
}

void COligoFarApp::ParseConfig( IRegistry * reg )
{
    const option * opts = GetLongOptions();
    for( int index = 0 ; opts && opts->name != 0 ; ++opts, ++index ) {
        if( reg->HasEntry( "oligofar", opts->name ) ) {
            ParseArg( opts->val, reg->Get( "oligofar", opts->name ).c_str(), index );
        }
    }
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
        switch( tolower(*f) ) {
        case '-': oflags = 0; break;
        case 'z': m_outputSam = true; break;
        case 'e': oflags |= COutputFormatter::fReportEmptyLines; break;
        case 'u': oflags |= COutputFormatter::fReportUnmapped; break;
        case 'm': oflags |= COutputFormatter::fReportMany; break;
        case 'x': oflags |= COutputFormatter::fReportMore; break;
        case 't': oflags |= COutputFormatter::fReportTerminator; break;
        case 'd': oflags |= COutputFormatter::fReportDifferences; break;
        case 'h': oflags |= COutputFormatter::fReportAllHits; break;
        case 'r': oflags |= COutputFormatter::fReportRawScore; break;
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
    m_passParam[m_hashPass].SetAlignParam().AddSplice( CIntron( arg ) );
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
    CSeqVecProcessor seqVecProcessor;
    CSeqScanner seqScanner;

    CFeatMap featMap;
    if( m_featFile.length() ) {
        featMap.SetSeqIds( &seqIds );
        featMap.ReadFile( m_featFile );
        seqScanner.SetFeatMap( &featMap );
    }

    auto_ptr<AOutputFormatter> formatter( 0 );
    int oflags = GetOutputFlags();
    if( m_outputSam ) formatter.reset( new CSamFormatter( o, seqIds ) );
    else {
        COutputFormatter * of = new COutputFormatter( o, seqIds );
        formatter.reset( of );
        of->AssignFlags( oflags );
        of->SetTopCount( m_topCnt );
        of->SetTopPct( m_topPct );
    }
    // o-lala!!! here something should be changed....

    CQueryHash queryHash( m_passParam[0].GetHashParam().GetHashMismatches(), m_passParam[0].GetHashParam().GetHashMaxAmb() ); 
    CScoreParam scoreParam( m_identityScore, m_mismatchScore, m_gapOpeningScore, m_gapExtentionScore );
    CScoringFactory scoringFactory( &scoreParam, &m_passParam[0].GetAlignParam() );
    CAligner aligner( &scoringFactory );
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
    cerr << "Memory usage:\n"
         << "  hits  left: " << CHit::GetCount() << "\n"
         << "queries left: " << CQuery::GetCount() << "\n";

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

