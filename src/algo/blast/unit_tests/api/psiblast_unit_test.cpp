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
 */

/** @file psiblast-cppunit.cpp
 * Unit test module for the PSI-BLAST class
 */
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <serial/iterator.hpp>

// BLAST API includes
#include <algo/blast/api/psiblast.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>

#include <algo/blast/blastinput/blast_scope_src.hpp>

#include <objtools/blast/seqdb_reader/seqdb.hpp>

// Pssm engine includes
#include <algo/blast/api/pssm_engine.hpp>
#include <algo/blast/api/psi_pssm_input.hpp>

// Object includes
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/scoremat/PssmIntermediateData.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include "psiblast_aux_priv.hpp"        // for PsiBlastComputePssmScores

// Auxiliary test includes
#include "blast_test_util.hpp"
//#include "psiblast_test_util.hpp"

// SeqAlign comparison includes
#include "seqalign_cmp.hpp"
#include "seqalign_set_convert.hpp"

#include <algo/blast/api/psiblast_iteration.hpp>
#include "bioseq_extract_data_priv.hpp"
#include "blast_objmgr_priv.hpp"

/// Calculate the size of a static array
#define STATIC_ARRAY_SIZE(array) (sizeof(array)/sizeof(*array))

using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

struct CPsiBlastTestFixture {

    CRef<CPSIBlastOptionsHandle> m_OptHandle;
    CRef<CPssmWithParameters> m_Pssm;
    CSearchDatabase* m_SearchDb;

    /// Contains a single Bioseq
    CRef<CSeq_entry> m_SeqEntry;

    /// Contains a Bioseq-set with two Bioseqs, gi 7450545 and gi 129295
    CRef<CSeq_entry> m_SeqSet;

    void x_ReadSeqEntriesFromFile() {
        const string kSeqEntryFile("data/7450545.seqentry.asn");
        m_SeqEntry = TestUtil::ReadObject<CSeq_entry>(kSeqEntryFile);

        m_SeqSet.Reset(new CSeq_entry);
        m_SeqSet->SetSet().SetSeq_set().push_back(m_SeqEntry);
        BOOST_REQUIRE(m_Pssm && 
               m_Pssm->CanGetPssm() &&
               m_Pssm->GetPssm().CanGetQuery());
        CRef<CSeq_entry> second_bioseq(&m_Pssm->SetPssm().SetQuery());
        m_SeqSet->SetSet().SetSeq_set().push_back(second_bioseq);
    }

    void x_ReadPssmFromFile() {
        const string kPssmFile("data/pssm_freq_ratios.asn");
        m_Pssm = TestUtil::ReadObject<CPssmWithParameters>(kPssmFile);
        BOOST_REQUIRE(m_Pssm->GetPssm().CanGetQuery());
        BOOST_REQUIRE(m_Pssm->GetPssm().CanGetIntermediateData());
        BOOST_REQUIRE(!m_Pssm->GetPssm().CanGetFinalData());
    }

    CPsiBlastTestFixture() {
        m_OptHandle.Reset(new CPSIBlastOptionsHandle);
        BOOST_REQUIRE_EQUAL(eCompositionMatrixAdjust,
                             m_OptHandle->GetCompositionBasedStats());
        m_SearchDb = new CSearchDatabase("data/seqp", 
                                         CSearchDatabase::eBlastDbIsProtein);

        x_ReadPssmFromFile();
        PsiBlastComputePssmScores(m_Pssm, m_OptHandle->GetOptions());
        BOOST_REQUIRE(m_Pssm->GetPssm().GetFinalData().CanGetScores());

        x_ReadSeqEntriesFromFile();
    }

    ~CPsiBlastTestFixture() {
        m_OptHandle.Reset();
        m_Pssm.Reset();
        delete m_SearchDb;
        m_SeqEntry.Reset();
        m_SeqSet.Reset();
    }

    int s_CountNumberUniqueGIs(CConstRef<CSeq_align_set> sas)
    {
        int num_gis = 0;
        TGi last_gi = INVALID_GI;
        ITERATE(CSeq_align_set::Tdata, itr, sas->Get()){
            const CSeq_id& seqid = (*itr)->GetSeq_id(1);
            TGi new_gi = seqid.GetGi();
            if (new_gi != last_gi)
            {
                 num_gis++;
                 last_gi = new_gi;
            }
        }
        return num_gis;
    }

    IQueryFactory *s_SetupSubject(CConstRef<CBioseq> bioseq) {
        TSeqLocVector subjects;
        CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
        CConstRef<CSeq_id> sid = (scope->AddBioseq(*bioseq)).GetSeqId();
        CRef<CSeq_loc> sl(new CSeq_loc());
        sl->SetWhole();
        sl->SetId(*sid);
        SSeqLoc ssl(*sl, *scope);
        subjects.push_back(ssl);
        return (IQueryFactory *)(new CObjMgr_QueryFactory(subjects));
    }

    IQueryFactory *s_SetupSubject(CConstRef<CBioseq_set> bioseq_set) {
        TSeqLocVector subjects;
        CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
        CTypeConstIterator<CBioseq> itr(ConstBegin(*bioseq_set, eDetectLoops));
        for (; itr; ++itr) {
            CConstRef<CSeq_id> sid = (scope->AddBioseq(*itr)).GetSeqId();
            CRef<CSeq_loc> sl(new CSeq_loc());
            sl->SetWhole();
            sl->SetId(*sid);
            SSeqLoc ssl(*sl, *scope);
            subjects.push_back(ssl);
        }
        return (IQueryFactory *)(new CObjMgr_QueryFactory(subjects));
    }

