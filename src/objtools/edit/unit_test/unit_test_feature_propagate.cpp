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
#include <objects/seq/seqport_util.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/align_ci.hpp>

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
    BOOST_CHECK(expected.Equals(propagated));
}

/*
good1: 60
good2: 65
good3: 70
  annot {
    {
      data align {
        {
          type global,
          dim 3,
          segs denseg {
            dim 3,
            numseg 1,
            ids {
              local str "good1",
              local str "good2",
              local str "good3"
            },
            starts {
              0,
              5,
              10
            },
            lens {
              60
            }
          }
        }
      }
    }
  }
}
*/

tuple<CRef<CSeq_entry>, CRef<CSeq_align>, CRef<CSeq_entry>, CRef<CSeq_entry>, CRef<CSeq_entry> >  CreateBioseqsAndAlign(size_t front_insert)
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSetWithAlign(front_insert);
    CRef<CSeq_align> align = entry->SetSet().SetAnnot().front()->SetData().SetAlign().front();
    auto it = entry->SetSet().SetSeq_set().begin();
    CRef<CSeq_entry> seq1 = *it;
    ++it;
    CRef<CSeq_entry> seq2 = *it;
    ++it;
    CRef<CSeq_entry> seq3 = *it;
    return make_tuple(entry, align, seq1, seq2, seq3);
}

tuple<CBioseq_Handle, CBioseq_Handle, CBioseq_Handle, CRef<CScope> > AddBioseqsToScope(CRef<CSeq_entry> entry)
{
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry (*entry);
    CBioseq_CI bi(seh);
    CBioseq_Handle bsh1 = *bi;
    ++bi;
    CBioseq_Handle bsh2 = *bi;
    ++bi;
    CBioseq_Handle bsh3 = *bi;

    return make_tuple(bsh1,bsh2,bsh3, scope);
}

CRef<CSeq_loc> CreateLoc(TSeqPos from, TSeqPos to, const CSeq_id &id, bool loc_partial5, bool loc_partial3, bool is_minus_strand = false)
{
    CRef<CSeq_loc> loc(new CSeq_loc());
    loc->SetInt().SetFrom(from);
    loc->SetInt().SetTo(to);
    loc->SetInt().SetId().Assign(id);
    if (is_minus_strand) {
        loc->SetInt().SetStrand(eNa_strand_minus);
    }
    loc->SetPartialStart(loc_partial5, eExtreme_Biological);
    loc->SetPartialStop(loc_partial3, eExtreme_Biological);
    return loc;
}

CRef<CSeq_loc> CreateTwoIntLoc(TSeqPos from1, TSeqPos to1, TSeqPos from2, TSeqPos to2, ENa_strand strand, const CSeq_id &id, bool loc_partial5, bool loc_partial3)
{
    CRef<CSeq_loc> loc1(new CSeq_loc());
    loc1->SetInt().SetFrom(from1);
    loc1->SetInt().SetTo(to1);
    loc1->SetInt().SetId().Assign(id);
    loc1->SetInt().SetStrand(strand);

    CRef<CSeq_loc> loc2(new CSeq_loc());
    loc2->SetInt().SetFrom(from2);
    loc2->SetInt().SetTo(to2);
    loc2->SetInt().SetId().Assign(id);
    loc2->SetInt().SetStrand(strand);

    CRef<CSeq_loc> loc(new CSeq_loc());
    loc->SetMix().AddSeqLoc(*loc1);
    loc->SetMix().AddSeqLoc(*loc2);
    loc->SetPartialStart(loc_partial5, eExtreme_Biological);
    loc->SetPartialStop(loc_partial3, eExtreme_Biological);
    return loc;
}

CRef<CSeq_loc> CreateOrderedLoc(TSeqPos from1, TSeqPos to1, TSeqPos from2, TSeqPos to2, ENa_strand strand, const CSeq_id &id, bool loc_partial5, bool loc_partial3)
{
    CRef<CSeq_loc> loc1(new CSeq_loc());
    loc1->SetInt().SetFrom(from1);
    loc1->SetInt().SetTo(to1);
    loc1->SetInt().SetId().Assign(id);
    loc1->SetInt().SetStrand(strand);

    CRef<CSeq_loc> loc2(new CSeq_loc());
    loc2->SetNull();

    CRef<CSeq_loc> loc3(new CSeq_loc());
    loc3->SetInt().SetFrom(from2);
    loc3->SetInt().SetTo(to2);
    loc3->SetInt().SetId().Assign(id);
    loc3->SetInt().SetStrand(strand);

    CRef<CSeq_loc> loc(new CSeq_loc());
    loc->SetMix().AddSeqLoc(*loc1);
    loc->SetMix().AddSeqLoc(*loc2);
    loc->SetMix().AddSeqLoc(*loc3);
    loc->SetPartialStart(loc_partial5, eExtreme_Biological);
    loc->SetPartialStop(loc_partial3, eExtreme_Biological);
    return loc;
}

CRef<CSeq_loc> CreatePointLoc(TSeqPos pos, const CSeq_id &id)
{
    CRef<CSeq_loc> loc(new CSeq_loc());
    loc->SetPnt().SetPoint(pos);
    loc->SetPnt().SetId().Assign(id);
    return loc;
}

CRef<CSeq_feat> CreateCds(CRef<CSeq_loc> main_loc, CRef<CSeq_entry> seq)
{
    CRef<CSeq_feat> cds = unit_test_util::AddMiscFeature(seq, 15);
    cds->SetData().SetCdregion();
    cds->SetLocation().Assign(*main_loc);
    return cds;
}

void AddCodeBreak(CRef<CSeq_feat> cds, CRef<CSeq_loc> subloc)
{
    CRef<CCode_break> cbr(new CCode_break());
    cbr->SetLoc().Assign(*subloc);
    cds->SetData().SetCdregion().SetCode_break().push_back(cbr);
}

CRef<CSeq_feat> CreateTrna(CRef<CSeq_loc> main_loc, CRef<CSeq_entry> seq)
{
    CRef<CSeq_feat> trna = unit_test_util::AddMiscFeature(seq, 15);
    trna->SetData().SetRna().SetType(CRNA_ref::eType_tRNA);
    trna->SetLocation().Assign(*main_loc);
    return trna;
}

void AddAnticodon(CRef<CSeq_feat> trna, CRef<CSeq_loc> subloc)
{
    trna->SetData().SetRna().SetExt().SetTRNA().SetAnticodon().Assign(*subloc);
}


// propagate cds without code-break from seq 1 to seq 2 and 3
void TestCds(bool loc_partial5, bool loc_partial3)
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateLoc(0, 15, id1, loc_partial5, loc_partial3);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq1);

    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh1, bsh2, *align, false, false, true, true, &listener);
    CRef<CSeq_loc> expected_loc1 = CreateLoc(front_insert, 15+front_insert, id2, loc_partial5, loc_partial3);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    BOOST_CHECK_EQUAL(new_feat1->GetData().GetSubtype(), cds->GetData().GetSubtype());
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
    BOOST_CHECK_EQUAL(new_feat1->GetData().GetCdregion().IsSetCode_break(), false);
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    listener.Clear();

    edit::CFeaturePropagator propagator2(bsh1, bsh3, *align, false, false, true, true, &listener);
    CRef<CSeq_loc> expected_loc2 = CreateLoc(front_insert*2, 15+front_insert*2, id3, loc_partial5, loc_partial3);
    CRef<CSeq_feat> new_feat2 = propagator2.Propagate(*cds);
    BOOST_CHECK_EQUAL(new_feat2->GetData().GetSubtype(), cds->GetData().GetSubtype());
    BOOST_CHECK(expected_loc2->Equals(new_feat2->GetLocation()));
    BOOST_CHECK_EQUAL(new_feat2->GetData().GetCdregion().IsSetCode_break(), false);
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    listener.Clear();
}



// propagate cds with code-break from seq 1 to seq 2 and 3
void TestCdsWithCodeBreak(bool subloc_partial5, bool subloc_partial3)
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateLoc(0, 15, id1, false, false);
    CRef<CSeq_loc> subloc = CreateLoc(3, 5, id1, subloc_partial5, subloc_partial3);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq1);
    AddCodeBreak(cds, subloc);

    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh1, bsh2, *align, false, false, true, true, &listener);
    CRef<CSeq_loc> expected_subloc1 = CreateLoc(3+front_insert, 5+front_insert, id2, subloc_partial5, subloc_partial3);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    BOOST_CHECK_EQUAL(new_feat1->GetData().GetCdregion().IsSetCode_break(), true);
    BOOST_CHECK(expected_subloc1->Equals(new_feat1->GetData().GetCdregion().GetCode_break().front()->GetLoc()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    listener.Clear();

    edit::CFeaturePropagator propagator2(bsh1, bsh3, *align, false, false, true, true, &listener);
    CRef<CSeq_loc> expected_subloc2 = CreateLoc(3+front_insert*2, 5+front_insert*2, id3, subloc_partial5, subloc_partial3);
    CRef<CSeq_feat> new_feat2 = propagator2.Propagate(*cds);
    BOOST_CHECK_EQUAL(new_feat2->GetData().GetCdregion().IsSetCode_break(), true);
    BOOST_CHECK(expected_subloc2->Equals(new_feat2->GetData().GetCdregion().GetCode_break().front()->GetLoc()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    listener.Clear();
}

// propagate cds without code-break from seq 3 to seq 1 and 2
void TestCdsFromLastBioseq(bool loc_partial5, bool loc_partial3)
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateLoc(0, 15, id3, loc_partial5, loc_partial3);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq3);

    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh3, bsh1, *align, false, false, true, true, &listener);
    CRef<CSeq_loc> expected_loc1 = CreateLoc(0, 5, id1, true, loc_partial3);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    BOOST_CHECK_EQUAL(new_feat1->GetData().GetSubtype(), cds->GetData().GetSubtype());
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
    BOOST_CHECK_EQUAL(new_feat1->GetData().GetCdregion().IsSetCode_break(), false);
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    listener.Clear();

    edit::CFeaturePropagator propagator2(bsh3, bsh2, *align, false, false, true, true, &listener);
    CRef<CSeq_loc> expected_loc2 = CreateLoc(5, 10, id2, true, loc_partial3);
    CRef<CSeq_feat> new_feat2 = propagator2.Propagate(*cds);
    BOOST_CHECK_EQUAL(new_feat2->GetData().GetSubtype(), cds->GetData().GetSubtype());
    BOOST_CHECK(expected_loc2->Equals(new_feat2->GetLocation()));
    BOOST_CHECK_EQUAL(new_feat2->GetData().GetCdregion().IsSetCode_break(), false);
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    listener.Clear();
}

