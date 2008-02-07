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
 * Author:  Vahram Avagyan
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>

#include <objtools/readers/gene_info/gene_info.hpp>
#include <objtools/readers/gene_info/gene_info_reader.hpp>

#include <map>

//==========================================================================//
// Unit-testing includes

// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>
#ifndef BOOST_PARAM_TEST_CASE
#  include <boost/test/parameterized_test.hpp>
#endif
#include <boost/current_function.hpp>
#ifndef BOOST_AUTO_TEST_CASE
#  define BOOST_AUTO_TEST_CASE BOOST_AUTO_UNIT_TEST
#endif

#include <common/test_assert.h>  /* This header must go last */

//==========================================================================//

#ifndef WORDS_BIGENDIAN

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
using boost::unit_test::test_suite;


// Use macros rather than inline functions to get accurate line number reports

#define CHECK_NO_THROW(statement)                                       \
    try {                                                               \
        statement;                                                      \
        BOOST_CHECK_MESSAGE(true, "no exceptions were thrown by "#statement); \
    } catch (std::exception& e) {                                       \
        BOOST_FAIL("an exception was thrown by "#statement": " << e.what()); \
    } catch (...) {                                                     \
        BOOST_FAIL("a nonstandard exception was thrown by "#statement); \
    }

#define CHECK(expr)       CHECK_NO_THROW(BOOST_CHECK(expr))
#define CHECK_EQUAL(x, y) CHECK_NO_THROW(BOOST_CHECK_EQUAL(x, y))

//==========================================================================//

typedef map<int, int> TIntToIntMap;
typedef multimap<int, int> TIntToIntMultimap;
typedef map<int, string> TIntToStringMap;

static void
    s_MakeGeneInfoFileReaders(CGeneInfoFileReader*& pReader1,
                              CGeneInfoFileReader*& pReader2)
{
    pReader1 = new CGeneInfoFileReader(true);
    pReader2 = new CGeneInfoFileReader(false);
}

static void
    s_InitTestData(IGeneInfoInput::TGeneIdList& listIds,
                   IGeneInfoInput::TGiList& listGis,
                   TIntToIntMultimap& mapGiToIds,
                   IGeneInfoInput::TGeneIdToGeneInfoMap& mapIdToInfo)
{
    int geneId;

    // Initialize Gene IDs and Gene Infos

    TIntToIntMap mapIdToPMIDs;
    mapIdToPMIDs[1] = 1;
    mapIdToPMIDs[2] = 2;
    mapIdToPMIDs[3] = 2;
    mapIdToPMIDs[4] = 1;
    mapIdToPMIDs[5] = 1;
    mapIdToPMIDs[6] = 2;
    mapIdToPMIDs[7] = 0;

    TIntToStringMap mapIdToOrgname;
    mapIdToOrgname[1] = "unknown";
    mapIdToOrgname[2] = "Gallus gallus";
    mapIdToOrgname[3] = "Homo sapiens";
    mapIdToOrgname[4] = "Homo sapiens";
    mapIdToOrgname[5] = "Gallus gallus";
    mapIdToOrgname[6] = "Homo sapiens";
    mapIdToOrgname[7] = "unknown";

    for (geneId = 1; geneId <= 7; geneId++)
    {
        listIds.push_back(geneId);

        string strGeneId = NStr::IntToString(geneId);
        mapIdToInfo[geneId] = CRef<CGeneInfo>(new CGeneInfo(
            geneId,
            "GeneID" + strGeneId,
            "Description text for GeneID" + strGeneId,
            mapIdToOrgname[geneId],
            mapIdToPMIDs[geneId]));
    }

    // Link Gis to Gene IDs

    mapGiToIds.insert(TIntToIntMultimap::value_type(1, 7)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(2, 2)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(2, 3)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(2, 5)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(3, 4)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(4, 4)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(4, 6)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(10, 1)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(11, 1)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(11, 7)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(20, 1)); 
//  (21, 1), (21, 7) excluded: "Genomic" Gi, multiple IDs
    mapGiToIds.insert(TIntToIntMultimap::value_type(30, 2)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(31, 5)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(31, 4)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(32, 3)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(32, 6)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(40, 2)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(41, 2)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(42, 2)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(50, 5)); 
//  (60, 3), (60, 4) excluded: "Genomic" Gi, multiple IDs
    mapGiToIds.insert(TIntToIntMultimap::value_type(61, 4)); 
    mapGiToIds.insert(TIntToIntMultimap::value_type(62, 6)); 

    listGis.push_back(1);
    listGis.push_back(2);
    listGis.push_back(3);
    listGis.push_back(4);
    listGis.push_back(10);
    listGis.push_back(11);
    listGis.push_back(20);
//    listGis.push_back(21);
    listGis.push_back(30);
    listGis.push_back(31);
    listGis.push_back(32);
    listGis.push_back(40);
    listGis.push_back(41);
    listGis.push_back(42);
    listGis.push_back(50);
//    listGis.push_back(60);
    listGis.push_back(61);
    listGis.push_back(62);
}

static void
    s_InitGisWithNoGeneIds(IGeneInfoInput::TGiList& listGis)
{
    listGis.push_back(100);         // no gene links
    listGis.push_back(60);          // "Genomic" Gi, multiple gene links
}

static bool
    s_CheckPubMedLinkCount(int nLinks1, int nLinks2)
{
    while (nLinks1 != 0 && nLinks2 != 0)
    {
        nLinks1 /= 10;
        nLinks2 /= 10;
    }
    return nLinks1 == nLinks2;
}

static void
    s_CheckInfoEquality(CRef<CGeneInfo> info1,
                        CRef<CGeneInfo> info2)
{
    CHECK_EQUAL(info1->GetGeneId(),
                info2->GetGeneId());
    CHECK_EQUAL(info1->GetSymbol(),
                info2->GetSymbol());
    CHECK_EQUAL(info1->GetDescription(),
                info2->GetDescription());
    CHECK_EQUAL(info1->GetOrganismName(),
                info2->GetOrganismName());
    CHECK(s_CheckPubMedLinkCount(info1->GetNumPubMedLinks(),
                                 info2->GetNumPubMedLinks()));
}

struct SGeneInfoListSorter {
    bool operator() (const CRef<CGeneInfo>& a,
                     const CRef<CGeneInfo>& b) const
    {
        return a->GetGeneId() < b->GetGeneId();
    }
};

static void
    s_SortInfoList(IGeneInfoInput::TGeneInfoList& infoList)
{
    sort(infoList.begin(), infoList.end(), SGeneInfoListSorter() );
}

static void
    s_CheckInfoListEquality(IGeneInfoInput::TGeneInfoList& infoList1,
                            IGeneInfoInput::TGeneInfoList& infoList2)
{
    s_SortInfoList(infoList1);
    s_SortInfoList(infoList2);

    IGeneInfoInput::TGeneInfoList::iterator it1, it2;
    for (it1 = infoList1.begin(), it2 = infoList2.begin();
         it1 != infoList1.end() && it2 != infoList2.end();
         it1++, it2++)
    {
        s_CheckInfoEquality(*it1, *it2);
    }
    CHECK(it1 == infoList1.end() && it2 == infoList2.end());
    if (it1 != infoList1.end())
        cout << endl << "Extra info 1: " << **it1 << endl;
    if (it2 != infoList2.end())
        cout << endl << "Extra info 2: " << **it2 << endl;
}

static void
    s_FillExpectedInfoListForGi(int gi,
                                TIntToIntMultimap& mapGiToIds,
                                IGeneInfoInput::TGeneIdToGeneInfoMap& mapIdToInfo,
                                IGeneInfoInput::TGeneInfoList& infoList)
{
    // cout << endl << "Gene IDs for Gi=" << gi << ": ";
    TIntToIntMultimap::iterator itGiToGeneId = mapGiToIds.find(gi);
    while (itGiToGeneId != mapGiToIds.end() &&
           itGiToGeneId->first == gi)
    {
        int geneId = itGiToGeneId->second;
        // cout << geneId << " ";

        CRef<CGeneInfo> info = mapIdToInfo[geneId];
        infoList.push_back(info);

        // cout << endl << *info << endl;

        itGiToGeneId++;
    }
    // cout << endl;
}

static void
    s_CheckIntInList(int val, list<int>& listVals)
{
    CHECK(find(listVals.begin(), listVals.end(), val) != listVals.end());
}

static void
    s_CheckGiToGeneConsistency(int gi,
                               TIntToIntMultimap& mapGiToIds,
                               CGeneInfoFileReader *pReader)
{
    // see if this gi appears in the gi lists for each of its Gene IDs

    TIntToIntMultimap::iterator itGiToGeneId = mapGiToIds.find(gi);
    while (itGiToGeneId != mapGiToIds.end() &&
           itGiToGeneId->first == gi)
    {
        int geneId = itGiToGeneId->second;
        // cout << "\nGi's for GeneID=" << geneId << ": ";

        IGeneInfoInput::TGiList giListRNA, giListProtein, giListGenomic;
        bool bRNA, bProtein, bGenomic;
        bRNA     = pReader->GetRNAGisForGeneId(geneId, giListRNA);
        bProtein = pReader->GetProteinGisForGeneId(geneId, giListProtein);
        bGenomic = pReader->GetGenomicGisForGeneId(geneId, giListGenomic);
        CHECK(bRNA || bProtein || bGenomic);

        // cout << endl << "\tRNA Gi's: ";
        // s_OutputList(giListRNA);
        // cout << endl << "\tProtein Gi's: ";
        // s_OutputList(giListProtein);
        // cout << endl << "\tGenomic Gi's: ";
        // s_OutputList(giListGenomic);

        IGeneInfoInput::TGiList giListAll;
        copy(giListRNA.begin(), giListRNA.end(),
                back_inserter(giListAll));
        copy(giListProtein.begin(), giListProtein.end(),
                back_inserter(giListAll));
        copy(giListGenomic.begin(), giListGenomic.end(),
                back_inserter(giListAll));

        s_CheckIntInList(gi, giListAll);

        itGiToGeneId++;
    }
    // cout << endl;

    // see if this gi's Gene IDs appear in the actual Gene ID list
    // returned by the reader

    IGeneInfoInput::TGeneIdList geneIdsFromReader;
    CHECK(pReader->GetGeneIdsForGi(gi, geneIdsFromReader));

    itGiToGeneId = mapGiToIds.find(gi);
    while (itGiToGeneId != mapGiToIds.end() &&
           itGiToGeneId->first == gi)
    {
        int geneId = itGiToGeneId->second;
        s_CheckIntInList(geneId, geneIdsFromReader);

        itGiToGeneId++;
    }
}

//==========================================================================//
// Test successful Gi to Gene Info mapping

BOOST_AUTO_TEST_CASE(s_MainInfoReaderTest)
{
    CNcbiEnvironment env;
    env.Set(GENE_INFO_PATH_ENV_VARIABLE, "data/");

    try
    {
        IGeneInfoInput::TGeneIdList listGeneIds;
        IGeneInfoInput::TGiList listGis;
        TIntToIntMultimap mapGiToIds;
        IGeneInfoInput::TGeneIdToGeneInfoMap mapIdToInfo;

        s_InitTestData(listGeneIds, listGis,
                       mapGiToIds, mapIdToInfo);

        CGeneInfoFileReader *pReader1, *pReader2;
        CHECK_NO_THROW(s_MakeGeneInfoFileReaders(pReader1, pReader2));
        auto_ptr<CGeneInfoFileReader> fileReader1(pReader1);
        auto_ptr<CGeneInfoFileReader> fileReader2(pReader2);

        IGeneInfoInput::TGiList::iterator itGi = listGis.begin();
        for (; itGi != listGis.end(); itGi++)
        {
            int gi = *itGi;

            // cout << endl << "Processing new Gi: " << gi << endl;

            IGeneInfoInput::TGeneInfoList infoList1, infoList2,
                                          infoListExpected;
            CHECK(fileReader1->GetGeneInfoForGi(gi, infoList1));
            CHECK(fileReader2->GetGeneInfoForGi(gi, infoList2));

            s_FillExpectedInfoListForGi(gi, mapGiToIds,
                                        mapIdToInfo, infoListExpected);

            s_CheckInfoListEquality(infoList1, infoList2);
            s_CheckInfoListEquality(infoList1, infoListExpected);

            s_CheckGiToGeneConsistency(gi, mapGiToIds,
                                       fileReader1.get());
            s_CheckGiToGeneConsistency(gi, mapGiToIds,
                                       fileReader2.get());
        }
    }
    catch (CException& e)
    {
        BOOST_FAIL(e.what());
    }
}

//==========================================================================//
// Test Gis that are not mapped to a single Gene Id

BOOST_AUTO_TEST_CASE(s_GiWithNoGeneIdTest)
{
    try
    {
        IGeneInfoInput::TGiList listGis;
        s_InitGisWithNoGeneIds(listGis);

        CGeneInfoFileReader *pReader1, *pReader2;
        CHECK_NO_THROW(s_MakeGeneInfoFileReaders(pReader1, pReader2));
        auto_ptr<CGeneInfoFileReader> fileReader1(pReader1);
        auto_ptr<CGeneInfoFileReader> fileReader2(pReader2);

        IGeneInfoInput::TGiList::iterator itGi = listGis.begin();
        for (; itGi != listGis.end(); itGi++)
        {
            int gi = *itGi;

            IGeneInfoInput::TGeneInfoList infoList1, infoList2,
                                          infoListExpected;
            CHECK(!fileReader1->GetGeneInfoForGi(gi, infoList1));
            CHECK(!fileReader2->GetGeneInfoForGi(gi, infoList2));

            CHECK(infoList1.empty());
            CHECK(infoList2.empty());
        }
    }
    catch (CException& e)
    {
        BOOST_FAIL(e.what());
    }
}

//==========================================================================//
// Test basic functionality of the Gene Info class

BOOST_AUTO_TEST_CASE(s_TestGeneInfo)
{
    try
    {
        CGeneInfo info;
        CHECK(!info.IsInitialized());

        int geneId = 3481;
        string strSymbol = "IGF2";
        string strDescription =
            "insulin-like growth factor 2 (somatomedin A)";
        string strOrganism = "Homo sapiens";
        int nPubMedCount = 100;

        info = CGeneInfo(geneId,
                         strSymbol,
                         strDescription,
                         strOrganism,
                         nPubMedCount);

        CHECK(info.IsInitialized());
        CHECK(info.GetGeneId() == geneId);
        CHECK(info.GetSymbol() == strSymbol);
        CHECK(info.GetDescription() == strDescription);
        CHECK(info.GetOrganismName() == strOrganism);
        CHECK(info.GetNumPubMedLinks() == nPubMedCount);

        string strPlain, strHTML;
        CHECK_NO_THROW(info.ToString(strPlain, false));
        CHECK_NO_THROW(info.ToString(strHTML, true, "GENE_URL"));

        string strExpectedPlain =
                   " GENE ID: 3481 IGF2"
                   " | insulin-like growth factor 2 (somatomedin A)"
                   "\n[Homo sapiens]"
                   " (Over 100 PubMed links)";
        CHECK(strPlain == strExpectedPlain);

        string strExpectedHTML =
                   " <a href=\"GENE_URL\">GENE ID: 3481 IGF2</a>"
                   " | insulin-like growth factor 2 (somatomedin A)"
                   "\n[Homo sapiens]"
                   " <span class=\"Gene_PubMedLinks\">"
                     "(Over 100 PubMed links)</span>";
        CHECK(strHTML == strExpectedHTML);
    }
    catch (CException& e)
    {
        BOOST_FAIL(e.what());
    }
}

//==========================================================================//
// Test failed attempts to read Gene Info from an incorrect path

BOOST_AUTO_TEST_CASE(s_IncorrectPathTest)
{
    CGeneInfoFileReader *pReader1 = 0, *pReader2 = 0;

    CNcbiEnvironment env;
    string strDirPath = env.Get(GENE_INFO_PATH_ENV_VARIABLE);

    env.Set(GENE_INFO_PATH_ENV_VARIABLE, "./");
    BOOST_REQUIRE_THROW(pReader1 = new CGeneInfoFileReader(true),
                        CGeneInfoException);
    BOOST_REQUIRE_THROW(pReader1 = new CGeneInfoFileReader(false),
                        CGeneInfoException);

    env.Set(GENE_INFO_PATH_ENV_VARIABLE, "invalid_path");
    BOOST_REQUIRE_THROW(pReader1 = new CGeneInfoFileReader(true),
                        CGeneInfoException);
    BOOST_REQUIRE_THROW(pReader1 = new CGeneInfoFileReader(false),
                        CGeneInfoException);

    env.Set(GENE_INFO_PATH_ENV_VARIABLE, strDirPath);
    CHECK_NO_THROW(pReader1 = new CGeneInfoFileReader(true));
    CHECK_NO_THROW(pReader2 = new CGeneInfoFileReader(false));
    delete pReader1;
    delete pReader2;
}
#endif /* SKIP_DOXYGEN_PROCESSING */

//==========================================================================//

#endif /* WORDS_BIGENDIAN */
