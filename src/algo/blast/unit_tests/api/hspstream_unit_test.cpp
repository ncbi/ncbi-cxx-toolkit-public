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
*   Unit test module for checking implementations of the BlastHSPStream API
*
* ===========================================================================
*/

// force include of C++ toolkit; necessary for Mac OS X build
// to prevent duplicate Handle typedef
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <algo/blast/core/blast_hspstream.h>
#include <algo/blast/core/hspfilter_collector.h>

#include "test_objmgr.hpp"
#include "hspstream_test_util.hpp"
// For C++ mutex locking
#include <algo/blast/api/blast_mtlock.hpp>

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

BOOST_AUTO_TEST_SUITE(hspstream)

typedef enum EHSPStreamType {
    eHSPListCollector = 0,
    eHSPListQueue
} EHSPStreamType;

void testHSPStream(EHSPStreamType stream_type) {
    const int kNumQueries = 10;
    const int kNumThreads = 40;
    int num_hsp_lists = 1000;
    BlastHSPList* hsp_list = NULL;
    int num_reads=0, num_hsps=0;
    int status, write_status=0;
    int index;
        
    const EBlastProgramType kProgram = eBlastTypeBlastp;
    const bool kIsGapped = true;

    BlastExtensionOptions* ext_options = NULL;
    BlastExtensionOptionsNew(kProgram, &ext_options, kIsGapped);
    BlastHitSavingOptions* hit_options = NULL;
    BlastHitSavingOptionsNew(kProgram, &hit_options, kIsGapped);

    MT_LOCK lock = Blast_CMT_LOCKInit();

    BlastScoringOptions* scoring_options = NULL;
    BlastScoringOptionsNew(kProgram, &scoring_options);

    BlastHSPCollectorParams* col_params = BlastHSPCollectorParamsNew(
    hit_options, ext_options->compositionBasedStats,
    scoring_options->gapped_calculation);

    BlastHSPWriterInfo * writer_info = BlastHSPCollectorInfoNew(col_params);

	BlastHSPWriter* writer = BlastHSPWriterNew(&writer_info, NULL);
    BOOST_REQUIRE(writer_info == NULL);

    BlastHSPStream* hsp_stream = BlastHSPStreamNew(kProgram, ext_options, TRUE,
                                                   kNumQueries, writer);

	BlastHSPStreamRegisterMTLock(hsp_stream, lock);

    scoring_options = BlastScoringOptionsFree(scoring_options);
    BOOST_REQUIRE(scoring_options == NULL);
    ext_options = BlastExtensionOptionsFree(ext_options);
    BOOST_REQUIRE(ext_options == NULL);
    /* One written HSP list will be split into one per query. */
    num_reads = kNumQueries;
    num_hsps = 1;
    write_status = kBlastHSPStream_Error;

    // Writing a NULL does not cause error.
    status = BlastHSPStreamWrite(hsp_stream, &hsp_list);
    BOOST_REQUIRE_EQUAL(kBlastHSPStream_Success, status);

    // Writing an empty HSP list does causes neither error, nor a memory 
    // leak, but nothing gets written to the stream. 
    hsp_list = Blast_HSPListNew(0);
    status = BlastHSPStreamWrite(hsp_stream, &hsp_list);
    BOOST_REQUIRE_EQUAL(kBlastHSPStream_Success, status);

    vector<CRef<CHspStreamWriteThread> > write_thread_v;
    write_thread_v.reserve(kNumThreads);

    // Create threads to write HSP lists to stream
    for (index = 0; index < kNumThreads; ++index) {
        CRef<CHspStreamWriteThread> write_thread(
            new CHspStreamWriteThread(hsp_stream, index, kNumThreads, 
                                      num_hsp_lists, kNumQueries));
        write_thread_v.push_back(write_thread);
        write_thread->Run();
    }

    for (index = 0; index < kNumThreads; ++index) {
        write_thread_v[index]->Join();
    }
    num_hsp_lists = MIN(num_hsp_lists, col_params->prelim_hitlist_size);

    // For the collector-type stream, HSP lists should 
    // be read out of the HSPStream in order of increasing 
    // subject OID
    Int4 last_oid = -1;

    for (index = 0; index < num_hsp_lists*num_reads; ++index) {
        status = BlastHSPStreamRead(hsp_stream, &hsp_list);
        BOOST_REQUIRE_EQUAL(kBlastHSPStream_Success, status);
        BOOST_REQUIRE(hsp_list->oid >= last_oid);
        last_oid = hsp_list->oid;
        BOOST_REQUIRE_EQUAL(num_hsps, hsp_list->hspcnt);
        hsp_list = Blast_HSPListFree(hsp_list);
        BOOST_REQUIRE(hsp_list == NULL);
    }

    /* Check whether we can write more results. */
    hsp_list = setupHSPList(0, 1, 0);
    status = BlastHSPStreamWrite(hsp_stream, &hsp_list);
    BOOST_REQUIRE_EQUAL(write_status, status);
    /* For the case of the queue, read the just written HSP list */
    if (status == kBlastHSPStream_Success) {
        status = BlastHSPStreamRead(hsp_stream, &hsp_list);
    }
    hsp_list = Blast_HSPListFree(hsp_list);
    BOOST_REQUIRE(hsp_list == NULL);

    /* Close the stream for writing. Do it here, to imitate the situation
       when reading starts before the stream is explicitly closed for 
       writing. In the collector case, the first Read() call closes the 
       stream anyway, but repeated call to Close() does not hurt.*/
    BlastHSPStreamClose(hsp_stream);

    /* Now the HSPList collector should be empty */
    status = BlastHSPStreamRead(hsp_stream, &hsp_list);
    BOOST_REQUIRE_EQUAL(kBlastHSPStream_Eof, status);
    BOOST_REQUIRE(hsp_list == NULL);
    hsp_stream = BlastHSPStreamFree(hsp_stream);
    BOOST_REQUIRE(hsp_stream == NULL);
    hit_options = BlastHitSavingOptionsFree(hit_options);
    BOOST_REQUIRE(hit_options == NULL);
}

