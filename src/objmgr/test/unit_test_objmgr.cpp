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
* Authors:  Eugene Vasilchenko
*
* File Description:
*   Unit test for various OM operations
*/

#define NCBI_TEST_APPLICATION

#include <ncbi_pch.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/seq_align_handle.hpp>
#include <objmgr/seq_graph_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seq_table_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/impl/synonyms.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqtable/seqtable__.hpp>
#include <objmgr/util/sequence.hpp>
#include <serial/iterator.hpp>

#ifdef NCBI_THREADS
# include <thread>
# include <future>
#endif // NCBI_THREADS

#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

static CRef<CSeq_id> s_GetId(size_t i)
{
    CRef<CSeq_id> id(new CSeq_id());
    id->SetGi(TIntId(i+1));
    return id;
}


static CRef<CSeq_id> s_GetId2(size_t i)
{
    CRef<CSeq_id> id(new CSeq_id());
    id->SetLocal().SetId(TIntId(i+1));
    return id;
}


static CRef<CSeq_entry> s_GetEntry(size_t i, TSeqPos length = 2)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq& seq = entry->SetSeq();
    seq.SetId().push_back(s_GetId(i));
    seq.SetId().push_back(s_GetId2(i));
    CSeq_inst& inst = seq.SetInst();
    inst.SetRepr(inst.eRepr_raw);
    inst.SetMol(inst.eMol_aa);
    inst.SetLength(length);
    inst.SetSeq_data().SetIupacaa().Set(string(length, 'A'));
    return entry;
}


static CRef<CSeq_entry> s_GetDeltaSeqEntry(size_t i, size_t seg_i)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq& seq = entry->SetSeq();
    seq.SetId().push_back(s_GetId(i));
    seq.SetId().push_back(s_GetId2(i));
    CSeq_inst& inst = seq.SetInst();
    inst.SetRepr(inst.eRepr_delta);
    inst.SetMol(inst.eMol_aa);
    inst.SetExt().SetRef().Set().SetWhole(*s_GetId(seg_i));
    return entry;
}


static CRef<CSeq_annot> s_GetAnnot(CSeq_id& id, int count = 1)
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    for ( int i = 0; i < count; ++i ) {
        CRef<CSeq_feat> feat(new CSeq_feat);
        feat->SetLocation().SetWhole(id);
        feat->SetData().SetRegion("test");
        annot->SetData().SetFtable().push_back(feat);
    }
    return annot;
}


static CRef<CSeq_annot> s_GetAnnotAlign(CSeq_id& id1, CSeq_id& id2, int count = 1)
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    for ( int i = 0; i < count; ++i ) {
        CRef<CSeq_align> align(new CSeq_align);
        align->SetType(CSeq_align::eType_not_set);
        auto& segs = align->SetSegs().SetDenseg();
        segs.SetNumseg(1);
        segs.SetIds().push_back(Ref(&id1));
        segs.SetIds().push_back(Ref(&id2));
        segs.SetStarts().push_back(0);
        segs.SetStarts().push_back(i*100);
        segs.SetLens().push_back(2);
        annot->SetData().SetAlign().push_back(align);
    }
    return annot;
}


static CRef<CSeq_annot> s_GetAnnotGraph(CSeq_id& id, int count = 1)
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    for ( int i = 0; i < count; ++i ) {
        CRef<CSeq_graph> graph(new CSeq_graph);
        graph->SetLoc().SetInt().SetId(id);
        graph->SetLoc().SetInt().SetFrom(0);
        graph->SetLoc().SetInt().SetTo(1);
        graph->SetLoc().SetInt().SetStrand(eNa_strand_plus);
        graph->SetGraph().SetByte().SetMin(0);
        graph->SetGraph().SetByte().SetMax(1);
        graph->SetGraph().SetByte().SetAxis(0);
        graph->SetGraph().SetByte().SetValues().resize(2);
        annot->SetData().SetGraph().push_back(graph);
    }
    return annot;
}

template<class T, class... Args>
inline CRef<T> make_ref(Args&&... args)
{
    return CRef<T>(new T(std::forward<Args>(args)...));
}

template<class T, class... Args>
inline CConstRef<T> make_const_ref(Args&&... args)
{
    return CConstRef<T>(new T(std::forward<Args>(args)...));
}

static CRef<CSeq_annot> s_GetAnnotTable(CSeq_id& id)
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    CRef<CSeq_table> table(new CSeq_table);
    table->SetFeat_type(0);
    table->SetNum_rows(1);
    CRef<CSeqTable_column> loc_col(new CSeqTable_column);
    loc_col->SetHeader().SetField_name("Seq-table location");
    loc_col->SetDefault().SetLoc(*make_ref<CSeq_loc>(id, 0, 1, eNa_strand_plus));
    table->SetColumns().push_back(loc_col);
    annot->SetData().SetSeq_table(*table);
    return annot;
}


BOOST_AUTO_TEST_CASE(TestReResolve1)
{
    // check re-resolve after adding
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id = s_GetId(1);
    CRef<CSeq_id> id2 = s_GetId2(1);
    CRef<CSeq_entry> entry = s_GetEntry(1);
    BOOST_REQUIRE(!scope.GetBioseqHandle(*id));
    BOOST_REQUIRE(scope.GetIds(*id).empty());
    BOOST_CHECK(!scope.Exists(*id));
    scope.AddTopLevelSeqEntry(*entry);
    BOOST_REQUIRE(scope.GetBioseqHandle(*id));
    BOOST_REQUIRE(scope.GetSynonyms(*id));
    BOOST_REQUIRE(scope.GetSynonyms(*s_GetId2(1))->ContainsSynonym(CSeq_id_Handle::GetHandle(*id2)));
    BOOST_REQUIRE(scope.GetSynonyms(*s_GetId2(1))->ContainsSynonym(CSeq_id_Handle::GetHandle(*id)));
    BOOST_REQUIRE(!scope.GetIds(*id).empty());
    BOOST_CHECK(scope.Exists(*id));
}


