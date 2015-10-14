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

#include <common/test_assert.h>  /* This header must go last */


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


BOOST_AUTO_TEST_CASE(Test_SeqLocPrintUseBestID)
{
  CRef <CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
  CRef <CSeq_entry> 
     prot_entry = unit_test_util::GetProteinSequenceFromGoodNucProtSet(entry);

  // add seq_ids
  CRef <CSeq_id> sid (new CSeq_id);
  sid->SetGeneral().SetDb("BankIt");
  sid->SetGeneral().SetTag().SetStr("1572628/Seq1");
  prot_entry->SetSeq().SetId().push_back(sid);
  sid.Reset(new CSeq_id);
  sid->SetGeneral().SetDb("TMSMART");
  sid->SetGeneral().SetTag().SetId(33590036);
  prot_entry->SetSeq().SetId().push_back(sid);
  sid.Reset(new CSeq_id);
  sid->SetGenbank().SetAccession("JX983656");
  prot_entry->SetSeq().SetId().push_back(sid);

  // make new feat with new type of seq_loc
  unit_test_util :: AddProtFeat(prot_entry);
  CRef <CSeq_feat> new_feat = prot_entry->SetSeq().SetAnnot().front()->SetData().SetFtable().back();
  CRef <CSeq_loc> new_seq_loc = unit_test_util::MakeMixLoc(sid);
  new_feat->SetLocation(*new_seq_loc);

  // imp feat with a CSeq_pnt seq_loc
  unit_test_util :: AddGoodImpFeat(prot_entry, "fake imp");
  new_feat.Reset(prot_entry->SetSeq().SetAnnot().front()->SetData().SetFtable().back());
  new_seq_loc.Reset(new CSeq_loc);
  CRef <CSeq_point> pnt (new CSeq_point);
  pnt->SetPoint(102);
  pnt->SetStrand(eNa_strand_minus);
  pnt->SetId().SetLocal().SetStr("prot");
  new_seq_loc->SetPnt(*pnt);
  new_feat->SetLocation(*new_seq_loc);
  
  CRef <CScope> scope(new CScope(*CObjectManager::GetInstance()));
  CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

  NON_CONST_ITERATE (list <CRef <CSeq_feat> >, it, prot_entry->SetSeq().SetAnnot().front()->SetData().SetFtable()) {
  CSeq_loc& seq_loc = (*it)->SetLocation();
  if (seq_loc.IsInt()) {
      seq_loc.SetInt().SetStrand(eNa_strand_minus);
  }
  string strtmp = edit::SeqLocPrintUseBestID(seq_loc, *scope);
  NCBITEST_CHECK_MESSAGE(!strtmp.empty(), "No seq-loc string printed out");
}

// cout << MSerial_AsnText << *entry << endl;
  
};

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
    // the protein sequence before and after the coding region extension should be the same
    scope.RemoveTopLevelSeqEntry(seh);
    cds->SetLocation().SetInt().SetFrom(2);
    cds->SetLocation().SetInt().SetTo(26);
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_two);
    seh = scope.AddTopLevelSeqEntry(*entry);
    string orig_prot_seq, new_prot_seq;
    CSeqTranslator::Translate(*cds, scope, orig_prot_seq);
    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    policy.SetExtend5(true);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    policy.SetExtend3(true);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, true, false, 0, 26);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().GetFrame(), CCdregion::eFrame_not_set);
    CSeqTranslator::Translate(*cds, scope, new_prot_seq);
    BOOST_CHECK_EQUAL(string("M") + orig_prot_seq, new_prot_seq);
    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eClearForGoodEnd);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, false, false, 0, 26);
    

    scope.RemoveTopLevelSeqEntry(seh);
    cds->SetLocation().SetInt().SetFrom(1);
    cds->SetLocation().SetInt().SetTo(26);
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_three);
    seh = scope.AddTopLevelSeqEntry(*entry);
    orig_prot_seq = new_prot_seq = kEmptyStr;
    CSeqTranslator::Translate(*cds, scope, orig_prot_seq);
    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    policy.SetExtend5(true);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd);
    policy.SetExtend3(true);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, true, true), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    s_CheckLocationPolicyResults(*cds, true, false, 0, 26);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().GetFrame(), CCdregion::eFrame_not_set);
    CSeqTranslator::Translate(*cds, scope, new_prot_seq);
    BOOST_CHECK_EQUAL(string("M") + orig_prot_seq, new_prot_seq);
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
    cds->SetLocation().SetInt().SetFrom(2);
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

    scope.RemoveTopLevelSeqEntry(seh);
    cds->SetLocation().SetInt().SetFrom(2);
    cds->SetLocation().SetInt().SetTo(25);
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_one);
    seh = scope.AddTopLevelSeqEntry(*entry);
    orig_prot_seq = new_prot_seq = kEmptyStr;
    CSeqTranslator::Translate(*cds, scope, orig_prot_seq);
    policy.SetPartial5Policy(edit::CLocationEditPolicy::ePartialPolicy_eSet);
    policy.SetExtend5(true);
    policy.SetPartial3Policy(edit::CLocationEditPolicy::ePartialPolicy_eNoChange);
    policy.SetExtend3(false);
    BOOST_CHECK_EQUAL(edit::ApplyPolicyToFeature(policy, *cds, scope, false, false), true);
    cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().GetFrame(), CCdregion::eFrame_three);
    CSeqTranslator::Translate(*cds, scope, new_prot_seq);
    BOOST_CHECK_EQUAL(orig_prot_seq, new_prot_seq);

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