BOOST_AUTO_TEST_CASE(testCollectorHSPStream) {
    testHSPStream(eHSPListCollector);
}

BOOST_AUTO_TEST_CASE(testQueueHSPStream) {
    testHSPStream(eHSPListQueue);
}

BOOST_AUTO_TEST_CASE(testMultiSeqHSPCollector) {
    const int kNumSubjects = 10;
    const EBlastProgramType kProgram = eBlastTypeBlastp;
    
    BlastExtensionOptions* ext_options = NULL;
    BlastExtensionOptionsNew(kProgram, &ext_options, true);

    BlastScoringOptions* scoring_options = NULL;
    BlastScoringOptionsNew(kProgram, &scoring_options);

    BlastHitSavingOptions* hit_options = NULL;
    BlastHitSavingOptionsNew(kProgram, &hit_options,
                             scoring_options->gapped_calculation);

    BlastHSPWriterInfo * writer_info = BlastHSPCollectorInfoNew(
            BlastHSPCollectorParamsNew(
        hit_options, ext_options->compositionBasedStats,
        scoring_options->gapped_calculation));

	BlastHSPWriter* writer = BlastHSPWriterNew(&writer_info, NULL);
    BOOST_REQUIRE(writer_info == NULL);

    BlastHSPStream* hsp_stream = BlastHSPStreamNew(
        kProgram, ext_options, FALSE, 1, writer);

    scoring_options = BlastScoringOptionsFree(scoring_options);
    BOOST_REQUIRE(scoring_options == NULL);
    ext_options = BlastExtensionOptionsFree(ext_options);
    BOOST_REQUIRE(ext_options == NULL);

    BlastHSPList* hsp_list = NULL;
    // Create HSPLists for all odd subjects
    int index, status;
    for (index = 1; index < kNumSubjects; index += 2) { 
        hsp_list = setupHSPList(index, 1, index);
        status = BlastHSPStreamWrite(hsp_stream, &hsp_list);
        BOOST_REQUIRE_EQUAL(kBlastHSPStream_Success, status);
    }
    // Read them back and put in a BlastHSPResults structure
    BlastHSPResults* results = Blast_HSPResultsNew(1);
    for (index = 1; index < kNumSubjects; index += 2) {
        status = BlastHSPStreamRead(hsp_stream, &hsp_list);
        BOOST_REQUIRE_EQUAL(kBlastHSPStream_Success, status);
        Blast_HSPResultsInsertHSPList(results, hsp_list, kNumSubjects);
        /* Check that HSPLists are returned in correct order of 
           ordinal ids. */
        BOOST_REQUIRE_EQUAL(index, (int)hsp_list->oid);
    }
    /* Now the HSPList collector should be empty */
    status = BlastHSPStreamRead(hsp_stream, &hsp_list);
    BOOST_REQUIRE_EQUAL(kBlastHSPStream_Eof, status);
    BOOST_REQUIRE(hsp_list == NULL);
    hsp_stream = BlastHSPStreamFree(hsp_stream);
    BOOST_REQUIRE(hsp_stream == NULL);
    Blast_HSPResultsFree(results);
    hit_options = BlastHitSavingOptionsFree(hit_options);
    BOOST_REQUIRE(hit_options == NULL);
}
BOOST_AUTO_TEST_SUITE_END()