BOOST_AUTO_TEST_CASE(TestReResolve2)
{
    // check re-resolve after adding, removing, and restoring
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id = s_GetId(1);
    CRef<CSeq_id> id2 = s_GetId2(1);
    CRef<CSeq_entry> entry = s_GetEntry(1);
    BOOST_REQUIRE(!scope.GetBioseqHandle(*id));
    BOOST_REQUIRE(scope.GetIds(*id).empty());
    BOOST_CHECK(!scope.Exists(*id));
    CSeq_entry_EditHandle seh = scope.AddTopLevelSeqEntry(*entry).GetEditHandle();
    BOOST_REQUIRE(!scope.GetIds(*id).empty());
    BOOST_CHECK(scope.Exists(*id));
    BOOST_REQUIRE(scope.GetSynonyms(*id2));
    BOOST_REQUIRE(scope.GetSynonyms(*s_GetId2(1))->ContainsSynonym(CSeq_id_Handle::GetHandle(*id2)));
    BOOST_REQUIRE(scope.GetSynonyms(*s_GetId2(1))->ContainsSynonym(CSeq_id_Handle::GetHandle(*id)));
    BOOST_REQUIRE(scope.GetBioseqHandle(*id));
    CBioseq_EditHandle bh = seh.SetSeq();
    seh.SelectNone();
    BOOST_REQUIRE(!scope.GetBioseqHandle(*id));
    seh.SelectSeq(bh);
    BOOST_REQUIRE(!scope.GetIds(*id).empty());
    BOOST_CHECK(scope.Exists(*id));
    BOOST_REQUIRE(scope.GetBioseqHandle(*id));
    BOOST_REQUIRE(scope.GetSynonyms(*id2));
    BOOST_REQUIRE(scope.GetSynonyms(*s_GetId2(1))->ContainsSynonym(CSeq_id_Handle::GetHandle(*id2)));
    BOOST_REQUIRE(scope.GetSynonyms(*s_GetId2(1))->ContainsSynonym(CSeq_id_Handle::GetHandle(*id)));
}


BOOST_AUTO_TEST_CASE(TestReResolve3)
{
    // check re-resolve after adding, removing, and restoring, resolving only at the end
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id = s_GetId(1);
    CRef<CSeq_id> id2 = s_GetId2(1);
    CRef<CSeq_entry> entry = s_GetEntry(1);
    BOOST_REQUIRE(!scope.GetBioseqHandle(*id));
    BOOST_REQUIRE(scope.GetIds(*id).empty());
    BOOST_CHECK(!scope.Exists(*id));
    CSeq_entry_EditHandle seh = scope.AddTopLevelSeqEntry(*entry).GetEditHandle();
    CBioseq_EditHandle bh = seh.SetSeq();
    seh.SelectNone();
    BOOST_REQUIRE(!scope.GetBioseqHandle(*id));
    BOOST_REQUIRE(scope.GetIds(*id).empty());
    BOOST_CHECK(!scope.Exists(*id));
    seh.SelectSeq(bh);
    BOOST_REQUIRE(!scope.GetIds(*id).empty());
    BOOST_CHECK(scope.Exists(*id));
    BOOST_REQUIRE(scope.GetBioseqHandle(*id));
    BOOST_REQUIRE(scope.GetSynonyms(*id2));
    BOOST_REQUIRE(scope.GetSynonyms(*s_GetId2(1))->ContainsSynonym(CSeq_id_Handle::GetHandle(*id2)));
    BOOST_REQUIRE(scope.GetSynonyms(*s_GetId2(1))->ContainsSynonym(CSeq_id_Handle::GetHandle(*id)));
}


BOOST_AUTO_TEST_CASE(TestReResolve4)
{
    // check re-resolve after adding, removing, and restoring, resolving only at the end
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id1 = s_GetId(1);
    CRef<CSeq_id> id2 = s_GetId2(1);
    CRef<CSeq_entry> entry = s_GetEntry(1);
    entry->SetSeq().SetAnnot().push_back(s_GetAnnot(*id2));
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry((const CSeq_entry&)*entry);
    CRef<CSeq_loc> loc1(new CSeq_loc); loc1->SetWhole(*id1);
    CRef<CSeq_loc> loc2(new CSeq_loc); loc2->SetWhole(*id2);
    scope.AddSeq_annot(*s_GetAnnot(*id1, 3));
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc1).GetSize(), 4u);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc1).GetSize(), 4u);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc2).GetSize(), 4u);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc2).GetSize(), 4u);
    scope.AddSeq_annot(*s_GetAnnot(*id2, 5));
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc1).GetSize(), 9u);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc1).GetSize(), 9u);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc2).GetSize(), 9u);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc2).GetSize(), 9u);
    scope.RemoveTopLevelSeqEntry(seh);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc1, SAnnotSelector().SetSearchUnresolved()).GetSize(), 3u);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc1, SAnnotSelector().SetSearchUnresolved()).GetSize(), 3u);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc2, SAnnotSelector().SetSearchUnresolved()).GetSize(), 5u);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc2, SAnnotSelector().SetSearchUnresolved()).GetSize(), 5u);
}


static CRef<CSeq_feat> s_CreateFeatForEdit(const CSeq_id* id)
{
    CRef<CSeq_feat> feat(new CSeq_feat);
    feat->SetData().SetGene().SetLocus("test locus");
    feat->SetLocation(*make_ref<CSeq_loc>(*SerialClone(*id), 419, 1171, eNa_strand_plus));
    return feat;
}


static CRef<CSeq_align> s_CreateAlignForEdit(const CSeq_id* id1, const CSeq_id* id2)
{
    CRef<CSeq_align> align(new CSeq_align);
    align->SetType(CSeq_align::eType_not_set);
    auto& segs = align->SetSegs().SetDenseg();
    segs.SetNumseg(1);
    segs.SetIds().push_back(Ref(SerialClone(*id1)));
    segs.SetIds().push_back(Ref(SerialClone(*id2)));
    segs.SetStarts().push_back(1);
    segs.SetStarts().push_back(100);
    segs.SetLens().push_back(100);
    return align;
}


static CRef<CSeq_graph> s_CreateGraphForEdit(const CSeq_id* id)
{
    CRef<CSeq_graph> graph(new CSeq_graph);
    graph->SetLoc(*make_ref<CSeq_loc>(*SerialClone(*id), 419, 1171, eNa_strand_plus));
    graph->SetGraph().SetByte().SetMin(0);
    graph->SetGraph().SetByte().SetMax(1);
    graph->SetGraph().SetByte().SetAxis(0);
    graph->SetGraph().SetByte().SetValues().resize(1171-419+1);
    return graph;
}