// propagate cds with code-break from seq 3 to seq 1 and 2
void TestCdsFromLastBioseqWithCodeBreak()
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateLoc(0, 15, id3, false, false);
    CRef<CSeq_loc> subloc = CreateLoc(3, 5, id3, false, false);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq3);
    AddCodeBreak(cds, subloc);

    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh3, bsh1, *align, false, false, true, true, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    BOOST_CHECK_EQUAL(new_feat1->GetData().GetCdregion().IsSetCode_break(), false);
    BOOST_CHECK_EQUAL(listener.Count(), 1);
    BOOST_CHECK_EQUAL(NStr::StartsWith(listener.GetMessage(0).GetText(), "Unable to propagate location of translation exception"), true);
    BOOST_CHECK_EQUAL(listener.GetMessage(0).GetCode(), edit::CFeaturePropagator::eFeaturePropagationProblem_CodeBreakLocation);

    listener.Clear();

    edit::CFeaturePropagator propagator2(bsh3, bsh2, *align, false, false, true, true, &listener);
    CRef<CSeq_feat> new_feat2 = propagator2.Propagate(*cds);
    BOOST_CHECK_EQUAL(new_feat2->GetData().GetCdregion().IsSetCode_break(), false);
    BOOST_CHECK_EQUAL(listener.Count(), 1);
    BOOST_CHECK_EQUAL(NStr::StartsWith(listener.GetMessage(0).GetText(), "Unable to propagate location of translation exception"), true);
    BOOST_CHECK_EQUAL(listener.GetMessage(0).GetCode(), edit::CFeaturePropagator::eFeaturePropagationProblem_CodeBreakLocation);

    listener.Clear();
}

// propagate trna with anticodon from seq 1 to seq 2 and 3
void TestTrnaAnticodon(bool subloc_partial5, bool subloc_partial3)
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateLoc(0, 15, id1, false, false);
    CRef<CSeq_loc> subloc = CreateLoc(3, 5, id1, subloc_partial5, subloc_partial3);
    CRef<CSeq_feat> trna = CreateTrna(main_loc, seq1);
    AddAnticodon(trna, subloc);

    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh1, bsh2, *align, false, false, true, true, &listener);
    CRef<CSeq_loc> expected_subloc1 = CreateLoc(3+front_insert, 5+front_insert, id2, subloc_partial5, subloc_partial3);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*trna);
    BOOST_CHECK_EQUAL(new_feat1->GetData().GetRna().GetExt().GetTRNA().IsSetAnticodon(), true);
    BOOST_CHECK(expected_subloc1->Equals(new_feat1->GetData().GetRna().GetExt().GetTRNA().GetAnticodon()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    listener.Clear();

    edit::CFeaturePropagator propagator2(bsh1, bsh3, *align, false, false, true, true, &listener);
    CRef<CSeq_loc> expected_subloc2 = CreateLoc(3+front_insert*2, 5+front_insert*2, id3, subloc_partial5, subloc_partial3);
    CRef<CSeq_feat> new_feat2 = propagator2.Propagate(*trna);
    BOOST_CHECK_EQUAL(new_feat2->GetData().GetRna().GetExt().GetTRNA().IsSetAnticodon(), true);
    BOOST_CHECK(expected_subloc2->Equals(new_feat2->GetData().GetRna().GetExt().GetTRNA().GetAnticodon()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    listener.Clear();
}

// propagate trna with anticodon from seq 3 to seq 1 and 2
void TestTrnaAnticodonFromLastBioseq()
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateLoc(0, 15, id3, false, false);
    CRef<CSeq_loc> subloc = CreateLoc(3, 5, id3, false, false);
    CRef<CSeq_feat> trna = CreateTrna(main_loc, seq1);
    AddAnticodon(trna, subloc);

    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh3, bsh1, *align, false, false, true, true, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*trna);
    BOOST_CHECK_EQUAL(new_feat1->GetData().GetRna().GetExt().GetTRNA().IsSetAnticodon(), false);
    BOOST_CHECK_EQUAL(listener.Count(), 1);
    BOOST_CHECK_EQUAL(NStr::StartsWith(listener.GetMessage(0).GetText(), "Unable to propagate location of anticodon"), true);
    BOOST_CHECK_EQUAL(listener.GetMessage(0).GetCode(), edit::CFeaturePropagator::eFeaturePropagationProblem_AnticodonLocation);

    listener.Clear();

    edit::CFeaturePropagator propagator2(bsh3, bsh2, *align, false, false, true, true, &listener);
    CRef<CSeq_feat> new_feat2 = propagator2.Propagate(*trna);
    BOOST_CHECK_EQUAL(new_feat2->GetData().GetRna().GetExt().GetTRNA().IsSetAnticodon(), false);
    BOOST_CHECK_EQUAL(listener.Count(), 1);
    BOOST_CHECK_EQUAL(NStr::StartsWith(listener.GetMessage(0).GetText(), "Unable to propagate location of anticodon"), true);
    BOOST_CHECK_EQUAL(listener.GetMessage(0).GetCode(), edit::CFeaturePropagator::eFeaturePropagationProblem_AnticodonLocation);

    listener.Clear();
}

// propagate cds outside of the alignment from seq 3 to seq 1
void TestCdsFromLastBioseqOutsideAlign()
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateLoc(0, 5, id3, false, false);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq3);

    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh3, bsh1, *align, false, false, true, true, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    BOOST_CHECK(new_feat1.IsNull());

    listener.Clear();
}

// propagate 2 exon cds with 1 exon outside of the alignment from seq 3 to seq 1
void TestTwoIntCdsFromLastBioseqOutsideAlign()
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateTwoIntLoc(0, 5, 20, 30, eNa_strand_plus, id3, false, false);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq3);

    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh3, bsh1, *align, false, false, true, true, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    CRef<CSeq_loc> expected_loc1 = CreateLoc(20-front_insert*2, 30-front_insert*2, id1, true, false);
    expected_loc1->SetInt().SetStrand(eNa_strand_plus);
    BOOST_CHECK_EQUAL(new_feat1->GetData().GetSubtype(), cds->GetData().GetSubtype());
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
    BOOST_CHECK_EQUAL(new_feat1->GetData().GetCdregion().IsSetCode_break(), false);
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    listener.Clear();
}

// propagate 2 exon cds on minus strand from seq 3 to seq 1
void TestTwoIntCdsOnMinusStrand()
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateTwoIntLoc(20, 30, 5, 15, eNa_strand_minus, id3, true, true);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq3);
    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);
//    cout << "Bad order: " << sequence::BadSeqLocSortOrder(bsh3, *main_loc) << endl;
    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh3, bsh1, *align, false, false, true, true, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    CRef<CSeq_loc> expected_loc1 = CreateTwoIntLoc(10, 20, 0, 5, eNa_strand_minus, id1, true, true);
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    listener.Clear();
}

// test partial when the stop is cut off
void TestPartialWhenCutStop(bool partial3)
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateTwoIntLoc(5, 15, 20, 40, eNa_strand_plus, id1, false, partial3);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq1);
    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetNumseg(1);
    denseg.ResetLens();
    denseg.SetLens().push_back(30);
    denseg.ResetStarts();
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh1, bsh2, *align, false, false, true, true, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    /*CSeq_loc_Mapper_Options mapper_options(CSeq_loc_Mapper::fTrimMappedLocation);
    CRef<CSeq_loc_Mapper> mapper(new CSeq_loc_Mapper(*bsh1.GetSeqId(), *bsh2.GetSeqId(), *align, &bsh2.GetScope(), mapper_options));
    mapper->SetMergeAll();
    mapper->SetGapRemove();
    mapper->SetFuzzOption(CSeq_loc_Mapper::fFuzzOption_RemoveLimTlOrTr);
    CRef<CSeq_loc> new_loc = mapper->Map(cds->GetLocation());
    new_loc->ChangeToMix();
    cout << MSerial_AsnText << cds->GetLocation();
    cout << MSerial_AsnText << *new_loc;
    */
    CRef<CSeq_loc> expected_loc1 = CreateTwoIntLoc(5, 15, 20, 29, eNa_strand_plus, id2, false, true);
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
//    BOOST_CHECK(expected_loc1->Equals(*new_loc));
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    listener.Clear();
}