    CRef<CPssmWithParameters> 
    x_ComputePssmForNextIteration(const CBioseq& query,
                                  CConstRef<CSeq_align_set> sset,
                                  CConstRef<CPSIBlastOptionsHandle> opts_handle,
                                  CConstRef<CBlastAncillaryData> ancillary_data)
    {
        CRef<CSeqDB> db(new CSeqDB("data/seqp", CSeqDB::eProtein));
        CBlastScopeSource blast_om(db);
        CRef<CScope> scope(blast_om.NewScope());
        CPSIDiagnosticsRequest diags(PSIDiagnosticsRequestNew());
        diags->frequency_ratios = true;
        return blast::PsiBlastComputePssmFromAlignment(query, sset, scope,
                                                       *opts_handle, 
                                                       ancillary_data,
                                                       diags);
    }

};

BOOST_FIXTURE_TEST_SUITE(psiblast, CPsiBlastTestFixture)

BOOST_AUTO_TEST_CASE(TestSingleIteration_ProteinAsQuery_NoCBS) {
    m_OptHandle->SetCompositionBasedStats(eNoCompositionBasedStats);
    m_OptHandle->SetEvalueThreshold(1.5);
    CConstRef<CBioseq> bioseq(&m_SeqEntry->GetSeq());
    CRef<IQueryFactory> query_factory(s_SetupSubject(bioseq));
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CPsiBlast psiblast(query_factory, dbadapter, m_OptHandle); 

    CSearchResultSet results(*psiblast.Run());
    BOOST_REQUIRE(results[0].GetErrors().empty());

    const int kNumExpectedMatchingSeqs = 3;
    CConstRef<CSeq_align_set> sas = results[0].GetSeqAlign();

    BOOST_REQUIRE_EQUAL(kNumExpectedMatchingSeqs, s_CountNumberUniqueGIs(sas));

    const size_t kNumExpectedHSPs = 3;
    qa::TSeqAlignSet expected_results(kNumExpectedHSPs);

    // HSP # 1
    expected_results[0].score = 64;
    expected_results[0].evalue = 2.46806e-1;
    expected_results[0].bit_score = 292610051e-7;
    expected_results[0].num_ident = 18;
    expected_results[0].starts.push_back(34);
    expected_results[0].starts.push_back(95);
    expected_results[0].lengths.push_back(53);

    // HSP # 2
    {
        int starts[] = { 203, 280, 220, -1, 226, 297, 258, -1, 265, 329 };
        int lengths[] = { 17, 6, 32, 7, 39 };
        expected_results[1].score = 63;
        expected_results[1].evalue = 3.48342e-1;
        expected_results[1].bit_score = 288758055e-7;
        expected_results[1].num_ident = 24;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[1].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[1].lengths));
    }

    // HSP # 3
    {
        int starts[] = { 180, 97, 204, -1, 205, 121, -1, 127, 211, 128,
            241, -1, 242, 158, -1, 197, 281, 201, 306, -1, 318, 226, 323,
            -1, 327, 231 };
        int lengths[] = { 24, 1, 6, 1, 30, 1, 39, 4, 25, 12, 5, 4, 35 };
        expected_results[2].score = 60;
        expected_results[2].evalue = 7.36231e-1;
        expected_results[2].bit_score = 277202068e-7;
        expected_results[2].num_ident = 42;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[2].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[2].lengths));
    }

    /* HSP # 4
    {
        int starts[] = { 43, 13, -1, 84, 114, 91, 135, -1, 136, 112,
            149, -1, 151, 125, -1, 146, 172, 147, 177, -1, 179, 152,
            195, -1, 200, 168, 226, -1, 228, 194 };
        int lengths[] = { 71, 7, 21, 1, 13, 2, 21, 1, 5, 2, 16, 5, 26, 2,
            11 };
        expected_results[3].score = 57;
        expected_results[3].evalue = 147345337e-8;
        expected_results[3].bit_score = 265646081e-7;
        expected_results[3].num_ident = 48;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[3].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[3].lengths));
    }
    */

    qa::TSeqAlignSet actual_results;
    qa::SeqAlignSetConvert(*sas, actual_results);

    qa::CSeqAlignCmpOpts opts;
    qa::CSeqAlignCmp cmp(expected_results, actual_results, opts);
    string errors;
    bool identical_results = cmp.Run(&errors);

    BOOST_REQUIRE_MESSAGE(identical_results, errors);
}

BOOST_AUTO_TEST_CASE(TestSingleIteration_ProteinAsQuery_CBS) {
    m_OptHandle->SetCompositionBasedStats(eCompositionBasedStats);
    CConstRef<CBioseq> bioseq(&m_SeqEntry->GetSeq());
    CRef<IQueryFactory> query_factory(s_SetupSubject(bioseq));
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CPsiBlast psiblast(query_factory, dbadapter, m_OptHandle); 

    CSearchResultSet results(*psiblast.Run());
    BOOST_REQUIRE(results[0].GetErrors().empty());

    const int kNumExpectedMatchingSeqs = 4;
    CConstRef<CSeq_align_set> sas = results[0].GetSeqAlign();

    BOOST_REQUIRE_EQUAL(kNumExpectedMatchingSeqs, s_CountNumberUniqueGIs(sas));

    const size_t kNumExpectedHSPs = 4;
    qa::TSeqAlignSet expected_results(kNumExpectedHSPs);

    // HSP # 1
    expected_results[0].score = 63;
    expected_results[0].evalue = 2.96035e-1;
    expected_results[0].bit_score = 288758056e-7;
    expected_results[0].starts.push_back(34);
    expected_results[0].starts.push_back(95);
    expected_results[0].lengths.push_back(53);
    expected_results[0].num_ident = 18;

    // HSP # 2
    expected_results[1].score = 59;
    expected_results[1].evalue = 9.29330e-1;
    expected_results[1].bit_score = 273350073e-7;
    expected_results[1].starts.push_back(203);
    expected_results[1].starts.push_back(280);
    expected_results[1].starts.push_back(220);
    expected_results[1].starts.push_back(-1);
    expected_results[1].starts.push_back(226);
    expected_results[1].starts.push_back(297);
    expected_results[1].starts.push_back(258);
    expected_results[1].starts.push_back(-1);
    expected_results[1].starts.push_back(265);
    expected_results[1].starts.push_back(329);
    expected_results[1].lengths.push_back(17);
    expected_results[1].lengths.push_back(6);
    expected_results[1].lengths.push_back(32);
    expected_results[1].lengths.push_back(7);
    expected_results[1].lengths.push_back(39);
    expected_results[1].num_ident = 24;

    // HSP # 3
    expected_results[2].score = 52;
    expected_results[2].evalue = 6.67208;
    expected_results[2].bit_score = 246386102e-7;
    expected_results[2].starts.push_back(322);
    expected_results[2].starts.push_back(46);
    expected_results[2].lengths.push_back(28);
    expected_results[2].num_ident = 10;


    // HSP # 4
    expected_results[3].score = 50;
    expected_results[3].evalue = 7.15763;
    expected_results[3].bit_score = 23.8682;
    expected_results[3].starts.push_back(295);
    expected_results[3].starts.push_back(23);
    expected_results[3].starts.push_back(301);
    expected_results[3].starts.push_back(-1);
    expected_results[3].starts.push_back(304);
    expected_results[3].starts.push_back(29);
    expected_results[3].lengths.push_back(6);
    expected_results[3].lengths.push_back(3);
    expected_results[3].lengths.push_back(23);
    expected_results[3].num_ident = 16;

    qa::TSeqAlignSet actual_results;
    qa::SeqAlignSetConvert(*sas, actual_results);

    qa::CSeqAlignCmpOpts opts;
    qa::CSeqAlignCmp cmp(expected_results, actual_results, opts);
    string errors;
    bool identical_results = cmp.Run(&errors);

    BOOST_REQUIRE_MESSAGE(identical_results, errors);
}

