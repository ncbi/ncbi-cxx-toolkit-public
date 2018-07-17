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
*   Unit tests for feature propagation.
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

#include <objects/misc/sequence_macros.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/seqport_util.hpp>

#include <objects/seqalign/Dense_seg.hpp>

#include <objtools/unit_test_util/unit_test_util.hpp>

#include <objtools/edit/feature_propagate.hpp>

#include <corelib/ncbiapp.hpp>

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


void CheckPropagatedLocation(const CSeq_loc& expected, const CSeq_loc& propagated)
{
    BOOST_CHECK_EQUAL(expected.GetInt().GetFrom(), propagated.GetInt().GetFrom());
    BOOST_CHECK_EQUAL(expected.GetInt().GetTo(), propagated.GetInt().GetTo());
    BOOST_CHECK_EQUAL(expected.GetInt().GetId().GetLocal().GetStr(), propagated.GetInt().GetId().GetLocal().GetStr());
    if (expected.IsSetStrand()) {
        BOOST_CHECK_EQUAL(expected.GetStrand(), propagated.GetStrand());
    } else {
        BOOST_CHECK_EQUAL(propagated.IsSetStrand(), false);
    }

    BOOST_CHECK_EQUAL(expected.IsPartialStart(eExtreme_Biological), propagated.IsPartialStart(eExtreme_Biological));
    BOOST_CHECK_EQUAL(expected.IsPartialStop(eExtreme_Biological), propagated.IsPartialStop(eExtreme_Biological));

}


void CheckPropagatedLocations(CSeq_entry_Handle seh, vector<CRef<CSeq_loc> > expected_loc, vector<CRef<CSeq_loc> > expected_subloc, bool from_first)
{
    CConstRef<CSeq_align> align = seh.GetSet().GetCompleteBioseq_set()->GetAnnot().front()->GetData().GetAlign().front();

    CBioseq_CI b1(seh);
    CBioseq_Handle src;
    if (from_first) {
        src = *b1;
    } else {
        ++b1;
        ++b1;
        src = *b1;
    }

    CBioseq_CI b(seh);
    size_t offset = 0;
    while (b) {
        if (*b == src) {
            ++b;
            continue;
        }
        CMessageListener_Basic listener;
        edit::CFeaturePropagator propagator(src, *b, *align, false, false, false, &listener);

        CFeat_CI f(src);
        while (f) {
            CRef<CSeq_feat> new_feat = propagator.Propagate(*(f->GetOriginalSeq_feat()));
            BOOST_CHECK_EQUAL(new_feat->GetData().GetSubtype(), f->GetData().GetSubtype());
            CheckPropagatedLocation(*(expected_loc[offset]), new_feat->GetLocation());
            if (f->GetData().IsCdregion()) {
                if (f->GetData().GetCdregion().IsSetCode_break()) {
                    if (from_first) {
                        CheckPropagatedLocation(*(expected_subloc[offset]), new_feat->GetData().GetCdregion().GetCode_break().front()->GetLoc());
                        BOOST_CHECK_EQUAL(listener.Count(), 0);
                    } else {
                        BOOST_CHECK_EQUAL(new_feat->GetData().GetCdregion().IsSetCode_break(), false);
                        BOOST_CHECK_EQUAL(listener.Count(), 1);
                        BOOST_CHECK_EQUAL(NStr::StartsWith(listener.GetMessage(0).GetText(),
                            "Unable to propagate location of translation exception:"), true);
                        BOOST_CHECK_EQUAL(listener.GetMessage(0).GetCode(), 
                             edit::CFeaturePropagator::eFeaturePropagationProblem_CodeBreakLocation);
                    }
                } else {
                    BOOST_CHECK_EQUAL(new_feat->GetData().GetCdregion().IsSetCode_break(), false);
                    BOOST_CHECK_EQUAL(listener.Count(), 0);
                }
            } else if (f->GetData().GetSubtype() == CSeqFeatData::eSubtype_tRNA) {
                if (f->GetData().GetRna().IsSetExt()) {
                    if (from_first) {
                        CheckPropagatedLocation(*(expected_subloc[offset]), new_feat->GetData().GetRna().GetExt().GetTRNA().GetAnticodon());
                        BOOST_CHECK_EQUAL(listener.Count(), 0);
                    } else {
                        BOOST_CHECK_EQUAL(new_feat->GetData().GetRna().GetExt().GetTRNA().IsSetAnticodon(), false);
                        BOOST_CHECK_EQUAL(listener.Count(), 1);
                        BOOST_CHECK_EQUAL(NStr::StartsWith(listener.GetMessage(0).GetText(),
                            "Unable to propagate location of anticodon:"), true);
                        BOOST_CHECK_EQUAL(listener.GetMessage(0).GetCode(), 
                             edit::CFeaturePropagator::eFeaturePropagationProblem_AnticodonLocation);
                    }
                } else {
                    BOOST_CHECK_EQUAL(new_feat->GetData().GetRna().IsSetExt(), false);
                    BOOST_CHECK_EQUAL(listener.Count(), 0);
                }
            } else {
                BOOST_CHECK_EQUAL(listener.Count(), 0);
            }
            listener.Clear();
            ++f;
        }
        offset++;
        ++b;
    }
}


