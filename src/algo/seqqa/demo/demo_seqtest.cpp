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
 * Authors:  Mike DiCuccio, Josh Cherry
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/scope.hpp>
#include <algo/seqqa/seqtest.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/Bioseq.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CDemoSeqQaApp : public CNcbiApplication
{
private:
    void Init();
    int Run();
};


void CDemoSeqQaApp::Init()
{
    auto_ptr<CArgDescriptions> arg_descr(new CArgDescriptions);
    arg_descr->SetUsageContext(GetArguments().GetProgramName(),
                               "Demo application testing xalgoseqqa library");

    arg_descr->AddOptionalKey("id", "Accession", "Accession to test",
                              CArgDescriptions::eString);

    arg_descr->AddOptionalKey("infile", "Input_file", "Input file to test",
                              CArgDescriptions::eInputFile);

    arg_descr->AddDefaultKey("out", "OutputFile", "Output File",
                             CArgDescriptions::eOutputFile, "-");

    arg_descr->AddExtra(0, 100, "key/value pairs for context",
                        CArgDescriptions::eString);

    SetupArgDescriptions(arg_descr.release());
}


int CDemoSeqQaApp::Run()
{
    CArgs args = GetArgs();

    CNcbiOstream& ostr = args["out"].AsOutputFile();

    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*obj_mgr);
    CRef<CScope> scope(new CScope(*obj_mgr));
    scope->AddDefaults();

    CSeqTestManager test_mgr;
    CSeqTestContext ctx(*scope);
    test_mgr.RegisterStandardTests();

    // Set any context key/value pairs specified on command line
    for(unsigned int arg_num = 1;  arg_num <= args.GetNExtra();  arg_num += 2) {
        const string& key
            = args['#' + NStr::IntToString(arg_num)].AsString();
        const string& value
            = args['#' + NStr::IntToString(arg_num + 1)].AsString();
        ctx[key] = value;
    }

    if (args["id"] && args["infile"]) {
        NcbiCerr << "Only one of -id and -infile must be specified" << NcbiEndl;
        return 1;
    }

    if (args["id"]) {
        string id_str = args["id"].AsString();
        list<string> id_list;
        NStr::Split(id_str, " ", id_list);
    
        ITERATE (list<string>, it, id_list) {
            CRef<CSeq_id> id;
            try {
                id.Reset(new CSeq_id(*it));
            } catch (CSeqIdException& e) {
                LOG_POST(Fatal << "can't interpret accession: " << *it << ": "
                         << e.what());
            }

            CBioseq_Handle handle = scope->GetBioseqHandle(*id);
            if ( !handle ) {
                LOG_POST(Fatal << "can't retrieve sequence for " << *it);
            }
        
            CRef<CSeq_test_result_set> results = test_mgr.RunTests(*id, &ctx);

            ostr << MSerial_AsnText << *results;
        }
    } else if (args["infile"]) {
        CNcbiIstream& istr = args["infile"].AsInputFile();
        CSeq_align aln;
        istr >> MSerial_AsnText >> aln;
        CRef<CSeq_test_result_set> results = test_mgr.RunTests(aln, &ctx);
        ostr << MSerial_AsnText << *results;
    } else {
        NcbiCerr << "Either -id or -infile must be specified" << NcbiEndl;
        return 1;
    }

    return 0;
}

END_NCBI_SCOPE
USING_SCOPE(ncbi);


int main(int argc, char** argv)
{
    return CDemoSeqQaApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
