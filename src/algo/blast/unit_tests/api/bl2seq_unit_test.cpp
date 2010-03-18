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
#include <corelib/test_boost.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>

#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <serial/objostr.hpp>

#include <algo/blast/api/tblastn_options.hpp>
#include <algo/blast/format/blastfmtutil.hpp>

#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/blastx_options.hpp>
#include <algo/blast/api/tblastn_options.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include <algo/blast/api/local_blast.hpp>       // for CLocalBlast
#include <algo/blast/api/local_db_adapter.hpp>  // for CLocalDbAdapter
#include <algo/blast/api/objmgr_query_data.hpp> // for CObjMgr_QueryFactory
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>

#include <objtools/simple/simple_om.hpp>        // for CSimpleOM
#include <objtools/readers/fasta.hpp>           // for CFastaReader
#include <objmgr/util/seq_loc_util.hpp>

#include "test_objmgr.hpp"

#ifdef NCBI_OS_DARWIN
#include <corelib/plugin_manager_store.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <objtools/data_loaders/genbank/processors.hpp>
#endif

#include <util/random_gen.hpp>

#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);

BOOST_AUTO_TEST_SUITE(bl2seq)

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

/* The following functions are used to test the functionality to interrupt
 * CBl2Seq runs */

/// Returns true so that the processing stops upon the first invocation of this
/// callback
extern "C" Boolean interrupt_immediately(SBlastProgress* /*progress_info*/)
{
    return TRUE;
}

/// Returns false so that the processing never stops in spite of a callback
/// function to interrupt the process is provided
extern "C" Boolean do_not_interrupt(SBlastProgress* /*progress_info*/)
{
    return FALSE;
}

/// This callback never interrupts the BLAST search, its only purpose is to
/// count the number of times this is invoked for the given input. Also to be
/// used in CBl2SeqTest::testInterruptXExitAtRandom.
extern "C" Boolean callback_counter(SBlastProgress* progress_info)
{
    int& counter = *reinterpret_cast<int*>(progress_info->user_data);
    counter++;
    return FALSE;
}

/// This callback interrupts the BLAST search after the callback has been
/// executed the requested number of times in the pair's second member.
/// This is used in CBl2SeqTest::testInterruptXExitAtRandom.
extern "C" Boolean interrupt_at_random(SBlastProgress* progress_info)
{
    pair<int, int>& progress_pair =
        *reinterpret_cast< pair<int, int>* >(progress_info->user_data);

    if (++progress_pair.first == progress_pair.second) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/// The interruption occurs after 3 invokations of this callback
extern "C" Boolean interrupt_after3calls(SBlastProgress* /*progress_info*/)
{
    static int num_calls = 0;
    if (++num_calls < 3) {
        return FALSE;
    } else {
        return TRUE;
    }
}

/// The interruption occurs after starting the traceback stage
extern "C" Boolean interrupt_on_traceback(SBlastProgress* progress_info)
{
    if (progress_info->stage == eTracebackSearch) {
        return TRUE;
    } else {
        return FALSE;
    }
}

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
        // CC changed 08/07/08
        //BOOST_CHECK_EQUAL(55, raw_cutoffs->x_drop_gap_final);
        BOOST_CHECK_EQUAL(110, raw_cutoffs->x_drop_gap_final);
        BOOST_CHECK_EQUAL(gap_trigger, raw_cutoffs->ungapped_cutoff);
        break;
    case eMegablast:
        BOOST_CHECK_EQUAL(x_drop_ungapped, 
                             raw_cutoffs->x_drop_ungapped);
        BOOST_CHECK_EQUAL(16, raw_cutoffs->x_drop_gap);
        // CC changed 08/07/08
        //BOOST_CHECK_EQUAL(27, raw_cutoffs->x_drop_gap_final);
        BOOST_CHECK_EQUAL(54, raw_cutoffs->x_drop_gap_final);
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

void testResultAlignments(size_t num_queries,
                          size_t num_subjects,
                          TSeqAlignVector result_alnvec)
{
    size_t num_total_alns = num_queries * num_subjects;

    // test the number of resulting alignments
    BOOST_REQUIRE_EQUAL(result_alnvec.size(), num_total_alns);

    // test the correct ordering of resulting alignments
    // (q1 s1 q1 s2 ... q2 s1 q2 s2 ...)

    CConstRef<CSeq_id> id_query, id_prev_query;
    CConstRef<CSeq_id> id_subject;
    vector< CConstRef<CSeq_id> > id_prev_subjects;
    id_prev_subjects.resize(num_subjects);

    bool prev_query_available = false;
    vector<bool> prev_subjects_available(num_subjects, false);

    /* DEBUG OUTPUT
    cerr << "................................................" << endl;
    for (size_t i = 0; i < result_alnvec.size(); i++)
        cerr << "\n<" << i << ">\n"
            << MSerial_AsnText << result_alnvec[i].GetObject() << endl;
    cerr << "................................................" << endl;
    ------------ */

    for (size_t i_query = 0; i_query < num_queries; i_query++)
    {
        prev_query_available = false;
        for (size_t i_subject = 0; i_subject < num_subjects; i_subject++)
        {
            size_t i_lin_index = i_query * num_subjects + i_subject;
            CRef<CSeq_align_set> aln_set = result_alnvec[i_lin_index];

            // test if the alignment set is available (even if empty)
            BOOST_REQUIRE(aln_set.NotNull());

            // if the alignment set is not empty, take the first alignment
            // and see if the ID's are in correct order
            if (aln_set->Get().size() > 0)
            {
                CRef<CSeq_align> aln = aln_set->Get().front();
                id_query.Reset(&(aln->GetSeq_id(0)));
                id_subject.Reset(&(aln->GetSeq_id(1)));

                // check if the query id was the same
                // for the previous subject
                if (i_subject > 0 &&
                    prev_query_available)
                {
                    BOOST_REQUIRE(
                        id_query->Match(
                        id_prev_query.GetObject()));
                }

                // check if the subject id was the same
                // on the same position for the previous query
                if (i_query > 0 &&
                    prev_subjects_available[i_subject])
                {
                    BOOST_REQUIRE(
                        id_subject->Match(
                        id_prev_subjects[i_subject].GetObject()));
                }

                // update the entry in previous subjects vector
                prev_subjects_available[i_subject] = true;
                id_prev_subjects[i_subject] = id_subject;

                // update the previous query entry
                prev_query_available = true;
                id_prev_query = id_query;
            }
        }
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
        // CC changed 08/07/08
        //BOOST_CHECK_EQUAL(32, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(1, ungapped_stats->init_extends);
        // CC changed 08/07/08
        //BOOST_CHECK_EQUAL(32, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(1, ungapped_stats->good_init_extends);
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
        BOOST_CHECK_EQUAL(2129, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(78, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(14, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(8, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(8, gapped_stats->good_extensions);
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
        // Note: Seg is not enabled for this case anymore
        // (changed blastp defaults)
        BOOST_CHECK_EQUAL(3939, (int)ungapped_stats->lookup_hits);
        BOOST_CHECK_EQUAL(159, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(59, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(25, gapped_stats->extensions);
        BOOST_CHECK_EQUAL(24, gapped_stats->good_extensions);
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
        // CC changed 08/07/08
        //BOOST_CHECK_EQUAL(15, ungapped_stats->init_extends);
        //BOOST_CHECK_EQUAL(15, ungapped_stats->good_init_extends);
        BOOST_CHECK_EQUAL(3, ungapped_stats->init_extends);
        BOOST_CHECK_EQUAL(3, ungapped_stats->good_init_extends);
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
    BOOST_REQUIRE(sav[0].NotEmpty());
    BOOST_REQUIRE( !sav[0]->IsEmpty() );
    BOOST_REQUIRE(sav[0]->Get().begin()->NotEmpty());
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
    BOOST_CHECK_EQUAL(232, num_ident);

    // calculate the number of identities using the BLAST formatter
/*
    double percent_identity = 
        CBlastFormatUtil::GetPercentIdentity(*sar, *sl->scope, false);
    BOOST_CHECK_EQUAL(1, (int) percent_identity);
*/

    // Check the ancillary results
    CSearchResultSet::TAncillaryVector ancillary_data;
    blaster.GetAncillaryResults(ancillary_data);
    BOOST_CHECK_EQUAL((size_t)1, ancillary_data.size());
    BOOST_CHECK( ancillary_data.front()->GetGappedKarlinBlk() != NULL );
    BOOST_CHECK( ancillary_data.front()->GetUngappedKarlinBlk() != NULL );
    BOOST_CHECK( ancillary_data.front()->GetSearchSpace() != (Int8)0 );

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

#if 0
    ofstream o("1.asn");
    o << MSerial_AsnText << *sar ;
    o.close();
#endif

    BOOST_CHECK_EQUAL(1, (int)sar->GetSegs().GetStd().size());
    testBlastHitCounts(blaster, eTblastn_129295_555);
    testRawCutoffs(blaster, eTblastn, eTblastn_129295_555);

    int score = 0, comp_adj = 0;
    sar->GetNamedScore(CSeq_align::eScore_Score, score);
    sar->GetNamedScore(CSeq_align::eScore_CompAdjMethod, comp_adj);
    BOOST_CHECK_EQUAL(26, score);
    BOOST_CHECK_EQUAL(2, comp_adj);

    // Check the ancillary results
    CSearchResultSet::TAncillaryVector ancillary_data;
    blaster.GetAncillaryResults(ancillary_data);
    BOOST_CHECK_EQUAL((size_t)1, ancillary_data.size());
    BOOST_REQUIRE( ancillary_data.front().NotEmpty() );
    BOOST_CHECK( ancillary_data.front()->GetGappedKarlinBlk() != NULL );
    BOOST_CHECK( ancillary_data.front()->GetUngappedKarlinBlk() != NULL );
    BOOST_CHECK( ancillary_data.front()->GetSearchSpace() != (Int8)0 );
}

BOOST_AUTO_TEST_CASE(TBlastn2SeqsRevStrand1)
{
    CSeq_id qid("gi|1945390");
    auto_ptr<SSeqLoc> query(CTestObjMgr::Instance().CreateSSeqLoc(qid));

    pair<TSeqPos, TSeqPos> range(150000, 170000);
    CSeq_id sid("gi|4755212");
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(sid, range, eNa_strand_minus));

    CBl2Seq blaster(*query, *subj, eTblastn);
    TSeqAlignVector sav(blaster.Run());
    BOOST_CHECK_EQUAL(12, (int) sav[0]->Get().size());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_CHECK_EQUAL(1, (int)sar->GetSegs().GetStd().size());
    vector < CRef< CSeq_loc > > locs = sar->GetSegs().GetStd().front()->GetLoc();
    BOOST_CHECK_EQUAL(eNa_strand_minus, (int) (locs[1])->GetStrand());
    int num_ident = 0;
    sar->GetNamedScore(CSeq_align::eScore_IdentityCount, num_ident);
    BOOST_CHECK_EQUAL(155, num_ident);
#if 0
ofstream o("minus1.asn");
o << MSerial_AsnText << *sar ;
o.close();
#endif
}

BOOST_AUTO_TEST_CASE(TBlastn2SeqsRevStrand2)
{
    CSeq_id qid("gi|1945390");
    auto_ptr<SSeqLoc> query(CTestObjMgr::Instance().CreateSSeqLoc(qid));

    CSeq_id sid("gi|1945388");
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_minus));

    CBl2Seq blaster(*query, *subj, eTblastn);
    TSeqAlignVector sav(blaster.Run());
    BOOST_CHECK_EQUAL(2, (int) sav[0]->Get().size());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_CHECK_EQUAL(1, (int)sar->GetSegs().GetStd().size());
    vector < CRef< CSeq_loc > > locs = sar->GetSegs().GetStd().front()->GetLoc();
    BOOST_CHECK_EQUAL(eNa_strand_minus, (int) (locs[1])->GetStrand());
    int num_ident = 0;
    sar->GetNamedScore(CSeq_align::eScore_IdentityCount, num_ident);
    BOOST_CHECK_EQUAL(11, num_ident);
#if 0
ofstream o("minus2.asn");
o << MSerial_AsnText << *sar ;
o.close();
#endif
}


BOOST_AUTO_TEST_CASE(TBlastn2SeqsCompBasedStats)
{
    CSeq_id qid("gi|68737"); // "pir|A01243|DXCH"
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
    BOOST_CHECK_EQUAL(229, num_ident);
#if 0
ofstream o("2.asn");
o << MSerial_AsnText << *sar ;
o.close();
#endif

    // Check the ancillary results
    CSearchResultSet::TAncillaryVector ancillary_data;
    blaster.GetAncillaryResults(ancillary_data);
    BOOST_CHECK_EQUAL((size_t)1, ancillary_data.size());
    BOOST_CHECK( ancillary_data.front()->GetGappedKarlinBlk() != NULL );
    BOOST_CHECK( ancillary_data.front()->GetUngappedKarlinBlk() != NULL );
    BOOST_CHECK( ancillary_data.front()->GetSearchSpace() != (Int8)0 );
}

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
    opts->SetOptions().SetCompositionBasedStats(eNoCompositionBasedStats);

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
    BOOST_CHECK_EQUAL(377, num_ident);

    // calculate the number of identities using the BLAST formatter
/*
    double percent_identity = 
        CBlastFormatUtil::GetPercentIdentity(*sar, *query->scope, false);
    BOOST_CHECK_EQUAL(1, (int) percent_identity);
*/

    // Check the ancillary results
    CSearchResultSet::TAncillaryVector ancillary_data;
    blaster.GetAncillaryResults(ancillary_data);
    BOOST_CHECK_EQUAL((size_t)1, ancillary_data.size());
    BOOST_CHECK( ancillary_data.front()->GetGappedKarlinBlk() != NULL );
    BOOST_CHECK( ancillary_data.front()->GetUngappedKarlinBlk() != NULL );
    BOOST_CHECK( ancillary_data.front()->GetSearchSpace() != (Int8)0 );
}

BOOST_AUTO_TEST_CASE(UnsupportedOption) {
    CDiscNucleotideOptionsHandle opts_handle;
    BOOST_REQUIRE_THROW(opts_handle.SetTraditionalBlastnDefaults(),
                        CBlastException);
}

BOOST_AUTO_TEST_CASE(PositiveMismatchOption) {
    CSeq_id qid("gi|408478");  // zebrafish sequence U02544
    CSeq_id sid("gi|1546012"); // mouse sequence U61969

    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(sid));

    const int kMatch = 2;
    const int kMismatch = 5;  // Positive mismatch not allowed.

    CBlastNucleotideOptionsHandle nucl_options_handle;

    nucl_options_handle.SetMatchReward(kMatch);
    nucl_options_handle.SetMismatchPenalty(kMismatch);
    CBl2Seq blaster(*query, *subj, nucl_options_handle);
    try {
       TSeqAlignVector sav(blaster.Run());
    } catch (const CException& e) {
        BOOST_REQUIRE(
            !strcmp("BLASTN penalty must be negative",  
                    e.GetMsg().c_str()));
    }
}

BOOST_AUTO_TEST_CASE(FullyMaskedSequence) {
    CSeq_id qid("ref|NT_024524.13");
    pair<TSeqPos, TSeqPos> range(27886902, 27886932);
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, range, 
                                               eNa_strand_plus));
    range.first = 2052;
    range.second = 2082;
    CSeq_id sid("emb|BX641126.1");
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range,
                                               eNa_strand_minus));
    CBlastNucleotideOptionsHandle options;
    options.SetTraditionalBlastnDefaults();
    options.SetMismatchPenalty(-1);
    options.SetMatchReward(1);
    options.SetGapXDropoff(100);
    options.SetMaskAtHash(false);
    CBl2Seq blaster(*query, *subj, options);
    try { blaster.Run(); }
    catch (const CException& e) {
        const string msg1("invalid query sequence");
        const string msg2("verify the query sequence(s) and/or filtering "
                          "options");
        BOOST_REQUIRE(string(e.what()).find(msg1) != NPOS);
        BOOST_REQUIRE(string(e.what()).find(msg2) != NPOS);
    }
}

