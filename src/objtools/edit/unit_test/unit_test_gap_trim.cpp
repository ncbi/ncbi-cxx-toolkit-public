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
*   Sample unit tests file for adjusting features for gaps.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

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

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objtools/edit/gap_trim.hpp>
#include <objtools/unit_test_util/unit_test_util.hpp>

#include <serial/objistrasn.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

using namespace unit_test_util;
using namespace edit;

// Needed under windows for some reason.
#ifdef BOOST_NO_EXCEPTIONS

namespace boost
{
    void throw_exception( std::exception const & e ) {
        throw e;
    }
}

#endif

edit::TGappedFeatList TryOneCase(TSeqPos start, TSeqPos stop, bool is_minus = false)
{
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();

    CRef<CSeq_feat> misc = AddMiscFeature(entry);

    misc->SetLocation().SetInt().SetFrom(start);
    misc->SetLocation().SetInt().SetTo(stop);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    CFeat_CI f(seh);

    edit::TGappedFeatList gapped_list = ListGappedFeatures(f, *scope);
    return gapped_list;
}

void TestTrimMiscOnRight(TSeqPos start, TSeqPos stop, bool is_minus)
{
    edit::TGappedFeatList gapped_list = TryOneCase(start, stop, is_minus);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 1);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStart(eExtreme_Positional), 0);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStop(eExtreme_Positional), 11);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStart(eExtreme_Positional), false);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStop(eExtreme_Positional), true);
}


void TestTrimMiscOnLeft(TSeqPos start, TSeqPos stop, bool is_minus)
{
    edit::TGappedFeatList gapped_list = TryOneCase(17, 33, is_minus);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 1);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStart(eExtreme_Positional), 22);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStop(eExtreme_Positional), 33);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStart(eExtreme_Positional), true);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStop(eExtreme_Positional), false);
}


void TestSplitMisc(TSeqPos start, TSeqPos stop, bool is_minus)
{
    edit::TGappedFeatList gapped_list = TryOneCase(start, stop, is_minus);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), true);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 2);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStart(eExtreme_Positional), 0);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStop(eExtreme_Positional), 11);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStart(eExtreme_Positional), false);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStop(eExtreme_Positional), true);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().GetStart(eExtreme_Positional), 22);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().GetStop(eExtreme_Positional), 33);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().IsPartialStart(eExtreme_Positional), true);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().IsPartialStop(eExtreme_Positional), false);
}


void TestRemoveMisc(TSeqPos start, TSeqPos stop, bool is_minus)
{
    edit::TGappedFeatList gapped_list = TryOneCase(start, stop, is_minus);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 0);
}


BOOST_AUTO_TEST_CASE(Test_FeatGapInfoMisc)
{
    // no gap
    edit::TGappedFeatList gapped_list = TryOneCase(0, 5);
    BOOST_CHECK_EQUAL(gapped_list.size(), 0);
    gapped_list = TryOneCase(30, 34);
    BOOST_CHECK_EQUAL(gapped_list.size(), 0);
    gapped_list = TryOneCase(0, 5, true);
    BOOST_CHECK_EQUAL(gapped_list.size(), 0);
    gapped_list = TryOneCase(30, 34, true);
    BOOST_CHECK_EQUAL(gapped_list.size(), 0);

    // trim on right
    TestTrimMiscOnRight(0, 15, false);
    TestTrimMiscOnRight(0, 15, true);

    // trim on left
    TestTrimMiscOnLeft(17, 33, false);
    TestTrimMiscOnLeft(17, 33, true);

    // split
    TestSplitMisc(0, 33, false);
    TestSplitMisc(0, 33, true);

    // remove
    TestRemoveMisc(12, 21, false);
    TestRemoveMisc(12, 21, true);
}

void CheckTags(CSeq_entry_Handle seh, const string& orig_tag, size_t num_updates)
{
    CFeat_CI f(seh, SAnnotSelector(CSeqFeatData::e_Cdregion));
    size_t offset = 0;
    while (f) {
        const string& tag = f->GetProduct().GetWhole().GetGeneral().GetTag().GetStr();
        if (num_updates == 1 || offset == 0) {
            BOOST_CHECK_EQUAL(tag, orig_tag);
        } else {
            BOOST_CHECK_EQUAL(tag, orig_tag + "_" + NStr::NumericToString(offset));
        }
        ++f;
        ++offset;
    }
    BOOST_CHECK_EQUAL(offset, num_updates);

    CBioseq_CI prot_i(seh, CSeq_inst::eMol_aa);
    offset = 0;
    while (prot_i) {
        const string& tag = prot_i->GetId().front().GetSeqId()->GetGeneral().GetTag().GetStr();
        if (num_updates == 1 || offset == 0) {
            BOOST_CHECK_EQUAL(tag, orig_tag);
        } else {
            BOOST_CHECK_EQUAL(tag, orig_tag + "_" + NStr::NumericToString(offset));
        }
        ++prot_i;
        ++offset;
    }
    BOOST_CHECK_EQUAL(offset, num_updates);
}


