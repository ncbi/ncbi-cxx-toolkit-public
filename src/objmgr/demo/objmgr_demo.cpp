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
*   Examples of using the C++ object manager
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2002/03/28 14:32:58  grichenk
* Minor fixes
*
* Revision 1.1  2002/03/28 14:07:25  grichenk
* Initial revision
*
*
* ===========================================================================
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

// Object manager includes
#include <objects/objmgr1/object_manager.hpp>
#include <objects/objmgr1/scope.hpp>
#include <objects/objmgr1/seq_vector.hpp>
#include <objects/objmgr1/desc_ci.hpp>
#include <objects/objmgr1/feat_ci.hpp>
#include <objects/objmgr1/align_ci.hpp>
#include <objects/objmgr1/gbloader.hpp>
#include <objects/objmgr1/reader_id1.hpp>

BEGIN_NCBI_SCOPE
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CDemoApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CDemoApp::Init(void)
{
    // Prepare command line descriptions
    //

    // Create
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // GI to fetch
    arg_desc->AddKey("gi", "SeqEntryID", "GI id of the Seq-Entry to fetch",
         CArgDescriptions::eInteger);

    // Program description
    string prog_description = "Example of the C++ object manager usage\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


int CDemoApp::Run(void)
{
    // Process command line args: get GI to load
    const CArgs& args = GetArgs();
    int gi = args["gi"].AsInteger();

    // Create object manager. Use CRef<> to delete the OM on exit.
    CRef<CObjectManager> pOm = new CObjectManager;

    // Create genbank data loader and register it with the OM.
    // The last argument "eDefault" informs the OM that the loader
    // must be included in scopes during the CScope::AddDefaults() call.
    pOm->RegisterDataLoader(
        *new CGBDataLoader("ID", new CId1Reader, 2),
        CObjectManager::eDefault);

    // Create a new scope.
    CScope scope(*pOm);
    // Add default loaders (GB loader in this demo) to the scope.
    scope.AddDefaults();

    // Create seq-id, set it to GI specified on the command line
    CSeq_id id;
    id.SetGi(gi);

    // Get bioseq handle for the seq-id. Most of requests will use
    // this handle.
    CBioseq_Handle handle = scope.GetBioseqHandle(id);

    // Check if the handle is valid
    if ( !handle ) {
        NcbiCout << "Bioseq not found" << NcbiEndl;
        return 0;
    }

    // Get the bioseq
    CConstRef<CBioseq> bioseq = &handle.GetBioseq();
    // -- use the bioseq: print the first seq-id
    NcbiCout << "First ID = " <<
        (*bioseq->GetId().begin())->DumpAsFasta() << NcbiEndl;

    // Get the sequence using CSeqVector. Use default encoding:
    // CSeq_data::e_Iupacna or CSeq_data::e_Iupacaa.
    CSeqVector seq_vect = handle.GetSeqVector();
    // -- use the vector: print length and the first 10 symbols
    NcbiCout << "Sequence: length=" << seq_vect.size();
    NcbiCout << " data=";
    string sout = "";
    for (size_t i = 0; (i < seq_vect.size()) && (i < 10); i++) {
        // Convert sequence symbols to printable form
        sout += seq_vect[i];
    }
    NcbiCout << NStr::PrintableString(sout) << NcbiEndl;

    // CSeq_descr iterator: iterates all descriptors starting
    // from the bioseq and going the seq-entries tree up to the
    // top-level seq-entry.
    int count = 0;
    for (CDesc_CI desc_it(handle);
        desc_it;  ++desc_it) {
        count++;
    }
    NcbiCout << "Desc count: " << count << NcbiEndl;

    // CSeq_feat iterator: iterates all features which can be found in the
    // current scope including features from all TSEs.
    // Construct seq-loc to get features for.
    CSeq_loc loc;
    // No region restrictions -- the whole bioseq is used:
    loc.SetWhole().SetGi(gi);
    count = 0;
    // Create CFeat_CI using the current scope and location.
    // No feature type restrictions.
    for (CFeat_CI feat_it(scope, loc, CSeqFeatData::e_not_set);
        feat_it;  ++feat_it) {
        count++;
        // Get seq-annot containing the feature
        CConstRef<CSeq_annot> annot = &feat_it.GetSeq_annot();
    }
    NcbiCout << "Feat count (whole, any):       " << count << NcbiEndl;

    count = 0;
    // The same region (whole sequence), but restricted feature type:
    // searching for e_Gene features only.
    for (CFeat_CI feat_it(scope, loc, CSeqFeatData::e_Gene);
        feat_it;  ++feat_it) {
        count++;
    }
    NcbiCout << "Feat count (whole, gene):      " << count << NcbiEndl;

    // Region set to interval 0..9 on the bioseq. Any feature
    // intersecting with the region should be selected.
    loc.SetInt().SetId().SetGi(gi);
    loc.SetInt().SetFrom(0);
    loc.SetInt().SetTo(9);
    count = 0;
    // Iterate features. No feature type restrictions.
    for (CFeat_CI feat_it(scope, loc, CSeqFeatData::e_not_set);
        feat_it;  ++feat_it) {
        count++;
    }
    NcbiCout << "Feat count (int. 0..9, any):   " << count << NcbiEndl;

    // Search features only in the TSE containing the target bioseq.
    // Since only one seq-id may be used as the target bioseq, the
    // iterator is constructed not from a seq-loc, but from a bioseq handle
    // and start/stop points on the bioseq. If both start and stop are 0 the
    // whole bioseq is used. The last parameter may be used for type filtering.
    count = 0;
    for (CFeat_CI feat_it(handle, 0, 999, CSeqFeatData::e_not_set);
        feat_it;  ++feat_it) {
        count++;
    }
    NcbiCout << "Feat count (TSE only, 0..9):   " << count << NcbiEndl;

    // The same way may be used to iterate aligns and graphs,
    // except that there is no type filter for both of them.
    // No region restrictions -- the whole bioseq is used:
    loc.SetWhole().SetGi(gi);
    count = 0;
    // Create CAnnot_CI using the current scope and location.
    for (CAlign_CI align_it(scope, loc); align_it;  ++align_it) {
        count++;
    }
    NcbiCout << "Annot count (whole, any):       " << count << NcbiEndl;

    NcbiCout << "Done" << NcbiEndl;
    return 0;
}


END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  MAIN


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CDemoApp().AppMain(argc, argv);
}