BOOST_AUTO_TEST_CASE(testInterruptBlastpExitImmediately) {
    CSeq_id id("gi|129295");
    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(id));

    CBl2Seq blaster(*sl, *sl, eBlastp);
    TInterruptFnPtr fnptr =
        blaster.SetInterruptCallback(interrupt_immediately);
    BOOST_REQUIRE(fnptr == NULL);

    TSeqAlignVector sav;
    try { sav = blaster.Run(); }
    catch (...) {
        BOOST_REQUIRE_EQUAL((size_t)0, sav.size());
    }
}

BOOST_AUTO_TEST_CASE(testInterruptBlastnExitImmediately) {
    CSeq_id id("gi|555");
    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(id));

    CBl2Seq blaster(*sl, *sl, eBlastn);
    TInterruptFnPtr fnptr =
        blaster.SetInterruptCallback(interrupt_immediately);
    BOOST_REQUIRE(fnptr == NULL);

    TSeqAlignVector sav;
    try { sav = blaster.Run(); }
    catch (...) {
        BOOST_REQUIRE_EQUAL((size_t)0, sav.size());
    }
}

BOOST_AUTO_TEST_CASE(testInterruptBlastxExitImmediately) {
    CSeq_id query_id("gi|555");
    auto_ptr<SSeqLoc> slq(CTestObjMgr::Instance().CreateSSeqLoc(query_id));
    CSeq_id subj_id("gi|129295");
    auto_ptr<SSeqLoc> sls(CTestObjMgr::Instance().CreateSSeqLoc(subj_id));

    CBl2Seq blaster(*slq, *sls, eBlastx);
    TInterruptFnPtr fnptr =
        blaster.SetInterruptCallback(interrupt_immediately);
    BOOST_REQUIRE(fnptr == NULL);

    TSeqAlignVector sav;
    try { sav = blaster.Run(); }
    catch (...) {
        BOOST_REQUIRE_EQUAL((size_t)0, sav.size());
    }
}

BOOST_AUTO_TEST_CASE(testInterruptTblastxExitImmediately) {
    CSeq_id query_id("gi|555");
    auto_ptr<SSeqLoc> slq(CTestObjMgr::Instance().CreateSSeqLoc(query_id));
    CSeq_id subj_id("gi|555");
    auto_ptr<SSeqLoc> sls(CTestObjMgr::Instance().CreateSSeqLoc(subj_id));

    CBl2Seq blaster(*slq, *sls, eTblastx);
    TInterruptFnPtr fnptr =
        blaster.SetInterruptCallback(interrupt_immediately);
    BOOST_REQUIRE(fnptr == NULL);

    TSeqAlignVector sav;
    try { sav = blaster.Run(); }
    catch (...) {
        BOOST_REQUIRE_EQUAL((size_t)0, sav.size());
    }
}

BOOST_AUTO_TEST_CASE(testInterruptTblastnExitImmediately) {
    CSeq_id query_id("gi|129295");
    auto_ptr<SSeqLoc> slq(CTestObjMgr::Instance().CreateSSeqLoc(query_id));
    CSeq_id subj_id("gi|555");
    auto_ptr<SSeqLoc> sls(CTestObjMgr::Instance().CreateSSeqLoc(subj_id));

    CBl2Seq blaster(*slq, *sls, eTblastn);
    TInterruptFnPtr fnptr =
        blaster.SetInterruptCallback(interrupt_immediately);
    BOOST_REQUIRE(fnptr == NULL);

    TSeqAlignVector sav;
    try { sav = blaster.Run(); }
    catch (...) {
        BOOST_REQUIRE_EQUAL((size_t)0, sav.size());
    }
}

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*a))
static
CRef<CBl2Seq> s_SetupWithMultipleQueriesAndSubjects(bool query_is_nucl,
                                                    bool subj_is_nucl,
                                                    EProgram program) {

    int protein_gis[] = { 6, 129295, 15606659, 4336138, 5556 };
    int nucl_gis[] = { 272208, 272217, 272211, 272247, 272227, 272236, 
        272219 };

    vector<int> q_gis, s_gis;
    if (query_is_nucl) {
        copy(&nucl_gis[0],
             &nucl_gis[ARRAY_SIZE(nucl_gis)],
             back_inserter(q_gis));
    } else {
        copy(&protein_gis[0],
             &protein_gis[ARRAY_SIZE(protein_gis)],
             back_inserter(q_gis));
    }

    if (subj_is_nucl) {
        copy(&nucl_gis[0],
             &nucl_gis[ARRAY_SIZE(nucl_gis)],
             back_inserter(s_gis));
    } else {
        copy(&protein_gis[0],
             &protein_gis[ARRAY_SIZE(protein_gis)],
             back_inserter(s_gis));
    }


    TSeqLocVector queries;
    ITERATE(vector<int>, itr, q_gis) {
        CRef<CSeq_loc> loc(new CSeq_loc());
        loc->SetWhole().SetGi(*itr);

        CScope* scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
        scope->AddDefaults();
        queries.push_back(SSeqLoc(loc, scope));
    }

    TSeqLocVector subjects;
    ITERATE(vector<int>, itr, s_gis) {
        CRef<CSeq_loc> loc(new CSeq_loc());
        loc->SetWhole().SetGi(*itr);

        CScope* scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
        scope->AddDefaults();
        subjects.push_back(SSeqLoc(loc, scope));
    }

    return CRef<CBl2Seq>(new CBl2Seq(queries, subjects, program));
}

BOOST_AUTO_TEST_CASE(testInterruptBlastpExitAtRandom) {

    CRef<CBl2Seq> blaster = s_SetupWithMultipleQueriesAndSubjects(false,
                                                                  false,
                                                                  eBlastp);

    int num_callbacks_executed(0);
    TInterruptFnPtr fnptr =
        blaster->SetInterruptCallback(callback_counter, 
                                      (void*) &num_callbacks_executed);
    BOOST_REQUIRE(fnptr == NULL);

    TSeqAlignVector sav(blaster->Run()); // won't throw
    CRandom r(time(0));
    int max_interrupt_callbacks = r.GetRand(1, num_callbacks_executed);
    pair<int, int> progress_pair(make_pair(0, max_interrupt_callbacks));

    fnptr = blaster->SetInterruptCallback(interrupt_at_random,
                                          (void*)&progress_pair);
    BOOST_REQUIRE(fnptr == callback_counter);
    sav.clear();

    try { sav = blaster->Run(); }
    catch (...) {
        BOOST_REQUIRE_EQUAL((size_t)0, sav.size());
    }
}

BOOST_AUTO_TEST_CASE(testInterruptBlastnExitAtRandom) {

    CRef<CBl2Seq> blaster =
        s_SetupWithMultipleQueriesAndSubjects(true, true, eBlastn);

    int num_callbacks_executed(0);
    TInterruptFnPtr fnptr =
        blaster->SetInterruptCallback(callback_counter,
                                      (void*)&num_callbacks_executed);
    BOOST_REQUIRE(fnptr == NULL);

    TSeqAlignVector sav(blaster->Run()); // won't throw
    CRandom r(time(0));
    int max_interrupt_callbacks = r.GetRand(1, num_callbacks_executed);
    pair<int, int> progress_pair(make_pair(0, max_interrupt_callbacks));

    fnptr = blaster->SetInterruptCallback(interrupt_at_random,
                                          (void*)&progress_pair);
    BOOST_REQUIRE(fnptr == callback_counter);
    sav.clear();

    try { sav = blaster->Run(); }
    catch (...) {
        BOOST_REQUIRE_EQUAL((size_t)0, sav.size());
    }
}

// interrupt_at_random.
BOOST_AUTO_TEST_CASE(testInterruptBlastxExitAtRandom) {

    CRef<CBl2Seq> blaster =
        s_SetupWithMultipleQueriesAndSubjects(true, false, eBlastx);

    int num_callbacks_executed(0);
    TInterruptFnPtr fnptr =
        blaster->SetInterruptCallback(callback_counter,
                                      (void*) & num_callbacks_executed);
    BOOST_REQUIRE(fnptr == NULL);

    TSeqAlignVector sav(blaster->Run()); // won't throw
    CRandom r(time(0));
    int max_interrupt_callbacks = r.GetRand(1, num_callbacks_executed);
    pair<int, int> progress_pair(make_pair(0, max_interrupt_callbacks));

    fnptr = blaster->SetInterruptCallback(interrupt_at_random,
                                          (void*)&progress_pair);
    BOOST_REQUIRE(fnptr == callback_counter);
    sav.clear();

    try { sav = blaster->Run(); }
    catch (...) {
        BOOST_REQUIRE_EQUAL((size_t)0, sav.size());
    }
}

BOOST_AUTO_TEST_CASE(testInterruptTblastnExitAtRandom) {

    CRef<CBl2Seq> blaster =
        s_SetupWithMultipleQueriesAndSubjects(false, true, eTblastn);

    int num_callbacks_executed(0);
    TInterruptFnPtr fnptr =
        blaster->SetInterruptCallback(callback_counter,
                                      (void*)&num_callbacks_executed);
    BOOST_REQUIRE(fnptr == NULL);

    TSeqAlignVector sav(blaster->Run()); // won't throw
    CRandom r(time(0));
    int max_interrupt_callbacks = r.GetRand(1, num_callbacks_executed);
    pair<int, int> progress_pair(make_pair(0, max_interrupt_callbacks));

    fnptr = blaster->SetInterruptCallback(interrupt_at_random,
                                          (void*)&progress_pair);
    BOOST_REQUIRE(fnptr == callback_counter);
    sav.clear();

    try { sav = blaster->Run(); }
    catch (...) {
        BOOST_REQUIRE_EQUAL((size_t)0, sav.size());
    }
}