edit::TGappedFeatList TryOneCDSCase(TSeqPos start, TSeqPos stop, TSeqPos gap_len, CCdregion::TFrame frame_one)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_id> prot_id(new CSeq_id());
    prot_id->SetGeneral().SetDb("foo");
    prot_id->SetGeneral().SetTag().SetStr("bar");
    unit_test_util::ChangeNucProtSetProteinId(entry, prot_id);
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetData().SetCdregion().SetFrame(frame_one);
    cds->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("orig_protein_id", "x")));
    cds->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("orig_transcript_id", "y")));

    nuc->SetSeq().SetInst().ResetSeq_data();
    nuc->SetSeq().SetInst().SetRepr(objects::CSeq_inst::eRepr_delta);
    nuc->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("ATGATGATGCCC", objects::CSeq_inst::eMol_dna);
    CRef<objects::CDelta_seq> gap_seg(new objects::CDelta_seq());
    gap_seg->SetLiteral().SetSeq_data().SetGap();
    gap_seg->SetLiteral().SetLength(gap_len);
    nuc->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(gap_seg);
    nuc->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("CCCAAATTTTAA", objects::CSeq_inst::eMol_dna);
    nuc->SetSeq().SetInst().SetLength(24 + gap_len);

    cds->SetLocation().SetInt().SetFrom(start);
    cds->SetLocation().SetInt().SetTo(stop);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    unit_test_util::RetranslateCdsForNucProtSet(entry, *scope);
    scope->RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_entry> prot = unit_test_util::GetProteinSequenceFromGoodNucProtSet(entry);
    if (prot->GetSeq().GetLength() > 3) {
        CRef<CSeq_feat> mat_peptide = unit_test_util::AddMiscFeature(prot);
        mat_peptide->SetData().SetProt().SetProcessed(CProt_ref::eProcessed_mature);
        mat_peptide->SetLocation().SetInt().SetFrom(1);
        mat_peptide->SetLocation().SetInt().SetTo(prot->GetSeq().GetInst().GetLength() - 2);
    }
    seh = scope->AddTopLevelSeqEntry(*entry);

    CFeat_CI f(seh);
    edit::TGappedFeatList gapped_list = ListGappedFeatures(f, *scope);
    return gapped_list;
}


void CheckQual(const CSeq_feat& feat, const string& qual, const string& val)
{
    bool found = false;
    if (feat.IsSetQual()) {
        ITERATE(CSeq_feat::TQual, it, feat.GetQual()) {
            if (NStr::Equal((*it)->GetQual(), qual)) {
                found = true;
                BOOST_CHECK_EQUAL((*it)->GetVal(), val);
            }
        }
    }
    if (!found) {
        BOOST_CHECK_EQUAL(val, "");
    }
}


void TestUpdateCDS(CSeq_feat_Handle cds, vector<CRef<CSeq_feat> > adjusted_feats)
{
    CScope& scope = cds.GetScope();
    CSeq_entry_Handle seh = scope.GetBioseqHandle(cds.GetLocation()).GetTopLevelEntry();
    string orig_tag = cds.GetProduct().GetWhole().GetGeneral().GetTag().GetStr();

    ProcessForTrimAndSplitUpdates(cds, adjusted_feats);

    CheckTags(seh, orig_tag, adjusted_feats.size());
}


void TestSplitCDS(TSeqPos start, TSeqPos stop, TSeqPos gap_len, CCdregion::TFrame frame_one, CCdregion::TFrame frame_two)
{
    edit::TGappedFeatList gapped_list = TryOneCDSCase(start, stop, gap_len, frame_one);

    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), true);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 2);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStart(eExtreme_Positional), start);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStop(eExtreme_Positional), 11);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStart(eExtreme_Positional), false);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStop(eExtreme_Positional), true);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetData().GetCdregion().GetFrame(), frame_one);
    CheckQual(*(adjusted_feats.front()), "orig_protein_id", "x");
    CheckQual(*(adjusted_feats.front()), "orig_transcript_id", "y");
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().GetStart(eExtreme_Positional), 11 + gap_len + 1);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().GetStop(eExtreme_Positional), stop);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().IsPartialStart(eExtreme_Positional), true);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().IsPartialStop(eExtreme_Positional), false);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetData().GetCdregion().GetFrame(), frame_two);
    CheckQual(*(adjusted_feats.back()), "orig_protein_id", "x_1");
    CheckQual(*(adjusted_feats.back()), "orig_transcript_id", "y_1");

    TestUpdateCDS(gapped_list[0]->GetFeature(), adjusted_feats);
}