static CRef<CBioseq> s_CreateBioseqForEdit(const CSeq_id* id1 = 0, const CSeq_id* id2 = 0)
{
    CRef<CBioseq> seq(new CBioseq);
    if ( id1 ) {
        seq->SetId().push_back(Ref(SerialClone(*id1)));
    }
    if ( id2 ) {
        seq->SetId().push_back(Ref(SerialClone(*id2)));
    }
    seq->SetInst().SetRepr(CSeq_inst::eRepr_virtual);
    seq->SetInst().SetMol(CSeq_inst::eMol_dna);
    seq->SetInst().SetLength(1172);
    seq->SetAnnot().push_back(Ref(new CSeq_annot));
    seq->SetAnnot().back()->SetData().SetFtable();
    return seq;
}


BOOST_AUTO_TEST_CASE(TestEditAnnotBioseqAdd)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id1(new CSeq_id("lcl|Seq1"));
    CRef<CSeq_id> id2(new CSeq_id("KF591393"));
    CRef<CBioseq> seq = s_CreateBioseqForEdit(id1, id2);
    CRef<CSeq_feat> feat = s_CreateFeatForEdit(id1);
    CBioseq_Handle bh = scope.AddBioseq(*seq);
    BOOST_CHECK_EQUAL(CFeat_CI(bh).GetSize(), 0u);
    BOOST_REQUIRE(CSeq_annot_CI(bh));
    CSeq_annot_CI(bh)->GetEditHandle().AddFeat(*feat);
    BOOST_CHECK_EQUAL(CFeat_CI(bh).GetSize(), 1u);
}


BOOST_AUTO_TEST_CASE(TestEditAnnotBioseqReplace)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id1(new CSeq_id("lcl|Seq1"));
    CRef<CSeq_id> id2(new CSeq_id("KF591393"));
    CRef<CBioseq> seq = s_CreateBioseqForEdit(id1, id2);
    CRef<CSeq_feat> feat = s_CreateFeatForEdit(id1);
    CRef<CSeq_feat> feat2 = s_CreateFeatForEdit(id2);
    seq->SetAnnot().back()->SetData().SetFtable().push_back(feat);
    CBioseq_Handle bh = scope.AddBioseq(*seq);
    BOOST_CHECK_EQUAL(CFeat_CI(bh).GetSize(), 1u);
    BOOST_REQUIRE(CSeq_annot_CI(bh));
    BOOST_REQUIRE_EQUAL(CFeat_CI(*CSeq_annot_CI(bh)).GetSize(), 1u);
    CSeq_feat_EditHandle(*CFeat_CI(*CSeq_annot_CI(bh))).Replace(*feat2);
    BOOST_CHECK_EQUAL(CFeat_CI(bh).GetSize(), 1u);
}


BOOST_AUTO_TEST_CASE(TestEditAnnotExternalAdd)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id1(new CSeq_id("lcl|Seq1"));
    CRef<CSeq_id> id2(new CSeq_id("KF591393"));
    CRef<CBioseq> seq = s_CreateBioseqForEdit(id1, id2);
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetFtable();
    CRef<CSeq_feat> feat = s_CreateFeatForEdit(id1);
    CBioseq_Handle bh = scope.AddBioseq(*seq);
    CSeq_annot_Handle ah = scope.AddSeq_annot(*annot);
    BOOST_CHECK_EQUAL(CFeat_CI(bh).GetSize(), 0u);
    ah.GetEditHandle().AddFeat(*feat);
    BOOST_CHECK_EQUAL(CFeat_CI(bh).GetSize(), 1u);
}


BOOST_AUTO_TEST_CASE(TestEditAnnotExternalReplace)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id1(new CSeq_id("lcl|Seq1"));
    CRef<CSeq_id> id2(new CSeq_id("KF591393"));
    CRef<CBioseq> seq = s_CreateBioseqForEdit(id1, id2);
    CRef<CSeq_feat> feat = s_CreateFeatForEdit(id1);
    CRef<CSeq_feat> feat2 = s_CreateFeatForEdit(id2);
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetFtable().push_back(feat);
    CBioseq_Handle bh = scope.AddBioseq(*seq);
    CSeq_annot_Handle ah = scope.AddSeq_annot(*annot);
    BOOST_CHECK_EQUAL(CFeat_CI(bh).GetSize(), 1u);
    BOOST_REQUIRE_EQUAL(CFeat_CI(ah).GetSize(), 1u);
    CSeq_feat_EditHandle(*CFeat_CI(ah)).Replace(*feat2);
    BOOST_CHECK_EQUAL(CFeat_CI(bh).GetSize(), 1u);
}


BOOST_AUTO_TEST_CASE(TestEditAnnotOrphanAdd)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id1(new CSeq_id("lcl|Seq1"));
    CRef<CSeq_id> id2(new CSeq_id("KF591393"));
    CRef<CBioseq> seq = s_CreateBioseqForEdit(id1, id2);
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetFtable();
    CRef<CSeq_feat> feat = s_CreateFeatForEdit(id1);
    CBioseq_Handle bh = scope.AddBioseq(*seq);
    CSeq_annot_Handle ah = scope.AddSeq_annot(*annot, 3);
    BOOST_CHECK_EQUAL(CFeat_CI(bh).GetSize(), 0u);
    ah.GetEditHandle().AddFeat(*feat);
    BOOST_CHECK_EQUAL(CFeat_CI(bh).GetSize(), 1u);
}


BOOST_AUTO_TEST_CASE(TestEditAnnotOrphanReplace)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id1(new CSeq_id("lcl|Seq1"));
    CRef<CSeq_id> id2(new CSeq_id("KF591393"));
    CRef<CBioseq> seq = s_CreateBioseqForEdit(id1, id2);
    CRef<CSeq_feat> feat = s_CreateFeatForEdit(id1);
    CRef<CSeq_feat> feat2 = s_CreateFeatForEdit(id2);
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetFtable().push_back(feat);
    CBioseq_Handle bh = scope.AddBioseq(*seq);
    CSeq_annot_Handle ah = scope.AddSeq_annot(*annot, 3);
    BOOST_CHECK_EQUAL(CFeat_CI(bh).GetSize(), 1u);
    BOOST_REQUIRE_EQUAL(CFeat_CI(ah).GetSize(), 1u);
    CSeq_feat_EditHandle(*CFeat_CI(ah)).Replace(*feat2);
    BOOST_CHECK_EQUAL(CFeat_CI(bh).GetSize(), 1u);
}


