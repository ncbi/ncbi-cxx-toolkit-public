#include <ncbi_pch.hpp>
#include "coligofarapp.hpp"
#include "cprogressindicator.hpp"
#include "cseqscanner.hpp"
#include "cguidefile.hpp"
#include "ialigner.hpp"
#include "cfilter.hpp"
#include "cbatch.hpp"
#include "csnpdb.hpp"

#include "string-util.hpp"
#include "iupac-util.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#ifndef _WIN32
#include <sys/resource.h>
#else
#define strtoll( a, b, c ) strtoui64( a, b, c )
#endif

USING_OLIGOFAR_SCOPES;

#ifndef OLIGOFAR_VERSION
#define OLIGOFAR_VERSION "3.25" 
#endif

char COligoFarApp::HashTypeChar( int i ) const 
{
    return (m_hashType[i] == CQueryHash::eHash_vector ? 'v' : 
            m_hashType[i] == CQueryHash::eHash_multimap ? 'm' : 
            m_hashType[i] == CQueryHash::eHash_arraymap ? 'a' : '?' );
}
void COligoFarApp::SetHashType( int i, char type ) 
{
    switch( type ) {
    case 'v': case 'V': m_hashType[i] = CQueryHash::eHash_vector; break;
    case 'm': case 'M': m_hashType[i] = CQueryHash::eHash_multimap; break;
    case 'a': case 'A': m_hashType[i] = CQueryHash::eHash_arraymap; break;
    default: THROW( runtime_error, "Unknown hash type " << type );
    }
}

COligoFarApp::COligoFarApp( int argc, char ** argv ) :
    CApp( argc, argv ),
    m_minHashMism( 1 ),
    m_maxHashMism( 1 ),
    m_maxHashAlt( 256 ),
    m_maxFastaAlt( 256 ),
    m_strands( 0x03 ),
    m_readsPerRun( 250000 ),
    m_phrapSensitivity( 4 ),
    m_xdropoff( 2 ),
    m_topCnt( 10 ),
    m_topPct( 99 ),
    m_minPctid( 40 ),
    m_maxSimplicity( 2.0 ),
    m_identityScore(  1.0 ),
    m_mismatchScore( -1.0 ),
    m_gapOpeningScore( -3.0 ),
    m_gapExtentionScore( -1.5 ),
    m_rehashFactor( 0.3 ),
    m_minPair( 100 ),
    m_maxPair( 200 ),
    m_pairMargin( 20 ),
    m_qualityChannels( 0 ),
    m_qualityBase( 33 ),
    m_minBlockLength( 1000 ),
    m_guideFilemaxMismatch( 0 ),
    m_memoryLimit( Uint8( sizeof(void*) == 4 ? 3 : 8 ) * int(kGigaByte) ),
    m_performTests( false ),
    m_colorSpace( false ),
    m_run_old_scanning_code( false ),
    m_ambiguifyPositions( false ),
    m_alignmentAlgo( eAlignment_fast ),
#ifdef _WIN32
    //m_guideFile( "nul:" ),
    m_outputFile( "con:" ),
#else
    m_guideFile( "/dev/null" ),
    m_outputFile( "/dev/stdout" ),