// total length is 36
// if gap is longer than 12, take difference out of second piece
void SetUpDelta(CBioseq& seq, TSeqPos gap_len)
{
    seq.SetInst().ResetSeq_data();
    seq.SetInst().SetRepr(objects::CSeq_inst::eRepr_delta);
    string nuc_before = "ATGATGATGCCC";
    string nuc_after =  "CCCAAATTTTAA";
    seq.SetInst().SetExt().SetDelta().AddLiteral(nuc_before, objects::CSeq_inst::eMol_dna);
    CRef<objects::CDelta_seq> gap_seg(new objects::CDelta_seq());
    gap_seg->SetLiteral().SetSeq_data().SetGap();
    gap_seg->SetLiteral().SetLength(gap_len);
    seq.SetInst().SetExt().SetDelta().Set().push_back(gap_seg);
    seq.SetInst().SetExt().SetDelta().AddLiteral(nuc_after.substr(gap_len - 12), objects::CSeq_inst::eMol_dna);
    seq.SetInst().SetLength(36);
}


void TestTrimForFrame(TSeqPos gap_len, CCdregion::EFrame frame)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_id> prot_id(new CSeq_id());
    prot_id->SetGeneral().SetDb("foo");
    prot_id->SetGeneral().SetTag().SetStr("bar");
    unit_test_util::ChangeNucProtSetProteinId(entry, prot_id);
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetData().SetCdregion().SetFrame(frame);
    cds->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("orig_protein_id", "x")));
    cds->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("orig_transcript_id", "y")));

    SetUpDelta(nuc->SetSeq(), gap_len);

    switch (frame) {
        case CCdregion::eFrame_one:
        case CCdregion::eFrame_not_set:
            cds->SetLocation().SetInt().SetFrom(0);
            break;
        case CCdregion::eFrame_two:
            cds->SetLocation().SetInt().SetFrom(2);
            break;
        case CCdregion::eFrame_three:
            cds->SetLocation().SetInt().SetFrom(1);
            break;
    }
    cds->SetLocation().SetInt().SetTo(35);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    unit_test_util::RetranslateCdsForNucProtSet(entry, *scope);
    scope->RemoveTopLevelSeqEntry(seh);
    seh = scope->AddTopLevelSeqEntry(*entry);

    string orig_protstr;
    CSeqTranslator::Translate(*cds, *scope, orig_protstr);
    switch (frame) {
    case CCdregion::eFrame_one:
    case CCdregion::eFrame_not_set:
        if (gap_len > 12) {
            BOOST_CHECK_EQUAL(orig_protstr, "MMMPXXXXXKF*");
        } else {
            BOOST_CHECK_EQUAL(orig_protstr, "MMMPXXXXPKF*");
        }
        break;
    case CCdregion::eFrame_two:
    case CCdregion::eFrame_three:
        if (gap_len > 12) {
            BOOST_CHECK_EQUAL(orig_protstr, "MMPXXXXXKF*");
        } else {
            BOOST_CHECK_EQUAL(orig_protstr, "MMPXXXXPKF*");
        }
        break;
    }

    CFeat_CI f(seh);
    edit::TGappedFeatList gapped_list = ListGappedFeatures(f, *scope);
    gapped_list.front()->CalculateRelevantIntervals(true, true);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true, false);

    string before_protstr;
    CSeqTranslator::Translate(*(adjusted_feats[0]), *scope, before_protstr);

    switch (frame) {
    case CCdregion::eFrame_one:
    case CCdregion::eFrame_not_set:
        BOOST_CHECK_EQUAL(before_protstr, orig_protstr.substr(0, 4));
        break;
    case CCdregion::eFrame_two:
    case CCdregion::eFrame_three:
        BOOST_CHECK_EQUAL(before_protstr, orig_protstr.substr(0, 3));
        break;
    }


    string after_protstr;
    CSeqTranslator::Translate(*(adjusted_feats[1]), *scope, after_protstr);
    switch (frame) {
    case CCdregion::eFrame_one:
    case CCdregion::eFrame_not_set:
        if (gap_len > 12) {
            BOOST_CHECK_EQUAL(after_protstr, orig_protstr.substr(9));
        } else {
            BOOST_CHECK_EQUAL(after_protstr, orig_protstr.substr(8));
        }
        break;
    case CCdregion::eFrame_two:
    case CCdregion::eFrame_three:
        if (gap_len > 12) {
            BOOST_CHECK_EQUAL(after_protstr, orig_protstr.substr(8));
        } else {
            BOOST_CHECK_EQUAL(after_protstr, orig_protstr.substr(7));
        }
        break;
    }

}


