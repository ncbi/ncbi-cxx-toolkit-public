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

/** @file psibl2seq-cppunit.cpp
 * Unit test module for the PSI-BLAST 2 Sequences class
 */
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <serial/iterator.hpp>
#include <algo/blast/api/psibl2seq.hpp>
#include "psiblast_aux_priv.hpp"        // for PsiBlastComputePssmScores
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>

// Object includes
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/scoremat/PssmIntermediateData.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objtools/simple/simple_om.hpp>

#include "test_objmgr.hpp"
#include "blast_test_util.hpp"      // needed to read datatool generated objects
//#include "psiblast_test_util.hpp"   // needed for construction of PSSM

// SeqAlign comparison includes
#include "seqalign_cmp.hpp"
#include "seqalign_set_convert.hpp"

using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

class CPsiBl2SeqTestFixture {
public:

    CRef<CPSIBlastOptionsHandle> m_OptHandle;
    CRef<CPssmWithParameters> m_Pssm;

    // Data members which store the subject(s)

    /// must be initialized with one of the two data members below
    CRef<IQueryFactory> m_Subject; 

    /// Contains a single Bioseq
    CRef<CSeq_entry> m_SeqEntry;

    /// Contains a Bioseq-set with two Bioseqs, gi 7450545 and gi 129295
    CRef<CSeq_entry> m_SeqSet;

    CRef<CScope> m_Scope;

    CPsiBl2SeqTestFixture() {
        m_OptHandle.Reset(new CPSIBlastOptionsHandle);

        x_ReadPssmFromFile();
        PsiBlastComputePssmScores(m_Pssm, m_OptHandle->GetOptions());
        BOOST_REQUIRE(m_Pssm->GetPssm().GetFinalData().CanGetScores());

        x_ReadSeqEntriesFromFile();

        CConstRef<CBioseq> bioseq(&m_SeqEntry->GetSeq());
        x_SetupSubject(bioseq);
    }

    ~CPsiBl2SeqTestFixture() {
        m_Scope.Reset();
        m_Pssm.Reset();
        m_OptHandle.Reset();
        m_SeqEntry.Reset();
        m_SeqSet.Reset();
        m_Subject.Reset();
    }

    // Auxiliary private functions go below...

    void x_SetupSubject(CConstRef<CBioseq> bioseq) {
        TSeqLocVector subjects;
        m_Scope.Reset(new CScope(*CObjectManager::GetInstance()));
        CConstRef<CSeq_id> sid = (m_Scope->AddBioseq(*bioseq)).GetSeqId();
        CRef<CSeq_loc> sl(new CSeq_loc());
        sl->SetWhole();
        sl->SetId(*sid);
        SSeqLoc ssl(*sl, *m_Scope);
        subjects.push_back(ssl);
        m_Subject.Reset(new CObjMgr_QueryFactory(subjects));
    }

    void x_SetupSubject(CConstRef<CBioseq_set> bioseq_set) {
        TSeqLocVector subjects;
        m_Scope.Reset(new CScope(*CObjectManager::GetInstance()));
        CTypeConstIterator<CBioseq> itr(ConstBegin(*bioseq_set, eDetectLoops));
        for (; itr; ++itr) {
            CConstRef<CSeq_id> sid = (m_Scope->AddBioseq(*itr)).GetSeqId();
            CRef<CSeq_loc> sl(new CSeq_loc());
            sl->SetWhole();
            sl->SetId(*sid);
            SSeqLoc ssl(*sl, *m_Scope);
            subjects.push_back(ssl);
        }
        m_Subject.Reset(new CObjMgr_QueryFactory(subjects));
    }

