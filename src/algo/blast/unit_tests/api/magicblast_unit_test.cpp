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
 * Author:  Greg Boratyn
 *
 * Test module for Magic-BLAST API
 */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

// Serial library includes
#include <serial/serial.hpp>
#include <serial/objistr.hpp>

// Object includes
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Splice_site.hpp>
#include <objects/seqalign/Product_pos.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>

#include <objects/general/User_object.hpp>

// BLAST includes
#include <algo/blast/api/magicblast.hpp>
#include <algo/blast/api/objmgrfree_query_data.hpp>
#include <algo/blast/api/magicblast_options.hpp>
#include <algo/blast/api/local_db_adapter.hpp>

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

struct CMagicBlastTestFixture {

    CRef<CMagicBlastOptionsHandle> m_OptHandle;
    CRef<CSearchDatabase> m_Db;

    /// Contains a single Bioseq
    CRef<CBioseq_set> m_Queries;


    CMagicBlastTestFixture() {
        m_OptHandle.Reset(new CMagicBlastOptionsHandle);
        m_Db.Reset(new CSearchDatabase("data/pombe",
                                       CSearchDatabase::eBlastDbIsNucleotide));

        m_Queries.Reset(new CBioseq_set);
    }


    ~CMagicBlastTestFixture() {
        m_OptHandle.Reset();
        m_Db.Reset();
        m_Queries.Reset();
    }
};


BOOST_FIXTURE_TEST_SUITE(magicblast, CMagicBlastTestFixture)


struct SExon
{
    unsigned int prod_start;
    unsigned int prod_end;
    unsigned int gen_start;
    unsigned int gen_end;

    ENa_strand prod_strand;
    ENa_strand gen_strand;
    string acceptor;
    string donor;
};

struct SMatch
{
    int score;
    unsigned int prod_length;
    vector<SExon> exons;
};


BOOST_AUTO_TEST_CASE(MappingNonMatch)
{
    ifstream istr("data/magicblast_nonmatch.asn");
    BOOST_REQUIRE(istr);
    istr >> MSerial_AsnText >> *m_Queries;

    CRef<IQueryFactory> query_factory(new CObjMgrFree_QueryFactory(m_Queries));
    CRef<CLocalDbAdapter> db_adapter(new CLocalDbAdapter(*m_Db));
    m_OptHandle->SetMismatchPenalty(-8);
    m_OptHandle->SetGapExtensionCost(8);
    m_OptHandle->SetPaired(true);
    CMagicBlast magicblast(query_factory, db_adapter, m_OptHandle);
    CRef<CMagicBlastResultSet> results = magicblast.RunEx();

    const size_t kExpectedNumResults = 1;
    BOOST_REQUIRE_EQUAL(results->size(), kExpectedNumResults);
}


BOOST_AUTO_TEST_CASE(MappingAllConcordant)
{
    ifstream istr("data/magicblast_concordant.asn");
    BOOST_REQUIRE(istr);
    istr >> MSerial_AsnText >> *m_Queries;

    CRef<IQueryFactory> query_factory(new CObjMgrFree_QueryFactory(m_Queries));
    CRef<CLocalDbAdapter> db_adapter(new CLocalDbAdapter(*m_Db));
    m_OptHandle->SetPaired(true);
    CMagicBlast magicblast(query_factory, db_adapter, m_OptHandle);
    CRef<CMagicBlastResultSet> results = magicblast.RunEx();
    
    const size_t kExpectedNumResults = 3;
    const size_t kExpectedConcordant = 3;
    BOOST_REQUIRE_EQUAL(results->size(), kExpectedNumResults);
    size_t count = 0;
    for (auto r = results->begin(); r != results->end(); ++r) {
        CRef<CMagicBlastResults> re = *r;
        if (re->IsConcordant()) ++count;
    }
    BOOST_REQUIRE_EQUAL(count, kExpectedConcordant);
}


BOOST_AUTO_TEST_CASE(MappingAllDiscordant)
{
    ifstream istr("data/magicblast_discordant.asn");
    BOOST_REQUIRE(istr);
    istr >> MSerial_AsnText >> *m_Queries;

    CRef<IQueryFactory> query_factory(new CObjMgrFree_QueryFactory(m_Queries));
    CRef<CLocalDbAdapter> db_adapter(new CLocalDbAdapter(*m_Db));
    m_OptHandle->SetPaired(true);
    CMagicBlast magicblast(query_factory, db_adapter, m_OptHandle);
    CRef<CMagicBlastResultSet> results = magicblast.RunEx();
    
    const size_t kExpectedNumResults = 4;
    const size_t kExpectedDiscordant = 4;
    BOOST_REQUIRE_EQUAL(results->size(), kExpectedNumResults);
    size_t count = 0;
    for (auto r = results->begin(); r != results->end(); ++r) {
        CRef<CMagicBlastResults> re = *r;
        if (!re->IsConcordant()) ++count;
    }
    BOOST_REQUIRE_EQUAL(count, kExpectedDiscordant);
}


