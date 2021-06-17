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
 * Author: Nathan Bouk
 *
 * File Description:
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/genomecoll/genome_collection__.hpp>
#include <objects/genomecoll/genomic_collections_cli.hpp>
#include <misc/hgvs/hgvs_reader.hpp>


#if BOOST_VERSION >= 105900
#  include <boost/test/tools/output_test_stream.hpp>
#else
#  include <boost/test/output_test_stream.hpp>
#endif
using boost::test_tools::output_test_stream;

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BOOST_AUTO_TEST_SUITE(TestSuiteHgvsReader)


typedef CRef<CSeq_annot> TSeqAnnotRef;
typedef vector<TSeqAnnotRef> TSeqAnnotList;

void
CheckSeqAnnotList(const TSeqAnnotList& annots)
{
    ITERATE (TSeqAnnotList, it, annots) {
        TSeqAnnotRef sa(*it);
        if (sa.NotNull()) {
            cerr << MSerial_AsnText << *sa;
        }
    }
}

CConstRef<CGC_Assembly>
GetAssembly(const string& assmacc)
{
    static CGenomicCollectionsService gcservice;
    CConstRef<CGC_Assembly> assembly
        (gcservice.GetAssembly(assmacc, CGenomicCollectionsService::SAssemblyMode::kScaffolds()));
    return assembly;
}

void
TestCase(CHgvsReader& reader, const string& input)
{
    CMemoryLineReader line_reader(input.c_str(), input.length());

    TSeqAnnotList annots;
    reader.ReadSeqAnnots(annots, line_reader);
    CheckSeqAnnotList(annots);

    // Check that Map results meet expectations
    BOOST_CHECK_EQUAL(1, 1);
}


BOOST_AUTO_TEST_CASE(TestCaseHgvs1)
{
    CConstRef<CGC_Assembly> assembly = GetAssembly("GCF_000001405.13");
    CHgvsReader reader(*assembly);
    const string lines(
        "NC_000023.10:g.107930740_107930741insT\n"
/*
        "NC_000001.10:g.197031020C>T\n"
        "chr1:g.169519049T=\n"
        "NC_000006.11:g.32627914delA\n"
        "NC_000006.11:g.32627914insA\n"
        "NC_000008.10:g.24810374_24810376delCTC\n"
        "NC_000011.9:g.61723378_61723379delGCinsAA\n"
        "NC_000017.10:g.66519861dupT\n"
*/
    );
    TestCase(reader, lines);

    // Check that Map results meet expectations
    BOOST_CHECK_EQUAL(1, 1);
}

BOOST_AUTO_TEST_CASE(Chr1_RS_GCF)
{
    CConstRef<CGC_Assembly> assembly = GetAssembly("GCF_000001405.13");
    CHgvsReader reader(*assembly);
    const string line = "NC_000001.10:g.197031021C>T";
    TestCase(reader, line);

    // Check that Map results meet expectations
    BOOST_CHECK_EQUAL(1, 1);
}

BOOST_AUTO_TEST_CASE(Chr1_name_GCF)
{
    CConstRef<CGC_Assembly> assembly = GetAssembly("GCF_000001405.13");
    CHgvsReader reader(*assembly);
    const string line = "1:g.197031021C>T";
    TestCase(reader, line);

    // Check that Map results meet expectations
    BOOST_CHECK_EQUAL(1, 1);
}

BOOST_AUTO_TEST_CASE(Chr1_UCSC_GCF)
{
    CConstRef<CGC_Assembly> assembly = GetAssembly("GCF_000001405.13");
    CHgvsReader reader(*assembly);
    const string line = "chr1:g.197031021C>T";
    TestCase(reader, line);

    // Check that Map results meet expectations
    BOOST_CHECK_EQUAL(1, 1);
}

BOOST_AUTO_TEST_CASE(Chr1_GB_GCF)
{
    CConstRef<CGC_Assembly> assembly = GetAssembly("GCF_000001405.13");
    CHgvsReader reader(*assembly);
    const string line = "CM000663.1:g.197031021C>T";
    TestCase(reader, line);

    // Check that Map results meet expectations
    BOOST_CHECK_EQUAL(1, 1);
}

BOOST_AUTO_TEST_CASE(Chr8_name_GCF)
{
    CConstRef<CGC_Assembly> assembly = GetAssembly("GCF_000001405.13");
    CHgvsReader reader(*assembly);
    const string line = "8:g.19813529A>G";
    TestCase(reader, line);

    // Check that Map results meet expectations
    BOOST_CHECK_EQUAL(1, 1);
}

BOOST_AUTO_TEST_CASE(Chr17_RS_GCF)
{
    CConstRef<CGC_Assembly> assembly = GetAssembly("GCF_000001405.13");
    CHgvsReader reader(*assembly);
    const string line = "NC_000017.10:g.66519861dupT";
    TestCase(reader, line);

    // Check that Map results meet expectations
    BOOST_CHECK_EQUAL(1, 1);
}

BOOST_AUTO_TEST_CASE(Chr17_name_GCF)
{
    CConstRef<CGC_Assembly> assembly = GetAssembly("GCF_000001405.13");
    CHgvsReader reader(*assembly);
    const string line = "17:g.66519861dupT";
    TestCase(reader, line);

    // Check that Map results meet expectations
    BOOST_CHECK_EQUAL(1, 1);
}

BOOST_AUTO_TEST_CASE(ChrX_name_GCF)
{
    CConstRef<CGC_Assembly> assembly = GetAssembly("GCF_000001405.13");
    CHgvsReader reader(*assembly);
    const string line = "X:g.6619861dupT";
    TestCase(reader, line);

    // Check that Map results meet expectations
    BOOST_CHECK_EQUAL(1, 1);
}

BOOST_AUTO_TEST_SUITE_END();