    // Note that the scoremat stored in the file does not have scores
    void x_ReadPssmFromFile() {
        const string kPssmFile("data/pssm_freq_ratios.asn");
        m_Pssm = TestUtil::ReadObject<CPssmWithParameters>(kPssmFile);
        BOOST_REQUIRE(m_Pssm->GetPssm().CanGetQuery());
        BOOST_REQUIRE(m_Pssm->GetPssm().CanGetIntermediateData());
        BOOST_REQUIRE(!m_Pssm->GetPssm().CanGetFinalData());
    }

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

    void x_ValidatePssmVsGi40456275(CConstRef<CSeq_align> sa) {
        BOOST_REQUIRE_EQUAL(false, sa->IsSetSegs());
    }

    void x_ValidatePssmVsGi7450545(CConstRef<CSeq_align> sa, int hsp_num) {
        BOOST_REQUIRE(sa->GetSegs().IsDenseg());

        const CDense_seg & denseg = sa->GetSegs().GetDenseg();

        if (hsp_num == 1)
        {
            // Validate the first HSP
            pair<TSeqRange, TSeqRange> first_hsp = 
                make_pair(TSeqRange(24, 29), TSeqRange(245, 250));
            TSeqRange hsp1_query = denseg.GetSeqRange(0);
            TSeqRange hsp1_subj = denseg.GetSeqRange(1);
            BOOST_REQUIRE_EQUAL(first_hsp.first.GetFrom(), hsp1_query.GetFrom());
            BOOST_REQUIRE_EQUAL(first_hsp.first.GetTo(), hsp1_query.GetTo());
            BOOST_REQUIRE_EQUAL(first_hsp.second.GetFrom(), hsp1_subj.GetFrom());
            BOOST_REQUIRE_EQUAL(first_hsp.second.GetTo(), hsp1_subj.GetTo());
        }
        else if (hsp_num == 2)
        {
            // Validate the second HSP
            const pair<TSeqRange, TSeqRange> second_hsp = 
                make_pair(TSeqRange(74, 86), TSeqRange(108, 120));
            TSeqRange hsp2_query = denseg.GetSeqRange(0);
            TSeqRange hsp2_subj = denseg.GetSeqRange(1);
            BOOST_REQUIRE_EQUAL(second_hsp.first.GetFrom(), hsp2_query.GetFrom());
            BOOST_REQUIRE_EQUAL(second_hsp.first.GetTo(), hsp2_query.GetTo());
            BOOST_REQUIRE_EQUAL(second_hsp.second.GetFrom(), hsp2_subj.GetFrom());
            BOOST_REQUIRE_EQUAL(second_hsp.second.GetTo(), hsp2_subj.GetTo());
        }

    }

    void x_ValidatePssmVsGi129295(CConstRef<CSeq_align> sa) {
        BOOST_REQUIRE(sa->GetSegs().IsDenseg());

        const CDense_seg & denseg = sa->GetSegs().GetDenseg();

        // Validate the first (and only) HSP, which is a self hit
        const TSeqRange hsp(0, 231);
        TSeqRange hsp1_query = denseg.GetSeqRange(0);
        TSeqRange hsp1_subj = denseg.GetSeqRange(1);
        BOOST_REQUIRE_EQUAL(hsp.GetFrom(), hsp1_query.GetFrom());
        BOOST_REQUIRE_EQUAL(hsp.GetTo(), hsp1_query.GetTo());
        BOOST_REQUIRE_EQUAL(hsp.GetFrom(), hsp1_subj.GetFrom());
        BOOST_REQUIRE_EQUAL(hsp.GetTo(), hsp1_subj.GetTo());

    }

};

BOOST_FIXTURE_TEST_SUITE(psibl2seq, CPsiBl2SeqTestFixture)

#if 0
BOOST_AUTO_TEST_CASE(TestInvalidPSSM_ScaledPSSM) {
    m_Pssm->SetPssm().SetFinalData().SetScalingFactor(2);
    BOOST_REQUIRE_THROW(CPsiBl2Seq blaster(m_Pssm, m_Subject, m_OptHandle),
        CBlastException);
}