BOOST_AUTO_TEST_CASE(MappingNoPairs)
{
    ifstream istr("data/magicblast_queries.asn");
    BOOST_REQUIRE(istr);
    istr >> MSerial_AsnText >> *m_Queries;

    CRef<IQueryFactory> query_factory(new CObjMgrFree_QueryFactory(m_Queries));
    CRef<CLocalDbAdapter> db_adapter(new CLocalDbAdapter(*m_Db));
    m_OptHandle->SetMismatchPenalty(-8);
    m_OptHandle->SetGapExtensionCost(8);
    m_OptHandle->SetCutoffScore(49);
    CMagicBlast magicblast(query_factory, db_adapter, m_OptHandle);
    CRef<CSeq_align_set> results = magicblast.Run();

    const size_t kExpectedNumResults = 4;
    BOOST_REQUIRE_EQUAL(results->Get().size(), kExpectedNumResults);

    SExon exon;
    vector<SMatch> expected_hits(kExpectedNumResults);

    // expected HSPs

    int results_idx = 0;
    // HSP #1
    expected_hits[results_idx].score = 49;
    expected_hits[results_idx].prod_length = 49;

    exon.prod_start = 0;
    exon.prod_end = 21;
    exon.gen_start = 1827220;
    exon.gen_end = 1827241;
    exon.prod_strand = eNa_strand_minus;
    exon.gen_strand = eNa_strand_plus;
    exon.acceptor = "";
    exon.donor = "CT";
    expected_hits[results_idx].exons.push_back(exon);

    exon.prod_start = 22;
    exon.prod_end = 48;
    exon.gen_start = 1827292;
    exon.gen_end = 1827318;
    exon.prod_strand = eNa_strand_minus;
    exon.gen_strand = eNa_strand_plus;
    exon.acceptor = "AC";
    exon.donor = "";
    expected_hits[results_idx].exons.push_back(exon);

    // HSP #2
    results_idx++;
    expected_hits[results_idx].score = 49;
    expected_hits[results_idx].prod_length = 49;

    exon.prod_start = 0;
    exon.prod_end = 28;
    exon.gen_start = 181290;
    exon.gen_end = 181318;
    exon.prod_strand = eNa_strand_minus;
    exon.gen_strand = eNa_strand_plus;
    exon.acceptor = "";
    exon.donor = "CT";
    expected_hits[results_idx].exons.push_back(exon);

    exon.prod_start = 29;
    exon.prod_end = 48;
    exon.gen_start = 181367;
    exon.gen_end = 181386;
    exon.prod_strand = eNa_strand_minus;
    exon.gen_strand = eNa_strand_plus;
    exon.acceptor = "AC";
    exon.donor = "";
    expected_hits[results_idx].exons.push_back(exon);

    // HSP #3
    results_idx++;
    expected_hits[results_idx].score = 49;
    expected_hits[results_idx].prod_length = 49;

    exon.prod_start = 0;
    exon.prod_end = 20;
    exon.gen_start = 1033352;
    exon.gen_end = 1033372;
    exon.prod_strand = eNa_strand_minus;
    exon.gen_strand = eNa_strand_plus;
    exon.acceptor = "";
    exon.donor = "CT";
    expected_hits[results_idx].exons.push_back(exon);

    exon.prod_start = 21;
    exon.prod_end = 48;
    exon.gen_start = 1033432;
    exon.gen_end = 1033459;
    exon.prod_strand = eNa_strand_minus;
    exon.gen_strand = eNa_strand_plus;
    exon.acceptor = "AC";
    exon.donor = "";
    expected_hits[results_idx].exons.push_back(exon);

    // HSP #4
    results_idx++;
    expected_hits[results_idx].score = 49;
    expected_hits[results_idx].prod_length = 49;

    exon.prod_start = 0;
    exon.prod_end = 23;
    exon.gen_start = 89112;
    exon.gen_end = 89135;
    exon.prod_strand = eNa_strand_minus;
    exon.gen_strand = eNa_strand_plus;
    exon.acceptor = "";
    exon.donor = "CT";
    expected_hits[results_idx].exons.push_back(exon);

    exon.prod_start = 24;
    exon.prod_end = 48;
    exon.gen_start = 89420;
    exon.gen_end = 89444;
    exon.prod_strand = eNa_strand_minus;
    exon.gen_strand = eNa_strand_plus;
    exon.acceptor = "AC";
    exon.donor = "";
    expected_hits[results_idx].exons.push_back(exon);

    // compare computed HSPs with the expected ones
    results_idx = 0;
    for (auto it: results->Get()) {

        // we do not expect paired results
        BOOST_REQUIRE(it->GetSegs().IsSpliced());

        int score;
        it->GetNamedScore("score", score);
        BOOST_REQUIRE_EQUAL(score, expected_hits[results_idx].score);

        const CSpliced_seg& seg = it->GetSegs().GetSpliced();
        BOOST_REQUIRE_EQUAL(seg.GetProduct_length(),
                            expected_hits[results_idx].prod_length);

        BOOST_REQUIRE_EQUAL(seg.GetExons().size(),
                            expected_hits[results_idx].exons.size());

        // compare exon data
        auto expected_exon = expected_hits[results_idx].exons.begin();
        for (auto exon: seg.GetExons()) {

            // exon starts and stops
            BOOST_REQUIRE_EQUAL(exon->GetProduct_start().GetNucpos(),
                                expected_exon->prod_start);

            BOOST_REQUIRE_EQUAL(exon->GetProduct_end().GetNucpos(),
                                expected_exon->prod_end);

            BOOST_REQUIRE_EQUAL(exon->GetGenomic_start(),
                                expected_exon->gen_start);

            BOOST_REQUIRE_EQUAL(exon->GetGenomic_end(),
                                expected_exon->gen_end);

            // strands
            BOOST_REQUIRE_EQUAL(exon->GetProduct_strand(),
                                expected_exon->prod_strand);

            BOOST_REQUIRE_EQUAL(exon->GetGenomic_strand(),
                                expected_exon->gen_strand);

            // splice signals
            if (!expected_exon->acceptor.empty()) {
                BOOST_REQUIRE(exon->CanGetAcceptor_before_exon());
                BOOST_REQUIRE_EQUAL(exon->GetAcceptor_before_exon().GetBases(),
                                    expected_exon->acceptor);
            }

            if (!expected_exon->donor.empty()) {
                BOOST_REQUIRE(exon->CanGetDonor_after_exon());
                BOOST_REQUIRE_EQUAL(exon->GetDonor_after_exon().GetBases(),
                                    expected_exon->donor);
            }

            ++expected_exon;
        }
        results_idx++;
    }
}