BOOST_AUTO_TEST_CASE(TestEditAnnotNoSeqAdd)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id1(new CSeq_id("lcl|Seq1"));
    CRef<CSeq_id> id2(new CSeq_id("KF591393"));
    CRef<CSeq_loc> loc1(new CSeq_loc); loc1->SetWhole(*id1);
    CRef<CSeq_loc> loc2(new CSeq_loc); loc2->SetWhole(*id2);
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetFtable();
    CRef<CSeq_feat> feat = s_CreateFeatForEdit(id1);
    CSeq_annot_Handle ah = scope.AddSeq_annot(*annot);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc1, SAnnotSelector().SetSearchUnresolved()).GetSize(), 0u);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc2, SAnnotSelector().SetSearchUnresolved()).GetSize(), 0u);
    ah.GetEditHandle().AddFeat(*feat);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc1, SAnnotSelector().SetSearchUnresolved()).GetSize(), 1u);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc2, SAnnotSelector().SetSearchUnresolved()).GetSize(), 0u);
}


BOOST_AUTO_TEST_CASE(TestEditAnnotNoSeqReplace)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id1(new CSeq_id("lcl|Seq1"));
    CRef<CSeq_id> id2(new CSeq_id("KF591393"));
    CRef<CSeq_loc> loc1(new CSeq_loc); loc1->SetWhole(*id1);
    CRef<CSeq_loc> loc2(new CSeq_loc); loc2->SetWhole(*id2);
    CRef<CSeq_feat> feat = s_CreateFeatForEdit(id1);
    CRef<CSeq_feat> feat2 = s_CreateFeatForEdit(id2);
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetFtable().push_back(feat);
    CSeq_annot_Handle ah = scope.AddSeq_annot(*annot);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc1, SAnnotSelector().SetSearchUnresolved()).GetSize(), 1u);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc2, SAnnotSelector().SetSearchUnresolved()).GetSize(), 0u);
    BOOST_REQUIRE_EQUAL(CFeat_CI(ah).GetSize(), 1u);
    CSeq_feat_EditHandle(*CFeat_CI(ah)).Replace(*feat2);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc1, SAnnotSelector().SetSearchUnresolved()).GetSize(), 0u);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc2, SAnnotSelector().SetSearchUnresolved()).GetSize(), 1u);
}


BOOST_AUTO_TEST_CASE(TestEditAnnotBioseqAlignAdd)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id0(new CSeq_id("lcl|Ref"));
    CRef<CSeq_id> id1(new CSeq_id("lcl|Seq1"));
    CRef<CSeq_id> id2(new CSeq_id("KF591393"));
    CRef<CBioseq> seq = s_CreateBioseqForEdit(id1, id2);
    seq->SetAnnot().back()->SetData().SetAlign();
    CRef<CSeq_align> align = s_CreateAlignForEdit(id0, id1);
    CBioseq_Handle bh = scope.AddBioseq(*seq);
    BOOST_CHECK_EQUAL(CAlign_CI(bh).GetSize(), 0u);
    BOOST_REQUIRE(CSeq_annot_CI(bh));
    CSeq_annot_CI(bh)->GetEditHandle().AddAlign(*align);
    BOOST_CHECK_EQUAL(CAlign_CI(bh).GetSize(), 1u);
}


BOOST_AUTO_TEST_CASE(TestEditAnnotBioseqAlignReplace)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id0(new CSeq_id("lcl|Ref"));
    CRef<CSeq_id> id1(new CSeq_id("lcl|Seq1"));
    CRef<CSeq_id> id2(new CSeq_id("KF591393"));
    CRef<CBioseq> seq = s_CreateBioseqForEdit(id1, id2);
    seq->SetAnnot().back()->SetData().SetAlign();
    CRef<CSeq_align> align = s_CreateAlignForEdit(id0, id1);
    CRef<CSeq_align> align2 = s_CreateAlignForEdit(id0, id2);
    seq->SetAnnot().back()->SetData().SetAlign().push_back(align);
    CBioseq_Handle bh = scope.AddBioseq(*seq);
    BOOST_REQUIRE_EQUAL(CAlign_CI(bh).GetSize(), 1u);
    BOOST_CHECK_EQUAL(&CAlign_CI(bh).GetOriginalSeq_align(), align);
    BOOST_REQUIRE(CSeq_annot_CI(bh));
    BOOST_REQUIRE_EQUAL(CAlign_CI(*CSeq_annot_CI(bh)).GetSize(), 1u);
    CAlign_CI(*CSeq_annot_CI(bh)).GetSeq_align_Handle().Replace(*align2);
    BOOST_REQUIRE_EQUAL(CAlign_CI(bh).GetSize(), 1u);
    BOOST_CHECK_EQUAL(&CAlign_CI(bh).GetOriginalSeq_align(), align2);
}


BOOST_AUTO_TEST_CASE(TestEditAnnotBioseqGraphAdd)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id1(new CSeq_id("lcl|Seq1"));
    CRef<CSeq_id> id2(new CSeq_id("KF591393"));
    CRef<CBioseq> seq = s_CreateBioseqForEdit(id1, id2);
    seq->SetAnnot().back()->SetData().SetGraph();
    CRef<CSeq_graph> graph = s_CreateGraphForEdit(id1);
    CBioseq_Handle bh = scope.AddBioseq(*seq);
    BOOST_CHECK_EQUAL(CGraph_CI(bh).GetSize(), 0u);
    BOOST_REQUIRE(CSeq_annot_CI(bh));
    CSeq_annot_CI(bh)->GetEditHandle().AddGraph(*graph);
    BOOST_CHECK_EQUAL(CGraph_CI(bh).GetSize(), 1u);
}


