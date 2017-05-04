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
*   Unit test for data loading from ID.
*/

#define NCBI_TEST_APPLICATION

#include <ncbi_pch.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/annot_ci.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seq/seq__.hpp>
#include <objmgr/util/sequence.hpp>
#include <serial/iterator.hpp>

#include <thread>
#include <future>

#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

static CRef<CSeq_id> s_GetId(size_t i)
{
    CRef<CSeq_id> id(new CSeq_id());
    id->SetGi(TIntId(i+1));
    return id;
}


static CRef<CSeq_entry> s_GetEntry(size_t i)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq& seq = entry->SetSeq();
    seq.SetId().push_back(s_GetId(i));
    CSeq_inst& inst = seq.SetInst();
    inst.SetRepr(inst.eRepr_raw);
    inst.SetMol(inst.eMol_aa);
    inst.SetSeq_data().SetIupacaa().Set("AA");
    return entry;
}


BOOST_AUTO_TEST_CASE(TestReResolve1)
{
    // check re-resolve after adding
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id = s_GetId(1);
    CRef<CSeq_entry> entry = s_GetEntry(1);
    BOOST_REQUIRE(!scope.GetBioseqHandle(*id));
    scope.AddTopLevelSeqEntry(*entry);
    BOOST_REQUIRE(scope.GetBioseqHandle(*id));
}


BOOST_AUTO_TEST_CASE(TestReResolve2)
{
    // check re-resolve after adding, removing, and restoring
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id = s_GetId(1);
    CRef<CSeq_entry> entry = s_GetEntry(1);
    BOOST_REQUIRE(!scope.GetBioseqHandle(*id));
    CSeq_entry_EditHandle seh = scope.AddTopLevelSeqEntry(*entry).GetEditHandle();
    BOOST_REQUIRE(scope.GetBioseqHandle(*id));
    CBioseq_EditHandle bh = seh.SetSeq();
    seh.SelectNone();
    BOOST_REQUIRE(!scope.GetBioseqHandle(*id));
    seh.SelectSeq(bh);
    BOOST_REQUIRE(scope.GetBioseqHandle(*id));
}


BOOST_AUTO_TEST_CASE(TestReResolve3)
{
    // check re-resolve after adding, removing, and restoring, resolving only at the end
    CScope scope(*CObjectManager::GetInstance());
    CRef<CSeq_id> id = s_GetId(1);
    CRef<CSeq_entry> entry = s_GetEntry(1);
    BOOST_REQUIRE(!scope.GetBioseqHandle(*id));
    CSeq_entry_EditHandle seh = scope.AddTopLevelSeqEntry(*entry).GetEditHandle();
    CBioseq_EditHandle bh = seh.SetSeq();
    seh.SelectNone();
    BOOST_REQUIRE(!scope.GetBioseqHandle(*id));
    seh.SelectSeq(bh);
    BOOST_REQUIRE(scope.GetBioseqHandle(*id));
}


static vector<size_t> s_GetBioseqParallel(size_t THREADS,
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
                      for ( auto& id : ids ) {
                          got_count += bool(scope.GetBioseqHandle(*id));
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
    vector< CRef<CSeq_id> > ids;
    vector< CRef<CSeq_entry> > entries;
    for ( size_t i = 0; i < COUNT; ++i ) {
        ids.push_back(s_GetId(i));
        entries.push_back(s_GetEntry(i));
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids) ) {
        BOOST_REQUIRE_EQUAL(c, 0u);
    }
    for ( size_t i = 0; i < COUNT; ++i ) {
        scope.AddTopLevelSeqEntry(*entries[i]);
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids) ) {
        BOOST_REQUIRE_EQUAL(c, COUNT);
    }
}


BOOST_AUTO_TEST_CASE(TestReResolveMT2)
{
    const size_t COUNT = 1000;
    const size_t THREADS = 10;
    
    // check re-resolve after adding
    CScope scope(*CObjectManager::GetInstance());
    vector< CRef<CSeq_id> > ids;
    vector< CRef<CSeq_entry> > entries;
    for ( size_t i = 0; i < COUNT; ++i ) {
        ids.push_back(s_GetId(i));
        entries.push_back(s_GetEntry(i));
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids) ) {
        BOOST_REQUIRE_EQUAL(c, 0u);
    }
    vector<CSeq_entry_EditHandle> sehs;
    for ( size_t i = 0; i < COUNT; ++i ) {
        sehs.push_back(scope.AddTopLevelSeqEntry(*entries[i]).GetEditHandle());
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids) ) {
        BOOST_REQUIRE_EQUAL(c, COUNT);
    }
    vector<CBioseq_EditHandle> bhs;
    for ( size_t i = 0; i < COUNT; ++i ) {
        bhs.push_back(sehs[i].SetSeq());
        sehs[i].SelectNone();
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids) ) {
        BOOST_REQUIRE_EQUAL(c, 0u);
    }
    for ( size_t i = 0; i < COUNT; ++i ) {
        sehs[i].SelectSeq(bhs[i]);
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids) ) {
        BOOST_REQUIRE_EQUAL(c, COUNT);
    }
}


BOOST_AUTO_TEST_CASE(TestReResolveMT3)
{
    const size_t COUNT = 1000;
    const size_t THREADS = 10;
    
    // check re-resolve after adding
    CScope scope(*CObjectManager::GetInstance());
    vector< CRef<CSeq_id> > ids;
    vector< CRef<CSeq_entry> > entries;
    for ( size_t i = 0; i < COUNT; ++i ) {
        ids.push_back(s_GetId(i));
        entries.push_back(s_GetEntry(i));
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids) ) {
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
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids) ) {
        BOOST_REQUIRE_EQUAL(c, 0u);
    }
    for ( size_t i = 0; i < COUNT; ++i ) {
        sehs[i].SelectSeq(bhs[i]);
    }
    for ( auto c : s_GetBioseqParallel(THREADS, scope, ids) ) {
        BOOST_REQUIRE_EQUAL(c, COUNT);
    }
}
