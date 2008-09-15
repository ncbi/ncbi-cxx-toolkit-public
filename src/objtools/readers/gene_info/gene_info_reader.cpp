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
 * Authors:  Vahram Avagyan
 *
 */

/// @file gene_info_reader.cpp
/// Implementation of reading Gene information from files.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

//==========================================================================//

#include <ncbi_pch.hpp>
#include <corelib/env_reg.hpp>

#include <objtools/readers/gene_info/gene_info_reader.hpp>
#include <objtools/readers/gene_info/file_utils.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE

//==========================================================================//
// Constants

/// Index of the RNA Gi field in the Gene ID to Gi records.
static const int k_iRNAGiField = 1;
/// Index of the Protein Gi field in the Gene ID to Gi records.
static const int k_iProteinGiField = 2;
/// Index of the Genomic Gi field in the Gene ID to Gi records.
static const int k_iGenomicGiField = 3;

//==========================================================================//

/// Returns the field of a record given its index.
///
/// @param record
///     Two-integer record.
/// @param iField
///     Index of the field in record.
/// @return
///     Corresponding field of the record.
/* static */ int& s_GetField(CGeneInfoFileReader::STwoIntRecord& record,
                       int iField)
{
    if (iField == 0)
        return record.n1;
    return record.n2;
}

/// Returns the field of a record given its index.
///
/// @param record
///     N-integer record.
/// @param iField
///     Index of the field in record.
/// @return
///     Corresponding field of the record.
template<int k_nFields>
/* static */ int& s_GetField(CGeneInfoFileReader::
                            SMultiIntRecord<k_nFields>& record,
                       int iField)
{
    return record.n[iField];
}

/// Searches an array of records sorted by the first field.
///
/// The function implements a variety of binary search that
/// will locate the first record in the array whose first
/// field matches the input key.
///
/// @param pRecs
///     Pointer to the records.
/// @param nRecs
///     Number of records in the array.
/// @param n1
///     Key to search for.
/// @param iFirstIndex
///     Will be set to the first record matching the key, if any.
/// @return
///     True if any records were found matching the key.
template <typename TRecordType>
/* static */ bool s_SearchSortedArray(TRecordType* pRecs, int nRecs,
                         int n1, int& iFirstIndex)
{
    int iRecBeg = 0, iRecEnd = nRecs;
    int iRecMid, n1Mid;
    while (iRecBeg < iRecEnd)
    {
        iRecMid = (iRecBeg + iRecEnd) / 2;
        n1Mid = s_GetField(pRecs[iRecMid], 0);
        if (n1Mid < n1)
            iRecBeg = iRecMid + 1;
        else
            iRecEnd = iRecMid;
    }
    if (iRecEnd < nRecs)
        if (s_GetField(pRecs[iRecEnd], 0) == n1)
        {
            iFirstIndex = iRecEnd;
            return true;
        }
    return false;
}

/// Sorts and filters a list of integers.
///
/// @param listVals
///     List of integers to sort and filter.
/// @param bRemoveZeros
///     Remove zeros from the list.
/* static */ void s_SortAndFilter(list<int>& listVals, bool bRemoveZeros)
{
    listVals.sort();
    listVals.unique();
    if (bRemoveZeros)
    {
        while (!listVals.empty() &&
               listVals.front() == 0)
        {
            listVals.pop_front();
        }
    }
}

/// Searches an array of records sorted by the first field.
///
/// The function returns the list of values of a given field
/// of all the records whose first field matches the key. The
/// list of values is sorted, filtered, and all the zeros are
/// removed (i.e. returns unique sorted positive integers).
///
/// @param pRecs
///     Pointer to the records.
/// @param nRecs
///     Number of records in the array.
/// @param n1
///     Key to search for.
/// @param iField
///     Return matching records' fields at this index.
/// @param listFieldVals
///     List of returned values.
/// @param bRemoveZeros
///     Remove zeros from the list of returned values.
/// @return
///     True if any records were found matching the key.
template <typename TRecordType>
static bool s_SearchSortedArray(TRecordType* pRecs, int nRecs,
                         int n1, int iField,
                         list<int>& listFieldVals,
                         bool bRemoveZeros)
{
    int iFirstIndex = -1;
    if (s_SearchSortedArray(pRecs, nRecs, n1, iFirstIndex))
    {
        while (iFirstIndex < nRecs &&
               s_GetField(pRecs[iFirstIndex], 0) == n1)
        {
            listFieldVals.push_back(
                s_GetField(pRecs[iFirstIndex], iField));
            iFirstIndex++;
        }
        s_SortAndFilter(listFieldVals, bRemoveZeros);
        return true;
    }
    return false;
}