BOOST_AUTO_TEST_CASE(Test_FrameForSplit)
{
    TestTrimForFrame(13, CCdregion::eFrame_one);
    TestTrimForFrame(13, CCdregion::eFrame_two);
    TestTrimForFrame(13, CCdregion::eFrame_three);
    TestTrimForFrame(14, CCdregion::eFrame_one);
    TestTrimForFrame(14, CCdregion::eFrame_two);
    TestTrimForFrame(14, CCdregion::eFrame_three);
    TestTrimForFrame(12, CCdregion::eFrame_one);
    TestTrimForFrame(12, CCdregion::eFrame_two);
    TestTrimForFrame(12, CCdregion::eFrame_three);
}


void TestTrimCDS(TSeqPos start, TSeqPos stop, CCdregion::TFrame frame_before, CCdregion::TFrame frame_after)
{
    edit::TGappedFeatList gapped_list = TryOneCDSCase(start, stop, 10, frame_before);

    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 1);
    if (start < 11) {
        // trim on right, no frame change
        BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStart(eExtreme_Positional), start);
        BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStop(eExtreme_Positional), 11);
        BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStart(eExtreme_Positional), false);
        BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStop(eExtreme_Positional), true);
        BOOST_CHECK_EQUAL(adjusted_feats.front()->GetData().GetCdregion().GetFrame(), frame_before);
    } else {
        // trim on left
        BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStart(eExtreme_Positional), 22);
        BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStop(eExtreme_Positional), stop);
        BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStart(eExtreme_Positional), true);
        BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStop(eExtreme_Positional), false);
        BOOST_CHECK_EQUAL(adjusted_feats.front()->GetData().GetCdregion().GetFrame(), frame_after);
    }
    CheckQual(*(adjusted_feats.front()), "orig_protein_id", "x");
    CheckQual(*(adjusted_feats.front()), "orig_transcript_id", "y");

    TestUpdateCDS(gapped_list[0]->GetFeature(), adjusted_feats);
}


BOOST_AUTO_TEST_CASE(Test_FeatGapInfoCDS)
{
    TestSplitCDS(0, 32, 9, CCdregion::eFrame_one, CCdregion::eFrame_one);
    TestSplitCDS(0, 32, 10, CCdregion::eFrame_one, CCdregion::eFrame_three);
    TestSplitCDS(0, 32, 11, CCdregion::eFrame_one, CCdregion::eFrame_two);

    TestSplitCDS(0, 32, 9, CCdregion::eFrame_two, CCdregion::eFrame_two);
    TestSplitCDS(0, 32, 10, CCdregion::eFrame_two, CCdregion::eFrame_one);
    TestSplitCDS(0, 32, 11, CCdregion::eFrame_two, CCdregion::eFrame_three);

    TestSplitCDS(0, 32, 9, CCdregion::eFrame_three, CCdregion::eFrame_three);
    TestSplitCDS(0, 32, 10, CCdregion::eFrame_three, CCdregion::eFrame_two);
    TestSplitCDS(0, 32, 11, CCdregion::eFrame_three, CCdregion::eFrame_one);

    // trim left
    TestTrimCDS(16, 33, CCdregion::eFrame_one, CCdregion::eFrame_one);
    TestTrimCDS(17, 33, CCdregion::eFrame_one, CCdregion::eFrame_two);
    TestTrimCDS(18, 33, CCdregion::eFrame_one, CCdregion::eFrame_three);
    // trim right
    TestTrimCDS(0, 15, CCdregion::eFrame_one, CCdregion::eFrame_one);
}


BOOST_AUTO_TEST_CASE(TestRemoveCDS)
{
    edit::TGappedFeatList gapped_list = TryOneCDSCase(12, 21, 10, CCdregion::eFrame_one);

    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 0);

    TestUpdateCDS(gapped_list[0]->GetFeature(), adjusted_feats);
}