BOOST_AUTO_TEST_CASE(testInterruptTblastxExitAtRandom) {

    CRef<CBl2Seq> blaster =
        s_SetupWithMultipleQueriesAndSubjects(true, true, eTblastx);

    int num_callbacks_executed(0);
    TInterruptFnPtr fnptr =
        blaster->SetInterruptCallback(callback_counter,
                                      (void*) & num_callbacks_executed);
    BOOST_REQUIRE(fnptr == NULL);

    TSeqAlignVector sav(blaster->Run()); // won't throw
    CRandom r(time(0));
    int max_interrupt_callbacks = r.GetRand(1, num_callbacks_executed);
    pair<int, int> progress_pair(make_pair(0, max_interrupt_callbacks));

    fnptr = blaster->SetInterruptCallback(interrupt_at_random,
                                          (void*)&progress_pair);
    BOOST_REQUIRE(fnptr == callback_counter);
    sav.clear();

    try { sav = blaster->Run(); }
    catch (...) {
        BOOST_REQUIRE_EQUAL((size_t)0, sav.size());
    }
}

BOOST_AUTO_TEST_CASE(testInterruptBlastpExitAfter3Callbacks) {
    CSeq_id id("gi|129295");
    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(id));

    CBl2Seq blaster(*sl, *sl, eBlastp);
    TInterruptFnPtr fnptr =
        blaster.SetInterruptCallback(interrupt_after3calls);
    BOOST_REQUIRE(fnptr == NULL);

    TSeqAlignVector sav;
    try { sav = blaster.Run(); }
    catch (...) {
        BOOST_REQUIRE_EQUAL((size_t)0, sav.size());
    }
}

BOOST_AUTO_TEST_CASE(testInterruptBlastxExitOnTraceback) {

    CRef<CBl2Seq> blaster = s_SetupWithMultipleQueriesAndSubjects(true,
                                                                  false,
                                                                  eBlastx);
    TInterruptFnPtr fnptr =
        blaster->SetInterruptCallback(interrupt_on_traceback);
    BOOST_REQUIRE(fnptr == NULL);

    TSeqAlignVector sav;
    try { sav = blaster->Run(); }
    catch (...) {
        BOOST_REQUIRE_EQUAL((size_t)0, sav.size());
    }
}

BOOST_AUTO_TEST_CASE(testInterruptTblastxExitOnTraceback) {

    CRef<CBl2Seq> blaster = s_SetupWithMultipleQueriesAndSubjects
        (true, true, eTblastx);
    TInterruptFnPtr fnptr =
        blaster->SetInterruptCallback(interrupt_on_traceback);
    BOOST_REQUIRE(fnptr == NULL);

    TSeqAlignVector sav;
    try { sav = blaster->Run(); }
    catch (...) {
        BOOST_REQUIRE_EQUAL((size_t)0, sav.size());
    }
}

BOOST_AUTO_TEST_CASE(ProteinBlastMultipleQueriesWithInvalidSeqId) {
    vector<int> q_gis, s_gis;

    // Setup the queries
    q_gis.push_back(129295);
    q_gis.push_back(-1);        // invalid seqid

    // setup the subjects
    s_gis.push_back(129295);
    s_gis.push_back(4336138);   // no hits with gi 129295

    TSeqLocVector queries;
    ITERATE(vector<int>, itr, q_gis) {
        CRef<CSeq_loc> loc(new CSeq_loc());
        loc->SetWhole().SetGi(*itr);

        CScope* scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
        scope->AddDefaults();
        queries.push_back(SSeqLoc(loc, scope));
    }

    TSeqLocVector subjects;
    ITERATE(vector<int>, itr, s_gis) {
        CRef<CSeq_loc> loc(new CSeq_loc());
        loc->SetWhole().SetGi(*itr);

        CScope* scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
        scope->AddDefaults();
        subjects.push_back(SSeqLoc(loc, scope));
    }

    // BLAST by concatenating all queries
    CBl2Seq blaster4all(queries, subjects, eBlastp);
    TSeqAlignVector sas_v = blaster4all.Run(); 

    TSearchMessages m;
    blaster4all.GetMessages(m);
    BOOST_REQUIRE_EQUAL(subjects.size()*queries.size(), sas_v.size());
    BOOST_REQUIRE_EQUAL(queries.size(), m.size()); 

    BOOST_REQUIRE(m[0].empty());
    BOOST_REQUIRE(!m[1].empty());

    // Verify the error message
    TQueryMessages qm = m[1];
    BOOST_REQUIRE(qm.front()->GetMessage().find("Cannot resolve") !=
                   string::npos);

    // Verify that the alignments corresponding to the 2nd query are indeed empty
    // in older version this was sas_v[1], order has changed
    BOOST_REQUIRE_EQUAL(0, (int) sas_v[2]->Size());
}

BOOST_AUTO_TEST_CASE(NucleotideBlastMultipleQueriesWithInvalidSeqId) {
    CSeq_id id1(CSeq_id::e_Gi, 555);
    auto_ptr<SSeqLoc> sl1(CTestObjMgr::Instance().CreateSSeqLoc(id1));
    CSeq_id id2(CSeq_id::e_Gi, 556);
    auto_ptr<SSeqLoc> sl2(CTestObjMgr::Instance().CreateSSeqLoc(id2));

    const TSeqPos kFakeBioseqLength = 12;
    const char byte(0);   // string of 4 A's in ncbi2na
    vector<char> na_data(kFakeBioseqLength/4, byte);

    CRef<CSeq_id> fake_id(new CSeq_id("lcl|77"));
    CBioseq fake_bioseq;
    fake_bioseq.SetInst().SetLength(kFakeBioseqLength);
    fake_bioseq.SetInst().SetSeq_data().SetNcbi2na().Set().swap(na_data);
    fake_bioseq.SetInst().SetMol(CSeq_inst::eMol_na);
    fake_bioseq.SetInst().SetRepr(CSeq_inst::eRepr_raw);
    fake_bioseq.SetId().push_back(fake_id);
    CRef<CSeq_loc> fake_loc(new CSeq_loc);
    fake_loc->SetWhole(*fake_id);

    CRef<CScope> scope(CSimpleOM::NewScope(false));
    scope->AddBioseq(fake_bioseq);
    auto_ptr<SSeqLoc> sl_bad(new SSeqLoc(*fake_loc, *scope));

    TSeqPos len = sequence::GetLength(*sl_bad->seqloc, sl_bad->scope);
    BOOST_REQUIRE_EQUAL(kFakeBioseqLength, len);

    TSeqLocVector queries;
    queries.push_back(*sl1);
    queries.push_back(*sl_bad);
    queries.push_back(*sl2);

    // All subjects have matches against this gi
    CSeq_id subj_id(CSeq_id::e_Gi, 555);
    auto_ptr<SSeqLoc> subj_loc
        (CTestObjMgr::Instance().CreateSSeqLoc(subj_id));
    TSeqLocVector subject;
    subject.push_back(*subj_loc);;
    
    CBlastNucleotideOptionsHandle opts_handle;
    opts_handle.SetMaskAtHash(false);
    CBl2Seq bl2seq(queries, subject, opts_handle);
    TSeqAlignVector sas_v = bl2seq.Run(); 
    TSearchMessages m;
    bl2seq.GetMessages(m);
    BOOST_REQUIRE_EQUAL(sas_v.size(), m.size());
    BOOST_REQUIRE_EQUAL(queries.size(), sas_v.size());

    BOOST_REQUIRE(m[0].empty());
    BOOST_REQUIRE(!m[1].empty());
    BOOST_REQUIRE(m[2].empty());

    TQueryMessages qm = m[1];

    // no duplicate messages for the contexts
    BOOST_REQUIRE(qm.size() == 1); 

    // Verify the error message
    ITERATE(TQueryMessages, itr, qm) {
        BOOST_REQUIRE((*itr)->GetMessage().find("Could not calculate "
                                                 "ungapped Karlin-Altschul "
                                                 "parameters") 
                       != string::npos);
    }

    // Verify that the alignments corresponding to the 2nd query are indeed
    // empty
    ITERATE(CSeq_align_set::Tdata, alignments, sas_v[1]->Get()) {
        BOOST_REQUIRE((*alignments)->GetSegs().IsDisc());
        BOOST_REQUIRE((*alignments)->GetSegs().GetDisc().Get().empty());
    }
}

BOOST_AUTO_TEST_CASE(ProteinSelfHitWithMask) {
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 129295));
    CRef<CSeq_loc> sl(new CSeq_loc());
    sl->SetWhole(*id);
    CRef<CSeq_loc> mask(new CSeq_loc(*id, 50, 100));
    CRef<CScope> scope(CSimpleOM::NewScope());
    SSeqLoc seqloc(sl, scope, mask);

    CBl2Seq bl2seq(seqloc, seqloc, eBlastp);
    TSeqAlignVector sav(bl2seq.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());
}

// Inspired by SB-285
BOOST_AUTO_TEST_CASE(NucleotideMaskedLocation) {
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 83219349));
    CRef<CSeq_loc> sl(new CSeq_loc());
    sl->SetWhole(*id);
    CRef<CSeq_loc> mask(new CSeq_loc(*id, 57, 484));
    CRef<CScope> scope(CSimpleOM::NewScope());
    SSeqLoc query_seqloc(sl, scope, mask);

    CRef<CSeq_id> sid(new CSeq_id(CSeq_id::e_Gi, 88954065));
    CRef<CSeq_loc> ssl(new CSeq_loc(*sid, 9909580-100, 9909607+100));
    SSeqLoc subj_seqloc(ssl, scope);

    CBl2Seq bl2seq(query_seqloc, subj_seqloc, eMegablast);
    TSeqAlignVector sav(bl2seq.Run());
    BOOST_REQUIRE_EQUAL(0, sav[0]->Get().size());
}

// Inspired by SB-285
BOOST_AUTO_TEST_CASE(NucleotideMaskedLocation_FromFile) {
    CNcbiIfstream infile("data/masked.fsa");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetLowercaseMask(true);
    CRef<CBlastFastaInputSource> fasta_src
        (new CBlastFastaInputSource(infile, iconfig));
    CRef<CBlastInput> input(new CBlastInput(&*fasta_src));
    //CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    //scope->AddDefaults();
    CRef<CScope> scope = CBlastScopeSource(is_protein).NewScope();

    CRef<blast::CBlastQueryVector> seqs = input->GetNextSeqBatch(*scope);
    CRef<IQueryFactory> queries(new CObjMgr_QueryFactory(*seqs));

    TSeqLocVector subj_vec;
    CRef<CSeq_id> sid(new CSeq_id(CSeq_id::e_Gi, 88954065));
    CRef<CSeq_loc> ssl(new CSeq_loc(*sid, 9909580-100, 9909607+100));
    subj_vec.push_back(SSeqLoc(ssl, scope));
    CRef<IQueryFactory> subj_qf(new CObjMgr_QueryFactory(subj_vec));
    CRef<CBlastOptionsHandle>
        opts_handle(CBlastOptionsFactory::Create(eBlastn));
    CRef<CLocalDbAdapter> subjects(new CLocalDbAdapter(subj_qf,
                                                       opts_handle));

    size_t num_queries = seqs->Size();
    size_t num_subjects = subj_vec.size();
    BOOST_REQUIRE_EQUAL((size_t)1, num_queries);
    BOOST_REQUIRE_EQUAL((size_t)1, num_subjects);

    // BLAST by concatenating all queries
    CLocalBlast blaster(queries, opts_handle, subjects);
    CRef<CSearchResultSet> results = blaster.Run();
    BOOST_REQUIRE(results->GetResultType() == eSequenceComparison);
    BOOST_REQUIRE_EQUAL((num_queries*num_subjects),
                        results->GetNumResults());
    BOOST_REQUIRE_EQUAL((num_queries*num_subjects), results->size());
    BOOST_REQUIRE_EQUAL(num_queries, results->GetNumQueries());
    BOOST_REQUIRE_EQUAL(num_subjects,
                        results->GetNumResults()/results->GetNumQueries());

    CSearchResults& res = (*results)[0];
    BOOST_REQUIRE(res.HasAlignments() == false);
}

// test for the case where the use of composition based
// satistics should have deleted a hit but did not (used to crash)
BOOST_AUTO_TEST_CASE(ProteinCompBasedStats) {

    CRef<CObjectManager> kObjMgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*kObjMgr));
    CRef<CSeq_entry> seq_entry1;
    const string kFileName("data/blastp_compstats.fa");
    ifstream in1(kFileName.c_str());
    if ( !in1 )
        throw runtime_error("Failed to open " + kFileName);
    if ( !(seq_entry1 = CFastaReader(in1).ReadOneSeq()))
        throw runtime_error("Failed to read sequence from " + kFileName);
    scope->AddTopLevelSeqEntry(*seq_entry1);
    CRef<CSeq_loc> seqloc1(new CSeq_loc);
    const string kSeqIdString1("lcl|1");
    CRef<CSeq_id> id1(new CSeq_id(kSeqIdString1));
    seqloc1->SetWhole(*id1);
    SSeqLoc ss1(seqloc1, scope);

    CSeq_id id("gi|4503637");
    auto_ptr<SSeqLoc> ss2(CTestObjMgr::Instance().CreateSSeqLoc(id));

    CBlastProteinOptionsHandle opts_handle;
    opts_handle.SetWordSize(2);
    opts_handle.SetEvalueThreshold(20000);
    opts_handle.SetFilterString("F");/* NCBI_FAKE_WARNING */
    opts_handle.SetMatrixName("PAM30");
    opts_handle.SetGapOpeningCost(9);
    opts_handle.SetGapExtensionCost(1);
    opts_handle.SetOptions().SetCompositionBasedStats(
                                          eCompositionBasedStats);

    CBl2Seq blaster(ss1, *ss2, opts_handle);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());
}