BOOST_AUTO_TEST_CASE(TestEditAnnotBioseqGraphReplace)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id1(new CSeq_id("lcl|Seq1"));
    CRef<CSeq_id> id2(new CSeq_id("KF591393"));
    CRef<CBioseq> seq = s_CreateBioseqForEdit(id1, id2);
    seq->SetAnnot().back()->SetData().SetGraph();
    CRef<CSeq_graph> graph = s_CreateGraphForEdit(id1);
    CRef<CSeq_graph> graph2 = s_CreateGraphForEdit(id2);
    seq->SetAnnot().back()->SetData().SetGraph().push_back(graph);
    CBioseq_Handle bh = scope.AddBioseq(*seq);
    BOOST_REQUIRE_EQUAL(CGraph_CI(bh).GetSize(), 1u);
    BOOST_CHECK_EQUAL(&CGraph_CI(bh)->GetOriginalGraph(), graph);
    BOOST_REQUIRE(CSeq_annot_CI(bh));
    BOOST_REQUIRE_EQUAL(CGraph_CI(*CSeq_annot_CI(bh)).GetSize(), 1u);
    CGraph_CI(*CSeq_annot_CI(bh))->GetSeq_graph_Handle().Replace(*graph2);
    BOOST_REQUIRE_EQUAL(CGraph_CI(bh).GetSize(), 1u);
    BOOST_CHECK_EQUAL(&CGraph_CI(bh)->GetOriginalGraph(), graph2);
}


BOOST_AUTO_TEST_CASE(TestParent)
{
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id1(new CSeq_id("lcl|Seq1"));
    CRef<CBioseq> seq = s_CreateBioseqForEdit(id1);
    CBioseq_Handle bh = scope.AddBioseq(*seq);
    BOOST_CHECK(bh.GetParentEntry());
    BOOST_CHECK(bh.GetCompleteObject()->GetParentEntry());
    CSeq_entry_Handle seh = bh.GetParentEntry();
    
    CSeq_entry_EditHandle eh = seh.GetEditHandle();
    auto pBioseqEntry = eh.GetCompleteSeq_entry();
    auto pSeqParentEntry = pBioseqEntry->GetSeq().GetParentEntry();
    if (pSeqParentEntry) {
        cout << "Seq has parent entry" << endl;
    }
    else {
        cout << "Seq has no parent entry" << endl;
    }
    BOOST_CHECK(pSeqParentEntry);

    auto bioseq_set_handle = eh.ConvertSeqToSet(CBioseq_set::eClass_nuc_prot);
    auto parentHandle = bioseq_set_handle.GetParentEntry();
    if (parentHandle) {
        cout << "Set has parent handle" << endl;
    }
    else {
        cout << "Set has no parent handle" << endl;
    }
    BOOST_CHECK(parentHandle);

    auto pSetEntry = eh.GetCompleteSeq_entry();
    auto pSetParentEntry = pSetEntry->GetSet().GetParentEntry();
    if (pSetParentEntry) {
        cout << "Set has parent entry" << endl;
    }
    else {
        cout << "Set has no parent entry" << endl;
    }
    BOOST_CHECK(pSetParentEntry);
}


#ifdef NCBI_THREADS
static vector<size_t> s_GetBioseqParallel(size_t THREADS,
                                          CScope& scope,
                                          const vector< CRef<CSeq_id> >& ids,
                                          const vector< CRef<CSeq_id> >& ids2)
{
    vector< future<size_t> > ff(THREADS);
    for ( size_t ti = 0; ti < THREADS; ++ti ) {
        ff[ti] =
            async(std::launch::async,
                  [&]() -> size_t
                  {
                      size_t got_count = 0;
                      for ( size_t i = 0; i < ids.size(); ++i ) {
                          auto& id = ids[i];
                          auto& id2 = ids2[i];
                          bool good = scope.GetBioseqHandle(*id);
                          if ( good ) {
                              auto syn = scope.GetSynonyms(*id2);
                              good = syn &&
                                  syn->ContainsSynonym(CSeq_id_Handle::GetHandle(*id2)) &&
                                  syn->ContainsSynonym(CSeq_id_Handle::GetHandle(*id));
                          }
                          got_count += good;
                      }
                      return got_count;
                  });
    }
    vector<size_t> rr(THREADS);
    for ( size_t ti = 0; ti < THREADS; ++ti ) {
        rr[ti] = ff[ti].get();
    }
    return rr;
}

static vector<size_t> s_GetFeatParallel(size_t THREADS,
                                        CScope& scope,
                                        const vector< CRef<CSeq_id> >& ids)
{
    vector< future<size_t> > ff(THREADS);
    for ( size_t ti = 0; ti < THREADS; ++ti ) {
        ff[ti] =
            async(std::launch::async,
                  [&]() -> size_t
                  {
                      size_t got_count = 0;
                      for ( size_t i = 0; i < ids.size(); ++i ) {
                          auto& id = ids[i];
                          if ( CBioseq_Handle bh = scope.GetBioseqHandle(*id) ) {
                              got_count += CFeat_CI(bh).GetSize();
                          }
                      }
                      return got_count;
                  });
    }
    vector<size_t> rr(THREADS);
    for ( size_t ti = 0; ti < THREADS; ++ti ) {
        rr[ti] = ff[ti].get();
    }
    return rr;
}


BOOST_AUTO_TEST_CASE(TestReResolveMT1)
{
    const size_t COUNT = 1000;
    const size_t THREADS = 10;
    
    // check re-resolve after adding
    CScope scope(*CObjectManager::GetInstance());
    vector< CRef<CSeq_id> > ids, ids2;
    vector< CRef<CSeq_entry> > entries;
    for ( size_t i = 0; i < COUNT; ++i ) {
        ids.push_back(s_GetId(i));
        ids2.push_back(s_GetId2(i));
        entries.push_back(s_GetEntry(i));
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids, ids2) ) {
        BOOST_REQUIRE_EQUAL(c, 0u);
    }
    for ( size_t i = 0; i < COUNT; ++i ) {
        scope.AddTopLevelSeqEntry(*entries[i]);
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids, ids2) ) {
        BOOST_REQUIRE_EQUAL(c, COUNT);
    }
}