/// Interprets a memory file as a record array.
///
/// @param pMemFile
///     Pointer to a valid and initialized CMemoryFile.
/// @param pRecs
///     Set to pointer to the records.
/// @param nRecs
///     Set to number of records in the array.
/// @return
///     True if conversion to a pointer was successful.
template <typename TRecordType>
static bool s_GetMemFilePtrAndLength(CMemoryFile* pMemFile,
                              TRecordType*& pRecs, int& nRecs)
{
    if (pMemFile != 0)
    {
        nRecs = pMemFile->GetSize() / (sizeof(TRecordType));
        if (nRecs > 0)
        {
            pRecs = (TRecordType*)(pMemFile->GetPtr());
            return pRecs != 0;
        }
    }
    return false;
}

//==========================================================================//

void CGeneInfoFileReader::x_MapMemFiles()
{
    if (!CheckExistence(m_strGi2GeneFile))
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
        "Gi->GeneId processed file not found: " + m_strGi2GeneFile);
    m_memGi2GeneFile.reset(new CMemoryFile(m_strGi2GeneFile));

    if (!CheckExistence(m_strGene2OffsetFile))
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
        "GeneId->Offset processed file not found: " + m_strGene2OffsetFile);
    m_memGene2OffsetFile.reset(new CMemoryFile(m_strGene2OffsetFile));

    if (m_bGiToOffsetLookup)
    {
        if (!CheckExistence(m_strGi2OffsetFile))
            NCBI_THROW(CGeneInfoException, eFileNotFoundError,
            "Gi->Offset processed file not found: " + m_strGi2OffsetFile);
        m_memGi2OffsetFile.reset(new CMemoryFile(m_strGi2OffsetFile));
    }

    if (!CheckExistence(m_strGene2GiFile))
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
        "Gene->Gi processed file not found: " + m_strGene2GiFile);
    m_memGene2GiFile.reset(new CMemoryFile(m_strGene2GiFile));
}

void CGeneInfoFileReader::x_UnmapMemFiles()
{
    if (m_memGi2GeneFile.get() != 0)
        m_memGi2GeneFile->Unmap();

    if (m_memGene2OffsetFile.get() != 0)
        m_memGene2OffsetFile->Unmap();

    if (m_memGi2OffsetFile.get() != 0)
        m_memGi2OffsetFile->Unmap();

    if (m_memGene2GiFile.get() != 0)
        m_memGene2GiFile->Unmap();
}

bool CGeneInfoFileReader::x_GiToGeneId(int gi, list<int>& listGeneIds)
{
    STwoIntRecord* pRecs;
    int nRecs;
    bool retval = false;
    if (s_GetMemFilePtrAndLength(m_memGi2GeneFile.get(),
                                 pRecs, nRecs))
    {
        retval = s_SearchSortedArray(pRecs, nRecs,
                                   gi, 1, listGeneIds, false);
    }
    else
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
            "Cannot access the memory-mapped file for "
            "Gi to Gene ID conversion.");
    }

    return retval;
}

bool CGeneInfoFileReader::x_GeneIdToOffset(int geneId, int& nOffset)
{
    STwoIntRecord* pRecs;
    int nRecs;
    if (s_GetMemFilePtrAndLength(m_memGene2OffsetFile.get(),
                                 pRecs, nRecs))
    {
        int iIndex = -1;
        if (s_SearchSortedArray(pRecs, nRecs,
                                geneId, iIndex))
        {
            nOffset = s_GetField(pRecs[iIndex], 1);
            return true;
        }
    }
    else
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
            "Cannot access the memory-mapped file for "
            "Gene ID to Gene Info Offset conversion.");
    }

    return false;
}

