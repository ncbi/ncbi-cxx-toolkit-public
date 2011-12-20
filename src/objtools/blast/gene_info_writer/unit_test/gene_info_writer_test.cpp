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

#include <objtools/blast/gene_info_writer/gene_info_writer.hpp>

#include <map>

//==========================================================================//
// Unit-testing includes

#include <corelib/test_boost.hpp>

//==========================================================================//

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;

//==========================================================================//

typedef map<int, int> TIntToIntMap;
typedef multimap<int, int> TIntToIntMultimap;
typedef map<int, string> TIntToStringMap;

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
    listGis.push_back(21);
    listGis.push_back(30);
    listGis.push_back(31);
    listGis.push_back(32);
    listGis.push_back(40);
    listGis.push_back(41);
    listGis.push_back(42);
    listGis.push_back(50);
    listGis.push_back(60);
    listGis.push_back(61);
    listGis.push_back(62);
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
    BOOST_REQUIRE_EQUAL(info1->GetGeneId(),
                info2->GetGeneId());
    BOOST_REQUIRE_EQUAL(info1->GetSymbol(),
                info2->GetSymbol());
    BOOST_REQUIRE_EQUAL(info1->GetDescription(),
                info2->GetDescription());
    BOOST_REQUIRE_EQUAL(info1->GetOrganismName(),
                info2->GetOrganismName());
    BOOST_REQUIRE(s_CheckPubMedLinkCount(info1->GetNumPubMedLinks(),
                                 info2->GetNumPubMedLinks()));
}

static void
    s_FillExpectedIdListForGi(int gi,
                              TIntToIntMultimap& mapGiToIds,
                              IGeneInfoInput::TGeneIdList& listIds)
{
    // cout << endl << "Gene IDs for Gi=" << gi << ": ";
    TIntToIntMultimap::iterator itGiToGeneId = mapGiToIds.find(gi);
    while (itGiToGeneId != mapGiToIds.end() &&
           itGiToGeneId->first == gi)
    {
        int geneId = itGiToGeneId->second;
        // cout << geneId << " ";

        listIds.push_back(geneId);
        itGiToGeneId++;
    }
    // cout << endl;
}

static void
    s_CheckIntInList(int val, list<int>& listVals)
{
    BOOST_REQUIRE(find(listVals.begin(), listVals.end(), val) != listVals.end());
}

static void
    s_CheckIntListEquality(list<int>& list1, list<int>& list2)
{
    for (list<int>::iterator it1 = list1.begin();
         it1 != list1.end(); it1++)
    {
        s_CheckIntInList(*it1, list2);
    }

    for (list<int>::iterator it2 = list2.begin();
         it2 != list2.end(); it2++)
    {
        s_CheckIntInList(*it2, list1);
    }
}

//==========================================================================//
// Test successful writing of the Gene info files
BOOST_AUTO_TEST_SUITE(gene_info_writer)