// test partial when the last interval is cut off
void TestPartialWhenCutLastInterval(bool partial3)
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateTwoIntLoc(5, 15, 40, 50, eNa_strand_plus, id1, false, partial3);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq1);
    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetNumseg(1);
    denseg.ResetLens();
    denseg.SetLens().push_back(30);
    denseg.ResetStarts();
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh1, bsh2, *align, false, false, true, true, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    /*CSeq_loc_Mapper_Options mapper_options(CSeq_loc_Mapper::fTrimMappedLocation);
    CRef<CSeq_loc_Mapper> mapper(new CSeq_loc_Mapper(*bsh1.GetSeqId(), *bsh2.GetSeqId(), *align, &bsh2.GetScope(), mapper_options));
    mapper->SetMergeAll();
    mapper->SetGapRemove();
    mapper->SetFuzzOption(CSeq_loc_Mapper::fFuzzOption_RemoveLimTlOrTr);
    CRef<CSeq_loc> new_loc = mapper->Map(cds->GetLocation());
    cout << MSerial_AsnText << cds->GetLocation();
    cout << MSerial_AsnText << *new_loc;
    */
    CRef<CSeq_loc> expected_loc1 = CreateLoc(5, 15, id2, false, true);
    expected_loc1->SetInt().SetStrand(eNa_strand_plus);
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
// BOOST_CHECK(expected_loc1->Equals(*new_loc));
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    listener.Clear();
}

// test partial when the start is cut off
void TestPartialWhenCutStart(bool partial5)
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateTwoIntLoc(5, 15, 20, 25, eNa_strand_plus, id1, partial5, false);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq1);
    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetNumseg(1);
    denseg.ResetLens();
    denseg.SetLens().push_back(30);
    denseg.ResetStarts();
    denseg.SetStarts().push_back(10);
    denseg.SetStarts().push_back(10);
    denseg.SetStarts().push_back(10);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh1, bsh2, *align, false, false, true, true, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    CRef<CSeq_loc> expected_loc1 = CreateTwoIntLoc(10, 15, 20, 25, eNa_strand_plus, id2, true, false);
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    listener.Clear();
}

// test fuse abutting intervals
void TestFuseAbuttingIntervals()
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateTwoIntLoc(5, 12, 17, 25, eNa_strand_plus, id1, false, false);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq1);
    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetNumseg(3);
    denseg.ResetLens();
    denseg.SetLens().push_back(10);
    denseg.SetLens().push_back(10);
    denseg.SetLens().push_back(10);
    denseg.ResetStarts();
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(10);
    denseg.SetStarts().push_back(-1);
    denseg.SetStarts().push_back(10);
    denseg.SetStarts().push_back(20);
    denseg.SetStarts().push_back(10);
    denseg.SetStarts().push_back(20);;

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh1, bsh2, *align, false, false, true, true, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    CRef<CSeq_loc> expected_loc1 = CreateLoc(5, 15, id2, false, false);
    expected_loc1->SetInt().SetStrand(eNa_strand_plus);
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    listener.Clear();
}

// test do not fuse abutting intervals
void TestDoNotFuseAbuttingIntervals()
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateTwoIntLoc(5, 12, 17, 25, eNa_strand_plus, id1, false, false);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq1);
    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetNumseg(3);
    denseg.ResetLens();
    denseg.SetLens().push_back(10);
    denseg.SetLens().push_back(10);
    denseg.SetLens().push_back(10);
    denseg.ResetStarts();
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(10);
    denseg.SetStarts().push_back(-1);
    denseg.SetStarts().push_back(10);
    denseg.SetStarts().push_back(20);
    denseg.SetStarts().push_back(10);
    denseg.SetStarts().push_back(20);;

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh1, bsh2, *align, false, false, false, true, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    CRef<CSeq_loc> expected_loc1 = CreateTwoIntLoc(5, 9, 10, 15, eNa_strand_plus, id2, false, false);
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    listener.Clear();
}

// test extend over gaps
void TestExtendOverGap()
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateLoc(5, 25, id1, false, false);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq1);
    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetNumseg(3);
    denseg.ResetLens();
    denseg.SetLens().push_back(10);
    denseg.SetLens().push_back(10);
    denseg.SetLens().push_back(10);
    denseg.ResetStarts();
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(10);
    denseg.SetStarts().push_back(-1);
    denseg.SetStarts().push_back(10);
    denseg.SetStarts().push_back(20);
    denseg.SetStarts().push_back(20);
    denseg.SetStarts().push_back(20);;

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh1, bsh2, *align, false, false, false, true, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    CRef<CSeq_loc> expected_loc1 = CreateLoc(5, 25, id2, false, false);
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    listener.Clear();
}

// test do not extend over gaps
void TestDoNotExtendOverGap()
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateLoc(5, 25, id2, false, false);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq2);
    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetNumseg(3);
    denseg.ResetLens();
    denseg.SetLens().push_back(10);
    denseg.SetLens().push_back(10);
    denseg.SetLens().push_back(10);
    denseg.ResetStarts();
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(10);
    denseg.SetStarts().push_back(-1);
    denseg.SetStarts().push_back(10);
    denseg.SetStarts().push_back(20);
    denseg.SetStarts().push_back(20);
    denseg.SetStarts().push_back(20);;

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh2, bsh1, *align, false, false, false, false, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    CRef<CSeq_loc> expected_loc1 = CreateTwoIntLoc(5, 9, 20, 25, eNa_strand_plus, id1, false, false);
    expected_loc1->ResetStrand();
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    listener.Clear();
}

// test ordered vs. joined locations
void TestOrderedLoc()
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateOrderedLoc(5, 15, 20, 30, eNa_strand_plus, id3, true, true);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq3);
    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);
    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh3, bsh1, *align, false, false, true, true, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    CRef<CSeq_loc> expected_loc1 = CreateOrderedLoc(0, 5, 10, 20, eNa_strand_plus, id1, true, true);
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    listener.Clear();
}

// test circular topology
void TestCircularTopology()
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    seq1->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_circular);
    seq2->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_circular);
    seq3->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_circular);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateTwoIntLoc(50, 59, 0, 5, eNa_strand_plus, id1, false, false);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq1);
    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetNumseg(3);
    denseg.ResetLens();
    denseg.SetLens().push_back(20);
    denseg.SetLens().push_back(20);
    denseg.SetLens().push_back(20);
    denseg.ResetStarts();
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(20);
    denseg.SetStarts().push_back(-1);
    denseg.SetStarts().push_back(-1);
    denseg.SetStarts().push_back(40);
    denseg.SetStarts().push_back(45);
    denseg.SetStarts().push_back(50);;

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh1, bsh2, *align, false, false, false, true, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    CRef<CSeq_loc> expected_loc1 = CreateTwoIntLoc(55, 64, 0, 5, eNa_strand_plus, id2, false, false);
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    listener.Clear();

}



// test point location inside alignment
void TestPointLocInside()
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreatePointLoc(15, id3);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq3);

    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh3, bsh1, *align, false, false, true, true, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    CRef<CSeq_loc> expected_loc1 = CreatePointLoc(5, id1);
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    listener.Clear();
}

// test point location outside alignment
void TestPointLocOutside()
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreatePointLoc(5, id3);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq3);

    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh3, bsh1, *align, false, false, true, true, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    BOOST_CHECK(new_feat1.IsNull());

    listener.Clear();
}

// test partial when the stop is cut off and do not extend
void TestPartialWhenCutStopDoNotExtend(bool partial3)
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateTwoIntLoc(5, 15, 20, 40, eNa_strand_plus, id1, false, partial3);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq1);
    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetNumseg(1);
    denseg.ResetLens();
    denseg.SetLens().push_back(30);
    denseg.ResetStarts();
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh1, bsh2, *align, false, false, true, false, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    CRef<CSeq_loc> expected_loc1 = CreateTwoIntLoc(5, 15, 20, 29, eNa_strand_plus, id2, false, true);
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    listener.Clear();
}

// test partial when the last interval is cut off and do not extend
void TestPartialWhenCutLastIntervalDoNotExtend(bool partial3)
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateTwoIntLoc(5, 15, 40, 50, eNa_strand_plus, id1, false, partial3);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq1);
    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetNumseg(1);
    denseg.ResetLens();
    denseg.SetLens().push_back(30);
    denseg.ResetStarts();
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh1, bsh2, *align, false, false, true, false, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    CRef<CSeq_loc> expected_loc1 = CreateLoc(5, 15, id2, false, true);
    expected_loc1->SetInt().SetStrand(eNa_strand_plus);
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    listener.Clear();
}