bool CGeneInfoFileReader::x_GiToOffset(int gi, list<int>& listOffsets)
{
    if (!m_bGiToOffsetLookup)
    {
        NCBI_THROW(CGeneInfoException, eInternalError,
                   "Gi to offset lookup is disabled.");
    }

    STwoIntRecord* pRecs;
    int nRecs;
    bool retval = false;
    if (s_GetMemFilePtrAndLength(m_memGi2OffsetFile.get(),
                                 pRecs, nRecs))
    {
        retval = s_SearchSortedArray(pRecs, nRecs,
                                   gi, 1, listOffsets, false);
    }
    else
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
            "Cannot access the memory-mapped file for "
            "Gi to Gene Info Offset conversion.");
    }

    return retval;
}

bool CGeneInfoFileReader::x_GeneIdToGi(int geneId, int iGiField,
                                       list<int>& listGis)
{
    SMultiIntRecord<4>* pRecs;
    int nRecs;
    bool retval = false;
    if (s_GetMemFilePtrAndLength(m_memGene2GiFile.get(),
                                 pRecs, nRecs))
    {
        retval = s_SearchSortedArray(pRecs, nRecs,
                                   geneId, iGiField, listGis, true);
    }
    else
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
            "Cannot access the memory-mapped file for "
            "Gene ID to Gi conversion.");
    }

    return retval;
}


bool CGeneInfoFileReader::x_OffsetToInfo(int nOffset, CRef<CGeneInfo>& info)
{
    // read the line at nOffset from the gene data file
    ReadGeneInfo(m_inAllData, nOffset, info);
    return true;
}

//==========================================================================//

CGeneInfoFileReader::CGeneInfoFileReader(const string& strGi2GeneFile,
                                         const string& strGene2OffsetFile,
                                         const string& strGi2OffsetFile,
                                         const string& strAllGeneDataFile,
                                         const string& strGene2GiFile,
                                         bool bGiToOffsetLookup)
    : m_strGi2GeneFile(strGi2GeneFile),
      m_strGene2OffsetFile(strGene2OffsetFile),
      m_strGi2OffsetFile(strGi2OffsetFile),
      m_strGene2GiFile(strGene2GiFile),
      m_strAllGeneDataFile(strAllGeneDataFile),
      m_bGiToOffsetLookup(bGiToOffsetLookup)
{
    if (!OpenBinaryInputFile(m_strAllGeneDataFile, m_inAllData))
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
            "Cannot open the Gene Data file for reading: " +
            m_strAllGeneDataFile);
    }

    x_MapMemFiles();
}

/// Find the path to the gene info files, first checking the environment
/// variable GENE_INFO_PATH, then the section BLAST, label
/// GENE_INFO_PATH in the NCBI configuration file. If not found in either
/// location, try the $BLASTDB/features directory. If all fails return the
/// current working directory
/// @sa s_FindPathToWM
static string
s_FindPathToGeneInfoFiles(void)
{
    string retval = kEmptyStr;
    const string kSection("BLAST");
    CNcbiIstrstream empty_stream(kEmptyCStr);
    CRef<CNcbiRegistry> reg(new CNcbiRegistry(empty_stream,
                                              IRegistry::fWithNcbirc));
    CRef<CSimpleEnvRegMapper> mapper(new CSimpleEnvRegMapper(kSection,
                                                             kEmptyStr));
    CRef<CEnvironmentRegistry> env_reg(new CEnvironmentRegistry);
    env_reg->AddMapper(*mapper, CEnvironmentRegistry::ePriority_Max);
    reg->Add(*env_reg, CNcbiRegistry::ePriority_MaxUser);
    retval = reg->Get(kSection, GENE_INFO_PATH_ENV_VARIABLE);

    // Try the features subdirectory in the BLAST database storage location
    if (retval == kEmptyStr) {
        if ( (retval = reg->Get(kSection, "BLASTDB")) != kEmptyStr) {
            retval = CDirEntry::ConcatPath(retval, "features");
            if ( !CDir(retval).Exists() ) {
                retval = kEmptyStr;
            }
        }
    }

    if (retval == kEmptyStr) {
        retval = CDir::GetCwd();
    }
#if defined(NCBI_OS_MSWIN)
	// We address this here otherwise CDirEntry::IsAbsolutePath() fails
	if (NStr::StartsWith(retval, "//")) {
		NStr::ReplaceInPlace(retval, "//", "\\\\");
	}
#endif
    return retval;
}