BOOST_AUTO_TEST_CASE(TestReResolveMT2)
{
    const size_t COUNT = 1000;
    const size_t THREADS = 10;
    
    // check re-resolve after adding
    CScope scope(*CObjectManager::GetInstance());
    vector< CRef<CSeq_id> > ids, ids2;
    vector< CRef<CSeq_entry> > entries;
    for ( size_t i = 0; i < COUNT; ++i ) {
        ids.push_back(s_GetId(i));
        ids2.push_back(s_GetId2(i));
        entries.push_back(s_GetEntry(i));
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids, ids2) ) {
        BOOST_REQUIRE_EQUAL(c, 0u);
    }
    vector<CSeq_entry_EditHandle> sehs;
    for ( size_t i = 0; i < COUNT; ++i ) {
        sehs.push_back(scope.AddTopLevelSeqEntry(*entries[i]).GetEditHandle());
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids, ids2) ) {
        BOOST_REQUIRE_EQUAL(c, COUNT);
    }
    vector<CBioseq_EditHandle> bhs;
    for ( size_t i = 0; i < COUNT; ++i ) {
        bhs.push_back(sehs[i].SetSeq());
        sehs[i].SelectNone();
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids, ids2) ) {
        BOOST_REQUIRE_EQUAL(c, 0u);
    }
    for ( size_t i = 0; i < COUNT; ++i ) {
        sehs[i].SelectSeq(bhs[i]);
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids, ids2) ) {
        BOOST_REQUIRE_EQUAL(c, COUNT);
    }
}


BOOST_AUTO_TEST_CASE(TestReResolveMT3)
{
    const size_t COUNT = 1000;
    const size_t THREADS = 10;
    
    // check re-resolve after adding
    CScope scope(*CObjectManager::GetInstance());
    vector< CRef<CSeq_id> > ids, ids2;
    vector< CRef<CSeq_entry> > entries;
    for ( size_t i = 0; i < COUNT; ++i ) {
        ids.push_back(s_GetId(i));
        ids2.push_back(s_GetId2(i));
        entries.push_back(s_GetEntry(i));
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids, ids2) ) {
        BOOST_REQUIRE_EQUAL(c, 0u);
    }
    vector<CSeq_entry_EditHandle> sehs;
    for ( size_t i = 0; i < COUNT; ++i ) {
        sehs.push_back(scope.AddTopLevelSeqEntry(*entries[i]).GetEditHandle());
    }
    vector<CBioseq_EditHandle> bhs;
    for ( size_t i = 0; i < COUNT; ++i ) {
        bhs.push_back(sehs[i].SetSeq());
        sehs[i].SelectNone();
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids, ids2) ) {
        BOOST_REQUIRE_EQUAL(c, 0u);
    }
    for ( size_t i = 0; i < COUNT; ++i ) {
        sehs[i].SelectSeq(bhs[i]);
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids, ids2) ) {
        BOOST_REQUIRE_EQUAL(c, COUNT);
    }
}


BOOST_AUTO_TEST_CASE(TestReResolveMT4)
{
    const size_t COUNT = 10000;
    const size_t THREADS = 40;
    
    // check re-resolve after adding
    CScope scope(*CObjectManager::GetInstance());
    vector< CRef<CSeq_id> > ids, ids2;
    vector< CRef<CSeq_entry> > entries;
    vector< CRef<CSeq_annot> > annots;
    size_t total_feats = 0;
    for ( size_t i = 0; i < COUNT; ++i ) {
        ids.push_back(s_GetId(i));
        ids2.push_back(s_GetId2(i));
        entries.push_back(s_GetEntry(i));
        size_t count = i%5+1;
        annots.push_back(s_GetAnnot(*ids.back(), count));
        total_feats += count;
    }
    LOG_POST("Resolving sequences");
    for ( auto c : s_GetBioseqParallel(2, scope, ids, ids2) ) {
        BOOST_REQUIRE_EQUAL(c, 0u);
    }
    LOG_POST("Resolving annots");
    for ( auto c : s_GetFeatParallel(2, scope, ids) ) {
        BOOST_REQUIRE_EQUAL(c, 0u);
    }
    LOG_POST("Adding sequences and annots");
    for ( size_t i = 0; i < COUNT; ++i ) {
        scope.AddTopLevelSeqEntry((const CSeq_entry&)*entries[i]);
        scope.AddSeq_annot(*annots[i]);
    }
    LOG_POST("Re-resolving sequences");
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids, ids2) ) {
        BOOST_REQUIRE_EQUAL(c, COUNT);
    }
    LOG_POST("Re-resolving annots");
    for ( auto c : s_GetFeatParallel(THREADS, scope, ids) ) {
        BOOST_REQUIRE_EQUAL(c, total_feats);
    }
}
#endif // NCBI_THREADS