#endif
    m_outputFlags( "m" ),
    m_geometry( "p" )
{
    m_windowLength[0] = m_windowLength[1];
    m_hashType[0] = m_hashType[1] = CQueryHash::eHash_arraymap;
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
        cout << "usage: [-hV] [--help[=full|brief|extended]] [-U version] [-C config]\n"
             << "  [-i inputfile] [-d genomedb] [-b snpdb] [-g guidefile] [-l gilist]\n"
             << "  [-1 solexa1] [-2 solexa2] [-q 0|1] [-0 qbase] [-c +|-] [-o output]\n"
             << "  [-O -eumxtadh] [-B batchsz] [-x 0|1|2] [-w [ini,]size]\n"
             << "  [-k skipPos] [-n [minMism[-maxmism]] [-r f|s] [-H v|m|a[,v|m|a]]\n"
             << "  [-f fraction] [-a maxalt] [-A maxalt] [-P phrap] [-F dust] [-s 1|2|3]\n"
             << "  [-p cutoff] [-u topcnt] [-t toppct] [-X xdropoff] [-I idscore]\n"
             << "  [-M mismscore] [-G gapcore] [-Q gapextscore] [-D minPair[-maxPair]]\n"
             << "  [-m margin] [-R geometry] [-L memlimit] [-T +|-]\n";
    if( flags & fDetails ) 
        cout 
            << "\nFile options:\n" 
            << "  -i file       --input-file=file          short reads input file [" << m_readFile << "]\n"
            << "  -d file       --fasta-file=file          database (fasta or basename of blastdb) file [" << m_fastaFile << "]\n"
            << "  -b file       --snpdb-file=file          snp database subject file [" << m_snpdbFile << "]\n"
            << "  -g file       --guide-file=file          guide file (output of sr-search [" << m_guideFile << "]\n"
            << "  -l file       --gi-list=file             gi list to use for the blast db [" << m_gilistFile << "]\n"
            << "  -1 file       --qual-1-file=file         read 1 4-channel quality file [" << m_read1qualityFile << "]\n"
            << "  -2 file       --qual-2-file=file         read 2 4-channel quality file [" << m_read2qualityFile << "]\n"
            << "  -o output     --output-file=output       set output file [" << m_outputFile << "]\n"
            << "  -x count      --guide-max-mism=count     set maximal number of mismatches for hits in guide file [" << m_guideFilemaxMismatch << "]\n"
            << "  -c +|-        --colorspace=+|-          *reads are set in dibase colorspace [" << (m_colorSpace?"yes":"no") << "]\n"
            << "  -q 0|1        --quality-channels=cnt     number of channels in input file quality columns [" << m_qualityChannels << "]\n"
            << "  -0 value      --quality-base=value       base quality number (ASCII value for character representing phrap score 0) [" << m_qualityBase << "]\n"
            << "  -0 +char      --quality-base=+char       base quality char (character representing phrap score 0) [+" << char(m_qualityBase) << "]\n"
            << "  -O flags      --output-flags=flags       add output flags (-huxmtdae) [" << m_outputFlags << "]\n"
            << "  -B count      --batch-size=count         how many short seqs to map at once [" << m_readsPerRun << "]\n"
            << "  -C config     --config-file=file         take parameters from config file section `oligofar' and continue parsing commandline\n"
            << "\nHashing and scanning options:\n"
            << "  -w [ini,]size --window-size=[ini,]size   window size for first and for consecutive passes [" << m_windowLength[0] << "," << m_windowLength[1] << "]\n"
            << "  -k pos[,...]  --window-skip=pos[,...]    skip window positions when hashing (1-based) [" << Join( ",", m_skipPositions ) << "]\n"
            << "  -n [ini,]mism --input-mism=[ini,]mism    initial and final maximal number of mismatches in hash window [" << m_minHashMism << "," << m_maxHashMism << "]\n"
            << "  -H v|m|a[,..] --hash-type=v|m|a[,..]     set hash type to vector, multimap, or arraymap [" 
            << HashTypeChar() << (m_hashType[0] == m_hashType[1] ? string("") : string(",") + HashTypeChar(1) ) << "]\n"
            << "  -f fraction   --rehash-fraction=fraction what fraction of reads should be rehashed for second pass to switch hash type [" << m_rehashFactor << "]\n"
            << "  -a alt        --input-max-alt=alt        maximal number of alternatives in hash window [" << m_maxHashAlt << "]\n"
            << "  -A alt        --fasta-max-alt=alt        maximal number of alternatives in fasta window [" << m_maxFastaAlt << "]\n"
            << "  -P score      --phrap-cutoff=score       set maximal phrap score to consider base as ambiguous [" << m_phrapSensitivity << "]\n"
            << "  -F simpl      --max-simplicity=val       low complexity filter cutoff for hash words [" << m_maxSimplicity << "]\n"
            << "  -s 1|2|3      --strands=1|2|3            align strands [" << m_strands << "]\n"
            << "\nAlignment and scoring options:\n"
            << "  -r f|q|s      --algorithm=f|s            use alignment algoRithm (Fast or Smith-waterman) [" << char(m_alignmentAlgo) << "]\n"
            << "  -X value      --x-dropoff=value          set half band width for alignment [" << m_xdropoff << "]\n"
            << "  -I score      --identity-score=score     set identity score [" << m_identityScore << "]\n"
            << "  -M score      --mismatch-score=score     set mismatch score [" << m_mismatchScore << "]\n"
            << "  -G score      --gap-opening-score=score  set gap opening score [" << m_gapOpeningScore << "]\n"
            << "  -Q score      --gap-extention-score=val  set gap extention score [" << m_gapExtentionScore << "]\n"
            << "\nFiltering and ranking options:\n"
            << "  -p pctid      --min-pctid=pctid          set global percent identity cutoff [" << m_minPctid << "]\n"
            << "  -u topcnt     --top-count=val            maximal number of top hits per read [" << m_topCnt << "]\n"
            << "  -t toppct     --top-percent=val          maximal score of hit (in % to best) to be reported [" << m_topPct << "]\n"
            << "  -D min[-max]  --pair-distance=min[-max]  pair distance [" << m_minPair << "-" << m_maxPair << "]\n"
            << "  -m dist       --pair-margin=len          pair distance margin [" << m_pairMargin << "]\n"
            << "  -R value      --geometry=value           restrictions on relative hit orientation and order for paired hits [" << (m_geometry) << "]\n"
            << "\nOther options:\n"
            << "  -h            --help=[brief|full|ext]    print help with current parameter values and exit after parsing cmdline\n"
            << "  -V            --version                  print version and exit after parsing cmdline\n"
            << "  -U version    --assert-version=version   make sure that the oligofar version is what expected [" OLIGOFAR_VERSION "]\n"
            << "  -L value      --memory-limit=value       set rlimit for the program (k|M|G suffix is allowed) [" << m_memoryLimit << "]\n"
            << "  -T +|-        --test-suite=+|-           turn test suite on/off [" << (m_performTests?"on":"off") << "]\n"
            << "\nRelative orientation flags recognized:\n"
            << "    p|centripetal|inside|pcr|solexa     reads are oriented so that vectors 5'->3' pointing to each other\n"
            << "    f|centrifugal|outside               reads are oriented so that vectors 5'->3' are pointing outside\n"
            << "    i|incr|incremental|solid            reads are on same strand, first preceeds second on this strand\n"
            << "    d|decr|decremental                  reads are on same strand, first succeeds second on this strand\n"
            << "    o|opposite                          reads are on opposite strands (combination of p and f)\n"
            << "    c|consecutive                       reads are on same strand (combination of i and d)\n"
            << "    a|all|any                           any orientation, no constraints are applied\n"
            << "\nOutput flags (for -O):\n"
            << "    -   reset all flags\n"
            << "    h   report all hits before ranking\n"
            << "    u   report unmapped reads\n"
            << "    x   indicate that there are more reads of this rank\n"
            << "    m   indicate that there are more reads of lower ranks\n"
            << "    t   indicate that there were no more hits\n"
            << "    d   report differences between query and subject\n"
            << "    a   report alignment details\n"
            << "    e   print empty line after all hits of the read are reported\n"
            << "\nNB: although -L flag is optional, it is strongly recommended to use it!\n"
            ;
    if( flags & fExtended ) 
        cout << "\nExtended options:\n"
             << "  --scan-old=true|false      Use older versions algorithms [" << (m_run_old_scanning_code?"true":"false") << "]\n"
             << "  --min-block-length=bases   Length for subject sequence to be scanned at once [" << m_minBlockLength << "]\n"
             << "  --ambiguify-positions=true|false Use skip positions to make them ambiguous [" << (m_ambiguifyPositions?"true":"false") << "]\n"
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
        {"input-mism", 1, 0, 'n'},
        {"hash-type", 1, 0, 'H'},
        {"rehash-fraction", 1, 0, 'f'},
        {"input-max-alt", 1, 0, 'a'},
        {"fasta-max-alt", 1, 0, 'A'},
        {"colorspace", 1, 0, 'c'},
        {"input-file", 1, 0, 'i'},
        {"fasta-file", 1, 0, 'd'},
        {"snpdb-file", 1, 0, 'b'},
        {"vardb-file", 1, 0, 'v'},
        {"guide-file", 1, 0, 'g'},
        {"output-file", 1, 0, 'o'},
        {"output-flags", 1, 0, 'O'},
        {"config-file", 1, 0, 'C'},
        {"gi-list", 1, 0, 'l'},
        {"strands", 1, 0, 's'},
        {"batch-size", 1, 0, 'B'},
        {"guide-max-mism", 1, 0, 'x'},
        {"min-pctid", 1, 0, 'p'},
        {"top-count", 1, 0, 'u'},
        {"top-percent", 1, 0, 't'},
        {"qual-1-file", 1, 0, '1'},
        {"qual-2-file", 1, 0, '2'},
        {"quality-channels", 1, 0, 'q'},
        {"quality-base", 1, 0, '0'},
        {"phrap-score", 1, 0, 'P'},
        {"pair-distance", 1, 0, 'D'},
        {"pair-margin", 1, 0, 'm'},
        {"geometry", 1, 0, 'R'},
        {"max-simplicity", 1, 0, 'F'},
        {"algorithm", 1, 0, 'r'},
        {"identity-score", 1, 0, 'I'},
        {"mismatch-score", 1, 0, 'M'},
        {"gap-opening-score", 1, 0, 'G'},
        {"gap-extention-score", 1, 0, 'Q'},
        {"x-dropoff", 1, 0, 'X'},
        {"memory-limit", 1, 0, 'L'},
        {"test-suite", 1, 0, 'T'},
        {"scan-old", 1, 0, kLongOpt_old },
        {"min-block-length", 1, 0, kLongOpt_min_block_length },
        {"ambiguify-positions", 1, 0, kLongOpt_ambiguifyPositions },
        {0,0,0,0}
    };
    return opt;
}