BOOST_AUTO_TEST_CASE(TestInvalidPSSM_MissingScoresAndFreqRatios) {
    m_Pssm->SetPssm().SetFinalData().ResetScores();
    m_Pssm->SetPssm().SetIntermediateData().ResetFreqRatios();
    BOOST_REQUIRE_THROW(CPsiBl2Seq blaster(m_Pssm, m_Subject, m_OptHandle),
        CBlastException);
}

BOOST_AUTO_TEST_CASE(TestInvalidPSSM_MissingQuery) {
    m_Pssm->SetPssm().ResetQuery();
    BOOST_REQUIRE_THROW(CPsiBl2Seq blaster(m_Pssm, m_Subject, m_OptHandle),
        CBlastException);
}

BOOST_AUTO_TEST_CASE(TestInvalidPSSM_Bioseq_setAsQuery) {
    m_Pssm->SetPssm().SetQuery(*m_SeqSet);
    BOOST_REQUIRE_THROW(CPsiBl2Seq blaster(m_Pssm, m_Subject, m_OptHandle),
        CBlastException);
}

BOOST_AUTO_TEST_CASE(TestInvalidPSSM_NuclScoringMatrix) {
    m_Pssm->SetPssm().SetIsProtein(false);
    BOOST_REQUIRE_THROW(CPsiBl2Seq blaster(m_Pssm, m_Subject, m_OptHandle),
        CBlastException);
}

BOOST_AUTO_TEST_CASE(TestMissingQuery) {
    CRef<CBlastProteinOptionsHandle>
            opts(dynamic_cast<CBlastProteinOptionsHandle*>(&*m_OptHandle));
    CRef<IQueryFactory> empty_query;
    BOOST_REQUIRE_THROW(CPsiBl2Seq blaster(empty_query, m_Subject, opts),
        CBlastException);
}
BOOST_AUTO_TEST_CASE(TestMultipleQueries) {
    TSeqLocVector queries;
    int gis[] = { 
            129295,         // this gi is protein
            555 };          // this gi is nucleotide
    for (size_t i = 0; i < sizeof(gis)/sizeof(*gis); i++) {
        CRef<CSeq_id> seqid(new CSeq_id(CSeq_id::e_Gi, gis[i]));
        TSeqRange range(0U, 50U);
        auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().
                          CreateSSeqLoc(*seqid, range));
        queries.push_back(*sl);
    }
    CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(queries));
    CRef<CBlastProteinOptionsHandle>
        opts(dynamic_cast<CBlastProteinOptionsHandle*>(&*m_OptHandle));
    BOOST_REQUIRE_THROW(CPsiBl2Seq blaster(qf, m_Subject, opts), 
        CBlastException);
}

BOOST_AUTO_TEST_CASE(TestQueryIsNucleotide) {
    TSeqLocVector queries;
    CRef<CSeq_id> seqid(new CSeq_id(CSeq_id::e_Gi, 555));
    TSeqRange range(0U, 500U);
    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().
                     CreateSSeqLoc(*seqid, range));
    queries.push_back(*sl);
    CRef<IQueryFactory> query(new CObjMgr_QueryFactory(queries));

    CRef<CBlastProteinOptionsHandle>
        opts(dynamic_cast<CBlastProteinOptionsHandle*>(&*m_OptHandle));
    BOOST_REQUIRE_THROW(CPsiBl2Seq blaster(query, m_Subject, opts), 
        CBlastException);
}

BOOST_AUTO_TEST_CASE(TestSubjectIsNucleotide) {
    TSeqLocVector sequences;
    CRef<CSeq_id> seqid(new CSeq_id(CSeq_id::e_Gi, 555));
    TSeqRange range(0U, 500U);
    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().
                     CreateSSeqLoc(*seqid, range));
    sequences.push_back(*sl);
    m_Subject.Reset(new CObjMgr_QueryFactory(sequences));
    BOOST_REQUIRE_THROW(CPsiBl2Seq blaster(m_Pssm, m_Subject, m_OptHandle), 
        CBlastException);
}