BOOST_AUTO_TEST_CASE(Blastx2Seqs_QueryBothStrands) {
    CSeq_id qid("gi|555");
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));
    query->genetic_code_id = 1;

    CSeq_id sid("gi|129295");
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(sid));

    CBl2Seq blaster(*query, *subj, eBlastx);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetStd().size());
    testBlastHitCounts(blaster, eBlastx_555_129295);
    testRawCutoffs(blaster, eBlastx, eBlastx_555_129295);
}

BOOST_AUTO_TEST_CASE(NucleotideSelfHitWithSubjectMask) {
    CRef<CSeq_id> query_id(new CSeq_id(CSeq_id::e_Gi, 148727250));
    CRef<CSeq_id> subj_id(new CSeq_id(CSeq_id::e_Gi, 89059606));
    CRef<CSeq_loc> qsl(new CSeq_loc(*query_id, 0, 1000));
    CRef<CSeq_loc> ssl(new CSeq_loc(*subj_id, 0, 1000));
    CPacked_seqint::TRanges mask_vector;
    mask_vector.push_back(TSeqRange(0, 44));
    mask_vector.push_back(TSeqRange(69, 582));
    mask_vector.push_back(TSeqRange(610, 834));
    mask_vector.push_back(TSeqRange(854, 1000));
    CRef<CPacked_seqint> masks(new CPacked_seqint(*subj_id,
                                                  mask_vector));
    CRef<CSeq_loc> subj_mask(new CSeq_loc());
    subj_mask->SetPacked_int(*masks);
    CRef<CScope> scope(CSimpleOM::NewScope());
    SSeqLoc query(qsl, scope);
    auto_ptr<SSeqLoc> subject(new SSeqLoc(ssl, scope, subj_mask));
    {
        CBl2Seq bl2seq(query, *subject, eBlastn);
        TSeqAlignVector sav(bl2seq.Run());
        BOOST_REQUIRE_EQUAL((size_t)1, sav.front()->Get().size());
    }

    // Now compare the same sequences, without the subject masks
    subject.reset(new SSeqLoc(ssl, scope));
    {
        CBl2Seq bl2seq(query, *subject, eBlastn);
        TSeqAlignVector sav(bl2seq.Run());
        BOOST_REQUIRE_EQUAL((size_t)4, sav.front()->Get().size());
    }
}

BOOST_AUTO_TEST_CASE(NucleotideBlastSelfHit) {
    CSeq_id id("gi|555");
    auto_ptr<SSeqLoc> sl(
        CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_both));

    // Traditional blastn search
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastn));
    CBl2Seq blaster(*sl, *sl, *opts);
    TSeqAlignVector sav = blaster.Run();
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());
    testBlastHitCounts(blaster, eBlastn_555_555);
    testRawCutoffs(blaster, eBlastn, eBlastn_555_555);

    // Change the options to megablast
    opts.Reset(CBlastOptionsFactory::Create(eMegablast));
    blaster.SetOptionsHandle() = *opts;
    sav = blaster.Run();
    BOOST_REQUIRE_EQUAL(1, (int)sav.size());
    sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());
    testBlastHitCounts(blaster, eMegablast_555_555);
    testRawCutoffs(blaster, eMegablast, eMegablast_555_555);

    // Change the options to discontiguous megablast
    opts.Reset(CBlastOptionsFactory::Create(eDiscMegablast));
    blaster.SetOptionsHandle() = *opts;
    sav = blaster.Run();
    sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());
    testBlastHitCounts(blaster, eDiscMegablast_555_555);
    testRawCutoffs(blaster, eDiscMegablast, eDiscMegablast_555_555);
}

BOOST_AUTO_TEST_CASE(MegablastGreedyTraceback) {
    CSeq_id query_id("gi|2655203");
    auto_ptr<SSeqLoc> ql(
        CTestObjMgr::Instance().CreateSSeqLoc(query_id, 
                                              eNa_strand_plus));

    CSeq_id subject_id("gi|200811");
    auto_ptr<SSeqLoc> sl(
        CTestObjMgr::Instance().CreateSSeqLoc(subject_id, 
                                              eNa_strand_minus));

    // test a fix for a bug that corrupted the traceback
    // in the one hit returned from this search

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalMegablastDefaults();
    opts.SetMatchReward(1);
    opts.SetMismatchPenalty(-2);
    opts.SetGapOpeningCost(3);
    opts.SetGapExtensionCost(1);
    opts.SetWordSize(24);
    opts.SetGapExtnAlgorithm(eGreedyScoreOnly);
    opts.SetGapTracebackAlgorithm(eGreedyTbck);

    CBl2Seq blaster(*ql, *sl, opts);
    blaster.RunWithoutSeqalignGeneration(); /* NCBI_FAKE_WARNING */
    BlastHSPResults *results = blaster.GetResults(); /* NCBI_FAKE_WARNING */
    BlastHSPList *hsplist = results->hitlist_array[0]->hsplist_array[0];
    BOOST_REQUIRE_EQUAL(1, hsplist->hspcnt);
    BlastHSP *hsp = hsplist->hsp_array[0];
    BOOST_REQUIRE_EQUAL(832, hsp->score);
}


BOOST_AUTO_TEST_CASE(MegablastGreedyTraceback2) {
    CRef<CObjectManager> kObjMgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*kObjMgr));

    CRef<CSeq_entry> seq_entry1;
    ifstream in1("data/greedy1a.fsa");
    if ( !in1 )
        throw runtime_error("Failed to open file1");
    if ( !(seq_entry1 = CFastaReader(in1).ReadOneSeq()))
        throw runtime_error("Failed to read sequence from file1");
    scope->AddTopLevelSeqEntry(*seq_entry1);
    CRef<CSeq_loc> seqloc1(new CSeq_loc);
    const string kSeqIdString1("lcl|1");
    CRef<CSeq_id> id1(new CSeq_id(kSeqIdString1));
    seqloc1->SetWhole(*id1);
    SSeqLoc ss1(seqloc1, scope);

    CRef<CSeq_entry> seq_entry2;
    ifstream in2("data/greedy1b.fsa");
    if ( !in2 )
        throw runtime_error("Failed to open file2");
    if ( !(seq_entry2 = CFastaReader(in2).ReadOneSeq()))
        throw runtime_error("Failed to read sequence from file2");
    scope->AddTopLevelSeqEntry(*seq_entry2);
    CRef<CSeq_loc> seqloc2(new CSeq_loc);
    const string kSeqIdString2("lcl|2");
    CRef<CSeq_id> id2(new CSeq_id(kSeqIdString2));
    seqloc2->SetWhole(*id2);
    SSeqLoc ss2(seqloc2, scope);

    CBlastNucleotideOptionsHandle handle;
    handle.SetGapOpeningCost(0);
    handle.SetGapExtensionCost(0);
    handle.SetDustFiltering(false);

    // test multiple bug fixes in greedy gapped alignment

    CBl2Seq blaster1(ss1, ss2, handle);
    TSeqAlignVector sav(blaster1.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sav[0]->Size());

    const CSeq_align& seqalign1 = *sar;
    BOOST_REQUIRE(seqalign1.IsSetScore());
    ITERATE(CSeq_align::TScore, itr, seqalign1.GetScore()) {
        BOOST_REQUIRE((*itr)->IsSetId());
        if ((*itr)->GetId().GetStr() == "score") {
            BOOST_REQUIRE_EQUAL(619, (*itr)->GetValue().GetInt());
            break;
        }
    }

    handle.SetMatchReward(10);
    handle.SetMismatchPenalty(-25);
    handle.SetGapXDropoff(100.0);
    handle.SetGapXDropoffFinal(100.0);

    CBl2Seq blaster2(ss1, ss2, handle);
    sav = blaster2.Run();
    sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sav[0]->Size());

    const CSeq_align& seqalign2 = *sar;
    BOOST_REQUIRE(seqalign2.IsSetScore());
    ITERATE(CSeq_align::TScore, itr, seqalign2.GetScore()) {
        BOOST_REQUIRE((*itr)->IsSetId());
        if ((*itr)->GetId().GetStr() == "score") {
            BOOST_REQUIRE_EQUAL(6034, (*itr)->GetValue().GetInt());
            break;
        }
    }
}

BOOST_AUTO_TEST_CASE(Blastx2Seqs_QueryPlusStrand) {
    CSeq_id qid("gi|555");
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_plus));

    CSeq_id sid("gi|129295");
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(sid));

    CBl2Seq blaster(*query, *subj, eBlastx);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetStd().size());
}

BOOST_AUTO_TEST_CASE(Blastx2Seqs_QueryMinusStrand) {
    CSeq_id qid("gi|555");
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_minus));

    CSeq_id sid("gi|129295");
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(sid));

    CBl2Seq blaster(*query, *subj, eBlastx);
    TSeqAlignVector sav(blaster.Run());
    // No hits.  Empty CSeq_align_set returned.
    BOOST_REQUIRE(sav[0]->IsEmpty() == true);
}


BOOST_AUTO_TEST_CASE(TBlastx2Seqs_QueryBothStrands) {
    CSeq_id id("gi|555");
    auto_ptr<SSeqLoc> sl(
        CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_both));

    CBl2Seq blaster(*sl, *sl, eTblastx);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(39, (int)sar->GetSegs().GetStd().size());
    testBlastHitCounts(blaster, eTblastx_555_555);
    testRawCutoffs(blaster, eTblastx, eTblastx_555_555);
}

BOOST_AUTO_TEST_CASE(TBlastx2Seqs_QueryPlusStrand) {
    CSeq_id id("gi|555");
    auto_ptr<SSeqLoc> sl(
        CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_plus));

    CBl2Seq blaster(*sl, *sl, eTblastx);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(11, (int)sar->GetSegs().GetStd().size());
}

BOOST_AUTO_TEST_CASE(TBlastx2Seqs_QueryMinusStrand) {
    CSeq_id id("gi|555");
    auto_ptr<SSeqLoc> sl(
        CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_minus));

    CBl2Seq blaster(*sl, *sl, eTblastx);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(12, (int)sar->GetSegs().GetStd().size());
}


BOOST_AUTO_TEST_CASE(TblastxManyHits) {
    const int total_num_hsps = 50;
    const int num_hsps_to_check = 8;
    const int score_array[num_hsps_to_check] = 
        { 947, 125, 820, 113, 624, 221, 39, 778};
    const int sum_n_array[num_hsps_to_check] = 
        { 2, 2, 2, 2, 3, 3, 3, 0};
    CSeq_id qid("gi|24719404");
    auto_ptr<SSeqLoc> qsl(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));
    CSeq_id sid("gi|29807292");
    pair<TSeqPos, TSeqPos> range(15185000, 15195000);
    auto_ptr<SSeqLoc> ssl(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range, eNa_strand_both));
    CBl2Seq blaster(*qsl, *ssl, eTblastx);
    blaster.SetOptionsHandle().SetMaxNumHspPerSequence(total_num_hsps);

    TSeqAlignVector sav(blaster.Run());

    testBlastHitCounts(blaster, eTblastx_many_hits);
    testRawCutoffs(blaster, eTblastx, eTblastx_many_hits);

    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    list< CRef<CStd_seg> >& segs = sar->SetSegs().SetStd();
    BOOST_REQUIRE_EQUAL(total_num_hsps, (int)segs.size());
    int index = 0;
    ITERATE(list< CRef<CStd_seg> >, itr, segs) {
        const vector< CRef< CScore > >& score_v = (*itr)->GetScores();
        ITERATE(CSeq_align::TScore, sitr, score_v) {
            BOOST_REQUIRE((*sitr)->IsSetId());
            if ((*sitr)->GetId().GetStr() == "score") {
                BOOST_REQUIRE_EQUAL(score_array[index], 
                                     (*sitr)->GetValue().GetInt());
            } else if ((*sitr)->GetId().GetStr() == "sum_n") {
                BOOST_REQUIRE_EQUAL(sum_n_array[index], 
                                     (*sitr)->GetValue().GetInt());
            }
        }
        if (++index == num_hsps_to_check)
            break;
    }
}

