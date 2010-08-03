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
* Author:  Ilya Dondoshansky
*
* File Description:
*   Unit test module for sequence source functionality
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <algo/blast/api/seqsrc_multiseq.hpp>
#include <algo/blast/api/seqsrc_seqdb.hpp>
#include <algo/blast/core/blast_util.h>
#include "blast_objmgr_priv.hpp"

#ifdef KAPPA_PRINT_DIAGNOSTICS
/* C toolkit ! */
#include <algo/blast/api/seqsrc_readdb.h>
#endif /* KAPPA_PRINT_DIAGNOSTICS */

#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <corelib/ncbithr.hpp>

#include "test_objmgr.hpp"

using namespace std;
USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);

// Retrieves a vector of sequences from a file
static TSeqLocVector s_getSequences()
{
    const int kNumSeqs = 70;

    const int kGiList[kNumSeqs] = 
        { 1786181, 1786192, 2367095, 1786217, 1786230, 1786240, 1786250, 
          1786262, 1786283, 1786298, 1786306, 1786315, 1786327, 1786339, 
          1786348, 1786358, 1786370, 1786383, 1786395, 1786402, 1786415, 
          2367098, 2367099, 1786454, 1786465, 2367103, 2367108, 1786501, 
          1786510, 1786520, 1786532, 1786542, 1786554, 1786568, 1786580, 
          1786596, 1786603, 1786614, 1786628, 1786639, 1786649, 1786660, 
          1786671, 1786683, 1786692, 1786705, 1786716, 1786728, 1786739, 
          1786751, 1786766, 1786782, 1786790, 1786800, 1786808, 1786819, 
          1786836, 1786849, 1786862, 1786875, 1786888, 1786896, 1786910, 
          1786920, 1786934, 1786947, 1786955, 1786967, 1786978, 1786988 };
    TSeqLocVector retval;
    char id_buffer[16];
    int index;

    for (index = 0; index < kNumSeqs; ++index) {
        sprintf(id_buffer, "gi|%d", kGiList[index]);
        CSeq_id id(id_buffer);
        auto_ptr<SSeqLoc> sl(
                             CTestObjMgr::Instance().
                             CreateSSeqLoc(id, eNa_strand_both));
        retval.push_back(*sl);
    }

    return retval;
}

BOOST_AUTO_TEST_SUITE(seqsrc)

#ifdef KAPPA_PRINT_DIAGNOSTICS

static void 
s_TestGetGis(BlastSeqSrc* seqsrc, const pair<string, bool>& dbinfo) {
    const int gis[] = { 68737, 129295 };

    CSeqDB seqdb(dbinfo.first, dbinfo.second
                 ? CSeqDB::eProtein : CSeqDB::eNucleotide);
    int oid = -1;
    seqdb.GiToOid(gis[0], oid);
    BOOST_REQUIRE(oid != -1);

    Blast_GiList* gilist = BlastSeqSrcGetGis(seqsrc, (void*)&oid);
    sort(&gilist->data[0], &gilist->data[gilist->num_used]);
    for (size_t i = 0; i < gilist->num_used; i++) {
        BOOST_REQUIRE_EQUAL(gilist->data[i], gis[i]);
    }
    gilist = Blast_GiListFree(gilist);
}

BOOST_AUTO_TEST_CASE(testGetGisSeqDb) {
    const pair<string, bool> kDbInfo("nr", true);

    BlastSeqSrc* seq_src = SeqDbBlastSeqSrcInit(kDbInfo.first,
                                                kDbInfo.second);
    char* error_str = BlastSeqSrcGetInitError(seq_src);
    BOOST_REQUIRE(error_str == NULL);

    s_TestGetGis(seq_src, kDbInfo);

    BlastSeqSrcFree(seq_src);
}

BOOST_AUTO_TEST_CASE(testGetGisReaddb) {
    const pair<string, bool> kDbInfo("nr", true);
    BlastSeqSrc* seq_src = ReaddbBlastSeqSrcInit(kDbInfo.first.c_str(),
                                                 kDbInfo.second, 0, 0);
    char* error_str = BlastSeqSrcGetInitError(seq_src);
    BOOST_REQUIRE(error_str == NULL);

    s_TestGetGis(seq_src, kDbInfo);

    BlastSeqSrcFree(seq_src);
}

#endif /* KAPPA_PRINT_DIAGNOSTICS */

BOOST_AUTO_TEST_CASE(testMultiSeqSrc_FailureToInitialize) {
    TSeqLocVector sequences = s_getSequences();
    // sequences contains nucleotide sequences, thus initialization should
    // fail
    BlastSeqSrc* seq_src = 
        MultiSeqBlastSeqSrcInit(sequences, eBlastTypeBlastp);
    char* error_str = BlastSeqSrcGetInitError(seq_src);
    BOOST_REQUIRE(error_str != NULL);
    sfree(error_str);
    BlastSeqSrcFree(seq_src);
}

