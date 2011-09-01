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
* File Description:
*   Unit tests for k-mer counts computing classes
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <objmgr/object_manager.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_data.hpp>
#include <algo/cobalt/kmercounts.hpp>
#include "cobalt_test_util.hpp"

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
#define NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <boost/test/floating_point_comparison.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(cobalt);
USING_SCOPE(objects);


BOOST_AUTO_TEST_SUITE(kmer_counts)

static CRef<CBioseq> s_CreateBioseq(const string& sequence, int id)
{
    CRef<CBioseq> bioseq(new CBioseq());
    CSeq_data& seq_data = bioseq->SetInst().SetSeq_data();
    vector<char>& data = seq_data.SetNcbistdaa().Set();
    data.resize(sequence.length());
    for (size_t i=0;i < sequence.length();i++) {
        data[i] = (char)AMINOACID_TO_NCBISTDAA[(int)sequence[i]];
    }
    bioseq->SetInst().SetLength(sequence.length());
    bioseq->SetInst().SetMol(CSeq_inst::eMol_aa);

    bioseq->SetId().clear();
    bioseq->SetId().push_back(CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Local, id)));

    return bioseq;
}

template <class TKmerCounts>
static void s_CreateKmerCounts(const string& seq, TKmerCounts& counts)
{
    CRef<CBioseq> bioseq = s_CreateBioseq(seq, 1);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*objmgr));
    CBioseq_Handle handle = scope->AddBioseq(*bioseq);

    CSeq_loc seqloc(CSeq_loc::e_Whole);
    seqloc.SetId(*handle.GetSeqId());

    counts.Reset(seqloc, *scope);
}


BOOST_AUTO_TEST_CASE(TestSparseKmerCounts)
{
    CSparseKmerCounts::SetUseCompressed(false);

    const string seq1 = "AAAAAAAAA";
    const string seq2 = "BBBBBBBBBB";

    CSparseKmerCounts counts, counts2;
    for (int i=3;i < 6;i++) {
        for (int j=15;j <= 20;j++) {

            CSparseKmerCounts::SetKmerLength(i);
            CSparseKmerCounts::SetAlphabetSize(j);

            s_CreateKmerCounts(seq1, counts);
            s_CreateKmerCounts(seq2, counts2);

            // Number of k-mers is equal to seq_len - k-mer_len + 1
            // for sequences that do not contain X
            BOOST_CHECK_EQUAL(counts.GetNumCounts(), seq1.length()
                          - CSparseKmerCounts::GetKmerLength() + 1);

            // Sequence compared to itself, yields all k-mers common
            BOOST_CHECK_EQUAL(CSparseKmerCounts::CountCommonKmers(counts,
                                       counts), counts.GetNumCounts());

            // Each common k-mer is counted once in this case
            BOOST_CHECK_EQUAL((int)CSparseKmerCounts::CountCommonKmers(counts,
                                                       counts, false), 1);

            // For sequences that do not share subsequences, zero common k-mers
            BOOST_CHECK_EQUAL((int)CSparseKmerCounts::CountCommonKmers(counts,
                                                                  counts2), 0);

        }
    }

    const string seq_x = "AXAXAXAXAXAXAX";
    const string seq_xonly = "XXXXXXXXXXXXX";

    // Make sure that k-mers that contain X are not counted
    CSparseKmerCounts::SetAlphabetSize(20);
    for (int i=3;i < 6;i++) {

        CSparseKmerCounts::SetKmerLength(i);
        CSparseKmerCounts counts_x, counts_xonly;
        s_CreateKmerCounts(seq_x, counts_x);
        s_CreateKmerCounts(seq_xonly, counts_xonly);
        
        BOOST_CHECK_EQUAL((int)counts_x.GetNumCounts(), 0);
        BOOST_CHECK_EQUAL((int)counts_xonly.GetNumCounts(), 0);
    }

    const string seq_short = "ABC";

    // Sequence shorter than k-mer length causes exception
    CSparseKmerCounts::SetKmerLength(4);
    BOOST_CHECK_THROW(s_CreateKmerCounts(seq_short, counts),
                      CKmerCountsException);

    // Letter codes are assumed to belong to [1, alphabet_size]
    // Letter out of alphabet in sequence causes exception
    CSparseKmerCounts::SetAlphabetSize(5);
    BOOST_CHECK_THROW(s_CreateKmerCounts(seq_short + "AAK", counts),
                      CKmerCountsException);

    // Not specifying translation table for compressed alphabets causes 
    // exception
    CSparseKmerCounts::SetUseCompressed(true);
    CSparseKmerCounts::SetTransTable().clear();
    BOOST_CHECK_THROW(s_CreateKmerCounts(seq1, counts), CKmerCountsException);

    // K-mers containig compressed letters are counted as the same k-mers
    vector<Uint1>& table = CSparseKmerCounts::SetTransTable();
    table.resize(3);
    table[0] = 0;
    table[1] = 1;
    table[2] = 1;
    CSparseKmerCounts::SetUseCompressed(true);
    CSparseKmerCounts::SetAlphabetSize(2);
    CSparseKmerCounts::SetKmerLength(4);
    s_CreateKmerCounts(seq1, counts);
    s_CreateKmerCounts(seq2, counts2);
    BOOST_CHECK_EQUAL(CSparseKmerCounts::CountCommonKmers(counts, counts2),
                      counts.GetNumCounts());

}


