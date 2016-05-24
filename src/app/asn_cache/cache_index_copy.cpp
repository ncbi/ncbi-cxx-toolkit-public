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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>

#include <objtools/data_loaders/asn_cache/asn_index.hpp>
#include <db/bdb/bdb_cursor.hpp>


USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  CCacheIndexCopyApp::


class CCacheIndexCopyApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CCacheIndexCopyApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddKey("i", "InputFile",
                     "Cache index source",
                     CArgDescriptions::eInputFile);

    arg_desc->AddKey("o", "OutputFile",
                     "Cache index destination",
                     CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey("type", "IndexType", "Type of index to copy", CArgDescriptions::eString);
    arg_desc->AddAlias("t", "type");
    arg_desc->SetConstraint("type",
                            &(*new CArgAllow_Strings,
                              "main",
                              "seq-id"));

    // Setup arg.descriptions for this application
    arg_desc->SetCurrentGroup("Default application arguments");
    SetupArgDescriptions(arg_desc.release());
}


int CCacheIndexCopyApp::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    string input_file  = args["i"].AsString();
    string output_file = args["o"].AsString();
    CAsnIndex::E_index_type index_type;
    if(args["type"])
        index_type = args["type"].AsString() == "main" ? CAsnIndex::e_main : CAsnIndex::e_seq_id;
    else if(input_file.find("seq_id_cache.idx") != string::npos)
        index_type = CAsnIndex::e_seq_id;
    else if(input_file.find("asn_cache.idx") != string::npos)
        index_type = CAsnIndex::e_main;
    else
        NCBI_THROW(CException, eUnknown, "Index type not specified");

    CAsnIndex input(index_type);
    input.Open(input_file, CBDB_RawFile::eReadOnly);

    CFile(output_file).Remove();
    CAsnIndex output(index_type);
    output.SetCacheSize(256 * 1024);
    output.Open(output_file, CBDB_RawFile::eReadWriteCreate);

    CBDB_FileCursor cursor(input);
    cursor.InitMultiFetch(1 * 1024 * 1024);

    CStopWatch sw;
    sw.Start();
    size_t count = 0;
    for ( ;  cursor.Fetch() == eBDB_Ok;  ++count) {
        output.SetSeqId     (input.GetSeqId());
        output.SetVersion   (input.GetVersion());
        output.SetGi        (input.GetGi());
        output.SetTimestamp (input.GetTimestamp());
        output.SetChunkId   (input.GetChunkId());
        output.SetOffset    (input.GetOffset());
        output.SetSize      (input.GetSize());
        output.SetSeqLength (input.GetSeqLength());
        output.SetTaxId     (input.GetTaxId());
        if (output.Insert() != eBDB_Ok) {
            NCBI_THROW(CException, eUnknown,
                       "failed to add item to index");
        }

        if (count  &&  count % 10000 == 0) {
            LOG_POST(Error << "  copied " << count << " items in "
                     << sw.Elapsed() << " seconds");
        }

    }
    LOG_POST(Error << "done, copied " << count << " items in " << sw.Elapsed() << " seconds");

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CCacheIndexCopyApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CCacheIndexCopyApp().AppMain(argc, argv);
}
