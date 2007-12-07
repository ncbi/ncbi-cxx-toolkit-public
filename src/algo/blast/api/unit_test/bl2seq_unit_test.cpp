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
 * Authors: Christiam Camacho
 *
 */

/** @file blast_unit_test.cpp
 * Unit tests for the CBl2Seq class
 */

#include <ncbi_pch.hpp>
#include <boost/test/auto_unit_test.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>

#include <algo/blast/api/tblastn_options.hpp>
#include <objtools/blast_format/blastfmtutil.hpp>

#include "test_objmgr.hpp"

#ifdef NCBI_OS_DARWIN
#include <corelib/plugin_manager_store.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <objtools/data_loaders/genbank/processors.hpp>
#endif

#include <common/test_assert.h>  /* This header must go last */

#ifndef BOOST_AUTO_TEST_CASE
#  define BOOST_AUTO_TEST_CASE BOOST_AUTO_UNIT_TEST
#endif

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);

BOOST_AUTO_TEST_CASE(ProteinBlastInvalidSeqIdSelfHit)
{
    CRef<CSeq_loc> loc(new CSeq_loc());
    loc->SetWhole().SetGi(-1);

    CRef<CScope> scope(new CScope(CTestObjMgr::Instance().GetObjMgr()));
    scope->AddDefaults();
    SSeqLoc query(loc, scope);

    TSeqLocVector subjects;
    {
        CRef<CSeq_loc> local_loc(new CSeq_loc());
        local_loc->SetWhole().SetGi(-1);

        CScope* local_scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
        local_scope->AddDefaults();
        subjects.push_back(SSeqLoc(local_loc, local_scope));
    }

    // BLAST by concatenating all queries
    CBl2Seq blaster4all(query, subjects, eBlastp);
    TSeqAlignVector sas_v;
    BOOST_CHECK_THROW(sas_v = blaster4all.Run(), CBlastException);
}

enum EBl2seqTest {
    eBlastp_129295_129295 = 0,
    eBlastn_555_555,
    eMegablast_555_555,
    eDiscMegablast_555_555,
    eBlastx_555_129295,
    eTblastn_129295_555,
    eTblastn_129295_555_large_word,
    eTblastx_555_555,
    eTblastx_many_hits,
    eBlastp_129295_7662354,
    eBlastn_555_3090,
    eBlastp_multi_q,
    eBlastn_multi_q,
    eBlastp_multi_q_s,
    eTblastn_oof,
    eBlastx_oof,
    eDiscMegablast_U02544_U61969,
    eMegablast_chrom_mrna
};

void testRawCutoffs(CBl2Seq& blaster, EProgram program, 
                    EBl2seqTest test_id)
{
    BlastRawCutoffs* raw_cutoffs = 
        blaster.GetDiagnostics()->cutoffs;
    int x_drop_ungapped;
    int gap_trigger;

    if (program == eBlastn || program == eDiscMegablast) {
        x_drop_ungapped = 16;
        gap_trigger = 16;
    } else if (program == eMegablast) {
        x_drop_ungapped = 8;
        gap_trigger = 8;
    } else {
        x_drop_ungapped = 16;
        gap_trigger = 41;
    }

    switch (test_id) {
    case eBlastn_555_3090:
        x_drop_ungapped = 18; 
        gap_trigger = 18;
        break;
    case eBlastn_multi_q:
        x_drop_ungapped = 18; 
        gap_trigger = 18;
        break;
    case eMegablast_chrom_mrna: 
        x_drop_ungapped = 7;
        gap_trigger = 7; 
        break;
    case eDiscMegablast_U02544_U61969:
        x_drop_ungapped = 20; 
        gap_trigger = 20; 
        break;
    case eBlastp_multi_q:
        gap_trigger = 23;
        break;
    case eBlastp_multi_q_s:
        gap_trigger = 19;
        break;
    case eBlastp_129295_129295:
    case eTblastn_129295_555:
    case eTblastn_129295_555_large_word:
        gap_trigger = 20; break;
    case eBlastp_129295_7662354:
        gap_trigger = 23; break;
    case eBlastx_555_129295:
        gap_trigger = 19; break;
    case eTblastn_oof:
        gap_trigger = 43;
    default:
        break;
    }

    switch (program) {
    case eBlastn: case eDiscMegablast:
        BOOST_CHECK_EQUAL(x_drop_ungapped, 
                             raw_cutoffs->x_drop_ungapped);
        BOOST_CHECK_EQUAL(33, raw_cutoffs->x_drop_gap);
        BOOST_CHECK_EQUAL(55, raw_cutoffs->x_drop_gap_final);
        BOOST_CHECK_EQUAL(gap_trigger, raw_cutoffs->ungapped_cutoff);
        break;
    case eMegablast:
        BOOST_CHECK_EQUAL(x_drop_ungapped, 
                             raw_cutoffs->x_drop_ungapped);
        BOOST_CHECK_EQUAL(16, raw_cutoffs->x_drop_gap);
        BOOST_CHECK_EQUAL(27, raw_cutoffs->x_drop_gap_final);
        BOOST_CHECK_EQUAL(gap_trigger, raw_cutoffs->ungapped_cutoff);
        break;
    case eBlastp: case eBlastx: case eTblastn:
        BOOST_CHECK_EQUAL(38, raw_cutoffs->x_drop_gap);
        BOOST_CHECK_EQUAL(64, raw_cutoffs->x_drop_gap_final);
        BOOST_CHECK_EQUAL(gap_trigger, raw_cutoffs->ungapped_cutoff);
        /* No break intentional: next test is valid for all the above 
           programs */
    case eTblastx:
        BOOST_CHECK_EQUAL(x_drop_ungapped,
                             raw_cutoffs->x_drop_ungapped);
        break;
    default: break;
    }
}