BOOST_AUTO_TEST_CASE(ProteinBlast2Seqs) {
    CSeq_id id("gi|129295");
    auto_ptr<SSeqLoc> query(CTestObjMgr::Instance().CreateSSeqLoc(id));

    id.SetGi(7662354);
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(id));

    CBl2Seq blaster(*query, *subj, eBlastp);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());
    testBlastHitCounts(blaster, eBlastp_129295_7662354);
    testRawCutoffs(blaster, eBlastp, eBlastp_129295_7662354);
}

BOOST_AUTO_TEST_CASE(BlastnWithRepeatFiltering_InvalidDB) {
    CSeq_id qid("gi|555");
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalMegablastDefaults();
    const string kRepeatDb("junk");
    opts.SetRepeatFilteringDB(kRepeatDb.c_str());
    bool is_repeat_filtering_on = opts.GetRepeatFiltering();
    BOOST_REQUIRE(is_repeat_filtering_on);
    string repeat_db(opts.GetRepeatFilteringDB() 
                     ? opts.GetRepeatFilteringDB()
                     : kEmptyStr);
    BOOST_REQUIRE_EQUAL(kRepeatDb, repeat_db);

    CBl2Seq blaster(*query, *query, opts);
    BOOST_REQUIRE_THROW(blaster.Run(), CBlastException);
}

BOOST_AUTO_TEST_CASE(BlastnWithRepeatFiltering) {
    CSeq_id qid("gi|555");
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalMegablastDefaults();
    opts.SetRepeatFiltering(true);
    string repeat_db(opts.GetRepeatFilteringDB() 
                     ? opts.GetRepeatFilteringDB()
                     : kEmptyStr);
    BOOST_REQUIRE_EQUAL(string(kDefaultRepeatFilterDb), repeat_db);
    // it's harmless to set them both, but only the latter one will be used
    const string kRepeatDb("repeat/repeat_9606");
    opts.SetRepeatFilteringDB(kRepeatDb.c_str());
    repeat_db.assign(opts.GetRepeatFilteringDB() 
                     ? opts.GetRepeatFilteringDB()
                     : kEmptyStr);
    BOOST_REQUIRE_EQUAL(kRepeatDb, repeat_db);

    bool is_repeat_filtering_on = opts.GetRepeatFiltering();
    BOOST_REQUIRE(is_repeat_filtering_on);

    CBl2Seq blaster(*query, *query, opts);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE(sar.NotEmpty());
    BOOST_REQUIRE(sar->GetSegs().GetDenseg().GetNumseg() >= 1);
}

BOOST_AUTO_TEST_CASE(BlastnWithWindowMasker_Db) {
    CSeq_id qid("gi|555");
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalMegablastDefaults();
    const string kWindowMaskerDb("9606");
    opts.SetWindowMaskerDatabase(kWindowMaskerDb.c_str());
    string wmdb(opts.GetWindowMaskerDatabase()
                ? opts.GetWindowMaskerDatabase() : kEmptyStr);
    BOOST_REQUIRE_EQUAL(kWindowMaskerDb, wmdb);
    BOOST_REQUIRE_EQUAL(0, opts.GetWindowMaskerTaxId());
    CBl2Seq blaster(*query, *query, opts);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE(sar.NotEmpty());
    BOOST_REQUIRE(sar->GetSegs().GetDenseg().GetNumseg() >= 1);
}

BOOST_AUTO_TEST_CASE(BlastnWithWindowMasker_Taxid) {
    CSeq_id qid("gi|555");
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalMegablastDefaults();
    opts.SetWindowMaskerTaxId(9606);
    CBl2Seq blaster(*query, *query, opts);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE(sar.NotEmpty());
    BOOST_REQUIRE(sar->GetSegs().GetDenseg().GetNumseg() >= 1);
}

BOOST_AUTO_TEST_CASE(BlastnWithWindowMasker_InvalidDb) {
    CSeq_id qid("gi|555");
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalMegablastDefaults();
    const string kWindowMaskerDb("Dummydb");
    opts.SetWindowMaskerDatabase(kWindowMaskerDb.c_str());
    string wmdb(opts.GetWindowMaskerDatabase()
                ? opts.GetWindowMaskerDatabase() : kEmptyStr);
    BOOST_REQUIRE_EQUAL(kWindowMaskerDb, wmdb);
    CBl2Seq blaster(*query, *query, opts);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE(sar.NotEmpty());
    BOOST_REQUIRE(sar->GetSegs().GetDenseg().GetNumseg() == 1);
}

BOOST_AUTO_TEST_CASE(BlastnWithWindowMasker_InvalidTaxid) {
    CSeq_id qid("gi|555");
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalMegablastDefaults();
    const int kInvalidTaxId = -1;
    opts.SetWindowMaskerTaxId(kInvalidTaxId);
    BOOST_REQUIRE_EQUAL(kInvalidTaxId, opts.GetWindowMaskerTaxId());
    CBl2Seq blaster(*query, *query, opts);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE(sar.NotEmpty());
    // find self hit, silently ignoring the failed filtering
    BOOST_REQUIRE(sar->GetSegs().GetDenseg().GetNumseg() == 1);
}

BOOST_AUTO_TEST_CASE(BlastnWithWindowMasker_DbAndTaxid) {
    CSeq_id qid("gi|555");
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalMegablastDefaults();
    // if both are set, the database name will be given preference
    opts.SetWindowMaskerDatabase("9606");
    opts.SetWindowMaskerTaxId(-1);
    CBl2Seq blaster(*query, *query, opts);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE(sar.NotEmpty());
    BOOST_REQUIRE(sar->GetSegs().GetDenseg().GetNumseg() >= 1);
}

// Bug report from Alex Astashyn
BOOST_AUTO_TEST_CASE(Alex) {
    CSeq_id qid("NG_007092.2");
    TSeqRange qr(0, 2311633);
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, qr, eNa_strand_plus));

    CSeq_id sid("NT_007914.14");
    TSeqRange sr(5233652, 9849919);
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, sr));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalMegablastDefaults();
    opts.SetRepeatFiltering(true);
    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE(sar.NotEmpty());
    BOOST_REQUIRE(sar->GetSegs().GetDenseg().GetNumseg() >= 1);
}

BOOST_AUTO_TEST_CASE(NucleotideBlast2Seqs) {
    CSeq_id qid("gi|555");
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));

    CSeq_id sid("gi|3090");
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalBlastnDefaults();
    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(3, (int)sar->GetSegs().GetDenseg().GetNumseg());
    testBlastHitCounts(blaster, eBlastn_555_3090);
    testRawCutoffs(blaster, eBlastn, eBlastn_555_3090);
}

BOOST_AUTO_TEST_CASE(ProteinBlastChangeQuery) {
    CSeq_id id("gi|129295");
    auto_ptr<SSeqLoc> query(CTestObjMgr::Instance().CreateSSeqLoc(id));

    id.SetGi(7662354);
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(id));

    // Run self hit first
    CBl2Seq blaster(*subj, *subj, eBlastp);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());

    // Change the query sequence (recreates the lookup table)
    blaster.SetQuery(*query);
    sav = blaster.Run();
    sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());
}

BOOST_AUTO_TEST_CASE(ProteinBlastChangeSubject) {
    CSeq_id qid("gi|129295");
    auto_ptr<SSeqLoc> query(CTestObjMgr::Instance().CreateSSeqLoc(qid));

    CSeq_id sid("gi|7662354");
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(sid));

    // Run self hit first
    CBl2Seq blaster(*query, *query, eBlastp);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());

    // Change the subject sequence
    blaster.SetSubject(*subj);
    sav = blaster.Run();
    BOOST_REQUIRE_EQUAL(1, (int)sav.size());
    sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());
}

BOOST_AUTO_TEST_CASE(NucleotideBlastChangeQuery) {
    CSeq_id qid("gi|555");
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));

    CSeq_id sid("gi|3090");
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));

    // Run self hit first
    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalBlastnDefaults();
    CBl2Seq blaster(*subj, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());

    // Change the query sequence (recreates the lookup table)
    blaster.SetQuery(*query);
    sav = blaster.Run();
    BOOST_REQUIRE_EQUAL(2, (int)sav[0]->Size());
    sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(3, (int)sar->GetSegs().GetDenseg().GetNumseg());
}

BOOST_AUTO_TEST_CASE(NucleotideBlastChangeSubject) {
    CSeq_id qid("gi|555");
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));

    CSeq_id sid("gi|3090");
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));

    // Run self hit first
    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalBlastnDefaults();
    CBl2Seq blaster(*query, *query, opts);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());

    // Change the subject sequence
    blaster.SetSubject(*subj);
    sav = blaster.Run();
    sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(3, (int)sar->GetSegs().GetDenseg().GetNumseg());
}


BOOST_AUTO_TEST_CASE(ProteinBlastMultipleQueries) {
    TSeqLocVector sequences;

    CSeq_id qid("gi|129295");
    auto_ptr<SSeqLoc> sl1(CTestObjMgr::Instance().CreateSSeqLoc(qid));
    sequences.push_back(*sl1);

    CSeq_id sid("gi|7662354");
    auto_ptr<SSeqLoc> sl2(CTestObjMgr::Instance().CreateSSeqLoc(sid));
    sequences.push_back(*sl2);

    CBl2Seq blaster(sequences, sequences, eBlastp);
    TSeqAlignVector seqalign_v = blaster.Run();

    BOOST_REQUIRE_EQUAL(4, (int)seqalign_v.size());
    BOOST_REQUIRE_EQUAL(2, (int)sequences.size());

    CRef<CSeq_align> sar;
    
    BOOST_REQUIRE_EQUAL(1, seqalign_v[0]->Get().size());
    sar = *(seqalign_v[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());

    BOOST_REQUIRE_EQUAL(2, seqalign_v[1]->Get().size());
    sar = *(seqalign_v[1]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());
    sar = *(++(seqalign_v[1]->Get().begin()));
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());

    BOOST_REQUIRE_EQUAL(2, seqalign_v[2]->Get().size());
    sar = *(seqalign_v[2]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());
    sar = *(++(seqalign_v[2]->Get().begin()));
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());

    BOOST_REQUIRE_EQUAL(1, seqalign_v[3]->Get().size());
    sar = *(seqalign_v[3]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());


    /* DEBUG OUTPUT
    for (size_t i = 0; i < seqalign_v.size(); i++)
        cerr << "\n<" << i << ">\n"
            << MSerial_AsnText << seqalign_v[i].GetObject() << endl;
    */
    
    testBlastHitCounts(blaster, eBlastp_multi_q);
    testRawCutoffs(blaster, eBlastp, eBlastp_multi_q);

    // test the order of queries and subjects:
    testResultAlignments(sequences.size(), sequences.size(),
                            seqalign_v);
}

BOOST_AUTO_TEST_CASE(NucleotideBlastMultipleQueries) {
    TSeqLocVector sequences;

    CSeq_id qid("gi|555");
    auto_ptr<SSeqLoc> sl1(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));
    sequences.push_back(*sl1);
    BOOST_REQUIRE(sl1->mask.Empty());

    CSeq_id sid("gi|3090");
    auto_ptr<SSeqLoc> sl2(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, eNa_strand_both));
    sequences.push_back(*sl2);
    BOOST_REQUIRE(sl2->mask.Empty());

    CBl2Seq blaster(sequences, sequences, eBlastn);
    TSeqAlignVector seqalign_v = blaster.Run();
    BOOST_REQUIRE_EQUAL(2, (int)sequences.size());
    BOOST_REQUIRE_EQUAL(4, (int)seqalign_v.size());

    CRef<CSeq_align> sar = *(seqalign_v[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());

    // in older version this was seqalign_v[1], order has changed
    sar = *(seqalign_v[2]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());

    testBlastHitCounts(blaster, eBlastn_multi_q);
    testRawCutoffs(blaster, eBlastn, eBlastn_multi_q);

    // test the order of queries and subjects:
    testResultAlignments(sequences.size(), sequences.size(),
                            seqalign_v);
}