BOOST_AUTO_TEST_CASE(CppIterFeat)
{
    // check for C++-11 style feature iteration
    CScope scope(*CObjectManager::GetInstance());
    const int master_num = 1;
    const int segment_num = 2;
    CRef<CSeq_id> master_id1 = s_GetId(master_num);
    CRef<CSeq_id> master_id2 = s_GetId2(master_num);
    CRef<CSeq_id> segment_id1 = s_GetId(segment_num);
    CRef<CSeq_id> segment_id2 = s_GetId2(segment_num);
    CRef<CSeq_entry> master_entry = s_GetDeltaSeqEntry(master_num, segment_num);
    CRef<CSeq_entry> segment_entry = s_GetEntry(segment_num, 20);
    master_entry->SetSeq().SetAnnot().push_back(s_GetAnnot(*master_id2));
    CSeq_entry_Handle master_seh =
        scope.AddTopLevelSeqEntry(const_cast<const CSeq_entry&>(*master_entry));
    CSeq_entry_Handle segment_seh =
        scope.AddTopLevelSeqEntry(const_cast<const CSeq_entry&>(*segment_entry));
    CRef<CSeq_loc> loc1(new CSeq_loc); loc1->SetWhole(*master_id1);
    CSeq_annot_Handle ah = scope.AddSeq_annot(*s_GetAnnot(*segment_id2, 3));
    SAnnotSelector sel;
    sel.SetResolveAll();
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc1).GetSize(), 1u);
    BOOST_CHECK_EQUAL(CFeat_CI(scope, *loc1, sel).GetSize(), 4u);
    vector<CMappedFeat> vv;
    for ( CFeat_CI it(scope, *loc1, sel); it; ++it ) {
        vv.push_back(*it);
    }
    BOOST_CHECK_EQUAL(vv.size(), 4u);
    {{
        auto vv_it = begin(vv);
        for ( CFeat_CI it(scope, *loc1, sel); it; ++it ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK_EQUAL(*it, *vv_it);
            BOOST_CHECK(it->GetMappedFeature().Equals(vv_it->GetMappedFeature()));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
    {{
        auto vv_it = begin(vv);
        for ( auto f : CFeat_CI(scope, *loc1, sel) ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK_EQUAL(f, *vv_it);
            BOOST_CHECK(f.GetMappedFeature().Equals(vv_it->GetMappedFeature()));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
    {{
        auto vv_it = begin(vv);
        for ( auto& f : CFeat_CI(scope, *loc1, sel) ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK_EQUAL(f, *vv_it);
            BOOST_CHECK(f.GetMappedFeature().Equals(vv_it->GetMappedFeature()));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
    {{
        auto vv_it = begin(vv);
        for ( auto&& f : CFeat_CI(scope, *loc1, sel) ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK_EQUAL(f, *vv_it);
            BOOST_CHECK(f.GetMappedFeature().Equals(vv_it->GetMappedFeature()));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
}


BOOST_AUTO_TEST_CASE(CppIterAlign)
{
    // check for C++-11 style align iteration
    CScope scope(*CObjectManager::GetInstance());
    const int master_num = 1;
    const int segment_num = 2;
    const int other_num = 3;
    CRef<CSeq_id> master_id1 = s_GetId(master_num);
    CRef<CSeq_id> master_id2 = s_GetId2(master_num);
    CRef<CSeq_id> segment_id1 = s_GetId(segment_num);
    CRef<CSeq_id> segment_id2 = s_GetId2(segment_num);
    CRef<CSeq_id> other_id = s_GetId(other_num);
    CRef<CSeq_entry> master_entry = s_GetDeltaSeqEntry(master_num, segment_num);
    CRef<CSeq_entry> segment_entry = s_GetEntry(segment_num, 20);
    master_entry->SetSeq().SetAnnot().push_back(s_GetAnnotAlign(*master_id2, *other_id));
    CSeq_entry_Handle master_seh =
        scope.AddTopLevelSeqEntry(const_cast<const CSeq_entry&>(*master_entry));
    CSeq_entry_Handle segment_seh =
        scope.AddTopLevelSeqEntry(const_cast<const CSeq_entry&>(*segment_entry));
    CRef<CSeq_loc> loc1(new CSeq_loc); loc1->SetWhole(*master_id1);
    CSeq_annot_Handle ah = scope.AddSeq_annot(*s_GetAnnotAlign(*segment_id2, *other_id, 3));
    //LOG_POST("master "<<MSerial_AsnText<<*master_seh.GetCompleteObject());
    //LOG_POST("segment "<<MSerial_AsnText<<*segment_seh.GetCompleteObject());
    //LOG_POST("annot "<<MSerial_AsnText<<*ah.GetCompleteObject());
    //LOG_POST("loc "<<MSerial_AsnText<<*loc1);
    SAnnotSelector sel;
    sel.SetResolveAll();
    BOOST_CHECK_EQUAL(CAlign_CI(scope, *loc1).GetSize(), 1u);
    BOOST_CHECK_EQUAL(CAlign_CI(scope, *loc1, sel).GetSize(), 4u);
    vector<CConstRef<CSeq_align>> vv;
    for ( CAlign_CI it(scope, *loc1, sel); it; ++it ) {
        vv.push_back(Ref(&*it));
    }
    BOOST_CHECK_EQUAL(vv.size(), 4u);
    {{
        auto vv_it = begin(vv);
        for ( CAlign_CI it(scope, *loc1, sel); it; ++it ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK(it->Equals(**vv_it));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
    /* no CSeq_align copy
    {{
        auto vv_it = begin(vv);
        for ( auto f : CAlign_CI(scope, *loc1, sel) ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK(f.Equals(**vv_it));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
    */
    {{
        auto vv_it = begin(vv);
        for ( auto& f : CAlign_CI(scope, *loc1, sel) ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK(f.Equals(**vv_it));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
    {{
        auto vv_it = begin(vv);
        for ( auto&& f : CAlign_CI(scope, *loc1, sel) ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK(f.Equals(**vv_it));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
}


BOOST_AUTO_TEST_CASE(CppIterGraph)
{
    // check for C++-11 style graph iteration
    CScope scope(*CObjectManager::GetInstance());
    const int master_num = 1;
    const int segment_num = 2;
    CRef<CSeq_id> master_id1 = s_GetId(master_num);
    CRef<CSeq_id> master_id2 = s_GetId2(master_num);
    CRef<CSeq_id> segment_id1 = s_GetId(segment_num);
    CRef<CSeq_id> segment_id2 = s_GetId2(segment_num);
    CRef<CSeq_entry> master_entry = s_GetDeltaSeqEntry(master_num, segment_num);
    CRef<CSeq_entry> segment_entry = s_GetEntry(segment_num, 20);
    master_entry->SetSeq().SetAnnot().push_back(s_GetAnnotGraph(*master_id2));
    CSeq_entry_Handle master_seh =
        scope.AddTopLevelSeqEntry(const_cast<const CSeq_entry&>(*master_entry));
    CSeq_entry_Handle segment_seh =
        scope.AddTopLevelSeqEntry(const_cast<const CSeq_entry&>(*segment_entry));
    CRef<CSeq_loc> loc1(new CSeq_loc); loc1->SetWhole(*master_id1);
    CSeq_annot_Handle ah = scope.AddSeq_annot(*s_GetAnnotGraph(*segment_id2, 3));
    //LOG_POST("master "<<MSerial_AsnText<<*master_seh.GetCompleteObject());
    //LOG_POST("segment "<<MSerial_AsnText<<*segment_seh.GetCompleteObject());
    //LOG_POST("annot "<<MSerial_AsnText<<*ah.GetCompleteObject());
    //LOG_POST("loc "<<MSerial_AsnText<<*loc1);
    SAnnotSelector sel;
    sel.SetResolveAll();
    BOOST_CHECK_EQUAL(CGraph_CI(scope, *loc1).GetSize(), 1u);
    BOOST_CHECK_EQUAL(CGraph_CI(scope, *loc1, sel).GetSize(), 4u);
    vector<CMappedGraph> vv;
    for ( CGraph_CI it(scope, *loc1, sel); it; ++it ) {
        vv.push_back(*it);
    }
    BOOST_CHECK_EQUAL(vv.size(), 4u);
    {{
        auto vv_it = begin(vv);
        for ( CGraph_CI it(scope, *loc1, sel); it; ++it ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK(*it == *vv_it);
            BOOST_CHECK(it->GetMappedGraph().Equals(vv_it->GetMappedGraph()));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
    {{
        auto vv_it = begin(vv);
        for ( auto f : CGraph_CI(scope, *loc1, sel) ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK(f == *vv_it);
            BOOST_CHECK(f.GetMappedGraph().Equals(vv_it->GetMappedGraph()));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
    {{
        auto vv_it = begin(vv);
        for ( auto& f : CGraph_CI(scope, *loc1, sel) ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK(f == *vv_it);
            BOOST_CHECK(f.GetMappedGraph().Equals(vv_it->GetMappedGraph()));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
    {{
        auto vv_it = begin(vv);
        for ( auto&& f : CGraph_CI(scope, *loc1, sel) ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK(f == *vv_it);
            BOOST_CHECK(f.GetMappedGraph().Equals(vv_it->GetMappedGraph()));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
}


BOOST_AUTO_TEST_CASE(CppIterTable)
{
    // check for C++-11 style Seq-table iteration
    CScope scope(*CObjectManager::GetInstance());
    const int master_num = 1;
    const int segment_num = 2;
    CRef<CSeq_id> master_id1 = s_GetId(master_num);
    CRef<CSeq_id> master_id2 = s_GetId2(master_num);
    CRef<CSeq_id> segment_id1 = s_GetId(segment_num);
    CRef<CSeq_id> segment_id2 = s_GetId2(segment_num);
    CRef<CSeq_entry> master_entry = s_GetDeltaSeqEntry(master_num, segment_num);
    CRef<CSeq_entry> segment_entry = s_GetEntry(segment_num, 20);
    master_entry->SetSeq().SetAnnot().push_back(s_GetAnnotTable(*master_id2));
    CSeq_entry_Handle master_seh =
        scope.AddTopLevelSeqEntry(const_cast<const CSeq_entry&>(*master_entry));
    CSeq_entry_Handle segment_seh =
        scope.AddTopLevelSeqEntry(const_cast<const CSeq_entry&>(*segment_entry));
    CRef<CSeq_loc> loc1(new CSeq_loc); loc1->SetWhole(*master_id1);
    CSeq_annot_Handle ah = scope.AddSeq_annot(*s_GetAnnotTable(*segment_id2));
    SAnnotSelector sel;
    sel.SetResolveAll();
    BOOST_CHECK_EQUAL(CSeq_table_CI(scope, *loc1).GetSize(), 1u);
    BOOST_CHECK_EQUAL(CSeq_table_CI(scope, *loc1, sel).GetSize(), 2u);
    vector<CSeq_annot_Handle> vv;
    for ( CSeq_table_CI it(scope, *loc1, sel); it; ++it ) {
        vv.push_back(*it);
    }
    BOOST_CHECK_EQUAL(vv.size(), 2u);
    {{
        auto vv_it = begin(vv);
        for ( CSeq_table_CI it(scope, *loc1, sel); it; ++it ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK(*it == *vv_it);
            //BOOST_CHECK(it->GetMappedTable().Equals(vv_it->GetMappedTable()));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
    {{
        auto vv_it = begin(vv);
        for ( auto f : CSeq_table_CI(scope, *loc1, sel) ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK(f == *vv_it);
            //BOOST_CHECK(f.GetMappedTable().Equals(vv_it->GetMappedTable()));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
    {{
        auto vv_it = begin(vv);
        for ( auto& f : CSeq_table_CI(scope, *loc1, sel) ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK(f == *vv_it);
            //BOOST_CHECK(f.GetMappedTable().Equals(vv_it->GetMappedTable()));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
    {{
        auto vv_it = begin(vv);
        for ( auto&& f : CSeq_table_CI(scope, *loc1, sel) ) {
            BOOST_REQUIRE(vv_it != end(vv));
            BOOST_CHECK(f == *vv_it);
            //BOOST_CHECK(f.GetMappedTable().Equals(vv_it->GetMappedTable()));
            ++vv_it;
        }
        BOOST_CHECK(vv_it == end(vv));
    }}
}


// verify error message
class CExpectDiagHandler : public CDiagHandler
{
public:
    CExpectDiagHandler()
    {
        Start();
    }
    ~CExpectDiagHandler()
    {
        Stop();
    }
    virtual void Post(const SDiagMessage& mess)
    {
        PostToConsole(mess);
        string s;
        mess.Write(s);
        m_Messages.push_back(s);
    }

    void Start(void)
    {
        m_Messages.clear();
        SetDiagHandler(this, false);
    }
    void Stop(void)
    {
        SetDiagStream(&cerr);
    }
    
    vector<string> m_Messages;
};


BOOST_AUTO_TEST_CASE(AddSeqIdHistory)
{
    // check Seq-id history reset and re-resolve
    auto old_level = SetDiagPostLevel(eDiag_Info);
    CExpectDiagHandler diag;
    string err_msg = "adding new data to a scope with non-empty history make data inconsistent";
    {{
        diag.Start();
        CScope scope(*CObjectManager::GetInstance());
        CRef<CSeq_id> id1 = s_GetId(1);
        CRef<CSeq_entry> entry = s_GetEntry(1, 0);
        BOOST_CHECK(!scope.GetBioseqHandle(*id1));
        BOOST_CHECK(diag.m_Messages.empty());
        LOG_POST("One message from CScope_Impl about data inconsistence is expected");
        diag.Start();
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry.GetNCObject());
        BOOST_REQUIRE_EQUAL(diag.m_Messages.size(), 1u);
        BOOST_CHECK(diag.m_Messages[0].find(err_msg) != NPOS);
        diag.Start();
        BOOST_CHECK(scope.GetBioseqHandle(*id1));
        BOOST_CHECK(diag.m_Messages.empty());
    }}
    {{
        diag.Start();
        CScope scope(*CObjectManager::GetInstance());
        CRef<CSeq_id> id1 = s_GetId(1);
        CRef<CSeq_entry> entry = s_GetEntry(1, 0);
        BOOST_CHECK(!scope.GetBioseqHandle(*id1));
        BOOST_CHECK(diag.m_Messages.empty());
        scope.RemoveFromHistory(*id1);
        BOOST_CHECK(diag.m_Messages.empty());
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry.GetNCObject());
        BOOST_CHECK(diag.m_Messages.empty());
        BOOST_CHECK(scope.GetBioseqHandle(*id1));
        BOOST_CHECK(diag.m_Messages.empty());
    }}
    SetDiagPostLevel(old_level);
}
