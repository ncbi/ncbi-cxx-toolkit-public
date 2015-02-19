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
* Author:  Colleen Bollin, Jie Chen, NCBI
*
* File Description:
*   Unit tests for the field handlers.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include "unit_test_cds_fix.hpp"

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
#include <objects/macro/Simple_replace.hpp>
#include <objects/macro/Replace_func.hpp>
#include <objects/macro/Replace_rule.hpp>
#include <objects/macro/Search_func.hpp>
#include <objects/macro/String_constraint.hpp>
#include <objects/macro/Suspect_rule.hpp>
#include <objects/macro/Word_substitution.hpp>
#include <objects/macro/Word_substitution_set.hpp>
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
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
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
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/unit_test_util/unit_test_util.hpp>
#include <corelib/ncbiapp.hpp>

#include <objtools/unit_test_util/unit_test_util.hpp>
#include <objtools/edit/cds_fix.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


extern const char* sc_TestEntry;


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


void CheckTerminalExceptionResults (CSeq_feat& cds, CScope& scope,
                  bool strict, bool extend,
                  bool expected_rval, bool set_codebreak, 
                  bool set_comment, TSeqPos expected_endpoint)
{
    const CCdregion& cdr = cds.GetData().GetCdregion();
    BOOST_CHECK_EQUAL(edit::SetTranslExcept(cds, 
                                            "TAA stop codon is completed by the addition of 3' A residues to the mRNA",
                                            strict, extend, scope),
                      expected_rval);
    BOOST_CHECK_EQUAL(cdr.IsSetCode_break(), set_codebreak);
    if (set_codebreak) {
        BOOST_CHECK_EQUAL(cdr.GetCode_break().size(), 1);
    }

    BOOST_CHECK_EQUAL(cds.IsSetComment(), set_comment);
    if (set_comment) {
        BOOST_CHECK_EQUAL(cds.GetComment(), "TAA stop codon is completed by the addition of 3' A residues to the mRNA");
    }
    BOOST_CHECK_EQUAL(cds.GetLocation().GetStop(eExtreme_Biological), expected_endpoint);
}


void OneTerminalTranslationExceptionTest(bool strict, bool extend, TSeqPos endpoint,
                                         const string& seq,
                                         bool expected_rval, bool set_codebreak, bool set_comment,                                         
                                         TSeqPos expected_endpoint)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    STANDARD_SETUP

    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    CCdregion& cdr = cds->SetData().SetCdregion();
    CBioseq& nuc_seq = entry->SetSet().SetSeq_set().front()->SetSeq();
    nuc_seq.SetInst().SetSeq_data().SetIupacna().Set(seq);
    cds->SetLocation().SetInt().SetTo(endpoint);
    
    // Should not set translation exception if coding region already has stop codon
    CheckTerminalExceptionResults(*cds, seh.GetScope(),
                                  strict, extend, expected_rval, 
                                  set_codebreak, set_comment, expected_endpoint);

    cdr.ResetCode_break();
    cds->ResetComment();
    cds->SetLocation().SetInt().SetTo(endpoint);

    // same results if reverse-complement
    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::RevComp(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    CheckTerminalExceptionResults(*cds, seh.GetScope(),
                                  strict, extend, expected_rval, 
                                  set_codebreak, set_comment, 
                                  nuc_seq.GetLength() - expected_endpoint - 1);
}


BOOST_AUTO_TEST_CASE(Test_AddTerminalTranslationException)
{
    string original_seq = "ATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG";
    // no change if normal
    OneTerminalTranslationExceptionTest(true, true, 26, 
                                        original_seq,
                                        false, false, false, 26);

    // should not set translation exception, but should extend to cover stop codon if extend is true
    OneTerminalTranslationExceptionTest(true, true, 23, 
                                        original_seq,
                                        true, false, false, 26);

    // but no change if extend flag is false
    OneTerminalTranslationExceptionTest(true, false, 23, 
                                        original_seq,
                                        false, false, false, 23);

    // should be set if last A in stop codon is replaced with other NT and coding region is one shorter
    string changed_seq = original_seq;
    changed_seq[26] = 'C';
    OneTerminalTranslationExceptionTest(true, true, 25, 
                                        changed_seq,
                                        true, true, true, 25);

    // should extend for partial stop codon and and add terminal exception if coding region missing
    // entire last codon
    OneTerminalTranslationExceptionTest(true, true, 23, 
                                        changed_seq,
                                        true, true, true, 25);

    // for non-strict, first NT could be N
    changed_seq[24] = 'N';
    OneTerminalTranslationExceptionTest(false, true, 25, 
                                        changed_seq,
                                        true, true, true, 25);
    // but not for strict
    OneTerminalTranslationExceptionTest(true, true, 23, 
                                        changed_seq,
                                        false, false, false, 23);


}


