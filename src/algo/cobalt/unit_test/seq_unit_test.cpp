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
*   Unit tests for CSequence class
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/seq_vector.hpp>

#include <objects/seqalign/Dense_seg.hpp>

#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <objtools/readers/fasta.hpp>

#include <algo/cobalt/seq.hpp>

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
#undef NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(cobalt);
USING_SCOPE(objects);

BOOST_AUTO_TEST_SUITE(sequence)

// Verify that the alignment represented by vector<CSequence> is the same as
// as the one represented by Seq_align
BOOST_AUTO_TEST_CASE(TestCreateMsa)
{
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*objmgr));
    scope->AddDefaults();

    CRef<CSeq_align> align;

    int status = ReadMsa("data/msa.fa", align, scope);
    BOOST_REQUIRE_EQUAL(status, 0);

    // Test
    // create msa a residues and gaps
    vector<CSequence> msa;
    CSequence::CreateMsa(*align, *scope, msa);

    const CDense_seg& denseg = align->GetSegs().GetDenseg();
    const CDense_seg::TLens lens = denseg.GetLens();
    
    TSeqPos align_len = 0;
    ITERATE (CDense_seg::TLens, it, lens) {
        align_len += *it;
    }

    size_t num_seqs = denseg.GetDim();

    // verify that msa has correct number of sequences
    BOOST_REQUIRE_EQUAL(msa.size(), num_seqs);

    // verify that msa has correct number of columns
    ITERATE(vector<CSequence>, it, msa) {
        BOOST_REQUIRE_EQUAL(it->GetLength(), (int)align_len);

        // verufy dimensions of frequency matrix
        BOOST_REQUIRE_EQUAL(it->GetFreqs().GetRows(), align_len);
        BOOST_REQUIRE_EQUAL((int)it->GetFreqs().GetCols(), kAlphabetSize);
    }

    // verify that each msa row has correct residues
    // for each sequence in denseg
    for (int i=0;i < denseg.GetDim();i++) {

        // get sequence from scope
        CBioseq_Handle bhandle = scope->GetBioseqHandle(denseg.GetSeq_id(i));
        CSeqVector seq_vector(bhandle);
        
        TSeqPos k=0; // position in msa sequence
        // for each residue in sequence
        ITERATE (CSeqVector, it, seq_vector) {
            // skip gaps
            while (k < align_len && msa[i].GetLetter(k) == CSequence::kGapChar) {
                k++;
            }
            // varify that residue in msa is correct
            BOOST_REQUIRE_EQUAL(msa[i].GetLetter(k), *it);
            k++;
        }
        // after the last residue only gaps can appear in the msa row
        while (k < align_len) {
            BOOST_REQUIRE(msa[i].GetLetter(k) == CSequence::kGapChar);
            k++;
        }
    }
}



BOOST_AUTO_TEST_SUITE_END()

#endif /* SKIP_DOXYGEN_PROCESSING */