void s_CheckInterval(const CSeq_interval& interval, TSeqPos from, TSeqPos to)
{
    BOOST_CHECK_EQUAL(interval.GetFrom(), from);
    BOOST_CHECK_EQUAL(interval.GetTo(), to);
}


void s_MakePackedInt(CSeq_loc& loc, const CSeq_id& id, vector<TSeqPos> from, vector<TSeqPos> to)
{
    vector<TSeqPos>::iterator from_it = from.begin();
    vector<TSeqPos>::iterator to_it = to.begin();

    loc.SetPacked_int().Set().clear();

    while (from_it != from.end()) {
        CRef<CSeq_interval> interval(new CSeq_interval());
        interval->SetId().Assign(id);
        interval->SetFrom(*from_it);
        interval->SetTo(*to_it);
        loc.SetPacked_int().Set().push_back(interval);
        ++from_it;
        ++to_it;
    }
}


void s_CheckPackedInt(const CPacked_seqint& pack, vector<TSeqPos> from, vector<TSeqPos> to)
{
    BOOST_CHECK_EQUAL(pack.Get().size(), from.size());
    BOOST_CHECK_EQUAL(pack.Get().size(), to.size());
    CPacked_seqint::Tdata::const_iterator lit = pack.Get().begin();
    vector<TSeqPos>::iterator from_it = from.begin();
    vector<TSeqPos>::iterator to_it = to.begin();
    while (lit != pack.Get().end()) {
        s_CheckInterval(**lit, *from_it, *to_it);
        ++lit;
        ++from_it;
        ++to_it;
    }

}


void s_MakeMixLoc(CSeq_loc& loc, const CSeq_id& id, vector<TSeqPos> from, vector<TSeqPos> to)
{
    vector<TSeqPos>::iterator from_it = from.begin();
    vector<TSeqPos>::iterator to_it = to.begin();

    loc.SetMix().Set().clear();

    while (from_it != from.end()) {
        CRef<CSeq_loc> sub(new CSeq_loc());
        if (*from_it == *to_it) {
            sub->SetPnt().SetId().Assign(id);
            sub->SetPnt().SetPoint(*from_it);
        } else {
            sub->SetInt().SetId().Assign(id);
            sub->SetInt().SetFrom(*from_it);
            sub->SetInt().SetTo(*to_it);
        }
        loc.SetMix().Set().push_back(sub);
        ++from_it;
        ++to_it;
    }
}