BOOST_AUTO_TEST_CASE(Test_FeaturePartialSynchronization)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    STANDARD_SETUP

    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    CRef<CSeq_entry> prot_seq = unit_test_util::GetProteinSequenceFromGoodNucProtSet (entry);
    CRef<CSeq_feat> prot_feat = unit_test_util::GetProtFeatFromGoodNucProtSet (entry);
    CRef<CSeqdesc> prot_molinfo;
    NON_CONST_ITERATE(CBioseq::TDescr::Tdata, it, prot_seq->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            prot_molinfo.Reset(it->GetPointer());
        }
    }

    // establish that everything is ok before
    BOOST_CHECK_EQUAL(cds->GetLocation().IsPartialStart(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(cds->GetLocation().IsPartialStop(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(cds->IsSetPartial(), false);
    BOOST_CHECK_EQUAL(edit::AdjustFeaturePartialFlagForLocation(*cds), false);
    BOOST_CHECK_EQUAL(prot_feat->GetLocation().IsPartialStart(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(prot_feat->GetLocation().IsPartialStop(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(prot_feat->IsSetPartial(), false);
    BOOST_CHECK_EQUAL(edit::AdjustProteinFeaturePartialsToMatchCDS(*prot_feat, *cds), false);
    BOOST_CHECK_EQUAL(prot_molinfo->GetMolinfo().GetCompleteness(), (CMolInfo::TCompleteness)CMolInfo::eCompleteness_complete);
    BOOST_CHECK_EQUAL(edit::AdjustProteinMolInfoToMatchCDS(prot_molinfo->SetMolinfo(), *cds), false);
    BOOST_CHECK_EQUAL(edit::AdjustForCDSPartials(*cds, seh), false);

    cds->SetLocation().SetPartialStart(true, eExtreme_Biological);
    BOOST_CHECK_EQUAL(cds->IsSetPartial(), false);
    BOOST_CHECK_EQUAL(edit::AdjustFeaturePartialFlagForLocation(*cds), true);
    BOOST_CHECK_EQUAL(cds->IsSetPartial(), true);
            
    BOOST_CHECK_EQUAL(edit::AdjustProteinFeaturePartialsToMatchCDS(*prot_feat, *cds), true);
    BOOST_CHECK_EQUAL(prot_feat->GetLocation().IsPartialStart(eExtreme_Biological), true);
    BOOST_CHECK_EQUAL(prot_feat->GetLocation().IsPartialStop(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(prot_feat->IsSetPartial(), true);

    BOOST_CHECK_EQUAL(edit::AdjustProteinMolInfoToMatchCDS(prot_molinfo->SetMolinfo(), *cds), true);
    BOOST_CHECK_EQUAL(prot_molinfo->GetMolinfo().GetCompleteness(), (CMolInfo::TCompleteness)CMolInfo::eCompleteness_no_left);

    // all changes in one go
    cds->SetLocation().SetPartialStart(false, eExtreme_Biological);
    BOOST_CHECK_EQUAL(edit::AdjustFeaturePartialFlagForLocation(*cds), true);
    BOOST_CHECK_EQUAL(edit::AdjustForCDSPartials(*cds, seh), true);
    prot_feat = unit_test_util::GetProtFeatFromGoodNucProtSet (entry);
    BOOST_CHECK_EQUAL(prot_feat->GetLocation().IsPartialStart(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(prot_feat->GetLocation().IsPartialStop(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(prot_feat->IsSetPartial(), false);
    BOOST_CHECK_EQUAL(prot_molinfo->GetMolinfo().GetCompleteness(), (CMolInfo::TCompleteness)CMolInfo::eCompleteness_complete);

}


BOOST_AUTO_TEST_CASE(Test_MakemRNAforCDS)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    STANDARD_SETUP

    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    CRef<CSeq_feat> mrna = edit::MakemRNAforCDS(*cds, scope);
    BOOST_CHECK_EQUAL(sequence::Compare(cds->GetLocation(), mrna->GetLocation(),
        &scope, sequence::fCompareOverlapping), sequence::eSame);
    BOOST_CHECK_EQUAL(mrna->GetLocation().IsPartialStart(eExtreme_Biological), true);
    BOOST_CHECK_EQUAL(mrna->GetLocation().IsPartialStop(eExtreme_Biological), true);

    // with a 3' UTR
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<objects::CSeq_entry> nuc_seq = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet (entry);
    CRef<CSeq_feat> utr3 = unit_test_util::AddGoodImpFeat(nuc_seq, "3'UTR");
    utr3->ResetComment();
    utr3->SetLocation().SetInt().SetFrom(27);
    utr3->SetLocation().SetInt().SetTo(30);
    seh = scope.AddTopLevelSeqEntry(*entry);

    mrna = edit::MakemRNAforCDS(*cds, scope);
    BOOST_CHECK_EQUAL(sequence::Compare(cds->GetLocation(), mrna->GetLocation(),
        &scope, sequence::fCompareOverlapping), sequence::eContained);
    BOOST_CHECK_EQUAL(mrna->GetLocation().IsPartialStart(eExtreme_Biological), true);
    BOOST_CHECK_EQUAL(mrna->GetLocation().IsPartialStop(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(mrna->GetLocation().GetStop(eExtreme_Biological), utr3->GetLocation().GetStop(eExtreme_Biological));

    // with a 5' UTR and a 3' UTR
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_feat> utr5 = unit_test_util::AddGoodImpFeat(nuc_seq, "5'UTR");
    utr5->ResetComment();
    utr5->SetLocation().SetInt().SetFrom(0);
    utr5->SetLocation().SetInt().SetTo(2);
    cds->SetLocation().SetInt().SetFrom(3);
    seh = scope.AddTopLevelSeqEntry(*entry);
    mrna = edit::MakemRNAforCDS(*cds, scope);
    BOOST_CHECK_EQUAL(sequence::Compare(cds->GetLocation(), mrna->GetLocation(),
        &scope, sequence::fCompareOverlapping), sequence::eContained);
    BOOST_CHECK_EQUAL(mrna->GetLocation().IsPartialStart(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(mrna->GetLocation().IsPartialStop(eExtreme_Biological), false);
    BOOST_CHECK_EQUAL(mrna->GetLocation().GetStart(eExtreme_Biological), utr5->GetLocation().GetStart(eExtreme_Biological));
    BOOST_CHECK_EQUAL(mrna->GetLocation().GetStop(eExtreme_Biological), utr3->GetLocation().GetStop(eExtreme_Biological));

    scope.RemoveTopLevelSeqEntry(seh);
    unit_test_util::AddFeat(mrna, nuc_seq);
    seh = scope.AddTopLevelSeqEntry(*entry);
    // should not create another mRNA if one is already on the record with the right product name
    CRef<CSeq_feat> mrna2 = edit::MakemRNAforCDS(*cds, scope);
    BOOST_REQUIRE(!mrna2);

    // but will create if the existing mRNA has the wrong product
    mrna->SetData().SetRna().SetExt().SetName("abc");
    mrna2 = edit::MakemRNAforCDS(*cds, scope);
    BOOST_CHECK_EQUAL(sequence::Compare(mrna2->GetLocation(), mrna->GetLocation(),
        &scope, sequence::fCompareOverlapping), sequence::eSame);

}

BOOST_AUTO_TEST_CASE(Test_GetmRNAforCDS)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    STANDARD_SETUP

    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet (entry);
    CConstRef<CSeq_feat> mrna = edit::GetmRNAforCDS(*cds, scope);
    BOOST_CHECK_EQUAL(mrna.Empty(), true);

    CRef<objects::CSeq_entry> nuc_seq = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet (entry);
    CRef<CSeq_feat> mrna1 = unit_test_util::MakemRNAForCDS (cds);
    mrna1->SetData().SetRna().SetExt().SetName("product 1");
    CRef<CSeq_annot> annot = unit_test_util::AddFeat(mrna1, nuc_seq);
    CSeq_entry_EditHandle edit_seh = seh.GetEditHandle();
    edit_seh.AttachAnnot(*annot);

    mrna = edit::GetmRNAforCDS(*cds, scope);
    BOOST_REQUIRE(!mrna.Empty());
    BOOST_CHECK_EQUAL(mrna == mrna1, true);
}

BOOST_AUTO_TEST_CASE(Test_GetGeneticCodeForBioseq)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    STANDARD_SETUP

    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    CRef<CGenetic_code> code = edit::GetGeneticCodeForBioseq(*bi);
    BOOST_REQUIRE(!code);

    unit_test_util::SetGcode(entry, 6);
    code = edit::GetGeneticCodeForBioseq(*bi);
    BOOST_CHECK_EQUAL(code->GetId(), 6);

    unit_test_util::SetGenome(entry, CBioSource::eGenome_mitochondrion);
    code = edit::GetGeneticCodeForBioseq(*bi);
    BOOST_REQUIRE(!code);

    unit_test_util::SetMGcode(entry, 2);
    code = edit::GetGeneticCodeForBioseq(*bi);
    BOOST_CHECK_EQUAL(code->GetId(), 2);

    unit_test_util::SetGenome(entry, CBioSource::eGenome_apicoplast);
    code = edit::GetGeneticCodeForBioseq(*bi);
    BOOST_CHECK_EQUAL(code->GetId(), 11);

    unit_test_util::SetPGcode(entry, 12);
    code = edit::GetGeneticCodeForBioseq(*bi);
    BOOST_CHECK_EQUAL(code->GetId(), 12);
}


BOOST_AUTO_TEST_CASE(Test_TruncateCDSAtStop)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGGCCAAAATTGGCCAAATAAGTAAATAATTGGCCAAAATTGGCCAAAATTGGCCAA");
    CRef<CSeq_feat> cds = unit_test_util::AddMiscFeature(entry, entry->GetSeq().GetInst().GetLength() - 1);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    cds->SetData().SetCdregion();
    STANDARD_SETUP

    // check for frame 1/unset
    bool found_stop = edit::TruncateCDSAtStop(*cds, scope);
    BOOST_CHECK_EQUAL(found_stop, true);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), 23);
    BOOST_CHECK_EQUAL(cds->GetLocation().IsPartialStop(eExtreme_Biological), false);

    // check for frame 2
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_two);
    cds->SetLocation().SetInt().SetTo(entry->GetSeq().GetInst().GetLength() - 1);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    found_stop = edit::TruncateCDSAtStop(*cds, scope);
    BOOST_CHECK_EQUAL(found_stop, true);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), 27);
    BOOST_CHECK_EQUAL(cds->GetLocation().IsPartialStop(eExtreme_Biological), false);

    // check for frame 3
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_three);
    cds->SetLocation().SetInt().SetTo(entry->GetSeq().GetInst().GetLength() - 1);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    found_stop = edit::TruncateCDSAtStop(*cds, scope);
    BOOST_CHECK_EQUAL(found_stop, true);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), 31);
    BOOST_CHECK_EQUAL(cds->GetLocation().IsPartialStop(eExtreme_Biological), false);


}


BOOST_AUTO_TEST_CASE(Test_ExtendCDSToStopCodon)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGGCCAAAATTGGCCAAATAAGTAAATAATTGGCCAAAATTGGCCAAAATTGGCCAA");
    CRef<CSeq_feat> cds = unit_test_util::AddMiscFeature(entry, 15);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    cds->SetData().SetCdregion();
    STANDARD_SETUP

    // check for frame 1/unset
    bool found_stop = edit::ExtendCDSToStopCodon(*cds, scope);
    BOOST_CHECK_EQUAL(found_stop, true);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), 23);
    BOOST_CHECK_EQUAL(cds->GetLocation().IsPartialStop(eExtreme_Biological), false);

    // check for frame 2
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_two);
    cds->SetLocation().SetInt().SetTo(15);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    found_stop = edit::ExtendCDSToStopCodon(*cds, scope);
    BOOST_CHECK_EQUAL(found_stop, true);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), 27);
    BOOST_CHECK_EQUAL(cds->GetLocation().IsPartialStop(eExtreme_Biological), false);

    // check for frame 3
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_three);
    cds->SetLocation().SetInt().SetTo(15);
    cds->SetLocation().SetPartialStop(true, eExtreme_Biological);
    found_stop = edit::ExtendCDSToStopCodon(*cds, scope);
    BOOST_CHECK_EQUAL(found_stop, true);
    BOOST_CHECK_EQUAL(cds->GetLocation().GetStop(eExtreme_Biological), 31);
    BOOST_CHECK_EQUAL(cds->GetLocation().IsPartialStop(eExtreme_Biological), false);


}