BOOST_AUTO_TEST_CASE(TestSingleIteration_ProteinAsQuery_CBSConditional) {
    m_OptHandle->SetCompositionBasedStats(eCompositionMatrixAdjust);
    m_OptHandle->SetEvalueThreshold(5.0);
    CConstRef<CBioseq> bioseq(&m_SeqEntry->GetSeq());
    CRef<IQueryFactory> query_factory(s_SetupSubject(bioseq));
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CPsiBlast psiblast(query_factory, dbadapter, m_OptHandle); 

    CSearchResultSet results(*psiblast.Run());
    BOOST_REQUIRE(results[0].GetErrors().empty());

    const int kNumExpectedMatchingSeqs = 3;
    CConstRef<CSeq_align_set> sas = results[0].GetSeqAlign();

    BOOST_REQUIRE_EQUAL(kNumExpectedMatchingSeqs, s_CountNumberUniqueGIs(sas));

    const size_t kNumExpectedHSPs = 3;
    qa::TSeqAlignSet expected_results(kNumExpectedHSPs);

    // HSP # 1
    expected_results[0].score = 59;
    expected_results[0].evalue = 8.66100e-1;
    expected_results[0].bit_score = 273350073e-7;
    expected_results[0].starts.push_back(34);
    expected_results[0].starts.push_back(95);
    expected_results[0].lengths.push_back(53);
    expected_results[0].sequence_gis.SetQuery(7450545);
    expected_results[0].sequence_gis.SetSubject(22982149);
    expected_results[0].num_ident = 18;

    // HSP # 3
    {
        int starts[] = { 322 , 46 , -1 , 75 , 351 , 81 , -1 , 94 , 364 , 
            97 , -1 , 106 , 373 , 109 };
        int lengths[] = { 29 , 6 , 13 , 3 , 9 , 3 , 17 };
        expected_results[1].score = 53;
        expected_results[1].evalue = 4.15768;
        expected_results[1].bit_score = 250238098e-7;
        expected_results[1].sequence_gis.SetQuery(7450545);
        expected_results[1].sequence_gis.SetSubject(43121985);
        expected_results[1].num_ident = 19;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[1].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[1].lengths));
    }

    // HSP # 2
    {
        int starts[] = { 125 , 199 , 146 , -1 , 148 , 220 , -1 , 228 , 
            156 , 233 , -1 , 250 , 173 , 252 , 179 , -1 , 181 , 258 , 
            220 , -1 , 226 , 297 , 258 , -1 , 265 , 329 };
        int lengths[] = { 21 , 2 , 8 , 5 , 17 , 2 , 6 , 2 , 39 , 6 , 32 ,
            7 , 39 };
        expected_results[2].score = 54;
        expected_results[2].evalue = 4.40967;
        expected_results[2].bit_score = 254090094e-7;
        expected_results[2].sequence_gis.SetQuery(7450545);
        expected_results[2].sequence_gis.SetSubject(13242404);
        expected_results[2].num_ident = 39;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[2].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[2].lengths));
    }


    // HSP # 4
/*
    expected_results[3].score = 53;
    expected_results[3].evalue = 5.65033;
    expected_results[3].bit_score = 25.0238;
    expected_results[3].sequence_gis.SetQuery(7450545);
    expected_results[3].sequence_gis.SetSubject(15836829);
    expected_results[3].starts.push_back(39);
    expected_results[3].starts.push_back(304);
    expected_results[3].lengths.push_back(41);
    expected_results[3].lengths.push_back(41);
    expected_results[3].num_ident = 15;
*/


    qa::TSeqAlignSet actual_results;
    qa::SeqAlignSetConvert(*sas, actual_results);

    qa::CSeqAlignCmpOpts opts;
    qa::CSeqAlignCmp cmp(expected_results, actual_results, opts);
    string errors;
    bool identical_results = cmp.Run(&errors);

    BOOST_REQUIRE_MESSAGE(identical_results, errors);
}