// test partial when the start is cut off and do not extend
void TestPartialWhenCutStartDoNotExtend(bool partial5)
{
    size_t front_insert = 5;
    CRef<CSeq_align> align;
    CRef<CSeq_entry> entry, seq1, seq2, seq3;
    tie(entry, align, seq1, seq2, seq3) = CreateBioseqsAndAlign(front_insert);

    const CSeq_id &id1 = *seq1->GetSeq().GetId().front();
    const CSeq_id &id2 = *seq2->GetSeq().GetId().front();
    const CSeq_id &id3 = *seq3->GetSeq().GetId().front();

    CRef<CSeq_loc> main_loc = CreateTwoIntLoc(5, 15, 20, 25, eNa_strand_plus, id1, partial5, false);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq1);
    CBioseq_Handle bsh1, bsh2, bsh3;
    CRef<CScope> scope;
    tie(bsh1,bsh2,bsh3,scope) = AddBioseqsToScope(entry);

    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetNumseg(1);
    denseg.ResetLens();
    denseg.SetLens().push_back(30);
    denseg.ResetStarts();
    denseg.SetStarts().push_back(10);
    denseg.SetStarts().push_back(10);
    denseg.SetStarts().push_back(10);

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh1, bsh2, *align, false, false, true, false, &listener);
    CRef<CSeq_feat> new_feat1 = propagator1.Propagate(*cds);
    CRef<CSeq_loc> expected_loc1 = CreateTwoIntLoc(10, 15, 20, 25, eNa_strand_plus, id2, true, false);
    BOOST_CHECK(expected_loc1->Equals(new_feat1->GetLocation()));
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    listener.Clear();
}

void TestFeatInsideGap(bool is_minus)
{
    CRef<CSeq_entry> entry(new CSeq_entry);

    string str1 = "TCACTCTTTGAAAAAAAAAA";
    CRef<CSeq_entry> seq1(new CSeq_entry);
    CRef< CSeq_id > id1(new CSeq_id);
    id1->SetLocal().SetStr("seq1");
    seq1->SetSeq().SetId().push_back(id1);
    seq1->SetSeq().SetInst().SetSeq_data().SetIupacna().Set(str1);
    seq1->SetSeq().SetInst().SetLength(str1.length());
    seq1->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
    seq1->SetSeq().SetInst().SetMol(CSeq_inst::eMol_na);
    entry->SetSet().SetSeq_set().push_back(seq1);

    string str2 = "TCACTGAAAAAAAAAA";
    CRef<CSeq_entry> seq2(new CSeq_entry);
    CRef< CSeq_id > id2(new CSeq_id);
    id2->SetLocal().SetStr("seq2");
    seq2->SetSeq().SetId().push_back(id2);
    seq2->SetSeq().SetInst().SetSeq_data().SetIupacna().Set(str2);
    seq2->SetSeq().SetInst().SetLength(str2.length());
    seq2->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
    seq2->SetSeq().SetInst().SetMol(CSeq_inst::eMol_na);
    entry->SetSet().SetSeq_set().push_back(seq2);

    CRef<CSeq_align> align(new CSeq_align());
    align->SetType(objects::CSeq_align::eType_global);
    align->SetDim(entry->GetSet().GetSeq_set().size());
    align->SetSegs().SetDenseg().SetIds().push_back(id1);
    align->SetSegs().SetDenseg().SetIds().push_back(id2);

    auto& denseg = align->SetSegs().SetDenseg();
    denseg.SetNumseg(3);
    denseg.SetLens().push_back(5);
    denseg.SetLens().push_back(4);
    denseg.SetLens().push_back(11);
    denseg.SetDim(entry->GetSet().GetSeq_set().size());
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(5);
    denseg.SetStarts().push_back(-1);
    denseg.SetStarts().push_back(9);
    denseg.SetStarts().push_back(5);

    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetAlign().push_back(align);
    entry->SetSet().SetAnnot().push_back(annot);

    CRef<CSeq_loc> main_loc = CreateLoc(6, 7, *id1, false, false, is_minus);
    CRef<CSeq_feat> cds = CreateCds(main_loc, seq1);

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry (*entry);
    CBioseq_CI bi(seh);
    CBioseq_Handle bsh1 = *bi;
    ++bi;
    CBioseq_Handle bsh2 = *bi;

    CMessageListener_Basic listener;

    edit::CFeaturePropagator propagator1(bsh1, bsh2, *align, false, false, true, true, &listener);
    CRef<CSeq_feat> new_feat = propagator1.Propagate(*cds);
    BOOST_CHECK(new_feat.IsNull());
    BOOST_CHECK_EQUAL(listener.Count(), 1);

    listener.Clear();
}

BOOST_AUTO_TEST_CASE(Test_FeaturePropagation)
{
    TestCds(false, false);
    TestCds(false, true);
    TestCds(true, false);
    TestCds(true, true);

    TestCdsWithCodeBreak(false, false);
    TestCdsWithCodeBreak(false, true);
    TestCdsWithCodeBreak(true, false);
    TestCdsWithCodeBreak(true, true);

    TestCdsFromLastBioseq(false, false);
    TestCdsFromLastBioseq(false, true);
    TestCdsFromLastBioseq(true, false);
    TestCdsFromLastBioseq(true, true);

    TestCdsFromLastBioseqWithCodeBreak();

    TestTrnaAnticodon(false, false);
    TestTrnaAnticodon(false, true);
    TestTrnaAnticodon(true, false);
    TestTrnaAnticodon(true, true);

    TestTrnaAnticodonFromLastBioseq();

    TestCdsFromLastBioseqOutsideAlign();

    TestTwoIntCdsFromLastBioseqOutsideAlign();

    TestTwoIntCdsOnMinusStrand();

    TestPartialWhenCutStop(false);
    TestPartialWhenCutStop(true);
    TestPartialWhenCutLastInterval(false);
    TestPartialWhenCutLastInterval(true);
    TestPartialWhenCutStart(false);
    TestPartialWhenCutStart(true);

    TestFuseAbuttingIntervals();
    TestDoNotFuseAbuttingIntervals();
    TestExtendOverGap();
    TestDoNotExtendOverGap();
    TestOrderedLoc();
    TestCircularTopology();
    TestPointLocInside();
    TestPointLocOutside();

    TestPartialWhenCutStopDoNotExtend(false);
    TestPartialWhenCutStopDoNotExtend(true);
    TestPartialWhenCutLastIntervalDoNotExtend(false);
    TestPartialWhenCutLastIntervalDoNotExtend(true);
    TestPartialWhenCutStartDoNotExtend(false);
    TestPartialWhenCutStartDoNotExtend(true);

    TestFeatInsideGap(false);
    TestFeatInsideGap(true);
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
        edit::CFeaturePropagator propagator(src, *b, *align, stop_at_stop, fix_partials, true, true, &listener);

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

// TODO? Bad alignment!

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
    edit::CFeaturePropagator propagator1(src, *b, *align, false, false, true, true, &listener);
    vector<CRef<CSeq_feat> > first_feats = propagator1.PropagateAll();
    BOOST_CHECK_EQUAL(first_feats.size(), 1);
    BOOST_CHECK_EQUAL(listener.Count(), 2);
    BOOST_CHECK_EQUAL(listener.GetMessage(0).GetText(), "Unable to propagate location of feature lcl|good3:1-20 to lcl|good1");
    BOOST_CHECK_EQUAL(listener.GetMessage(1).GetText(), "Unable to propagate location of feature lcl|good3:1-10 to lcl|good1");
    listener.Clear();

    ++b;
    edit::CFeaturePropagator propagator2(src, *b, *align, false, false, true, true, &listener);
    vector<CRef<CSeq_feat> > second_feats = propagator2.PropagateAll();
    BOOST_CHECK_EQUAL(second_feats.size(), 2);
    BOOST_CHECK_EQUAL(listener.Count(), 1);
    BOOST_CHECK_EQUAL(listener.GetMessage(0).GetText(), "Unable to propagate location of feature lcl|good3:1-10 to lcl|good2");
}

BOOST_AUTO_TEST_CASE(Test_PropagateAllReportFailures)
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
    edit::CFeaturePropagator propagator1(src, *b, *align, false, false, true, true, &listener);
    vector<CConstRef<CSeq_feat> > failures1;
    vector<CRef<CSeq_feat> > first_feats = propagator1.PropagateAllReportFailures(failures1);
    BOOST_CHECK_EQUAL(first_feats.size(), 1);
    BOOST_CHECK_EQUAL(listener.Count(), 2);
    BOOST_CHECK_EQUAL(listener.GetMessage(0).GetText(), "Unable to propagate location of feature lcl|good3:1-20 to lcl|good1");
    BOOST_CHECK_EQUAL(listener.GetMessage(1).GetText(), "Unable to propagate location of feature lcl|good3:1-10 to lcl|good1");
    listener.Clear();

    ++b;
    edit::CFeaturePropagator propagator2(src, *b, *align, false, false, true, true, &listener);
    vector<CConstRef<CSeq_feat> > failures2;
    vector<CRef<CSeq_feat> > second_feats = propagator2.PropagateAllReportFailures(failures2);
    BOOST_CHECK_EQUAL(second_feats.size(), 2);
    BOOST_CHECK_EQUAL(listener.Count(), 1);
    BOOST_CHECK_EQUAL(listener.GetMessage(0).GetText(), "Unable to propagate location of feature lcl|good3:1-10 to lcl|good2");
}

CObject_id::TId s_FindHighestFeatId(const CSeq_entry_Handle entry)
{
    CObject_id::TId id = 0;
    for (CFeat_CI feat_it(entry); feat_it; ++feat_it) {
        if (feat_it->IsSetId()) {
            const CFeat_id& feat_id = feat_it->GetId();
            if (feat_id.IsLocal() && feat_id.GetLocal().IsId() && feat_id.GetLocal().GetId() > id) {
                id = feat_id.GetLocal().GetId();
            }
        }
    }
    return id;
}

