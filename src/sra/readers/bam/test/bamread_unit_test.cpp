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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Unit tests for SRA data loader
*
* ===========================================================================
*/
#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>
#include <sra/readers/bam/bamread.hpp>
#include <corelib/ncbi_system.hpp>

#include <corelib/test_boost.hpp>
#include <common/test_data_path.h>
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);

struct SRangeQuery {
    TSeqPos pos;
    TSeqPos end;
    int count0;
    int count1;
    int countHi;
};

static bool s_Test(const char* description,
                   const char* bam_name, const char* ref_name,
                   const char* index_suffix,
                   CBamDb::EUseAPI api,
                   CBamAlignIterator::ESearchMode search_mode,
                   const SRangeQuery& query)
{
    LOG_POST("Testing "<<description<<" in "<<bam_name<<" with "<<index_suffix);
    bool good = true;
    string bam_path = CFile::MakePath(NCBI_GetTestDataPath(), bam_name);
    CBamMgr mgr;
    CBamDb bam(mgr, bam_path, bam_path+index_suffix, api);
    TSeqPos pos = query.pos;
    TSeqPos window = query.end-pos;
    {{
        int count = 0;
        for ( CBamAlignIterator it(bam, ref_name, pos, window, search_mode); it; ++it ) {
            ++count;
        }
        int expectedAll = query.count0 + query.count1 + query.countHi;
        BOOST_CHECK_EQUAL(count, expectedAll);
        good &= (count == expectedAll);
    }}
    if ( !bam.UsesRawIndex() ) {
        return good;
    }
    {{
        int count = 0;
        for ( CBamAlignIterator it(bam, ref_name, pos, window,
                                   CBamIndex::kLevel0, CBamIndex::kLevel0, search_mode);
              it; ++it ) {
            ++count;
        }
        int expected0 = query.count0;
        BOOST_CHECK_EQUAL(count, expected0);
        good &= (count == expected0);
    }}
    {{
        int count = 0;
        for ( CBamAlignIterator it(bam, ref_name, pos, window,
                                   CBamIndex::kLevel1, CBamIndex::kLevel1, search_mode);
              it; ++it ) {
            ++count;
        }
        int expected1 = query.count1;
        BOOST_CHECK_EQUAL(count, expected1);
        good &= (count == expected1);
    }}
    {{
        int count = 0;
        for ( CBamAlignIterator it(bam, ref_name, pos, window,
                                   CBamIndex::kLevel1, CBamIndex::kMaxLevel, search_mode);
              it; ++it ) {
            ++count;
        }
        int expected1Up = query.count1 + query.countHi;
        BOOST_CHECK_EQUAL(count, expected1Up);
        good &= (count == expected1Up);
    }}
    {{
        int count = 0;
        for ( CBamAlignIterator it(bam, ref_name, pos, window,
                                   CBamIndex::kLevel0, CBamIndex::kLevel1, search_mode);
              it; ++it ) {
            ++count;
        }
        int expected01 = query.count0 + query.count1;
        BOOST_CHECK_EQUAL(count, expected01);
        good &= (count == expected01);
    }}
    {{
        int count = 0;
        for ( CBamAlignIterator it(bam, ref_name, pos, window,
                                   CBamIndex::kLevel0, CBamIndex::kMaxLevel, search_mode);
              it; ++it ) {
            ++count;
        }
        int expected0Up = query.count0 + query.count1 + query.countHi;
        BOOST_CHECK_EQUAL(count, expected0Up);
        good &= (count == expected0Up);
    }}
    return good;
}

BOOST_AUTO_TEST_CASE(BamQuery1AO)
{
    BOOST_CHECK(s_Test("lookup of alignments crossing bin border",
                       "bam/hs108_sra.fil_sort.chr1.bam", "NC_000001.11",
                       ".bai", CBamDb::eUseAlignAccess,
                       CBamAlignIterator::eSearchByOverlap,
                       { 16398, 32768, 2, 1 }));
}


BOOST_AUTO_TEST_CASE(BamQuery1BO)
{
    BOOST_CHECK(s_Test("lookup of alignments crossing bin border",
                       "bam/hs108_sra.fil_sort.chr1.bam", "NC_000001.11",
                       ".bai", CBamDb::eUseRawIndex,
                       CBamAlignIterator::eSearchByOverlap,
                       { 16398, 32768, 2, 1 }));
}


BOOST_AUTO_TEST_CASE(BamQuery1CO)
{
    BOOST_CHECK(s_Test("lookup of alignments crossing bin border",
                       "bam/hs108_sra.fil_sort.chr1.bam", "NC_000001.11",
                       ".csi", CBamDb::eUseRawIndex,
                       CBamAlignIterator::eSearchByOverlap,
                       { 16398, 32768, 2, 1 }));
}


BOOST_AUTO_TEST_CASE(BamQuery1AS)
{
    BOOST_CHECK(s_Test("lookup of alignments crossing bin border",
                       "bam/hs108_sra.fil_sort.chr1.bam", "NC_000001.11",
                       ".bai", CBamDb::eUseAlignAccess,
                       CBamAlignIterator::eSearchByStart,
                       { 16398, 32768, 2, 0 }));
}


BOOST_AUTO_TEST_CASE(BamQuery1BS)
{
    BOOST_CHECK(s_Test("lookup of alignments crossing bin border",
                       "bam/hs108_sra.fil_sort.chr1.bam", "NC_000001.11",
                       ".bai", CBamDb::eUseRawIndex,
                       CBamAlignIterator::eSearchByStart,
                       { 16398, 32768, 2, 0 }));
}


BOOST_AUTO_TEST_CASE(BamQuery1CS)
{
    BOOST_CHECK(s_Test("lookup of alignments crossing bin border",
                       "bam/hs108_sra.fil_sort.chr1.bam", "NC_000001.11",
                       ".csi", CBamDb::eUseRawIndex,
                       CBamAlignIterator::eSearchByStart,
                       { 16398, 32768, 2, 0 }));
}
