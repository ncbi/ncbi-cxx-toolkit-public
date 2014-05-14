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
* Author:  Colleen Bollin, NCBI
*
* File Description:
*   Unit tests for the field handlers.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include "unit_test_loc_edit.hpp"

#include <corelib/ncbi_system.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#define NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objects/biblio/Id_pat.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/GIBB_mol.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Ref_ext.hpp>
#include <objects/seq/Map_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/Giimport_id.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/valid/Comment_set.hpp>
#include <objects/valid/Comment_rule.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/unit_test_util/unit_test_util.hpp>
#include <corelib/ncbiapp.hpp>

#include <objtools/unit_test_util/unit_test_util.hpp>
#include <objtools/edit/loc_edit.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)





NCBITEST_INIT_TREE()
{
    if ( !CNcbiApplication::Instance()->GetConfig().HasEntry("NCBI", "Data") ) {
    }
}

static bool s_debugMode = false;

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddFlag( "debug_mode",
        "Debugging mode writes errors seen for each test" );
}

NCBITEST_AUTO_INIT()
{
    // initialization function body

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    if (args["debug_mode"]) {
        s_debugMode = true;
    }
}


void s_CheckLocationResults(const CSeq_loc& loc, bool partial5, bool partial3, TSeqPos start, TSeqPos stop)
{
    BOOST_CHECK_EQUAL(loc.IsPartialStart(eExtreme_Biological), partial5);
    BOOST_CHECK_EQUAL(loc.IsPartialStop(eExtreme_Biological), partial3);
    BOOST_CHECK_EQUAL(loc.GetStart(eExtreme_Biological), start);
    BOOST_CHECK_EQUAL(loc.GetStop(eExtreme_Biological), stop);

}


void s_CheckLocationPolicyResults(const CSeq_feat& cds, bool partial5, bool partial3, TSeqPos start, TSeqPos stop)
{
    s_CheckLocationResults(cds.GetLocation(), partial5, partial3, start, stop);
    BOOST_CHECK_EQUAL(cds.IsSetPartial(), (partial5 || partial3));
}


BOOST_AUTO_TEST_CASE(Test_ApplyPolicyToFeature)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    STANDARD_SETUP

    edit::CLocationEditPolicy policy(edit::CLocationEditPolicy::ePartialPolicy_eNoChange,
                                     edit::CLocationEditPolicy::ePartialPolicy_eNoChange, 
                                     false, false,
                                     edit::CLocationEditPolicy::eMergePolicy_NoChange);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), false);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, false, false, 0, 26);

    // both ends should be good, so there should be no change
    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), false);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, false, false, 0, 26);


    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eSet);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, true, false, 0, 26);

    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eClear);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eSet);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, false, true, 0, 26);

    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetAtEnd);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eClearNotAtEnd);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, true, false, 0, 26);

    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eClearForGoodEnd);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eSet);
    policy.SetExtend3(true);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, false, true, 0, 59);

    scope.RemoveTopLevelSeqEntry(seh);
    cds->SetLocation().SetInt().SetFrom(3);
    cds->SetLocation().SetInt().SetTo(26);
    seh = scope.AddTopLevelSeqEntry(*entry);
    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    policy.SetExtend5(true);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eClearForGoodEnd);
    policy.SetExtend3(true);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, true, false, 0, 26);

    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eClearForGoodEnd);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, false, false, 0, 26);

    // look for frame fixing
    scope.RemoveTopLevelSeqEntry(seh);
    cds->SetLocation().SetInt().SetFrom(1);
    cds->SetLocation().SetInt().SetTo(26);
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_two);
    seh = scope.AddTopLevelSeqEntry(*entry);
    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    policy.SetExtend5(true);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    policy.SetExtend3(true);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, true, false, 0, 26);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().GetFrame(), CCdregion::eFrame_not_set);

    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eClearForGoodEnd);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, false, false, 0, 26);

    scope.RemoveTopLevelSeqEntry(seh);
    cds->SetLocation().SetInt().SetFrom(2);
    cds->SetLocation().SetInt().SetTo(26);
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_three);
    seh = scope.AddTopLevelSeqEntry(*entry);
    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    policy.SetExtend5(true);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    policy.SetExtend3(true);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, true, false, 0, 26);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().GetFrame(), CCdregion::eFrame_not_set);

    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eClearForGoodEnd);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, false, false, 0, 26);

    scope.RemoveTopLevelSeqEntry(seh);
    cds->SetLocation().SetInt().SetFrom(3);
    cds->SetLocation().SetInt().SetTo(26);
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_not_set);
    seh = scope.AddTopLevelSeqEntry(*entry);
    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    policy.SetExtend5(true);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    policy.SetExtend3(true);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, true, false, 0, 26);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().GetFrame(), CCdregion::eFrame_not_set);


    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eClearForGoodEnd);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, false, false, 0, 26);

    scope.RemoveTopLevelSeqEntry(seh);
    cds->SetLocation().SetInt().SetFrom(1);
    cds->SetLocation().SetInt().SetTo(22);
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_not_set);
    seh = scope.AddTopLevelSeqEntry(*entry);
    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    policy.SetExtend5(true);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    policy.SetExtend3(true);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, true, true, 0, 59);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().GetFrame(), CCdregion::eFrame_three);
    
    // should not crash if Seq-entry not in scope
    scope.RemoveTopLevelSeqEntry(seh);
    CSeq_annot_Handle annot_handle = scope.AddSeq_annot(*(entry->GetAnnot().front()));
    cds->SetLocation().SetInt().SetFrom(1);
    cds->SetLocation().SetInt().SetTo(22);
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_not_set);
    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eClear);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eClear);
    policy.SetExtend5(false);
    policy.SetExtend3(false);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = annot_handle.GetCompleteSeq_annot()->GetData().GetFtable().front();
    s_CheckLocationPolicyResults(*cds, false, false, 1, 22);

    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eSet);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eSet);
    policy.SetExtend5(true);
    policy.SetExtend3(true);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = annot_handle.GetCompleteSeq_annot()->GetData().GetFtable().front();
    s_CheckLocationPolicyResults(*cds, true, true, 0, 22);

    cds->SetLocation().SetInt().SetFrom(1);
    cds->SetLocation().SetInt().SetTo(22);
    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eClear);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eClear);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = annot_handle.GetCompleteSeq_annot()->GetData().GetFtable().front();
    s_CheckLocationPolicyResults(*cds, false, false, 1, 22);

    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), false);
    cds = annot_handle.GetCompleteSeq_annot()->GetData().GetFtable().front();
    s_CheckLocationPolicyResults(*cds, false, false, 1, 22);

}


