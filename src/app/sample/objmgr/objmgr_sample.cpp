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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>


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
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    // Process command line args:  get GI to load
    const CArgs& args = GetArgs();
    int gi = args["gi"].AsInteger();

    /////////////////////////////////////////////////////////////////////////
    // Create object manager
    // * We use CRef<> here to automatically delete the OM on exit.
    // * While the CRef<> exists GetInstance() will return the same object.
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    // Create GenBank data loader and register it with the OM.
    // * The GenBank loader is automatically marked as a default loader
    // * to be included in scopes during the CScope::AddDefaults() call.
    CGBDataLoader::RegisterInObjectManager(*object_manager);

    // Create a new scope ("attached" to our OM).
    CScope scope(*object_manager);
    // Add default loaders (GB loader in this demo) to the scope.
    scope.AddDefaults();

    // Create Seq-id, set it to the GI specified on the command line
    CSeq_id seq_id;
    seq_id.SetGi(gi);

    /////////////////////////////////////////////////////////////////////////
    // Get Bioseq handle for the Seq-id.
    // * Most of requests will use this handle.
    CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(seq_id);

    // Terminate the program if the GI cannot be resolved to a Bioseq.
    if ( !bioseq_handle ) {
        ERR_POST(Fatal << "Bioseq not found, with GI=" << gi);
    }

    /////////////////////////////////////////////////////////////////////////
    // Get the bioseq core, which is CBioseq with only most important
    // members (Id and parts of Inst) guaranteed to be set.
    CBioseq_Handle::TBioseqCore bioseq_core = bioseq_handle.GetBioseqCore();
    // Printout each Seq-id from the Bioseq.
    NcbiCout << "ID: ";
    // "iterate" is the same as:
    // for (CBioseq::TId::const_iterator id_it = bioseq_core->GetId().begin();
    //      id_it != bioseq->GetId().end(); ++id_it)
    ITERATE (CBioseq::TId, id_it, bioseq_core->GetId()) {
        if (id_it != bioseq_core->GetId().begin())
            NcbiCout << " + "; // print id separator
        NcbiCout << (*id_it)->DumpAsFasta();
    }
    NcbiCout << NcbiEndl;

    /////////////////////////////////////////////////////////////////////////
    // Get the sequence using CSeqVector.
    // Use default encoding:  CSeq_data::e_Iupacna or CSeq_data::e_Iupacaa.
    CSeqVector seq_vect = bioseq_handle.GetSeqVector();
    // Printout length of the sequence.
    NcbiCout << "Sequence:  length=" << seq_vect.size();
    NcbiCout << ",  data=";

    // Copy up to 10 first symbmols into string and print it.
    string str;
    seq_vect.GetSeqData(0, 9, str);
    // Use NStr::PrintableString() to print the sequence since it may
    // contain non-printable characters.
    NcbiCout << NStr::PrintableString(str) << NcbiEndl;

    /////////////////////////////////////////////////////////////////////////
    // Use CSeqdesc iterator.
    // Iterate through all descriptors -- starting from the Bioseq, go
    // all the way up the Seq-entries tree, to the top-level Seq-entry (TSE).
    unsigned desc_count = 0;
    for (CSeqdesc_CI desc_it(bioseq_handle); desc_it; ++desc_it) {
        desc_count++;
    }
    NcbiCout << "# of descriptors:  " << desc_count << NcbiEndl;

    /////////////////////////////////////////////////////////////////////////
    // Use CSeq_feat iterator.
    // Iterate through all features which can be found for the given Bioseq
    // in the current scope, including features from all TSEs.
    // Construct Seq-loc to get features for.
    CSeq_loc seq_loc;
    // No region restrictions -- use the whole Bioseq
    seq_loc.SetWhole().SetGi(gi);

    // Construct selector to restrict features by type etc. Right now
    // it's set to find all features includung those located on sequence
    // segments, resolve far references if any.
    SAnnotSelector sel(CSeqFeatData::e_not_set); // no type restrictions
    sel.SetResolveAll();                         // no resolution restrictions
    NcbiCout << "# of features:" << NcbiEndl;

    unsigned feat_count = 0;
    // Create CFeat_CI using the current scope and location.
    // The 3rd arg value specifies that no feature type restriction is applied.
    for (CFeat_CI feat_it(scope, seq_loc, sel); feat_it; ++feat_it) {
        feat_count++;
        // Get Seq-annot containing the feature
        CConstRef<CSeq_annot> annot(&feat_it.GetSeq_annot());
    }
    NcbiCout << "   [whole]           Any:    " << feat_count << NcbiEndl;

    // The same region (whole sequence), but restricted feature type:
    // searching for e_Gene features only.
    unsigned gene_count = 0;
    sel.SetFeatType(CSeqFeatData::e_Gene);
    for (CFeat_CI feat_it(scope, seq_loc, sel); feat_it; ++feat_it) {
        gene_count++;
    }
    NcbiCout << "   [whole]           Genes:  " << gene_count << NcbiEndl;

    // Region set to interval 0..9 on the Bioseq.
    // Any feature intersecting with the region should be selected.
    seq_loc.SetInt().SetId().SetGi(gi);
    seq_loc.SetInt().SetFrom(0);
    seq_loc.SetInt().SetTo(9);
    sel.SetFeatType(CSeqFeatData::e_not_set);
    unsigned ranged_feat_count = 0;
    // No feature type restrictions applied.
    for (CFeat_CI feat_it(scope, seq_loc, sel); feat_it; ++feat_it) {
        ranged_feat_count++;
    }
    NcbiCout << "   [0..9]            Any:    " << ranged_feat_count << NcbiEndl;

    // Search features only in the TSE containing the target Bioseq.
    // Since only one Seq-id can be used as the target Bioseq, the iterator
    // is constructed not from a Seq-loc, but from a Bioseq handle and the
    // start/stop points on the Bioseq. If both start and stop are 0, then the
    // whole Bioseq is used.
    unsigned ranged_tse_feat_count = 0;
    for (CFeat_CI feat_it(bioseq_handle, 0, 999, sel); feat_it; ++feat_it) {
        ranged_tse_feat_count++;
    }
    NcbiCout << "   [0..999, TSE]     Any:    "
        << ranged_tse_feat_count << NcbiEndl;

    /////////////////////////////////////////////////////////////////////////
    // The same method can be used to iterate through alignments and graphs,
    // except that there is no type filtering for these two.
    NcbiCout << "# of alignments:" << NcbiEndl;

    seq_loc.SetWhole().SetGi(gi);
    unsigned align_count = 0;
    for (CAlign_CI align_it(scope, seq_loc); align_it; ++align_it) {
        align_count++;
    }
    NcbiCout << "   [whole]           Any:    " << align_count << NcbiEndl;

    // Done
    NcbiCout << "Done" << NcbiEndl;
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
 * Revision 1.17  2004/08/19 21:10:51  grichenk
 * Updated object manager sample code
 *
 * Revision 1.16  2004/07/21 15:51:24  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.15  2004/05/21 21:41:42  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.14  2004/02/09 19:18:51  grichenk
 * Renamed CDesc_CI to CSeq_descr_CI. Redesigned CSeq_descr_CI
 * and CSeqdesc_CI to avoid using data directly.
 *
 * Revision 1.13  2004/01/07 17:38:03  vasilche
 * Fixed include path to genbank loader.
 *
 * Revision 1.12  2003/06/02 16:06:17  dicuccio
 * Rearranged src/objects/ subtree.  This includes the following shifts:
 *     - src/objects/asn2asn --> arc/app/asn2asn
 *     - src/objects/testmedline --> src/objects/ncbimime/test
 *     - src/objects/objmgr --> src/objmgr
 *     - src/objects/util --> src/objmgr/util
 *     - src/objects/alnmgr --> src/objtools/alnmgr
 *     - src/objects/flat --> src/objtools/flat
 *     - src/objects/validator --> src/objtools/validator
 *     - src/objects/cddalignview --> src/objtools/cddalignview
 * In addition, libseq now includes six of the objects/seq... libs, and libmmdb
 * replaces the three libmmdb? libs.
 *
 * Revision 1.11  2003/04/24 16:17:10  vasilche
 * Added '-repeat' option.
 * Updated includes.
 *
 * Revision 1.10  2003/04/15 14:25:06  vasilche
 * Added missing includes.
 *
 * Revision 1.9  2003/03/10 18:48:48  kuznets
 * iterate->ITERATE
 *
 * Revision 1.8  2002/11/08 19:43:34  grichenk
 * CConstRef<> constructor made explicit
 *
 * Revision 1.7  2002/11/04 21:29:01  grichenk
 * Fixed usage of const CRef<> and CRef<> constructor
 *
 * Revision 1.6  2002/08/30 18:10:20  ucko
 * Let CGBDataLoader pick a driver automatically rather than forcing it
 * to use ID1.
 *
 * Revision 1.5  2002/06/12 18:35:16  ucko
 * Take advantage of new CONNECT_Init() function.
 *
 * Revision 1.4  2002/05/31 13:51:31  grichenk
 * Added comment about iterate()
 *
 * Revision 1.3  2002/05/22 14:29:25  grichenk
 * +registry and logs setup; +iterating over CRef<> container
 *
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