BOOST_AUTO_TEST_CASE(testMultiSeqSrc) {
    const int kNumBytes = 4;
    const int kSeqIndex = 40;
    const Uint1 kNcbi2naSeqBytes[kNumBytes] = { 48, 215, 159, 129 };
    const Uint1 kBlastnaSeqBytes[kNumBytes] = { 0, 3, 0, 0 };
    
    TSeqLocVector sequences = s_getSequences();
    const int kNumSeqs = (int) sequences.size();
    BlastSeqSrc* seq_src = 
        MultiSeqBlastSeqSrcInit(sequences, eBlastTypeBlastn);
    
    BOOST_REQUIRE_EQUAL(kNumSeqs, (int)BlastSeqSrcGetNumSeqs(seq_src));
    BOOST_REQUIRE_EQUAL(0, (int) BlastSeqSrcGetTotLen(seq_src));
    BOOST_REQUIRE(BlastSeqSrcGetIsProt(seq_src) == false);
    BOOST_REQUIRE(strcmp(BlastSeqSrcGetName(seq_src), NcbiEmptyCStr) == 0);
    
    BlastSeqSrcIterator* itr = BlastSeqSrcIteratorNew();
    int index, last_index=0;
    Uint4 length, max_len=0, avg_len=0;

    while ((index = BlastSeqSrcIteratorNext(seq_src, itr)) 
           != BLAST_SEQSRC_EOF) {
        ++last_index;
        length = BlastSeqSrcGetSeqLen(seq_src, (void*)&index);
        BOOST_REQUIRE_EQUAL(sequence::GetLength(*sequences[index].seqloc, sequences[index].scope), length);
        
        avg_len += length;
        max_len = MAX(max_len, length);
    }
    itr = BlastSeqSrcIteratorFree(itr);
    BOOST_REQUIRE_EQUAL(kNumSeqs, last_index);
    
    avg_len /= kNumSeqs;
    
    BOOST_REQUIRE_EQUAL((int)max_len, BlastSeqSrcGetMaxSeqLen(seq_src));
    BOOST_REQUIRE_EQUAL((int)avg_len, BlastSeqSrcGetAvgSeqLen(seq_src));

    BlastSeqSrcGetSeqArg* seq_arg = (BlastSeqSrcGetSeqArg*) calloc(1, sizeof(BlastSeqSrcGetSeqArg));
    seq_arg->oid = kSeqIndex;
    BOOST_REQUIRE(BlastSeqSrcGetSequence(seq_src, seq_arg) >= 0);
    // Sequence length
    BOOST_REQUIRE_EQUAL((int)sequence::GetLength(*sequences[kSeqIndex].seqloc, 
                                                  sequences[kSeqIndex].scope), 
                         seq_arg->seq->length);
    // Check first few bytes of a compressed sequence
    for (index = 0; index < kNumBytes; ++index) {
        BOOST_REQUIRE_EQUAL(kNcbi2naSeqBytes[index], 
                             seq_arg->seq->sequence[index]);
    }
    BlastSequenceBlkClean(seq_arg->seq);
    
    // Now check the uncompressed sequence retrieval
    seq_arg->encoding = eBlastEncodingNucleotide;
    BOOST_REQUIRE(BlastSeqSrcGetSequence(seq_src, seq_arg) >= 0);
    for (index = 0; index < kNumBytes; ++index) {
        BOOST_REQUIRE_EQUAL(kBlastnaSeqBytes[index], 
                             seq_arg->seq->sequence[index]);
    }
    BlastSequenceBlkFree(seq_arg->seq);
    sfree(seq_arg);
    
    BlastSeqSrcFree(seq_src);
}
    
BOOST_AUTO_TEST_CASE(testSeqDBSrc_FailureToInitialize_NullDb) {
    string null_db("");
    BlastSeqSrc* seq_src = SeqDbBlastSeqSrcInit(null_db, true);
    char* error_str = BlastSeqSrcGetInitError(seq_src);
    BOOST_REQUIRE(error_str != NULL);
    sfree(error_str);
    BlastSeqSrcFree(seq_src);
}

BOOST_AUTO_TEST_CASE(testSeqDBSrc_FailureToInitialize_NonExistentDb) {
    BlastSeqSrc* seq_src = SeqDbBlastSeqSrcInit("junk", false);
    char* error_str = BlastSeqSrcGetInitError(seq_src);
    BOOST_REQUIRE(error_str != NULL);
    sfree(error_str);
    BlastSeqSrcFree(seq_src);
}