BOOST_AUTO_TEST_CASE(Test_MakemRNAAnnotOnly)
{
    CRef<CSeq_feat> cds(new CSeq_feat());
    cds->SetData().SetCdregion();
    cds->SetLocation().SetInt().SetId().SetLocal().SetStr("abc");
    cds->SetLocation().SetInt().SetFrom(10);
    cds->SetLocation().SetInt().SetTo(40);
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetFtable().push_back(cds);

    CScope scope(*CObjectManager::GetInstance());
    scope.AddDefaults();
    CSeq_annot_Handle sah = scope.AddSeq_annot(*annot);
    CFeat_CI it(sah);
    while (it) {
        if (it->GetFeatSubtype() == CSeqFeatData::eSubtype_cdregion) {
            const CSeq_feat& cds = it->GetOriginalFeature();
            CRef<CSeq_feat> pRna = edit::MakemRNAforCDS(cds, scope); //<-- blows up on NULL ptr !!!
            BOOST_CHECK_EQUAL(pRna->GetLocation().GetStart(eExtreme_Biological), 10);
            BOOST_CHECK_EQUAL(pRna->GetLocation().GetStop(eExtreme_Biological), 40);
        }
        ++it;
    }

    scope.RemoveSeq_annot(sah);
    CRef<CSeq_feat> utr5(new CSeq_feat());
    utr5->SetData().SetImp().SetKey("5'UTR");
    utr5->SetLocation().SetInt().SetId().SetLocal().SetStr("abc");
    utr5->SetLocation().SetInt().SetFrom(0);
    utr5->SetLocation().SetInt().SetTo(9);
    annot->SetData().SetFtable().push_back(utr5);
    CRef<CSeq_feat> utr3(new CSeq_feat());
    utr3->SetData().SetImp().SetKey("3'UTR");
    utr3->SetLocation().SetInt().SetId().SetLocal().SetStr("abc");
    utr3->SetLocation().SetInt().SetFrom(41);
    utr3->SetLocation().SetInt().SetTo(50);
    annot->SetData().SetFtable().push_back(utr3);

    sah = scope.AddSeq_annot(*annot);
    CFeat_CI it2(sah);
    while (it2) {
        if (it2->GetFeatSubtype() == CSeqFeatData::eSubtype_cdregion) {
            const CSeq_feat& cds = it2->GetOriginalFeature();
            CRef<CSeq_feat> pRna = edit::MakemRNAforCDS(cds, scope); //<-- blows up on NULL ptr !!!
            BOOST_CHECK_EQUAL(pRna->GetLocation().GetStart(eExtreme_Biological), 0);
            BOOST_CHECK_EQUAL(pRna->GetLocation().GetStop(eExtreme_Biological), 50);
        }
        ++it2;
    }

    // should not make mRNA if one already exists
    scope.RemoveSeq_annot(sah);
    CRef<CSeq_feat> mrna(new CSeq_feat());
    mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    mrna->SetData().SetRna().SetExt().SetName("");
    mrna->SetLocation().SetInt().SetId().SetLocal().SetStr("abc");
    mrna->SetLocation().SetInt().SetFrom(10);
    mrna->SetLocation().SetInt().SetTo(40);
    annot->SetData().SetFtable().push_back(mrna);
    BOOST_CHECK_EQUAL(mrna->GetData().GetSubtype(), CSeqFeatData::eSubtype_mRNA);
    sah = scope.AddSeq_annot(*annot);

    CFeat_CI it3(sah);
    while (it3) {
        if (it3->GetFeatSubtype() == CSeqFeatData::eSubtype_cdregion) {
            const CSeq_feat& cds = it3->GetOriginalFeature();
            CRef<CSeq_feat> pRna = edit::MakemRNAforCDS(cds, scope);
            BOOST_REQUIRE(!pRna);
        }
        ++it3;
    }

}