void s_CheckLocationAndStrandResults(const CSeq_loc& loc, bool partial5, bool partial3, TSeqPos start, TSeqPos stop, ENa_strand strand)
{
    s_CheckLocationResults(loc, partial5, partial3, start, stop);
    ENa_strand loc_strand = loc.GetStrand();
    if (strand == eNa_strand_minus) {
        BOOST_REQUIRE(loc_strand == eNa_strand_minus);
    } else {
        BOOST_REQUIRE(loc_strand == eNa_strand_plus || loc_strand == eNa_strand_unknown);
    }
}


BOOST_AUTO_TEST_CASE(Test_ReverseComplementLocation)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    STANDARD_SETUP

    CRef<objects::CSeq_feat> imp = unit_test_util::AddGoodImpFeat (entry, "misc_feature");

    CRef<CSeq_id> id(new CSeq_id());
    id->Assign(*(imp->GetLocation().GetId()));

    // interval
    imp->SetLocation().SetInt().SetFrom(0);
    imp->SetLocation().SetInt().SetTo(15);
    edit::ReverseComplementFeature(*imp, scope);
    s_CheckLocationAndStrandResults(imp->GetLocation(), false, false,
                                    entry->GetSeq().GetInst().GetLength() - 1,
                                    entry->GetSeq().GetInst().GetLength() - 16,
                                    eNa_strand_minus);

    // with one end partial
    imp->SetLocation().SetPartialStart(true, eExtreme_Biological);
    edit::ReverseComplementFeature(*imp, scope);
    s_CheckLocationAndStrandResults(imp->GetLocation(), true, false, 0, 15, eNa_strand_plus);

    // point
    imp->SetLocation().SetPnt().SetId().Assign(*id);
    imp->SetLocation().SetPnt().SetPoint(15);
    edit::ReverseComplementFeature(*imp, scope);
    s_CheckLocationAndStrandResults(imp->GetLocation(), false, false,
                                    entry->GetSeq().GetInst().GetLength() - 16,
                                    entry->GetSeq().GetInst().GetLength() - 16,
                                    eNa_strand_minus);

    // mix
    CRef<objects::CSeq_loc> mix_loc = unit_test_util::MakeMixLoc (id);
    edit::ReverseComplementLocation(*mix_loc, scope);
    s_CheckLocationAndStrandResults(*mix_loc, false, false, 
                                    entry->GetSeq().GetInst().GetLength() - 1,
                                    entry->GetSeq().GetInst().GetLength() - 57,
                                    eNa_strand_minus);
    // check internal intervals
    s_CheckLocationAndStrandResults(*(mix_loc->GetMix().Get().front()), false, false,
                                    entry->GetSeq().GetInst().GetLength() - 1,
                                    entry->GetSeq().GetInst().GetLength() - 16,
                                    eNa_strand_minus);
    s_CheckLocationAndStrandResults(*(mix_loc->GetMix().Get().back()), false, false,
                                    entry->GetSeq().GetInst().GetLength() - 47,
                                    entry->GetSeq().GetInst().GetLength() - 57,
                                    eNa_strand_minus);

    // with one end partial
    mix_loc->SetPartialStop(true, eExtreme_Biological);
    edit::ReverseComplementLocation(*mix_loc, scope);
    s_CheckLocationAndStrandResults(*mix_loc, false, true, 0, 56, eNa_strand_plus);
    // check internal intervals
    s_CheckLocationAndStrandResults(*(mix_loc->GetMix().Get().front()), false, false, 0, 15, eNa_strand_plus);
    s_CheckLocationAndStrandResults(*(mix_loc->GetMix().Get().back()), false, true, 46, 56, eNa_strand_plus);

    // packed point
    CRef<CSeq_loc> packed_pnt(new CSeq_loc());
    packed_pnt->SetPacked_pnt().SetId().Assign(*id);
    packed_pnt->SetPacked_pnt().SetPoints().push_back(0);
    packed_pnt->SetPacked_pnt().SetPoints().push_back(15);
    packed_pnt->SetPacked_pnt().SetPoints().push_back(46);
    packed_pnt->SetPacked_pnt().SetPoints().push_back(56);
    edit::ReverseComplementLocation(*packed_pnt, scope);
    ENa_strand strand = packed_pnt->GetPacked_pnt().GetStrand();
    BOOST_REQUIRE(strand == eNa_strand_minus);
    BOOST_CHECK_EQUAL(packed_pnt->GetPacked_pnt().GetPoints().front(), entry->GetSeq().GetInst().GetLength() - 1);
    BOOST_CHECK_EQUAL(packed_pnt->GetPacked_pnt().GetPoints().back(), entry->GetSeq().GetInst().GetLength() - 57);
}


