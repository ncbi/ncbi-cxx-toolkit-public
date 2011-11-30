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
* Author: Tom Madden
*
* File Description:
*   Unit tests for QueryInfo setup
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <corelib/ncbi_limits.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include "test_objmgr.hpp"

#include <algo/blast/api/bl2seq.hpp>
#include <blast_objmgr_priv.hpp>

#include <algo/blast/api/blast_options_handle.hpp>

using namespace ncbi::objects;
using namespace ncbi::blast;

BOOST_AUTO_TEST_SUITE(QueryInfo)

BOOST_AUTO_TEST_CASE(ProteinGetQueryInfo) {
    const int kNumQueries=1;
    CSeq_id id("gi|3091");
    auto_ptr<SSeqLoc> qsl(CTestObjMgr::Instance().CreateSSeqLoc(id));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);
    CBlastQueryInfo query_info;
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastp));

    const CBlastOptions& kOpts = opts->GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(query_v, prog, strand_opt, &query_info);

    BOOST_REQUIRE_EQUAL(kNumQueries, query_info->num_queries);
    BOOST_REQUIRE_EQUAL(0, query_info->first_context);
    BOOST_REQUIRE_EQUAL(0, query_info->last_context);
    BOOST_REQUIRE_EQUAL(0, query_info->contexts[0].query_offset);
    BOOST_REQUIRE_EQUAL(607, query_info->contexts[0].query_length);
}

BOOST_AUTO_TEST_CASE(EmptyBlastxGetQueryInfo) {
    const int kNumQueries=1;
    CSeq_id id("gi|3090");
    pair<TSeqPos, TSeqPos> range(11, 10);
    auto_ptr<SSeqLoc> qsl(
        CTestObjMgr::Instance().CreateSSeqLoc(id, range, eNa_strand_both));
        
    TSeqLocVector query_v;
    query_v.push_back(*qsl);
    CBlastQueryInfo query_info=NULL;
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastx));

    const CBlastOptions& kOpts = opts->GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();
    
    SetupQueryInfo(query_v, prog, strand_opt, &query_info);
    BOOST_REQUIRE_EQUAL(kNumQueries, query_info->num_queries);
    BOOST_REQUIRE_EQUAL(0, query_info->first_context);
    BOOST_REQUIRE_EQUAL(5, query_info->last_context);
    BOOST_REQUIRE_EQUAL(0, query_info->contexts[0].query_offset);
    BOOST_REQUIRE_EQUAL(0, query_info->contexts[5].query_length);
}

BOOST_AUTO_TEST_CASE(BlastnGetQueryInfo) {
    const int kNumQueries=2;
    CSeq_id id1("gi|3090");
    CSeq_id id2("gi|555");
    auto_ptr<SSeqLoc> qsl1(CTestObjMgr::Instance().CreateSSeqLoc(id1));
    auto_ptr<SSeqLoc> qsl2(CTestObjMgr::Instance().CreateSSeqLoc(id2));
    TSeqLocVector query_v;
    query_v.push_back(*qsl1);
    query_v.push_back(*qsl2);
    CBlastQueryInfo query_info;
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastn));

    const CBlastOptions& kOpts = opts->GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(query_v, prog, strand_opt, &query_info);

    BOOST_REQUIRE_EQUAL(kNumQueries, query_info->num_queries);
    BOOST_REQUIRE_EQUAL(0, query_info->first_context);
    BOOST_REQUIRE_EQUAL(3, query_info->last_context);
    BOOST_REQUIRE_EQUAL(0, query_info->contexts[0].query_offset);
    BOOST_REQUIRE_EQUAL(2338, query_info->contexts[0].query_length);
}

BOOST_AUTO_TEST_CASE(BlastnGetQueryIndex) {
    CSeq_id id1("gi|3090");
    CSeq_id id2("gi|555");
    auto_ptr<SSeqLoc> qsl1(CTestObjMgr::Instance().CreateSSeqLoc(id1));
    auto_ptr<SSeqLoc> qsl2(CTestObjMgr::Instance().CreateSSeqLoc(id2));
    TSeqLocVector query_v;
    query_v.push_back(*qsl1);
    query_v.push_back(*qsl2);
    CBlastQueryInfo query_info;
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastn));

    const CBlastOptions& kOpts = opts->GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(query_v, prog, strand_opt, &query_info);

    int query_index = Blast_GetQueryIndexFromQueryOffset(3000, prog, query_info);
    BOOST_REQUIRE_EQUAL(0, query_index);
    query_index = Blast_GetQueryIndexFromQueryOffset(5010, prog, query_info);
    BOOST_REQUIRE_EQUAL(1, query_index);
}


BOOST_AUTO_TEST_SUITE_END()