BOOST_AUTO_TEST_CASE(Test_SimpleReplace)
{
    CRef<CSimple_replace> repl(new CSimple_replace());
    repl->SetReplace("foo");

    string test = "abc";

    CRef<CString_constraint> constraint(NULL);
    BOOST_CHECK_EQUAL(repl->ApplyToString(test, constraint), true);
    BOOST_CHECK_EQUAL(test, "foo");

    test = "candidate abc";
    repl->SetWeasel_to_putative(true);
    BOOST_CHECK_EQUAL(repl->ApplyToString(test, constraint), true);
    BOOST_CHECK_EQUAL(test, "putative foo");
}


BOOST_AUTO_TEST_CASE(Test_ReplaceFunc)
{
    CRef<CReplace_func> repl(new CReplace_func());
    repl->SetHaem_replace("haem");

    string test = "haemagglutination domain protein";

    CRef<CString_constraint> constraint(NULL);
    BOOST_CHECK_EQUAL(repl->ApplyToString(test, constraint), true);
    BOOST_CHECK_EQUAL(test, "hemagglutination domain protein");

    test = "land of the free, haem of the brave";
    BOOST_CHECK_EQUAL(repl->ApplyToString(test, constraint), true);
    BOOST_CHECK_EQUAL(test, "land of the free, heme of the brave");

    repl->SetSimple_replace().SetReplace("foo");
    test = "abc";

    BOOST_CHECK_EQUAL(repl->ApplyToString(test, constraint), true);
    BOOST_CHECK_EQUAL(test, "foo");

    test = "candidate abc";
    repl->SetSimple_replace().SetWeasel_to_putative(true);
    BOOST_CHECK_EQUAL(repl->ApplyToString(test, constraint), true);
    BOOST_CHECK_EQUAL(test, "putative foo");

}