BOOST_AUTO_TEST_CASE(TestSingleIteration_ProteinAsQuery_CBSUniversal) {
    m_OptHandle->SetCompositionBasedStats(eCompoForceFullMatrixAdjust);
    CConstRef<CBioseq> bioseq(&m_SeqEntry->GetSeq());
    CRef<IQueryFactory> query_factory(s_SetupSubject(bioseq));
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CPsiBlast psiblast(query_factory, dbadapter, m_OptHandle); 

    CSearchResultSet results(*psiblast.Run());
    BOOST_REQUIRE(results[0].GetErrors().empty());

    const int kNumExpectedMatchingSeqs = 5;
    CConstRef<CSeq_align_set> sas = results[0].GetSeqAlign();

    BOOST_REQUIRE_EQUAL(kNumExpectedMatchingSeqs, s_CountNumberUniqueGIs(sas));

    const size_t kNumExpectedHSPs = 5;
    qa::TSeqAlignSet expected_results(kNumExpectedHSPs);

    // HSP # 1
    expected_results[0].score = 59;
    expected_results[0].evalue = 8.66100e-1;
    expected_results[0].bit_score = 273350073e-7;
    expected_results[0].starts.push_back(34);
    expected_results[0].starts.push_back(95);
    expected_results[0].lengths.push_back(53);
    expected_results[0].sequence_gis.SetQuery(7450545);
    expected_results[0].sequence_gis.SetSubject(22982149);
    expected_results[0].num_ident = 18;

    // HSP # 2
    {
        int starts[] = { 322 , 46 , -1 , 75 , 351 , 81 , -1 , 94 , 364 , 
            97 , -1 , 106 , 373 , 109 };
        int lengths[] = { 29 , 6 , 13 , 3 , 9 , 3 , 17 };
        expected_results[1].score = 53;
        expected_results[1].evalue = 4.15768;
        expected_results[1].bit_score = 250238098e-7;
        expected_results[1].sequence_gis.SetQuery(7450545);
        expected_results[1].sequence_gis.SetSubject(43121985);
        expected_results[1].num_ident = 19;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[1].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[1].lengths));
    }


    // HSP # 3
    {
        int starts[] = { 125 , 199 , 146 , -1 , 148 , 220 , -1 , 228 , 
            156 , 233 , -1 , 250 , 173 , 252 , 179 , -1 , 181 , 258 , 
            220 , -1 , 226 , 297 , 258 , -1 , 265 , 329 };
        int lengths[] = { 21 , 2 , 8 , 5 , 17 , 2 , 6 , 2 , 39 , 6 , 32 ,
            7 , 39 };
        expected_results[2].score = 54;
        expected_results[2].evalue = 4.40967;
        expected_results[2].bit_score = 254090094e-7;
        expected_results[2].sequence_gis.SetQuery(7450545);
        expected_results[2].sequence_gis.SetSubject(13242404);
        expected_results[2].num_ident = 39;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[2].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[2].lengths));
    }

    /* HSP # 4
    expected_results[3].score = 53;
    expected_results[3].evalue = 466014414e-8;
    expected_results[3].bit_score = 250238098e-7;
    expected_results[3].sequence_gis.SetQuery(7450545);
    expected_results[3].sequence_gis.SetSubject(45683609);
    expected_results[3].starts.push_back(39);
    expected_results[3].starts.push_back(304);
    expected_results[3].lengths.push_back(41);
    */

    qa::TSeqAlignSet actual_results;
    qa::SeqAlignSetConvert(*sas, actual_results);

    qa::CSeqAlignCmpOpts opts;
    qa::CSeqAlignCmp cmp(expected_results, actual_results, opts);
    string errors;
    bool identical_results = cmp.Run(&errors);

    BOOST_REQUIRE_MESSAGE(identical_results, errors);
}

BOOST_AUTO_TEST_CASE(TestMultipleIterationsAndConvergence_ProteinAsQuery_NoCBS) {
    const int kNumIterations = 4;
    const int kNumExpectedIterations = 2;
    CPsiBlastIterationState itr(kNumIterations);
    m_OptHandle->SetCompositionBasedStats(eNoCompositionBasedStats);

    CConstRef<CBioseq> bioseq(&m_SeqEntry->GetSeq());
    CRef<IQueryFactory> query_factory(s_SetupSubject(bioseq));
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CPsiBlast psiblast(query_factory, dbadapter, m_OptHandle);

    int hits_below_threshold[kNumIterations] = { 0, 0, 0, 0 };
    size_t number_hits[kNumIterations] = { 11, 14, 0, 0 };

    int iteration_counter = 0;
    while (itr) {
        CSearchResultSet results = *psiblast.Run();
        BOOST_REQUIRE(results[0].GetErrors().empty());
        CConstRef<CSeq_align_set> alignment = results[0].GetSeqAlign();
        BOOST_REQUIRE_EQUAL(number_hits[iteration_counter],
                             alignment->Get().size());

        CPsiBlastIterationState::TSeqIds ids;
        CPsiBlastIterationState::GetSeqIds(alignment, m_OptHandle, ids);

        string m("On round ");
        m += NStr::IntToString(itr.GetIterationNumber()) + " found ";
        m += NStr::SizetToString(ids.size()) + " qualifying ids";
        BOOST_REQUIRE_MESSAGE( 
                hits_below_threshold[iteration_counter]==(int)ids.size(), m);
        itr.Advance(ids);

        if (itr) {
            CRef<CPssmWithParameters> pssm =
                x_ComputePssmForNextIteration(*bioseq, alignment,
                      m_OptHandle, results[0].GetAncillaryData());
            psiblast.SetPssm(pssm);
        }
        iteration_counter++;
    }

    BOOST_REQUIRE_EQUAL(kNumExpectedIterations, iteration_counter);
}