CSeq_entry_Handle GetGoodSeqEntryWithFeatureIds(int& feat_id)
{
    size_t front_insert = 5;
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSetWithAlign(front_insert);
    CRef<CSeq_align> align = entry->SetSet().SetAnnot().front()->SetData().SetAlign().front();

    CRef<CSeq_entry> first = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_entry> last = entry->SetSet().SetSeq_set().back();

    CRef<CSeq_loc> main_loc(new CSeq_loc());
    main_loc->SetInt().SetFrom(0);
    main_loc->SetInt().SetTo(15);
    main_loc->SetInt().SetId().Assign(*(first->GetSeq().GetId().front()));

    CRef<CSeq_loc> subloc(new CSeq_loc());
    subloc->SetInt().SetFrom(3);
    subloc->SetInt().SetTo(5);
    subloc->SetInt().SetId().Assign(*(first->GetSeq().GetId().front()));

    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(first, 15);
    gene->SetData().SetGene().SetLocus("gene locus");
    gene->SetId().SetLocal().SetId(++feat_id);
    gene->SetLocation().Assign(*main_loc);

    CRef<CSeq_feat> mrna = unit_test_util::AddMiscFeature(first, 15);
    mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    mrna->SetId().SetLocal().SetId(++feat_id);
    mrna->SetLocation().Assign(*main_loc);

    CRef<CSeq_feat> cds_withoutprot = unit_test_util::AddMiscFeature(first, 15);
    cds_withoutprot->SetComment("CDS without product");
    cds_withoutprot->SetData().SetCdregion();
    cds_withoutprot->SetId().SetLocal().SetId(++feat_id);
    cds_withoutprot->SetLocation().SetInt().SetFrom(10);
    cds_withoutprot->SetLocation().SetInt().SetTo(25);
    cds_withoutprot->SetLocation().SetInt().SetId().Assign(*(first->GetSeq().GetId().front()));

    CRef<CSeq_feat> cds_withprot = unit_test_util::MakeMiscFeature(unit_test_util::IdFromEntry(first), 15);
    cds_withprot->SetComment("CDS with product");
    cds_withprot->SetData().SetCdregion();
    cds_withprot->SetId().SetLocal().SetId(++feat_id);
    cds_withprot->SetLocation().Assign(*main_loc);

    // constructing the protein sequence
    CRef<CSeq_entry> prot_entry(new CSeq_entry());
    prot_entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
    prot_entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_aa);
    prot_entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("-WPKL");
    prot_entry->SetSeq().SetInst().SetLength(5);

    const string prot_id = "good1_1";
    CRef<CSeq_id> id(new CSeq_id());
    id->SetLocal().SetStr(prot_id);
    prot_entry->SetSeq().SetId().push_back(id);

    CRef<CSeqdesc> mdesc(new CSeqdesc());
    mdesc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
    prot_entry->SetSeq().SetDescr().Set().push_back(mdesc);

    CRef<CSeq_feat> prot_feat(new CSeq_feat());
    prot_feat->SetData().SetProt().SetName().push_back("hypothetical protein");
    prot_feat->SetLocation().SetInt().SetId().Assign(*(prot_entry->GetSeq().GetId().front()));
    prot_feat->SetLocation().SetInt().SetFrom(0);
    prot_feat->SetLocation().SetInt().SetTo(prot_entry->GetSeq().GetInst().GetLength() - 1);
    prot_feat->SetId().SetLocal().SetId(++feat_id);
    unit_test_util::AddFeat(prot_feat, prot_entry);

    cds_withprot->SetProduct().SetWhole().SetLocal().SetStr(prot_id);

    CRef<CBioseq_set> set(new CBioseq_set());
    set->SetClass(CBioseq_set::eClass_nuc_prot);
    set->SetSeq_set().push_back(first);
    set->SetSeq_set().push_back(prot_entry);
    CRef<CSeq_entry> set_entry(new CSeq_entry());
    set_entry->SetSet(*set);

    unit_test_util::AddFeat(cds_withprot, set_entry);

    auto it = entry->SetSet().SetSeq_set().begin();
    it = entry->SetSet().SetSeq_set().erase(it);

    entry->SetSet().SetSeq_set().insert(it, set_entry);

    //cout << MSerial_AsnText << *entry << endl;

    // add entry to the scope
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    return seh;
}


BOOST_AUTO_TEST_CASE(Test_PropagateFeatsTo2Sequences_UsingFeatureIds)
{
    int feat_id = 0;
    CSeq_entry_Handle seh = GetGoodSeqEntryWithFeatureIds(feat_id);
    CScope& scope = seh.GetScope();

    BOOST_CHECK(feat_id == 5);
    BOOST_TEST_MESSAGE("A set containing " + NStr::IntToString(feat_id) + " five features");
    CFeat_CI gene_it(seh, SAnnotSelector(CSeqFeatData::e_Gene));
    CConstRef<CSeq_feat> gene = gene_it->GetOriginalSeq_feat();
    CFeat_CI mrna_it(seh, SAnnotSelector(CSeqFeatData::eSubtype_mRNA));
    CConstRef<CSeq_feat> mrna = mrna_it->GetOriginalSeq_feat();
    CFeat_CI cds_it(seh, SAnnotSelector(CSeqFeatData::e_Cdregion));
    CConstRef<CSeq_feat> cds_withoutprot;
    CConstRef<CSeq_feat> cds_withprot;
    CConstRef<CSeq_feat> protein;
    for (; cds_it; ++cds_it) {
        if (cds_it->IsSetProduct()) {
            cds_withprot = cds_it->GetOriginalSeq_feat();
            CFeat_CI prot_it(scope.GetBioseqHandle(cds_it->GetProduct()));
            protein = prot_it->GetOriginalSeq_feat();
        }
        else {
            cds_withoutprot = cds_it->GetOriginalSeq_feat();
        }
    }

    BOOST_CHECK(!gene.IsNull());
    BOOST_CHECK(!mrna.IsNull());
    BOOST_CHECK(!cds_withoutprot.IsNull());
    BOOST_CHECK(!cds_withprot.IsNull());
    BOOST_CHECK(!protein.IsNull());

    CAlign_CI align_it(seh);
    CConstRef<CSeq_align> align(&*align_it);
    BOOST_CHECK(!align.IsNull());

    CObject_id::TId maxFeatId = s_FindHighestFeatId(seh);
    BOOST_CHECK(maxFeatId == feat_id);

    CBioseq_CI b_iter(seh, CSeq_inst::eMol_na);
    CBioseq_Handle src_bseq = *b_iter;
    CBioseq_Handle target_bseq1 = *(++b_iter);
    CBioseq_Handle target_bseq2 = *(++b_iter);


    BOOST_TEST_MESSAGE("Propagating to the second sequence");
    CMessageListener_Basic listener;
    edit::CFeaturePropagator propagator1(src_bseq, target_bseq1, *align, true, true, true, true, &listener, &maxFeatId);
    CRef<CSeq_feat> propagated_gene1 = propagator1.Propagate(*gene);

    BOOST_CHECK_EQUAL(listener.Count(), 0);
    BOOST_CHECK(propagated_gene1->IsSetId());
    BOOST_CHECK(propagated_gene1->GetId().GetLocal().GetId() == (++feat_id));

    CRef<CSeq_feat> propagated_mrna1 = propagator1.Propagate(*mrna);
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    BOOST_CHECK(propagated_mrna1->IsSetId());
    BOOST_CHECK(propagated_mrna1->GetId().GetLocal().GetId() == ++feat_id);

    CRef<CSeq_feat> propagated_cds_woprot1 = propagator1.Propagate(*cds_withoutprot);
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    BOOST_CHECK(!propagated_cds_woprot1->IsSetProduct());
    BOOST_CHECK(propagated_cds_woprot1->IsSetId());
    BOOST_CHECK(propagated_cds_woprot1->GetId().GetLocal().GetId() == ++feat_id);

    CRef<CSeq_feat> propagated_cds_wprot1 = propagator1.Propagate(*cds_withprot);
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    BOOST_CHECK(!propagated_cds_wprot1->IsSetProduct()); // this cds also does not have a product at this point
    BOOST_CHECK(propagated_cds_wprot1->IsSetId());
    BOOST_CHECK(propagated_cds_wprot1->GetId().GetLocal().GetId() == ++feat_id);

    CRef<CSeq_feat> propagated_prot1 = propagator1.ConstructProteinFeatureForPropagatedCodingRegion(*cds_withprot, *propagated_cds_wprot1);
    BOOST_CHECK(propagated_prot1->IsSetId());
    BOOST_CHECK(propagated_prot1->GetId().GetLocal().GetId() == ++feat_id);
    listener.Clear();

    BOOST_TEST_MESSAGE("Propagating to the third sequence");
    edit::CFeaturePropagator propagator2(src_bseq, target_bseq2, *align, true, true, true, true, &listener, &maxFeatId);
    CRef<CSeq_feat> propagated_gene2 = propagator2.Propagate(*gene);
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    BOOST_CHECK(propagated_gene2->IsSetId());
    BOOST_CHECK(propagated_gene2->GetId().GetLocal().GetId() == (++feat_id));

    CRef<CSeq_feat> propagated_mrna2 = propagator2.Propagate(*mrna);
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    BOOST_CHECK(propagated_mrna2->IsSetId());
    BOOST_CHECK(propagated_mrna2->GetId().GetLocal().GetId() == ++feat_id);
    listener.Clear();

    CRef<CSeq_feat> propagated_cds_woprot2 = propagator2.Propagate(*cds_withoutprot);
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    BOOST_CHECK(!propagated_cds_woprot2->IsSetProduct());
    BOOST_CHECK(propagated_cds_woprot2->IsSetId());
    BOOST_CHECK(propagated_cds_woprot2->GetId().GetLocal().GetId() == ++feat_id);

    CRef<CSeq_feat> propagated_cds_wprot2 = propagator2.Propagate(*cds_withprot);
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    BOOST_CHECK(!propagated_cds_wprot2->IsSetProduct());
    BOOST_CHECK(propagated_cds_wprot2->IsSetId());
    BOOST_CHECK(propagated_cds_wprot2->GetId().GetLocal().GetId() == ++feat_id);

    CRef<CSeq_feat> propagated_prot2 = propagator2.ConstructProteinFeatureForPropagatedCodingRegion(*cds_withprot, *propagated_cds_wprot2);
    BOOST_CHECK(propagated_prot2->IsSetId());
    BOOST_CHECK(propagated_prot2->GetId().GetLocal().GetId() == ++feat_id);
    listener.Clear();
}


