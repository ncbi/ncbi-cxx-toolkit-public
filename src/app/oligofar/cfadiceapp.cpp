#include <ncbi_pch.hpp>
#include "cfadiceapp.hpp"
#include "cprogressindicator.hpp"
#include "cseqvecprocessor.hpp"

#include "string-util.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include "getopt.h"

USING_OLIGOFAR_SCOPES;

CFaDiceApp::CFaDiceApp( int argc, char ** argv ) :
    CApp( argc, argv ),
    m_strands( 3 ),
    m_readlen( 27 ),
    m_pairlen( 0 ),
    m_margin( 0 ),
    m_pairStrands( 1 ),
    m_colorspace( false ),
    m_prbMismatch( 0 ),
    m_prbIndel( 0 ),
    m_coverage( 0.01 ),
    m_qualityDegradation( 2 ),
    m_output("/dev/stdout")
{}

void CFaDiceApp::Version( const char * )
{
    cout << GetProgramBasename() << " ver 0.9x" << endl;
}

void CFaDiceApp::Help( const char * )
{
    cout << "usage: [-hV] [options] [fasta-file ...]\n"
         << "where options are:\n"
         << "  -o file    --output=file         output file name [" << m_output << "]\n"
         << "  -s 1|2|3   --strands=1|2|3       strands for reads to generate [" << m_strands << "]\n"
         << "  -r len     --readlen=len         set read length [" << m_readlen << "]\n"
         << "  -p len     --pairlen=len         set pair length (should exceed 2*readlen, or 0) [" << m_pairlen << "]\n"
         << "  -m len     --margin=len          set variation of pair length [" << m_margin << "]\n"
         << "  -M prob    --prb-mismatch=prob   set average mismatch count (per read) [" << m_prbMismatch << "]\n"
         << "  -G prob    --prb-indel=prob      set average indel count (per read) [" << m_prbIndel << "] - not implemented\n"
         << "  -c cvg     --coverage=cvg        set expected coverage [" << m_coverage << "]\n"
         << "  -q factor  --quality-degrade=f   set quality degradation factor along read [" << m_qualityDegradation << "]\n"
         << "  -S 1|2     --pair-strands=1|2    should straids of oligos in pair be opposite (1), or same (2) [" << m_pairStrands << "]\n"
         << "  -d +|-     --colorspace=+|-      use dibase colorspace [" << (m_colorspace?'+':'-') << "]\n"
        ;
}

const option * CFaDiceApp::GetLongOptions() const
{
    static struct option opt[] = {
        {"output", 1, 0, 'o'},
        {"strands", 1, 0, 's'},
        {"readlen", 1, 0, 'r'},
        {"pairlen", 1, 0, 'p'},
        {"margin", 1, 0, 'm'},
        {"pair-strands", 1, 0, 'S'},
        {"prb-mismatch", 1, 0, 'M'},
        {"prb-indel", 1, 0, 'G'},
        {"coverage", 1, 0, 'c'},
        {"quality-degrade", 1, 0, 'q'},
        {"colorspace", 1, 0, 'd'},
        {0,0,0,0}
    };
    return opt;
}

const char * CFaDiceApp::GetOptString() const
{
    return "o:s:r:p:m:S:M:G:c:q:d:";
}

int CFaDiceApp::ParseArg( int opt, const char * arg, int longindex )
{
    switch( opt ) {
    case 's': m_strands = NStr::StringToInt( arg ); break;
    case 'r': m_readlen = NStr::StringToInt( arg ); break;
    case 'p': m_pairlen = NStr::StringToInt( arg ); break;
    case 'm': m_margin  = NStr::StringToInt( arg ); break;
    case 'S': m_pairStrands = NStr::StringToInt( arg ); break;
    case 'M': m_prbMismatch = NStr::StringToDouble( arg ); break;
    case 'G': m_prbIndel = NStr::StringToDouble( arg ); break;
    case 'c': m_coverage = NStr::StringToDouble( arg ); break;
    case 'q': m_qualityDegradation = NStr::StringToDouble( arg ); break;
    case 'o': m_output = arg; break;
    case 'd': m_colorspace = *arg == '+'; break;
    default: return CApp::ParseArg( opt, arg, longindex );
    }
    return 0;
}

