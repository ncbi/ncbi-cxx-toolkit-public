#include <ncbi_pch.hpp>
#include "oligofar-version.hpp"
#include "csamrecords.hpp"
#include "cgetopt.hpp"

USING_OLIGOFAR_SCOPES;

int main( int argc, char ** argv )
{
    string input;
    string output;
    bool richCigar = true;
    CGetOpt getopt( argc, argv );
    getopt
        .AddHelp( argv[0], "test CSamRecord class" )
        .AddVersion( argv[0], OLIGOFAR_VERSION, __DATE__ )
        .AddArg( input, 'i', "input", "Input file (SAM 1.2)" )
        .AddArg( output, 'o', "output", "Output file (SAM 1.2)" )
        .AddArg( richCigar, 'r', "rich-cigar", "Should B,R,C be used in output CIGAR" )
        .Parse();
    if( getopt.Done() ) return getopt.GetResultCode();

    if( input == "" || input == "-" ) input = "/dev/stdin";
    if( output == "" || output == "-" ) output = "/dev/stdout";

    ifstream in( input.c_str() );
    ofstream out( output.c_str() );

    string buff;
    while( getline( in, buff ) ) {
        if( buff.length() == 0 || buff[0] == '@' ) {
            out << buff << "\n";
            continue;
        }
        CSamRecord rec( buff );
        rec.WriteAsSam( out, !richCigar );
    }

    return out.fail();
}