BOOST_AUTO_TEST_CASE(Test_SuspectRule)
{
    CRef<CSuspect_rule> rule(new CSuspect_rule());
    rule->SetFind().SetString_constraint().SetMatch_text("haem");
    rule->SetReplace().SetReplace_func().SetHaem_replace("haem");

    string test = "haemagglutination domain protein";

    BOOST_CHECK_EQUAL(rule->ApplyToString(test), true);
    BOOST_CHECK_EQUAL(test, "hemagglutination domain protein");

    test = "land of the free, haem of the brave";
    BOOST_CHECK_EQUAL(rule->ApplyToString(test), true);
    BOOST_CHECK_EQUAL(test, "land of the free, heme of the brave");

    rule->SetFind().SetString_constraint().SetMatch_text("abc");
    rule->SetReplace().SetReplace_func().SetSimple_replace().SetReplace("foo");
    rule->SetReplace().SetReplace_func().SetSimple_replace().SetWhole_string(true);
    test = "abc";

    BOOST_CHECK_EQUAL(rule->ApplyToString(test), true);
    BOOST_CHECK_EQUAL(test, "foo");

    test = "candidate abc";
    rule->SetReplace().SetReplace_func().SetSimple_replace().SetWeasel_to_putative(true);
    BOOST_CHECK_EQUAL(rule->ApplyToString(test), true);
    BOOST_CHECK_EQUAL(test, "putative foo");

    test = "do not match me";
    rule->SetReplace().SetReplace_func().SetSimple_replace().ResetWhole_string();
    rule->SetFind().SetString_constraint().SetMatch_text("me");
    rule->SetFind().SetString_constraint().SetMatch_location(eString_location_starts);
    BOOST_CHECK_EQUAL(rule->ApplyToString(test), false);
    BOOST_CHECK_EQUAL(test, "do not match me");

    rule->SetFind().SetString_constraint().SetMatch_location(eString_location_ends);
    BOOST_CHECK_EQUAL(rule->ApplyToString(test), true);
    BOOST_CHECK_EQUAL(test, "do not match foo");

    test = "me first";
    rule->SetFind().SetString_constraint().SetMatch_location(eString_location_starts);
    BOOST_CHECK_EQUAL(rule->ApplyToString(test), true);
    BOOST_CHECK_EQUAL(test, "foo first");

    test = "me me me me";
    rule->SetFind().SetString_constraint().ResetMatch_location();

    BOOST_CHECK_EQUAL(rule->ApplyToString(test), true);
    BOOST_CHECK_EQUAL(test, "foo foo foo foo");
    
    test = "30S ribosomal protein S12";
    rule->SetFind().Reset();
    rule->SetFind().SetString_constraint().SetMatch_location(eString_location_equals);
    rule->SetFind().SetString_constraint().SetMatch_text("CHC2 zinc finger");
    rule->SetFind().SetString_constraint().SetIgnore_weasel(true);
    rule->SetReplace().Reset();
    rule->SetReplace().SetReplace_func().SetSimple_replace().SetReplace("CHC2 zinc finger protein");
    rule->SetReplace().SetReplace_func().SetSimple_replace().SetWhole_string(false);
    rule->SetReplace().SetReplace_func().SetSimple_replace().SetWeasel_to_putative(true);
    BOOST_CHECK_EQUAL(rule->ApplyToString(test), false);

    test = "hypothetical protein";
    rule->SetFind().Reset();
    rule->SetFind().SetString_constraint().SetMatch_location(eString_location_equals);
    rule->SetFind().SetString_constraint().SetMatch_text("protein");
    rule->SetFind().SetString_constraint().SetIgnore_weasel(true);
    rule->SetReplace().Reset();
    rule->SetReplace().SetReplace_func().SetSimple_replace().SetReplace("hypothetical protein");
    BOOST_CHECK_EQUAL(rule->ApplyToString(test), true);

    // string_constraint with ignore-words
    test = "human";
    rule->SetFind().Reset();
    rule->SetFind().SetString_constraint().SetMatch_text("Homo sapiens");
    rule->SetFind().SetString_constraint().SetMatch_location(eString_location_equals);
    rule->SetFind().SetString_constraint().SetIgnore_space(true);
    rule->SetFind().SetString_constraint().SetIgnore_punct(true);

    CRef <CWord_substitution_set> word_subs(new CWord_substitution_set);
    rule->SetFind().SetString_constraint().SetIgnore_words(word_subs.GetObject());

    CRef <CWord_substitution> word_sub(new CWord_substitution);
    word_sub->SetWord("Homo sapiens");
    list <string> syns;
    syns.push_back("human");
    syns.push_back("Homo sapien");
    syns.push_back("Homosapiens");
    syns.push_back("Homo-sapiens");
    syns.push_back("Homo spiens");
    syns.push_back("Homo Sapience");
    syns.push_back("homosapein");
    syns.push_back("homosapiens");
    syns.push_back("homosapien");
    syns.push_back("homo_sapien");
    syns.push_back("homo_sapiens");
    syns.push_back("Homosipian");
    word_sub->SetSynonyms() = syns;
    rule->SetFind().SetString_constraint().SetIgnore_words().Set().push_back(word_sub);

    word_sub.Reset(new CWord_substitution);
    word_sub->SetWord("sapiens");
    syns.clear();
    syns.push_back("sapien");
    syns.push_back("sapeins");
    syns.push_back("sapein");
    syns.push_back("sapins");
    syns.push_back("sapens");
    syns.push_back("sapin");
    syns.push_back("sapen");
    syns.push_back("sapians");
    syns.push_back("sapian");
    syns.push_back("sapies");
    syns.push_back("sapie");
    word_sub->SetSynonyms() = syns;
    rule->SetFind().SetString_constraint().SetIgnore_words().Set().push_back(word_sub);
    BOOST_CHECK_EQUAL(rule->ApplyToString(test), true);
    test = "human";
    BOOST_CHECK_EQUAL(rule->ApplyToString(test), true);
    test = "human1";
    BOOST_CHECK_EQUAL(rule->ApplyToString(test), false);
    test = "Homo sapien";
    BOOST_CHECK_EQUAL(rule->ApplyToString(test), true);
    test = "Human sapien";
    BOOST_CHECK_EQUAL(rule->ApplyToString(test), false);
    test = "sapien";
    BOOST_CHECK_EQUAL(rule->ApplyToString(test), false);

    word_sub.Reset(new CWord_substitution);
    // all the syns won't match because of missing word_sub.Word;
    syns.clear();
    syns.push_back("fruit");     
    syns.push_back("apple");
    syns.push_back("apple, pear");
    syns.push_back("grape");
    syns.push_back("peaches");
    syns.push_back("peach");
    word_sub->SetSynonyms() = syns;
    rule->SetFind().SetString_constraint().SetIgnore_words().Set().push_back(word_sub);
    test = "fruit";
    BOOST_CHECK_EQUAL(rule->ApplyToString(test), false);
    test = "pear, apple";
    BOOST_CHECK_EQUAL(rule->ApplyToString(test), false);
}

