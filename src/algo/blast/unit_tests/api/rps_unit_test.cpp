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
* Author:  Jason Papadopoulos
*
* File Description:
*   Unit test for RPS blast
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>

#include <algo/blast/api/seqsrc_seqdb.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <blast_seqalign.hpp>

#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_lookup.h>

#include "test_objmgr.hpp"
#include "blast_test_util.hpp"

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

void testNuclHitList(const CSeq_align_set& results, ENa_strand strand)
{
   const size_t num_hsps_total = 10;
   const size_t num_hsps_plus = 4;
   const int scores[num_hsps_total] = 
     {62, 51, 49, 44, 44, 43, 43, 48, 45, 46};
   const ENa_strand strands[num_hsps_total] = 
   {
       eNa_strand_minus,
       eNa_strand_plus,
       eNa_strand_minus,
       eNa_strand_minus,
       eNa_strand_plus,
       eNa_strand_plus,
       eNa_strand_minus,
       eNa_strand_minus,
       eNa_strand_minus,
       eNa_strand_plus,
   };
   const int q_offsets[num_hsps_total] =
      {3244, 1045, 3133, 3163, 8179, 1090, 1328, 832, 6776, 3633};
   const int s_offsets[num_hsps_total] =
      {700, 662, 812, 146, 538, 930, 1340, 1917, 1467, 966};
   const int q_ends[num_hsps_total] =
      {3430, 1159, 3283, 3328, 8287, 1174, 1388, 937, 6968, 3759};
   const int s_ends[num_hsps_total] =
      {761, 699, 861, 205, 564, 958, 1360, 1952, 1531, 1007};
   const double evalues[num_hsps_total] =
      {0.0546847, 1.0657, 2.00126, 7.83533, 8.12416, 9.05612, 9.84108, 2.53047, 5.47761, 3.70474};

   // compute the total number of alignments
   size_t num_hsps = results.Size();
/*
   ITERATE(CSeq_align_set::Tdata, itr, results.Get()) {
       num_hsps += (*itr).Size();
   }
*/
   switch (strand) {
   case eNa_strand_plus:
  BOOST_REQUIRE_EQUAL(num_hsps_plus, num_hsps);
  break;
   case eNa_strand_minus:
  BOOST_REQUIRE_EQUAL(num_hsps_total - num_hsps_plus, num_hsps);
  break;
   default:
  BOOST_REQUIRE_EQUAL(num_hsps_total, num_hsps);
  break;
   }

   // for each subject sequence
   size_t align_num = 0;
   ITERATE(CSeq_align_set::Tdata, list1, results.Get()) {
       const CSeq_align& hitlist = **list1;

       // for each hit

           BOOST_REQUIRE(hitlist.GetSegs().IsStd());
           const list<CRef<CStd_seg> >& stdseg = hitlist.GetSegs().GetStd();
           ENa_strand curr_strand = 
                     stdseg.front()->GetLoc().front()->GetInt().GetStrand();

           // skip ahead to the known answer for this hit

           if (strand != eNa_strand_both) {
               while (align_num < num_hsps_total && 
                      strands[align_num] != curr_strand)
                   align_num++;
               BOOST_REQUIRE(align_num < num_hsps_total);
           }

           // test scores and e-values

           ITERATE(CSeq_align::TScore, sitr, hitlist.GetScore()) {
               const CScore& curr_score = **sitr;
               if (curr_score.GetId().GetStr() == "e_value" ) {
                   BOOST_REQUIRE_CLOSE(evalues[align_num],
                                 curr_score.GetValue().GetReal(), 0.1);
               }
               else if (curr_score.GetId().GetStr() == "score") {
                   BOOST_REQUIRE_EQUAL(scores[align_num],
                                        curr_score.GetValue().GetInt());
               }
           }

           // test sequence offsets; the end offsets can only
           // be computed by iterating through traceback

           const CStd_seg::TLoc& locs = stdseg.front()->GetLoc();
           int off1 = locs[0]->GetInt().GetFrom();
           int off2 = locs[1]->GetInt().GetFrom();
           BOOST_REQUIRE_EQUAL(q_offsets[align_num], off1);
           BOOST_REQUIRE_EQUAL(s_offsets[align_num], off2);
           
           ITERATE(list<CRef<CStd_seg> >, seg_itr, stdseg) {
               const CStd_seg::TLoc& seqloc = (*seg_itr)->GetLoc();
               if (seqloc[0]->IsEmpty()) {
                   off2 += seqloc[1]->GetInt().GetTo() -
                           seqloc[1]->GetInt().GetFrom() + 1;
               }
               else if (seqloc[1]->IsEmpty()) {
                   off1 += seqloc[0]->GetInt().GetTo() -
                           seqloc[0]->GetInt().GetFrom() + 1;
               }
               else {
                   off1 += seqloc[0]->GetInt().GetTo() -
                           seqloc[0]->GetInt().GetFrom() + 1;
                   off2 += seqloc[1]->GetInt().GetTo() -
                           seqloc[1]->GetInt().GetFrom() + 1;
               }
           }
           BOOST_REQUIRE_EQUAL(q_ends[align_num], off1);
           BOOST_REQUIRE_EQUAL(s_ends[align_num], off2);
           align_num++;
   }
}

struct RpsTestFixture {
    string m_DbName;