BOOST_AUTO_TEST_CASE(Test_FeatGapInfoCDSGapInIntronMixLoc)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_one);
    cds->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("orig_protein_id", "x")));
    cds->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("orig_transcript_id", "y")));

    nuc->SetSeq().SetInst().ResetSeq_data();
    nuc->SetSeq().SetInst().SetRepr(objects::CSeq_inst::eRepr_delta);
    nuc->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("ATGATGATGCCC", objects::CSeq_inst::eMol_dna);
    CRef<objects::CDelta_seq> gap_seg(new objects::CDelta_seq());
    gap_seg->SetLiteral().SetSeq_data().SetGap().SetType(CSeq_gap::eType_unknown);
    gap_seg->SetLiteral().SetLength(10);
    nuc->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(gap_seg);
    nuc->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("CCCAAATTTTAA", objects::CSeq_inst::eMol_dna);
    nuc->SetSeq().SetInst().SetLength(34);

    CRef<CSeq_loc> int1(new CSeq_loc());
    int1->SetInt().SetId().Assign(*(nuc->GetSeq().GetId().front()));
    int1->SetInt().SetFrom(0);
    int1->SetInt().SetTo(10);
    CRef<CSeq_loc> int2(new CSeq_loc());
    int2->SetInt().SetId().Assign(*(nuc->GetSeq().GetId().front()));
    int2->SetInt().SetFrom(24);
    int2->SetInt().SetTo(33);
    cds->SetLocation().SetMix().Set().push_back(int1);
    cds->SetLocation().SetMix().Set().push_back(int2);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    unit_test_util::RetranslateCdsForNucProtSet(entry, *scope);


    CFeat_CI f(seh);
    edit::TGappedFeatList gapped_list = ListGappedFeatures(f, *scope);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);

    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), true);

    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 2);
    adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, false, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 1);
}

BOOST_AUTO_TEST_CASE(Test_FeatGapInfoCDSGapInIntronPackedInt)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = unit_test_util::GetNucleotideSequenceFromGoodNucProtSet(entry);
    CRef<CSeq_feat> cds = unit_test_util::GetCDSFromGoodNucProtSet(entry);
    cds->SetData().SetCdregion().SetFrame(CCdregion::eFrame_one);
    cds->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("orig_protein_id", "x")));
    cds->SetQual().push_back(CRef<CGb_qual>(new CGb_qual("orig_transcript_id", "y")));

    nuc->SetSeq().SetInst().ResetSeq_data();
    nuc->SetSeq().SetInst().SetRepr(objects::CSeq_inst::eRepr_delta);
    nuc->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("ATGATGATGCCC", objects::CSeq_inst::eMol_dna);
    CRef<objects::CDelta_seq> gap_seg(new objects::CDelta_seq());
    gap_seg->SetLiteral().SetSeq_data().SetGap().SetType(CSeq_gap::eType_unknown);
    gap_seg->SetLiteral().SetLength(10);
    nuc->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(gap_seg);
    nuc->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("CCCAAATTTTAA", objects::CSeq_inst::eMol_dna);
    nuc->SetSeq().SetInst().SetLength(34);

    CRef<CSeq_interval> int1(new CSeq_interval());
    int1->SetId().Assign(*(nuc->GetSeq().GetId().front()));
    int1->SetFrom(0);
    int1->SetTo(10);
    CRef<CSeq_interval> int2(new CSeq_interval());
    int2->SetId().Assign(*(nuc->GetSeq().GetId().front()));
    int2->SetFrom(24);
    int2->SetTo(33);
    cds->SetLocation().SetPacked_int().Set().push_back(int1);
    cds->SetLocation().SetPacked_int().Set().push_back(int2);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    unit_test_util::RetranslateCdsForNucProtSet(entry, *scope);


    CFeat_CI f(seh);
    edit::TGappedFeatList gapped_list = ListGappedFeatures(f, *scope);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);

    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), true);

    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 2);
    adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, false, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 1);
}

void AddNsToGoodDelta(CBioseq& seq)
{
    seq.SetInst().ResetSeq_data();
    seq.SetInst().SetRepr(objects::CSeq_inst::eRepr_delta);
    seq.SetInst().SetExt().SetDelta().AddLiteral("NNNATGCCCANNNTGATGCCCNNN", objects::CSeq_inst::eMol_dna);
    CRef<objects::CDelta_seq> gap_seg1(new objects::CDelta_seq());
    gap_seg1->SetLiteral().SetSeq_data().SetGap();
    gap_seg1->SetLiteral().SetLength(10);
    seq.SetInst().SetExt().SetDelta().Set().push_back(gap_seg1);
    seq.SetInst().SetExt().SetDelta().AddLiteral("NNNCCCATGNNNATGATGNNN", objects::CSeq_inst::eMol_dna);
    CRef<objects::CDelta_seq> gap_seg2(new objects::CDelta_seq());
    gap_seg2->SetLiteral().SetSeq_data().SetGap();
    gap_seg2->SetLiteral().SetLength(10);
    gap_seg2->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
    seq.SetInst().SetExt().SetDelta().Set().push_back(gap_seg2);
    seq.SetInst().SetExt().SetDelta().AddLiteral("CCCATGNNNATGATG", objects::CSeq_inst::eMol_dna);
    seq.SetInst().SetLength(80);
}