BOOST_AUTO_TEST_CASE(MappingPaired)
{
    ifstream istr("data/magicblast_paired.asn");
    BOOST_REQUIRE(istr);
    istr >> MSerial_AsnText >> *m_Queries;

    bool queries_paired = false;
    auto q = m_Queries->GetSeq_set().begin();
    BOOST_REQUIRE(q != m_Queries->GetSeq_set().end());
    const CBioseq& bioseq = (*q)->GetSeq();
    BOOST_REQUIRE(bioseq.CanGetDescr());
    for (auto it: bioseq.GetDescr().Get()) {
        if (it->IsUser()) {
            const CUser_object& obj = it->GetUser();
            if (obj.GetType().IsStr() && obj.GetType().GetStr() == "Mapping") {
                queries_paired = obj.HasField("has_pair");
            }
        }
    }
    BOOST_REQUIRE(queries_paired);

    CRef<IQueryFactory> query_factory(new CObjMgrFree_QueryFactory(m_Queries));
    CRef<CLocalDbAdapter> db_adapter(new CLocalDbAdapter(*m_Db));

    m_OptHandle->SetPaired(true);
    CMagicBlast magicblast(query_factory, db_adapter, m_OptHandle);
    CRef<CSeq_align_set> results = magicblast.Run();

    const size_t kExpectedNumResults = 3;
    BOOST_REQUIRE_EQUAL(results->Get().size(), kExpectedNumResults);

    SExon exon;
    vector<SMatch> expected_hits(2 * kExpectedNumResults);

    // expected HSPs

    int results_idx = 0;

    // HSP #1
    expected_hits[results_idx].score = 68;
    expected_hits[results_idx].prod_length = 75;

    exon.prod_start = 0;
    exon.prod_end = 67;
    exon.gen_start = 9925;
    exon.gen_end = 9992;
    exon.prod_strand = eNa_strand_minus;
    exon.gen_strand = eNa_strand_plus;
    exon.acceptor = "";
    exon.donor = "";
    expected_hits[results_idx].exons.push_back(exon);

    // HSP #2
    results_idx++;
    expected_hits[results_idx].score = 74;
    expected_hits[results_idx].prod_length = 75;

    exon.prod_start = 1;
    exon.prod_end = 74;
    exon.gen_start = 9842;
    exon.gen_end = 9915;
    exon.prod_strand = eNa_strand_plus;
    exon.gen_strand = eNa_strand_plus;
    exon.acceptor = "";
    exon.donor = "";
    expected_hits[results_idx].exons.push_back(exon);

    // HSP #3
    results_idx++;
    expected_hits[results_idx].score = 68;
    expected_hits[results_idx].prod_length = 75;

    exon.prod_start = 0;
    exon.prod_end = 67;
    exon.gen_start = 20795;
    exon.gen_end = 20862;
    exon.prod_strand = eNa_strand_minus;
    exon.gen_strand = eNa_strand_plus;
    exon.acceptor = "";
    exon.donor = "";
    expected_hits[results_idx].exons.push_back(exon);

    // HSP #4
    results_idx++;
    expected_hits[results_idx].score = 74;
    expected_hits[results_idx].prod_length = 75;

    exon.prod_start = 1;
    exon.prod_end = 74;
    exon.gen_start = 20712;
    exon.gen_end = 20785;
    exon.prod_strand = eNa_strand_plus;
    exon.gen_strand = eNa_strand_plus;
    exon.acceptor = "";
    exon.donor = "";
    expected_hits[results_idx].exons.push_back(exon);

    // HSP #5
    results_idx++;
    expected_hits[results_idx].score = 68;
    expected_hits[results_idx].prod_length = 75;

    exon.prod_start = 7;
    exon.prod_end = 74;
    exon.gen_start = 2443260;
    exon.gen_end = 2443327;
    exon.prod_strand = eNa_strand_plus;
    exon.gen_strand = eNa_strand_plus;
    exon.acceptor = "";
    exon.donor = "";
    expected_hits[results_idx].exons.push_back(exon);

    // HSP #6
    results_idx++;
    expected_hits[results_idx].score = 74;
    expected_hits[results_idx].prod_length = 75;

    exon.prod_start = 0;
    exon.prod_end = 73;
    exon.gen_start = 2443337;
    exon.gen_end = 2443410;
    exon.prod_strand = eNa_strand_minus;
    exon.gen_strand = eNa_strand_plus;
    exon.acceptor = "";
    exon.donor = "";
    expected_hits[results_idx].exons.push_back(exon);

    // compare computed HSPs with the expected ones
    results_idx = 0;
    for (auto seg: results->Get()) {

        // we do not expect paired results
        BOOST_REQUIRE(seg->GetSegs().IsDisc());
        BOOST_REQUIRE_EQUAL(seg->GetSegs().GetDisc().Get().size(), 2u);

        for (auto it: seg->GetSegs().GetDisc().Get()) {

            BOOST_REQUIRE(it->GetSegs().IsSpliced());

            int score;
            it->GetNamedScore("score", score);
            BOOST_REQUIRE_EQUAL(score, expected_hits[results_idx].score);


            const CSpliced_seg& seg = it->GetSegs().GetSpliced();
            BOOST_REQUIRE_EQUAL(seg.GetProduct_length(),
                                expected_hits[results_idx].prod_length);

            BOOST_REQUIRE_EQUAL(seg.GetExons().size(),
                                expected_hits[results_idx].exons.size());

            // compare exon data
            auto expected_exon = expected_hits[results_idx].exons.begin();
            for (auto exon: seg.GetExons()) {

                // exon starts and stops
                BOOST_REQUIRE_EQUAL(exon->GetProduct_start().GetNucpos(),
                                    expected_exon->prod_start);

                BOOST_REQUIRE_EQUAL(exon->GetProduct_end().GetNucpos(),
                                    expected_exon->prod_end);

                BOOST_REQUIRE_EQUAL(exon->GetGenomic_start(),
                                    expected_exon->gen_start);

                BOOST_REQUIRE_EQUAL(exon->GetGenomic_end(),
                                    expected_exon->gen_end);

                // strands
                BOOST_REQUIRE_EQUAL(exon->GetProduct_strand(),
                                expected_exon->prod_strand);

                BOOST_REQUIRE_EQUAL(exon->GetGenomic_strand(),
                                    expected_exon->gen_strand);

                // splice signals
                if (!expected_exon->acceptor.empty()) {
                    BOOST_REQUIRE(exon->CanGetAcceptor_before_exon());
                    BOOST_REQUIRE_EQUAL(
                                 exon->GetAcceptor_before_exon().GetBases(),
                                 expected_exon->acceptor);
                }

                if (!expected_exon->donor.empty()) {
                    BOOST_REQUIRE(exon->CanGetDonor_after_exon());
                    BOOST_REQUIRE_EQUAL(exon->GetDonor_after_exon().GetBases(),
                                        expected_exon->donor);
                }

                ++expected_exon;
            }
            results_idx++;
        }
    }
}


BOOST_AUTO_TEST_SUITE_END()

