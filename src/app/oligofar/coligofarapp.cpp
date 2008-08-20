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

COligoFarApp::COligoFarApp( int argc, char ** argv ) :
    CApp( argc, argv ),
    m_windowLength( 13 ),
    m_maxHashMism( 1 ),
    m_maxHashAlt( 256 ),
    m_maxFastaAlt( 256 ),
    m_strands( 0x03 ),
    m_readsPerRun( 250000 ),
    m_solexaSensitivity( 8 ),
    m_xdropoff( 2 ),
    m_topCnt( 10 ),
    m_topPct( 99 ),
    m_minPctid( 40 ),
    m_maxSimplicity( 2.0 ),
    m_identityScore(  1.0 ),
    m_mismatchScore( -1.0 ),
    m_gapOpeningScore( -3.0 ),
    m_gapExtentionScore( -1.5 ),
    m_minPair( 100 ),
    m_maxPair( 200 ),
    m_pairMargin( 20 ),
    m_qualityChannels( 0 ),
//    m_reversePair( true ),
    m_memoryLimit( Uint8( sizeof(void*) == 4 ? 3 : 8 ) * int(kGigaByte) ),
    m_performTests( false ),
    m_maxMismOnly( false ),
	m_alignmentAlgo( eAlignment_fast ),
	m_hashType( CQueryHash::eHash_arraymap ),
#ifdef _WIN32
	//m_guideFile( "nul:" ),
	m_outputFile( "con:" ),
#else
    m_guideFile( "/dev/null" ),
    m_outputFile( "/dev/stdout" ),
