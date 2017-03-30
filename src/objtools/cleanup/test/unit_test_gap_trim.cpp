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
#include <objects/misc/sequence_macros.hpp>
#include <objtools/cleanup/gap_trim.hpp>
#include <objtools/unit_test_util/unit_test_util.hpp>

#include <serial/objistrasn.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

using namespace unit_test_util;

// Needed under windows for some reason.
#ifdef BOOST_NO_EXCEPTIONS

namespace boost
{
    void throw_exception( std::exception const & e ) {
        throw e;
    }
}

#endif

TGappedFeatList TryOneCase(TSeqPos start, TSeqPos stop, bool is_minus = false)
{
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();

    CRef<CSeq_feat> misc = AddMiscFeature(entry);

    misc->SetLocation().SetInt().SetFrom(start);
    misc->SetLocation().SetInt().SetTo(stop);

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
    CFeat_CI f(seh);

    TGappedFeatList gapped_list = ListGappedFeatures(f, *scope);
    return gapped_list;
}

void TestTrimMiscOnRight(TSeqPos start, TSeqPos stop, bool is_minus)
{
    TGappedFeatList gapped_list = TryOneCase(start, stop, is_minus);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 1);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStart(eExtreme_Positional), 0);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStop(eExtreme_Positional), 11);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStart(eExtreme_Positional), false);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStop(eExtreme_Positional), true);
}


void TestTrimMiscOnLeft(TSeqPos start, TSeqPos stop, bool is_minus)
{
    TGappedFeatList gapped_list = TryOneCase(17, 33, is_minus);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 1);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStart(eExtreme_Positional), 22);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStop(eExtreme_Positional), 33);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStart(eExtreme_Positional), true);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStop(eExtreme_Positional), false);
}


void TestSplitMisc(TSeqPos start, TSeqPos stop, bool is_minus)
{
    TGappedFeatList gapped_list = TryOneCase(start, stop, is_minus);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), true);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true);
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
    TGappedFeatList gapped_list = TryOneCase(start, stop, is_minus);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 0);
}


BOOST_AUTO_TEST_CASE(Test_FeatGapInfoMisc)
{
    // no gap
    TGappedFeatList gapped_list = TryOneCase(0, 5);
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


TGappedFeatList TryOneCDSCase(TSeqPos start, TSeqPos stop, TSeqPos gap_len, CCdregion::TFrame frame_one)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodNucProtSet();
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

    CFeat_CI f(seh);
    TGappedFeatList gapped_list = ListGappedFeatures(f, *scope);
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


void TestSplitCDS(TSeqPos start, TSeqPos stop, TSeqPos gap_len, CCdregion::TFrame frame_one, CCdregion::TFrame frame_two)
{
    TGappedFeatList gapped_list = TryOneCDSCase(start, stop, gap_len, frame_one);

    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), true);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 2);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStart(eExtreme_Positional), start);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().GetStop(eExtreme_Positional), 11);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStart(eExtreme_Positional), false);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetLocation().IsPartialStop(eExtreme_Positional), true);
    BOOST_CHECK_EQUAL(adjusted_feats.front()->GetData().GetCdregion().GetFrame(), frame_one);
    CheckQual(*(adjusted_feats.front()), "orig_protein_id", "x_1");
    CheckQual(*(adjusted_feats.front()), "orig_transcript_id", "y_1");
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().GetStart(eExtreme_Positional), 11 + gap_len + 1);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().GetStop(eExtreme_Positional), stop);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().IsPartialStart(eExtreme_Positional), true);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetLocation().IsPartialStop(eExtreme_Positional), false);
    BOOST_CHECK_EQUAL(adjusted_feats.back()->GetData().GetCdregion().GetFrame(), frame_two);
    CheckQual(*(adjusted_feats.back()), "orig_protein_id", "x_2");
    CheckQual(*(adjusted_feats.back()), "orig_transcript_id", "y_2");
}


void TestTrimCDS(TSeqPos start, TSeqPos stop, CCdregion::TFrame frame_before, CCdregion::TFrame frame_after)
{
    TGappedFeatList gapped_list = TryOneCDSCase(start, stop, 10, frame_before);

    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), false);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true);
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
}


BOOST_AUTO_TEST_CASE(Test_FeatGapInfoCDS)
{
    TestSplitCDS(0, 32, 9, CCdregion::eFrame_one, CCdregion::eFrame_one);
    TestSplitCDS(0, 32, 10, CCdregion::eFrame_one, CCdregion::eFrame_two);
    TestSplitCDS(0, 32, 11, CCdregion::eFrame_one, CCdregion::eFrame_three);

    TestSplitCDS(0, 32, 9, CCdregion::eFrame_two, CCdregion::eFrame_two);
    TestSplitCDS(0, 32, 10, CCdregion::eFrame_two, CCdregion::eFrame_three);
    TestSplitCDS(0, 32, 11, CCdregion::eFrame_two, CCdregion::eFrame_one);

    TestSplitCDS(0, 32, 9, CCdregion::eFrame_three, CCdregion::eFrame_three);
    TestSplitCDS(0, 32, 10, CCdregion::eFrame_three, CCdregion::eFrame_one);
    TestSplitCDS(0, 32, 11, CCdregion::eFrame_three, CCdregion::eFrame_two);

    // trim left
    TestTrimCDS(16, 33, CCdregion::eFrame_one, CCdregion::eFrame_one);
    TestTrimCDS(17, 33, CCdregion::eFrame_one, CCdregion::eFrame_three);
    TestTrimCDS(18, 33, CCdregion::eFrame_one, CCdregion::eFrame_two);
    // trim right
    TestTrimCDS(0, 15, CCdregion::eFrame_one, CCdregion::eFrame_one);
}


BOOST_AUTO_TEST_CASE(Test_FeatGapInfoCDSGapInIntron)
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
    gap_seg->SetLiteral().SetSeq_data().SetGap();
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
    TGappedFeatList gapped_list = ListGappedFeatures(f, *scope);
    BOOST_CHECK_EQUAL(gapped_list.size(), 1);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasKnown(), true);
    BOOST_CHECK_EQUAL(gapped_list.front()->HasUnknown(), false);
    gapped_list.front()->CalculateRelevantIntervals(false, true);
    BOOST_CHECK_EQUAL(gapped_list.front()->ShouldRemove(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Trimmable(), false);
    BOOST_CHECK_EQUAL(gapped_list.front()->Splittable(), true);
    vector<CRef<CSeq_feat> > adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, true);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 2);
    adjusted_feats = gapped_list.front()->AdjustForRelevantGapIntervals(true, true, true, false);
    BOOST_CHECK_EQUAL(adjusted_feats.size(), 1);
}




