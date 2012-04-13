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

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <serial/iterator.hpp>


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

    arg_descr->AddDefaultKey("i", "Input_file", "Input file to test",
                              CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);

    arg_descr->AddDefaultKey("o", "OutputFile", "Output File",
                             CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);

    arg_descr->AddFlag("ids", "Input is seq-ids");
    arg_descr->AddFlag("b", "Output is binary asn");
    arg_descr->AddFlag("add_named_scores", "If input is alignments, add numeric results as named-scores; otherwise append as user-object");

    arg_descr->AddExtra(0, 100, "key/value pairs for context",
                        CArgDescriptions::eString);

    SetupArgDescriptions(arg_descr.release());
}


int CDemoSeqQaApp::Run()
{
    CArgs args = GetArgs();

    CNcbiIstream& istr = args["i"].AsInputFile();
    CNcbiOstream& ostr = args["o"].AsOutputFile();

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

    if(args["ids"]) {
        string line("");
        while(!NcbiGetlineEOL(istr, line).eof()) {
            if(line == "" || line[0] == '#') {
                continue;
            }
            CRef<CSeq_id> id(new CSeq_id(line));
            CBioseq_Handle handle = scope->GetBioseqHandle(*id);
            CRef<CSeq_test_result_set> results = test_mgr.RunTests(*id, &ctx);

            CRef<CSeqTestResults> results_container(new CSeqTestResults);
            results_container->SetSource().SetSeq_id(*id);
            results_container->SetResults(*results);
            
            if(args["b"]) {
                ostr << MSerial_AsnBinary << *results_container;
            } else {
                ostr << MSerial_AsnText << *results_container;
            }
        }
    } else {
        while(true) {
            CSeq_align aln;
            try {
                istr >> MSerial_AsnBinary >> aln;
            } catch (CEofException&) {
                break;
            }
            CRef<CSeq_test_result_set> results = test_mgr.RunTests(aln, &ctx);

            if(args["add_named_scores"]) {            
                for(CTypeConstIterator<CUser_field> it(Begin(*results)); it; ++it) {
                    const CUser_field& uf = *it;
                    if(uf.GetData().IsInt()) {
                        aln.SetNamedScore("qa_" + uf.GetLabel().GetStr(), uf.GetData().GetInt());
                    } else if(uf.GetData().IsReal()) {
                        aln.SetNamedScore("qa_ " + uf.GetLabel().GetStr(), uf.GetData().GetReal());
                    } else if(uf.GetData().IsBool()) {
                        aln.SetNamedScore("qa_" + uf.GetLabel().GetStr(), uf.GetData().GetBool());
                    }
                }
            } else {
                CRef<CUser_object> uo(new CUser_object);
                uo->SetClass("seqqa");
                uo->SetType().SetStr("QA-result-set");
                vector<CRef<CUser_object > > results_vector;
                NON_CONST_ITERATE(CSeq_test_result_set::Tdata, it, results->Set()) {
                    CSeq_test_result& result = **it;
                    CRef<CUser_object> result_uo(&result.SetOutput_data());
                    results_vector.push_back(result_uo);
                }
                uo->AddField("results", results_vector);
                aln.SetExt().push_back(uo);
            }
        
            if(args["b"]) {
                ostr << MSerial_AsnBinary << aln;
            } else {
                ostr << MSerial_AsnText << aln;
            }
        }
    }

    return 0;
}

END_NCBI_SCOPE
USING_SCOPE(ncbi);


int main(int argc, char** argv)
{
    return CDemoSeqQaApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