BOOST_AUTO_TEST_CASE(s_MainWritingTest)
{
    try
    {
        IGeneInfoInput::TGeneIdList listGeneIds;
        IGeneInfoInput::TGiList listGis;
        TIntToIntMultimap mapGiToIds;
        IGeneInfoInput::TGeneIdToGeneInfoMap mapIdToInfo;

        s_InitTestData(listGeneIds, listGis,
                       mapGiToIds, mapIdToInfo);

        string strGene2AccessionFile = "data/gene2accession_test.txt";
        string strGeneInfoFile = "data/gene_info_test.txt";
        string strGene2PubMedFile = "data/gene2pubmed_test.txt";
        string strOutputDirPath = "data/";

        CGeneFileWriter *pWriter = 0;
        BOOST_REQUIRE_NO_THROW(pWriter = new CGeneFileWriter(strGene2AccessionFile,
                                                      strGeneInfoFile,
                                                      strGene2PubMedFile,
                                                      strOutputDirPath));
        auto_ptr<CGeneFileWriter> fileWriter(pWriter);

        fileWriter->EnableMultipleGeneIdsForRNAGis(true);
        fileWriter->EnableMultipleGeneIdsForProteinGis(true);

        BOOST_REQUIRE_NO_THROW(fileWriter->ProcessFiles(true));
        fileWriter.reset(0);

        // open the generated binary files and verify the written data

        // Gene ID -> Gene Info

        CNcbiIfstream inAllGeneData;
        string strAllGeneData = strOutputDirPath +
                                GENE_ALL_GENE_DATA_FILE_NAME;
        BOOST_REQUIRE(CGeneFileUtils::
                OpenBinaryInputFile(strAllGeneData, inAllGeneData));

        CNcbiIfstream inGene2Offset;
        string strGene2Offset = strOutputDirPath +
                                GENE_GENE2OFFSET_FILE_NAME;
        BOOST_REQUIRE(CGeneFileUtils::
                OpenBinaryInputFile(strGene2Offset, inGene2Offset));

        TIntToIntMap mapIdToOffset;
        CGeneFileUtils::STwoIntRecord recordGene2Offset;
        while (inGene2Offset)
        {
            CGeneFileUtils::ReadRecord(inGene2Offset, recordGene2Offset);
            if (!inGene2Offset)
                break;

            s_CheckIntInList(recordGene2Offset.n1, listGeneIds);

            mapIdToOffset[recordGene2Offset.n1] = recordGene2Offset.n2;

            CRef<CGeneInfo> info;
            BOOST_REQUIRE_NO_THROW(
                    CGeneFileUtils::ReadGeneInfo(inAllGeneData,
                                                 recordGene2Offset.n2,
                                                 info));
            s_CheckInfoEquality(info, mapIdToInfo[recordGene2Offset.n1]);
        }

        // Gi -> Gene ID

        CNcbiIfstream inGi2Gene;
        string strGi2Gene = strOutputDirPath +
                                GENE_GI2GENE_FILE_NAME;
        BOOST_REQUIRE(CGeneFileUtils::
                OpenBinaryInputFile(strGi2Gene, inGi2Gene));
        
        TIntToIntMultimap mapGiToIdsFromFile;
        CGeneFileUtils::STwoIntRecord recordGi2Gene;
        while (inGi2Gene)
        {
            CGeneFileUtils::ReadRecord(inGi2Gene, recordGi2Gene);
            if (!inGi2Gene)
                break;

            s_CheckIntInList(recordGi2Gene.n1, listGis);
            s_CheckIntInList(recordGi2Gene.n2, listGeneIds);

            mapGiToIdsFromFile.insert(TIntToIntMultimap::value_type
                                      (recordGi2Gene.n1, 
                                       recordGi2Gene.n2));
        }

        IGeneInfoInput::TGiList::iterator itGi = listGis.begin();
        for (; itGi != listGis.end(); itGi++)
        {
            int gi = *itGi;

            IGeneInfoInput::TGeneIdList listIdsForGi;
            s_FillExpectedIdListForGi(gi, mapGiToIds,
                                      listIdsForGi);

            IGeneInfoInput::TGeneIdList listIdsFromFile;
            s_FillExpectedIdListForGi(gi, mapGiToIdsFromFile,
                                      listIdsFromFile);

            s_CheckIntListEquality(listIdsForGi, listIdsFromFile);
        }

        // Gene ID -> Gi

        CNcbiIfstream inGene2Gi;
        string strGene2Gi = strOutputDirPath +
                                GENE_GENE2GI_FILE_NAME;
        BOOST_REQUIRE(CGeneFileUtils::
                OpenBinaryInputFile(strGene2Gi, inGene2Gi));
        
        IGeneInfoInput::TGiList listGisFromFile;
        CGeneFileUtils::SMultiIntRecord<4> recordGene2Gi;
        while (inGene2Gi)
        {
            CGeneFileUtils::ReadRecord(inGene2Gi, recordGene2Gi);
            if (!inGene2Gi)
                break;

            int geneId = recordGene2Gi.n[0];
            s_CheckIntInList(geneId, listGeneIds);

            for (int iGi = 1; iGi <= 3; iGi++)
            {
                int gi = recordGene2Gi.n[iGi];
                if (gi == 0)
                    continue;

                s_CheckIntInList(gi, listGis);
                listGisFromFile.push_back(gi);

                IGeneInfoInput::TGeneIdList listIdsForGi;
                s_FillExpectedIdListForGi(gi, mapGiToIds,
                                          listIdsForGi);
                if (!listIdsForGi.empty())
                {
                    s_CheckIntInList(geneId, listIdsForGi);
                }
            }
        }

        s_CheckIntListEquality(listGis, listGisFromFile);

        // Gi -> Gene Info

        CNcbiIfstream inGi2Offset;
        string strGi2Offset = strOutputDirPath +
                                GENE_GI2OFFSET_FILE_NAME;
        BOOST_REQUIRE(CGeneFileUtils::
                OpenBinaryInputFile(strGi2Offset, inGi2Offset));

        int curGiFromFile = 0;
        list<int> listOffsetsFromFile;
        CGeneFileUtils::STwoIntRecord recordGi2Offset;
        while (inGi2Offset)
        {
            CGeneFileUtils::ReadRecord(inGi2Offset, recordGi2Offset);
            if (!inGi2Offset)
                break;

            int gi = recordGi2Offset.n1;
            int offset = recordGi2Offset.n2;

            s_CheckIntInList(gi, listGis);

            if (gi != curGiFromFile)
            {
                if (curGiFromFile > 0)
                {
                    IGeneInfoInput::TGeneIdList listIdsForGi;
                    s_FillExpectedIdListForGi(curGiFromFile, mapGiToIds,
                                              listIdsForGi);
                    BOOST_REQUIRE(!listIdsForGi.empty());

                    list<int> listOffsets;
                    for (IGeneInfoInput::TGeneIdList::iterator it =
                         listIdsForGi.begin();
                         it != listIdsForGi.end(); it++)
                    {
                        listOffsets.push_back(mapIdToOffset[*it]);
                    }

                    s_CheckIntListEquality(listOffsets, listOffsetsFromFile);
                }

                listOffsetsFromFile.clear();
                curGiFromFile = gi;
            }

            listOffsetsFromFile.push_back(offset);
        }
    }
    catch (CException& e)
    {
        BOOST_FAIL(e.what());
    }
}

