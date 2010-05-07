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
* Author:  Tom Madden
*
* File Description:
*   Unit test module to test CVecscreenRun class.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbistre.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>

#include <algo/blast/format/vecscreen_run.hpp>

#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>
#include <boost/test/auto_unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>


using namespace ncbi;
using namespace ncbi::blast;
using namespace ncbi::objects;


BOOST_AUTO_TEST_SUITE(vecscreen_run)

BOOST_AUTO_TEST_CASE(VecscreenRunWithHits)
{
    const bool is_prot = false;
    SDataLoaderConfig config(is_prot, SDataLoaderConfig::eUseGenbankDataLoader);

    CBlastScopeSource ss(config);
    CRef<CScope> scope = ss.NewScope();

    CRef<CSeq_loc> query_loc(new CSeq_loc());
    query_loc->SetWhole().SetGi(555);

    CVecscreenRun vs_run(query_loc, scope);

    list<CVecscreenRun::SVecscreenSummary> vs_list = vs_run.GetList();

    CVecscreenRun::SVecscreenSummary& match = vs_list.front();

    BOOST_REQUIRE(vs_list.size() == 1);
    BOOST_REQUIRE(match.range.GetFrom() == 588);
    BOOST_REQUIRE(match.range.GetTo() == 623);

    CRef<CSeq_align_set> seqalign_set = vs_run.GetSeqalignSet();
    BOOST_REQUIRE(seqalign_set->Size() == 1);
}

BOOST_AUTO_TEST_CASE(VecscreenRunWithNoHits)
{
    const bool is_prot = false;
    SDataLoaderConfig config(is_prot, SDataLoaderConfig::eUseGenbankDataLoader);

    CBlastScopeSource ss(config);
    CRef<CScope> scope = ss.NewScope();

    CRef<CSeq_loc> query_loc(new CSeq_loc());
    query_loc->SetWhole().SetGi(405832);

    CVecscreenRun vs_run(query_loc, scope);

    list<CVecscreenRun::SVecscreenSummary> vs_list = vs_run.GetList();

    BOOST_REQUIRE(vs_list.size() == 0);

    CRef<CSeq_align_set> seqalign_set = vs_run.GetSeqalignSet();
    BOOST_REQUIRE(seqalign_set->Size() == 0);
}

BOOST_AUTO_TEST_CASE(VecscreenRunWithNoDataLoader)
{
    // Data loader is intentionally missing, so as to provoke constructor exception
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));

    CRef<CSeq_loc> query_loc(new CSeq_loc());
    query_loc->SetWhole().SetGi(555);

    BOOST_REQUIRE_THROW(CVecscreenRun vs_run(query_loc, scope), blast::CBlastException);
}

BOOST_AUTO_TEST_SUITE_END()
