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
 * Author:  Christiam Camacho, Greg Boratyn
 *
 */

/** @file delta_unit_test.cpp
 * Unit test module for deltablast.
 */
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

// Serial library includes
#include <serial/serial.hpp>
#include <serial/objistr.hpp>

// Object includes
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <objmgr/scope.hpp>

// PSSM includes
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>

// BLAST includes
#include <algo/blast/api/deltablast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/deltablast_options.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/local_db_adapter.hpp>

// SeqAlign comparison includes
#include "seqalign_cmp.hpp"
#include "seqalign_set_convert.hpp"

// Unit test auxiliary includes
#include "blast_test_util.hpp"
#include "test_objmgr.hpp"

/// Calculate the size of a static array
#define STATIC_ARRAY_SIZE(array) (sizeof(array)/sizeof(*array))

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

struct CDeltaBlastTestFixture {

    CRef<CDeltaBlastOptionsHandle> m_OptHandle;
    CRef<CSearchDatabase> m_SearchDb;
    CRef<CSearchDatabase> m_DomainDb;

    // This is needed only to get sequence for gi 129295 from asn1 file
    CRef<CPssmWithParameters> m_Pssm;

    /// Contains a single Bioseq
    CRef<CSeq_entry> m_SeqEntry;

    /// Contains a Bioseq-set with two Bioseqs, gi 7450545 and gi 129295
    CRef<CSeq_entry> m_SeqSet;

    /// Scope
    CRef<CScope> m_Scope;

    /// Seq-locs for creating instances of CObjMgr_QueryFactory
    vector< CRef<CSeq_loc> > m_Seq_locs;

    void x_ReadSeqEntriesFromFile() {

        const string kPssmFile("data/pssm_freq_ratios.asn");
        m_Pssm = TestUtil::ReadObject<CPssmWithParameters>(kPssmFile);
        BOOST_REQUIRE(m_Pssm->GetPssm().CanGetQuery());
        m_SeqEntry.Reset(&m_Pssm->SetPssm().SetQuery());
        BOOST_REQUIRE(m_SeqEntry->IsSeq());

        CRef<CSeq_id> id(
                    const_cast<CSeq_id*>(m_SeqEntry->SetSeq().GetFirstId()));
        CRef<CSeq_loc> seqloc(new CSeq_loc(CSeq_loc::e_Whole));
        seqloc->SetId(*id);
        m_Seq_locs.push_back(seqloc);
               

        const string kSeqEntryFile("data/7450545.seqentry.asn");
        CRef<CSeq_entry> seq_entry =
            TestUtil::ReadObject<CSeq_entry>(kSeqEntryFile);

        m_SeqSet.Reset(new CSeq_entry);
        m_SeqSet->SetSet().SetSeq_set().push_back(m_SeqEntry);
        m_SeqSet->SetSet().SetSeq_set().push_back(seq_entry);
        BOOST_REQUIRE(m_SeqEntry->IsSeq());

        id.Reset(const_cast<CSeq_id*>(seq_entry->SetSeq().GetFirstId()));
        seqloc.Reset(new CSeq_loc(CSeq_loc::e_Whole));
        seqloc->SetId(*id);
        m_Seq_locs.push_back(seqloc);
    }

    void x_InitScope() {
        m_Scope.Reset(new CScope(*CObjectManager::GetInstance()));
        m_Scope->AddTopLevelSeqEntry(*m_SeqSet);
    }

    CDeltaBlastTestFixture() {
        m_OptHandle.Reset(new CDeltaBlastOptionsHandle);
        BOOST_REQUIRE_EQUAL(eCompositionBasedStats,
                             m_OptHandle->GetCompositionBasedStats());
        m_SearchDb.Reset(new CSearchDatabase("data/seqp",
                                     CSearchDatabase::eBlastDbIsProtein));
        m_DomainDb.Reset(new CSearchDatabase("data/deltatest",
                                     CSearchDatabase::eBlastDbIsProtein));

        x_ReadSeqEntriesFromFile();
        x_InitScope();
    }