BOOST_AUTO_TEST_CASE(Test_PropagateAllFeatures_UsingFeatureIds)
{
    int feat_id = 0;
    CSeq_entry_Handle seh = GetGoodSeqEntryWithFeatureIds(feat_id);
    CScope& scope = seh.GetScope();

    BOOST_CHECK(feat_id == 5);
    BOOST_TEST_MESSAGE("A set containing " + NStr::IntToString(feat_id) + " five features");
    CFeat_CI gene_it(seh, SAnnotSelector(CSeqFeatData::e_Gene));
    CConstRef<CSeq_feat> gene = gene_it->GetOriginalSeq_feat();
    CFeat_CI mrna_it(seh, SAnnotSelector(CSeqFeatData::eSubtype_mRNA));
    CConstRef<CSeq_feat> mrna = mrna_it->GetOriginalSeq_feat();
    CFeat_CI cds_it(seh, SAnnotSelector(CSeqFeatData::e_Cdregion));
    CConstRef<CSeq_feat> cds_withoutprot;
    CConstRef<CSeq_feat> cds_withprot;
    CConstRef<CSeq_feat> protein;
    for (; cds_it; ++cds_it) {
        if (cds_it->IsSetProduct()) {
            cds_withprot = cds_it->GetOriginalSeq_feat();
            CFeat_CI prot_it(scope.GetBioseqHandle(cds_it->GetProduct()));
            protein = prot_it->GetOriginalSeq_feat();
        }
        else {
            cds_withoutprot = cds_it->GetOriginalSeq_feat();
        }
    }

    BOOST_CHECK(!gene.IsNull());
    BOOST_CHECK(!mrna.IsNull());
    BOOST_CHECK(!cds_withoutprot.IsNull());
    BOOST_CHECK(!cds_withprot.IsNull());
    BOOST_CHECK(!protein.IsNull());

    CAlign_CI align_it(seh);
    CConstRef<CSeq_align> align(&*align_it);
    BOOST_CHECK(!align.IsNull());

    CObject_id::TId maxFeatId = s_FindHighestFeatId(seh);
    BOOST_CHECK(maxFeatId == feat_id);

    CBioseq_CI b_iter(seh, CSeq_inst::eMol_na);
    CBioseq_Handle src_bseq = *b_iter;
    CBioseq_Handle target_bseq = *(++b_iter);


    CMessageListener_Basic listener;
    edit::CFeaturePropagator propagator(src_bseq, target_bseq, *align, true, true, true, true, &listener, &maxFeatId);
    vector<CRef<CSeq_feat>> propagated_feats = propagator.PropagateAll();
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    BOOST_CHECK(propagated_feats.size() == feat_id - 1); // it's 'feat_id-1' because the protein is not propagated
    for (auto& it : propagated_feats) {
        BOOST_CHECK(it->IsSetId());
        BOOST_CHECK(it->GetId().GetLocal().GetId() == (++feat_id));
    }
    listener.Clear();
}

void CreateXRefLink(CSeq_feat& from_feat, CSeq_feat& to_feat)
{
    CRef<CSeqFeatXref> xref(new CSeqFeatXref);
    xref->SetId(to_feat.SetId());
    from_feat.SetXref().push_back(xref);
}

BOOST_AUTO_TEST_CASE(Test_Propagate2FeaturesWithXrefs)
{
    size_t front_insert = 5;
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSetWithAlign(front_insert);
    CRef<CSeq_align> align = entry->SetSet().SetAnnot().front()->SetData().SetAlign().front();

    CRef<CSeq_entry> first = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_entry> last = entry->SetSet().SetSeq_set().back();

    CRef<CSeq_loc> main_loc(new CSeq_loc());
    main_loc->SetInt().SetFrom(0);
    main_loc->SetInt().SetTo(15);
    main_loc->SetInt().SetId().Assign(*(first->GetSeq().GetId().front()));

    CRef<CSeq_loc> subloc(new CSeq_loc());
    subloc->SetInt().SetFrom(3);
    subloc->SetInt().SetTo(5);
    subloc->SetInt().SetId().Assign(*(first->GetSeq().GetId().front()));

    int feat_id = 0;
    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(first, 15);
    gene->SetData().SetGene().SetLocus("gene locus");
    gene->SetId().SetLocal().SetId(++feat_id);
    gene->SetLocation().Assign(*main_loc);

    CRef<CSeq_feat> mrna = unit_test_util::AddMiscFeature(first, 15);
    mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    mrna->SetId().SetLocal().SetId(++feat_id);
    mrna->SetLocation().Assign(*main_loc);

    CreateXRefLink(*mrna, *gene);

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CObject_id::TId maxFeatId = s_FindHighestFeatId(seh);

    CBioseq_CI b_iter(seh, CSeq_inst::eMol_na);
    CBioseq_Handle src_bseq = *b_iter;
    CBioseq_Handle target_bseq = *(++b_iter);

    BOOST_TEST_MESSAGE("When both mrna and gene are propagated");
    CMessageListener_Basic listener;
    edit::CFeaturePropagator propagator(src_bseq, target_bseq, *align, true, true, true, true, &listener, &maxFeatId);
    vector<CRef<CSeq_feat>> propagated_feats = propagator.PropagateFeatureList({ gene, mrna });
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    auto prop_gene = propagated_feats.front();
    BOOST_CHECK(prop_gene->IsSetId());
    BOOST_CHECK(prop_gene->GetId().GetLocal().GetId() == (++feat_id));
    BOOST_CHECK(!prop_gene->IsSetXref());

    auto prop_mrna = propagated_feats.back();
    BOOST_CHECK(prop_mrna->IsSetId());
    BOOST_CHECK(prop_mrna->GetId().GetLocal().GetId() == (++feat_id));
    BOOST_TEST_MESSAGE("the Xref is also propagated");
    BOOST_CHECK(prop_mrna->IsSetXref());
    CSeqFeatXref xref;
    xref.SetId(prop_gene->SetId());
    BOOST_CHECK(prop_mrna->HasSeqFeatXref(xref.GetId()));
    listener.Clear();
}

BOOST_AUTO_TEST_CASE(Test_Propagate1FeatureWithXrefs)
{
    size_t front_insert = 5;
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSetWithAlign(front_insert);
    CRef<CSeq_align> align = entry->SetSet().SetAnnot().front()->SetData().SetAlign().front();

    CRef<CSeq_entry> first = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_entry> last = entry->SetSet().SetSeq_set().back();

    CRef<CSeq_loc> main_loc(new CSeq_loc());
    main_loc->SetInt().SetFrom(0);
    main_loc->SetInt().SetTo(15);
    main_loc->SetInt().SetId().Assign(*(first->GetSeq().GetId().front()));

    CRef<CSeq_loc> subloc(new CSeq_loc());
    subloc->SetInt().SetFrom(3);
    subloc->SetInt().SetTo(5);
    subloc->SetInt().SetId().Assign(*(first->GetSeq().GetId().front()));

    int feat_id = 0;
    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(first, 15);
    gene->SetData().SetGene().SetLocus("gene locus");
    gene->SetId().SetLocal().SetId(++feat_id);
    gene->SetLocation().Assign(*main_loc);

    CRef<CSeq_feat> mrna = unit_test_util::AddMiscFeature(first, 15);
    mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    mrna->SetId().SetLocal().SetId(++feat_id);
    mrna->SetLocation().Assign(*main_loc);

    CreateXRefLink(*mrna, *gene);

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CObject_id::TId maxFeatId = s_FindHighestFeatId(seh);

    CBioseq_CI b_iter(seh, CSeq_inst::eMol_na);
    CBioseq_Handle src_bseq = *b_iter;
    CBioseq_Handle target_bseq = *(++b_iter);

    BOOST_TEST_MESSAGE("When the mrna is propagated alone");
    CMessageListener_Basic listener;
    edit::CFeaturePropagator propagator(src_bseq, target_bseq, *align, true, true, true, true, &listener, &maxFeatId);
    vector<CRef<CSeq_feat>> propagated_feats = propagator.PropagateFeatureList({ mrna });
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    auto prop_mrna = propagated_feats.front();
    BOOST_CHECK(prop_mrna->IsSetId());
    BOOST_CHECK(prop_mrna->GetId().GetLocal().GetId() == (++feat_id));
    BOOST_TEST_MESSAGE("the Xref is missing");
    BOOST_CHECK(!prop_mrna->IsSetXref());
    listener.Clear();
}