BOOST_AUTO_TEST_CASE(TestSingleIteration_PssmAsQuery_NoCBS) {

    m_OptHandle->SetCompositionBasedStats(eNoCompositionBasedStats);
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CPsiBlast psiblast(m_Pssm, dbadapter, m_OptHandle);

    CSearchResultSet results(*psiblast.Run());
    BOOST_REQUIRE(results[0].GetErrors().empty());

    const int kNumExpectedMatchingSeqs = 6;
    CConstRef<CSeq_align_set> sas = results[0].GetSeqAlign();
    BOOST_REQUIRE_EQUAL(kNumExpectedMatchingSeqs, s_CountNumberUniqueGIs(sas));

    const size_t kNumExpectedHSPs = 7;
    qa::TSeqAlignSet expected_results(kNumExpectedHSPs);

    // HSP # 1
    {
        int starts[] = { 0, 941, -1, 1093, 152, 1094 };
        int lengths[] = { 152, 1, 80 };
        expected_results[0].score = 595;
        expected_results[0].evalue = 2.70189-71;
        expected_results[0].bit_score = 233623298e-6;
        expected_results[0].num_ident = 101;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[0].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[0].lengths));
    }

    // HSP # 2
    {
        int starts[] = { 0, 154, -1, 308, 154, 309 };
        int lengths[] = { 154, 1, 24 };
        expected_results[1].score = 424;
        expected_results[1].evalue = 5.17079e-48;
        expected_results[1].bit_score = 167754171e-6;
        expected_results[1].num_ident = 73;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[1].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[1].lengths));
    }

    // HSP # 3
    {
        int starts[] = 
            { 0, 190, 65, -1, 67, 255, 91, -1, 92, 279, 111, -1, 113, 298,
             -1, 304, 119, 305, 151, -1, 152, 337, 163, -1, 164, 348, 
             -1, 374, 190, 380, 200, -1, 202, 390 };
        int lengths[] = 
            { 65, 2, 24, 1, 19, 2, 6, 1, 32, 1, 11, 1, 26, 6, 10, 2, 30 };
        expected_results[2].score = 372;
        expected_results[2].evalue = 1.34154e-42;
        expected_results[2].bit_score = 147723793e-6;
        expected_results[2].num_ident = 87;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[2].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[2].lengths));
    }

    // HSP # 4
    expected_results[3].score = 53;
    expected_results[3].evalue = 2.43336;
    expected_results[3].bit_score = 248451288e-7;
    expected_results[3].num_ident = 8;
    expected_results[3].starts.push_back(206);
    expected_results[3].starts.push_back(46);
    expected_results[3].lengths.push_back(19);

    // HSP # 5
    {
        int starts[] = { 177, 100, -1, 106, 183, 107, 205, -1, 215, 129 };
        int lengths[] = { 6, 1, 22, 10, 14 };
        expected_results[4].score = 52;
        expected_results[4].evalue = 3.12771;
        expected_results[4].bit_score = 244599292e-7;
        expected_results[4].num_ident = 11;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[4].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[4].lengths));
    }

    // HSP # 6
    {
        int starts[] = { 74, 181, 108, -1, 109, 215 };
        int lengths[] = { 34, 1, 23 };
        expected_results[5].score = 49;
        expected_results[5].evalue = 8.37737;
        expected_results[5].bit_score = 233043305e-7;
        expected_results[5].num_ident = 14;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[5].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[5].lengths));
    }

    // HSP # 7
    expected_results[6].score = 49;
    expected_results[6].evalue = 8.62465;
    expected_results[6].bit_score = 233043305e-7;
    expected_results[6].num_ident = 6;
    expected_results[6].starts.push_back(188);
    expected_results[6].starts.push_back(709);
    expected_results[6].lengths.push_back(30);

    qa::TSeqAlignSet actual_results;
    qa::SeqAlignSetConvert(*sas, actual_results);

    qa::CSeqAlignCmpOpts opts;
    qa::CSeqAlignCmp cmp(expected_results, actual_results, opts);
    string errors;
    bool identical_results = cmp.Run(&errors);

    BOOST_REQUIRE_MESSAGE(identical_results, errors);
}

BOOST_AUTO_TEST_CASE(TestSingleIteration_PssmAsQuery_CBS) {

    m_OptHandle->SetCompositionBasedStats(eCompositionBasedStats);
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CPsiBlast psiblast(m_Pssm, dbadapter, m_OptHandle);

    CSearchResultSet results(*psiblast.Run());
    BOOST_REQUIRE(results[0].GetErrors().empty());

    const int kNumExpectedMatchingSeqs = 4;
    CConstRef<CSeq_align_set> sas = results[0].GetSeqAlign();
    BOOST_REQUIRE_EQUAL(kNumExpectedMatchingSeqs, s_CountNumberUniqueGIs(sas));

    const size_t kNumExpectedHSPs = 5;
    qa::TSeqAlignSet expected_results(kNumExpectedHSPs);

    // HSP # 1
    {
        int starts[] = { 0, 941, -1, 1093, 152, 1094 };
        int lengths[] = { 152, 1, 80 };
        expected_results[0].score = 593;
        expected_results[0].evalue = 5.81663e-71;
        expected_results[0].bit_score = 232843196e-6;
        expected_results[0].sequence_gis.SetQuery(129295);
        expected_results[0].sequence_gis.SetSubject(34878800);
        expected_results[0].num_ident = 101;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[0].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[0].lengths));
    }

    // HSP # 2
    {
        int starts[] = { 0, 154, -1, 308, 154, 309 };
        int lengths[] = { 154, 1, 24 };
        expected_results[1].score = 417;
        expected_results[1].evalue = 4.07856e-47;
        expected_results[1].bit_score = 165048071e-6;
        expected_results[1].sequence_gis.SetQuery(129295);
        expected_results[1].sequence_gis.SetSubject(34878800);
        expected_results[1].num_ident = 73;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[1].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[1].lengths));
    }

    // HSP # 3
    {
        int starts[] = 
            { 0, 190, 65, -1, 67, 255, 93, -1, 94, 281, 111, -1, 113, 298,
             -1, 304, 119, 305, 153, -1, 154, 339, 164, -1, 165, 349, 
             -1, 378, 194, 382 };
        int lengths[] = 
            { 65, 2, 26, 1, 17, 2, 6, 1, 34, 1, 10, 1, 29, 4, 38 };
        expected_results[2].score = 359;
        expected_results[2].evalue = 8.35828e-41;
        expected_results[2].bit_score = 142706496e-6;
        expected_results[2].sequence_gis.SetQuery(129295);
        expected_results[2].sequence_gis.SetSubject(20092202);
        expected_results[2].num_ident = 85;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[2].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[2].lengths));
    }

    // HSP # 4
    expected_results[3].score = 53;
    expected_results[3].evalue = 2.14427;
    expected_results[3].bit_score = 248354256e-7;
    expected_results[3].sequence_gis.SetQuery(129295);
    expected_results[3].sequence_gis.SetSubject(44343511);
    expected_results[3].starts.push_back(206);
    expected_results[3].starts.push_back(46);
    expected_results[3].lengths.push_back(19);
    expected_results[3].num_ident = 8;

    // HSP # 5
    expected_results[4].score = 51;
    expected_results[4].evalue = 5.09267;
    expected_results[4].bit_score = 240650265e-7;
    expected_results[4].sequence_gis.SetQuery(129295);
    expected_results[4].sequence_gis.SetSubject(23481125);
    expected_results[4].starts.push_back(188);
    expected_results[4].starts.push_back(709);
    expected_results[4].lengths.push_back(30);
    expected_results[4].num_ident = 6;

    /* HSP # 6
    {
        int starts[] = { 74, 181, 108, -1, 109, 215 };
        int lengths[] = { 34, 1, 23 };
        expected_results[5].score = 48;
        expected_results[5].evalue = 878932376e-8;
        expected_results[5].bit_score = 229094278e-7;
        expected_results[5].sequence_gis.SetQuery(129295);
        expected_results[5].sequence_gis.SetSubject(38088088);
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[5].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[5].lengths));
    }
    */

    qa::TSeqAlignSet actual_results;
    qa::SeqAlignSetConvert(*sas, actual_results);

    qa::CSeqAlignCmpOpts opts;
    qa::CSeqAlignCmp cmp(expected_results, actual_results, opts);
    string errors;
    bool identical_results = cmp.Run(&errors);

    BOOST_REQUIRE_MESSAGE(identical_results, errors);
}