    ~CDeltaBlastTestFixture() {
        m_OptHandle.Reset();
        m_SearchDb.Reset();
        m_DomainDb.Reset();
        m_SeqEntry.Reset();
        m_SeqSet.Reset();
        m_Seq_locs.clear();
        m_Scope.Reset();
    }

    int x_CountNumberUniqueIds(CConstRef<CSeq_align_set> sas)
    {
        int num_ids = 0;
        int last_id = -1;
        ITERATE(CSeq_align_set::Tdata, itr, sas->Get()){
            const CSeq_id& seqid = (*itr)->GetSeq_id(1);

            BOOST_REQUIRE(seqid.IsGi() || seqid.IsGeneral());

            if (seqid.IsGi()) {
                int new_gi = seqid.GetGi();
            
                if (new_gi != last_id) {
                        num_ids++;
                        last_id = new_gi;
                }
            }
            else {
                BOOST_REQUIRE(seqid.GetGeneral().IsSetTag());
                int new_tag = seqid.GetGeneral().GetTag().GetId();

                if (new_tag != last_id) {
                    num_ids++;
                    last_id = new_tag;
                }
            }
            
        }
        return num_ids;
    }
};


BOOST_FIXTURE_TEST_SUITE(deltablast, CDeltaBlastTestFixture)

