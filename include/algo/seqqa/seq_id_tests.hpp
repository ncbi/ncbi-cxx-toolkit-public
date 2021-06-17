#ifndef ALGO_SEQQA___SEQ_ID_TESTS__HPP
#define ALGO_SEQQA___SEQ_ID_TESTS__HPP

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
 * Authors:  Josh Cherry
 *
 * File Description:
 *
 */

#include <algo/seqqa/seqtest.hpp>


BEGIN_NCBI_SCOPE

class NCBI_XALGOSEQQA_EXPORT CTestSeqId : public CSeqTest
{
public:
    bool CanTest(const CSerialObject& obj, const CSeqTestContext* ctx) const;
};

#define DECLARE_SEQ_ID_TEST(name) \
class NCBI_XALGOSEQQA_EXPORT CTestSeqId_##name : public CTestSeqId \
{ \
public: \
    CRef<objects::CSeq_test_result_set> \
        RunTest(const CSerialObject& obj, \
                const CSeqTestContext* ctx); \
}

DECLARE_SEQ_ID_TEST(Biomol);


END_NCBI_SCOPE

#endif  // ALGO_SEQQA___SEQ_ID_TESTS__HPP