    RpsTestFixture() {
#if defined(WORDS_BIGENDIAN) || defined(IS_BIG_ENDIAN)
        m_DbName = "data/rpstest_be";
#else
        m_DbName = "data/rpstest_le";
#endif
    }

    void NuclSearch(ENa_strand strand) {
        CRef<CBlastOptionsHandle> 
            opts(CBlastOptionsFactory::Create(eRPSTblastn));
        opts->SetFilterString("F");

        CSeq_id id("gi|19572546");
        auto_ptr<SSeqLoc> query(
            CTestObjMgr::Instance().CreateSSeqLoc(id, strand));
        TSeqLocVector query_v;
        query_v.push_back(*query);

        CSearchDatabase dbinfo(m_DbName, CSearchDatabase::eBlastDbIsProtein);
        CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(query_v));
        CLocalBlast blaster(query_factory, opts, dbinfo);
        CSearchResultSet results = *blaster.Run();

        testNuclHitList(*results[0].GetSeqAlign(), strand);
    }
};

BOOST_FIXTURE_TEST_SUITE(rps, RpsTestFixture)

/* prompted by a bug that caused alignments reaching the end
   of a DB sequence to not include the last letter */
BOOST_AUTO_TEST_CASE(WholeSequenceMatch) {

    CSeq_id id("gi|38092615");      /* query = first DB sequence */
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_unknown));
    TSeqLocVector query_v;
    query_v.push_back(*query);

    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eRPSBlast));

    CSearchDatabase dbinfo(m_DbName, CSearchDatabase::eBlastDbIsProtein);
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(query_v));
    CLocalBlast blaster(query_factory, opts, dbinfo);
    CSearchResultSet results = *blaster.Run();

    const CSeq_align& hitlist_sa = *results[0].GetSeqAlign()->Get().front();
    const CDense_seg& denseg = hitlist_sa.GetSegs().GetDenseg();

    int off1 = denseg.GetStarts()[0];
    int off2 = denseg.GetStarts()[1];
    BOOST_REQUIRE_EQUAL(0, off1);
    BOOST_REQUIRE_EQUAL(0, off2);
    off1 += denseg.GetLens()[0];
    off2 += denseg.GetLens()[0];
    BOOST_REQUIRE_EQUAL(1016, (int)denseg.GetLens()[0]);
}
BOOST_AUTO_TEST_CASE(NuclSearchPlusStrand) {
  NuclSearch(eNa_strand_plus);
}
BOOST_AUTO_TEST_CASE(NuclSearchMinusStrand) {
  NuclSearch(eNa_strand_minus);
}
BOOST_AUTO_TEST_CASE(NuclSearchBothStrands) {
  NuclSearch(eNa_strand_both);
}

BOOST_AUTO_TEST_CASE(testPreliminarySearch)
{
    const int kQueryGi = 129295;
    const int kNumHits = 2;
    const int kOids[kNumHits] = { 3, 1 };
    const int kNumHsps[kNumHits] = { 2, 3 };
    const int kTotalHsps = 5;
    const int kScores[kTotalHsps] = { 7458, 6898, 7055, 6997, 6542};
    const int kLengths[kTotalHsps] = { 36, 21, 15, 15, 10};

    CRef<CSeq_loc> query_loc(new CSeq_loc());
    query_loc->SetWhole().SetGi(kQueryGi);
    CScope* query_scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
    query_scope->AddDefaults();
    TSeqLocVector query_v;
    query_v.push_back(SSeqLoc(query_loc, query_scope));
    CBlastSeqSrc seq_src(SeqDbBlastSeqSrcInit(m_DbName, TRUE));
    TestUtil::CheckForBlastSeqSrcErrors(seq_src);
    
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eRPSBlast));
    CRef<CBlastOptions> options(&opts->SetOptions());
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(query_v));
    CBlastPrelimSearch prelim_search(query_factory, options, seq_src);
    CRef<SInternalData> id(prelim_search.Run());

    CBlastHSPResults results
                  (prelim_search.ComputeBlastHSPResults
                               (id->m_HspStream->GetPointer()));

    BOOST_REQUIRE_EQUAL(1, results->num_queries);
    BOOST_REQUIRE(results->hitlist_array[0]);
    BOOST_REQUIRE_EQUAL(kNumHits, 
                         results->hitlist_array[0]->hsplist_count);

    int hsp_index = 0;
    for (int index = 0; index < kNumHits; ++index) {
        BlastHSPList* hsp_list = 
            results->hitlist_array[0]->hsplist_array[index];
        BOOST_REQUIRE_EQUAL(kOids[index], hsp_list->oid);
        BOOST_REQUIRE_EQUAL(kNumHsps[index], hsp_list->hspcnt); 
        for (int index1 = 0; index1 < kNumHsps[index]; 
             ++index1, ++hsp_index) {
            BlastHSP* hsp = hsp_list->hsp_array[index1];
            BOOST_REQUIRE_EQUAL(kScores[hsp_index], hsp->score);
            //BOOST_REQUIRE(hsp->evalue == 0.0);
            BOOST_REQUIRE_EQUAL(kLengths[hsp_index], 
                                 hsp->query.end - hsp->query.offset);
        }
    }
    BOOST_REQUIRE_EQUAL(kTotalHsps, hsp_index);
}

BOOST_AUTO_TEST_SUITE_END()