int CFaDiceApp::Execute()
{
    if( m_pairlen && m_pairlen < m_readlen*2 ) 
        THROW( runtime_error, "Pair length should be either 0 or exceed double of read length" );
    if( m_prbMismatch > m_readlen/2 || m_prbMismatch < 0 ) 
        THROW( runtime_error, "Mismatch probability is out of reange" );
    ofstream out( m_output.c_str() );
    if( GetArgCount() > GetArgIndex() ) {
        for( int i = GetArgIndex(); i < GetArgCount(); ++i ) {
            ifstream in( GetArg(i) ); 
            Process( in, out );
        }
    } else {
        Process( cin, out );
    }
    return 0;
}

void CFaDiceApp::Process( istream& in, ostream& out ) 
{
    string line;
    string seqid("lcl|UNKNOWN"), sequence;
    while( getline( in, line ) ) {
        if( line.length() == 0 ) continue;
        if( line[0] == '>' ) {
            if( sequence.length() ) {
                Process( seqid, sequence, out );
            }
            const char * c = line.c_str() + 1;
            for( ; *c && isspace(*c); ++c );
            const char * d = c;
            for( ; *d && !isspace(*d); ++d );
            seqid = string( c, d );
            sequence.clear();
            continue;
        }
        sequence.append( line );
    }
    if( sequence.length() ) 
        Process( seqid, sequence, out );
}

void CFaDiceApp::Process( const string& seqid, string& sequence, ostream& out )
{
    TIds ids;
    objects::CSeq_id::ParseFastaIds( ids, seqid );
    string sid = ids.front()->AsFastaString();

    for( unsigned int i = 0; i < sequence.length(); ++i ) {
        sequence[i] = CNcbi8naBase( CIupacnaBase( sequence[i] ) );
    }
    int range = (sequence.length() - max(m_readlen, m_pairlen));
    int todo = int(  m_coverage * range / m_readlen );
    if( m_pairlen ) todo /= 2;

    CProgressIndicator p("Generating "+string(m_readlen?"pairs of ":"")+"reads for "+seqid );
    p.SetFinalValue( todo );
    while( todo-- >= 0 ) { 
        if( m_pairlen )
            GeneratePair( sid, sequence, range, out );
        else
            GenerateRead( sid, sequence, range, out );
        p.Increment();
    }
    p.Summary();
}

void CFaDiceApp::FormatRead( string& read ) const
{
    if( m_colorspace ) {
        read[0] = CIupacnaBase( CColorTwoBase( read[0] ).GetBaseCalls() );
        for( unsigned i = 1; i < read.length(); ++i ) read[i] = CColorTwoBase( read[i] ).GetColorOrd() + '0';
            //out += string(".") + char(CIupacnaBase( CColorTwoBase( read[i] ).GetBaseCalls() )) + NStr::IntToString( CColorTwoBase( read[i] ).GetColorOrd() );
        //read = out;
        //CColorTwoBase( read[i] ).GetColorOrd() + '0';
    } else {
        for( unsigned i = 0; i < read.length(); ++i ) read[i] = CIupacnaBase( CNcbi8naBase( read[i] ) );
    }
}

void CFaDiceApp::GenerateRead( const string& seqid, const string& sequence, int range, ostream& out )
{
    int pos = int( drand48()*range );
    int delta = drand48() > m_prbIndel ? 0 : drand48() < 0.5 ? -1 : 1;
    int readlen = m_readlen + delta;
    string read = sequence.substr( pos, readlen );
    bool revcompl = false;
    int miscnt = 0, gcnt = 0;
    UpdateReadStrand( read, revcompl, m_strands );
    RecodeRead( read );
    ModifyRead( read, miscnt, gcnt );
    int from = revcompl ? pos + readlen : pos + 1; // 1-based closed
    int to = revcompl ? pos + 1 : pos + readlen; // 1-based closed
    FormatRead( read );
    out << seqid << ":" << (revcompl?'-':'+') << ":" << from << ":" << to << "\t" << read << "\t-\n";
}