void s_CheckMixLoc(const CSeq_loc& mix, vector<TSeqPos> from, vector<TSeqPos> to)
{
    CSeq_loc_CI loc_i(mix);
    vector<TSeqPos>::iterator from_it = from.begin();
    vector<TSeqPos>::iterator to_it = to.begin();
    while (loc_i) {
        BOOST_CHECK_EQUAL(loc_i.GetRange().GetFrom(), *from_it);
        BOOST_CHECK_EQUAL(loc_i.GetRange().GetTo(), *to_it);
        ++loc_i;
        ++from_it;
        ++to_it;
    }

}


BOOST_AUTO_TEST_CASE(Test_AdjustForTrim)
{
    CRef<CSeq_loc> loc(new CSeq_loc());
    CRef<CSeq_id> id(new CSeq_id());
    id->SetLocal().SetStr("nuc1");

    // Test for Seq-interval
    loc->SetInt().SetId().Assign(*id);
    loc->SetInt().SetFrom(10);
    loc->SetInt().SetTo(50);

    bool cut = false;
    TSeqPos trim5 = 0;
    bool adjusted = false;

    // cut to the right, should have no change
    edit::SeqLocAdjustForTrim(*loc, 75, 100, id.GetPointer(), cut, trim5, adjusted); 
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, false);
    BOOST_CHECK_EQUAL(trim5, 0);
    s_CheckInterval(loc->GetInt(), 10, 50);

    // cut to the left, should shift left
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 0, 4, id.GetPointer(), cut, trim5, adjusted); 
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, true);
    BOOST_CHECK_EQUAL(trim5, 0);
    s_CheckInterval(loc->GetInt(), 5, 45);

    // cut in the middle, shift right but not left
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 10, 19, id.GetPointer(), cut, trim5, adjusted); 
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, true);
    BOOST_CHECK_EQUAL(trim5, 0);
    s_CheckInterval(loc->GetInt(), 5, 35);

    // cut on left end
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 0, 9, id.GetPointer(), cut, trim5, adjusted); 
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, true);
    BOOST_CHECK_EQUAL(trim5, 5);
    s_CheckInterval(loc->GetInt(), 0, 25);

    // cut on right end
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 20, 25, id.GetPointer(), cut, trim5, adjusted); 
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, true);
    BOOST_CHECK_EQUAL(trim5, 0);
    s_CheckInterval(loc->GetInt(), 0, 19);

    // cut entire feature
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 0, 25, id.GetPointer(), cut, trim5, adjusted); 
    BOOST_CHECK_EQUAL(cut, true);

    // Test for point
    loc->SetPnt().SetId().Assign(*id);
    loc->SetPnt().SetPoint(10);

    // cut to right, no change
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 20, 30, id.GetPointer(), cut, trim5, adjusted); 
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, false);
    BOOST_CHECK_EQUAL(trim5, 0);
    BOOST_CHECK_EQUAL(loc->GetPnt().GetPoint(), 10);

    // cut to left, shift
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 0, 4, id.GetPointer(), cut, trim5, adjusted); 
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, true);
    BOOST_CHECK_EQUAL(trim5, 0);
    BOOST_CHECK_EQUAL(loc->GetPnt().GetPoint(), 5);

    // cut point
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 0, 10, id.GetPointer(), cut, trim5, adjusted); 
    BOOST_CHECK_EQUAL(cut, true);
    BOOST_CHECK_EQUAL(trim5, 1);

    // Test for Packed-int
    vector<TSeqPos> from;
    vector<TSeqPos> to;
    from.push_back(10);
    from.push_back(50);
    from.push_back(90);
    to.push_back(30);
    to.push_back(70);
    to.push_back(110);

    s_MakePackedInt(*loc, *id, from, to);

    // cut to the right, no change
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 120, 130, id.GetPointer(), cut, trim5, adjusted); 
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, false);
    BOOST_CHECK_EQUAL(trim5, 0);
    s_CheckPackedInt(loc->GetPacked_int(), from, to);

    // cut portion of right end
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 106, 120, id.GetPointer(), cut, trim5, adjusted); 
    to[2] = 105;
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, true);
    BOOST_CHECK_EQUAL(trim5, 0);
    s_CheckPackedInt(loc->GetPacked_int(), from, to);

    // cut left portion of last interval
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 85, 94, id.GetPointer(), cut, trim5, adjusted); 
    from[2] = 85;
    to[2] = 95;
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, true);
    BOOST_CHECK_EQUAL(trim5, 0);
    s_CheckPackedInt(loc->GetPacked_int(), from, to);

    // cut between intervals
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 72, 76, id.GetPointer(), cut, trim5, adjusted); 
    from[2] = 80;
    to[2] = 90;
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, true);
    BOOST_CHECK_EQUAL(trim5, 0);
    s_CheckPackedInt(loc->GetPacked_int(), from, to);

    // cut entire middle interval
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 50, 70, id.GetPointer(), cut, trim5, adjusted);
    from.pop_back();
    to.pop_back();
    from[1] = 59;
    to[1] = 69;
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, true);
    BOOST_CHECK_EQUAL(trim5, 0);
    s_CheckPackedInt(loc->GetPacked_int(), from, to);
    
    // cut left portion of first interval
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 5, 14, id.GetPointer(), cut, trim5, adjusted);
    from.clear();
    to.clear();
    from.push_back(5);
    from.push_back(49);
    to.push_back(20);
    to.push_back(59);
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, true);
    BOOST_CHECK_EQUAL(trim5, 5);
    s_CheckPackedInt(loc->GetPacked_int(), from, to);

    // cut entire location
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 5, 59, id.GetPointer(), cut, trim5, adjusted);
    BOOST_CHECK_EQUAL(cut, true);

    // Test for Seq-loc-mix
    from.clear();
    to.clear();
    from.push_back(10);
    from.push_back(50);
    from.push_back(90);
    to.push_back(30);
    to.push_back(70);
    to.push_back(110);

    s_MakeMixLoc(*loc, *id, from, to);

    // cut to the right, no change
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 120, 130, id.GetPointer(), cut, trim5, adjusted); 
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, false);
    BOOST_CHECK_EQUAL(trim5, 0);
    s_CheckMixLoc(*loc, from, to);

    // cut portion of right end
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 106, 120, id.GetPointer(), cut, trim5, adjusted); 
    to[2] = 105;
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, true);
    BOOST_CHECK_EQUAL(trim5, 0);
    s_CheckMixLoc(*loc, from, to);

    // cut left portion of last interval
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 85, 94, id.GetPointer(), cut, trim5, adjusted); 
    from[2] = 85;
    to[2] = 95;
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, true);
    BOOST_CHECK_EQUAL(trim5, 0);
    s_CheckMixLoc(*loc, from, to);

    // cut between intervals
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 72, 76, id.GetPointer(), cut, trim5, adjusted); 
    from[2] = 80;
    to[2] = 90;
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, true);
    BOOST_CHECK_EQUAL(trim5, 0);
    s_CheckMixLoc(*loc, from, to);

    // cut entire middle interval
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 50, 70, id.GetPointer(), cut, trim5, adjusted);
    from.pop_back();
    to.pop_back();
    from[1] = 59;
    to[1] = 69;
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, true);
    BOOST_CHECK_EQUAL(trim5, 0);
    s_CheckMixLoc(*loc, from, to);
    
    // cut left portion of first interval
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 5, 14, id.GetPointer(), cut, trim5, adjusted);
    from.clear();
    to.clear();
    from.push_back(5);
    from.push_back(49);
    to.push_back(20);
    to.push_back(59);
    BOOST_CHECK_EQUAL(cut, false);
    BOOST_CHECK_EQUAL(adjusted, true);
    BOOST_CHECK_EQUAL(trim5, 5);
    s_CheckMixLoc(*loc, from, to);

    // cut entire location
    cut = false;
    trim5 = 0;
    adjusted = false;
    edit::SeqLocAdjustForTrim(*loc, 5, 59, id.GetPointer(), cut, trim5, adjusted);
    BOOST_CHECK_EQUAL(cut, true);
    
}