BOOST_AUTO_TEST_CASE(Test_ReverseComplementFeature)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    STANDARD_SETUP

    // check CDS code break
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationAndStrandResults(cds->GetLocation(), false, false, 0, 26, eNa_strand_plus);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStart(eExtreme_Biological), 0);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), 26);
    CRef<CCode_break> cb(new CCode_break());
    CRef<CSeq_id> id(new CSeq_id());
    id->Assign(*(cds->GetLocation().GetId()));
    cb->SetLoc().SetInt().SetId(*id);
    cb->SetLoc().SetInt().SetFrom(3);
    cb->SetLoc().SetInt().SetTo(5);
    cds->SetData().SetCdregion().SetCode_break().push_back(cb);
    edit::ReverseComplementFeature(*cds, scope);
    s_CheckLocationAndStrandResults(cds->GetLocation(), false, false, 59, 33, eNa_strand_minus);
    s_CheckLocationAndStrandResults(cb->GetLoc(), false, false, 56, 54, eNa_strand_minus);

    // check tRNA anticodon
    CRef<objects::CSeq_feat> trna = unit_test_util::BuildGoodtRNA(id);
    s_CheckLocationAndStrandResults(trna->GetLocation(), false, false, 0, 10, eNa_strand_plus);
    s_CheckLocationAndStrandResults(trna->GetData().GetRna().GetExt().GetTRNA().GetAnticodon(),
                                    false, false, 8, 10, eNa_strand_plus);
    edit::ReverseComplementFeature(*trna, scope);
    s_CheckLocationAndStrandResults(trna->GetLocation(), false, false, 59, 49, eNa_strand_minus);
    s_CheckLocationAndStrandResults(trna->GetData().GetRna().GetExt().GetTRNA().GetAnticodon(),
                                    false, false, 51, 49, eNa_strand_minus);

}


END_SCOPE(objects)
END_NCBI_SCOPE