void TestFeaturePropagation(bool loc_partial5, bool loc_partial3, bool subloc_partial5, bool subloc_partial3, bool from_first)
{
    size_t front_insert = 5;
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSetWithAlign(front_insert);
    CRef<CSeq_align> align = entry->SetSet().SetAnnot().front()->SetData().SetAlign().front();

    CRef<CSeq_entry> first = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_entry> last = entry->SetSet().SetSeq_set().back();

    CRef<CSeq_entry> use_seq = from_first ? first : last;

    CRef<CSeq_loc> main_loc(new CSeq_loc());
    main_loc->SetInt().SetFrom(0);
    main_loc->SetInt().SetTo(15);
    main_loc->SetInt().SetId().Assign(*(use_seq->GetSeq().GetId().front()));
    main_loc->SetPartialStart(loc_partial5, eExtreme_Biological);
    main_loc->SetPartialStop(loc_partial3, eExtreme_Biological);

    CRef<CSeq_loc> subloc(new CSeq_loc());
    subloc->SetInt().SetFrom(3);
    subloc->SetInt().SetTo(5);
    subloc->SetInt().SetId().Assign(*(use_seq->GetSeq().GetId().front()));
    subloc->SetPartialStart(subloc_partial5, eExtreme_Biological);
    subloc->SetPartialStop(subloc_partial3, eExtreme_Biological);

    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(use_seq, 15);
    misc->SetLocation().Assign(*main_loc);

    CRef<CSeq_feat> cds1 = unit_test_util::AddMiscFeature(use_seq, 15);
    cds1->SetData().SetCdregion();
    cds1->SetLocation().Assign(*main_loc);

    CRef<CSeq_feat> cds2 = unit_test_util::AddMiscFeature(use_seq, 15);
    CRef<CCode_break> cbr(new CCode_break());
    cbr->SetLoc().Assign(*subloc);
    cds2->SetData().SetCdregion().SetCode_break().push_back(cbr);
    cds2->SetLocation().Assign(*main_loc);

    CRef<CSeq_feat> trna1 = unit_test_util::AddMiscFeature(use_seq, 15);
    trna1->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);
    trna1->SetLocation().Assign(*main_loc);

    CRef<CSeq_feat> trna2 = unit_test_util::AddMiscFeature(use_seq, 15);
    trna2->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);
    trna2->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().Assign(*subloc);
    trna2->SetLocation().Assign(*main_loc);

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry (*entry);

    vector<CRef<CSeq_loc> > expected_loc;
    vector<CRef<CSeq_loc> > expected_subloc;
    if (from_first) {
        CRef<CSeq_loc> loc1(new CSeq_loc());
        loc1->SetInt().SetFrom(front_insert);
        loc1->SetInt().SetTo(15 + front_insert);
        loc1->SetInt().SetId().SetLocal().SetStr("good2");
        loc1->SetPartialStart(loc_partial5, eExtreme_Biological);
        loc1->SetPartialStop(loc_partial3, eExtreme_Biological);
        expected_loc.push_back(loc1);

        CRef<CSeq_loc> loc2(new CSeq_loc());
        loc2->SetInt().SetFrom(front_insert * 2);
        loc2->SetInt().SetTo(15 + front_insert * 2);
        loc2->SetInt().SetId().SetLocal().SetStr("good3");
        loc2->SetPartialStart(loc_partial5, eExtreme_Biological);
        loc2->SetPartialStop(loc_partial3, eExtreme_Biological);
        expected_loc.push_back(loc2);

        CRef<CSeq_loc> sloc1(new CSeq_loc());
        sloc1->SetInt().SetFrom(3 + front_insert);
        sloc1->SetInt().SetTo(5 + front_insert);
        sloc1->SetInt().SetId().SetLocal().SetStr("good2");
        sloc1->SetPartialStart(subloc_partial5, eExtreme_Biological);
        sloc1->SetPartialStop(subloc_partial3, eExtreme_Biological);
        expected_subloc.push_back(sloc1);

        CRef<CSeq_loc> sloc2(new CSeq_loc());
        sloc2->SetInt().SetFrom(3 + front_insert * 2);
        sloc2->SetInt().SetTo(5 + front_insert * 2);
        sloc2->SetInt().SetId().SetLocal().SetStr("good3");
        sloc2->SetPartialStart(subloc_partial5, eExtreme_Biological);
        sloc2->SetPartialStop(subloc_partial3, eExtreme_Biological);
        expected_subloc.push_back(sloc2);
    } else {
        CRef<CSeq_loc> loc1(new CSeq_loc());
        loc1->SetInt().SetFrom(0);
        loc1->SetInt().SetTo(5);
        loc1->SetInt().SetId().SetLocal().SetStr("good1");
        loc1->SetPartialStart(true, eExtreme_Biological);
        loc1->SetPartialStop(loc_partial3, eExtreme_Biological);
        expected_loc.push_back(loc1);

        CRef<CSeq_loc> loc2(new CSeq_loc());
        loc2->SetInt().SetFrom(5);
        loc2->SetInt().SetTo(10);
        loc2->SetInt().SetId().SetLocal().SetStr("good2");
        loc2->SetPartialStart(true, eExtreme_Biological);
        loc2->SetPartialStop(loc_partial3, eExtreme_Biological);
        expected_loc.push_back(loc2);

        CRef<CSeq_loc> sloc1(new CSeq_loc());
        sloc1->SetInt().SetFrom(3 + front_insert);
        sloc1->SetInt().SetTo(5 + front_insert);
        sloc1->SetInt().SetId().SetLocal().SetStr("good1");
        sloc1->SetPartialStart(subloc_partial5, eExtreme_Biological);
        sloc1->SetPartialStop(subloc_partial3, eExtreme_Biological);
        expected_subloc.push_back(sloc1);

        CRef<CSeq_loc> sloc2(new CSeq_loc());
        sloc2->SetInt().SetFrom(3 + front_insert * 2);
        sloc2->SetInt().SetTo(5 + front_insert * 2);
        sloc2->SetInt().SetId().SetLocal().SetStr("good2");
        sloc2->SetPartialStart(subloc_partial5, eExtreme_Biological);
        sloc2->SetPartialStop(subloc_partial3, eExtreme_Biological);
        expected_subloc.push_back(sloc2);

    }

    CheckPropagatedLocations(seh, expected_loc, expected_subloc, from_first);