void DoSearchWordSize4(const char *file1, const char *file2) {
    CRef<CObjectManager> kObjMgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*kObjMgr));

    CRef<CSeq_entry> seq_entry1;
    ifstream in1(file1);
    if ( !in1 )
        throw runtime_error("Failed to open file1");
    if ( !(seq_entry1 = CFastaReader(in1).ReadOneSeq()))
        throw runtime_error("Failed to read sequence from file1");
    scope->AddTopLevelSeqEntry(*seq_entry1);
    CRef<CSeq_loc> seqloc1(new CSeq_loc);
    const string kSeqIdString1("lcl|1");
    CRef<CSeq_id> id1(new CSeq_id(kSeqIdString1));
    seqloc1->SetWhole(*id1);
    SSeqLoc ss1(seqloc1, scope);

    CRef<CSeq_entry> seq_entry2;
    ifstream in2(file2);
    if ( !in2 )
        throw runtime_error("Failed to open file2");
    if ( !(seq_entry2 = CFastaReader(in2).ReadOneSeq()))
        throw runtime_error("Failed to read sequence from file2");
    scope->AddTopLevelSeqEntry(*seq_entry2);
    CRef<CSeq_loc> seqloc2(new CSeq_loc);
    const string kSeqIdString2("lcl|2");
    CRef<CSeq_id> id2(new CSeq_id(kSeqIdString2));
    seqloc2->SetWhole(*id2);
    SSeqLoc ss2(seqloc2, scope);

    CBlastNucleotideOptionsHandle handle;
    handle.SetTraditionalBlastnDefaults();
    handle.SetWordSize(4);
    handle.SetDustFiltering(false);
    handle.SetMismatchPenalty(-1);
    handle.SetMatchReward(1);
    handle.SetEvalueThreshold(10000);

    CBl2Seq blaster(ss1, ss2, handle);
    blaster.RunWithoutSeqalignGeneration(); /* NCBI_FAKE_WARNING */
    BlastHSPResults *results = blaster.GetResults(); /* NCBI_FAKE_WARNING */
    BOOST_REQUIRE(results != NULL);
    BOOST_REQUIRE(results->hitlist_array[0] != NULL);
    BOOST_REQUIRE(results->hitlist_array[0]->hsplist_array[0] != NULL);
    BlastHSPList *hsp_list = results->hitlist_array[0]->hsplist_array[0];
    BOOST_REQUIRE(hsp_list->hspcnt > 0);
    BOOST_REQUIRE(hsp_list->hsp_array[0] != NULL);

    // verify that all hits are properly formed, and
    // at least as long as the word size

    for (int i = 0; i < hsp_list->hspcnt; i++) {
        BlastHSP *hsp = hsp_list->hsp_array[i];
        BOOST_REQUIRE(hsp != NULL);
        BOOST_REQUIRE(hsp->query.offset < hsp->query.end);
        BOOST_REQUIRE(hsp->subject.offset < hsp->subject.end);
        BOOST_REQUIRE(hsp->query.gapped_start >= hsp->query.offset &&
                       hsp->query.gapped_start < hsp->query.end);
        BOOST_REQUIRE(hsp->subject.gapped_start >= hsp->subject.offset &&
                       hsp->subject.gapped_start < hsp->subject.end);
        BOOST_REQUIRE(hsp->query.end - hsp->query.offset >= 4);
        BOOST_REQUIRE(hsp->subject.end - hsp->subject.offset >= 4);
    }
}

BOOST_AUTO_TEST_CASE(NucleotideBlastWordSize4) {
    DoSearchWordSize4("data/blastn_size4a.fsa",
                      "data/blastn_size4b.fsa");
}

// test bug fix when size-4 seed falls at the end of
// the subject sequence
BOOST_AUTO_TEST_CASE(NucleotideBlastWordSize4_EOS) {
    DoSearchWordSize4("data/blastn_size4c.fsa",
                      "data/blastn_size4d.fsa");
}

BOOST_AUTO_TEST_CASE(TblastnOutOfFrame) {
    CSeq_id qid("NP_647642.2"); // Protein sequence
    CSeq_id sid("BC042576.1");  // DNA sequence

    auto_ptr<SSeqLoc> query(CTestObjMgr::Instance().CreateSSeqLoc(qid));
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(sid));

    // Set the options
    CTBlastnOptionsHandle opts;
    opts.SetOutOfFrameMode();
    opts.SetFrameShiftPenalty(10);
    opts.SetFilterString("m;L");/* NCBI_FAKE_WARNING */
    opts.SetEvalueThreshold(0.01);
    opts.SetCompositionBasedStats(eNoCompositionBasedStats);

    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    BOOST_REQUIRE_EQUAL(1, (int)sav.size());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(2, (int)sav[0]->Size());
    testBlastHitCounts(blaster, eTblastn_oof);
    testRawCutoffs(blaster, eTblastn, eTblastn_oof);
}

// test multiple fixes for OOF alignments on both subject strands
BOOST_AUTO_TEST_CASE(TblastnOutOfFrame2) {
    CSeq_id qid("gi|38111923"); // Protein sequence
    CSeq_id sid("gi|6648925");  // DNA sequence

    auto_ptr<SSeqLoc> query(CTestObjMgr::Instance().CreateSSeqLoc(qid));
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(sid));

    // Set the options
    CTBlastnOptionsHandle opts;
    opts.SetOutOfFrameMode();
    opts.SetFrameShiftPenalty(5);
    opts.SetCompositionBasedStats(eNoCompositionBasedStats);
    opts.SetFilterString("L");/* NCBI_FAKE_WARNING */

    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(5, (int)sav[0]->Size());

    // test fix for a bug generating OOF traceback

    const CSeq_align& seqalign = *sar;
    BOOST_REQUIRE(seqalign.IsSetScore());
    ITERATE(CSeq_align::TScore, itr, seqalign.GetScore()) {
        BOOST_REQUIRE((*itr)->IsSetId());
        if ((*itr)->GetId().GetStr() == "num_ident") {
            BOOST_REQUIRE_EQUAL(55, (*itr)->GetValue().GetInt());
            break;
        }
    }
}

BOOST_AUTO_TEST_CASE(BlastxOutOfFrame) {
    CSeq_id qid("BC042576.1");  // DNA sequence
    CSeq_id sid("NP_647642.2"); // Protein sequence

    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(sid));

    // Set the options
    CBlastxOptionsHandle opts;
    opts.SetOutOfFrameMode();
    opts.SetFrameShiftPenalty(10);
    opts.SetFilterString("m;L");/* NCBI_FAKE_WARNING */
    opts.SetEvalueThreshold(0.01);

    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(2, (int)sav[0]->Size());
    testBlastHitCounts(blaster, eBlastx_oof);
    testRawCutoffs(blaster, eBlastx, eBlastx_oof);
}

// test for a bug computing OOF sequence lengths during traceback

BOOST_AUTO_TEST_CASE(BlastxOutOfFrame_DifferentFrames) {
    CSeq_id qid("gi|27486285");  // DNA sequence
    CSeq_id sid("gi|7331210"); // Protein sequence

    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(sid));

    // Set the options
    CBlastxOptionsHandle opts;
    opts.SetOutOfFrameMode();
    opts.SetFrameShiftPenalty(10);

    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    BOOST_REQUIRE_EQUAL(5, (int)sav[0]->Size());
}

// The following 3 functions are for checking results in the strand 
// combinations tests.

void x_TestAlignmentQuerySubjStrandCombinations(TSeqAlignVector& sav, 
                                                string aligned_strands) {

    // Starting offsets in alignment as query/subject pairs
    vector< pair<TSignedSeqPos, TSignedSeqPos> > starts;
    starts.push_back(make_pair(7685759, 10));
    starts.push_back(make_pair(7685758, -1));
    starts.push_back(make_pair(7685718, 269));
    starts.push_back(make_pair(7685717, -1));
    starts.push_back(make_pair(7685545, 309));

    const size_t kNumSegments(starts.size());

    // Lengths of the aligned regions defined in starts vector
    vector<TSeqPos> lengths;
    lengths.reserve(kNumSegments);
    lengths.push_back(259);
    lengths.push_back(1);
    lengths.push_back(40);
    lengths.push_back(1);
    lengths.push_back(172);

    // Strands of the involved aligned segments as query/subject pairs
    typedef vector< pair<ENa_strand, ENa_strand> > TStrandPairs;
    TStrandPairs strands(kNumSegments, 
                         make_pair(eNa_strand_minus, eNa_strand_plus));

    // Reverse the contents of the vectors if necessary
    if (aligned_strands == "plus-minus") {
        reverse(starts.begin(), starts.end());
        reverse(lengths.begin(), lengths.end());
        NON_CONST_ITERATE(TStrandPairs, itr, strands) {
            swap(itr->first, itr->second);
        }
    }
    BOOST_REQUIRE_EQUAL(kNumSegments, lengths.size());
    BOOST_REQUIRE_EQUAL(kNumSegments, strands.size());

    // Obtain the data from the Seq-align's dense segs ...
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sav[0]->Size());
    // BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());
    const CDense_seg& ds = sar->GetSegs().GetDenseg();
    // CTypeIterator<CDense_seg> segs_itr(Begin(*sar));
    const size_t kNumDim(ds.GetDim());
    vector< TSignedSeqPos > seg_starts = ds.GetStarts();
    vector< TSeqPos> seg_lengths = ds.GetLens();
    vector< ENa_strand> seg_strands = ds.GetStrands();
    BOOST_REQUIRE_EQUAL(kNumSegments, seg_lengths.size());
    BOOST_REQUIRE_EQUAL(kNumSegments*kNumDim, seg_starts.size());

    // ... and compare it to what is expected
    for (size_t index = 0; index < kNumSegments; ++index) {
        ostringstream os;
        os << "Segment " << index << ": expected " << lengths[index]
           << " actual " << seg_lengths[index];
        BOOST_REQUIRE_MESSAGE(lengths[index] == seg_lengths[index],
                              os.str());

        os.str("");
        os << "Segment " << index << ": expected " << starts[index].first
           << " actual " << seg_starts[2*index];
        BOOST_REQUIRE_MESSAGE(starts[index].first == seg_starts[2*index],
                              os.str());
        os.str("");
        os << "Segment " << index << ": expected " << starts[index].second
           << " actual " << seg_starts[2*index];
        BOOST_REQUIRE_MESSAGE(starts[index].second == seg_starts[2*index+1],
                              os.str());
        os.str("");
        os << "Segment " << index << ": expected " << strands[index].first
           << " actual " << seg_strands[2*index];
        BOOST_REQUIRE_MESSAGE(strands[index].first == seg_strands[2*index],
                              os.str());
        os.str("");
        os << "Segment " << index << ": expected " << strands[index].second
           << " actual " << seg_strands[2*index];
        BOOST_REQUIRE_MESSAGE(strands[index].second == seg_strands[2*index+1],
                              os.str());
    }
}

static void testIntervalWholeAlignment(TSeqAlignVector& sav)
{
    const int num_segs = 5;
    const int num_starts = 10;
    const int starts[num_starts] = { 7685759, 0, 7685758, -1, 7685718,
                                     269, 7685717, -1, 7685545, 309 };
    const int lengths[num_segs] = { 269, 1, 40, 1, 172 };
    int index;

    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sav[0]->Size());
    CTypeIterator<CDense_seg> segs_itr(Begin(*sar));
    vector< TSignedSeqPos > seg_starts = segs_itr->GetStarts();
    vector< TSeqPos> seg_lengths = segs_itr->GetLens();
    vector< ENa_strand> seg_strands = segs_itr->GetStrands();
    BOOST_REQUIRE_EQUAL(num_segs, (int)seg_lengths.size());
    BOOST_REQUIRE_EQUAL(num_starts, (int)seg_starts.size());
    for (index = 0; index < num_segs; ++index) {
        BOOST_REQUIRE_EQUAL(lengths[index], (int)seg_lengths[index]);
        BOOST_REQUIRE_EQUAL(starts[2*index], (int)seg_starts[2*index]);
        BOOST_REQUIRE_EQUAL(starts[2*index+1], (int)seg_starts[2*index+1]);
        BOOST_REQUIRE(seg_strands[2*index] == eNa_strand_minus);
        BOOST_REQUIRE(seg_strands[2*index+1] == eNa_strand_plus);
    }
}

static void testWholeIntervalAlignment(TSeqAlignVector& sav)
{
    const int num_segs = 5;
    const int num_starts = 10;
    const int starts[num_starts] = { 309, 7685545, -1, 7685717, 269, 7685718,
                                     -1, 7685758, 0, 7685759 };
    const int lengths[num_segs] = { 172, 1, 40, 1, 269 };
    int index;

    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sav[0]->Size());
    CTypeIterator<CDense_seg> segs_itr(Begin(*sar));
    vector< TSignedSeqPos > seg_starts = segs_itr->GetStarts();
    vector< TSeqPos> seg_lengths = segs_itr->GetLens();
    vector< ENa_strand> seg_strands = segs_itr->GetStrands();
    BOOST_REQUIRE_EQUAL(num_segs, (int)seg_lengths.size());
    BOOST_REQUIRE_EQUAL(num_starts, (int)seg_starts.size());
    for (index = 0; index < num_segs; ++index) {
        BOOST_REQUIRE_EQUAL(lengths[index], (int)seg_lengths[index]);
        BOOST_REQUIRE_EQUAL(starts[2*index], (int)seg_starts[2*index]);
        BOOST_REQUIRE_EQUAL(starts[2*index+1], (int)seg_starts[2*index+1]);
        BOOST_REQUIRE(seg_strands[2*index] == eNa_strand_minus);
        BOOST_REQUIRE(seg_strands[2*index+1] == eNa_strand_plus);
    }
}