void CFaDiceApp::RecodeRead( string& read ) const
{
    if( m_colorspace == false ) return;
    CNcbi8naBase prev( read[0] );
    //cerr << char( CIupacnaBase( prev ) );
    for( unsigned int i = 1; i != read.length(); ++i ) {
        CNcbi8naBase next( read[i] );
        CColorTwoBase dibase( prev, next );
        //cerr << "," << char( CIupacnaBase( next ) ) << setw(2) << setfill('0') << hex << ((unsigned int)char(dibase));
        read[i] = dibase;
        prev = next;
    }
    //cerr << endl;
}
    
void CFaDiceApp::GeneratePair( const string& seqid, const string& sequence, int range, ostream& out )
{
    int pos = int( drand48()*range );
    string readf = sequence.substr( pos, m_readlen );
    bool revcompl = false;
    int p1 = pos + m_pairlen - m_readlen + int( m_margin*2*( drand48() - 0.5 ) );
    if( p1 < pos + m_readlen ) p1 = pos + m_readlen;
    if( p1 > int(sequence.length()) - m_readlen ) p1 = sequence.length() - m_readlen;
    int from = pos + 1; // 1-based closed
    int to = p1 + m_readlen; // 1-based closed
    string readr = sequence.substr( p1, m_readlen );
    bool foo;
    switch( m_pairStrands ) {
    case 1: UpdateReadStrand( readr, foo, 2 ); 
            revcompl = DiceStrand( m_strands );
            break;
    case 2: UpdateReadStrand( readf, revcompl, m_strands ); 
            UpdateReadStrand( readr, foo, revcompl ? 2 : 1 ); 
            break;
    }
    if( revcompl ) {
        swap( readf, readr );
        swap( from, to );
    }
    int mf,gf, mr,gr;
    RecodeRead(readf);
    RecodeRead(readr);
    ModifyRead(readf,mf,gf);
    ModifyRead(readr,mr,gr);
    out << seqid << ":" << ( revcompl ? '-' : '+' ) << ":" << from << ":" << to << ":" << "\t" << readf << "\t" << readr << "\n";
}

bool CFaDiceApp::DiceStrand( int s ) const 
{
    switch( s ) {
    case 3: if( drand48() >= 0.5 ) return false;
    case 2: return true;
    default: return 1;
    }
}

void CFaDiceApp::UpdateReadStrand( string& read, bool& revcompl, int s )
{
    if( revcompl = DiceStrand( s ) ) {
        string out( read );
        read.clear();
        for( int i = int(out.length()) - 1; i >= 0; --i )
            read += CNcbi8naBase( out[i] ).Complement();
    }
}

void CFaDiceApp::ModifyRead( string& read, int& mcnt, int& gcnt )
{
    mcnt = 0; 
    gcnt = 0;
    double avgPrb = m_prbMismatch/m_readlen;
    double p0 = avgPrb * 2 / ( 1 + m_qualityDegradation);
    double p1 = p0 * m_qualityDegradation;
    double pk = (p1 - p0) / read.length();
    int baseCount = m_colorspace ? 4 : 15;
    int baseBase = m_colorspace ? 0 : 1;
    while( read.length() != unsigned( m_readlen ) ) {
        int l = min( unsigned( read.length() ), unsigned( m_readlen ) );
        int k = int( 1 + (l - 2) * (1 - drand48() * drand48()) );
        if( read.length() > unsigned( m_readlen ) )
            read = read.substr( 0, k ) + read.substr( k + 1, read.length() ); // delete base
        else
            read = read.substr( 0, k ) + char(drand48()*baseCount + baseBase) + read.substr( k, read.length() ); // insert base
    }
    for( int i = 0; i < int(read.length()); ++i ) {
        double px = p0 + pk*i;
        if( drand48() < px ) {
            char c = read[i];
            do read[i] = char(i == 0 ? 1 + drand48() * 15 : ( baseBase + baseCount * drand48() )); 
            while( c == read[i] );
            ++mcnt;
        }
    }
}