static void 
s_checkDbSeqSrcFunctions(BlastSeqSrc* seq_src, const char* dbname)
{
    const int kNumSeqs = 2004;
    const int kRealNumSeqs = 1000;
    const int kMaxLen = 875;
    const int kMaxLenRange = 839;
    const int kTotLen = 943942;
    const int kTotLenRange = 478404;
    const int kAvgLen = 471;
    const int kSeqIndex = 1500;
    const int kSeqLength = 715;
    const int kNumBytes_2na = 4;
    const int kNumBytes_4na = 5;
    const int kNcbi2naSeqBytes[kNumBytes_2na] = { 159, 145, 213, 43 };
    const int kBlastnaSeqBytes[kNumBytes_4na] = { 15, 2, 1, 3, 3 };

    BOOST_REQUIRE(BlastSeqSrcGetIsProt(seq_src) == false);
    BOOST_REQUIRE_EQUAL(kNumSeqs, BlastSeqSrcGetNumSeqs(seq_src));
    BOOST_REQUIRE_EQUAL(kTotLen, (int) BlastSeqSrcGetTotLen(seq_src));
    BOOST_REQUIRE_EQUAL(kMaxLen, BlastSeqSrcGetMaxSeqLen(seq_src));
    BOOST_REQUIRE_EQUAL(kAvgLen, BlastSeqSrcGetAvgSeqLen(seq_src));

    BOOST_REQUIRE(!strcmp(dbname, BlastSeqSrcGetName(seq_src)));

    BlastSeqSrcIterator* itr = BlastSeqSrcIteratorNew();
    int index, last_index=0;
    Int8 total_length = 0;
    Int4 max_length = 0;
    Int4 length = 0;
    // Test the iterator
    while ((index = BlastSeqSrcIteratorNext(seq_src, itr)) 
           != BLAST_SEQSRC_EOF) {
        ++last_index;
        length = BlastSeqSrcGetSeqLen(seq_src, (void*)&index);
        max_length = MAX(max_length, length);
        total_length += (Int8) length;
    }
    itr = BlastSeqSrcIteratorFree(itr);
   
    BOOST_REQUIRE_EQUAL(kRealNumSeqs, last_index);
    BOOST_REQUIRE_EQUAL(kTotLenRange, (int)total_length);
    BOOST_REQUIRE_EQUAL(kMaxLenRange, max_length);

    // Test a particular sequence
    BlastSeqSrcGetSeqArg* seq_arg = (BlastSeqSrcGetSeqArg*) calloc(1, sizeof(BlastSeqSrcGetSeqArg));
    seq_arg->oid = kSeqIndex;
    BOOST_REQUIRE(BlastSeqSrcGetSequence(seq_src, seq_arg) >= 0);
    // Sequence length
    BOOST_REQUIRE_EQUAL(kSeqLength, seq_arg->seq->length);
    BOOST_REQUIRE_EQUAL(kSeqLength, 
                         BlastSeqSrcGetSeqLen(seq_src, (void*) &seq_arg->oid));
    // Check first few bytes of a compressed sequence
    for (index = 0; index < kNumBytes_2na; ++index) {
        BOOST_REQUIRE_EQUAL(kNcbi2naSeqBytes[index], 
                             (int)seq_arg->seq->sequence[index]);
    }
    BlastSeqSrcReleaseSequence(seq_src, seq_arg);

    // Now check the uncompressed sequence retrieval
    seq_arg->encoding = eBlastEncodingNucleotide;
    BOOST_REQUIRE(BlastSeqSrcGetSequence(seq_src, seq_arg) >= 0);
    for (index = 0; index < kNumBytes_4na; ++index) {
        BOOST_REQUIRE(seq_arg != NULL);
        BOOST_REQUIRE(seq_arg->seq != NULL);
        BOOST_REQUIRE(seq_arg->seq->sequence_start != NULL);
        BOOST_REQUIRE_EQUAL(kBlastnaSeqBytes[index], 
                             (int)seq_arg->seq->sequence_start[index]);
    }
    BlastSeqSrcReleaseSequence(seq_src, seq_arg);
    BlastSequenceBlkFree(seq_arg->seq);

    sfree(seq_arg);

}

BOOST_AUTO_TEST_CASE(testSeqDBSrc) {
    const char* kDbName = "data/seqn";
    const Uint4 kFirstSeq = 1000;
    const Uint4 kFinalSeq = 2000;
    BlastSeqSrc* seq_src = 
        SeqDbBlastSeqSrcInit(kDbName, false, kFirstSeq, kFinalSeq);
    s_checkDbSeqSrcFunctions(seq_src, kDbName);
    BlastSeqSrcFree(seq_src);
}

