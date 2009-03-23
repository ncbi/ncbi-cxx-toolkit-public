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

/** @file subj_ranges_unit_test.cpp
 * Unit tests for the functionality to keep track of HSP ranges to fetch during
 * the traceback stage.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/core/blast_util.h> // for FENCE_SENTRY
#include <algo/blast/api/subj_ranges_set.hpp>

#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);

BOOST_AUTO_TEST_SUITE(subj_ranges)

BOOST_AUTO_TEST_CASE(TestRangeMergingSingleQuery)
{
    const int kMinGap = 100;
    CRef<CSubjectRanges> range(new CSubjectRanges);
    const int kQueryId = 7;
    range->AddRange(kQueryId, 10, 50, kMinGap);
    range->AddRange(kQueryId, 60, 100, kMinGap);
    range->AddRange(kQueryId, 250, 400, kMinGap);

    BOOST_REQUIRE_EQUAL(false, range->IsUsedByMultipleQueries());

    const CSubjectRanges::TRangeList& ranges = range->GetRanges();
    BOOST_REQUIRE_EQUAL((size_t)2, ranges.size());
    CSubjectRanges::TRangeList::const_iterator itr = ranges.begin();
    {
        const pair<int, int>& r = *itr;
        BOOST_REQUIRE_EQUAL(10, r.first);
        BOOST_REQUIRE_EQUAL(100, r.second);
    }
    {
        const pair<int, int>& r = *++itr;
        BOOST_REQUIRE_EQUAL(250, r.first);
        BOOST_REQUIRE_EQUAL(400, r.second);
    }
}

BOOST_AUTO_TEST_CASE(TestRangeMergingMultipleQueries)
{
    const int kMinGap = 100;
    CRef<CSubjectRanges> range(new CSubjectRanges);
    vector<int> query_ids;
    query_ids.push_back(7);
    query_ids.push_back(77);

    range->AddRange(query_ids.front(), 10, 50, kMinGap);
    range->AddRange(query_ids.front(), 60, 100, kMinGap);
    range->AddRange(query_ids.front(), 250, 400, kMinGap);
    range->AddRange(query_ids.back(), 200, 500, kMinGap);
    range->AddRange(query_ids.back(), 1000, 3000, kMinGap);
    range->AddRange(query_ids.back(), 3500, 4000, kMinGap);

    BOOST_REQUIRE_EQUAL(true, range->IsUsedByMultipleQueries());

    const CSubjectRanges::TRangeList& ranges = range->GetRanges();
    BOOST_REQUIRE_EQUAL((size_t)3, ranges.size());
    CSubjectRanges::TRangeList::const_iterator itr = ranges.begin();
    {
        const pair<int, int>& r = *itr;
        BOOST_REQUIRE_EQUAL(10, r.first);
        BOOST_REQUIRE_EQUAL(500, r.second);
    }
    {
        const pair<int, int>& r = *++itr;
        BOOST_REQUIRE_EQUAL(1000, r.first);
        BOOST_REQUIRE_EQUAL(3000, r.second);
    }
    {
        const pair<int, int>& r = *++itr;
        BOOST_REQUIRE_EQUAL(3500, r.first);
        BOOST_REQUIRE_EQUAL(4000, r.second);
    }
}

BOOST_AUTO_TEST_CASE(TestCSubjectRangesSetApplyRanges)
{
    CSeqDB db("9606_genomic", CSeqDB::eNucleotide);
    CSubjectRangesSet srs;
    const int kQueryId(0);
    const int kSubjectId(1);
    const TSeqPos kSeqLength(135374737);
    TSeqRange range1(500, 2000), range2(10000, 12000), range3(130000, 400000);
    srs.AddRange(kQueryId, kSubjectId, range1.GetFrom(), range1.GetTo());
    srs.AddRange(kQueryId+1, kSubjectId, range2.GetFrom(), range2.GetTo());
    srs.AddRange(kQueryId, kSubjectId, range3.GetFrom(), range3.GetTo());
    srs.ApplyRanges(db);
    BOOST_REQUIRE(true);    // everything should be fine, no-op

    const char* buf;
    TSeqPos len = db.GetAmbigSeq(kSubjectId, &buf, kSeqDBNuclBlastNA8);
    BOOST_REQUIRE_EQUAL(kSeqLength, len);
    // the ranges must have expanded to the beginning and end of the sequence
    const char kSentinel = (char)FENCE_SENTRY;
    BOOST_REQUIRE(buf[0] != kSentinel);
    BOOST_REQUIRE(buf[len-1] != kSentinel);
    // verify that the 'fence' sentinels are placed in the right locations
    const int kPadSize = CSubjectRangesSet::kHspExpandSize;
    BOOST_REQUIRE_EQUAL(kSentinel, buf[range1.GetToOpen()+kPadSize]);
    BOOST_REQUIRE_EQUAL(kSentinel, buf[range2.GetFrom()-kPadSize]);
    BOOST_REQUIRE_EQUAL(kSentinel, buf[range2.GetToOpen()+kPadSize]);
    BOOST_REQUIRE_EQUAL(kSentinel, buf[range3.GetFrom()-kPadSize]);
    db.RetSequence(&buf);
}

BOOST_AUTO_TEST_CASE(TestCSubjectRangesSetRemoveSubject)
{
    CSeqDB db("9606_genomic", CSeqDB::eNucleotide);
    CSubjectRangesSet srs;
    const int kQueryId(0);
    const int kSubjectId(0);
    srs.AddRange(kQueryId, kSubjectId, 500, 2000);
    srs.RemoveSubject(kSubjectId);
    srs.ApplyRanges(db);
    BOOST_REQUIRE(true);    // everything should be fine, no-op
}

BOOST_AUTO_TEST_CASE(TestCSubjectRangesSetRemoveSubject_NoAdditions)
{
    CSeqDB db("9606_genomic", CSeqDB::eNucleotide);
    CSubjectRangesSet srs;
    srs.RemoveSubject(0);
    srs.RemoveSubject(7);
    srs.RemoveSubject(-1);
    srs.ApplyRanges(db);
    BOOST_REQUIRE(true);    // everything should be fine, no-op
}

BOOST_AUTO_TEST_SUITE_END()

#endif /* SKIP_DOXYGEN_PROCESSING */