edit::TGappedFeatList TryMiscWithNs(TSeqPos start, TSeqPos stop, bool is_minus = false)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    AddNsToGoodDelta(entry->SetSeq());

    CRef<CSeq_feat> misc = AddMiscFeature(entry);

    misc->SetLocation().SetInt().SetFrom(start);
    misc->SetLocation().SetInt().SetTo(stop);
    if (is_minus) {
        misc->SetLocation().SetStrand(eNa_strand_minus);
    }

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    CFeat_CI f(seh);

    edit::TGappedFeatList gapped_list = ListGappedFeatures(f, *scope);
    return gapped_list;
}


BOOST_AUTO_TEST_CASE(Test_NsAsGaps)
{
    edit::TGappedFeatList gapped_list;

    // feature entirely in Ns
    gapped_list = TryMiscWithNs(10, 12);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasNs(), true);
    gapped_list.front()->CalculateRelevantIntervals(false, false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);

    gapped_list = TryMiscWithNs(10, 12, true);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasNs(), true);
    gapped_list.front()->CalculateRelevantIntervals(false, false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);

    // feature ends in Ns
    gapped_list = TryMiscWithNs(8, 12);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasNs(), true);
    gapped_list.front()->CalculateRelevantIntervals(false, false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);

    gapped_list = TryMiscWithNs(8, 12, true);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasNs(), true);
    gapped_list.front()->CalculateRelevantIntervals(false, false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);


    // feature starts in Ns
    gapped_list = TryMiscWithNs(0, 5);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasNs(), true);
    gapped_list.front()->CalculateRelevantIntervals(false, false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    gapped_list = TryMiscWithNs(0, 5, true);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasNs(), true);
    gapped_list.front()->CalculateRelevantIntervals(false, false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);

    // just first segment, Ns but no gaps
    gapped_list = TryMiscWithNs(0, 23);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasNs(), true);
    gapped_list.front()->CalculateRelevantIntervals(false, false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), true);
    gapped_list = TryMiscWithNs(0, 23, true);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasNs(), true);
    gapped_list.front()->CalculateRelevantIntervals(false, false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), true);

    // feature has Ns and gaps - if not include gaps, trimmable, if include gaps, remove
    gapped_list = TryMiscWithNs(21, 30);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasNs(), true);
    gapped_list.front()->CalculateRelevantIntervals(false, false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    // minus
    gapped_list = TryMiscWithNs(21, 30, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasNs(), true);
    gapped_list.front()->CalculateRelevantIntervals(false, false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);


    // whole sequence
    gapped_list = TryMiscWithNs(0, 79);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasNs(), true);
    gapped_list.front()->CalculateRelevantIntervals(false, false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), true);
    // minus
    gapped_list = TryMiscWithNs(0, 79, true);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasNs(), true);
    gapped_list.front()->CalculateRelevantIntervals(false, false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), true);

}


void AddCodeBreak(CSeq_feat_Handle cds, TSeqPos anticodon_start)
{
    CSeq_annot_Handle p = cds.GetAnnot();
    CSeq_annot_EditHandle pe(p);
    CSeq_feat_EditHandle ce(cds);
    CRef<CSeq_feat> cpy(new CSeq_feat());
    cpy->Assign(*(cds.GetSeq_feat()));
    CRef<CCode_break> cb(new CCode_break());
    cb->SetLoc().SetInt().SetId().Assign(*(cpy->GetLocation().GetId()));
    cb->SetLoc().SetInt().SetFrom(anticodon_start);
    cb->SetLoc().SetInt().SetTo(anticodon_start + 2);
    cpy->SetData().SetCdregion().SetCode_break().push_back(cb);
    ce.Replace(*cpy);
}


CRef<CFeatGapInfo> MakeCDSWithCodeBreak(TSeqPos start, TSeqPos stop, TSeqPos anticodon_start)
{
    edit::TGappedFeatList gapped_list = TryOneCDSCase(start, stop, 10, CCdregion::eFrame_one);

    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    CSeq_feat_Handle cds = gapped_list[0]->GetFeature();
    AddCodeBreak(cds, anticodon_start);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    return gapped_list.front();
}