BOOST_AUTO_TEST_CASE(TestKmerMethods)
{
    typedef TKmerMethods<CSparseKmerCounts> TKMethods;
    const int kKmerLen = 4;
    const int kAlphabetSize = 28;

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*objmgr));
    vector< CRef<CSeq_loc> > seqs;
    vector<CSparseKmerCounts> counts_vect;
    TKMethods::TDistMatrix dmat;

    int status = ReadFastaQueries("data/small.fa", seqs, scope);
    BOOST_REQUIRE_EQUAL(status, 0);
    BOOST_REQUIRE(seqs.size() > 0);
    BOOST_REQUIRE(!scope.Empty());

    // Supplying empty list of sequences results in exception
    BOOST_CHECK_THROW(TKMethods::ComputeCounts(vector< CRef<CSeq_loc> >(),
                                               *scope, counts_vect),
                      CKmerCountsException);

    // Distance between a sequence and itself is zero
    seqs.resize(2);
    seqs[1] = seqs[0];

    // for regular alphabet
    TKMethods::SetParams(kKmerLen, kAlphabetSize);

    TKMethods::ComputeCounts(seqs, *scope, counts_vect);
    BOOST_REQUIRE_EQUAL(counts_vect.size(), seqs.size());

    TKMethods::ComputeDistMatrix(counts_vect,
                                 TKMethods::eFractionCommonKmersGlobal, dmat);

    BOOST_REQUIRE_EQUAL(dmat.GetRows(), seqs.size());
    BOOST_CHECK_CLOSE(dmat(0, 1), 0.0, 1e-6);

    TKMethods::ComputeDistMatrix(counts_vect,
                                 TKMethods::eFractionCommonKmersLocal, dmat);

    BOOST_REQUIRE_EQUAL(dmat.GetRows(), seqs.size());
    BOOST_CHECK_CLOSE(dmat(0, 1), 0.0, 1e-6);


    // for compressed alphabet SE-B15
    TKMethods::SetParams(kKmerLen, TKMethods::eSE_B15);

    TKMethods::ComputeCounts(seqs, *scope, counts_vect);
    BOOST_REQUIRE_EQUAL(counts_vect.size(), seqs.size());

    TKMethods::ComputeDistMatrix(counts_vect,
                                 TKMethods::eFractionCommonKmersGlobal, dmat);

    BOOST_REQUIRE_EQUAL(dmat.GetRows(), seqs.size());
    BOOST_CHECK_CLOSE(dmat(0, 1), 0.0, 1e-6);

    TKMethods::ComputeDistMatrix(counts_vect,
                                 TKMethods::eFractionCommonKmersLocal, dmat);

    BOOST_REQUIRE_EQUAL(dmat.GetRows(), seqs.size());
    BOOST_CHECK_CLOSE(dmat(0, 1), 0.0, 1e-6);

    // for compressed alphabet SE-V10
    TKMethods::SetParams(kKmerLen, TKMethods::eSE_V10);

    TKMethods::ComputeCounts(seqs, *scope, counts_vect);
    BOOST_REQUIRE_EQUAL(counts_vect.size(), seqs.size());

    TKMethods::ComputeDistMatrix(counts_vect,
                                 TKMethods::eFractionCommonKmersGlobal, dmat);

    BOOST_REQUIRE_EQUAL(dmat.GetRows(), seqs.size());
    BOOST_CHECK_CLOSE(dmat(0, 1), 0.0, 1e-6);

    TKMethods::ComputeDistMatrix(counts_vect,
                                 TKMethods::eFractionCommonKmersLocal, dmat);

    BOOST_REQUIRE_EQUAL(dmat.GetRows(), seqs.size());
    BOOST_CHECK_CLOSE(dmat(0, 1), 0.0, 1e-6);
}

BOOST_AUTO_TEST_SUITE_END()

#endif /* SKIP_DOXYGEN_PROCESSING */