#if 0
    // temporarily commented out - there seems to be a problem with SeqLocMapper
    // and mixed strand alignments, which we do not expect to see in BankIt 
    // applications

    // flip strands
    align->SetSegs().SetDenseg().SetStrands().push_back(eNa_strand_plus);
    align->SetSegs().SetDenseg().SetStrands().push_back(eNa_strand_minus);
    align->SetSegs().SetDenseg().SetStrands().push_back(eNa_strand_minus);

    if (from_first) {
        for (size_t offset = 0; offset < 2; offset++) {
            expected_loc[offset]->SetStrand(eNa_strand_minus);
            expected_loc[offset]->SetInt().SetFrom(first->GetSeq().GetInst().GetLength() + ((offset + 1)  * front_insert) - 16);
            expected_loc[offset]->SetInt().SetTo(first->GetSeq().GetInst().GetLength() + ((offset + 1)  * front_insert) - 1);
            expected_subloc[offset]->SetStrand(eNa_strand_minus);
            expected_subloc[offset]->SetInt().SetFrom(first->GetSeq().GetInst().GetLength() + ((offset + 1)  * front_insert) - 6);
            expected_subloc[offset]->SetInt().SetTo(first->GetSeq().GetInst().GetLength() + ((offset + 1)  * front_insert) - 4);
        }
    } else {
        expected_loc[0]->SetStrand(eNa_strand_minus);
        expected_loc[0]->SetInt().SetFrom(54);
        expected_loc[0]->SetInt().SetTo(59);
        expected_loc[0]->SetPartialStart(loc_partial5, eExtreme_Biological);
        expected_loc[0]->SetPartialStop(loc_partial3, eExtreme_Biological);
        expected_loc[1]->SetStrand(eNa_strand_minus);
        expected_loc[1]->SetInt().SetFrom(5);
        expected_loc[1]->SetInt().SetTo(10);
        expected_loc[1]->SetPartialStart(loc_partial5, eExtreme_Biological);
        expected_loc[1]->SetPartialStop(loc_partial3, eExtreme_Biological);

    }

    CheckPropagatedLocations(seh, expected_loc, expected_subloc, from_first);
