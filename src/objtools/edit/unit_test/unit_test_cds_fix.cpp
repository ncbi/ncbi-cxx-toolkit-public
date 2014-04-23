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
    BOOST_CHECK_EQUAL(sequence::Compare(cds->GetLocation(), mrna->GetLocation(), &scope), sequence::eSame);
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
    BOOST_CHECK_EQUAL(sequence::Compare(cds->GetLocation(), mrna->GetLocation(), &scope), sequence::eContained);
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
    BOOST_CHECK_EQUAL(sequence::Compare(cds->GetLocation(), mrna->GetLocation(), &scope), sequence::eContained);
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
    BOOST_CHECK_EQUAL(sequence::Compare(mrna2->GetLocation(), mrna->GetLocation(), &scope), sequence::eSame);

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



END_SCOPE(objects)
END_NCBI_SCOPE