BOOST_AUTO_TEST_CASE(testSeqDBSrcExisting)
{
    // Test that a SeqDB object can be shared with the SeqSrc
    // mechanism safely.
    
    const char* kDbName = "data/seqn";
    const Uint4 kFirstSeq = 1000;
    const Uint4 kFinalSeq = 2000;
    
    CRef<CSeqDB> seqdb(new CSeqDB(kDbName, CSeqDB::eNucleotide, kFirstSeq, kFinalSeq, true));
    
    string title(seqdb->GetTitle());
    
    BlastSeqSrc* seq_src = SeqDbBlastSeqSrcInit(seqdb);
    
    s_checkDbSeqSrcFunctions(seq_src, kDbName);
    BlastSeqSrcFree(seq_src);
    
    string title2(seqdb->GetTitle());
    
    BOOST_REQUIRE_EQUAL(title, title2);
}

BOOST_AUTO_TEST_CASE(testSeqDBSrcShared)
{
    // Further test - multiple SeqSrc objects can use the same
    // SeqDB object.
    
    const char* kDbName = "data/seqn";
    const Uint4 kFirstSeq = 1000;
    const Uint4 kFinalSeq = 2000;
    
    BlastSeqSrc * seq_src2 = 0;
    
    {
        BlastSeqSrc * seq_src1 = 0;
        
        CRef<CSeqDB> seqdb(new CSeqDB(kDbName, CSeqDB::eNucleotide, kFirstSeq, kFinalSeq, true));
        
        seq_src1 = SeqDbBlastSeqSrcInit(seqdb);
        seq_src2 = SeqDbBlastSeqSrcInit(seqdb);
        
        BlastSeqSrcGetSeqArg seq_arg1, seq_arg2;
        memset((void*) &seq_arg1, 0, sizeof(BlastSeqSrcGetSeqArg));
        memset((void*) &seq_arg2, 0, sizeof(BlastSeqSrcGetSeqArg));
        seq_arg1.oid = seq_arg2.oid = 5;
        
        s_checkDbSeqSrcFunctions(seq_src1, kDbName);
        
        
        BOOST_REQUIRE(BlastSeqSrcGetSequence(seq_src1, &seq_arg1) >= 0);
        BOOST_REQUIRE(BlastSeqSrcGetSequence(seq_src2, &seq_arg2) >= 0);
        
        BOOST_REQUIRE(seq_arg1.seq);
        BOOST_REQUIRE(seq_arg2.seq);
        
        BlastSeqSrcReleaseSequence(seq_src1, &seq_arg1);
        BlastSeqSrcReleaseSequence(seq_src2, &seq_arg2);
        BlastSequenceBlkFree(seq_arg1.seq);
        BlastSequenceBlkFree(seq_arg2.seq);
        
        BlastSeqSrcFree(seq_src1);
    }
    
    // The SeqDB object should not be deleted until here.
    BlastSeqSrcFree(seq_src2);
}
    
// Disabled because boost does not support MT testing
#if 0
/// Structure containing counts that are updated during iteration. 
/// Updating total length and maximal length is expensive, so it is not done
/// for large databases.
typedef struct SSeqSrcTestInfo {
    Int4 num_seqs;     ///< Number of sequences.
    Int4 max_length;   ///< Maximal length.
    Int8 total_length; ///< Total length of sequences.
} SSeqSrcTestInfo;

/// Enumeration specifying what to database lengths to check in iteration
/// tests.
typedef enum ECheckLengths {
    eCheckNone = 0,    ///< Test only number of sequences.
    eCheckTotal,       ///< Also test total database length and average 
                       /// sequence length
    eCheckTotalAndMax  ///< Also test maximal sequence length. This can only
                       /// work for real databases, not restricted to range 
                       /// or OID mask.
} ECheckLengths;

/// Class for multi-threaded iteration over a BlastSeqSrc.
class CSeqSrcTestThread : public CThread
{

    BlastSeqSrc* m_ipSeqSrc;
    CFastMutex* m_pMutex;
    SSeqSrcTestInfo* m_pTestInfo;
    ECheckLengths m_iCheckLengths;

    CSeqSrcTestThread(BlastSeqSrc* seq_src, CFastMutex* mutex, 
                      SSeqSrcTestInfo* info, ECheckLengths cl) 
        : m_pMutex(mutex), m_pTestInfo(info), m_iCheckLengths(cl)
    {
        m_ipSeqSrc = BlastSeqSrcCopy(seq_src);
    }
    ~CSeqSrcTestThread() {}

