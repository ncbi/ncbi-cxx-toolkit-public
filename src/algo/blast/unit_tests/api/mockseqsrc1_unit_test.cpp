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
*   Unit tests for mock implementation(s) of the BlastSeqSrc which fails always
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

/// Initializes m_SeqSrc with a BlastSeqSrc that always fails
struct FailingMockSeqSrcTestFixture {
    BlastSeqSrc* m_SeqSrc;

    FailingMockSeqSrcTestFixture() {
        m_SeqSrc = MockBlastSeqSrcInit(eMBSS_AlwaysFail);
    }

    ~FailingMockSeqSrcTestFixture() {
        m_SeqSrc = BlastSeqSrcFree(m_SeqSrc);
    }
};

BOOST_FIXTURE_TEST_SUITE(mockseqsrc1, FailingMockSeqSrcTestFixture)

BOOST_AUTO_TEST_CASE(TestCreation) {
    BOOST_REQUIRE(m_SeqSrc != NULL);
}

BOOST_AUTO_TEST_CASE(TestNumberOfSeqs) {
    int rv = BlastSeqSrcGetNumSeqs(m_SeqSrc);
    BOOST_REQUIRE(rv == BLAST_SEQSRC_ERROR);
}

BOOST_AUTO_TEST_CASE(TestMaxSeqLen) {
    int rv = BlastSeqSrcGetMaxSeqLen(m_SeqSrc);
    BOOST_REQUIRE(rv == BLAST_SEQSRC_ERROR);
}

BOOST_AUTO_TEST_CASE(TestAvgSeqLen) {
    int rv = BlastSeqSrcGetAvgSeqLen(m_SeqSrc);
    BOOST_REQUIRE(rv == BLAST_SEQSRC_ERROR);
}

BOOST_AUTO_TEST_CASE(TestTotLen) {
    Int8 rv = BlastSeqSrcGetTotLen(m_SeqSrc);
    BOOST_REQUIRE(rv == BLAST_SEQSRC_ERROR);
}

BOOST_AUTO_TEST_CASE(TestGetName) {
    const char* str = BlastSeqSrcGetName(m_SeqSrc);
    BOOST_REQUIRE(str == NULL);
}

BOOST_AUTO_TEST_CASE(TestIsProtein) {
    Boolean rv = BlastSeqSrcGetIsProt(m_SeqSrc);
    BOOST_REQUIRE(rv == FALSE);
    // repeated calls to this function should return the same value
    rv = BlastSeqSrcGetIsProt(m_SeqSrc);
    BOOST_REQUIRE(rv == FALSE);
}

BOOST_AUTO_TEST_CASE(TestGetSequence) {
    BlastSeqSrcGetSeqArg* empty = (BlastSeqSrcGetSeqArg*)0xdeadbeef;
    int rv = BlastSeqSrcGetSequence(m_SeqSrc, empty);
    BOOST_REQUIRE(rv == BLAST_SEQSRC_ERROR);
}

BOOST_AUTO_TEST_CASE(TestGetSeqLen) {
    void* dummy = (void*) &m_SeqSrc;
    int rv = BlastSeqSrcGetSeqLen(m_SeqSrc, dummy);
    BOOST_REQUIRE(rv == BLAST_SEQSRC_ERROR);
}

BOOST_AUTO_TEST_CASE(TestGetInitError) {
    const char* rv = BlastSeqSrcGetInitError(m_SeqSrc);
    BOOST_REQUIRE(rv == NULL);
}

/// This shouldn't really need to be here
BOOST_AUTO_TEST_CASE(TestIteratorCreation) {
    BlastSeqSrcIterator* itr = BlastSeqSrcIteratorNew();
    BOOST_REQUIRE(itr != NULL);
    itr = BlastSeqSrcIteratorFree(itr);
    BOOST_REQUIRE(itr == NULL);
}

BOOST_AUTO_TEST_CASE(TestIterationUseCase) {
    CBlastSeqSrcIterator itr(BlastSeqSrcIteratorNew());
    BlastSeqSrcGetSeqArg seq_arg;

    memset((void*) &seq_arg, 0, sizeof(seq_arg));

    int i = 0;
    while ( (seq_arg.oid = BlastSeqSrcIteratorNext(m_SeqSrc, itr)) !=
            BLAST_SEQSRC_EOF) {
        i++;
    }
    BOOST_REQUIRE(i == 0);
}

BOOST_AUTO_TEST_CASE(TestCreationFailure)
{
    CBlastSeqSrc seqsrc(MockBlastSeqSrcInit(eMBSS_Invalid));
    BOOST_REQUIRE(seqsrc.Get() != NULL);
    char* error_str = BlastSeqSrcGetInitError(seqsrc);
    BOOST_REQUIRE(error_str != NULL);
    sfree(error_str);
}

BOOST_AUTO_TEST_SUITE_END()