BOOST_AUTO_TEST_CASE(Test_FindMatchingFrame)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntry);
         istr >> MSerial_AsnText >> entry;
    }}

    CRef<CSeq_feat> cds = entry.SetSet().SetAnnot().front()->SetData().SetFtable().front();

    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);

    BOOST_CHECK_EQUAL(edit::ApplyCDSFrame::s_FindMatchingFrame(*cds, scope), CCdregion::eFrame_one);
    cds->SetLocation().SetInt().SetFrom(13);
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_one);
    BOOST_CHECK_EQUAL(edit::ApplyCDSFrame::s_FindMatchingFrame(*cds, scope), CCdregion::eFrame_two);
    cds->SetLocation().SetInt().SetFrom(12);
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_one);
    BOOST_CHECK_EQUAL(edit::ApplyCDSFrame::s_FindMatchingFrame(*cds, scope), CCdregion::eFrame_three);

    edit::ApplyCDSFrame::s_SetCDSFrame(*cds, edit::ApplyCDSFrame::eOne, scope);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().GetFrame(), CCdregion::eFrame_one);
    edit::ApplyCDSFrame::s_SetCDSFrame(*cds, edit::ApplyCDSFrame::eTwo, scope);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().GetFrame(), CCdregion::eFrame_two);
    edit::ApplyCDSFrame::s_SetCDSFrame(*cds, edit::ApplyCDSFrame::eThree, scope);
    BOOST_CHECK_EQUAL(cds->GetData().GetCdregion().GetFrame(), CCdregion::eFrame_three);

}