    virtual void* Main(void)
    {
        BlastSeqSrcIterator* itr = BlastSeqSrcIteratorNew();
        Int4 oid;
        Int4 counter = 0;
        Int4 length, max_len = 0;
        Int8 tot_len = 0;

        while ( (oid = BlastSeqSrcIteratorNext(m_ipSeqSrc, itr))
                != BLAST_SEQSRC_EOF) {
            if (oid == BLAST_SEQSRC_ERROR)
                break;
            ++counter;
            if (m_iCheckLengths != eCheckNone) {
                length = BlastSeqSrcGetSeqLen(m_ipSeqSrc, (void*)&oid);
                max_len = MAX(max_len, length);
                tot_len += (Int8) length;
            }
        }

        m_ipSeqSrc = BlastSeqSrcFree(m_ipSeqSrc);
        BlastSeqSrcIteratorFree(itr);

        CFastMutexGuard g(*m_pMutex);
        m_pTestInfo->num_seqs += counter;
        m_pTestInfo->total_length += tot_len;
        m_pTestInfo->max_length = MAX(m_pTestInfo->max_length, max_len);

        return NULL;
    }
    virtual void OnExit(void) {}
};

/// Tests multi-threaded BlastSeqSrc iterator functionality 
/// and frees BlastSeqSrc when done.
/// @param seq_src Pointer to the BlastSeqSrc structure. [in]
/// @param check_lengths Which lengths should be calculated in the iteration
///                      and tested? [in]
/// @param num_threads How many threads to use? [in]
/// @param numseqs Number of sequences in database, used if database is 
///                restricted to a range; real number of sequences is used
///                if 0 is passed. [in]
static void 
s_TestSeqSrcIteratorMT(BlastSeqSrc** seq_src_ptr, 
                       ECheckLengths check_lengths, 
                       int num_threads, int numseqs)
{
    vector< CRef<CSeqSrcTestThread> > test_thread_v;
    test_thread_v.reserve(num_threads);
    CFastMutex mutex;
    int thr_index;
    BlastSeqSrc* seq_src;

    if (!seq_src_ptr)
        return;
    
    seq_src = *seq_src_ptr;
    
    if (numseqs == 0)
        numseqs = BlastSeqSrcGetNumSeqs(seq_src);

    SSeqSrcTestInfo* info = 
        (SSeqSrcTestInfo*) calloc(1, sizeof(SSeqSrcTestInfo));

    for (thr_index = 0; thr_index < num_threads; ++thr_index) {
        CRef<CSeqSrcTestThread> 
            next_thread(new CSeqSrcTestThread(seq_src, &mutex, info, 
                                              check_lengths));
        test_thread_v.push_back(next_thread);
        next_thread->Run();
    }
    for (thr_index = 0; thr_index < num_threads; ++thr_index) {
        test_thread_v[thr_index]->Join();
    }
    BOOST_REQUIRE_EQUAL(numseqs, info->num_seqs);
    if (check_lengths != eCheckNone) {
        BOOST_REQUIRE_EQUAL(BlastSeqSrcGetTotLen(seq_src), 
                             info->total_length);
        BOOST_REQUIRE_EQUAL(BlastSeqSrcGetAvgSeqLen(seq_src), 
                             (int) (info->total_length/info->num_seqs));
        if (check_lengths == eCheckTotalAndMax) {
            BOOST_REQUIRE_EQUAL(BlastSeqSrcGetMaxSeqLen(seq_src), 
                                 info->max_length);
        }
    } 

    sfree(info);
    *seq_src_ptr = BlastSeqSrcFree(seq_src);
}

/// Finds a range in a multi-volume database which spans several volumes.
/// Range spans 'num' sequences in starting volume and ending volume, plus
/// everything in between.
/// @param dbname Name of the database [in]
/// @param is_prot Is database nucleotide or protein? [in]
/// @param vol_start Starting volume [in]
/// @param vol_end Ending volume [in]
/// @param num Number of sequences in first and last volumes to include in
///            the range [in]
/// @param first_seq First ordinal id in the range [out]
/// @param last_seq Last ordinal id in the range [out]
/// @param use_seqdb Use readdb or SeqDb interface? [in]
static void 
s_GetRangeBounds(const string& dbname, bool is_prot, 
                 int vol_start, int vol_end, int num, 
                 Uint4* first_seq, Uint4* last_seq,
                 bool use_seqdb)
{
    string vol_name;
    int index;
    int seqs_count = 0; 
    CSeqDB::ESeqType prot_nucl = (is_prot ? CSeqDB::eProtein : CSeqDB::eNucleotide);

    for (index = 0; index <= vol_start; ++index) {
        vol_name = dbname + ".0" + NStr::IntToString(index);
        CSeqDB seqdb(vol_name, prot_nucl);
        
        int nseqs(0);
        seqdb.GetTotals(CSeqDB::eFilteredAll, & nseqs, 0, true);
        
        seqs_count += nseqs;
    }
    *first_seq = seqs_count - num;
    for ( ; index < vol_end; ++index) {
        vol_name = dbname + ".0" + NStr::IntToString(index);
        CSeqDB seqdb(vol_name, prot_nucl);
        
        int nseqs(0);
        seqdb.GetTotals(CSeqDB::eFilteredAll, & nseqs, 0, true);
        
        seqs_count += nseqs;
    }
    *last_seq = seqs_count + num;
}