const char * COligoFarApp::GetOptString() const
{
    return "U:C:w:k:n:H:f:a:A:c:i:d:b:v:g:o:O:l:s:B:x:p:u:t:1:2:q:0:P:m:D:R:F:r:I:M:G:Q:X:L:T:";
}

int COligoFarApp::ParseArg( int opt, const char * arg, int longindex )
{
    switch( opt ) {
    case kLongOpt_old: m_run_old_scanning_code = NStr::StringToBool( arg ); break;
    case kLongOpt_min_block_length: m_minBlockLength = NStr::StringToInt( arg ); break;
    case kLongOpt_ambiguifyPositions: m_ambiguifyPositions = NStr::StringToBool( arg ); break;
    case 'U': if( strcmp( arg, OLIGOFAR_VERSION ) ) THROW( runtime_error, "Expected oligofar version " << arg << ", called " OLIGOFAR_VERSION ); break;
    case 'C': ParseConfig( arg ); break;
    case 'w': ParseRange( m_windowLength[0], m_windowLength[1], arg, "," ); break;
    case 'k': 
        do {
            list<string> x;
            Split( arg, ",", back_inserter( x ) );
            ITERATE( list<string>, t, x ) m_skipPositions.insert( m_skipPositions.end(), NStr::StringToInt( *t ) );
        } while(0);
        break;
    case 'n': ParseRange( m_minHashMism, m_maxHashMism, arg, ",-" ); break;
    case 'H': SetHashType( 0, *arg ); if( arg[1] == ',' ) SetHashType( 1, arg[2] ); break;
    case 'f': m_rehashFactor = NStr::StringToDouble( arg ); break;
    case 'a': m_maxHashAlt = strtol( arg, 0, 10 ); break;
    case 'A': m_maxFastaAlt = strtol( arg, 0, 10 ); break;
    case 'c': m_colorSpace = *arg == '+' ? true : *arg == '-' ? false : NStr::StringToBool( arg ); break;
    case 'i': m_readFile = arg; break;
    case 'd': m_fastaFile = arg; break;
    case 'b': m_snpdbFile = arg; break;
    case 'v': m_vardbFile = arg; break;
    case 'g': m_guideFile = arg; break;
    case 'o': m_outputFile = arg; break;
    case 'O': m_outputFlags += arg; if( const char * m = strrchr( m_outputFlags.c_str(), '-' ) ) m_outputFlags = m + 1; break;
    case 'l': m_gilistFile = arg; break;
    case 's': m_strands = strtol( arg, 0, 10 ); break;
    case 'B': m_readsPerRun = strtol( arg, 0, 10 ); break;
    case 'x': m_guideFilemaxMismatch = strtol( arg, 0, 10 ); break;
    case 'p': m_minPctid = NStr::StringToDouble( arg ); break;
    case 'u': m_topCnt = strtol( arg, 0, 10 ); break;
    case 't': m_topPct = NStr::StringToDouble( arg ); break;
    case '1': m_read1qualityFile = arg; break;
    case '2': m_read2qualityFile = arg; break;
    case 'q': m_qualityChannels = NStr::StringToInt( arg ); break;
    case '0': m_qualityBase = ( arg[0] && arg[0] == '+' ) ? arg[1] : NStr::StringToInt( arg ); break;
    case 'P': m_phrapSensitivity = strtol( arg, 0, 10 ); break;
    case 'D': ParseRange( m_minPair, m_maxPair, arg ); break;
    case 'm': m_pairMargin = strtol( arg, 0, 10 ); break;
    case 'R': m_geometry = arg; break;
    case 'F': m_maxSimplicity = NStr::StringToDouble( arg ); break;
    case 'r': 
        switch( *arg ) {
        case 'f': case 'F': m_alignmentAlgo = eAlignment_fast ; break;
        case 's': case 'S': m_alignmentAlgo = eAlignment_SW ; break;
        case 'g': case 'Q': THROW( runtime_error, "Quick algorithm is not supported in the current version" ); //m_alignmentAlgo = eAlignment_quick ; break;
        default: THROW( runtime_error, "Bad alignment algorithm option " << arg );
        }
        break;
    case 'I': m_identityScore = fabs( NStr::StringToDouble( arg ) ); break;
    case 'M': m_mismatchScore = -fabs( NStr::StringToDouble( arg ) ); break;
    case 'G': m_gapOpeningScore = -fabs( NStr::StringToDouble( arg ) ); break;
    case 'Q': m_gapExtentionScore = -fabs( NStr::StringToDouble( arg ) ); break;
    case 'X': m_xdropoff = abs( strtol( arg, 0, 10 ) ); break;
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
    struct rlimit rl;
    rl.rlim_cur = m_memoryLimit;
    rl.rlim_max = RLIM_INFINITY;
    cerr << "[" << GetProgramBasename() << "] Setting memory limit to " << m_memoryLimit << ": ";
    errno = 0;
    int rc = setrlimit( RLIMIT_AS, &rl );
    cerr << strerror( errno ) << endl;
    return rc;
#else
    cerr << "[" << GetProgramBasename() << "] Setting memory limit is not implemented for win32 yet, ignored.\n";
    return 0;
#endif
}

int COligoFarApp::Execute()
{
    if( int rc = RunTestSuite() ) return rc;
    if( int rc = SetLimits() ) return rc;
    return ProcessData();
}

int COligoFarApp::GetOutputFlags() const
{
    int oflags = 0;
    for( const char * f = m_outputFlags.c_str(); *f; ++f ) {
        switch( tolower(*f) ) {
        case '-': oflags = 0; break;
        case 'e': oflags |= COutputFormatter::fReportEmptyLines; break;
        case 'u': oflags |= COutputFormatter::fReportUnmapped; break;
        case 'm': oflags |= COutputFormatter::fReportMany; break;
        case 'x': oflags |= COutputFormatter::fReportMore; break;
        case 't': oflags |= COutputFormatter::fReportTerminator; break;
        case 'a': oflags |= COutputFormatter::fReportAlignment; break;
        case 'd': oflags |= COutputFormatter::fReportDifferences; break;
        case 'h': oflags |= COutputFormatter::fReportAllHits; break;
        default: cerr << "[" << GetProgramBasename() << "] Warning: unknown format flag `" << *f << "'\n"; break;
        }
    }
    return oflags;
}

void COligoFarApp::SetupGeometries( map<string,int>& geometries )
{
    geometries.insert( make_pair("p",CFilter::fInside) );
    geometries.insert( make_pair("centripetal",CFilter::fInside) );
    geometries.insert( make_pair("inside",CFilter::fInside) );
    geometries.insert( make_pair("pcr",CFilter::fInside) );
    geometries.insert( make_pair("solexa",CFilter::fInside) );
    geometries.insert( make_pair("f",CFilter::fOutside) );
    geometries.insert( make_pair("centrifugal",CFilter::fOutside) );
    geometries.insert( make_pair("outside",CFilter::fOutside) );
    geometries.insert( make_pair("i",CFilter::fInOrder) );
    geometries.insert( make_pair("incr",CFilter::fInOrder) );
    geometries.insert( make_pair("incremental",CFilter::fInOrder) );
    geometries.insert( make_pair("solid",CFilter::fInOrder) );
    geometries.insert( make_pair("d",CFilter::fBackOrder) );
    geometries.insert( make_pair("decr",CFilter::fBackOrder) );
    geometries.insert( make_pair("decremental",CFilter::fBackOrder) );
    geometries.insert( make_pair("o",CFilter::fInside|CFilter::fOutside) );
    geometries.insert( make_pair("opposite",CFilter::fInside|CFilter::fOutside) );
    geometries.insert( make_pair("c",CFilter::fInOrder|CFilter::fBackOrder) );
    geometries.insert( make_pair("consecutive",CFilter::fInOrder|CFilter::fBackOrder) );
    geometries.insert( make_pair("a",CFilter::fAll) );
    geometries.insert( make_pair("all",CFilter::fAll) );
    geometries.insert( make_pair("any",CFilter::fAll) );
}

int COligoFarApp::ProcessData()
{
    switch( m_qualityChannels ) {
        case 0: case 1: break;
        default: THROW( runtime_error, "Quality channels supported in main input file columns are 0 or 1" );
    }

    // TODO: this logic should be tweaked in future
    
    if( m_windowLength[0] < 3 || m_windowLength[0] > 30 ) 
        THROW( runtime_error, "Window length is set to " << m_windowLength[0] << " but should be in range of 3..30" );

    if( m_windowLength[0] < m_windowLength[1] ) { m_windowLength[1] = m_windowLength[0]; }

    ofstream o( m_outputFile.c_str() );
    ifstream reads( m_readFile.c_str() ); // to format output

    CSeqCoding::ECoding sbjCoding = m_colorSpace ? CSeqCoding::eCoding_colorsp : CSeqCoding::eCoding_ncbi8na;
    CSeqCoding::ECoding qryCoding = sbjCoding;

    auto_ptr<ifstream> i1(0), i2(0);
    if( m_read1qualityFile.length() ) {
        if( m_colorSpace ) THROW( runtime_error, "Have no idea how to use 4-channel quality files and colorspace at the same time" ); 
        qryCoding = CSeqCoding::eCoding_ncbipna;
        i1.reset( new ifstream( m_read1qualityFile.c_str() ) );
        if( i1->fail() )
            THROW( runtime_error, "Failed to open file [" << m_read1qualityFile << "]: " << strerror(errno) );
    } else if( m_qualityChannels == 1 ) {
        if( m_colorSpace ) THROW( runtime_error, "Have no idea how to use 1-channel quality scores and colorspace at the same time" ); 
        qryCoding = CSeqCoding::eCoding_ncbiqna;
    }
    if( m_read2qualityFile.length() ) {
        if( ! m_read1qualityFile.length() )
            THROW( runtime_error, "if -2 switch is used, -1 should be present, too" );
        i2.reset( new ifstream( m_read2qualityFile.c_str() ) );
        if( i2->fail() )
            THROW( runtime_error, "Failed to open file [" << m_read2qualityFile << "]: " << strerror(errno) );
    }
    map<string,int> geometries;
    SetupGeometries( geometries );

    if( geometries.find( m_geometry ) == geometries.end() ) {
        THROW( runtime_error, "Unknown geometry `" << m_geometry << "'" );
    }

    // TODO: change to better behaviour
    ITERATE( TSkipPositions, k, m_skipPositions ) {
        if( *k > 0 && *k <= int( m_windowLength[0] ) ) {
            m_maxHashAlt *= 4;
        }
    }

    CSeqIds seqIds;
    CFilter filter;
    CSeqVecProcessor seqVecProcessor;
    CSeqScanner seqScanner;
    COutputFormatter formatter( o, seqIds );
    CQueryHash queryHash( m_hashType[0], m_windowLength[0], m_maxHashMism, m_maxHashAlt, m_maxSimplicity );
    CScoreTbl scoreTbl( m_identityScore, m_mismatchScore, m_gapOpeningScore, m_gapExtentionScore );
    CGuideFile guideFile( m_guideFile, filter, seqIds );
    CBatch batch( m_readsPerRun, m_fastaFile, queryHash, seqVecProcessor, filter, formatter, scoreTbl );

    CSnpDb snpDb( CSnpDb::eSeqId_integer );
    if( m_snpdbFile.length() ) {
        snpDb.Open( m_snpdbFile, CBDB_File::eReadOnly );
        seqScanner.SetSnpDb( &snpDb );
    }

    batch.SetInitialWindowLength( m_windowLength[0] );
    batch.SetRegularWindowLength( m_windowLength[1] );
    batch.SetInitialAligner( CreateAligner( eAlignment_HSP,  &scoreTbl ), true );
    batch.SetRegularAligner( CreateAligner( m_alignmentAlgo, &scoreTbl ), true );
    batch.SetInitialHashType( m_hashType[0] );
    batch.SetRegularHashType( m_hashType[1] );
    batch.SetHashTypeChangeFraction( m_rehashFactor );

    guideFile.SetMismatchPenalty( scoreTbl );
    guideFile.SetMaxMismatch( m_guideFilemaxMismatch );

    formatter.AssignFlags( GetOutputFlags() );
    formatter.SetGuideFile( guideFile );

    filter.SetGeometryFlags( geometries[m_geometry] );
    filter.SetSeqIds( &seqIds );
    filter.SetMaxDist( m_maxPair + m_pairMargin );
    filter.SetMinDist( m_minPair - m_pairMargin );
    filter.SetTopPct( m_topPct );
    filter.SetTopCnt( m_topCnt );
    filter.SetScorePctCutoff( m_minPctid );
    filter.SetOutputFormatter( &formatter );

    if( m_minPctid > m_topPct ) {
        cerr << "[" << GetProgramBasename() << "] Warning: top% is greater then %cutoff ("
             << m_topPct << " < " << m_minPctid << ")\n";
    }

    for( array_set<int>::iterator i = m_skipPositions.begin(); i != m_skipPositions.end(); ++i ) --*i; // 1-based to 0-based

    if( !m_ambiguifyPositions ) {
        queryHash.SetSkipPositions( m_skipPositions );
    }
    queryHash.SetStrands( m_strands );
    queryHash.SetMinimalMaxMism( min( unsigned(queryHash.GetAbsoluteMaxMism()), m_minHashMism ) );
    queryHash.SetNcbipnaToNcbi4naScore( Uint2( 255 * pow( 10.0, double(m_phrapSensitivity)/10) ) );
    queryHash.SetNcbiqnaToNcbi4naScore( m_phrapSensitivity );

    seqScanner.SetFilter( &filter );
    seqScanner.SetQueryHash( &queryHash );
    seqScanner.SetSeqIds( &seqIds );
    seqScanner.SetMaxAlternatives( m_maxFastaAlt );
    seqScanner.SetMaxSimplicity( m_maxSimplicity );
    seqScanner.SetRunOldScanningCode( m_run_old_scanning_code );
    seqScanner.SetMinBlockLength( m_minBlockLength );
    seqScanner.SetInputChunk( batch.GetInputChunk() );

    seqVecProcessor.SetTargetCoding( sbjCoding );
    seqVecProcessor.AddCallback( 1, &filter );
    seqVecProcessor.AddCallback( 0, &seqScanner );

    if( m_gilistFile.length() ) seqVecProcessor.SetGiListFile( m_gilistFile );

    string buff;
    Uint8 queriesTotal = 0;
    Uint8 entriesTotal = 0;

    CProgressIndicator p( "Reading " + m_readFile, "lines" );
    batch.SetReadProgressIndicator( &p );
    batch.Start();

    for( int count = 0; getline( reads, buff ); ++count ) {
        if( buff.length() == 0 ) continue;
        if( buff[0] == '#' ) continue; // ignore comments
        istringstream iline( buff );
        string id, fwd, rev;
        iline >> id >> fwd;
        if( !iline.eof() ) iline >> rev;
        if( rev == "-" ) rev = "";
        if( i1.get() ) {
            if( !getline( *i1, fwd ) )
                THROW( runtime_error, "File [" << m_read1qualityFile << "] is too short" );
            if( i2.get() ) {
                if( !getline( *i2, rev ) )
                    THROW( runtime_error, "File [" << m_read2qualityFile << "] is too short" );
            } else if ( rev.length() )
                THROW( runtime_error, "Paired read on input, but single quality file is present" );
        } else if( m_qualityChannels == 1 ) {
            string qf, qr;
            iline >> qf;
            if( rev.length() ) iline >> qr;
            if( qr == "-" ) qr = "";
            if( qf.length() != fwd.length() ) {
                THROW( runtime_error, "Fwd read quality column length (" << qf << " - " << qf.length() 
                        << ") does not equal to IUPAC column length (" << fwd << " - " << fwd.length() << ") for read " << id );
            }
            if( qr.length() != rev.length() ) {
                THROW( runtime_error, "Rev read quality column length (" << qr << " - " << qr.length() 
                        << ") does not equal to IUPAC column length (" << rev << " - " << rev.length() << ") for read " << id );
            }
            fwd += qf;
            rev += qr;
        }
        if( iline.fail() ) THROW( runtime_error, "Failed to parse line [" << buff << "]" );
        CQuery * query = new CQuery( qryCoding, id, fwd, rev, m_qualityBase );
        if( m_ambiguifyPositions ) {
            ITERATE( TSkipPositions, k, m_skipPositions ) {
                query->MarkPositionAmbiguous( 0, *k - 1 );
                query->MarkPositionAmbiguous( 1, *k - 1 );
            }
        }
        query->ComputeBestScore( scoreTbl, 0 );
        query->ComputeBestScore( scoreTbl, 1 );
        while( guideFile.NextHit( queriesTotal, query ) ); // add guided hits
        entriesTotal += batch.AddQuery( query );
        queriesTotal ++;
        p.Increment();
    }
    batch.Purge();
    batch.SetReadProgressIndicator( 0 );
    p.Summary();
    cerr << "Queries processed: " << queriesTotal << " (" << entriesTotal << " hash entries)\n";

    return 0;
}

