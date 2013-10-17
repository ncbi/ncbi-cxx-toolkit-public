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
 * Author:  Christiam Camacho
 *
 * File Description:
 *   Unit test module for the preliminary stage of the BLAST search.
 *
 * ===========================================================================
 */
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <algo/blast/api/uniform_search.hpp>    // for CSearchDatabase
#include <algo/blast/api/prelim_stage.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/seqsrc_seqdb.hpp>
#include "blast_test_util.hpp"
#include "test_objmgr.hpp"

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

void x_ValidateResultsForShortProteinSearch(CBlastPrelimSearch& blaster,
                                           BlastHSPStream* hsp_stream, 
                                           CConstRef<CBlastOptions> options) {

        CBlastHSPResults hsp_results
            (blaster.ComputeBlastHSPResults(hsp_stream));

        BOOST_REQUIRE_EQUAL((Int4)1, hsp_results->num_queries);
        BOOST_REQUIRE(hsp_results->hitlist_array[0]);
        BOOST_REQUIRE_EQUAL((Int4)24, 
                             hsp_results->hitlist_array[0]->hsplist_count);
        BOOST_REQUIRE(hsp_results->hitlist_array[0]->hsplist_array[0]);
        BlastHSPList* hsp_list = 
            hsp_results->hitlist_array[0]->hsplist_array[0];
        BOOST_REQUIRE(hsp_list);
        BOOST_REQUIRE_EQUAL((Int4)0, hsp_list[0].oid);
        BOOST_REQUIRE_EQUAL((Int4)1, hsp_list[0].hspcnt);
        BOOST_REQUIRE(hsp_list[0].hsp_array[0]);
        BOOST_REQUIRE_EQUAL((Int4)103, hsp_list[0].hsp_array[0]->score);
        BOOST_REQUIRE_EQUAL((Int4)0, hsp_list[0].hsp_array[0]->query.offset);
        BOOST_REQUIRE_EQUAL((Int4)21, hsp_list[0].hsp_array[0]->query.end);
        BOOST_REQUIRE_EQUAL((Int4)0, 
                             hsp_list[0].hsp_array[0]->subject.offset);
        BOOST_REQUIRE_EQUAL((Int4)21, 
                             hsp_list[0].hsp_array[0]->query.end);

}

BOOST_AUTO_TEST_SUITE(prelimsearch)

BOOST_AUTO_TEST_CASE(ShortProteinSearch) {
    CSeq_id id(CSeq_id::e_Gi, 1786182);
    CBlastQueryVector q;
    q.AddQuery(CTestObjMgr::Instance().CreateBlastSearchQuery(id));
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(q));

    // Create the options
    CRef<CBlastOptionsHandle> options_handle
        (CBlastOptionsFactory::Create(eBlastp));
    CRef<CBlastOptions> options(&options_handle->SetOptions());
    options->SetSegFiltering(false);    // allow hits to be found

    // Create the database description (by default will use CSeqDB)
    CSearchDatabase dbinfo("ecoli", CSearchDatabase::eBlastDbIsProtein);

    CBlastPrelimSearch prelim_search(query_factory, options, dbinfo);
    BOOST_REQUIRE(prelim_search.GetNumberOfThreads() == 1);
    BOOST_REQUIRE(prelim_search.IsMultiThreaded() == false);

    CRef<SInternalData> results = prelim_search.Run();
    BOOST_REQUIRE(results.GetPointer() != 0);

    BOOST_REQUIRE(results->m_HspStream != 0);
    BOOST_REQUIRE(results->m_Diagnostics != 0);

    x_ValidateResultsForShortProteinSearch
        (prelim_search, results->m_HspStream->GetPointer(), options);
}

BOOST_AUTO_TEST_CASE(ShortProteinSearchMT) {
    CSeq_id id(CSeq_id::e_Gi, 1786182);
    CBlastQueryVector q;
    q.AddQuery(CTestObjMgr::Instance().CreateBlastSearchQuery(id));
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(q));

    // Create the options
    CRef<CBlastOptionsHandle> options_handle
        (CBlastOptionsFactory::Create(eBlastp));
    CRef<CBlastOptions> options(&options_handle->SetOptions());
    options->SetSegFiltering(false);    // allow hits to be found

    // Create the database description (by default will use CSeqDB)
    CSearchDatabase dbinfo("ecoli", CSearchDatabase::eBlastDbIsProtein);

    CBlastPrelimSearch prelim_search(query_factory, options, dbinfo);
    prelim_search.SetNumberOfThreads(2);
    BOOST_REQUIRE(prelim_search.GetNumberOfThreads() == 2);
    BOOST_REQUIRE(prelim_search.IsMultiThreaded() == true);

    CRef<SInternalData> results = prelim_search.Run();
    BOOST_REQUIRE(results.GetPointer() != 0);

    BOOST_REQUIRE(results->m_HspStream != 0);
    BOOST_REQUIRE(results->m_Diagnostics != 0);

    x_ValidateResultsForShortProteinSearch
        (prelim_search, results->m_HspStream->GetPointer(), options);
}