/// Tests multi-threaded SeqDB iteration.
static void
s_TestSeqDbIterator(const string& dbname, bool is_prot, int first_seq, 
                    int last_seq, int nthreads, ECheckLengths check_lengths)
{
    BlastSeqSrc* seq_src = 
        SeqDbBlastSeqSrcInit(dbname, is_prot, first_seq, last_seq);
    s_TestSeqSrcIteratorMT(&seq_src, check_lengths, nthreads, 
                           last_seq - first_seq);
}

/// Plain single volume seqdb, 2 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_patnt_2)
{
    s_TestSeqDbIterator("patnt", false, 0, 0, 2, eCheckNone);
}

/// Plain single volume seqdb, 4 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_patnt_4)
{
    s_TestSeqDbIterator("patnt", false, 0, 0, 4, eCheckNone);
}

/// Plain single volume seqdb, 20 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_patnt_20)
{
    s_TestSeqDbIterator("patnt", false, 0, 0, 20, eCheckNone);
}

/// Plain single volume seqdb, restricted to range of ordinal ids,
/// 2 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_patnt_range_2)
{
    s_TestSeqDbIterator("patnt", false, 1000, 10000, 2, eCheckNone);
}

/// Plain single volume seqdb, restricted to range of ordinal ids,
/// 4 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_patnt_range_4)
{
    s_TestSeqDbIterator("patnt", false, 1000, 10000, 4, eCheckNone);
}

/// Plain single volume seqdb, restricted to range of ordinal ids,
/// 2 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_patnt_range_20)
{
    s_TestSeqDbIterator("patnt", false, 1000, 10000, 20, eCheckNone);
}

/// Multi-volume seqdb, 2 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_nt_2)
{ 
    s_TestSeqDbIterator("nt", false, 0, 0, 2, eCheckNone);
}

/// Multi-volume seqdb, 4 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_nt_4)
{
    s_TestSeqDbIterator("nt", false, 0, 0, 4, eCheckNone);
}

/// Multi-volume seqdb, 20 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_nt_20)
{
    s_TestSeqDbIterator("nt", false, 0, 0, 20, eCheckNone);
}

/// Multi volume seqdb, restricted to range of ordinal ids spanning 
/// 3 volumes;
void s_TestSeqDbIterator_nt_range(int nthreads)
{
    const int kNumSeqs = 1000;
    const string kDbName("nt");
    Uint4 first_seq;
    Uint4 last_seq;

    s_GetRangeBounds(kDbName, false, 0, 2, kNumSeqs, &first_seq, &last_seq,
                     true);
    s_TestSeqDbIterator(kDbName, false, first_seq, last_seq,  nthreads, 
                        eCheckNone);
}

/// Multi volume seqdb, restricted to range of ordinal ids spanning 
/// 3 volumes; 2 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_nt_range_2)
{
    s_TestSeqDbIterator_nt_range(2);
}

/// Multi volume seqdb, restricted to range of ordinal ids spanning 
/// 3 volumes; 4 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_nt_range_4)
{
    s_TestSeqDbIterator_nt_range(4);
}

/// Multi volume seqdb, restricted to range of ordinal ids spanning 
/// 3 volumes; 20 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_nt_range_20)
{
    s_TestSeqDbIterator_nt_range(20);
}

/// Multi-volume protein seqdb, 2 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_pataa_multi_2)
{
    s_TestSeqDbIterator("UnitTest/pataa_multivol", true, 0, 0, 2, 
                        eCheckTotalAndMax);
}

/// Multi-volume protein seqdb, 4 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_pataa_multi_4)
{
    s_TestSeqDbIterator("UnitTest/pataa_multivol", true, 0, 0, 4,
                        eCheckTotalAndMax);
}

/// Multi-volume protein seqdb, 20 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_pataa_multi_20)
{
    s_TestSeqDbIterator("UnitTest/pataa_multivol", true, 0, 0, 20, 
                        eCheckTotalAndMax);
}

/// Multi volume readdb, restricted to range of ordinal ids spanning 
/// 2 volumes;
void s_TestSeqDbIterator_pataa_multi_range(int nthreads)
{
    const int kNumSeqs = 1000;
    const string kDbName("UnitTest/pataa_multivol");
    Uint4 first_seq;
    Uint4 last_seq;

    s_GetRangeBounds(kDbName, true, 0, 1, kNumSeqs, 
                     &first_seq, &last_seq, true);
    s_TestSeqDbIterator(kDbName, true, first_seq, last_seq, nthreads, 
                        eCheckNone);
}

