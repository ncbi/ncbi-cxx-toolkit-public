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

/** @file psi_iteration-cppunit.cpp
 * Unit tests for PSI-BLAST iteration state 
 */
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/api/psiblast_iteration.hpp>

using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

BOOST_AUTO_TEST_SUITE(psiblast_iteration);

BOOST_AUTO_TEST_CASE(TestAsLoopCounter) {
        const unsigned int kNumIterations = 10;
        unsigned int gi = 0;

        CPsiBlastIterationState itr(kNumIterations);
        while (itr) {
            BOOST_REQUIRE_EQUAL(true, itr.HasMoreIterations());
            BOOST_REQUIRE_EQUAL(false, itr.HasConverged());
            BOOST_REQUIRE_EQUAL(++gi, itr.GetIterationNumber());

            CPsiBlastIterationState::TSeqIds ids;
            ids.insert(CSeq_id_Handle::GetHandle(gi));
            itr.Advance(ids);
        }

        BOOST_REQUIRE_EQUAL(gi, kNumIterations);
        BOOST_REQUIRE_EQUAL(kNumIterations+1, itr.GetIterationNumber());
        BOOST_REQUIRE_EQUAL(false, itr.HasMoreIterations());
        BOOST_REQUIRE_EQUAL(false, itr.HasConverged());
}

BOOST_AUTO_TEST_CASE(TestConvergence) {
        CPsiBlastIterationState itr(0);

        BOOST_REQUIRE_EQUAL(false, itr.HasConverged());
        BOOST_REQUIRE_EQUAL(1U, itr.GetIterationNumber());
        CPsiBlastIterationState::TSeqIds ids, ids_plus_1;
        ids.insert(CSeq_id_Handle::GetHandle(555));
        ids_plus_1.insert(CSeq_id_Handle::GetHandle(556));

        itr.Advance(ids);
        BOOST_REQUIRE_EQUAL(false, itr.HasConverged());
        BOOST_REQUIRE_EQUAL(true, itr.HasMoreIterations());
        BOOST_REQUIRE_EQUAL(2U, itr.GetIterationNumber());

        itr.Advance(ids_plus_1);
        BOOST_REQUIRE_EQUAL(false, itr.HasConverged());
        BOOST_REQUIRE_EQUAL(true, itr.HasMoreIterations());
        BOOST_REQUIRE_EQUAL(3U, itr.GetIterationNumber());

        itr.Advance(ids_plus_1);
        BOOST_REQUIRE_EQUAL(true, itr.HasConverged());
        BOOST_REQUIRE_EQUAL(true, itr.HasMoreIterations());
        BOOST_REQUIRE_EQUAL(4U, itr.GetIterationNumber());
}

    // Note that for ids)itr2, there are less IDs, but those are present in the
    // previous iteration. This also qualifies as convergence
BOOST_AUTO_TEST_CASE(TestConvergence2) {
        CPsiBlastIterationState itr(0);

        BOOST_REQUIRE_EQUAL(false, itr.HasConverged());
        BOOST_REQUIRE_EQUAL(1U, itr.GetIterationNumber());
        CPsiBlastIterationState::TSeqIds ids_itr1, ids_itr2;
        ids_itr1.insert(CSeq_id_Handle::GetHandle(555));
        ids_itr1.insert(CSeq_id_Handle::GetHandle(556));
        ids_itr2.insert(CSeq_id_Handle::GetHandle(555));

        itr.Advance(ids_itr1);
        BOOST_REQUIRE_EQUAL(false, itr.HasConverged());
        BOOST_REQUIRE_EQUAL(true, itr.HasMoreIterations());
        BOOST_REQUIRE_EQUAL(2U, itr.GetIterationNumber());

        itr.Advance(ids_itr2);
        BOOST_REQUIRE_EQUAL(true, itr.HasConverged());
        BOOST_REQUIRE_EQUAL(true, itr.HasMoreIterations());
        BOOST_REQUIRE_EQUAL(3U, itr.GetIterationNumber());
}

BOOST_AUTO_TEST_CASE(TestModifyingConvergedIterationState) {
        CPsiBlastIterationState itr(0);
        BOOST_REQUIRE_EQUAL(false, itr.HasConverged());

        CPsiBlastIterationState::TSeqIds ids;
        ids.insert(CSeq_id_Handle::GetHandle(555));
        ids.insert(CSeq_id_Handle::GetHandle(555));
        itr.Advance(ids);
        BOOST_REQUIRE_EQUAL(false, itr.HasConverged());

        itr.Advance(ids);
        BOOST_REQUIRE_EQUAL(true, itr.HasConverged());

        BOOST_REQUIRE_THROW(itr.Advance(ids), CBlastException);
}

    // Ensure that at every iteration the list of seqids passed in is different
void RunNIterationsWithoutConverging(CPsiBlastIterationState& itr,
                                         unsigned int n_iterations) {

        for (unsigned int i = 0; i < n_iterations; i++) {
            CPsiBlastIterationState::TSeqIds ids;
            ids.insert(CSeq_id_Handle::GetHandle(i+1));
            itr.Advance(ids);
        }
}

    // Functionally similar to TestAsLoopCounter, but it forces an exception
    // to be thrown 
BOOST_AUTO_TEST_CASE(TestModifyingExhaustedIterationState) {
        const unsigned int kNumIterations = 10;
        CPsiBlastIterationState itr(kNumIterations);

        RunNIterationsWithoutConverging(itr, kNumIterations);

        BOOST_REQUIRE_EQUAL(kNumIterations+1, itr.GetIterationNumber());
        BOOST_REQUIRE_EQUAL(false, itr.HasMoreIterations());
        BOOST_REQUIRE_EQUAL(false, itr.HasConverged());

        // should throw exception
        BOOST_REQUIRE_THROW(RunNIterationsWithoutConverging(itr, kNumIterations),
           CBlastException);
}

BOOST_AUTO_TEST_SUITE_END()

