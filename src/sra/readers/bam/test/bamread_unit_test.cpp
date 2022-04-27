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


static bool s_TestOnce(bool goodAll,
                       CBamDb& bam, const char* ref_name,
                       CBamAlignIterator::ESearchMode search_mode,
                       const SRangeQuery& query,
                       int min_level = -1,
                       int max_level = -1)
{
    int count = 0;
    CBamAlignIterator it;
    int expected = 0;
    if ( min_level <= 0 && (max_level == -1 || max_level >= 0) ) {
        expected += query.count0;
    }
    if ( min_level <= 1 && (max_level == -1 || max_level >= 1) ) {
        expected += query.count1;
    }
    if ( min_level <= 2 && (max_level == -1 || max_level >= 2) ) {
        expected += query.countHi;
    }
    if ( min_level == -1 ) {
        it = CBamAlignIterator(bam, ref_name, query.pos, query.end-query.pos, search_mode);
    }
    else {
        it = CBamAlignIterator(bam, ref_name, query.pos, query.end-query.pos,
                               min_level == -1? CBamIndex::kMinLevel: CBamIndex::EIndexLevel(min_level),
                               max_level == -1? CBamIndex::kMaxLevel: CBamIndex::EIndexLevel(max_level),
                               search_mode);
        
    }
    for ( ; it; ++it ) {
        ++count;
    }
    BOOST_CHECK_EQUAL(count, expected);
    bool good = count == expected;
    goodAll &= good;
    return good;
}


static bool s_TestAllLevels(const char* description,
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
    BOOST_CHECK(s_TestOnce(good, bam, ref_name, search_mode, query));
    if ( !bam.UsesRawIndex() ) {
        return good;
    }
    BOOST_CHECK(s_TestOnce(good, bam, ref_name, search_mode, query, 0, 0));
    BOOST_CHECK(s_TestOnce(good, bam, ref_name, search_mode, query, 1, 1));
    BOOST_CHECK(s_TestOnce(good, bam, ref_name, search_mode, query, 1));
    BOOST_CHECK(s_TestOnce(good, bam, ref_name, search_mode, query, 0, 1));
    BOOST_CHECK(s_TestOnce(good, bam, ref_name, search_mode, query, 0));
    return good;
}


static bool s_TestAllAPI(const char* description,
                         const char* bam_name, const char* ref_name,
                         CBamAlignIterator::ESearchMode search_mode,
                         const SRangeQuery& query)
{
    bool goodA = s_TestAllLevels(description, bam_name, ref_name,
                                 ".bai", CBamDb::eUseAlignAccess,
                                 search_mode, query);
    BOOST_CHECK(goodA);
    bool goodB = s_TestAllLevels(description, bam_name, ref_name,
                                 ".bai", CBamDb::eUseRawIndex,
                                 search_mode, query);
    BOOST_CHECK(goodB);
    bool goodC = s_TestAllLevels(description, bam_name, ref_name,
                                 ".csi", CBamDb::eUseRawIndex,
                                 search_mode, query);
    BOOST_CHECK(goodC);
    return goodA & goodB & goodC;
}


BOOST_AUTO_TEST_CASE(BamQuery0Overlap)
{
    BOOST_CHECK(s_TestAllAPI("lookup of alignments within bin",
                             "traces04/1000genomes3/ftp/data/NA19240/exome_alignment/"
                             "NA19240.mapped.SOLID.bfast.YRI.exome.20111114.bam", "GL000207.1",
                             CBamAlignIterator::eSearchByOverlap,
                             { 50, 150, 2, 0, 0 }));
}


BOOST_AUTO_TEST_CASE(BamQuery0Start)
{
    BOOST_CHECK(s_TestAllAPI("lookup of alignments within bin",
                             "traces04/1000genomes3/ftp/data/NA19240/exome_alignment/"
                             "NA19240.mapped.SOLID.bfast.YRI.exome.20111114.bam", "GL000207.1",
                             CBamAlignIterator::eSearchByStart,
                             { 50, 150, 1, 0, 0 }));
}


BOOST_AUTO_TEST_CASE(BamQuery1Overlap)
{
    BOOST_CHECK(s_TestAllAPI("lookup of alignments crossing bin border",
                             "bam/hs108_sra.fil_sort.chr1.bam", "NC_000001.11",
                             CBamAlignIterator::eSearchByOverlap,
                             { 16398, 32768, 2, 1 }));
}


BOOST_AUTO_TEST_CASE(BamQuery1Start)
{
    BOOST_CHECK(s_TestAllAPI("lookup of alignments crossing bin border",
                             "bam/hs108_sra.fil_sort.chr1.bam", "NC_000001.11",
                             CBamAlignIterator::eSearchByStart,
                             { 16398, 32768, 2, 0 }));
}


BOOST_AUTO_TEST_CASE(BamQuery2Overlap)
{
    BOOST_CHECK(s_TestAllAPI("lookup of long alignments crossing bin border",
                             "bam/hs108_sra.fil_sort.chr1.bam", "NC_000001.11",
                             CBamAlignIterator::eSearchByOverlap,
                             { 243499772, 243500032, 0, 0, 108 }));
}


BOOST_AUTO_TEST_CASE(BamQuery2Start)
{
    BOOST_CHECK(s_TestAllAPI("lookup of long alignments crossing bin border",
                             "bam/hs108_sra.fil_sort.chr1.bam", "NC_000001.11",
                             CBamAlignIterator::eSearchByStart,
                             { 243499772, 243500032, 0, 0, 2 }));
}


BOOST_AUTO_TEST_CASE(BamQuery3Overlap)
{
    BOOST_CHECK(s_TestAllAPI("lookup of alignments crossing bin border",
                             "traces04/1000genomes3/ftp/data/NA10851/alignment/"
                             "NA10851.chrom20.ILLUMINA.bwa.CEU.low_coverage.20111114.bam", "20",
                             CBamAlignIterator::eSearchByOverlap,
                             { 114719, 200000, 14533, 28, 7 }));
}


BOOST_AUTO_TEST_CASE(BamQuery3Start)
{
    BOOST_CHECK(s_TestAllAPI("lookup of alignments crossing bin border",
                             "traces04/1000genomes3/ftp/data/NA10851/alignment/"
                             "NA10851.chrom20.ILLUMINA.bwa.CEU.low_coverage.20111114.bam", "20",
                             CBamAlignIterator::eSearchByStart,
                             { 114719, 200000, 14530, 26, 7 }));
}


BOOST_AUTO_TEST_CASE(BamQuery4Overlap)
{
    BOOST_CHECK(s_TestAllAPI("lookup of alignments crossing bin border",
                             "traces04/1000genomes3/ftp/data/NA10851/alignment/"
                             "NA10851.chrom20.ILLUMINA.bwa.CEU.low_coverage.20111114.bam", "20",
                             CBamAlignIterator::eSearchByOverlap,
                             { 131077, 200000, 11928, 26, 6 }));
}


BOOST_AUTO_TEST_CASE(BamQuery4Start)
{
    BOOST_CHECK(s_TestAllAPI("lookup of alignments crossing bin border",
                             "traces04/1000genomes3/ftp/data/NA10851/alignment/"
                             "NA10851.chrom20.ILLUMINA.bwa.CEU.low_coverage.20111114.bam", "20",
                             CBamAlignIterator::eSearchByStart,
                             { 131077, 200000, 11928, 26, 0 }));
}