/// Multi volume readdb, restricted to range of ordinal ids spanning 
/// 2 volumes; 2 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_pataa_multi_range_2)
{
    s_TestSeqDbIterator_pataa_multi_range(2);
}

/// Multi volume readdb, restricted to range of ordinal ids spanning 
/// 2 volumes; 4 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_pataa_multi_range_4)
{
    s_TestSeqDbIterator_pataa_multi_range(4);
}

/// Multi volume readdb, restricted to range of ordinal ids spanning 
/// 2 volumes; 20 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_pataa_multi_range_20)
{
    s_TestSeqDbIterator_pataa_multi_range(20);
}

/// Multi-volume masked protein seqdb, 2 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_pataa_emb_2)
{
    s_TestSeqDbIterator("UnitTest/pataa_emb", true, 0, 0, 2, eCheckTotal);
}

/// Multi-volume masked protein seqdb, 4 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_pataa_emb_4)
{
    s_TestSeqDbIterator("UnitTest/pataa_emb", true, 0, 0, 4, eCheckTotal);
}

/// Multi-volume masked protein seqdb, 20 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_pataa_emb_20)
{
    s_TestSeqDbIterator("UnitTest/pataa_emb", true, 0, 0, 20, eCheckTotal);
}

/// Single volume with mask protein seqdb, 2 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_pdbaa_2)
{
    s_TestSeqDbIterator("pdbaa", true, 0, 0, 2, eCheckTotal);
}

/// Single volume with mask protein seqdb, 4 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_pdbaa_4)
{
    s_TestSeqDbIterator("pdbaa", true, 0, 0, 4, eCheckTotal);
}

/// Single volume with mask protein seqdb, 20 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_pdbaa_20)
{
    s_TestSeqDbIterator("pdbaa", true, 0, 0, 20, eCheckTotal);
}

/// Multi-volume with mask seqdb, 2 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_pdbnt_2)
{
    s_TestSeqDbIterator("pdbnt", false, 0, 0, 2, eCheckTotal);
}

/// Multi-volume with mask seqdb, 4 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_pdbnt_4)
{
    s_TestSeqDbIterator("pdbnt", false, 0, 0, 4, eCheckTotal);
}

/// Multi-volume with mask seqdb, 20 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_pdbnt_20)
{
    s_TestSeqDbIterator("pdbnt", false, 0, 0, 20, eCheckTotal);
}

/// Mixed database seqdb, 2 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_mixed_2)
{
    s_TestSeqDbIterator("pataa pdbaa", true, 0, 0, 2, eCheckTotal);
}

/// Mixed database seqdb, 4 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_mixed_4)
{
    s_TestSeqDbIterator("pataa pdbaa", true, 0, 0, 4, eCheckTotal);
}

/// Mixed database seqdb, 20 threads
BOOST_AUTO_TEST_CASE(testSeqDbIterator_mixed_20)
{
    s_TestSeqDbIterator("pataa pdbaa", true, 0, 0, 20, eCheckTotal);
}
#endif

BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================
*
* $Log: seqsrc-cppunit.cpp,v $
* Revision 1.42  2008/10/27 17:00:12  camacho
* Fix include paths to deprecated headers
*
* Revision 1.41  2008/05/29 13:59:34  camacho
* Update after merge of mask_subjects branch
*
* Revision 1.39  2007/11/01 21:35:03  camacho
* Updated for recent changes in BlastSeqSrc
*
* Revision 1.38  2006/05/31 19:00:31  camacho
* + KAPPA_PRINT_DIAGNOSTICS debugging feature
*
* Revision 1.37  2006/02/17 17:35:47  camacho
* Update to reflect new return value of BlastSeqSrcGetName
*
* Revision 1.36  2006/01/09 21:10:17  bealer
* - Use GetTotals() method.
* - Fix some summary information for data directory cases.
*
* Revision 1.35  2005/12/22 21:18:40  bealer
* - Fix remote blast "null" input.
* - SeqSrc now tests for correct totals from iteration.
*
* Revision 1.34  2005/06/09 20:37:06  camacho
* Use new private header blast_objmgr_priv.hpp
*
* Revision 1.33  2005/05/17 22:16:21  dondosha
* Testing of C versions of BlastSeqSrc moved to the C toolkit area
*
* Revision 1.32  2005/05/10 16:09:04  camacho
* Changed *_ENCODING #defines to EBlastEncoding enumeration
*
* Revision 1.31  2005/04/21 15:00:54  dondosha
* Removed unused void* argument in ReaddbBlastSeqSrcInit
*
* Revision 1.30  2005/04/18 14:01:55  camacho
* Updates following BlastSeqSrc reorganization
*
* Revision 1.29  2005/04/13 22:36:46  camacho
* Renamed BlastSeqSrc RetSequence to ReleaseSequence
*
* Revision 1.28  2005/04/06 21:26:37  dondosha
* GapEditBlock structure and redundant fields in BlastHSP have been removed
*
* Revision 1.27  2005/03/08 23:26:18  bealer
* - Use eProtein and eNucleotide.
*
* Revision 1.26  2005/03/04 17:20:45  bealer
* - Command line option support.
*
* Revision 1.25  2005/01/27 02:33:25  dondosha
* Added test for ReaddbBlastSeqSrcAttach with NULL input
*
* Revision 1.24  2004/12/29 15:14:44  camacho
* Fix memory leak
*
* Revision 1.23  2004/12/20 17:03:50  bealer
* - Unit test for SeqDB seqsrc with shared SeqDB objects:
*
* Revision 1.22  2004/12/17 21:49:48  bealer
* - Fix indentations.
*
* Revision 1.21  2004/12/09 15:29:28  dondosha
* Refactored code; renamed constants according to toolkit guideline; added more tests
*
* Revision 1.20  2004/12/06 21:42:37  bealer
* - Re-enable test.
*
* Revision 1.19  2004/12/01 22:30:28  camacho
* Removed unused argument
*
* Revision 1.18  2004/11/23 22:03:33  camacho
* Remove readdb warning message
*
* Revision 1.17  2004/11/17 20:41:27  camacho
* Fix to previous commit
*
* Revision 1.16  2004/11/17 20:29:03  camacho
* Use new BlastSeqSrc initialization function names, added tests for BlastSeqSrc initialization failures
*
* Revision 1.15  2004/10/06 15:12:17  dondosha
* BlastSeqSrcGetName returns const char*
*
* Revision 1.14  2004/10/06 15:02:42  dondosha
* Removed testing of the functionality that is no longer provided
*
* Revision 1.13  2004/09/23 15:48:50  dondosha
* Added test for the BlastSeqSrcGetName method, previously effectively commented out
*
* Revision 1.12  2004/07/21 14:35:27  dondosha
* Added multi-threaded tests with various types of BLAST databases
*
* Revision 1.11  2004/07/19 15:05:00  dondosha
* Renamed multiseq_src to seqsrc_multiseq, seqdb_src to seqsrc_seqdb
*
* Revision 1.10  2004/07/19 14:12:33  dondosha
* Removed test of GetSeqLoc method, because this method has been purged from the interface
*
* Revision 1.9  2004/06/23 14:13:25  dondosha
* Call BlastSeqSrcReleaseSequence instead of BlastSequenceBlkClean when testing SeqDb
*
* Revision 1.8  2004/05/17 15:44:03  dondosha
* Memory leak fixes
*
* Revision 1.7  2004/05/17 14:01:16  dondosha
* Corrected range in testReadDBSeqSrc, due to a fix in seqsrc_readdb.c
*
* Revision 1.6  2004/04/28 19:43:33  dondosha
* Modified database tests to check a local database; enabled testSeqDBSrc
*
* Revision 1.5  2004/04/06 22:21:28  dondosha
* Added checks for all sequence source functions; added test for seqdb source, commented out because some parts are not working yet
*
* Revision 1.4  2004/03/24 22:14:23  dondosha
* Fixed memory leaks
*
* Revision 1.3  2004/03/23 16:10:34  camacho
* Minor changes to CTestObjMgr
*
* Revision 1.2  2004/03/22 20:26:33  dondosha
* Do not use reading sequences from file for testMultiSeqFile; fixed object manager error
*
* Revision 1.1  2004/03/15 20:07:54  dondosha
* Test to check sequence source functionality
*
* Revision 1.7  2004/03/09 18:58:56  dondosha
* Added extension parameters argument to BlastHitSavingParametersNew calls
*
* Revision 1.6  2004/03/01 14:14:50  madden
* Add check for number of identitical letters in final alignment
*
* Revision 1.5  2004/02/27 21:26:56  madden
* Cleanup testBLASTNTraceBack
*
* Revision 1.4  2004/02/27 21:22:25  madden
* Add tblastn test: testTBLASTNTraceBack
*
* Revision 1.3  2004/02/27 20:34:34  madden
* Add setUp and tearDown routines, use tearDown for deallocation
*
* Revision 1.2  2004/02/27 20:14:24  madden
* Add protein-protein test: testBLASTPTraceBack
*
* Revision 1.1  2004/02/27 19:45:35  madden
* Unit test for traceback (mostly BlastHSPListGetTraceback)
*
*
*
* ===========================================================================
*/
