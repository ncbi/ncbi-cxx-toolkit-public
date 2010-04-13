#include <ncbi_pch.hpp>
#include "cgetopt.hpp"
#include "cseqvecprocessor.hpp"
#include "cbitmaskbuilder.hpp"

USING_OLIGOFAR_SCOPES;


int main( int argc, char ** argv )
{
    string seqdb;
    string wmask;
    string gilist;
    string featfile;
    int wsize = 16;
    int wstep = 1;
    int maxamb = 4;
    unsigned pattern = 0;
    bool fastaParseIDs = false;
    CGetOpt getopt( argc, argv );
    getopt
        .AddGroup( "general" )
        .AddHelp( argv[0], "Build word mask for subject database" )
        .AddVersion( argv[0], "0.0.0", __DATE__ )
        .AddGroup( "files" )
        .AddArg( seqdb, 'd', "fasta-file", "Input fasta or blastdb file" )
        .AddArg( wmask, 'o', "output-file", "Output word bitmask file" )
        .AddArg( gilist, 'l', "gi-list", "Set gi list for blastdb file" )
        .AddArg( fastaParseIDs, 0, "fasta-parse-ids", "Parse FASTA ids (becomes broken if ranges are used)" )
// .AddArg( featfile, 'v', "feat-file", "Set feature list - regions to takeinto account" )
        .AddGroup( "hashing parameters" )
        .AddArg( wsize, 'w', "word-size", "Word size to use" )
        .AddArg( wstep, 'S', "word-step", "Step (stride size) to use" )
        .AddArg( maxamb, 'A', "max-amb", "Maximal number of ambiguities to count" )
        .AddArg( pattern, 'p', "pattern", "Set pattern to use with discontiguous words, 0x or 0b prefix may be used for hex or bin (-w## will be ignored)" )
        .Parse();
    if( getopt.Done() ) return getopt.GetResultCode();

    if( seqdb.length() == 0 || seqdb == "-" ) {
        if( wmask.length() == 0 ) 
            THROW( runtime_error, "Either input or output file name is required" );
        seqdb = "/dev/stdin";
    } else if( wmask.length() == 0 ) {
        wmask = seqdb + ".whbm";
    }

    if( wstep != 1 ) THROW( runtime_error, "Sorry, only wstep=1 is supported now" );

    CBitmaskBuilder builder( maxamb, wsize, wstep );
    if( pattern ) {
        builder.SetVersion( CBitmaskBuilder::eVersion_0_1_0 );
        builder.SetWindowPattern( pattern );
        cerr << "* Info: using 4^" << builder.GetWordSize() << " bits for discontiguous words " << CBitHacks::AsBits( builder.GetWindowPattern() ) << " of " << builder.GetWindowLength() << " bases\n";
    }
    CSeqVecProcessor processor;
    if( gilist.size() ) processor.SetGiListFile( gilist );
    processor.AddCallback( 0, &builder );
    processor.SetFastaReaderParseId( fastaParseIDs );
    processor.Process( seqdb );

    builder.CountBits();

    cerr << "* Have " << builder.GetBitcount() << " of " << builder.GetTotalBits() << " (" << double( builder.GetBitcount() ) / builder.GetTotalBits() * 100 << "%) " 
        << (builder.GetWindowLength() > builder.GetWordSize() ? "discontiguous " : " " )
        << builder.GetWordSize() << "-mers\n";

    return builder.Write( wmask ) ? 0 : 1;
}

