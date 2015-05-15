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
 * Authors:  Josh Cherry
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
//#include <algo/seqqa/seqtest.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>

#include <algo/align/contig_assembly/contig_assembly.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CDemoContigAssemblyApp : public CNcbiApplication
{
private:
    void Init();
    int Run();
};


void CDemoContigAssemblyApp::Init()
{
    auto_ptr<CArgDescriptions> arg_descr(new CArgDescriptions);
    arg_descr->SetUsageContext(GetArguments().GetProgramName(),
                               "Demo application testing "
                               "xalgocontig_assembly library");

    arg_descr->AddPositional("id1", "id of first sequence to align",
                             CArgDescriptions::eString);
    arg_descr->AddPositional("id2", "id of second sequence to align",
                             CArgDescriptions::eString);
    arg_descr->AddDefaultKey("blast_params", "parameter_string",
                             "Similar to what would be given to "
                             "command-line blast (single quotes respected)",
                             CArgDescriptions::eString, "-F F -e 1e-5");
    arg_descr->AddDefaultKey("bandwidth", "bandwidth",
                             "list of bandwidths to try in banded alignment",
                             CArgDescriptions::eString, "401");
    arg_descr->AddDefaultKey("min_frac_ident", "fraction",
                             "minumum acceptable fraction identity",
                             CArgDescriptions::eDouble, "0.985");
    arg_descr->AddDefaultKey("max_end_slop", "base_pairs",
                             "maximum allowable unaligned bases at ends",
                             CArgDescriptions::eInteger, "10");
    SetupArgDescriptions(arg_descr.release());
}


int CDemoContigAssemblyApp::Run()
{
    const CArgs& args = GetArgs();

    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*obj_mgr);
    CRef<CScope> scope(new CScope(*obj_mgr));
    scope->AddDefaults();

    string id_str1 = args["id1"].AsString();
    string id_str2 = args["id2"].AsString();
    CSeq_id id1(id_str1);
    CSeq_id id2(id_str2);
    string blast_params = args["blast_params"].AsString();
    double min_frac_ident = args["min_frac_ident"].AsDouble();
    int max_end_slop = args["max_end_slop"].AsInteger();
    vector<CRef<CSeq_align> > alns;
    vector<unsigned int> half_widths;
    list<string> width_strings;
    NStr::Split(args["bandwidth"].AsString(), " \t\n\r", width_strings);
    ITERATE (list<string>, width_string, width_strings) {
        half_widths.push_back(NStr::StringToUInt(*width_string) / 2);
    }
    CNcbiOstrstream ostr;
    alns = CContigAssembly::Align(id1, id2, blast_params, min_frac_ident,
                                  max_end_slop, *scope, &ostr, half_widths);
    cerr << string(CNcbiOstrstreamToString(ostr));

    CSeq_annot annot;
    annot.SetData().SetAlign();  // in case there are none
    ITERATE (vector<CRef<CSeq_align> >, aln, alns) {
        annot.SetData().SetAlign().push_back(*aln);
    }
    string title = id1.GetSeqIdString(true) + " x " + id2.GetSeqIdString(true);
    CRef<CAnnotdesc> desc(new CAnnotdesc);
    desc->SetTitle(title);
    annot.SetDesc().Set().push_back(desc);
    CRef<CUser_object> uo(new CUser_object);
    uo->SetClass("demo_contig_assembly");
    uo->SetType().SetStr("alignment info");
    CUser_field& uf = uo->SetField("comment");
    list<string> lines;
    NStr::Split((string)CNcbiOstrstreamToString(ostr), "\n\r", lines);
    ITERATE(list<string>, line, lines) {
        uf.SetData().SetStrs().push_back(*line);
    }
    desc.Reset(new CAnnotdesc);
    desc->SetUser(*uo);
    annot.SetDesc().Set().push_back(desc);

    cout << MSerial_AsnText << annot;

    return 0;
}

END_NCBI_SCOPE
USING_SCOPE(ncbi);


int main(int argc, char** argv)
{
    return CDemoContigAssemblyApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
