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
* File Description:
*   Unit tests for mock implementation(s) of the BlastSeqSrc that fails
*   randomly
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include "seqsrc_mock.hpp"
#include "test_objmgr.hpp"

USING_NCBI_SCOPE;
using namespace blast;

/// Initializes m_SeqSrc with a BlastSeqSrc that fails randomly
struct RandomlyFailingMockSeqSrcTestFixture {
    CBlastSeqSrc seqsrc;

    RandomlyFailingMockSeqSrcTestFixture() {
        seqsrc.Reset(MockBlastSeqSrcInit(eMBSS_RandomlyFail));
    }

    ~RandomlyFailingMockSeqSrcTestFixture() {
        seqsrc.Reset(NULL);
    }
};

BOOST_FIXTURE_TEST_SUITE(mockseqsrc2, RandomlyFailingMockSeqSrcTestFixture)

BOOST_AUTO_TEST_CASE(TestNumberOfSeqs) {
    for (int i = 0; i < 10; i++) {
        int rv = BlastSeqSrcGetNumSeqs(seqsrc);
        BOOST_REQUIRE(rv == BLAST_SEQSRC_ERROR ||
                       rv == CRandomlyFailMockBlastSeqSrc::kDefaultInt4);
    }
}

BOOST_AUTO_TEST_CASE(TestMaxSeqLen) {
    for (int i = 0; i < 10; i++) {
        int rv = BlastSeqSrcGetMaxSeqLen(seqsrc);
        BOOST_REQUIRE(rv == BLAST_SEQSRC_ERROR ||
                       rv == CRandomlyFailMockBlastSeqSrc::kDefaultInt4);
    }
}

BOOST_AUTO_TEST_CASE(TestAvgSeqLen) {
    for (int i = 0; i < 10; i++) {
        int rv = BlastSeqSrcGetAvgSeqLen(seqsrc);
        BOOST_REQUIRE(rv == BLAST_SEQSRC_ERROR ||
                       rv == CRandomlyFailMockBlastSeqSrc::kDefaultInt4);
    }
}

BOOST_AUTO_TEST_CASE(TestTotLen) {
    for (int i = 0; i < 10; i++) {
        Int8 rv = BlastSeqSrcGetTotLen(seqsrc);
        BOOST_REQUIRE(rv == BLAST_SEQSRC_ERROR ||
                       rv == CRandomlyFailMockBlastSeqSrc::kDefaultInt8);
    }
}

BOOST_AUTO_TEST_CASE(TestGetName) {
    for (int i = 0; i < 10; i++) {
        const char* str = BlastSeqSrcGetName(seqsrc);
        if (str == NULL) {
            BOOST_REQUIRE(true);
        } else {
            string expected(CRandomlyFailMockBlastSeqSrc::kDefaultString);
            string actual(str);
            BOOST_REQUIRE_EQUAL(expected, actual);
        }
    }
}

BOOST_AUTO_TEST_CASE(TestIsProtein) {
    Boolean rv = BlastSeqSrcGetIsProt(seqsrc);
    BOOST_REQUIRE(rv == TRUE || rv == FALSE); // duh!

    // repeated calls to this function should return the same value
    Boolean rv2 = BlastSeqSrcGetIsProt(seqsrc);
    BOOST_REQUIRE(rv == rv2);
}

BOOST_AUTO_TEST_CASE(TestGetSequenceValidOid)
{
    BlastSeqSrcGetSeqArg seq_arg;
    memset((void*) &seq_arg, 0, sizeof(seq_arg));
    seq_arg.oid = CRandomlyFailMockBlastSeqSrc::kDefaultOid;

    bool successful_return = false;
    for (int i = 0; i < 100; i++) {
        int rv = BlastSeqSrcGetSequence(seqsrc, &seq_arg);
        if (rv != BLAST_SEQSRC_SUCCESS) {
            continue;
        } else {
            successful_return = true;
            break;
        }
    }

    if (successful_return) {
        BOOST_REQUIRE(seq_arg.seq);
        BOOST_REQUIRE(CRandomlyFailMockBlastSeqSrc::kDefaultInt4 ==
                             seq_arg.seq->length);
    }/* else {
        // you'd had to be unlucky to get this
        BOOST_REQUIRE(false);
    } */

    BlastSeqSrcReleaseSequence(seqsrc, &seq_arg);
}

BOOST_AUTO_TEST_CASE(TestGetSequenceAnyOid)
{
    BlastSeqSrcGetSeqArg seq_arg;
    memset((void*) &seq_arg, 0, sizeof(seq_arg));
    seq_arg.oid = 66;   // request some random oid

    for (int i = 0; i < 10; i++) {
        int rv = BlastSeqSrcGetSequence(seqsrc, &seq_arg);
        BOOST_REQUIRE(rv == BLAST_SEQSRC_ERROR);
    }

    BlastSeqSrcReleaseSequence(seqsrc, &seq_arg);
}

BOOST_AUTO_TEST_CASE(TestGetSequenceEOF)
{
    BlastSeqSrcGetSeqArg seq_arg;
    memset((void*) &seq_arg, 0, sizeof(seq_arg));
    seq_arg.oid = BLAST_SEQSRC_EOF;

    for (int i = 0; i < 10; i++) {
        int rv = BlastSeqSrcGetSequence(seqsrc, &seq_arg);
        if (rv != BLAST_SEQSRC_ERROR) {
            BOOST_REQUIRE_EQUAL(BLAST_SEQSRC_EOF, rv);
            break;
        }
    }

    BlastSeqSrcReleaseSequence(seqsrc, &seq_arg);
}

BOOST_AUTO_TEST_CASE(TestGetSeqLenValidOid)
{
    Int4 oid = CRandomlyFailMockBlastSeqSrc::kDefaultOid;
    int rv = BlastSeqSrcGetSeqLen(seqsrc, (void*) &oid);
    BOOST_REQUIRE(CRandomlyFailMockBlastSeqSrc::kDefaultInt4 == rv);
}

BOOST_AUTO_TEST_CASE(TestGetSeqLenAnyOid)
{
    Int4 oid = 66;  // request some random oid
    int rv = BlastSeqSrcGetSeqLen(seqsrc, (void*) &oid);
    BOOST_REQUIRE(rv == BLAST_SEQSRC_ERROR);
}

// Note that initialization of a IMockBlastSeqSrc which fails randomly
// always succeeds
BOOST_AUTO_TEST_CASE(TestGetInitError) {
    const char* rv = BlastSeqSrcGetInitError(seqsrc);
    BOOST_REQUIRE(rv == NULL);
}

BOOST_AUTO_TEST_CASE(TestIterationUseCase)
{
    CBlastSeqSrcIterator itr(BlastSeqSrcIteratorNew());
    BlastSeqSrcGetSeqArg seq_arg;

    memset((void*) &seq_arg, 0, sizeof(seq_arg));

    while ( (seq_arg.oid = BlastSeqSrcIteratorNext(seqsrc, itr)) !=
            BLAST_SEQSRC_EOF) {
    }
    BOOST_REQUIRE(true);
}

BOOST_AUTO_TEST_SUITE_END()