#endif
	m_outputFlags( "m" )
{
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

void COligoFarApp::Version()
{
    cout << GetProgramBasename() << " ver 3.19." << RevNo() << " [" << (8*sizeof(void*)) << "bit]" << endl;
}

void COligoFarApp::Help()
{
    cout << "usage: [-hV] [-i inputfile] [-d genomedb] [-b snpdb] [-g guidefile] [-l gilist] [-o output]\n"
		 << "[-1 solexa1] [-2 solexa2] [-q 0|1] [-O -eumxtadh] [-B batchsz] [-w winsize] [-n maxmism] [-N +|-]\n"
		 << "[-H v|m|a] [-a maxalt] [-A maxalt] [-y bits] [-F dust] [-s 1|2|3] [-r f|q|s] [-X xdropoff]\n"
		 << "[-I idscore] [-M mismscore] [-G gapcore] [-Q gapextscore] [-p cutoff] [-u topcnt] [-t toppct]\n"
		 << "[-z minPair] [-Z maxPair] [-D margin] [-L memlimit] [-T +|-]\n"
		 << "\nFile options:\n" 
         << "  -i file    --input-file=file          short reads input file [" << m_readFile << "]\n"
         << "  -d file    --fasta-file=file          database (fasta or basename of blastdb) file [" << m_fastaFile << "]\n"
         << "  -b file    --snpdb-file=file          snp database subject file [" << m_snpdbFile << "]\n"
//       << "  -v file    --vardb-file=file         *general variation database subject file [" << m_vardbFile << "]\n"
		 << "  -g file    --guide-file=file          guide file (output of sr-search [" << m_guideFile << "]\n"
         << "  -l file    --gi-list=file             gi list to use for the blast db [" << m_gilistFile << "]\n"
         << "  -o output  --output-file=output       set output file [" << m_outputFile << "]\n"
         << "  -1 file    --qual-1-file=file         read 1 4-channel quality file [" << m_read1qualityFile << "]\n"
         << "  -2 file    --qual-2-file=file         read 2 4-channel quality file [" << m_read2qualityFile << "]\n"
         << "  -q 0|1     --quality-channels=cnt     number of channels in input file quality columns [" << m_qualityChannels << "]\n"
         << "  -O flags   --output-flags=flags       add output flags (-huxmtdae) [" << m_outputFlags << "]\n"
         << "  -B count   --batch-size=count         how many short seqs to map at once [" << m_readsPerRun << "]\n"
		 << "\nHashing and scanning options:\n"
         << "  -w size    --window-size=size         window size [" << m_windowLength << "]\n"
         << "  -n mism    --input-max-mism=mism      maximal number of mismatches in hash window [" << m_maxHashMism << "]\n"
         << "  -N +|-     --max-mism-only=+|-        hash with max mismatches only [" << (m_maxMismOnly ? "+" : "-") << "]\n"
         << "  -H v|m|a   --hash-type=v|m|a          set hash type to vector, multimap, or arraymap [" << 
		 	(m_hashType == CQueryHash::eHash_vector ? 'v' : m_hashType == CQueryHash::eHash_multimap ? 'm' : m_hashType == CQueryHash::eHash_arraymap ? 'a' : '?' ) << "]\n"
         << "  -a alt     --input-max-alt=alt        maximal number of alternatives in hash window [" << m_maxHashAlt << "]\n"
         << "  -A alt     --fasta-max-alt=alt        maximal number of alternatives in fasta window [" << m_maxFastaAlt << "]\n"
         << "  -y bits    --solexa-sensitivity=bits  set solexa seeding sensitivity (0-15) [" << m_solexaSensitivity << "]\n"
         << "  -F simpl   --max-simplicity=val       low complexity filter cutoff for hash words [" << m_maxSimplicity << "]\n"
         << "  -s 1|2|3   --strands=1|2|3            align strands [" << m_strands << "]\n"
		 << "\nAlignment and scoring options:\n"
         << "  -r f|q|s   --algorithm=f|q|s          use alignment algoRithm (Fast, Quick or Smith-waterman) [" << char(m_alignmentAlgo) << "]\n"
         << "  -X value   --x-dropoff=value          set half band width for alignment [" << m_xdropoff << "]\n"
         << "  -I score   --identity-score=score     set identity score [" << m_identityScore << "]\n"
         << "  -M score   --mismatch-score=score     set mismatch score [" << m_mismatchScore << "]\n"
         << "  -G score   --gap-opening-score=score  set gap opening score [" << m_gapOpeningScore << "]\n"
         << "  -Q score   --gap-extention-score=val  set gap extention score [" << m_gapExtentionScore << "]\n"
		 << "\nFiltering and ranking options:\n"
         << "  -p pctid   --min-pctid=pctid          set global percent identity cutoff [" << m_minPctid << "]\n"
         << "  -u topcnt  --top-count=val            maximal number of top hits per read [" << m_topCnt << "]\n"
         << "  -t toppct  --top-percent=val          maximal score of hit (in % to best) to be reported [" << m_topPct << "]\n"
         << "  -z minPair --min-pair-dist=len        minimal pair distance [" << m_minPair << "]\n"
         << "  -Z maxPair --max-pair-dist=len        maximal pair distance [" << m_maxPair << "]\n"
         << "  -D dist    --pair-margin=len          pair distance margin [" << m_pairMargin << "]\n"
//         << "  -R y|n     --reverse-pair=y|n         are paired reads on opposite strands [" << (m_reversePair?'y':'n') << "]\n"
		 << "\nOther options:\n"
         << "  -L value   --memory-limit=value       set rlimit for the program (k|M|G suffix is allowed) [" << m_memoryLimit << "]\n"
         << "  -T +|-     --test-suite=+|-           turn test suite on/off [" << (m_performTests?"on":"off") << "]\n"
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
}

const option * COligoFarApp::GetLongOptions() const
{
    static struct option opt[] = {
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'V'},
        {"window-size", 1, 0, 'w'},
        {"max-mism-only", 1, 0, 'N'},
        {"input-max-mism", 1, 0, 'n'},
        {"hash-type", 1, 0, 'H'},
        {"input-max-alt", 1, 0, 'a'},
        {"fasta-max-alt", 1, 0, 'A'},
        {"input-file", 1, 0, 'i'},
        {"fasta-file", 1, 0, 'd'},
        {"snpdb-file", 1, 0, 'b'},
        {"vardb-file", 1, 0, 'v'},
		{"guide-file", 1, 0, 'g'},
        {"output-file", 1, 0, 'o'},
        {"output-flags", 1, 0, 'O'},
        {"gi-list", 1, 0, 'l'},
        {"strands", 1, 0, 's'},
        {"batch-size", 1, 0, 'B'},
        {"min-pctid", 1, 0, 'p'},
        {"top-count", 1, 0, 'u'},
        {"top-percent", 1, 0, 't'},
        {"qual-1-file", 1, 0, '1'},
        {"qual-2-file", 1, 0, '2'},
        {"quality-channels", 1, 0, 'q'},
        {"solexa-sensitivity", 1, 0, 'y'},
        {"min-pair", 1, 0, 'z'},
        {"max-pair", 1, 0, 'Z'},
        {"pair-margin", 1, 0, 'D'},
//        {"reverse-pair", 1, 0, 'R'},
        {"max-simplicity", 1, 0, 'F'},
        {"algorithm", 1, 0, 'r'},
        {"identity-score", 1, 0, 'I'},
        {"mismatch-score", 1, 0, 'M'},
        {"gap-opening-score", 1, 0, 'G'},
        {"gap-extention-score", 1, 0, 'Q'},
        {"x-dropoff", 1, 0, 'X'},
        {"memory-limit", 1, 0, 'L'},
        {"test-suite", 1, 0, 'T'},
        {0,0,0,0}
    };
    return opt;
}