#endif
}


BOOST_AUTO_TEST_CASE(Test_FeaturePropagation)
{
    bool from_first = true;

    TestFeaturePropagation(false, false, false, false, from_first);
    TestFeaturePropagation(false, false, false, true, from_first);
    TestFeaturePropagation(false, false, true, false, from_first);
    TestFeaturePropagation(false, false, true, true, from_first);
    TestFeaturePropagation(false, true, false, false, from_first);
    TestFeaturePropagation(false, true, false, true, from_first);
    TestFeaturePropagation(false, true, true, false, from_first);
    TestFeaturePropagation(false, true, true, true, from_first);
    TestFeaturePropagation(true, false, false, false, from_first);
    TestFeaturePropagation(true, false, false, true, from_first);
    TestFeaturePropagation(true, false, true, false, from_first);
    TestFeaturePropagation(true, false, true, true, from_first);
    TestFeaturePropagation(true, true, false, false, from_first);
    TestFeaturePropagation(true, true, false, true, from_first);
    TestFeaturePropagation(true, true, true, false, from_first);
    TestFeaturePropagation(true, true, true, true, from_first);

    from_first = false;

    TestFeaturePropagation(false, false, false, false, from_first);
    TestFeaturePropagation(false, false, false, true, from_first);
    TestFeaturePropagation(false, false, true, false, from_first);
    TestFeaturePropagation(false, false, true, true, from_first);
    TestFeaturePropagation(false, true, false, false, from_first);
    TestFeaturePropagation(false, true, false, true, from_first);
    TestFeaturePropagation(false, true, true, false, from_first);
    TestFeaturePropagation(false, true, true, true, from_first);
    TestFeaturePropagation(true, false, false, false, from_first);
    TestFeaturePropagation(true, false, false, true, from_first);
    TestFeaturePropagation(true, false, true, false, from_first);
    TestFeaturePropagation(true, false, true, true, from_first);
    TestFeaturePropagation(true, true, false, false, from_first);
    TestFeaturePropagation(true, true, false, true, from_first);
    TestFeaturePropagation(true, true, true, false, from_first);
    TestFeaturePropagation(true, true, true, true, from_first);
}


void CheckPropagatedCDSLocation(CSeq_entry& entry, const CSeq_feat& cds, 
                                bool stop_at_stop, bool fix_partials,
                                const vector<CRef<CSeq_loc> >& expected_loc)
{
    CRef<CSeq_align> align = entry.SetSet().SetAnnot().front()->SetData().SetAlign().front();
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry (entry);

    CBioseq_CI b(seh);
    CBioseq_Handle src = *b;
    ++b;
    size_t offset = 0;
    while (b) {
        CMessageListener_Basic listener;
        edit::CFeaturePropagator propagator(src, *b, *align, stop_at_stop, fix_partials, false, &listener);

        CRef<CSeq_feat> new_feat = propagator.Propagate(cds);
        BOOST_CHECK_EQUAL(new_feat->GetData().GetSubtype(), CSeqFeatData::eSubtype_cdregion);
        CheckPropagatedLocation(*(expected_loc[offset]), new_feat->GetLocation());
        BOOST_CHECK_EQUAL(listener.Count(), 0);
        listener.Clear();
        offset++;
        ++b;
    }

}