//==========================================================================//
// Test failed writing of the Gene Info files

BOOST_AUTO_TEST_CASE(s_FailedWritingTest)
{
    CGeneFileWriter *pWriter = 0;

    string strGene2AccessionFile = "error";
    string strGeneInfoFile = "error";
    string strGene2PubMedFile = "error";
    string strOutputDirPath = "data/";

    BOOST_REQUIRE_THROW(pWriter =
                        new CGeneFileWriter(strGene2AccessionFile,
                                            strGeneInfoFile,
                                            strGene2PubMedFile,
                                            strOutputDirPath);,
                        CGeneInfoException);

    strGene2AccessionFile = "data/gene2accession_test.txt";
    strGeneInfoFile = "data/gene_info_test.txt";
    strGene2PubMedFile = "data/gene2pubmed_test.txt";
    strOutputDirPath = "error/";

    BOOST_REQUIRE_THROW(pWriter =
                        new CGeneFileWriter(strGene2AccessionFile,
                                            strGeneInfoFile,
                                            strGene2PubMedFile,
                                            strOutputDirPath),
                        CGeneInfoException);
    BOOST_REQUIRE(pWriter == NULL);
}

BOOST_AUTO_TEST_SUITE_END()
#endif /* SKIP_DOXYGEN_PROCESSING */