// N.B.: these can't be done from a PSSM
#if 0
BOOST_AUTO_TEST_CASE(TestSingleIteration_PssmAsQuery_CBSConditional) {

    m_OptHandle->SetCompositionBasedStats(eCompositionMatrixAdjust);
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CPsiBlast psiblast(m_Pssm, dbadapter, m_OptHandle);

    CSearchResultSet results(*psiblast.Run());
    BOOST_REQUIRE(results[0].GetErrors().empty());

    const int kNumExpectedMatchingSeqs = 6;
    CConstRef<CSeq_align_set> sas = results[0].GetSeqAlign();
    BOOST_REQUIRE_EQUAL(kNumExpectedMatchingSeqs, s_CountNumberUniqueGIs(sas));

    const size_t kNumExpectedHSPs = 7;
    qa::TSeqAlignSet expected_results(kNumExpectedHSPs);

    // HSP # 1
    {
        int starts[] = { 0, 941, -1, 1093, 152, 1094 };
        int lengths[] = { 152, 1, 80 };
        expected_results[0].score = 595;
        expected_results[0].evalue = 307180919e-71;
        expected_results[0].bit_score = 233623298e-6;
        expected_results[0].num_ident = 101;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[0].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[0].lengths));
    }

    // HSP # 2
    {
        int starts[] = { 0, 154, -1, 308, 154, 309 };
        int lengths[] = { 154, 1, 24 };
        expected_results[1].score = 424;
        expected_results[1].evalue = 20700336e-50;
        expected_results[1].bit_score = 167754171e-6;
        expected_results[1].num_ident = 73;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[1].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[1].lengths));
    }

    // HSP # 3
    {
        int starts[] = 
            { 0, 190, 65, -1, 67, 255, 91, -1, 92, 279, 111, -1, 113, 298,
             -1, 304, 119, 305, 151, -1, 152, 337, 163, -1, 164, 348, 
             -1, 374, 190, 380, 200, -1, 202, 390 };
        int lengths[] = 
            { 65, 2, 24, 1, 19, 2, 6, 1, 32, 1, 11, 1, 26, 6, 10, 2, 30 };
        expected_results[2].score = 372;
        expected_results[2].evalue = 221677687e-45;
        expected_results[2].bit_score = 147723793e-6;
        expected_results[2].num_ident = 87;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[2].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[2].lengths));
    }

    // HSP # 4
    expected_results[3].score = 53;
    expected_results[3].evalue = 216713461e-8;
    expected_results[3].bit_score = 248451288e-7;
    expected_results[3].num_ident = 8;
    expected_results[3].starts.push_back(206);
    expected_results[3].starts.push_back(46);
    expected_results[3].lengths.push_back(19);

    // HSP # 5
    {
        int starts[] = { 177, 100, -1, 106, 183, 107, 205, -1, 215, 129 };
        int lengths[] = { 6, 1, 22, 10, 14 };
        expected_results[4].score = 52;
        expected_results[4].evalue = 283036546e-8;
        expected_results[4].bit_score = 244599292e-7;
        expected_results[4].num_ident = 11;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[4].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[4].lengths));
    }

    // HSP # 6
    {
        int starts[] = { 74, 181, 108, -1, 109, 215 };
        int lengths[] = { 34, 1, 23 };
        expected_results[5].score = 49;
        expected_results[5].evalue = 630539642e-8;
        expected_results[5].bit_score = 233043305e-7;
        expected_results[5].num_ident = 14;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[5].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[5].lengths));
    }

    // HSP # 7
    expected_results[6].score = 49;
    expected_results[6].evalue = 630539642e-8;
    expected_results[6].bit_score = 233043305e-7;
    expected_results[6].num_ident = 6;
    expected_results[6].starts.push_back(188);
    expected_results[6].starts.push_back(709);
    expected_results[6].lengths.push_back(30);

    qa::TSeqAlignSet actual_results;
    qa::SeqAlignSetConvert(*sas, actual_results);

    qa::CSeqAlignCmpOpts opts;
    qa::CSeqAlignCmp cmp(expected_results, actual_results, opts);
    string errors;
    bool identical_results = cmp.Run(&errors);

    BOOST_REQUIRE_MESSAGE(identical_results, errors);
}