BOOST_AUTO_TEST_CASE(Test_Propagate2FeaturesWithXrefs_RevOrder)
{
    size_t front_insert = 5;
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSetWithAlign(front_insert);
    CRef<CSeq_align> align = entry->SetSet().SetAnnot().front()->SetData().SetAlign().front();

    CRef<CSeq_entry> first = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_entry> last = entry->SetSet().SetSeq_set().back();

    CRef<CSeq_loc> main_loc(new CSeq_loc());
    main_loc->SetInt().SetFrom(0);
    main_loc->SetInt().SetTo(15);
    main_loc->SetInt().SetId().Assign(*(first->GetSeq().GetId().front()));

    CRef<CSeq_loc> subloc(new CSeq_loc());
    subloc->SetInt().SetFrom(3);
    subloc->SetInt().SetTo(5);
    subloc->SetInt().SetId().Assign(*(first->GetSeq().GetId().front()));

    int feat_id = 0;
    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(first, 15);
    gene->SetData().SetGene().SetLocus("gene locus");
    gene->SetId().SetLocal().SetId(++feat_id);
    gene->SetLocation().Assign(*main_loc);

    CRef<CSeq_feat> mrna = unit_test_util::AddMiscFeature(first, 15);
    mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    mrna->SetId().SetLocal().SetId(++feat_id);
    mrna->SetLocation().Assign(*main_loc);

    CreateXRefLink(*gene, *mrna);

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CObject_id::TId maxFeatId = s_FindHighestFeatId(seh);

    CBioseq_CI b_iter(seh, CSeq_inst::eMol_na);
    CBioseq_Handle src_bseq = *b_iter;
    CBioseq_Handle target_bseq = *(++b_iter);

    CMessageListener_Basic listener;
    edit::CFeaturePropagator propagator(src_bseq, target_bseq, *align, true, true, true, true, &listener, &maxFeatId);
    vector<CRef<CSeq_feat>> propagated_feats = propagator.PropagateFeatureList({ gene, mrna });
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    auto prop_gene = propagated_feats.front();
    BOOST_CHECK_EQUAL(listener.Count(), 0);
    BOOST_CHECK(prop_gene->IsSetId());
    BOOST_CHECK(prop_gene->GetId().GetLocal().GetId() == (++feat_id));
    BOOST_CHECK(prop_gene->IsSetXref());

    auto prop_mrna = propagated_feats.back();
    CSeqFeatXref xref;
    xref.SetId(prop_mrna->SetId());
    BOOST_CHECK(prop_gene->HasSeqFeatXref(xref.GetId()));

    BOOST_CHECK(prop_mrna->IsSetId());
    BOOST_CHECK(prop_mrna->GetId().GetLocal().GetId() == (++feat_id));
    BOOST_CHECK(!prop_mrna->IsSetXref());
    listener.Clear();
}

BOOST_AUTO_TEST_CASE(Test_PropagateFeaturesWithXrefsWithCDS)
{
    size_t front_insert = 5;
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSetWithAlign(front_insert);
    CRef<CSeq_align> align = entry->SetSet().SetAnnot().front()->SetData().SetAlign().front();

    CRef<CSeq_entry> first = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_entry> last = entry->SetSet().SetSeq_set().back();

    CRef<CSeq_loc> main_loc(new CSeq_loc());
    main_loc->SetInt().SetFrom(0);
    main_loc->SetInt().SetTo(15);
    main_loc->SetInt().SetId().Assign(*(first->GetSeq().GetId().front()));

    CRef<CSeq_loc> subloc(new CSeq_loc());
    subloc->SetInt().SetFrom(3);
    subloc->SetInt().SetTo(5);
    subloc->SetInt().SetId().Assign(*(first->GetSeq().GetId().front()));

    int feat_id = 0;
    CRef<CSeq_feat> gene = unit_test_util::AddMiscFeature(first, 15);
    gene->SetData().SetGene().SetLocus("gene locus");
    gene->SetId().SetLocal().SetId(++feat_id);
    gene->SetLocation().Assign(*main_loc);

    CRef<CSeq_feat> mrna = unit_test_util::AddMiscFeature(first, 15);
    mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
    mrna->SetId().SetLocal().SetId(++feat_id);
    mrna->SetLocation().Assign(*main_loc);

    CreateXRefLink(*mrna, *gene);

    CRef<CSeq_feat> cds_withoutprot = unit_test_util::AddMiscFeature(first, 15);
    cds_withoutprot->SetData().SetCdregion();
    cds_withoutprot->SetId().SetLocal().SetId(++feat_id);
    cds_withoutprot->SetLocation().SetInt().SetFrom(10);
    cds_withoutprot->SetLocation().SetInt().SetTo(25);
    cds_withoutprot->SetLocation().SetInt().SetId().Assign(*(first->GetSeq().GetId().front()));

    CRef<CSeq_feat> cds_withprot = unit_test_util::MakeMiscFeature(unit_test_util::IdFromEntry(first), 15);
    cds_withprot->SetComment("CDS with product");
    cds_withprot->SetData().SetCdregion();
    cds_withprot->SetId().SetLocal().SetId(++feat_id);
    cds_withprot->SetLocation().Assign(*main_loc);

    CreateXRefLink(*cds_withprot, *gene);
    CreateXRefLink(*mrna, *cds_withprot);
    CreateXRefLink(*cds_withprot, *mrna);

    // constructing the protein sequence
    CRef<CSeq_entry> prot_entry(new CSeq_entry());
    prot_entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
    prot_entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_aa);
    prot_entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("-WPKL");
    prot_entry->SetSeq().SetInst().SetLength(5);

    const string prot_id = "good1_1";
    CRef<CSeq_id> id(new CSeq_id());
    id->SetLocal().SetStr(prot_id);
    prot_entry->SetSeq().SetId().push_back(id);

    CRef<CSeqdesc> mdesc(new CSeqdesc());
    mdesc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
    prot_entry->SetSeq().SetDescr().Set().push_back(mdesc);

    CRef<CSeq_feat> prot_feat(new CSeq_feat());
    prot_feat->SetData().SetProt().SetName().push_back("hypothetical protein");
    prot_feat->SetLocation().SetInt().SetId().Assign(*(prot_entry->GetSeq().GetId().front()));
    prot_feat->SetLocation().SetInt().SetFrom(0);
    prot_feat->SetLocation().SetInt().SetTo(prot_entry->GetSeq().GetInst().GetLength() - 1);
    prot_feat->SetId().SetLocal().SetId(++feat_id);
    unit_test_util::AddFeat(prot_feat, prot_entry);

    cds_withprot->SetProduct().SetWhole().SetLocal().SetStr(prot_id);

    CRef<CBioseq_set> set(new CBioseq_set());
    set->SetClass(CBioseq_set::eClass_nuc_prot);
    set->SetSeq_set().push_back(first);
    set->SetSeq_set().push_back(prot_entry);
    CRef<CSeq_entry> set_entry(new CSeq_entry());
    set_entry->SetSet(*set);

    unit_test_util::AddFeat(cds_withprot, set_entry);

    auto it = entry->SetSet().SetSeq_set().begin();
    it = entry->SetSet().SetSeq_set().erase(it);
    entry->SetSet().SetSeq_set().insert(it, set_entry);

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CObject_id::TId maxFeatId = s_FindHighestFeatId(seh);

    CBioseq_CI b_iter(seh, CSeq_inst::eMol_na);
    CBioseq_Handle src_bseq = *b_iter;
    CBioseq_Handle target_bseq = *(++b_iter);

    CMessageListener_Basic listener;
    edit::CFeaturePropagator propagator(src_bseq, target_bseq, *align, true, true, true, true, &listener, &maxFeatId);
    vector<CConstRef<CSeq_feat>> feat_list{ gene, mrna, cds_withoutprot, cds_withprot };
    vector<CRef<CSeq_feat>> propagated_feats = propagator.PropagateFeatureList(feat_list);
    BOOST_CHECK_EQUAL(listener.Count(), 0);

    BOOST_CHECK(propagated_feats.size() == feat_id );

    auto feat_it = propagated_feats.begin();
    auto prop_gene = *feat_it;
    BOOST_CHECK(prop_gene->IsSetId());
    BOOST_CHECK(prop_gene->GetId().GetLocal().GetId() == (++feat_id));
    BOOST_CHECK(!prop_gene->IsSetXref());

    ++feat_it;
    auto prop_mrna = *feat_it;
    BOOST_CHECK(prop_mrna->IsSetId());
    BOOST_CHECK(prop_mrna->GetId().GetLocal().GetId() == (++feat_id));
    BOOST_CHECK(prop_mrna->IsSetXref());
    BOOST_CHECK(prop_mrna->GetXref().size() == 2);

    ++feat_it;
    auto prop_cds_withoutprot = *feat_it;
    BOOST_CHECK(prop_cds_withoutprot->IsSetId());
    BOOST_CHECK(prop_cds_withoutprot->GetId().GetLocal().GetId() == (++feat_id));
    BOOST_CHECK(!prop_cds_withoutprot->IsSetXref());

    ++feat_it;
    auto prop_cds = *feat_it;
    BOOST_CHECK(prop_cds->IsSetId());
    BOOST_CHECK(prop_cds->GetId().GetLocal().GetId() == (++feat_id));
    BOOST_CHECK(prop_cds->IsSetXref());

    CSeqFeatXref mrna_xref1;
    mrna_xref1.SetId(prop_gene->SetId());
    BOOST_CHECK(prop_mrna->HasSeqFeatXref(mrna_xref1.GetId()));
    mrna_xref1.SetId(prop_cds->SetId());
    BOOST_CHECK(prop_mrna->HasSeqFeatXref(mrna_xref1.GetId()));

    CSeqFeatXref cds_xref;
    cds_xref.SetId(prop_gene->SetId());
    BOOST_CHECK(prop_cds->HasSeqFeatXref(cds_xref.GetId()));
    cds_xref.SetId(prop_mrna->SetId());
    BOOST_CHECK(prop_cds->HasSeqFeatXref(cds_xref.GetId()));

    ++feat_it;
    auto prop_protein = *feat_it;
    BOOST_CHECK(prop_protein->IsSetId());
    BOOST_CHECK(prop_protein->GetId().GetLocal().GetId() == (++feat_id));
    listener.Clear();
}