// This tests a problem that occurred when a chunk consisted of only N's, so that
// Karlin-Altschul statistics were not calculated.  This is a test for SB-546.
BOOST_AUTO_TEST_CASE(SplitNucleotideQuery) {
    CSeq_id q_id(CSeq_id::e_Gi, 224384753);
    const TSeqRange kRange(0, 5000000);
    const ENa_strand kStrand(eNa_strand_plus);
    auto_ptr<SSeqLoc> q_ssl(CTestObjMgr::Instance().CreateSSeqLoc(q_id, kRange, kStrand));
    TSeqLocVector q_tsl;
    q_tsl.push_back(*q_ssl);
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(q_tsl));


    CSearchDatabase dbinfo("data/nt.41646578", CSearchDatabase::eBlastDbIsNucleotide);

    // Create the options
    CRef<CBlastOptionsHandle> options_handle
        (CBlastOptionsFactory::Create(eMegablast));
    CRef<CBlastOptions> options(&options_handle->SetOptions());

    // Setting the chunk size low means we hit an area of all N's pretty quickly.
    CAutoEnvironmentVariable tmp_env("CHUNK_SIZE", "40000");

    CBlastPrelimSearch prelim_search(query_factory, options, dbinfo);

    // The main thing here is that an exception is NOT thrown.
    CRef<SInternalData> results = prelim_search.Run();
    BOOST_REQUIRE(results.GetPointer() != 0);
    BOOST_REQUIRE(results->m_HspStream != 0);
    BOOST_REQUIRE(results->m_Diagnostics != 0);
}

BOOST_AUTO_TEST_CASE(BuildCStd_seg_blastn) {
    CSeq_id q_id(CSeq_id::e_Gi, 41646578);
    const TSeqRange kRange(54, 560);
    const ENa_strand kStrand(eNa_strand_plus);
    auto_ptr<SSeqLoc> q_ssl(CTestObjMgr::Instance().CreateSSeqLoc(q_id, kRange, kStrand));
    TSeqLocVector q_tsl;
    q_tsl.push_back(*q_ssl);
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(q_tsl));


    CSearchDatabase dbinfo("data/nt.41646578", CSearchDatabase::eBlastDbIsNucleotide);

    // Create the options
    CRef<CBlastOptionsHandle> options_handle
        (CBlastOptionsFactory::Create(eMegablast));
    CRef<CBlastOptions> options(&options_handle->SetOptions());

    CBlastPrelimSearch prelim_search(query_factory, options, dbinfo);

    std::vector<std::list<CRef<CStd_seg> > > 	l;
    prelim_search.Run(l);

    BOOST_REQUIRE(l.size() == 1);
    BOOST_REQUIRE(l[0].size() >= 1);
    CRef<CStd_seg>  & seg = l[0].front();
    BOOST_REQUIRE(seg->GetSeqStart(0) == 0);
    BOOST_REQUIRE(seg->GetSeqStop(0) == 506);
    BOOST_REQUIRE(seg->GetSeqStart(1) == 54);
    BOOST_REQUIRE(seg->GetSeqStop(1) == 560);
    const vector<CRef<CSeq_id> > & id = seg->GetIds();
    BOOST_REQUIRE(id[0]->GetSeqIdString() == "41646578"); 
    const vector<CRef<CSeq_loc> > & loc = seg->GetLoc();
    BOOST_REQUIRE(loc[0]->GetStrand() ==  eNa_strand_plus);
    BOOST_REQUIRE(loc[1]->GetStrand() ==  eNa_strand_plus);
}


BOOST_AUTO_TEST_CASE(BuildCStd_seg_tblastx) {
    CSeq_id q_id(CSeq_id::e_Gi, 41646578);
    const TSeqRange kRange(54, 560);
    const ENa_strand kStrand(eNa_strand_plus);
    auto_ptr<SSeqLoc> q_ssl(CTestObjMgr::Instance().CreateSSeqLoc(q_id, kRange, kStrand));
    TSeqLocVector q_tsl;
    q_tsl.push_back(*q_ssl);
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(q_tsl));


    CSearchDatabase dbinfo("data/nt.41646578", CSearchDatabase::eBlastDbIsNucleotide);

    // Create the options
    CRef<CBlastOptionsHandle> options_handle
        (CBlastOptionsFactory::Create(eTblastx));
    CRef<CBlastOptions> options(&options_handle->SetOptions());

    CBlastPrelimSearch prelim_search(query_factory, options, dbinfo);

    std::vector<std::list<CRef<CStd_seg> > > 	l;
    prelim_search.Run(l);

    BOOST_REQUIRE(l.size() == 1);
    BOOST_REQUIRE(l[0].size() > 1);
    CRef<CStd_seg>  & seg = l[0].front();
    BOOST_REQUIRE(seg->GetSeqStart(0) == 0);
    BOOST_REQUIRE(seg->GetSeqStop(0) == 506);
    BOOST_REQUIRE(seg->GetSeqStart(1) == 54);
    BOOST_REQUIRE(seg->GetSeqStop(1) == 560);
    const vector<CRef<CSeq_id> > & id = seg->GetIds();
    BOOST_REQUIRE(id[0]->GetSeqIdString() == "41646578"); 
    const vector<CRef<CSeq_loc> > & loc = seg->GetLoc();
    BOOST_REQUIRE(loc[0]->GetStrand() ==  eNa_strand_plus);
    BOOST_REQUIRE(loc[1]->GetStrand() ==  eNa_strand_plus);
}


BOOST_AUTO_TEST_SUITE_END()