// should find alignments
BOOST_AUTO_TEST_CASE(Blastn_QueryBothStrands_SubjBothStrands) {
    // Alignment in these sequences is from plus/minus strands
    CSeq_id qid("NT_004487.15");
    pair<TSeqPos, TSeqPos> range(7685545, 7686027);
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, range, 
                                               eNa_strand_both));

    CSeq_id sid("AA441981.1");
    range.first = 10;
    range.second = 480;
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range,
                                               eNa_strand_both));

    CBlastNucleotideOptionsHandle* opts = new CBlastNucleotideOptionsHandle;
    opts->SetTraditionalBlastnDefaults();
    CBl2Seq blaster(*query, *subj, *opts);
    TSeqAlignVector sav(blaster.Run());
    x_TestAlignmentQuerySubjStrandCombinations(sav, "minus-plus");
}

// should find alignment
BOOST_AUTO_TEST_CASE(Blastn_QueryBothStrands_SubjPlusStrand) {
    // Alignment in these sequences is from plus/minus strands
    CSeq_id qid("NT_004487.15");
    pair<TSeqPos, TSeqPos> range(7685545, 7686027);
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, range, 
                                               eNa_strand_both));

    CSeq_id sid("AA441981.1");
    range.first = 10;
    range.second = 480;
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range,
                                               eNa_strand_plus));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalBlastnDefaults();
    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    x_TestAlignmentQuerySubjStrandCombinations(sav, "minus-plus");
}

// should find alignment
BOOST_AUTO_TEST_CASE(Blastn_QueryBothStrands_SubjMinusStrand) {
    // Alignment in these sequences is from plus/minus strands
    CSeq_id qid("NT_004487.15");
    pair<TSeqPos, TSeqPos> range(7685545, 7686027);
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, range, 
                                               eNa_strand_both));

    CSeq_id sid("AA441981.1");
    range.first = 10;
    range.second = 480;
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range,
                                               eNa_strand_minus));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalBlastnDefaults();
    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    x_TestAlignmentQuerySubjStrandCombinations(sav, "plus-minus");
}

// shouldn't find an alignment
BOOST_AUTO_TEST_CASE(Blastn_QueryPlusStrand_SubjPlusStrand) {
    // Alignment in these sequences is from plus/minus strands
    CSeq_id qid("NT_004487.15");
    pair<TSeqPos, TSeqPos> range(7685545, 7686027);
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, range, 
                                               eNa_strand_plus));

    CSeq_id sid("AA441981.1");
    range.first = 10;
    range.second = 480;
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range,
                                               eNa_strand_plus));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalBlastnDefaults();
    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    BOOST_REQUIRE(sav[0]->IsEmpty() == true);
}

// should find an alignment
BOOST_AUTO_TEST_CASE(Blastn_QueryPlusStrand_SubjMinusStrand) {
    // Alignment in these sequences is from plus/minus strands
    CSeq_id qid("NT_004487.15");
    pair<TSeqPos, TSeqPos> range(7685545, 7686027);
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, range, 
                                               eNa_strand_plus));

    CSeq_id sid("AA441981.1");
    range.first = 10;
    range.second = 480;
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range,
                                               eNa_strand_minus));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalBlastnDefaults();
    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    x_TestAlignmentQuerySubjStrandCombinations(sav, "plus-minus");
}

// should NOT find an alignment because we only search the plus strand of
// the subject sequence
BOOST_AUTO_TEST_CASE(Blastn_QueryPlusStrand_SubjBothStrands) {
    // Alignment in these sequences is from plus/minus strands
    CSeq_id qid("NT_004487.15");
    pair<TSeqPos, TSeqPos> range(7685545, 7686027);
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, range, 
                                               eNa_strand_plus));

    CSeq_id sid("AA441981.1");
    range.first = 10;
    range.second = 480;
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range,
                                               eNa_strand_both));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalBlastnDefaults();
    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    BOOST_REQUIRE(sav[0]->IsEmpty() == true);
}

// should not find an alignment because alignment is on opposite strands
BOOST_AUTO_TEST_CASE(Blastn_QueryMinusStrand_SubjMinusStrand) {
    // Alignment in these sequences is from plus/minus strands
    CSeq_id qid("NT_004487.15");
    pair<TSeqPos, TSeqPos> range(7685545, 7686027);
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, range, 
                                               eNa_strand_minus));

    CSeq_id sid("AA441981.1");
    range.first = 10;
    range.second = 480;
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range,
                                               eNa_strand_minus));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalBlastnDefaults();
    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    BOOST_REQUIRE(sav[0]->IsEmpty() == true);
}

// should find alignment
BOOST_AUTO_TEST_CASE(Blastn_QueryMinusStrand_SubjPlusStrand) {
    // Alignment in these sequences is from plus/minus strands
    CSeq_id qid("NT_004487.15");
    pair<TSeqPos, TSeqPos> range(7685545, 7686027);
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, range, 
                                               eNa_strand_minus));

    CSeq_id sid("AA441981.1");
    range.first = 10;
    range.second = 480;
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range,
                                               eNa_strand_plus));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalBlastnDefaults();
    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    x_TestAlignmentQuerySubjStrandCombinations(sav, "minus-plus");
}

// should find alignment
BOOST_AUTO_TEST_CASE(Blastn_QueryMinusStrand_SubjBothStrands) {
    // Alignment in these sequences is from plus/minus strands
    CSeq_id qid("NT_004487.15");
    pair<TSeqPos, TSeqPos> range(7685545, 7686027);
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, range, 
                                               eNa_strand_minus));

    CSeq_id sid("AA441981.1");
    range.first = 10;
    range.second = 480;
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range,
                                               eNa_strand_both));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalBlastnDefaults();
    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    x_TestAlignmentQuerySubjStrandCombinations(sav, "minus-plus");
}

// Should properly find alignment
BOOST_AUTO_TEST_CASE(Blastn_QueryWhole_SubjInterval)
{
    CSeq_id qid("AA441981.1");
    auto_ptr<SSeqLoc> query(CTestObjMgr::Instance().CreateWholeSSeqLoc(qid));

    CSeq_id sid("NT_004487.15");
    pair<TSeqPos, TSeqPos> range(7685545, 7686027);
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range, 
                                               eNa_strand_both));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalBlastnDefaults();
    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    testWholeIntervalAlignment(sav);
}

BOOST_AUTO_TEST_CASE(Blastn_QueryInterval_SubjWhole)
{
    CSeq_id qid("NT_004487.15");
    pair<TSeqPos, TSeqPos> range(7685545, 7686027);
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, range, 
                                               eNa_strand_both));

    CSeq_id sid("AA441981.1");
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateWholeSSeqLoc(sid));

    CBlastNucleotideOptionsHandle opts;
    opts.SetTraditionalBlastnDefaults();
    CBl2Seq blaster(*query, *subj, opts);
    TSeqAlignVector sav(blaster.Run());
    testIntervalWholeAlignment(sav);
}

BOOST_AUTO_TEST_CASE(BlastpMultipleQueries_MultipleSubjs) {
    vector<int> q_gis, s_gis;

    // Setup the queries
    q_gis.push_back(6);
    q_gis.push_back(129295);
    q_gis.push_back(15606659);

    // setup the subjects
    s_gis.push_back(129295);
    s_gis.push_back(6);
    s_gis.push_back(4336138); // no hits with gis 6 and 129295
    s_gis.push_back(15606659);
    s_gis.push_back(5556);

    TSeqLocVector queries;
    ITERATE(vector<int>, itr, q_gis) {
        CRef<CSeq_loc> loc(new CSeq_loc());
        loc->SetWhole().SetGi(*itr);

        CScope* scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
        scope->AddDefaults();
        queries.push_back(SSeqLoc(loc, scope));
    }

    TSeqLocVector subjects;
    ITERATE(vector<int>, itr, s_gis) {
        CRef<CSeq_loc> loc(new CSeq_loc());
        loc->SetWhole().SetGi(*itr);

        CScope* scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
        scope->AddDefaults();
        subjects.push_back(SSeqLoc(loc, scope));
    }

    size_t num_queries = queries.size();
    size_t num_subjects = subjects.size();

    // BLAST by concatenating all queries
    CBl2Seq blaster4all(queries, subjects, eBlastp);
    TSeqAlignVector sas_v = blaster4all.Run();
    BOOST_REQUIRE_EQUAL(num_queries*num_subjects, sas_v.size());
    testBlastHitCounts(blaster4all, eBlastp_multi_q_s);
    testRawCutoffs(blaster4all, eBlastp, eBlastp_multi_q_s);

    // test the order of queries and subjects:
    testResultAlignments(num_queries, num_subjects,
                            sas_v);
}

BOOST_AUTO_TEST_CASE(BlastpMultipleQueries_MultipleSubjs_RunEx) {
    vector<int> q_gis, s_gis;

    // Setup the queries
    q_gis.push_back(6);
    q_gis.push_back(129295);
    q_gis.push_back(15606659);

    // setup the subjects
    s_gis.push_back(129295);
    s_gis.push_back(6);
    s_gis.push_back(4336138); // no hits with gis 6 and 129295
    s_gis.push_back(15606659);
    s_gis.push_back(5556);

    TSeqLocVector queries;
    ITERATE(vector<int>, itr, q_gis) {
        CRef<CSeq_loc> loc(new CSeq_loc());
        loc->SetWhole().SetGi(*itr);

        CScope* scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
        scope->AddDefaults();
        queries.push_back(SSeqLoc(loc, scope));
    }

    TSeqLocVector subjects;
    ITERATE(vector<int>, itr, s_gis) {
        CRef<CSeq_loc> loc(new CSeq_loc());
        loc->SetWhole().SetGi(*itr);

        CScope* scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
        scope->AddDefaults();
        subjects.push_back(SSeqLoc(loc, scope));
    }

    size_t num_queries = queries.size();
    size_t num_subjects = subjects.size();

    // BLAST by concatenating all queries
    CBl2Seq blaster4all(queries, subjects, eBlastp);
    CRef<CSearchResultSet> results = blaster4all.RunEx();
    BOOST_REQUIRE(results->GetResultType() == eSequenceComparison);
    BOOST_REQUIRE_EQUAL((num_queries*num_subjects),
                        results->GetNumResults());

    // build the seqalign vector from the result set
    TSeqAlignVector sas_v;
    for (size_t i = 0; i < num_queries; i++)
    {
        for (size_t j = 0; j < num_subjects; j++)
        {
            CSearchResults& res_ij = results->GetResults(i, j);
            CRef<CSeq_align_set> aln_set;
            aln_set.Reset(const_cast<CSeq_align_set*>
                          (res_ij.GetSeqAlign().GetPointer()));
            sas_v.push_back(aln_set);
        }
    }
    
    // do the rest of the tests on sas_v as in the
    // BlastpMultipleQueries_MultipleSubjs function:

    BOOST_REQUIRE_EQUAL(num_queries*num_subjects, sas_v.size());
    testBlastHitCounts(blaster4all, eBlastp_multi_q_s);
    testRawCutoffs(blaster4all, eBlastp, eBlastp_multi_q_s);

    // test the order of queries and subjects:
    testResultAlignments(num_queries, num_subjects,
                            sas_v);
}

// This closely resembles how the command line applications invoke bl2seq
BOOST_AUTO_TEST_CASE(BlastpMultipleQueries_MultipleSubjs_CLocalBlast) {
    vector<int> q_gis, s_gis;

    // Setup the queries
    q_gis.push_back(6);
    q_gis.push_back(129295);
    q_gis.push_back(15606659);

    // setup the subjects
    s_gis.push_back(129295);
    s_gis.push_back(6);
    s_gis.push_back(4336138); // no hits with gis 6 and 129295
    s_gis.push_back(15606659);
    s_gis.push_back(5556);

    TSeqLocVector query_vec;
    ITERATE(vector<int>, itr, q_gis) {
        CRef<CSeq_loc> loc(new CSeq_loc());
        loc->SetWhole().SetGi(*itr);

        CScope* scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
        scope->AddDefaults();
        query_vec.push_back(SSeqLoc(loc, scope));
    }
    CRef<IQueryFactory> queries(new CObjMgr_QueryFactory(query_vec));

    CRef<CBlastOptionsHandle>
        opts_handle(CBlastOptionsFactory::Create(eBlastp));

    TSeqLocVector subj_vec;
    ITERATE(vector<int>, itr, s_gis) {
        CRef<CSeq_loc> loc(new CSeq_loc());
        loc->SetWhole().SetGi(*itr);

        CScope* scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
        scope->AddDefaults();
        subj_vec.push_back(SSeqLoc(loc, scope));
    }
    CRef<IQueryFactory> subj_qf(new CObjMgr_QueryFactory(subj_vec));
    CRef<CLocalDbAdapter> subjects(new CLocalDbAdapter(subj_qf,
                                                       opts_handle));

    size_t num_queries = query_vec.size();
    size_t num_subjects = subj_vec.size();

    // BLAST by concatenating all queries
    CLocalBlast blaster(queries, opts_handle, subjects);
    CRef<CSearchResultSet> results = blaster.Run();
    BOOST_REQUIRE(results->GetResultType() == eSequenceComparison);
    BOOST_REQUIRE_EQUAL((num_queries*num_subjects),
                        results->GetNumResults());
    BOOST_REQUIRE_EQUAL((num_queries*num_subjects), results->size());
    BOOST_REQUIRE_EQUAL(num_queries, results->GetNumQueries());
    BOOST_REQUIRE_EQUAL(num_subjects,
                        results->GetNumResults()/results->GetNumQueries());

    // build the seqalign vector from the result set
    TSeqAlignVector sas_v;
    for (size_t i = 0; i < num_queries; i++)
    {
        for (size_t j = 0; j < num_subjects; j++)
        {
            CSearchResults& res_ij = results->GetResults(i, j);
            CRef<CSeq_align_set> aln_set;
            aln_set.Reset(const_cast<CSeq_align_set*>
                          (res_ij.GetSeqAlign().GetPointer()));
            sas_v.push_back(aln_set);
        }
    }
    
    // do the rest of the tests on sas_v as in the
    // BlastpMultipleQueries_MultipleSubjs function:
    BOOST_REQUIRE_EQUAL(num_queries*num_subjects, sas_v.size());

    // test the order of queries and subjects:
    testResultAlignments(num_queries, num_subjects, sas_v);
}