void TestCodeBreakSplit(TSeqPos start, TSeqPos stop, TSeqPos anticodon_start, bool first_cb, bool second_cb)
{
    CRef<CFeatGapInfo> cds = MakeCDSWithCodeBreak(start, stop, anticodon_start);
    cds->CalculateRelevantIntervals(false, true);
    vector<CRef<CSeq_feat> > adjusted_feats = cds->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 2);
    BOOST_CHECK_EQUAL(adjusted_feats[0]->GetData().GetCdregion().IsSetCode_break(), first_cb);
    BOOST_CHECK_EQUAL(adjusted_feats[1]->GetData().GetCdregion().IsSetCode_break(), second_cb);
}


void TestCodeBreakTrim(TSeqPos start, TSeqPos stop, TSeqPos anticodon_start, bool expect_code_break)
{
    CRef<CFeatGapInfo> cds = MakeCDSWithCodeBreak(start, stop, anticodon_start);
    cds->CalculateRelevantIntervals(false, true);
    vector<CRef<CSeq_feat> > adjusted_feats = cds->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 1);
    BOOST_CHECK_EQUAL(adjusted_feats[0]->GetData().GetCdregion().IsSetCode_break(), expect_code_break);

}


BOOST_AUTO_TEST_CASE(Test_CodeBreak)
{
    TestCodeBreakSplit(0, 32, 9, true, false);
    TestCodeBreakSplit(0, 32, 10, true, false);
    TestCodeBreakSplit(0, 32, 11, true, false);
    TestCodeBreakSplit(0, 32, 12, false, false);
    TestCodeBreakSplit(0, 32, 22, false, true);

    TestCodeBreakTrim(0, 17, 3, true);
    TestCodeBreakTrim(0, 17, 12, false);
    TestCodeBreakTrim(15, 32, 18, false);
    TestCodeBreakTrim(15, 32, 22, true);
}


CRef<CFeatGapInfo> MakeTrnaWithAnticodon(TSeqPos start, TSeqPos stop, TSeqPos anticodon_start)
{
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();

    CRef<CSeq_feat> trna = AddMiscFeature(entry);

    trna->SetLocation().SetInt().SetFrom(start);
    trna->SetLocation().SetInt().SetTo(stop);
    trna->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetId().Assign(*(trna->GetLocation().GetId()));
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetFrom(anticodon_start);
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().SetInt().SetTo(anticodon_start + 2);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    CFeat_CI f(seh);

    edit::TGappedFeatList gapped_list = ListGappedFeatures(f, *scope);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    return gapped_list.front();
}


void TestAnticodonSplit(TSeqPos start, TSeqPos stop, TSeqPos anticodon_start, bool first, bool second)
{
    CRef<CFeatGapInfo> trna = MakeTrnaWithAnticodon(start, stop, anticodon_start);
    trna->CalculateRelevantIntervals(false, true);
    vector<CRef<CSeq_feat> > adjusted_feats = trna->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 2);
    BOOST_CHECK_EQUAL(adjusted_feats[0]->GetData().GetRna().GetExt().GetTRNA().IsSetAnticodon(), first);
    BOOST_CHECK_EQUAL(adjusted_feats[1]->GetData().GetRna().GetExt().GetTRNA().IsSetAnticodon(), second);
}


void TestAnticodonTrim(TSeqPos start, TSeqPos stop, TSeqPos anticodon_start, bool expect_anticodon)
{
    CRef<CFeatGapInfo> trna = MakeTrnaWithAnticodon(start, stop, anticodon_start);
    trna->CalculateRelevantIntervals(false, true);
    vector<CRef<CSeq_feat> > adjusted_feats = trna->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 1);
    BOOST_CHECK_EQUAL(adjusted_feats[0]->GetData().GetRna().GetExt().GetTRNA().IsSetAnticodon(), expect_anticodon);

}


BOOST_AUTO_TEST_CASE(Test_Anticodon)
{
    TestAnticodonSplit(0, 32, 9, true, false);
    TestAnticodonSplit(0, 32, 10, true, false);
    TestAnticodonSplit(0, 32, 11, true, false);
    TestAnticodonSplit(0, 32, 12, false, false);
    TestAnticodonSplit(0, 32, 22, false, true);

    TestAnticodonTrim(0, 17, 3, true);
    TestAnticodonTrim(0, 17, 12, false);
    TestAnticodonTrim(15, 32, 18, false);
    TestAnticodonTrim(15, 32, 22, true);
}