void InsertStop(CBioseq& seq, size_t pos)
{
    string na = seq.GetInst().GetSeq_data().GetIupacna();
    string before = na.substr(0, pos);
    string after = na.substr(pos + 3);
    na = before + "TAA" + after;
    seq.SetInst().SetSeq_data().SetIupacna().Set(na);
}


BOOST_AUTO_TEST_CASE(Test_CdRegionAlterations)
{
    size_t front_insert = 5;
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSetWithAlign(front_insert);

    CRef<CSeq_entry> first = entry->SetSet().SetSeq_set().front();

    CRef<CSeq_feat> cds = unit_test_util::AddMiscFeature(first, 15);
    cds->SetData().SetCdregion();

    // for this test, there are no stops
    vector<CRef<CSeq_loc> > expected_loc;

    CRef<CSeq_loc> loc1(new CSeq_loc());
    loc1->SetInt().SetFrom(front_insert);
    loc1->SetInt().SetTo(15 + front_insert);
    loc1->SetInt().SetId().SetLocal().SetStr("good2");
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc1->SetPartialStop(false, eExtreme_Biological);
    expected_loc.push_back(loc1);

    CRef<CSeq_loc> loc2(new CSeq_loc());
    loc2->SetInt().SetFrom(front_insert * 2);
    loc2->SetInt().SetTo(15 + front_insert * 2);
    loc2->SetInt().SetId().SetLocal().SetStr("good3");
    loc2->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    expected_loc.push_back(loc2);

    CheckPropagatedCDSLocation(*entry, *cds, true, false, expected_loc);
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStart(true, eExtreme_Biological);
    loc1->SetPartialStop(true, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    CheckPropagatedCDSLocation(*entry, *cds, true, true, expected_loc);
    CheckPropagatedCDSLocation(*entry, *cds, false, true, expected_loc);

    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStart(false, eExtreme_Biological);
    loc1->SetPartialStop(false, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);


    // repeat test with stops inserted for extension
    size_t offset = 0;
    for (auto s : entry->SetSet().SetSeq_set()) {
        if (offset > 0) {
            InsertStop(s->SetSeq(), 15 + (front_insert * offset) + 6);
        }
        offset++;
    }
    loc1->SetInt().SetTo(15 + front_insert + 8);
    loc2->SetInt().SetTo(15 + 2 * front_insert + 8);

    CheckPropagatedCDSLocation(*entry, *cds, true, false, expected_loc);
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStart(true, eExtreme_Biological);
    CheckPropagatedCDSLocation(*entry, *cds, true, true, expected_loc);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStart(false, eExtreme_Biological);


    // repeat test with stops inserted for truncation
    offset = 0;
    for (auto s : entry->SetSet().SetSeq_set()) {
        if (offset > 0) {
            // need to make three stop codons, frame will go to be the longest one
            InsertStop(s->SetSeq(), 15 + (front_insert * offset) - 14);
            InsertStop(s->SetSeq(), 15 + (front_insert * offset) - 10);
            InsertStop(s->SetSeq(), 15 + (front_insert * offset) - 6);
        }
        offset++;
    }
    loc1->SetInt().SetTo(15 + front_insert - 4);
    loc2->SetInt().SetTo(15 + 2 * front_insert - 4);

    CheckPropagatedCDSLocation(*entry, *cds, true, false, expected_loc);
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStart(true, eExtreme_Biological);
    CheckPropagatedCDSLocation(*entry, *cds, true, true, expected_loc);
}


void ImproveAlignment(CSeq_align& align, size_t front_insert)
{
    CDense_seg& denseg = align.SetSegs().SetDenseg();
    denseg.SetNumseg(3);
    denseg.ResetLens();
    denseg.SetLens().push_back(front_insert);
    denseg.SetLens().push_back(front_insert);
    denseg.SetLens().push_back(60);
    denseg.ResetStarts();
    denseg.SetStarts().push_back(-1);
    denseg.SetStarts().push_back(-1);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(-1);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(front_insert);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(front_insert);
    denseg.SetStarts().push_back(front_insert * 2);
}


BOOST_AUTO_TEST_CASE(Test_PropagateAll)
{
    size_t front_insert = 10;
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSetWithAlign(front_insert);
    CRef<CSeq_align> align = entry->SetSet().SetAnnot().front()->SetData().SetAlign().front();
    // make a better alignment, with some sequences in the gap at the front
    ImproveAlignment(*align, front_insert);

    CRef<CSeq_entry> last = entry->SetSet().SetSeq_set().back();
    
    // will not be able to propagate the first feature to either of the
    // other sequences.
    // second feature can only be propagated to the middle sequence.
    // third feature can be propagated to all.

    CRef<CSeq_feat> misc1 = unit_test_util::AddMiscFeature(last, front_insert - 1);
    CRef<CSeq_feat> misc2 = unit_test_util::AddMiscFeature(last, (2 * front_insert) - 1);
    CRef<CSeq_feat> misc3 = unit_test_util::AddMiscFeature(last, 4 * front_insert);

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry (*entry);

    CBioseq_CI b1(seh);
    ++b1;
    ++b1;
    CBioseq_Handle src = *b1;

    CBioseq_CI b(seh);

    CMessageListener_Basic listener;
    edit::CFeaturePropagator propagator1(src, *b, *align, false, false, false, &listener);
    vector<CRef<CSeq_feat> > first_feats = propagator1.PropagateAll();
    BOOST_CHECK_EQUAL(first_feats.size(), 1);
    BOOST_CHECK_EQUAL(listener.Count(), 2);
    BOOST_CHECK_EQUAL(listener.GetMessage(0).GetText(), "Unable to propagate location of feature: lcl|good3:1-20");
    BOOST_CHECK_EQUAL(listener.GetMessage(1).GetText(), "Unable to propagate location of feature: lcl|good3:1-10");
    listener.Clear();
    ++b;
    edit::CFeaturePropagator propagator2(src, *b, *align, false, false, false, &listener);
    vector<CRef<CSeq_feat> > second_feats = propagator2.PropagateAll();
    BOOST_CHECK_EQUAL(second_feats.size(), 2);
    BOOST_CHECK_EQUAL(listener.Count(), 1);
    BOOST_CHECK_EQUAL(listener.GetMessage(0).GetText(), "Unable to propagate location of feature: lcl|good3:1-10");
    
}


BOOST_AUTO_TEST_CASE(Test_MergeIntervals)
{
    size_t front_insert = 5;
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSetWithAlign(front_insert);
    CRef<CSeq_align> align = entry->SetSet().SetAnnot().front()->SetData().SetAlign().front();

    CRef<CSeq_entry> first = entry->SetSet().SetSeq_set().front();

    CRef<CSeq_feat> misc = unit_test_util::AddMiscFeature(first);
    CRef<CSeq_loc> l1(new CSeq_loc());
    l1->Assign(misc->GetLocation());
    CRef<CSeq_loc> l2(new CSeq_loc());
    l2->Assign(misc->GetLocation());
    l2->SetInt().SetFrom(l1->GetStop(eExtreme_Biological) + 1);
    l2->SetInt().SetTo(l2->GetInt().GetFrom() + 15);
    misc->SetLocation().SetMix().Set().push_back(l1);
    misc->SetLocation().SetMix().Set().push_back(l2);

    vector<CRef<CSeq_loc> > expected_loc;

    CRef<CSeq_loc> loc1(new CSeq_loc());
    loc1->SetInt().SetFrom(front_insert);
    loc1->SetInt().SetTo(30 + front_insert);
    loc1->SetInt().SetId().SetLocal().SetStr("good2");
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc1->SetPartialStop(false, eExtreme_Biological);
    expected_loc.push_back(loc1);

    CRef<CSeq_loc> loc2(new CSeq_loc());
    loc2->SetInt().SetFrom(front_insert * 2);
    loc2->SetInt().SetTo(30 + front_insert * 2);
    loc2->SetInt().SetId().SetLocal().SetStr("good3");
    loc2->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    expected_loc.push_back(loc2);

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry (*entry);
    CMessageListener_Basic listener;

    CBioseq_CI b(seh);
    CBioseq_Handle src = *b;
    ++b;
    size_t offset = 0;
    while (b) {
        edit::CFeaturePropagator propagator(src, *b, *align, false, false, true, &listener);

        CRef<CSeq_feat> new_feat = propagator.Propagate(*misc);
        CheckPropagatedLocation(*(expected_loc[offset]), new_feat->GetLocation());
        BOOST_CHECK_EQUAL(listener.Count(), 0);
        listener.Clear();
        offset++;
        ++b;
    }

}


END_SCOPE(objects)
END_NCBI_SCOPE

