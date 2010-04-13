#include <ncbi_pch.hpp>
#include "coligofarcfg.hpp"

USING_OLIGOFAR_SCOPES;

void COligofarCfg::InitArgs()
{

    AddOption( 'h', "help",    eArgOptional, eSection_ignore, "Print help with current values and exit after parsing the rest of command line", "full|brief|extra" );
    AddFlag( 'V', "version", eSection_ignore, "Print program version and exit after parsing the rest of command line" );

    AddOption( 'C',    "config-file", eSection_ignore, "Parse config file and continue parsing command line", "cfgfile" );
    AddOption( kLongOpt_writeConfig, "write-config", eArgOptional, eSection_ignore, "Write current options to config file and continue parsing command line", "cfgfile" );
    AddFlag( kLongOpt_pass0, "pass0", eSection_ignore, "Following pass options will be applied to initial pass" );
    AddFlag( kLongOpt_pass1, "pass1", eSection_ignore, "Switch to two-pass mode, following options will be applied to refining pass" );

    AddOption( 'w', "window-size", eSection_passopt, "Hash using window size and word size", "window[/word]" );
    AddOption( 'k', "window-skip", eSection_passopt, "Skip read positions when hashing (1-based)", "pos[,...]" );
    AddOption( 'f', "window-step", eSection_passopt, "Step between windows to hash (0 - consecutive)", "bases" );
    AddOption( 'r', "window-start", eSection_passopt, "Start position for first window to hash (1 - default)", "bases" );
    AddOption( 'S', "stride-size", eSection_passopt, "Hash with given stride size", "stride" );
    AddOption( 'H', "index-bits", eSection_passopt, "Set number of bits for index part of hash table", "bits" );
    AddOption( 'N', "max-windows", eSection_passopt, "Hash using maximum number of windows", "count" );
    AddOption( 'a', "input-max-amb", eSection_passopt, "Maximal number of ambiguities in hash window", "amb" );
    AddOption( 'A', "fasta-max-amb", eSection_passopt, "Maximal number of ambiguities in fasta window", "amb" );
    AddOption( 'P', "phrap-cutoff", eSection_passopt, "Set maximal phrap score to consider base as ambiguous", "score" );
    AddOption( 'F', "max-simplicity", eSection_passopt, "Low complexity filter cutoff for hash window", "simpl" );
    AddOption( 'n', "max-mismatch", eSection_passopt, "Hash allowing up to given number of mismatches (0-3)", "mismatch" );
    AddOption( 'e', "max-indel", eSection_passopt, "Hash allowing indel of up to given length (0-2)", "len" );
    AddOption( 'j', "max-ins", eSection_passopt, "Hash allowing insertion of up to given length (0-2)", "len" );
    AddOption( 'J', "max-del", eSection_passopt, "Hash allowing deletion  of up to given length (0-2)", "len" );
    AddOption( 'E', "max-hash-dist", eSection_passopt, "Hash allowing up to given total number of mismatches and indels (0-5)", "cnt" );
    AddOption( 'K', "indel-pos", eSection_passopt, "Hash allowing indels only at this position", "pos" );
    AddOption( 'X', "indel-dropoff", eSection_passopt, "Set longest indel for alignment", "value" );
    AddOption( 'Y', "band-half-width", eSection_passopt, "Set maximal number of consecutive indels of same type for alignment", "value" );
    AddOption( kLongOpt_maxInsertion, "longest-ins", eSection_passopt, "Set maximal length for insertions to be reliably found", "value" );
    AddOption( kLongOpt_maxDeletion, "longest-del", eSection_passopt, "Set maximal length for deletions to be reliably found", "value" );
    AddOption( kLongOpt_maxInsertions, "max-inserted", eSection_passopt, "Set maximal number of inserted bases to be allowed", "value" );
    AddOption( kLongOpt_maxDeletions,  "max-deleted",  eSection_passopt, "Set maximal number of deleted bases to be allowed", "value" );
    AddOption( kLongOpt_addSplice, "add-splice", eSection_passopt, "Add non-penalized splice site for alignment", "pos([min:]max)" );
    AddOption( 'D', "pair-distance", eSection_passopt, "Pair distance", "min[-max]" );
    AddOption( 'm', "pair-margin", eSection_passopt, "Pair distance margin", "len" );

    AddOption( 'i', "input-file", eSection_fileopt, "Short reads tab-separated input file", "infile" );
    AddOption( 'd', "fasta-file", eSection_fileopt, "Database (fasta or basename of blastdb) file", "dbfile" );
    AddOption( 'b', "snpdb-file", eSection_fileopt, "Snp database subject file", "snpdb" );
    AddOption( 'g', "guide-file", eSection_fileopt, "Guide file (output of sr-search in SAM 1.2 format)", "guide" );
    AddOption( 'v', "feat-file", eSection_fileopt, "Limit scanning to features listed in this file", "feat" );
    AddOption( 'l', "gi-list", eSection_fileopt, "Gi list to use for the blast db", "gilist" );
    AddOption( '1', "read-1-file", eSection_fileopt, "Read 1 4-channel quality file (requires -i), fasta or fastq file", "reads1" );
    AddOption( '2', "read-2-file", eSection_fileopt, "Read 2 4-channel quality file (requires -i), fasta or fastq file", "reads2" );
    AddOption( 'o', "output-file", eSection_fileopt, "Set output file", "outfile" );
    AddOption( 'y', "only-seqid", eSection_fileopt, "Make database scan only seqIds indicated here", "seq-id" );
    AddOption( 'c', "colorspace", eSection_fileopt, "Reads are set in dibase colorspace", "yes|no" );
    AddOption( 'q', "quality-channels", eSection_fileopt, "Number of channels in input file quality columns", "0|1|4" );
    AddOption( '0', "quality-base", eSection_fileopt, "Base quality number or char (ASCII value or character representing phrap score 0)", "value|+char" );
    AddOption( 'O', "output-flags", eSection_fileopt, "Add output flags (-huxmtdaez)", "flags" );
    AddOption( 'B', "batch-size", eSection_fileopt, "How many short seqs to map at once", "bsize" );
    AddOption( kLongOpt_batchRange, "batch-range", eSection_fileopt, "Which batches to run", "min[-max]" );
    AddOption( kLongOpt_NaHSO3, "NaHSO3", eSection_fileopt, "Subject sequences sodium bisulfite curation", "yes|no" );

    AddOption( 's', "strands", eSection_hashscan, "Hash and lookup for strands (bitmask: 1 for +, 2 for -, 3 for both)", "1|2|3" );
    AddOption( kLongOpt_hashBitMask, "hash-bitmap-file", eSection_hashscan, "Hash bitmap file", "hbmfile" );

    AddOption( 'I', "identity-score", eSection_align, "Set identity score", "score" );
    AddOption( 'M', "mismatch-score", eSection_align, "Set mismatch score", "score" );
    AddOption( 'G', "gap-opening-score", eSection_align, "Set gap opening score", "score" );
    AddOption( 'Q', "gap-extension-score", eSection_align, "Set gap extention score", "score" );
    AddOption( 'x', "extension-dropoff", eSection_align, "The worst penalty possible when extending alignment", "score" );

    AddOption( 'p', "min-pctid", eSection_filter, "Set global percent identity cutoff", "pctid" );
    AddOption( 'u', "top-count", eSection_filter, "Maximal number of top hits per read", "topcnt" );
    AddOption( 't', "top-percent", eSection_filter, "Maximal score of hit (in % to best) to be reported", "toppct" );
    AddOption( 'R', "geometry", eSection_filter, "Restrictions on relative hit orientation and order for paired hits", "value" );

    AddOption( 'U', "assert-version", eSection_other, "Make sure that the oligofar version is what expected", "version" );
    AddOption( 'L', "memory-limit", eSection_other, "Set rlimit for the program (k|M|G suffix is allowed)", "memsz" );
    AddOption( 'T', "test-suite", eSection_other, "Turn test suite on/off", "yes|no" );
    AddOption( kLongOpt_fastaParseIDs, "fasta-parse-ids", eSection_other, "Parse IDs when reading FASTA file", "yes|no" );

    AddOption( kLongOpt_min_block_length, "min-block-length", eSection_extended, "Length for subject sequence to be scanned at once", "bases" );
    AddOption( kLongOpt_printStatistics, "print-statistics", eSection_extended, "Print statistics upon completion (for debugging and profiling purposes)", "yes|no" );
}


