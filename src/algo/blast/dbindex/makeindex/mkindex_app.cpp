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

#include <ncbi_pch.hpp>

#include <memory>
#include <string>
#include <sstream>

#ifdef LOCAL_SVN

#include "../libindexdb_new/sequence_istream_fasta.hpp"
#include "../libindexdb_new/sequence_istream_bdb.hpp"
#include "../libindexdb_new/dbindex.hpp"

#else

#include <algo/blast/dbindex/sequence_istream_fasta.hpp>
#include <algo/blast/dbindex/sequence_istream_bdb.hpp>
#include <algo/blast/dbindex/dbindex.hpp>

#endif

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
    arg_desc->AddOptionalKey( 
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
    arg_desc->AddDefaultKey(
            "legacy", "use_legacy_index_format",
            "use legacy (0-terminated offset lists) dbindex format",
            CArgDescriptions::eBoolean, "true" );
    arg_desc->AddDefaultKey(
            "idmap", "generate_idmap",
            "generate id map for the sequences in the index",
            CArgDescriptions::eBoolean, "false" );
    arg_desc->AddOptionalKey(
            "nmer", "nmer_size",
            "length of the indexed words",
            CArgDescriptions::eInteger );
    arg_desc->AddOptionalKey(
            "ws_hint", "word_size_hint",
            "most likely word size used in searches",
            CArgDescriptions::eInteger );
    arg_desc->AddOptionalKey(
            "volsize", "volume_size", "size of an index volume in MB",
            CArgDescriptions::eInteger );
    arg_desc->AddOptionalKey(
            "stat", "statistics_file",
            "write index statistics into file with that name "
            "(for testing and debugging purposes only).",
            CArgDescriptions::eString );
    arg_desc->AddOptionalKey(
            "stride", "stride",
            "distance between stored database positions",
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
    arg_desc->SetConstraint(
            "stride",
            new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint(
            "ws_hint",
            new CArgAllow_Integers( 1, kMax_Int ) );
    arg_desc->SetConstraint(
            "nmer",
            new CArgAllow_Integers( 8, 15 ) );
    SetupArgDescriptions( arg_desc.release() );
}

//------------------------------------------------------------------------------
int CMkIndexApplication::Run()
{ 
    SetDiagPostLevel( eDiag_Warning );
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

    if( GetArgs()["stat"] ) {
        options.stat_file_name = GetArgs()["stat"].AsString();
    }

    if( GetArgs()["nmer"] ) {
        options.hkey_width = GetArgs()["nmer"].AsInteger();
    }

    options.legacy = GetArgs()["legacy"].AsBoolean();
    options.idmap  = GetArgs()["idmap"].AsBoolean();

    if( GetArgs()["stride"] ) {
        if( options.legacy ) {
            ERR_POST( Warning << "-stride has no effect upon "
                                 "legacy index creation" );
        }
        else options.stride = GetArgs()["stride"].AsInteger();
    }

    if( GetArgs()["ws_hint"] )
        if( options.legacy ) {
            ERR_POST( Warning << "-ws_hint has no effect upon "
                                 "legacy index creation" );
        }
        else {
            unsigned long ws_hint = GetArgs()["ws_hint"].AsInteger();
    
            if( ws_hint < options.hkey_width + options.stride - 1 ) {
                ws_hint = options.hkey_width + options.stride - 1;
                ERR_POST( Warning << "-ws_hint requested is too low. Setting "
                                     "to the minimum value of " << ws_hint );
            }

            options.ws_hint = ws_hint;
        }

    unsigned int vol_num = 0;

    CDbIndex::TSeqNum start, orig_stop( kMax_UI4 ), stop = 0;
    string ofname_base = GetArgs()["output"].AsString();
    CSequenceIStream * seqstream = 0;
    string iformat = GetArgs()["iformat"].AsString();

    if( iformat == "fasta" ) {
        if( GetArgs()["input"] ) {
            seqstream = new CSequenceIStreamFasta( 
                    ( GetArgs()["input"].AsString() ) );
        }
        else seqstream = new CSequenceIStreamFasta( NcbiCin );
    }else if( iformat == "blastdb" ) {
        if( GetArgs()["input"] ) {
            seqstream = new CSequenceIStreamBlastDB(
                    ( GetArgs()["input"].AsString() ) );
        }
        else {
            ERR_POST( Error << "input format 'blastdb' requires -input option" );
            exit( 1 );
        }
    }else {
        ASSERT( 0 );
    }

    do { 
        start = stop;
        stop = orig_stop;
        ostringstream os;
        os << ofname_base << "." << setfill( '0' ) << setw( 2 ) 
           << vol_num++ << ".idx";
        cerr << "creating " << os.str() << endl;
        CDbIndex::MakeIndex( 
                *seqstream,
                os.str(), start, stop, options );
    }while( start != stop );

    return 0;
}