BOOST_AUTO_TEST_CASE(TestSingleIteration_PssmAsQuery_CBSUniversal) {

    m_OptHandle->SetCompositionBasedStats(eCompoForceFullMatrixAdjust);
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CPsiBlast psiblast(m_Pssm, dbadapter, m_OptHandle);

    CSearchResultSet results(*psiblast.Run());
    BOOST_REQUIRE(results[0].GetErrors().empty());

    const int kNumExpectedMatchingSeqs = 6;
    CConstRef<CSeq_align_set> sas = results[0].GetSeqAlign();
    BOOST_REQUIRE_EQUAL(kNumExpectedMatchingSeqs, s_CountNumberUniqueGIs(sas));

    const size_t kNumExpectedHSPs = 7;
    qa::TSeqAlignSet expected_results(kNumExpectedHSPs);

    // HSP # 1
    {
        int starts[] = { 0, 941, -1, 1093, 152, 1094 };
        int lengths[] = { 152, 1, 80 };
        expected_results[0].score = 595;
        expected_results[0].evalue = 307180919e-71;
        expected_results[0].bit_score = 233623298e-6;
        expected_results[0].num_ident = 101;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[0].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[0].lengths));
    }

    // HSP # 2
    {
        int starts[] = { 0, 154, -1, 308, 154, 309 };
        int lengths[] = { 154, 1, 24 };
        expected_results[1].score = 424;
        expected_results[1].evalue = 20700336e-50;
        expected_results[1].bit_score = 167754171e-6;
        expected_results[1].num_ident = 73;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[1].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[1].lengths));
    }

    // HSP # 3
    {
        int starts[] = 
            { 0, 190, 65, -1, 67, 255, 91, -1, 92, 279, 111, -1, 113, 298,
             -1, 304, 119, 305, 151, -1, 152, 337, 163, -1, 164, 348, 
             -1, 374, 190, 380, 200, -1, 202, 390 };
        int lengths[] = 
            { 65, 2, 24, 1, 19, 2, 6, 1, 32, 1, 11, 1, 26, 6, 10, 2, 30 };
        expected_results[2].score = 372;
        expected_results[2].evalue = 221677687e-45;
        expected_results[2].bit_score = 147723793e-6;
        expected_results[2].num_ident = 87;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[2].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[2].lengths));
    }

    // HSP # 4
    expected_results[3].score = 53;
    expected_results[3].evalue = 216713461e-8;
    expected_results[3].bit_score = 248451288e-7;
    expected_results[3].num_ident = 8;
    expected_results[3].starts.push_back(206);
    expected_results[3].starts.push_back(46);
    expected_results[3].lengths.push_back(19);

    // HSP # 5
    {
        int starts[] = { 177, 100, -1, 106, 183, 107, 205, -1, 215, 129 };
        int lengths[] = { 6, 1, 22, 10, 14 };
        expected_results[4].score = 52;
        expected_results[4].evalue = 283036546e-8;
        expected_results[4].bit_score = 244599292e-7;
        expected_results[4].num_ident = 11;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[4].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[4].lengths));
    }

    // HSP # 6
    {
        int starts[] = { 74, 181, 108, -1, 109, 215 };
        int lengths[] = { 34, 1, 23 };
        expected_results[5].score = 49;
        expected_results[5].evalue = 630539642e-8;
        expected_results[5].bit_score = 233043305e-7;
        expected_results[5].num_ident = 14;
        copy(&starts[0], &starts[STATIC_ARRAY_SIZE(starts)],
             back_inserter(expected_results[5].starts));
        copy(&lengths[0], &lengths[STATIC_ARRAY_SIZE(lengths)],
             back_inserter(expected_results[5].lengths));
    }

    // HSP # 7
    expected_results[6].score = 49;
    expected_results[6].evalue = 630539642e-8;
    expected_results[6].bit_score = 233043305e-7;
    expected_results[6].num_ident = 6;
    expected_results[6].starts.push_back(188);
    expected_results[6].starts.push_back(709);
    expected_results[6].lengths.push_back(30);

    qa::TSeqAlignSet actual_results;
    qa::SeqAlignSetConvert(*sas, actual_results);

    qa::CSeqAlignCmpOpts opts;
    qa::CSeqAlignCmp cmp(expected_results, actual_results, opts);
    string errors;
    bool identical_results = cmp.Run(&errors);

    BOOST_REQUIRE_MESSAGE(identical_results, errors);
}
#endif

// This search will converge after 2 iterations
BOOST_AUTO_TEST_CASE(TestMultipleIterationsAndConvergence_PssmAsQuery_NoCBS) {

    const int kNumIterations = 4;
    const int kNumExpectedIterations = 2;
    CPsiBlastIterationState itr(kNumIterations);
    m_OptHandle->SetCompositionBasedStats(eNoCompositionBasedStats);
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CPsiBlast psiblast(m_Pssm, dbadapter, m_OptHandle);

    int hits_below_threshold[kNumIterations] = { 2, 2, 0, 0 };
    int number_hits[kNumIterations] = { 6, 5, 0, 0 };

    int iteration_counter = 0;
    while (itr) {
        CSearchResultSet results = *psiblast.Run();
        BOOST_REQUIRE(results[0].GetErrors().empty());
        CConstRef<CSeq_align_set> alignment = results[0].GetSeqAlign();

        BOOST_REQUIRE_EQUAL(number_hits[iteration_counter],
                             s_CountNumberUniqueGIs(alignment));

        CPsiBlastIterationState::TSeqIds ids;
        CPsiBlastIterationState::GetSeqIds(alignment, m_OptHandle, ids);

        string m("On round ");
        m += NStr::IntToString(itr.GetIterationNumber()) + " found ";
        m += NStr::SizetToString(ids.size()) + " qualifying ids";
        BOOST_REQUIRE_EQUAL(hits_below_threshold[iteration_counter],
                             (int)ids.size());
        itr.Advance(ids);

        if (itr) {
            const CBioseq& query = m_Pssm->GetPssm().GetQuery().GetSeq();
            CRef<CPssmWithParameters> pssm =
                x_ComputePssmForNextIteration(query, alignment,
                      m_OptHandle, results[0].GetAncillaryData());
            psiblast.SetPssm(pssm);
        }
        iteration_counter++;
    }

    BOOST_REQUIRE_EQUAL(kNumExpectedIterations, iteration_counter);
}