void COligofarCfg::PrintBriefHelp( ostream& o ) 
{
    o << "usage: [--help[=brief|full|extra]] [-hV] [-C cfgfile]\n"
      << " [--pass0] [pass-options] [--pass1 [pass-options]]\n";
    for( int section = 0; section < eSection_END; ++section ) {
        if( section == eSection_ignore || section == eSection_passopt || section == eSection_extended ) continue;
        PrintBriefHelp( EConfigSection( section ), o );
    }
    o << "where pass-options are:\n";
    PrintBriefHelp( eSection_passopt, o );
}

void COligofarCfg::PrintBriefHelp( EConfigSection section, ostream& out )
{
    int cols = 78;
    // TODO: change cols from tcgetattr()
    int pos = 0;
    ITERATE( TOptionList, opt, m_optionList ) {
        if( opt->GetSection() != section ) continue;
        ostringstream o;
        if( opt->GetOptChar() ) {
            o << " [-" << char( opt->GetOpt() );
            if( opt->GetArgRq() ) {
                if( opt->GetArgRq() == eArgOptional ) o << " [" << opt->GetValHelp() << "]";
                else o << " " << opt->GetValHelp();
            } o << "]";
        } else {
            o << " [--" << opt->GetName();
            if( opt->GetArgRq() ) {
                if( opt->GetArgRq() == eArgOptional ) o << "=[" << opt->GetValHelp() << "]";
                else o << "=" << opt->GetValHelp();
            } o << "]";
        }
        if( (int)o.str().length() + pos > cols ) {
            out << "\n";
            pos = 0;
        }
        out << o.str();
        pos += o.str().length();
    }
    out << "\n";
}