CGeneInfoFileReader::CGeneInfoFileReader(bool bGiToOffsetLookup)
    : m_bGiToOffsetLookup(bGiToOffsetLookup)
{
    string strDirPath = s_FindPathToGeneInfoFiles();
    if (strDirPath.length() == 0 ||
        !CheckDirExistence(strDirPath))
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
            "Invalid path to Gene info directory: " +
            strDirPath);
    }
    strDirPath = CDirEntry::AddTrailingPathSeparator(strDirPath);

    m_strGi2GeneFile = strDirPath + GENE_GI2GENE_FILE_NAME;
    m_strGene2OffsetFile = strDirPath + GENE_GENE2OFFSET_FILE_NAME;
    m_strGi2OffsetFile = strDirPath + GENE_GI2OFFSET_FILE_NAME;
    m_strGene2GiFile = strDirPath + GENE_GENE2GI_FILE_NAME;
    m_strAllGeneDataFile = strDirPath + GENE_ALL_GENE_DATA_FILE_NAME;

    if (!OpenBinaryInputFile(m_strAllGeneDataFile, m_inAllData))
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
            "Cannot open the Gene Data file for reading: " +
            m_strAllGeneDataFile);
    }

    x_MapMemFiles();
}


CGeneInfoFileReader::~CGeneInfoFileReader()
{
    x_UnmapMemFiles();
}

bool CGeneInfoFileReader::
        GetGeneIdsForGi(int gi, TGeneIdList& geneIdList)
{
    return x_GiToGeneId(gi, geneIdList);
}

bool CGeneInfoFileReader::
        GetRNAGisForGeneId(int geneId, TGiList& giList)
{
    return x_GeneIdToGi(geneId, k_iRNAGiField, giList);
}

bool CGeneInfoFileReader::
        GetProteinGisForGeneId(int geneId, TGiList& giList)
{
    return x_GeneIdToGi(geneId, k_iProteinGiField, giList);
}

bool CGeneInfoFileReader::
        GetGenomicGisForGeneId(int geneId, TGiList& giList)
{
    return x_GeneIdToGi(geneId, k_iGenomicGiField, giList);
}

bool CGeneInfoFileReader::GetGeneInfoForGi(int gi, TGeneInfoList& infoList)
{
    bool bSuccess = false;
    if (m_bGiToOffsetLookup)
    {            
        int nOffset = 0;
        CRef<CGeneInfo> info;
        list<int> listOffsets;
        if (x_GiToOffset(gi, listOffsets))
        {                
            list<int>::const_iterator itOffset = listOffsets.begin();
            for (; itOffset != listOffsets.end(); itOffset++)
            {
                nOffset = *itOffset;
                if (x_OffsetToInfo(nOffset, info))
                {
                    infoList.push_back(info);
                    bSuccess = true;
                }
            }
        }
    }
    else
    {
        list<int> listGeneIds;
        if (x_GiToGeneId(gi, listGeneIds))
        {
            list<int>::const_iterator itId = listGeneIds.begin();
            for (; itId != listGeneIds.end(); itId++)
            {
                if (GetGeneInfoForId(*itId, infoList))
                    bSuccess = true;
                else
                {
                    NCBI_THROW(CGeneInfoException, eDataFormatError,
                                "Gene info not found for Gene ID:" +
                                NStr::IntToString(*itId) +
                                " linked from valid Gi:" +
                                NStr::IntToString(gi));
                }
            }
        }
    }
    return bSuccess;
}

bool CGeneInfoFileReader::GetGeneInfoForId(int geneId, TGeneInfoList& infoList)
{
    bool bSuccess = false;
    if (m_mapIdToInfo.find(geneId) != m_mapIdToInfo.end())
    {
        infoList.push_back(m_mapIdToInfo[geneId]);
        bSuccess = true;
    }
    else
    {
        int nOffset = 0;
        CRef<CGeneInfo> info;

        if (x_GeneIdToOffset(geneId, nOffset))
        {
            if (x_OffsetToInfo(nOffset, info))
            {
                infoList.push_back(info);
                m_mapIdToInfo.insert(make_pair(geneId, info));
                bSuccess = true;
            }
            else
            {
                NCBI_THROW(CGeneInfoException, eDataFormatError,
                            "Invalid Offset:" +
                            NStr::IntToString(nOffset) +
                            " for Gene ID:" +
                            NStr::IntToString(geneId));
            }
        }
    }
    return bSuccess;
}

//==========================================================================//

END_NCBI_SCOPE