BOOST_AUTO_TEST_CASE(Test_AdjustForInsert)
{
    CRef<CSeq_loc> loc(new CSeq_loc());
    CRef<CSeq_id> id(new CSeq_id());
    id->SetLocal().SetStr("nuc1");

    // Test for Seq-interval
    loc->SetInt().SetId().Assign(*id);
    loc->SetInt().SetFrom(10);
    loc->SetInt().SetTo(50);

    // insert to right, no change
    edit::SeqLocAdjustForInsert(*loc, 75, 100, id.GetPointer()); 
    s_CheckInterval(loc->GetInt(), 10, 50);

    // insert in middle, move right end
    edit::SeqLocAdjustForInsert(*loc, 20, 29, id.GetPointer()); 
    s_CheckInterval(loc->GetInt(), 10, 60);

    // insert to left, shift both endpoints
    edit::SeqLocAdjustForInsert(*loc, 5, 9, id.GetPointer()); 
    s_CheckInterval(loc->GetInt(), 15, 65);

    // Test for point
    loc->SetPnt().SetId().Assign(*id);
    loc->SetPnt().SetPoint(10);

    // insert to right, no change
    edit::SeqLocAdjustForInsert(*loc, 75, 100, id.GetPointer()); 
    BOOST_CHECK_EQUAL(loc->GetPnt().GetPoint(), 10);
   
    // insert to left, shift
    edit::SeqLocAdjustForInsert(*loc, 5, 7, id.GetPointer()); 
    BOOST_CHECK_EQUAL(loc->GetPnt().GetPoint(), 13);


    // Test for Packed-int
    vector<TSeqPos> from;
    vector<TSeqPos> to;
    from.push_back(10);
    from.push_back(50);
    from.push_back(90);
    to.push_back(30);
    to.push_back(70);
    to.push_back(110);
    s_MakePackedInt(*loc, *id, from, to);

    // insert to right, no change
    edit::SeqLocAdjustForInsert(*loc, 120, 130, id.GetPointer()); 
    s_CheckPackedInt(loc->GetPacked_int(), from, to);

    // insert to left, shift all
    edit::SeqLocAdjustForInsert(*loc, 1, 5, id.GetPointer()); 
    for (vector<TSeqPos>::iterator it = from.begin(); it != from.end(); ++it) {
        (*it) += 5;
    }
    for (vector<TSeqPos>::iterator it = to.begin(); it != to.end(); ++it) {
        (*it) += 5;
    }
    s_CheckPackedInt(loc->GetPacked_int(), from, to);

    // insert between last two intervals
    edit::SeqLocAdjustForInsert(*loc, 80, 84, id.GetPointer());
    from.back() += 5;
    to.back() += 5;
    s_CheckPackedInt(loc->GetPacked_int(), from, to);

    // insert inside first interval
    edit::SeqLocAdjustForInsert(*loc, 20, 24, id.GetPointer());
    vector<TSeqPos>::iterator fit = from.begin();
    fit++;
    while (fit != from.end()) {
        (*fit) += 5;
        fit++;
    }
    for (vector<TSeqPos>::iterator it = to.begin(); it != to.end(); ++it) {
        (*it) += 5;
    }
    s_CheckPackedInt(loc->GetPacked_int(), from, to);

    // Test for Seq-loc-mix
    from.clear();
    to.clear();
    from.push_back(10);
    from.push_back(50);
    from.push_back(90);
    to.push_back(30);
    to.push_back(70);
    to.push_back(110);
    s_MakeMixLoc(*loc, *id, from, to);

    // insert to right, no change
    edit::SeqLocAdjustForInsert(*loc, 120, 130, id.GetPointer()); 
    s_CheckMixLoc(*loc, from, to);

    // insert to left, shift all
    edit::SeqLocAdjustForInsert(*loc, 1, 5, id.GetPointer()); 
    for (vector<TSeqPos>::iterator it = from.begin(); it != from.end(); ++it) {
        (*it) += 5;
    }
    for (vector<TSeqPos>::iterator it = to.begin(); it != to.end(); ++it) {
        (*it) += 5;
    }
    s_CheckMixLoc(*loc, from, to);

    // insert between last two intervals
    edit::SeqLocAdjustForInsert(*loc, 80, 84, id.GetPointer());
    from.back() += 5;
    to.back() += 5;
    s_CheckMixLoc(*loc, from, to);

    // insert inside first interval
    edit::SeqLocAdjustForInsert(*loc, 20, 24, id.GetPointer());
    fit = from.begin();
    fit++;
    while (fit != from.end()) {
        (*fit) += 5;
        fit++;
    }
    for (vector<TSeqPos>::iterator it = to.begin(); it != to.end(); ++it) {
        (*it) += 5;
    }
    s_CheckMixLoc(*loc, from, to);


}