void COligofarCfg::PrintFullHelp( ostream& o )
{
    o << "usage:\n";
    pair<int,int> colw(0,0);
    for( int section = 0; section < eSection_END; ++section ) {
        if( section != eSection_passopt ) 
            PrintFullHelp( EConfigSection( section ), colw, 0, 0 );
    }
    PrintFullHelp( eSection_passopt, colw, 0, 0 );
    if( GetPassCount() > 1 ) {
        PrintFullHelp( eSection_passopt, colw, 0, 1 );
    }
    for( int section = 0; section < eSection_END; ++section ) {
        if( section != eSection_passopt ) 
            PrintFullHelp( EConfigSection( section ), colw, &o, 0 );
    }
    if( GetPassCount() > 1 ) o << "Initial pass parameters are:\n";
    PrintFullHelp( eSection_passopt, colw, &o, 0 );
    if( GetPassCount() > 1 ) {
        o << "Refining pass parameters are:\n";
        PrintFullHelp( eSection_passopt, colw, &o, 1 );
    }
    o
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
        << "     z   output in SAM 1.2 format (clears flag Z)\n"
        << "     Z   output in native format (clears flag z)\n"
        << "     p   print unpaired hits only if there are no paired\n"
        << "   Only 'p' and 'u' flags work in SAM 1.2 format output\n"
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
        << "\nNB: although -L flag is optional, it is strongly recommended to use it!\n" ;
}

void COligofarCfg::PrintFullHelp( EConfigSection section, pair<int,int>& colw, ostream * out, int pass ) 
{
    ITERATE( TOptionList, opt, m_optionList ) {
        if( opt->GetSection() != section ) continue;
        ostringstream ol, os, oh;
        ol << "  --" << opt->GetName();
        if( opt->GetArgRq() ) {
            if( opt->GetArgRq() == eArgOptional ) ol << "[";
            ol << "=" << opt->GetValHelp();
            if( opt->GetArgRq() == eArgOptional ) ol << "]";
        }
        if( out ) {
            *out << setw( colw.first ) << left << ol.str();
        } else {
            colw.first = max( colw.first, (int)ol.str().length() );
        }
        if( opt->GetOptChar() ) {
            os << "  -" << char( opt->GetOpt() );
            if( opt->GetArgRq() ) {
                os << " ";
                if( opt->GetArgRq() == eArgOptional ) os << "[";
                os << opt->GetValHelp();
                if( opt->GetArgRq() == eArgOptional ) os << "]";
            }
            if( out ) {
                *out << setw( colw.second ) << left << os.str();
            } else {
                colw.second = max( colw.second, (int)os.str().length() );
            }
        } else {
            if( out ) *out << string( colw.second, ' ' );
        }
        if( out ) {
            *out << "  " << opt->GetHelp();
            ostringstream oh;
            PrintArgValue( opt->GetOpt(), oh, pass );
            if( oh.str().length() ) *out << " [" << oh.str() << "]";
            *out << "\n";
        }
    }
}

void COligofarCfg::PrintConfig( ostream& o )
{
    o << "[oligofar]";
    for( int section = 0; section < eSection_END; ++section ) {
        if( section != eSection_passopt && section != eSection_ignore ) 
            PrintConfig( EConfigSection( section ), o, 0 );
    }
    o << "\n[oligofar-pass0]";
    PrintConfig( eSection_passopt, o, 0 );
    if( GetPassCount() > 1 ) {
        o << "\n[oligofar-pass1]";
        PrintConfig( eSection_passopt, o, 1 );
    }
}

void COligofarCfg::PrintConfig( EConfigSection section, ostream& out, int pass ) 
{
    ITERATE( TOptionList, opt, m_optionList ) {
        if( opt->GetSection() != section ) continue;
        ostringstream oh;
        PrintArgValue( opt->GetOpt(), oh, pass );
        out << "\n;; " << opt->GetHelp() << "\n";
        out << opt->GetName() << " = " << oh.str() << "\n";
    }
}