BOOST_AUTO_TEST_CASE(Test_PromoteCDSToNucProtSet_And_DemoteCDSToNucSeq)
{
    CScope scope(*CObjectManager::GetInstance());

    CRef<CSeq_entry> nuc = unit_test_util::BuildGoodSeq();
    CRef<CSeq_id> nuc_id(new CSeq_id());
    nuc_id->SetLocal().SetStr("nuc");
    unit_test_util::ChangeId(nuc, nuc_id);
    CRef<objects::CSeq_feat> cds = unit_test_util::AddMiscFeature(nuc);
    cds->ResetComment();
    cds->SetData().SetCdregion();

    // should not change cdregion if not in nuc-prot set and product not set
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*nuc);
    CSeq_feat_Handle fh = scope.GetSeq_featHandle(*cds);

    BOOST_CHECK_EQUAL(edit::PromoteCDSToNucProtSet(fh), false);
    BOOST_ASSERT(fh.GetAnnot().GetParentEntry() == seh);
    scope.RemoveTopLevelSeqEntry(seh);

    // should not change cdregion if not in nuc-prot set and product is set
    CRef<CSeq_id> product_id(new CSeq_id());
    product_id->SetLocal().SetStr("prot");
    cds->SetProduct().SetWhole().Assign(*product_id);
    
    seh = scope.AddTopLevelSeqEntry(*nuc);
    fh = scope.GetSeq_featHandle(*cds);
    BOOST_CHECK_EQUAL(edit::PromoteCDSToNucProtSet(fh), false);
    BOOST_ASSERT(fh.GetAnnot().GetParentEntry() == seh);
    scope.RemoveTopLevelSeqEntry(seh);

    // should not change cdregion if in nuc-prot set and product set but
    // protein sequence not local
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_nuc_prot);
    entry->SetSet().SetSeq_set().push_back(nuc);

    seh = scope.AddTopLevelSeqEntry(*entry);
    CBioseq_Handle n_bsh = scope.GetBioseqHandle(*nuc_id);
    fh = scope.GetSeq_featHandle(*cds);
    BOOST_CHECK_EQUAL(edit::PromoteCDSToNucProtSet(fh), false);
    BOOST_ASSERT(fh.GetAnnot().GetParentEntry() == n_bsh.GetSeq_entry_Handle());
    scope.RemoveTopLevelSeqEntry(seh);

    // should change cdregion if in nuc-prot set and product set
    // and protein sequence is local
    CRef<CSeq_entry> prot = unit_test_util::BuildGoodProtSeq();
    unit_test_util::ChangeId(prot, product_id);
    entry->SetSet().SetSeq_set().push_back(prot);

    seh = scope.AddTopLevelSeqEntry(*entry);
    fh = scope.GetSeq_featHandle(*cds);
    BOOST_CHECK_EQUAL(edit::PromoteCDSToNucProtSet(fh), true);
    BOOST_ASSERT(fh.GetAnnot().GetParentEntry() == seh);

    // can't promote again
    BOOST_CHECK_EQUAL(edit::PromoteCDSToNucProtSet(fh), false);

    // after demotion, should go back to nucleotide sequence
    n_bsh = scope.GetBioseqHandle(*nuc_id);
    BOOST_CHECK_EQUAL(edit::DemoteCDSToNucSeq(fh), true);
    BOOST_ASSERT(fh.GetAnnot().GetParentEntry() == n_bsh.GetSeq_entry_Handle());

    // can't demote again
    BOOST_CHECK_EQUAL(edit::DemoteCDSToNucSeq(fh), false);
}


