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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   Demo of using the C++ Object Manager (OM)
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>

// Object Manager includes
#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seq_vector.hpp>
#include <objects/objmgr/desc_ci.hpp>
#include <objects/objmgr/feat_ci.hpp>
#include <objects/objmgr/align_ci.hpp>
#include <objects/objmgr/gbloader.hpp>
#include <objects/objmgr/reader_id1.hpp>


using namespace ncbi;
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CSampleObjmgrApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CSampleObjmgrApplication::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI to fetch
    arg_desc->AddKey("gi", "SeqEntryID", "GI id of the Seq-Entry to fetch",
                     CArgDescriptions::eInteger);
    arg_desc->SetConstraint
        ("gi", new CArgAllow_Integers(2, 40000000));

    // Program description
    string prog_description = "Example of the C++ Object Manager usage\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


int CSampleObjmgrApplication::Run(void)
{
    // Process command line args:  get GI to load
    const CArgs& args = GetArgs();
    int gi = args["gi"].AsInteger();

    // Create object manager
    // * We use CRef<> here to automatically delete the OM on exit.
    CRef<CObjectManager> object_manager = new CObjectManager;

    // Create GenBank data loader and register it with the OM.
    // * The last argument "eDefault" informs the OM that the loader must
    // * be included in scopes during the CScope::AddDefaults() call.
    object_manager->RegisterDataLoader
        (*new CGBDataLoader("ID", new CId1Reader, 2),
         CObjectManager::eDefault);

    // Create a new scope ("attached" to our OM).
    CScope scope(*object_manager);
    // Add default loaders (GB loader in this demo) to the scope.
    scope.AddDefaults();

    // Create Seq-id, set it to the GI specified on the command line
    CSeq_id seq_id;
    seq_id.SetGi(gi);

    // Get Bioseq handle for the Seq-id.
    // * Most of requests will use this handle.
    CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(seq_id);

    // Terminate the program if the GI cannot be resolved to a Bioseq.
    if ( !bioseq_handle ) {
        ERR_POST(Fatal << "Bioseq not found, with GI=" << gi);
    }

    // Get the Bioseq object.
    CConstRef<CBioseq> bioseq = &bioseq_handle.GetBioseq();
    // Printout the first Seq-id from the Bioseq.
    cout << "First ID:  "
         << (*bioseq->GetId().begin())->DumpAsFasta()
         << endl;

    // Get the sequence using CSeqVector.
    // Use default encoding:  CSeq_data::e_Iupacna or CSeq_data::e_Iupacaa.
    CSeqVector seq_vect = bioseq_handle.GetSeqVector();
    // Printout the length and the first 10 symbols of the sequence.
    cout << "Sequence:  length=" << seq_vect.size();
    cout << ",  data=";
    string str;
    for (TSeqPos i = 0;  i < seq_vect.size()  &&  i < 10;  i++) {
        str += seq_vect[i];
    }
    cout << NStr::PrintableString(str) << endl;

    // Use CSeq_descr iterator.
    // Iterate through all descriptors -- starting from the Bioseq, go
    // all the way up the Seq-entries tree, to the top-level Seq-entry (TSE).
    unsigned desc_count = 0;
    for (CDesc_CI desc_it(bioseq_handle);
         desc_it;  ++desc_it) {
        desc_count++;
    }
    cout << "# of descriptions:  " << desc_count << endl;

    // Use CSeq_feat iterator.
    // Iterate through all features which can be found for the given Bioseq
    // in the current scope, including features from all TSEs.
    // Construct Seq-loc to get features for.
    CSeq_loc seq_loc;
    cout << "# of features:" << endl;

    // No region restrictions -- use the whole Bioseq
    seq_loc.SetWhole().SetGi(gi);
    unsigned feat_count = 0;
    // Create CFeat_CI using the current scope and location.
    // The 3rd arg value specifies that no feature type restriction is applied.
    for (CFeat_CI feat_it(scope, seq_loc, CSeqFeatData::e_not_set);
         feat_it;  ++feat_it) {
        feat_count++;
        // Get Seq-annot containing the feature
        CConstRef<CSeq_annot> annot = &feat_it.GetSeq_annot();
    }
    cout << "   [whole]           Any:    " << feat_count << endl;

    // The same region (whole sequence), but restricted feature type:
    // searching for e_Gene features only.
    unsigned gene_count = 0;
    for (CFeat_CI feat_it(scope, seq_loc, CSeqFeatData::e_Gene);
         feat_it;  ++feat_it) {
        gene_count++;
    }
    cout << "   [whole]           Genes:  " << gene_count << endl;

    // Region set to interval 0..9 on the Bioseq.
    // Any feature intersecting with the region should be selected.
    seq_loc.SetInt().SetId().SetGi(gi);
    seq_loc.SetInt().SetFrom(0);
    seq_loc.SetInt().SetTo(9);
    unsigned ranged_feat_count = 0;
    // No feature type restrictions applied.
    for (CFeat_CI feat_it(scope, seq_loc, CSeqFeatData::e_not_set);
         feat_it;  ++feat_it) {
        ranged_feat_count++;
    }
    cout << "   [0..9]            Any:    " << ranged_feat_count << endl;

    // Search features only in the TSE containing the target Bioseq.
    // Since only one Seq-id can be used as the target Bioseq, the iterator
    // is constructed not from a Seq-loc, but from a Bioseq handle and the
    // start/stop points on the Bioseq. If both start and stop are 0, then the
    // whole Bioseq is used. The last parameter may be used for type filtering.
    unsigned ranged_tse_feat_count = 0;
    for (CFeat_CI feat_it(bioseq_handle, 0, 999, CSeqFeatData::e_not_set);
         feat_it;  ++feat_it) {
        ranged_tse_feat_count++;
    }
    cout << "   [0..9, TSE-only]  Any:    " << ranged_tse_feat_count << endl;


    // The same method can be used to iterate through alignments and graphs,
    // except that there is no type filtering for these two.
    cout << "# of alignments:" << endl;

    seq_loc.SetWhole().SetGi(gi);
    unsigned align_count = 0;
    for (CAlign_CI align_it(scope, seq_loc);  align_it;  ++align_it) {
        align_count++;
    }
    cout << "   [whole]           Any:    " << align_count << endl;


    // Done
    cout << "Done" << endl;
    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CSampleObjmgrApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}




/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.2  2002/05/10 16:57:10  kimelman
 * upper gi bound increased twice
 *
 * Revision 1.1  2002/05/08 14:33:53  ucko
 * Added sample object manager application from stock objmgr_lab code.
 *
 *
 * ---------------------------------------------------------------------------
 * Log for previous incarnation (c++/src/internal/objmgr_lab/sample/demo.cpp):
 *
 * Revision 1.5  2002/05/06 03:46:18  vakatov
 * OM/OM1 renaming
 *
 * Revision 1.4  2002/04/16 15:48:37  gouriano
 * correct gi limits
 *
 * Revision 1.3  2002/04/11 16:33:48  vakatov
 * Get rid of the extraneous USING_NCBI_SCOPE
 *
 * Revision 1.2  2002/04/11 02:32:20  vakatov
 * Prepare for public demo:  revamp names and comments, change output format
 *
 * Revision 1.1  2002/04/09 19:40:16  gouriano
 * ObjMgr sample project
 *
 * ===========================================================================
 */