BOOST_AUTO_TEST_CASE(TestSingleQuery_CBS)
{
    TSeqLocVector query;
    query.push_back(SSeqLoc(*m_Seq_locs.front(), *m_Scope));
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(query));


    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CRef<CLocalDbAdapter> domain_dbadapter(new CLocalDbAdapter(*m_DomainDb));
    CDeltaBlast deltablast(query_factory, dbadapter, domain_dbadapter,
                           m_OptHandle);

    CSearchResultSet results(*deltablast.Run());


    // check intermediate results: alignments with domains
    CRef<CSearchResultSet> domain_results = deltablast.GetDomainResults();
    BOOST_REQUIRE((*domain_results)[0].GetErrors().empty());

    const int kNumExpectedMatchingDomains = 3;
    CConstRef<CSeq_align_set> domain_sas = (*domain_results)[0].GetSeqAlign();

    BOOST_REQUIRE_EQUAL(kNumExpectedMatchingDomains,
                        x_CountNumberUniqueIds(domain_sas));

    const size_t kNumExpectedDomainHSPs = 3;
    qa::TSeqAlignSet expected_domain_results(kNumExpectedDomainHSPs);

    // HSP # 1
    { 
        expected_domain_results[0].score = 728;
        expected_domain_results[0].evalue = 2.45396e-98;
        expected_domain_results[0].bit_score = 2841082571e-7;
        expected_domain_results[0].num_ident = 111;
        int starts[] = {1, 139, 80, -1, 81, 218, 162, -1, 163, 299};
        int lengths[] = {79, 1, 81, 1, 69};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_domain_results[0].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_domain_results[0].lengths));
    }

    // HSP # 2
    { 
        expected_domain_results[1].score = 698;
        expected_domain_results[1].evalue = 7.61355e-94;
        expected_domain_results[1].bit_score = 2725169055e-7;
        expected_domain_results[1].num_ident = 107;
        int starts[] = {1, 135, 6, -1, 8, 140, -1, 190, 58, 191, 80, -1, 81,
                        213, 94, -1, 95, 226, 114, -1, 116, 245, 200, -1,
                        202, 329};
        int lengths[] = {5, 2, 50, 1, 22, 1, 13, 1, 19, 2, 84, 2, 30};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_domain_results[1].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_domain_results[1].lengths));
    }

    // HSP # 3
    { 
        expected_domain_results[2].score = 661;
        expected_domain_results[2].evalue = 2.92318e-88;
        expected_domain_results[2].bit_score = 2583366987e-7;
        expected_domain_results[2].num_ident = 106;
        int starts[] = {0, 137, 115, -1, 117, 252 };
        int lengths[] = {115, 2, 112};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_domain_results[2].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_domain_results[2].lengths));
    }


    qa::TSeqAlignSet actual_domain_results;
    qa::SeqAlignSetConvert(*domain_sas, actual_domain_results);

    qa::CSeqAlignCmpOpts opts;
    qa::CSeqAlignCmp cmp_d(expected_domain_results, actual_domain_results, opts);
    string errors;
    bool identical_results = cmp_d.Run(&errors);

    BOOST_REQUIRE_MESSAGE(identical_results, errors);

    // check PSSM

    CRef<CPssmWithParameters> pssm = deltablast.GetPssm();
    BOOST_REQUIRE(pssm->HasQuery());
    BOOST_REQUIRE(pssm->GetQuery().GetSeq().IsSetInst());
    BOOST_REQUIRE_EQUAL(pssm->GetQuery().GetSeq().GetFirstId()->GetGi(),
                        129295);

    // check alignments from sequence search results

    BOOST_REQUIRE(results[0].GetErrors().empty());

    BOOST_REQUIRE_EQUAL(
                results[0].GetSeqAlign()->Get().front()->GetSeq_id(0).GetGi(),
                129295);

    const int kNumExpectedMatchingSeqs = 7;
    CConstRef<CSeq_align_set> sas = results[0].GetSeqAlign();

    BOOST_REQUIRE_EQUAL(kNumExpectedMatchingSeqs, x_CountNumberUniqueIds(sas));

    const size_t kNumExpectedHSPs = 8;
    qa::TSeqAlignSet expected_results(kNumExpectedHSPs);

    // HSP # 1
    { 
        expected_results[0].score = 865;
        expected_results[0].evalue = 1.41573e-108;
        expected_results[0].bit_score = 3371923606e-7;
        expected_results[0].num_ident = 101; 
        int starts[] = {0, 941, -1, 1094, 153, 1095};
        int lengths[] = {153, 1, 79};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[0].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[0].lengths));
    }

    // HSP # 2
    {
        expected_results[1].score = 638;
        expected_results[1].evalue = 3.93488e-77;
        expected_results[1].bit_score = 2497520568e-7;
        expected_results[1].num_ident = 73; 
        int starts[] = {0, 154, -1, 307, 153, 308};
        int lengths[] = {153, 1, 25};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[1].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[1].lengths));
    }

    // HSP # 3
    {
        expected_results[2].score = 650;
        expected_results[2].evalue = 2.54216e-83;
        expected_results[2].bit_score = 2543744518e-7;
        expected_results[2].num_ident = 80; 
        int starts[] = {0, 190, 68, -1, 70, 258, 92, -1, 93, 280, 118, -1, 
                        119, 305, 151, -1, 152, 337, 161, -1, 162, 346, -1,
                        367, 183, 371};
        int lengths[] = {68, 2, 22, 1, 25, 1, 32, 1, 9, 1, 21, 4, 49};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[2].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[2].lengths));
    }

    // HSP #4
    {
        expected_results[3].score = 53;
        expected_results[3].evalue = 3.74227;
        expected_results[3].bit_score = 244103049e-7;
        expected_results[3].num_ident = 7; 
        int starts[] = {127, 104, 132, -1, 134, 109};
        int lengths[] = {5, 2, 15};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[3].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[3].lengths));
    }


    // HSP # 5
    {
        expected_results[4].score = 52;
        expected_results[4].evalue = 4.67139;
        expected_results[4].bit_score = 240251054e-7;
        expected_results[4].num_ident = 5; 
        int starts[] = {137, 20, 151, -1, 156, 34};
        int lengths[] = {14, 5, 17};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[4].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[4].lengths));
    }

    // HSP # 6
    {
        expected_results[5].score = 50;
        expected_results[5].evalue = 6.76621;
        expected_results[5].bit_score = 2325470625e-8;
        expected_results[5].num_ident = 6; 
        int starts[] = {159, 0, 173, -1, 178, 14};
        int lengths[] = {14, 5, 11};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[5].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[5].lengths));
    }

    // HSP # 7
    {
        expected_results[6].score = 50;
        expected_results[6].evalue = 7.18326;
        expected_results[6].bit_score = 2325470625e-8;
        expected_results[6].num_ident = 7; 
        int starts[] = {172, 305, 179, -1, 182, 312};
        int lengths[] = {7, 3, 33};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[6].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[6].lengths));
    }


    // HSP # 8
    {
        expected_results[7].score = 50;
        expected_results[7].evalue = 7.87994;
        expected_results[7].bit_score = 2325470625e-8;
        expected_results[7].num_ident = 8; 
        int starts[] = {153, 102, -1, 122, 173, 127};
        int lengths[] = {20, 5, 12};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[7].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[7].lengths));
    }


    qa::TSeqAlignSet actual_results;
    qa::SeqAlignSetConvert(*sas, actual_results);

    qa::CSeqAlignCmp cmp(expected_results, actual_results, opts);
    identical_results = cmp.Run(&errors);

    BOOST_REQUIRE_MESSAGE(identical_results, errors);
}