void testBlastHitCounts(CBl2Seq& blaster, EBl2seqTest test_id)
{
    BlastUngappedStats* ungapped_stats = 
        blaster.GetDiagnostics()->ungapped_stat;
    BlastGappedStats* gapped_stats = 
        blaster.GetDiagnostics()->gapped_stat;
    
    switch (test_id) {
    case eBlastp_129295_129295:
        BOOST_CHECK_EQUAL(314, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(3, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(1, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(1, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(1, gapped_stats->good_extensions);
        break;
    case eBlastn_555_555:
        BOOST_CHECK_EQUAL(157, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(3, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(3, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(1, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(1, gapped_stats->good_extensions);
        break;
    case eMegablast_555_555:
        BOOST_CHECK_EQUAL(30, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(1, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(1, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(1, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(1, gapped_stats->good_extensions);
        break;
    case eDiscMegablast_555_555:
        BOOST_CHECK_EQUAL(582, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(32, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(32, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(1, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(1, gapped_stats->good_extensions);
        break;
    case eBlastx_555_129295:
        BOOST_CHECK_EQUAL(280, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(3, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(1, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(1, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(1, gapped_stats->good_extensions);
        break;
    case eTblastn_129295_555:
        BOOST_CHECK_EQUAL(157, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(1, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(1, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(1, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(1, gapped_stats->good_extensions);
        break;
    case eTblastn_129295_555_large_word:
        BOOST_CHECK_EQUAL(5, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(4, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(2, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(2, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(2, gapped_stats->good_extensions);
        break;
    case eTblastx_555_555:
        BOOST_CHECK_EQUAL(2590, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(61, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(41, ungapped_stats->good_init_extends);
        break;
    case eTblastx_many_hits:
        BOOST_CHECK_EQUAL(18587, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(362, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(66, ungapped_stats->good_init_extends);
        break;
    case eBlastp_129295_7662354:
        BOOST_CHECK_EQUAL(210, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(10, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(3, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(3, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(3, gapped_stats->good_extensions);
        break;
    case eBlastn_555_3090:
        BOOST_CHECK_EQUAL(15, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(2, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(2, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(2, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(2, gapped_stats->good_extensions);
        break;
    case eBlastp_multi_q:
        BOOST_CHECK_EQUAL(1803, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(56, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(13, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(7, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(7, gapped_stats->good_extensions);
        break;
    case eBlastn_multi_q:
        BOOST_CHECK_EQUAL(963, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(13, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(13, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(5, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(5, gapped_stats->good_extensions);
        break;
    case eBlastp_multi_q_s:
#if 0
        // The following 2 numbers are different in Release and Debug modes
        // due to a minor discrepancy in locations masked by seg filtering.
        // The latter is due to a tiny difference in values involved in 
        // a comparison of real numbers inside the seg algorithm.
        // In Debug mode:
        BOOST_CHECK_EQUAL(3579, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(138, ungapped_stats->init_extends);
        // In Release mode:
        BOOST_CHECK_EQUAL(3580, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(140, ungapped_stats->init_extends);
#endif
        BOOST_CHECK_EQUAL(47, ungapped_stats->good_init_extends); 
        BOOST_CHECK_EQUAL(24, gapped_stats->extensions);  
        BOOST_CHECK_EQUAL(23, gapped_stats->good_extensions);
        break;
    case eTblastn_oof:
        BOOST_CHECK_EQUAL(2666, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(50, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(4, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(2, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(2, gapped_stats->good_extensions);
        break;
    case eBlastx_oof:
        BOOST_CHECK_EQUAL(5950, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(159, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(6, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(2, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(2, gapped_stats->good_extensions);
        break;
    case eDiscMegablast_U02544_U61969:
        BOOST_CHECK_EQUAL(108, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(15, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(15, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(3, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(3, gapped_stats->good_extensions);
        break;
    case eMegablast_chrom_mrna:
        BOOST_CHECK_EQUAL(14, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(1, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(1, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(1, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(1, gapped_stats->good_extensions);
        break;
    default: break;
    }
}

BOOST_AUTO_TEST_CASE(ProteinBlastSelfHit)
{
    //const int kSeqLength = 232;
    CSeq_id id("gi|129295");
    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(id));

    CBl2Seq blaster(*sl, *sl, eBlastp);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_CHECK_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());
    testBlastHitCounts(blaster, eBlastp_129295_129295);
    testRawCutoffs(blaster, eBlastp, eBlastp_129295_129295);

    // the number of identities is NOT calculated when composition based
    // statistics is turned on (default for blastp)
    int num_ident = 0;
    sar->GetNamedScore(CSeq_align::eScore_IdentityCount, num_ident);
#if 0
    ofstream o("0.asn");
    o << MSerial_AsnText << *sar ;
    o.close();
#endif
    BOOST_CHECK_EQUAL(0, num_ident);

    // calculate the number of identities using the BLAST formatter
    double percent_identity = 
        CBlastFormatUtil::GetPercentIdentity(*sar, *sl->scope, false);
    BOOST_CHECK_EQUAL(1, (int) percent_identity);
}

BOOST_AUTO_TEST_CASE(TBlastn2Seqs)
{
    CSeq_id qid("gi|129295");
    auto_ptr<SSeqLoc> query(CTestObjMgr::Instance().CreateSSeqLoc(qid));

    CSeq_id sid("gi|555");
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));

    CBl2Seq blaster(*query, *subj, eTblastn);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_CHECK_EQUAL(1, (int)sar->GetSegs().GetStd().size());
    testBlastHitCounts(blaster, eTblastn_129295_555);
    testRawCutoffs(blaster, eTblastn, eTblastn_129295_555);

    int num_ident = 0;
    sar->GetNamedScore(CSeq_align::eScore_IdentityCount, num_ident);
#if 0
ofstream o("1.asn");
o << MSerial_AsnText << *sar ;
o.close();
#endif
    BOOST_CHECK_EQUAL(5, num_ident);
}

BOOST_AUTO_TEST_CASE(TBlastn2SeqsCompBasedStats)
{
    CSeq_id qid("pir|A01243|DXCH");
    auto_ptr<SSeqLoc> query(CTestObjMgr::Instance().CreateSSeqLoc(qid));

    CSeq_id sid("gi|118086484");
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));

    CTBlastnOptionsHandle opts;
    opts.SetOptions().SetCompositionBasedStats(eCompositionBasedStats);

    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_CHECK_EQUAL(1, (int)sar->GetSegs().GetStd().size());

    int num_ident = 0;
    sar->GetNamedScore(CSeq_align::eScore_IdentityCount, num_ident);
#if 0
ofstream o("2.asn");
o << MSerial_AsnText << *sar ;
o.close();
#endif
    // N.B.: because we used composition based statistics, this field was not
    // calculated as it's not implemented
    BOOST_CHECK_EQUAL(0, num_ident);
}

#if 0
BOOST_AUTO_TEST_CASE(TBlastn2SeqsLargeWord)
{
    CSeq_id qid("gi|129295");
    auto_ptr<SSeqLoc> query(CTestObjMgr::Instance().CreateSSeqLoc(qid));

    CSeq_id sid("gi|555");
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));

    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eTblastn));
    opts->SetOptions().SetWordSize(6);
    opts->SetOptions().SetLookupTableType(eCompressedAaLookupTable);
    opts->SetOptions().SetWordThreshold(21.69);
    opts->SetOptions().SetWindowSize(0);

    CBl2Seq blaster(*query, *subj, *opts);
    TSeqAlignVector sav(blaster.Run());
    BOOST_CHECK_EQUAL(2, (int)sav[0]->Size());
    testBlastHitCounts(blaster, eTblastn_129295_555_large_word);
    testRawCutoffs(blaster, eTblastn, eTblastn_129295_555_large_word);

    int num_ident = 0;
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    sar->GetNamedScore(CSeq_align::eScore_IdentityCount, num_ident);
#if 0
ofstream o("3.asn");
o << MSerial_AsnText << *sar ;
o.close();
#endif
    BOOST_CHECK_EQUAL(5, num_ident);
}
#endif

BOOST_AUTO_TEST_CASE(IdenticalProteins)
{
    //const int kSeqLength = 377;
    CSeq_id qid("gi|34810917");
    auto_ptr<SSeqLoc> query(CTestObjMgr::Instance().CreateSSeqLoc(qid));
    CSeq_id sid("gi|34810916");
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(sid));

    CBl2Seq blaster(*query, *subj, eBlastp);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_CHECK_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());

    // the number of identities is NOT calculated when composition based
    // statistics is turned on (default for blastp)
    int num_ident = 0;
    sar->GetNamedScore(CSeq_align::eScore_IdentityCount, num_ident);
#if 0
    ofstream o("4.asn");
    o << MSerial_AsnText << *sar ;
    o.close();
#endif
    BOOST_CHECK_EQUAL(0, num_ident);

    // calculate the number of identities using the BLAST formatter
    double percent_identity = 
        CBlastFormatUtil::GetPercentIdentity(*sar, *query->scope, false);
    BOOST_CHECK_EQUAL(1, (int) percent_identity);
}

#endif /* SKIP_DOXYGEN_PROCESSING */