BOOST_AUTO_TEST_CASE(TestMissingSubjects) {
    m_Subject.Reset();
    BOOST_REQUIRE_THROW(CPsiBl2Seq blaster(m_Pssm, m_Subject, m_OptHandle), 
        CBlastException);
}

BOOST_AUTO_TEST_CASE(TestMissingPSSM) {
    m_Pssm.Reset();
    BOOST_REQUIRE_THROW(CPsiBl2Seq blaster(m_Pssm, m_Subject, m_OptHandle), 
        CBlastException);
}

BOOST_AUTO_TEST_CASE(TestMissingOptions) {
    m_OptHandle.Reset();
    BOOST_REQUIRE_THROW(CPsiBl2Seq blaster(m_Pssm, m_Subject, m_OptHandle), 
        CBlastException);
}

BOOST_AUTO_TEST_CASE(TestComparePssmWithSingleSequence) {
    CConstRef<CBioseq> bioseq(&m_SeqEntry->GetSeq());
    x_SetupSubject(bioseq);

    CPsiBl2Seq blaster(m_Pssm, m_Subject, m_OptHandle);
    CSearchResultSet results(*blaster.Run());
    BOOST_REQUIRE(results[0].GetErrors().empty());

    const size_t kNumExpectedAlignments = 2;
    CConstRef<CSeq_align_set> sas = results[0].GetSeqAlign();

    BOOST_REQUIRE_EQUAL(kNumExpectedAlignments, sas->Size());


    CSeq_align_set::Tdata::const_iterator alignment_itr
        = sas->Get().begin();
    x_ValidatePssmVsGi7450545(*alignment_itr, 1);
    ++alignment_itr;
    x_ValidatePssmVsGi7450545(*alignment_itr, 2);
}
#endif

BOOST_AUTO_TEST_CASE(TestComparePssmWithMultipleSequences) {
    const size_t kNumSubjects = 2;
    CConstRef<CBioseq_set> bioseq_set(&m_SeqSet->GetSet());
    x_SetupSubject(bioseq_set);

    CPsiBl2Seq blaster(m_Pssm, m_Subject, m_OptHandle);
    CSearchResultSet results(*blaster.Run());
    BOOST_REQUIRE(results[0].GetErrors().empty());

    const CBlastOptions& opts = m_OptHandle->GetOptions();
    BOOST_REQUIRE_EQUAL(kNumSubjects,
         (size_t)m_Subject->MakeLocalQueryData(&opts)->GetNumQueries());
    BOOST_REQUIRE_EQUAL(kNumSubjects,
                         results[0].GetSeqAlign()->Get().size());

    const size_t kNumExpectedAlignments = kNumSubjects;
    CConstRef<CSeq_align_set> sas = results[0].GetSeqAlign();
    BOOST_REQUIRE_EQUAL(kNumExpectedAlignments, sas->Get().size());

    CSeq_align_set::Tdata::const_iterator alignment_itr
        = sas->Get().begin();
    x_ValidatePssmVsGi7450545(*alignment_itr, 1);
    ++alignment_itr;
    x_ValidatePssmVsGi7450545(*alignment_itr, 2);


    BOOST_REQUIRE(results[1].GetErrors().empty());
    CConstRef<CSeq_align_set> sas2 = results[1].GetSeqAlign();

    x_ValidatePssmVsGi129295(*(sas2->Get().begin()));
    
}