BOOST_AUTO_TEST_CASE(TestSingleQuery_NoCBS)
{
    m_OptHandle->SetCompositionBasedStats(eNoCompositionBasedStats);
    m_OptHandle->SetEvalueThreshold(5);

    TSeqLocVector query;
    query.push_back(SSeqLoc(*m_Seq_locs.front(), *m_Scope));
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(query));       

    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CRef<CLocalDbAdapter> domain_dbadapter(new CLocalDbAdapter(*m_DomainDb));
    CDeltaBlast deltablast(query_factory, dbadapter, domain_dbadapter,
                           m_OptHandle);

    CSearchResultSet results(*deltablast.Run());


    // check intermediate results: alignments with domains
    CRef<CSearchResultSet> domain_results = deltablast.GetDomainResults();
    BOOST_REQUIRE((*domain_results)[0].GetErrors().empty());

    const int kNumExpectedMatchingDomains = 3;
    CConstRef<CSeq_align_set> domain_sas = (*domain_results)[0].GetSeqAlign();

    BOOST_REQUIRE_EQUAL(kNumExpectedMatchingDomains,
                        x_CountNumberUniqueIds(domain_sas));

    const size_t kNumExpectedDomainHSPs = 3;
    qa::TSeqAlignSet expected_domain_results(kNumExpectedDomainHSPs);

    // HSP # 1
    { 
        expected_domain_results[0].score = 728;
        expected_domain_results[0].evalue = 2.45396e-98;
        expected_domain_results[0].bit_score = 2841082571e-7;
        expected_domain_results[0].num_ident = 111;
        int starts[] = {1, 139, 80, -1, 81, 218, 162, -1, 163, 299};
        int lengths[] = {79, 1, 81, 1, 69};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_domain_results[0].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_domain_results[0].lengths));
    }

    // HSP # 2
    { 
        expected_domain_results[1].score = 698;
        expected_domain_results[1].evalue = 7.61355e-94;
        expected_domain_results[1].bit_score = 2725169055e-7;
        expected_domain_results[1].num_ident = 107;
        int starts[] = {1, 135, 6, -1, 8, 140, -1, 190, 58, 191, 80, -1, 81,
                        213, 94, -1, 95, 226, 114, -1, 116, 245, 200, -1,
                        202, 329};
        int lengths[] = {5, 2, 50, 1, 22, 1, 13, 1, 19, 2, 84, 2, 30};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_domain_results[1].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_domain_results[1].lengths));
    }

    // HSP # 3
    { 
        expected_domain_results[2].score = 661;
        expected_domain_results[2].evalue = 2.92318e-88;
        expected_domain_results[2].bit_score = 2583366987e-7;
        expected_domain_results[2].num_ident = 106;
        int starts[] = {0, 137, 115, -1, 117, 252 };
        int lengths[] = {115, 2, 112};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_domain_results[2].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_domain_results[2].lengths));
    }


    qa::TSeqAlignSet actual_domain_results;
    qa::SeqAlignSetConvert(*domain_sas, actual_domain_results);

    qa::CSeqAlignCmpOpts opts;
    qa::CSeqAlignCmp cmp_d(expected_domain_results, actual_domain_results, opts);
    string errors;
    bool identical_results = cmp_d.Run(&errors);

    BOOST_REQUIRE_MESSAGE(identical_results, errors);

    // check PSSM

    CRef<CPssmWithParameters> pssm = deltablast.GetPssm();
    BOOST_REQUIRE(pssm->HasQuery());
    BOOST_REQUIRE(pssm->GetQuery().GetSeq().IsSetInst());
    BOOST_REQUIRE_EQUAL(pssm->GetQuery().GetSeq().GetFirstId()->GetGi(),
                        129295);

    // check alignments from sequence search results

    BOOST_REQUIRE(results[0].GetErrors().empty());

    BOOST_REQUIRE_EQUAL(
                results[0].GetSeqAlign()->Get().front()->GetSeq_id(0).GetGi(),
                129295);

    const int kNumExpectedMatchingSeqs = 5;
    CConstRef<CSeq_align_set> sas = results[0].GetSeqAlign();

    BOOST_REQUIRE_EQUAL(kNumExpectedMatchingSeqs, x_CountNumberUniqueIds(sas));

    const size_t kNumExpectedHSPs = 6;
    qa::TSeqAlignSet expected_results(kNumExpectedHSPs);

    // HSP # 1
    { 
        expected_results[0].score = 876;
        expected_results[0].evalue = 3.83527e-110;
        expected_results[0].bit_score = 3414303038e-7;
        expected_results[0].num_ident = 101;
        int starts[] = {0, 941, -1, 1094, 153, 1095};
        int lengths[] = {153, 1, 79};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[0].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[0].lengths));
    }


    // HSP # 2
    {
        expected_results[1].score = 642;
        expected_results[1].evalue = 1.02795e-77;
        expected_results[1].bit_score = 2512936031e-7;
        expected_results[1].num_ident = 73;
        int starts[] = {0, 154, -1, 307, 153, 308};
        int lengths[] = {153, 1, 25};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[1].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[1].lengths));
    }

    // HSP # 3
    {
        expected_results[2].score = 736;
        expected_results[2].evalue = 3.93428e-96;
        expected_results[2].bit_score = 2875023632e-7;
        expected_results[2].num_ident = 83;
        int starts[] = {0, 190, 68, -1, 70, 258, 92, -1, 93, 280, 118, -1,
                        119, 305, 151, -1, 152, 337, 161, -1, 162, 346, -1,
                        374, 190, 378};
        int lengths[] = {68, 2, 22, 1, 25, 1, 32, 1, 9, 1, 28, 4, 42};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[2].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[2].lengths));
    }

    // HSP # 4
    {
        expected_results[3].score = 53;
        expected_results[3].evalue = 3.45771;
        expected_results[3].bit_score = 2441105291e-8;
        expected_results[3].num_ident = 4;
        int starts[] = {139, 22, 151, -1, 156, 34};
        int lengths[] = {12, 5, 17};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[3].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[3].lengths));
    }


    // HSP # 5
    {
        expected_results[4].score = 52;
        expected_results[4].evalue = 4.61874;
        expected_results[4].bit_score = 2402585333e-8;
        expected_results[4].num_ident = 7;
        int starts[] = {172, 305, 179, -1, 182, 312};
        int lengths[] = {7, 3, 33};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[4].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[4].lengths));
    }

    // HSP # 6
    {
        expected_results[5].score = 52;
        expected_results[5].evalue = 4.62283;
        expected_results[5].bit_score = 2402585333e-8;
        expected_results[5].num_ident = 7;
        int starts[] = {127, 104, 132, -1, 134, 109};
        int lengths[] = {5, 2, 15};
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[5].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[5].lengths));
    }

    qa::TSeqAlignSet actual_results;
    qa::SeqAlignSetConvert(*sas, actual_results);

    qa::CSeqAlignCmp cmp(expected_results, actual_results, opts);
    identical_results = cmp.Run(&errors);

    BOOST_REQUIRE_MESSAGE(identical_results, errors);
}