const char * COligoFarApp::GetOptString() const
{
    return "w:n:N:H:a:A:i:d:b:v:g:o:O:l:s:B:p:u:t:1:2:q:y:z:Z:D:"/*R:*/"F:r:I:M:G:Q:X:L:T:";
}

int COligoFarApp::ParseArg( int opt, const char * arg, int longindex )
{
    switch( opt ) {
    case 'w': m_windowLength = strtol( arg, 0, 10 ); break;
    case 'n': m_maxHashMism = strtol( arg, 0, 10 ); break;
    case 'N': m_maxMismOnly = *arg == '+' ? true : *arg == '-' ? false : m_maxMismOnly; break;
	case 'H': m_hashType = (*arg == 'v'? CQueryHash::eHash_vector : *arg == 'm'? CQueryHash::eHash_multimap : *arg == 'v'? CQueryHash::eHash_arraymap : 
			  ((cerr << "Warning: Unknown hash type " << arg << ": ignored"), m_hashType) ); break;
    case 'a': m_maxHashAlt = strtol( arg, 0, 10 ); break;
    case 'A': m_maxFastaAlt = strtol( arg, 0, 10 ); break;
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
    case 'p': m_minPctid = NStr::StringToDouble( arg ); break;
    case 'u': m_topCnt = strtol( arg, 0, 10 ); break;
    case 't': m_topPct = NStr::StringToDouble( arg ); break;
    case '1': m_read1qualityFile = arg; break;
    case '2': m_read2qualityFile = arg; break;
    case 'q': m_qualityChannels = NStr::StringToInt( arg ); break;
    case 'y': m_solexaSensitivity = strtol( arg, 0, 10 ); break;
    case 'z': m_minPair = strtol( arg, 0, 10 ); break;
    case 'Z': m_maxPair = strtol( arg, 0, 10 ); break;
    case 'D': m_pairMargin = strtol( arg, 0, 10 ); break;
//    case 'R': m_reversePair = *arg == 'y'? true : *arg == 'n' ? false : m_reversePair; break;
    case 'F': m_maxSimplicity = NStr::StringToDouble( arg ); break;
	case 'r': m_alignmentAlgo = *arg == 'f' ? eAlignment_fast : *arg == 'q' ? eAlignment_quick : *arg == 's' ? eAlignment_SW : 
			  ( (cerr << "Warning: ignoring bad alignment algorithm option " << arg << endl), m_alignmentAlgo ); break;
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
    case 'T': m_performTests = (*arg == '+') ? true : (*arg == '-') ? false : m_performTests; break;
    default: return CApp::ParseArg( opt, arg, longindex );
    }
    return 0;
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

int COligoFarApp::ProcessData()
{
    switch( m_qualityChannels ) {
        case 0: case 1: break;
        default: THROW( runtime_error, "Quality channels supported in main input file columns are 0 or 1" );
    }

    ofstream o( m_outputFile.c_str() );
    ifstream reads( m_readFile.c_str() ); // to format output

    CSeqCoding::ECoding coding = CSeqCoding::eCoding_ncbi8na;

    auto_ptr<ifstream> i1(0), i2(0);
    if( m_read1qualityFile.length() ) {
        coding = CSeqCoding::eCoding_ncbipna;
        i1.reset( new ifstream( m_read1qualityFile.c_str() ) );
        if( i1->fail() )
            THROW( runtime_error, "Failed to open file [" << m_read1qualityFile << "]: " << strerror(errno) );
    } else if( m_qualityChannels == 1 ) {
        coding = CSeqCoding::eCoding_ncbiqna;
    }
    if( m_read2qualityFile.length() ) {
        if( ! m_read1qualityFile.length() )
            THROW( runtime_error, "if -2 switch is used, -1 should be present, too" );
        i2.reset( new ifstream( m_read2qualityFile.c_str() ) );
        if( i2->fail() )
            THROW( runtime_error, "Failed to open file [" << m_read2qualityFile << "]: " << strerror(errno) );
    }

    CSeqIds seqIds;
    CFilter filter;
    CSeqVecProcessor seqVecProcessor;
    CQueryHash queryHash( m_hashType, m_windowLength, m_maxHashMism, m_maxHashAlt, m_maxSimplicity );
    CSeqScanner seqScanner( m_windowLength );
    COutputFormatter formatter( o, seqIds );
    CScoreTbl scoreTbl( m_identityScore, m_mismatchScore, m_gapOpeningScore, m_gapExtentionScore );
    CGuideFile guideFile( m_guideFile, filter, seqIds );

    auto_ptr<IAligner> aligner( CreateAligner() );
    aligner->SetScoreTbl( scoreTbl );

    if( m_maxMismOnly )
        queryHash.SetMinimalMaxMism( queryHash.GetAbsoluteMaxMism() );

    CSnpDb snpDb( CSnpDb::eSeqId_integer );
    if( m_snpdbFile.length() ) {
        snpDb.Open( m_snpdbFile, CBDB_File::eReadOnly );
        seqScanner.SetSnpDb( &snpDb );
    }

    formatter.AssignFlags( GetOutputFlags() );
	formatter.SetGuideFile( guideFile );
    formatter.SetAligner( aligner.get() );

    filter.SetSeqIds( &seqIds );
	filter.SetAligner( aligner.get() );
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

    queryHash.SetStrands( m_strands );
    queryHash.SetNcbipnaToNcbi4naMask( 0xffff << (16 - m_solexaSensitivity) );

    seqScanner.SetFilter( &filter );
    seqScanner.SetQueryHash( &queryHash );
    seqScanner.SetSeqIds( &seqIds );
    seqScanner.SetMaxAlternatives( m_maxFastaAlt );
    seqScanner.SetMaxSimplicity( m_maxSimplicity );

    seqVecProcessor.AddCallback( 1, &filter );
    seqVecProcessor.AddCallback( 0, &seqScanner );

    if( m_gilistFile.length() ) seqVecProcessor.SetGiListFile( m_gilistFile );

    string buff;
    Uint8 queriesTotal = 0;
    Uint8 entriesTotal = 0;

    CProgressIndicator p( "Reading " + m_readFile, "lines" );
    CBatch batch( m_readsPerRun, m_fastaFile, queryHash, seqVecProcessor, formatter, scoreTbl );

    batch.SetReadProgressIndicator( &p );
    seqScanner.SetInputChunk( batch.GetInputChunk() );

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
            if( qf.length() != fwd.length() || qr.length() != rev.length() ) {
                THROW( runtime_error, "Quality column length does not equal to IUPAC column length" );
            }
            fwd += qf;
            rev += qr;
        }
        if( iline.fail() ) THROW( runtime_error, "Failed to parse line [" << buff << "]" );
        CQuery * query = new CQuery( coding, id, fwd, rev );
        while( guideFile.NextHit( queriesTotal + 1, query ) ); // add guided hits
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