#if 0
BOOST_AUTO_TEST_CASE(TestComparePssmWithMultipleSequences_OneWithNoResults) {
    CRef<CScope> scope(CSimpleOM::NewScope());

    // Prepare the subjects
    TSeqLocVector subjects;
    {
        int subj_gis[] = { 7450545, 40456275, 129295 };
        for (size_t i = 0; i < sizeof(subj_gis)/sizeof(*subj_gis); i++) {
            CRef<CSeq_loc> subj_loc(new CSeq_loc);
            subj_loc->SetWhole().SetGi(subj_gis[i]);
            subjects.push_back(SSeqLoc(subj_loc, scope));
        }
    }

    // set up the query factories for the subjects
    CRef<IQueryFactory> subj_factory(new CObjMgr_QueryFactory(subjects));

    CPsiBl2Seq blaster(m_Pssm, subj_factory, m_OptHandle);
    CSearchResultSet results(*blaster.Run());
    BOOST_REQUIRE(results[0].GetErrors().empty());

    BOOST_REQUIRE_EQUAL(subjects.size(), results.GetNumResults());

    CConstRef<CSeq_align_set> sas = results[0].GetSeqAlign();
    CSeq_align_set::Tdata::const_iterator alignment_itr
        = sas->Get().begin();
    x_ValidatePssmVsGi7450545(*alignment_itr, 1);
    ++alignment_itr;
    x_ValidatePssmVsGi7450545(*alignment_itr, 2);

    CConstRef<CSeq_align_set> sas2 = results[1].GetSeqAlign();
    
    // REMOVE??? x_ValidatePssmVsGi40456275(*(sas2->Get().begin()));
    BOOST_REQUIRE_EQUAL(0, (int) sas2->Size());

    x_ValidatePssmVsGi129295(*(results[2].GetSeqAlign()->Get().begin()));
    
}

BOOST_AUTO_TEST_CASE(TestComparePsiBl2SeqWithBl2Seq) {
    CRef<CScope> scope(CSimpleOM::NewScope());

    // Prepare the query
    TSeqLocVector query;
    {
        CRef<CSeq_loc> query_loc(new CSeq_loc);
        query_loc->SetWhole().SetGi(7662354);
        query.push_back(SSeqLoc(query_loc, scope));
    }

    // Prepare the subjects
    TSeqLocVector subjects;
    {
        // These gis have hits against the query sequence above
        int subj_gis[] = { 34535770, 46125411 };
        for (size_t i = 0; i < sizeof(subj_gis)/sizeof(*subj_gis); i++) {
            CRef<CSeq_loc> subj_loc(new CSeq_loc);
            subj_loc->SetWhole().SetGi(subj_gis[i]);
            subjects.push_back(SSeqLoc(subj_loc, scope));
        }
    }

    // set up the query factories for CPsiBl2Seq
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(query));
    CRef<IQueryFactory> subj_factory(new CObjMgr_QueryFactory(subjects));

    // Reset composition based statistics for now
    m_OptHandle->SetCompositionBasedStats(eNoCompositionBasedStats);

    // Run BLAST 2 Sequences (objmgr dependent version)
    CBl2Seq bl2seq(query, subjects, *m_OptHandle);
    TSeqAlignVector bl2seq_results = bl2seq.Run();

    // Run BLAST 2 Sequences (objmgr independent version)
    // Configure the options the same way
    CRef<CBlastProteinOptionsHandle>
        psi_opts(dynamic_cast<CBlastProteinOptionsHandle*>(&*m_OptHandle));
    CPsiBl2Seq psibl2seq(query_factory, subj_factory, psi_opts); 
    CSearchResultSet psibl2seq_results = *psibl2seq.Run();

    qa::TSeqAlignSet results_ref;
    qa::TSeqAlignSet results_test;

    qa::SeqAlignSetConvert(*bl2seq_results[0], results_ref); 
    qa::SeqAlignSetConvert(*psibl2seq_results[0].GetSeqAlign(), results_test);

    qa::CSeqAlignCmpOpts opts;
    qa::CSeqAlignCmp cmp(results_ref, results_test, opts);
    string errors;
    bool identical_results = cmp.Run(&errors);

    BOOST_REQUIRE_MESSAGE(identical_results, errors);
}
#endif

BOOST_AUTO_TEST_SUITE_END()