BOOST_AUTO_TEST_CASE(TestMultipleQueries)
{
    TSeqLocVector queries;
    ITERATE (vector< CRef<CSeq_loc> >, it, m_Seq_locs) {
        queries.push_back(SSeqLoc(**it, *m_Scope));
    }
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(queries));

    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CRef<CLocalDbAdapter> domain_dbadapter(new CLocalDbAdapter(*m_DomainDb));
    CDeltaBlast deltablast(query_factory, dbadapter, domain_dbadapter,
                           m_OptHandle);

    CSearchResultSet results(*deltablast.Run());

    // check results
    BOOST_REQUIRE(results[0].GetErrors().empty());
    BOOST_REQUIRE(results[1].GetErrors().empty());

    // verify query id in Seq_aligns
    BOOST_REQUIRE_EQUAL(
                 results[0].GetSeqAlign()->Get().front()->GetSeq_id(0).GetGi(),
                 129295);

    BOOST_REQUIRE_EQUAL(
     results[1].GetSeqAlign()->Get().front()->GetSeq_id(0).GetPir().GetName(),
     "H70430");


    // verify query id in PSSMs
    BOOST_REQUIRE_EQUAL(
           deltablast.GetPssm(0)->GetQuery().GetSeq().GetFirstId()->GetGi(),
           129295);
    
    BOOST_REQUIRE_EQUAL(
     deltablast.GetPssm(1)->GetQuery().GetSeq().GetFirstId()->GetPir().GetName(),
     "H70430");
}