//////////////////////////////////////////////////////////////////////////////////
const char* sc_TestEntry ="\
Seq-entry ::= set {\
  class nuc-prot,\
  seq-set {\
    seq {\
      id {\
        genbank {\
          name \"AF010144\",\
          accession \"AF010144\",\
          version 1\
        },\
        gi 3002526\
      },\
      inst {\
        repr raw,\
        mol rna,\
        length 1442,\
        seq-data iupacna \"TTTTTTTTTTTGAGATGGAGTTTTCGCTCTTGTTGCCCAGGCTGGAGTGCAA\
TGGCGCAATCTCAGCTCACCGCAACCTCCGCCTCCCGGGTTCAAGCGATTCTCCTGCCTCAGCCTCCCCAGTAGCTGG\
GATTACAGGCATGTGCACCCACGCTCGGCTAATTTTGTATTTTTTTTTAGTAGAGATGGAGTTTCTCCATGTTGGTCA\
GGCTGGTCTCGAACTCCCGACCTCAGATGATCCCTCCGTCTCGGCCTCCCAAAGTGCTAGATACAGGACTGGCCACCA\
TGCCCGGCTCTGCCTGGCTAATTTTTGTGGTAGAAACAGGGTTTCACTGATGTGCCCAAGCTGGTCTCCTGAGCTCAA\
GCAGTCCACCTGCCTCAGCCTCCCAAAGTGCTGGGATTACAGGCGTGCAGCCGTGCCTGGCCTTTTTATTTTATTTTT\
TTTAAGACACAGGTGTCCCACTCTTACCCAGGATGAAGTGCAGTGGTGTGATCACAGCTCACTGCAGCCTTCAACTCC\
TGAGATCAAGCATCCTCCTGCCTCAGCCTCCCAAGTAGCTGGGACCAAAGACATGCACCACTACACCTGGCTAATTTT\
TATTTTTATTTTTAATTTTTTGAGACAGAGTCTCAACTCTGTCACCCAGGCTGGAGTGCAGTGGCGCAATCTTGGCTC\
ACTGCAACCTCTGCCTCCCGGGTTCAAGTTATTCTCCTGCCCCAGCCTCCTGAGTAGCTGGGACTACAGGCGCCCACC\
ACGCCTAGCTAATTTTTTTGTATTTTTAGTAGAGATGGGGTTCACCATGTTCGCCAGGTTGATCTTGATCTCTGGACC\
TTGTGATCTGCCTGCCTCGGCCTCCCAAAGTGCTGGGATTACAGGCGTGAGCCACCACGCCCGGCTTATTTTTAATTT\
TTGTTTGTTTGAAATGGAATCTCACTCTGTTACCCAGGCTGGAGTGCAATGGCCAAATCTCGGCTCACTGCAACCTCT\
GCCTCCCGGGCTCAAGCGATTCTCCTGTCTCAGCCTCCCAAGCAGCTGGGATTACGGGCACCTGCCACCACACCCCGC\
TAATTTTTGTATTTTCATTAGAGGCGGGGTTTCACCATATTTGTCAGGCTGGTCTCAAACTCCTGACCTCAGGTGACC\
CACCTGCCTCAGCCTTCCAAAGTGCTGGGATTACAGGCGTGAGCCACCTCACCCAGCCGGCTAATTTAGATAAAAAAA\
TATGTAGCAATGGGGGGTCTTGCTATGTTGCCCAGGCTGGTCTCAAACTTCTGGCTTCATGCAATCCTTCCAAATGAG\
CCACAACACCCAGCCAGTCACATTTTTTAAACAGTTACATCTTTATTTTAGTATACTAGAAAGTAATACAATAAACAT\
GTCAAACCTGCAAATTCAGTAGTAACAGAGTTCTTTTATAACTTTTAAACAAAGCTTTAGAGCA\"\
      }\
    },\
    seq {\
      id {\
        genbank {\
          accession \"AAC08737\",\
          version 1\
        },\
        gi 3002527\
      },\
      inst {\
        repr raw,\
        mol aa,\
        length 375,\
        topology not-set,\
        seq-data ncbieaa \"MEFSLLLPRLECNGAISAHRNLRLPGSSDSPASASPVAGITGMCTHARLILY\
FFLVEMEFLHVGQAGLELPTSDDPSVSASQSARYRTGHHARLCLANFCGRNRVSLMCPSWSPELKQSTCLSLPKCWDY\
RRAAVPGLFILFFLRHRCPTLTQDEVQWCDHSSLQPSTPEIKHPPASASQVAGTKDMHHYTWLIFIFIFNFLRQSLNS\
VTQAGVQWRNLGSLQPLPPGFKLFSCPSLLSSWDYRRPPRLANFFVFLVEMGFTMFARLILISGPCDLPASASQSAGI\
TGVSHHARLIFNFCLFEMESHSVTQAGVQWPNLGSLQPLPPGLKRFSCLSLPSSWDYGHLPPHPANFCIFIRGGVSPY\
LSGWSQTPDLR\"\
      },\
      annot {\
        {\
          data ftable {\
            {\
              data prot {\
                name {\
                  \"neuronal thread protein AD7c-NTP\"\
                }\
              },\
              location int {\
                from 0,\
                to 374,\
                strand plus,\
                id gi 3002527\
              }\
            }\
          }\
        }\
      }\
    }\
  },\
  annot {\
    {\
      data ftable {\
        {\
          data cdregion {\
            frame two,\
            code {\
              id 1\
            }\
          },\
          product whole gi 3002527,\
          location int {\
            from 14,\
            to 1141,\
            strand plus,\
            id gi 3002526\
          }\
        }\
      }\
    }\
  }\
}";
END_SCOPE(objects)
END_NCBI_SCOPE