BOOST_AUTO_TEST_CASE(BlastOptionsEquality) {
    // Create options object through factory
    auto_ptr<CBlastOptionsHandle> megablast_options_handle(
        CBlastOptionsFactory::Create(eMegablast));
    CBlastNucleotideOptionsHandle nucl_options_handle;
    BOOST_REQUIRE(megablast_options_handle->GetOptions() == 
                   nucl_options_handle.GetOptions());
}

BOOST_AUTO_TEST_CASE(BlastOptionsInequality) {
    CBlastProteinOptionsHandle prot_options_handle;
    CBlastNucleotideOptionsHandle nucl_options_handle;
    BOOST_REQUIRE(prot_options_handle.GetOptions() != 
                   nucl_options_handle.GetOptions());

    // Blastn and Megablast are different
    auto_ptr<CBlastOptionsHandle> blastn_options_handle(
        CBlastOptionsFactory::Create(eBlastn));
    BOOST_REQUIRE(blastn_options_handle->GetOptions() != 
                   nucl_options_handle.GetOptions());

    // Change the matrix and compare
    CBlastProteinOptionsHandle prot_options_handle2;
    prot_options_handle.SetMatrixName("pam30");
    BOOST_REQUIRE(prot_options_handle.GetOptions() !=
                   prot_options_handle2.GetOptions());
}

BOOST_AUTO_TEST_CASE(DiscontiguousMB) {
    CSeq_id qid("gi|408478");  // zebrafish sequence U02544
    CSeq_id sid("gi|1546012"); // mouse sequence U61969
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, eNa_strand_both));
    auto_ptr<SSeqLoc> subj(CTestObjMgr::Instance().CreateSSeqLoc(sid));

    CBl2Seq blaster(*query, *subj, eDiscMegablast);
    TSeqAlignVector sav(blaster.Run());
    BOOST_REQUIRE_EQUAL(1, (int)sav.size());

    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(13, (int)sar->GetSegs().GetDenseg().GetNumseg());
    testBlastHitCounts(blaster, eDiscMegablast_U02544_U61969);
    testRawCutoffs(blaster, eDiscMegablast, eDiscMegablast_U02544_U61969);
}

BOOST_AUTO_TEST_CASE(BlastnHumanChrom_MRNA) {
    CSeq_id qid("NT_004487.16");
    CSeq_id sid("AA621478.1");
    pair<TSeqPos, TSeqPos> qrange(7868209-1, 7868602-1);
    pair<TSeqPos, TSeqPos> srange(2-1, 397-1);
    auto_ptr<SSeqLoc> query(
        CTestObjMgr::Instance().CreateSSeqLoc(qid, 
                                               qrange, eNa_strand_plus));
    auto_ptr<SSeqLoc> subj(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, 
                                               srange, eNa_strand_plus));

    CBlastNucleotideOptionsHandle options;
    CBl2Seq blaster(*query, *subj, options);
    TSeqAlignVector sav(blaster.Run());
    BOOST_REQUIRE_EQUAL(1, (int)sav.size());

    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(5, (int)sar->GetSegs().GetDenseg().GetNumseg());
    testBlastHitCounts(blaster, eMegablast_chrom_mrna);
    testRawCutoffs(blaster, eMegablast, eMegablast_chrom_mrna);
}

// Checks that results for multiple subjects are put in correct places
// in the vector of Seq-aligns
BOOST_AUTO_TEST_CASE(testOneSubjectResults2CSeqAlign)
{
    const int num_subjects = 15;
    const int results_size[num_subjects] = 
        { 1, 1, 0, 1, 1, 1, 2, 1, 2, 0, 0, 0, 0, 2, 1 };
    const int query_gi = 7274302;
    const int gi_diff = 28;
    string seqid_str("gi|");
    CRef<CSeq_id> id(new CSeq_id(seqid_str + NStr::IntToString(query_gi)));
    auto_ptr<SSeqLoc> sl(
        CTestObjMgr::Instance().CreateSSeqLoc(*id, eNa_strand_both));
    TSeqLocVector query;
    query.push_back(*sl);
    TSeqLocVector subjects;
    int index;
    for (index = 0; index < num_subjects; ++index) {
        id.Reset(new CSeq_id(seqid_str + 
                 NStr::IntToString(query_gi + gi_diff + index)));
        sl.reset(CTestObjMgr::Instance().CreateSSeqLoc(*id, 
                                                       eNa_strand_both));
        subjects.push_back(*sl);
    }
    CBl2Seq blaster(query, subjects, eMegablast);
    TSeqAlignVector seqalign_v = blaster.Run();
    BOOST_REQUIRE_EQUAL(num_subjects, (int)seqalign_v.size());

    index = 0;
    ITERATE(TSeqAlignVector, itr, seqalign_v)
    {
        BOOST_REQUIRE_EQUAL(results_size[index], (int) (*itr)->Get().size());
        index++;
    }
}

BOOST_AUTO_TEST_CASE(testMultiSeqSearchSymmetry)
{
    const int num_seqs = 19;
    const int gi_list[num_seqs] = 
        { 1346057, 125527, 121064, 1711551, 125412, 128337, 2507199,
          1170625, 1730070, 585365, 140977, 1730069, 20455504, 125206,
          125319, 114152, 1706450, 1706307, 125565 };
    const int score_cutoff = 70;

    string seqid_str("gi|");
    TSeqLocVector seq_vec;
    int index;
    for (index = 0; index < num_seqs; ++index) {
        CRef<CSeq_id> id(new CSeq_id(seqid_str + 
                         NStr::IntToString(gi_list[index])));
        auto_ptr<SSeqLoc> sl(
            CTestObjMgr::Instance().CreateSSeqLoc(*id, eNa_strand_both));
        seq_vec.push_back(*sl);
    }
    
    CBlastProteinOptionsHandle prot_opts;
    prot_opts.SetSegFiltering(false);
    CBl2Seq blaster(seq_vec, seq_vec, prot_opts);
    blaster.RunWithoutSeqalignGeneration(); /* NCBI_FAKE_WARNING */
    BlastHSPResults* results = blaster.GetResults(); /* NCBI_FAKE_WARNING */

    int qindex, sindex, qindex1, sindex1;
    for (qindex = 0; qindex < num_seqs; ++qindex) {
        for (sindex = 0; sindex < results->hitlist_array[qindex]->hsplist_count;
             ++sindex) {
            BlastHSPList* hsp_list1, *hsp_list2 = NULL;
            hsp_list1 = results->hitlist_array[qindex]->hsplist_array[sindex];
            qindex1 = hsp_list1->oid;
            BlastHitList* hitlist = results->hitlist_array[qindex1];
            for (sindex1 = 0; sindex1 < hitlist->hsplist_count; ++sindex1) {
                if (hitlist->hsplist_array[sindex1]->oid == qindex) {
                    hsp_list2 = hitlist->hsplist_array[sindex1];
                    break;
                }
            }
            BOOST_REQUIRE(hsp_list2 != NULL);
            int hindex;
            for (hindex = 0; hindex < hsp_list1->hspcnt; ++hindex) {
                if (hsp_list1->hsp_array[hindex]->score <= score_cutoff)
                    break;
                BOOST_REQUIRE(hindex < hsp_list2->hspcnt);
                BOOST_REQUIRE_EQUAL(hsp_list1->hsp_array[hindex]->score,
                                     hsp_list2->hsp_array[hindex]->score);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(testInterruptCallbackWithNull) {
    CSeq_id id("gi|129295");
    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(id));

    CBl2Seq blaster(*sl, *sl, eBlastp);
    TInterruptFnPtr null_fnptr = 0;
    TInterruptFnPtr fnptr = blaster.SetInterruptCallback(null_fnptr);
    BOOST_REQUIRE(fnptr == NULL);

    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());

    fnptr = blaster.SetInterruptCallback(interrupt_immediately);
    // make sure we get the previous interrupt callback
    BOOST_REQUIRE(fnptr == null_fnptr);

    fnptr = blaster.SetInterruptCallback(null_fnptr);
    // make sure we get the previous interrupt callback
    BOOST_REQUIRE(fnptr == interrupt_immediately);

    // Retry the search now that we've removed the interrupt callback
    sav = blaster.Run();
    BOOST_REQUIRE_EQUAL(1, (int)sav.size());
    sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());
}

BOOST_AUTO_TEST_CASE(testInterruptCallbackDoNotInterrupt) {
    CSeq_id id("gi|129295");
    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(id));

    CBl2Seq blaster(*sl, *sl, eBlastp);
    TInterruptFnPtr fnptr = blaster.SetInterruptCallback(do_not_interrupt);
    BOOST_REQUIRE(fnptr == NULL);

    TSeqAlignVector sav(blaster.Run());
    BOOST_REQUIRE_EQUAL(1, (int)sav.size());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(1, (int)sar->GetSegs().GetDenseg().GetNumseg());
}

#if SEQLOC_MIX_QUERY_OK
BOOST_AUTO_TEST_CASE(MultiIntervalLoc) {
    const size_t kNumInts = 20;
    const size_t kStarts[kNumInts] = 
        { 838, 1838, 6542, 7459, 9246, 10431, 14807, 16336, 19563, 
          20606, 21232, 22615, 23822, 27941, 29597, 30136, 31287, 
          31786, 33315, 35402 };
    const size_t kEnds[kNumInts] = 
        { 961, 2010, 6740, 7573, 9408, 10609, 15043, 16511, 19783, 
          20748, 21365, 22817, 24049, 28171, 29839, 30348, 31362, 
          31911, 33485, 37952 };
    size_t index;

    CSeq_id qid("gi|3417288");
    CRef<CSeq_loc> qloc(new CSeq_loc());
    for (index = 0; index < kNumInts; ++index) {
        CRef<CSeq_loc> next_loc(new CSeq_loc());
        next_loc->SetInt().SetFrom(kStarts[index]);
        next_loc->SetInt().SetTo(kEnds[index]);
        next_loc->SetInt().SetId(qid);
        qloc->SetMix().Set().push_back(next_loc);
    }

    CRef<CScope> scope(new CScope(CTestObjMgr::Instance().GetObjMgr()));
    scope->AddDefaults();

    auto_ptr<SSeqLoc> query(new SSeqLoc(qloc, scope));

    CSeq_id sid("gi|51511732");
    pair<TSeqPos, TSeqPos> range(15595732, 15705419);
    auto_ptr<SSeqLoc> subject(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range, eNa_strand_both));
    CBl2Seq blaster(*query, *subject, eBlastn);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align> sar = *(sav[0]->Get().begin());
    BOOST_REQUIRE_EQUAL(60, (int)sar->GetSegs().GetDisc().Get().size());
}
#endif

BOOST_AUTO_TEST_CASE(QueryMaskIgnoredInMiniExtension) {
    CRef<CSeq_loc> qloc(new CSeq_loc());
    qloc->SetWhole().SetGi(4505696);
    CSeq_id sid("gi|29809252");
    pair<TSeqPos, TSeqPos> range(662070, 662129);

    CRef<CScope> scope(new CScope(CTestObjMgr::Instance().GetObjMgr()));
    scope->AddDefaults();

    auto_ptr<SSeqLoc> query(new SSeqLoc(qloc, scope));
    auto_ptr<SSeqLoc> subject(
        CTestObjMgr::Instance().CreateSSeqLoc(sid, range, eNa_strand_both));

    CBl2Seq blaster(*query, *subject, eMegablast);
    TSeqAlignVector sav(blaster.Run());
    CRef<CSeq_align_set> sas = sav.front();
    BOOST_REQUIRE(sas->Get().empty());
}

#endif /* SKIP_DOXYGEN_PROCESSING */

BOOST_AUTO_TEST_SUITE_END()