CRef<CSeq_entry> BuildAlignmentWithInternalGap()
{
    CRef<CSeq_entry> entry = unit_test_util::BuildGoodEcoSet();

    CRef<objects::CSeq_entry> seq4 = unit_test_util::BuildGoodSeq();
    unit_test_util::ChangeId(seq4, "4");
    entry->SetSet().SetSeq_set().push_back(seq4);

    CRef<objects::CSeq_align> align(new CSeq_align());
    align->SetType(objects::CSeq_align::eType_global);
    align->SetDim(entry->GetSet().GetSeq_set().size());

    // assign IDs
    for (auto& s : entry->SetSet().SetSeq_set()) {
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*(s->GetSeq().GetId().front()));
        align->SetSegs().SetDenseg().SetIds().push_back(id);
    }

    auto s = entry->SetSet().SetSeq_set().begin();
    auto first_seq = (*s)->GetSeq().GetInst().GetSeq_data().GetIupacna().Get(); // original
    s++;
    // second sequence: remove beginning
    (*s)->SetSeq().SetInst().SetSeq_data().SetIupacna().Set(first_seq.substr(20, 40));
    (*s)->SetSeq().SetInst().SetLength(40);
    s++;
    // third sequence: remove part of the middle
    (*s)->SetSeq().SetInst().SetSeq_data().SetIupacna().Set(first_seq.substr(0, 20) + first_seq.substr(40, 20));
    (*s)->SetSeq().SetInst().SetLength(40);
    s++;
    // fourth sequence: remove end
    (*s)->SetSeq().SetInst().SetSeq_data().SetIupacna().Set(first_seq.substr(0, 40));
    (*s)->SetSeq().SetInst().SetLength(40);

    // now make first sequence longer than alignment
    CRef<CSeq_entry> first = entry->SetSet().SetSeq_set().front();
    first->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AAAAATTTTTGGGGGCCCCC" + first_seq + "AAAAATTTTTGGGGGCCCCC");
    first->SetSeq().SetInst().SetLength(100);


    auto& denseg = align->SetSegs().SetDenseg();
    denseg.SetNumseg(3);
    denseg.SetLens().push_back(20);
    denseg.SetLens().push_back(20);
    denseg.SetLens().push_back(20);
    denseg.SetDim(entry->GetSet().GetSeq_set().size());
    // first segment - second sequence missing
    denseg.SetStarts().push_back(20);
    denseg.SetStarts().push_back(-1);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(0);
    // second segment - third sequence is gap
    denseg.SetStarts().push_back(40);
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(-1);
    denseg.SetStarts().push_back(20);
    // third segment - fourth sequence is gap
    denseg.SetStarts().push_back(60);
    denseg.SetStarts().push_back(20);
    denseg.SetStarts().push_back(20);
    denseg.SetStarts().push_back(-1);

    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetAlign().push_back(align);
    entry->SetSet().SetAnnot().push_back(annot);
    return entry;
}


BOOST_AUTO_TEST_CASE(Test_DoNotPropagateToGap_RW_887)
{
    CRef<CSeq_entry> entry = BuildAlignmentWithInternalGap();
    CRef<CSeq_align> align = entry->SetSet().SetAnnot().front()->SetData().SetAlign().front();
    CRef<CSeq_entry> first = entry->SetSet().SetSeq_set().front();

    // before alignment
    CRef<CSeq_feat> gene1 = unit_test_util::AddMiscFeature(first);
    gene1->SetData().SetGene().SetLocus("gene locus");
    gene1->SetLocation().SetInt().SetFrom(0);
    gene1->SetLocation().SetInt().SetTo(19);

    // first gap
    CRef<CSeq_feat> gene2 = unit_test_util::AddMiscFeature(first);
    gene2->SetData().SetGene().SetLocus("gene locus");
    gene2->SetLocation().SetInt().SetFrom(20);
    gene2->SetLocation().SetInt().SetTo(39);

    // second gap
    CRef<CSeq_feat> gene3 = unit_test_util::AddMiscFeature(first);
    gene3->SetData().SetGene().SetLocus("gene locus");
    gene3->SetLocation().SetInt().SetFrom(40);
    gene3->SetLocation().SetInt().SetTo(59);

    // third gap
    CRef<CSeq_feat> gene4 = unit_test_util::AddMiscFeature(first);
    gene4->SetData().SetGene().SetLocus("gene locus");
    gene4->SetLocation().SetInt().SetFrom(60);
    gene4->SetLocation().SetInt().SetTo(79);

    // after alignment
    CRef<CSeq_feat> gene5 = unit_test_util::AddMiscFeature(first);
    gene5->SetData().SetGene().SetLocus("gene locus");
    gene5->SetLocation().SetInt().SetFrom(80);
    gene5->SetLocation().SetInt().SetTo(99);

    vector<CConstRef<CSeq_feat>> feat_list{ gene1, gene2, gene3, gene4, gene5 };

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);

    CObject_id::TId maxFeatId = s_FindHighestFeatId(seh);


    CBioseq_CI b_iter(seh, CSeq_inst::eMol_na);
    CBioseq_Handle src_bseq = *b_iter;

    ++b_iter;

    CMessageListener_Basic listener;
    edit::CFeaturePropagator propagator_to_2(src_bseq, *b_iter, *align, true, true, true, true, &listener);
    vector<CRef<CSeq_feat>> propagated_feats = propagator_to_2.PropagateFeatureList(feat_list);
    BOOST_CHECK_EQUAL(listener.Count(), 3);
    BOOST_CHECK_EQUAL(listener.GetMessage(0).GetText(), "Unable to propagate location of feature lcl|good1:1-20 to lcl|good2");
    BOOST_CHECK_EQUAL(listener.GetMessage(1).GetText(), "Unable to propagate location of feature lcl|good1:21-40 to lcl|good2");
    BOOST_CHECK_EQUAL(listener.GetMessage(2).GetText(), "Unable to propagate location of feature lcl|good1:81-100 to lcl|good2");
    listener.Clear();

    ++b_iter;
    edit::CFeaturePropagator propagator_to_3(src_bseq, *b_iter, *align, true, true, true, true, &listener);
    propagated_feats = propagator_to_3.PropagateFeatureList(feat_list);
    BOOST_CHECK_EQUAL(listener.Count(), 3);
    BOOST_CHECK_EQUAL(listener.GetMessage(0).GetText(), "Unable to propagate location of feature lcl|good1:1-20 to lcl|good3");
    BOOST_CHECK_EQUAL(listener.GetMessage(1).GetText(), "Unable to propagate location of feature lcl|good1:41-60 to lcl|good3");
    BOOST_CHECK_EQUAL(listener.GetMessage(2).GetText(), "Unable to propagate location of feature lcl|good1:81-100 to lcl|good3");
    listener.Clear();

    ++b_iter;
    edit::CFeaturePropagator propagator_to_4(src_bseq, *b_iter, *align, true, true, true, true, &listener);
    propagated_feats = propagator_to_4.PropagateFeatureList(feat_list);
    BOOST_CHECK_EQUAL(listener.Count(), 3);
    BOOST_CHECK_EQUAL(listener.GetMessage(0).GetText(), "Unable to propagate location of feature lcl|good1:1-20 to lcl|good4");
    BOOST_CHECK_EQUAL(listener.GetMessage(1).GetText(), "Unable to propagate location of feature lcl|good1:61-80 to lcl|good4");
    BOOST_CHECK_EQUAL(listener.GetMessage(2).GetText(), "Unable to propagate location of feature lcl|good1:81-100 to lcl|good4");
    listener.Clear();

}


#if 0
// checked in by mistake
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
        edit::CFeaturePropagator propagator(src, *b, *align, false, false, true, true, &listener);

        CRef<CSeq_feat> new_feat = propagator.Propagate(*misc);
        CheckPropagatedLocation(*(expected_loc[offset]), new_feat->GetLocation());
        BOOST_CHECK_EQUAL(listener.Count(), 0);
        listener.Clear();
        offset++;
        ++b;
    }

}
#endif


END_SCOPE(objects)
END_NCBI_SCOPE