BOOST_AUTO_TEST_CASE(Test_SplitLocationForGap)
{
    CRef<CSeq_loc> loc1(new CSeq_loc());
    CRef<CSeq_id> id(new CSeq_id());
    id->SetLocal().SetStr("nuc1");
    CRef<CSeq_loc> loc2(new CSeq_loc());

    unsigned int options = edit::eSplitLocOption_make_partial
                         | edit::eSplitLocOption_split_in_exon
                         | edit::eSplitLocOption_split_in_intron;

    // Test for Seq-interval
    loc1->SetInt().SetId().Assign(*id);
    loc1->SetInt().SetFrom(10);
    loc1->SetInt().SetTo(50);

    // gap to right, no change   
    edit::SplitLocationForGap(*loc1, *loc2, 75, 100, id.GetPointer(), options); 
    s_CheckInterval(loc1->GetInt(), 10, 50);
    BOOST_CHECK_EQUAL(loc2->Which(), CSeq_loc::e_not_set);

    // gap in middle
    edit::SplitLocationForGap(*loc1, *loc2, 20, 29, id.GetPointer(), options); 
    s_CheckInterval(loc1->GetInt(), 10, 19);
    s_CheckInterval(loc2->GetInt(), 30, 50);

    // gap to right, move to loc2
    loc1->SetInt().SetId().Assign(*id);
    loc1->SetInt().SetFrom(10);
    loc1->SetInt().SetTo(50);
    loc2->Reset();
    edit::SplitLocationForGap(*loc1, *loc2, 5, 9, id.GetPointer(), options); 
    BOOST_CHECK_EQUAL(loc1->Which(), CSeq_loc::e_not_set);
    s_CheckInterval(loc2->GetInt(), 10, 50);

    // Test for point
    loc1->SetPnt().SetId().Assign(*id);
    loc1->SetPnt().SetPoint(10);
    loc2->Reset();

    // gap to right, no change
    edit::SplitLocationForGap(*loc1, *loc2, 75, 100, id.GetPointer(), options); 
    BOOST_CHECK_EQUAL(loc1->GetPnt().GetPoint(), 10);
    BOOST_CHECK_EQUAL(loc2->Which(), CSeq_loc::e_not_set);
   
    // gap to left, move to loc2
    edit::SplitLocationForGap(*loc1, *loc2, 5, 7, id.GetPointer(), options);
    BOOST_CHECK_EQUAL(loc1->Which(), CSeq_loc::e_not_set);
    BOOST_CHECK_EQUAL(loc2->GetPnt().GetPoint(), 10);

    // Test for Packed-int
    vector<TSeqPos> from;
    vector<TSeqPos> to;
    from.push_back(10);
    from.push_back(50);
    from.push_back(90);
    to.push_back(30);
    to.push_back(70);
    to.push_back(110);
    s_MakePackedInt(*loc1, *id, from, to);
    loc2->Reset();

    // gap to right, no change
    edit::SplitLocationForGap(*loc1, *loc2, 120, 130, id.GetPointer(), options); 
    s_CheckPackedInt(loc1->GetPacked_int(), from, to);
    BOOST_CHECK_EQUAL(loc2->Which(), CSeq_loc::e_not_set);

    // gap to left, move to loc2
    edit::SplitLocationForGap(*loc1, *loc2, 1, 5, id.GetPointer(), options); 
    BOOST_CHECK_EQUAL(loc1->Which(), CSeq_loc::e_not_set);
    s_CheckPackedInt(loc2->GetPacked_int(), from, to);

    // gap between last two intervals
    s_MakePackedInt(*loc1, *id, from, to);
    loc2->Reset();
    edit::SplitLocationForGap(*loc1, *loc2, 80, 84, id.GetPointer(), options);

    TSeqPos after_from;
    TSeqPos after_to;
    after_from = from.back();
    after_to = to.back();
    from.pop_back();
    to.pop_back();

    s_CheckPackedInt(loc1->GetPacked_int(), from, to);
    s_CheckInterval(loc2->GetInt(), after_from, after_to);

    // gap inside first interval
    from.clear();
    to.clear();
    from.push_back(10);
    from.push_back(50);
    from.push_back(90);
    to.push_back(30);
    to.push_back(70);
    to.push_back(110);
    s_MakePackedInt(*loc1, *id, from, to);
    loc2->Reset();

    edit::SplitLocationForGap(*loc1, *loc2, 20, 24, id.GetPointer(), options);
    TSeqPos before_from = from[0];
    TSeqPos before_to = 19;
    from[0] = 25;
    s_CheckInterval(loc1->GetInt(), before_from, before_to);
    s_CheckPackedInt(loc2->GetPacked_int(), from, to);

    // Test for Seq-loc-mix
    from.clear();
    to.clear();
    from.push_back(10);
    from.push_back(50);
    from.push_back(90);
    to.push_back(30);
    to.push_back(70);
    to.push_back(110);
    s_MakeMixLoc(*loc1, *id, from, to);
    loc2->Reset();

    // gap to right, no change
    edit::SplitLocationForGap(*loc1, *loc2, 120, 130, id.GetPointer(), options); 
    s_CheckMixLoc(*loc1, from, to);
    BOOST_CHECK_EQUAL(loc2->Which(), CSeq_loc::e_not_set);

    // gap to left, move to loc2
    edit::SplitLocationForGap(*loc1, *loc2, 1, 5, id.GetPointer(), options); 
    BOOST_CHECK_EQUAL(loc1->Which(), CSeq_loc::e_not_set);
    s_CheckMixLoc(*loc2, from, to);

    // gap between last two intervals
    s_MakeMixLoc(*loc1, *id, from, to);
    loc2->Reset();
    edit::SplitLocationForGap(*loc1, *loc2, 80, 84, id.GetPointer(), options);

    after_from = from.back();
    after_to = to.back();
    from.pop_back();
    to.pop_back();

    s_CheckMixLoc(*loc1, from, to);
    s_CheckInterval(loc2->GetInt(), after_from, after_to);

    // gap inside first interval
    from.clear();
    to.clear();
    from.push_back(10);
    from.push_back(50);
    from.push_back(90);
    to.push_back(30);
    to.push_back(70);
    to.push_back(110);
    s_MakeMixLoc(*loc1, *id, from, to);
    loc2->Reset();

    edit::SplitLocationForGap(*loc1, *loc2, 20, 24, id.GetPointer(), options);
    before_from = from[0];
    before_to = 19;
    from[0] = 25;
    s_CheckInterval(loc1->GetInt(), before_from, before_to);
    s_CheckMixLoc(*loc2, from, to);

}


END_SCOPE(objects)
END_NCBI_SCOPE