edit::TGappedFeatList TryOneCaseMixLoc(TSeqPos start1, TSeqPos stop1, TSeqPos start2, TSeqPos stop2, bool is_minus = false)
{
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();
    // 12 AAA 10 NNN 12 AAA
    CRef<CSeq_feat> misc = AddMiscFeature(entry);
    const CSeq_id &id = misc->GetLocation().GetInt().GetId();

    CRef<CSeq_loc_mix> mix(new CSeq_loc_mix);

    CRef<CSeq_loc> add1(new CSeq_loc());
    add1->SetInt().SetId().Assign(id);
    add1->SetInt().SetFrom(start1);
    add1->SetInt().SetTo(stop1);
    if (is_minus)
        add1->SetStrand(eNa_strand_minus);
    mix->Set().push_back(add1);

    CRef<CSeq_loc> add2(new CSeq_loc());
    add2->SetInt().SetId().Assign(id);
    add2->SetInt().SetFrom(start2);
    add2->SetInt().SetTo(stop2);
    if (is_minus)
      add2->SetStrand(eNa_strand_minus);
    mix->Set().push_back(add2);

    misc->SetLocation().SetMix().Assign(*mix);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    CFeat_CI f(seh);

    edit::TGappedFeatList gapped_list = ListGappedFeatures(f, *scope);
    return gapped_list;
}

void TestTrimMiscOnRightMixLoc(TSeqPos start1, TSeqPos stop1, TSeqPos start2, TSeqPos stop2, bool is_minus)
{
    edit::TGappedFeatList gapped_list = TryOneCaseMixLoc(start1, stop1, start2, stop2, is_minus);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 1);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStart(eExtreme_Positional), 0);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStop(eExtreme_Positional), 11);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStart(eExtreme_Positional), false);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStop(eExtreme_Positional), true);
}

void TestTrimMiscOnLeftMixLoc(TSeqPos start1, TSeqPos stop1, TSeqPos start2, TSeqPos stop2, bool is_minus)
{
    edit::TGappedFeatList gapped_list = TryOneCaseMixLoc(start1, stop1, start2, stop2, is_minus);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 1);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStart(eExtreme_Positional), 22);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStop(eExtreme_Positional), 33);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStart(eExtreme_Positional), true);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStop(eExtreme_Positional), false);
}

void TestSplitMiscMixLoc(TSeqPos start1, TSeqPos stop1, TSeqPos start2, TSeqPos stop2, bool is_minus)
{
    edit::TGappedFeatList gapped_list = TryOneCaseMixLoc(start1, stop1, start2, stop2, is_minus);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), true);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 2);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStart(eExtreme_Positional), 0);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStop(eExtreme_Positional), 11);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStart(eExtreme_Positional), false);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStop(eExtreme_Positional), true);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().GetStart(eExtreme_Positional), 22);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().GetStop(eExtreme_Positional), 33);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().IsPartialStart(eExtreme_Positional), true);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().IsPartialStop(eExtreme_Positional), false);
}

void TestRemoveMiscMixLoc(TSeqPos start1, TSeqPos stop1, TSeqPos start2, TSeqPos stop2, bool is_minus)
{
    edit::TGappedFeatList gapped_list = TryOneCaseMixLoc(start1, stop1, start2, stop2, is_minus);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 0);
}

BOOST_AUTO_TEST_CASE(Test_FeatGapInfoMiscMixLoc)
{
    // no gap
    edit::TGappedFeatList gapped_list = TryOneCaseMixLoc(0, 5, 9, 11);
    BOOST_CHECK_EQUAL(gapped_list.size(), 0);
    gapped_list = TryOneCaseMixLoc(9, 11, 0, 5, true);
    BOOST_CHECK_EQUAL(gapped_list.size(), 0);
    gapped_list = TryOneCaseMixLoc(25, 28, 30, 34);
    BOOST_CHECK_EQUAL(gapped_list.size(), 0);
    gapped_list = TryOneCaseMixLoc(30, 34, 25, 28, true);
    BOOST_CHECK_EQUAL(gapped_list.size(), 0);

    // trim on right
    TestTrimMiscOnRightMixLoc(0, 5, 10, 15, false);
    TestTrimMiscOnRightMixLoc(10, 15, 0, 5, true);

    // trim on left
    TestTrimMiscOnLeftMixLoc(17, 25, 29, 33, false);
    TestTrimMiscOnLeftMixLoc(29, 33, 17, 25, true);

    // split
    TestSplitMiscMixLoc(0, 11, 22, 33, false);
    TestSplitMiscMixLoc(22, 33, 0, 11, true);

    // remove
    TestRemoveMiscMixLoc(12, 15, 17, 21, false);
    TestRemoveMiscMixLoc(17, 21, 12, 15, true);
}