// This search will converge after 2 iterations
BOOST_AUTO_TEST_CASE(TestMultipleIterationsAndConvergence_PssmAsQuery_CBS) {

    const int kNumIterations = 4;
    const int kNumExpectedIterations = 2;
    CPsiBlastIterationState itr(kNumIterations);
    m_OptHandle->SetCompositionBasedStats(eCompositionBasedStats);
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CPsiBlast psiblast(m_Pssm, dbadapter, m_OptHandle);

    int hits_below_threshold[kNumIterations] = { 2, 2, 0, 0 };
    int number_hits[kNumIterations] = { 4, 3, 0, 0 };

    int iteration_counter = 0;
    while (itr) {
        CSearchResultSet results = *psiblast.Run();
        BOOST_REQUIRE(results[0].GetErrors().empty());
        CConstRef<CSeq_align_set> alignment = results[0].GetSeqAlign();
        BOOST_REQUIRE(alignment.NotEmpty());
        BOOST_REQUIRE_EQUAL(number_hits[iteration_counter],
                             s_CountNumberUniqueGIs(alignment));

        CPsiBlastIterationState::TSeqIds ids;
        CPsiBlastIterationState::GetSeqIds(alignment, m_OptHandle, ids);

        string m("On round ");
        m += NStr::IntToString(itr.GetIterationNumber()) + " found ";
        m += NStr::SizetToString(ids.size()) + " qualifying ids";
        BOOST_REQUIRE_EQUAL(hits_below_threshold[iteration_counter],
                             (int)ids.size());
        itr.Advance(ids);

        if (itr) {
            const CBioseq& query = m_Pssm->GetPssm().GetQuery().GetSeq();
            CRef<CPssmWithParameters> pssm =
                x_ComputePssmForNextIteration(query, alignment,
                      m_OptHandle, results[0].GetAncillaryData());
            psiblast.SetPssm(pssm);
        }
        iteration_counter++;
    }

    BOOST_REQUIRE_EQUAL(kNumExpectedIterations, iteration_counter);
}

// Should throw exception as only one query sequence/pssm is allowed
BOOST_AUTO_TEST_CASE(TestMultipleQueries) {
    CConstRef<CBioseq_set> bioseq_set(&m_SeqSet->GetSet());
    CRef<IQueryFactory> query_factory(s_SetupSubject(bioseq_set));
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    BOOST_REQUIRE_THROW(CPsiBlast psiblast(query_factory, dbadapter, m_OptHandle), CBlastException);
}

BOOST_AUTO_TEST_CASE(TestNullQuery) {
    CRef<IQueryFactory> query_factory;
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    BOOST_REQUIRE_THROW(CPsiBlast psiblast(query_factory, dbadapter, m_OptHandle),
                        CBlastException);
}

BOOST_AUTO_TEST_CASE(TestFrequencyRatiosWithAllZerosInPssm) {
    NON_CONST_ITERATE(CPssmIntermediateData::TFreqRatios, fr,
            m_Pssm->SetPssm().SetIntermediateData().SetFreqRatios()) {
        *fr = 0.0;
    }

    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CPsiBlast psiblast(m_Pssm, dbadapter, m_OptHandle);
    CSearchResultSet r = *psiblast.Run();
    TQueryMessages messages = r[0].GetErrors(eBlastSevWarning);
    BOOST_REQUIRE( !messages.empty() );

    string expected_warning("Frequency ratios for PSSM are all zeros");
    string warning;
    ITERATE(TQueryMessages, m, messages) {
        if (((*m)->GetSeverity() == eBlastSevWarning) &&
            ((*m)->GetMessage().find(expected_warning) != string::npos)) {
                warning = (*m)->GetMessage();
                break;
        }
    }
    BOOST_REQUIRE_MESSAGE(!warning.empty(), "Did not find expected warning");
}

BOOST_AUTO_TEST_CASE(TestNullPssm) {
    m_Pssm.Reset();
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    BOOST_REQUIRE_THROW(CPsiBlast psiblast(m_Pssm, dbadapter, m_OptHandle),
                        CBlastException);
}

BOOST_AUTO_TEST_CASE(TestSetNullPssm) {
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CPsiBlast psiblast(m_Pssm, dbadapter, m_OptHandle);
    CRef<CPssmWithParameters> pssm;
    BOOST_REQUIRE_THROW(psiblast.SetPssm(pssm), CBlastException);
}

BOOST_AUTO_TEST_CASE(TestNonExistantDb) {
    m_SearchDb->SetDatabaseName("dummy");
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    CPsiBlast psiblast(m_Pssm, dbadapter, m_OptHandle); 
    BOOST_REQUIRE_THROW(psiblast.Run(),CSeqDBException);
}

BOOST_AUTO_TEST_CASE(TestNullOptions) {
    m_OptHandle.Reset();
    CConstRef<CBioseq> bioseq(&m_SeqEntry->GetSeq());
    CRef<IQueryFactory> query_factory(s_SetupSubject(bioseq));
    CRef<CLocalDbAdapter> dbadapter(new CLocalDbAdapter(*m_SearchDb));
    BOOST_REQUIRE_THROW(CPsiBlast psiblast(query_factory, dbadapter, m_OptHandle), 
                        CBlastException);
}

BOOST_AUTO_TEST_SUITE_END()