// Verify that null inputs result in exceptions
BOOST_AUTO_TEST_CASE(TestNullQuery)
{
    CRef<IQueryFactory> query_factory;
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CRef<CLocalDbAdapter> domain_dbadapter(new CLocalDbAdapter(*m_DomainDb));
    BOOST_REQUIRE_THROW(CDeltaBlast deltablast(query_factory, dbadapter,
                                               domain_dbadapter, m_OptHandle),
                        CBlastException);    
}


BOOST_AUTO_TEST_CASE(TestNullOptions)
{
    m_OptHandle.Reset();
    TSeqLocVector query;
    query.push_back(SSeqLoc(*m_Seq_locs.front(), *m_Scope));
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(query));
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CRef<CLocalDbAdapter> domain_dbadapter(new CLocalDbAdapter(*m_DomainDb));
    BOOST_REQUIRE_THROW(CDeltaBlast deltablast(query_factory, dbadapter,
                                               domain_dbadapter, m_OptHandle),
                        CBlastException);    
}


BOOST_AUTO_TEST_CASE(TestNullDatabase)
{
    TSeqLocVector query;
    query.push_back(SSeqLoc(*m_Seq_locs.front(), *m_Scope));
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(query));
    CRef<CLocalDbAdapter> dbadapter;
    CRef<CLocalDbAdapter> domain_dbadapter(new CLocalDbAdapter(*m_DomainDb));
    BOOST_REQUIRE_THROW(CDeltaBlast deltablast(query_factory, dbadapter,
                                               domain_dbadapter, m_OptHandle),
                        CBlastException);    
}


BOOST_AUTO_TEST_CASE(TestNullDomainDatabase)
{
    TSeqLocVector query;
    query.push_back(SSeqLoc(*m_Seq_locs.front(), *m_Scope));
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(query));
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CRef<CLocalDbAdapter> domain_dbadapter;
    BOOST_REQUIRE_THROW(CDeltaBlast deltablast(query_factory, dbadapter,
                                               domain_dbadapter, m_OptHandle),
                        CBlastException);    
}


BOOST_AUTO_TEST_SUITE_END()

