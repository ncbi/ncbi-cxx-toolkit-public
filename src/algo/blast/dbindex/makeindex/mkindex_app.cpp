/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   Implementation of class CMkIndexApplication.
 *
 */

/*
TODO:

1. Modify to work on chunk granularity.

*/

#include <ncbi_pch.hpp>

#include <memory>
#include <string>
#include <sstream>

#include "../libindexdb_new/sequence_istream_fasta.hpp"
#include "../libindexdb_new/sequence_istream_bdb.hpp"
#include "../libindexdb_new/dbindex.hpp"
#include "mkindex_app.hpp"

using namespace std;

USING_NCBI_SCOPE;
USING_SCOPE( blastdbindex );

//------------------------------------------------------------------------------
const char * const CMkIndexApplication::USAGE_LINE = 
    "Create a BLAST database index.";

//------------------------------------------------------------------------------
void CMkIndexApplication::Init()
{
    auto_ptr< CArgDescriptions > arg_desc( new CArgDescriptions );
    arg_desc->SetUsageContext( 
            GetArguments().GetProgramBasename(), USAGE_LINE );
    arg_desc->AddKey( 
            "input", "input_file_name", "input file name",
            CArgDescriptions::eString );
    arg_desc->AddKey(
            "output", "output_file_name", "output file name",
            CArgDescriptions::eString );
    arg_desc->AddDefaultKey(
            "verbosity", "reporting_level", "how much to report",
            CArgDescriptions::eString, "normal" );
    arg_desc->AddDefaultKey(
            "iformat", "input_format", "type of input used",
            CArgDescriptions::eString, "fasta" );
    arg_desc->AddOptionalKey(
            "volsize", "volume_size", "size of an index volume in MB",
            CArgDescriptions::eInteger );
    arg_desc->AddOptionalKey(
            "iversion", "index_version", "index format version",
            CArgDescriptions::eInteger );
    arg_desc->SetConstraint( 
            "verbosity",
            &(*new CArgAllow_Strings, "quiet", "normal", "verbose") );
    arg_desc->SetConstraint(
            "iformat",
            &(*new CArgAllow_Strings, "fasta", "blastdb") );
    arg_desc->SetConstraint(
            "volsize",
            new CArgAllow_Integers( 1, kMax_Int ) );
    SetupArgDescriptions( arg_desc.release() );
}

//------------------------------------------------------------------------------
int CMkIndexApplication::Run()
{ 
    CDbIndex::SOptions options = CDbIndex::DefaultSOptions();
    std::string verbosity = GetArgs()["verbosity"].AsString();

    if( verbosity == "quiet" ) {
        options.report_level = REPORT_QUIET;
    }else if( verbosity == "verbose" ) {
        options.report_level = REPORT_VERBOSE;
    }

    if( GetArgs()["volsize"] ) {
        options.max_index_size = GetArgs()["volsize"].AsInteger();
    }

    if( GetArgs()["iversion"] ) {
        options.version = GetArgs()["iversion"].AsInteger();
    }

    options.whole_seq = WHOLE_SEQ;
    unsigned int vol_num = 0;

    CDbIndex::TSeqNum start, orig_stop( kMax_UI4 ), stop = 0;
    string ofname_base = GetArgs()["output"].AsString();
    CSequenceIStream * seqstream = 0;
    string iformat = GetArgs()["iformat"].AsString();

    if( iformat == "fasta" ) {
        seqstream = new CSequenceIStreamFasta( 
                ( GetArgs()["input"].AsString() ) );
    }else if( iformat == "blastdb" ) {
        seqstream = new CSequenceIStreamBlastDB(
                ( GetArgs()["input"].AsString() ) );
    }else {
        ASSERT( 0 );
    }

    do { 
        start = stop;
        stop = orig_stop;
        ostringstream os;
        os << ofname_base << "_v" << vol_num++ << ".idx";
        CDbIndex::MakeIndex( 
                *seqstream,
                os.str(), start, stop, options );
    }while( start != stop );

    return 0;
}

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2006/09/27 15:29:06  morgulis
 * Adding makeindex project.
 *
 * ========================================================================
 */
