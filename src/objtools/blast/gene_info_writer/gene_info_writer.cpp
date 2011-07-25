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

//==========================================================================//

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/blast/gene_info_writer/gene_info_writer.hpp>
#include "gene_file_defs.hpp"

#include <algorithm>

BEGIN_NCBI_SCOPE

//==========================================================================//
// General file processing

void CGeneFileWriter::x_ReadAndProcessFile
(
    CNcbiIfstream& in,
    CLineProcessor* pLineProcessor,
    TTwoIntRecordVec& vecRecords,
    int nMinLineLength,
    int nMaxLineLength
)
{
    int nBufSize = nMaxLineLength;
    char* pBuf = new char[nBufSize + 1];
    try
    {
        while (in)
        {
            in.getline(pBuf, nBufSize);
            string strBuf = string(pBuf);

            if (int(strBuf.length()) >= nMinLineLength)
            {
                pLineProcessor->Process(strBuf, vecRecords);
            }
        }
        delete [] pBuf;
    }
    catch (...)
    {
        delete [] pBuf;
        throw;
    }
}

//==========================================================================//
// Data conversion and comparison functions

void CGeneFileWriter::
        x_GetOrgnameForTaxId(int nTaxId,
                             string& strName)
{
    if (m_seqDb.Empty())
    {
        m_seqDb.Reset(new CSeqDBExpert());
    }

    try
    {
        SSeqDBTaxInfo taxInfo;
        m_seqDb->GetTaxInfo(nTaxId, taxInfo);
        strName = taxInfo.scientific_name;
    }
    catch (CException)
    {
        strName = "unknown";
    }
}

bool CGeneFileWriter::x_GetOffsetForGeneId(int geneId, int& nOffset)
{
    TIntToIntMap::iterator itIdToOffset =
        m_mapIdToOffset.find(geneId);
    if (itIdToOffset != m_mapIdToOffset.end())
    {
        nOffset = itIdToOffset->second;
        return true;
    }
    return false;
}

int CGeneFileWriter::x_GetNumPubMedLinksForGeneId(int geneId)
{
    int nPubMedLinks = 0;
    TIntToIntMap::iterator itPM =
        m_mapIdToNumPMIDs.find(geneId);
    if (itPM != m_mapIdToNumPMIDs.end())
    {
        nPubMedLinks = itPM->second;
    }
    return nPubMedLinks;
}

bool CGeneFileWriter::
        x_CompareTwoIntRecords(const STwoIntRecord& record1,
                                    const STwoIntRecord& record2)
{
    return record1.n1 < record2.n1 ||
           (record1.n1 == record2.n1 &&
            record1.n2 < record2.n2);
}

bool CGeneFileWriter::
        x_CompareFourIntRecords(const TFourIntRecord& record1,
                                    const TFourIntRecord& record2)
{
    return (record1.n[0] < record2.n[0] ||
           (record1.n[0] == record2.n[0] &&
             (record1.n[1] < record2.n[1] ||
             (record1.n[1] == record2.n[1] &&
               (record1.n[2] < record2.n[2] ||
               (record1.n[2] == record2.n[2] &&
                 (record1.n[3] < record2.n[3])))))));
}

//==========================================================================//
// Gene->Accession file processing

bool CGeneFileWriter::
        x_Gene2Accn_ParseLine(const string& strLine,
                             SGene2AccnLine& lineData)
{
    if (NStr::StartsWith(strLine, "#"))
        return false;

    vector<string> strItems;
    NStr::Tokenize(strLine, "\t", strItems);

    if (strItems.size() != GENE_2_ACCN_NUM_ITEMS)
    {
        NCBI_THROW(CGeneInfoException, eDataFormatError,
                   "Gene2Accession file format not recognized.");
    }

    // read taxId
    if (strItems[GENE_2_ACCN_TAX_ID_INDEX] != "-")
        lineData.nTaxId =
            NStr::StringToInt(strItems[GENE_2_ACCN_TAX_ID_INDEX]);
    else
        lineData.nTaxId = 0;

    // read geneId
    if (strItems[GENE_2_ACCN_GENE_ID_INDEX] != "-")
        lineData.geneId =
            NStr::StringToInt(strItems[GENE_2_ACCN_GENE_ID_INDEX]);
    else
        lineData.geneId = 0;

    // read RNA nucleotide Gi
    if (strItems[GENE_2_ACCN_RNA_GI_INDEX] != "-")
        lineData.giRNANucl =
            NStr::StringToInt(strItems[GENE_2_ACCN_RNA_GI_INDEX]);
    else
        lineData.giRNANucl = 0;

    // read protein Gi
    if (strItems[GENE_2_ACCN_PROT_GI_INDEX] != "-")
        lineData.giProt =
            NStr::StringToInt(strItems[GENE_2_ACCN_PROT_GI_INDEX]);
    else
        lineData.giProt = 0;

    // read genomic nucleotide Gi
    if (strItems[GENE_2_ACCN_GENOMIC_GI_INDEX] != "-")
        lineData.giGenomicNucl =
            NStr::StringToInt(strItems[GENE_2_ACCN_GENOMIC_GI_INDEX]);
    else
        lineData.giGenomicNucl = 0;

    return true;
}

void CGeneFileWriter::
        x_Gene2Accn_LineToRecord(const SGene2AccnLine& lineData,
                                 TTwoIntRecordVec& vecRecords)
{
    if (lineData.nTaxId > 0 && lineData.geneId > 0)
    {
        STwoIntRecord record;
        record.n2 = lineData.geneId;

        TFourIntRecord recordGeneIdToGi;
        recordGeneIdToGi.n[0] = lineData.geneId;
        recordGeneIdToGi.n[1] = 0;
        recordGeneIdToGi.n[2] = 0;
        recordGeneIdToGi.n[3] = 0;

        if (lineData.giRNANucl > 0)
        {
            record.n1 = lineData.giRNANucl;
            vecRecords.push_back(record);

            m_mapGiToType.insert(TIntToIntMap::value_type(lineData.giRNANucl,
                                                          eRNAGi));
            m_nTotalGis++;
            m_nRNAGis++;

            recordGeneIdToGi.n[1] = lineData.giRNANucl;
        }
        if (lineData.giProt > 0)
        {
            record.n1 = lineData.giProt;
            vecRecords.push_back(record);

            m_mapGiToType.insert(TIntToIntMap::value_type(lineData.giProt,
                                                          eProtGi));
            m_nTotalGis++;
            m_nProtGis++;

            recordGeneIdToGi.n[2] = lineData.giProt;
        }
        if (lineData.giGenomicNucl > 0)
        {
            record.n1 = lineData.giGenomicNucl;
            vecRecords.push_back(record);

            m_mapGiToType.insert(TIntToIntMap::value_type
                                 (lineData.giGenomicNucl, eGenomicGi));
            m_nTotalGis++;
            m_nGenomicGis++;

            recordGeneIdToGi.n[3] = lineData.giGenomicNucl;
        }

        m_vecGeneIdToGiRecords.push_back(recordGeneIdToGi);
    }
}

void CGeneFileWriter::CGene2AccnProcessor::
        Process(const string& strLine,
                TTwoIntRecordVec& vecRecords)
{
    SGene2AccnLine lineData;
    if (m_pThis->x_Gene2Accn_ParseLine(strLine, lineData))
    {
        m_pThis->x_Gene2Accn_LineToRecord(lineData, vecRecords);
    }
}

void CGeneFileWriter::
        x_Gene2Accn_Filter(const TTwoIntRecordVec& vecRecords,
                           size_t iRec,
                           bool& bUnique,
                           TTwoIntRecordVec& vecFiltered)
{
    STwoIntRecord recordToAdd = vecRecords[iRec - 1];

    // has this record been added already?
    bool bHasBeenAdded = false;
    if (vecFiltered.size() > 0)
        if (vecFiltered.back().n1 == recordToAdd.n1 &&
            vecFiltered.back().n2 == recordToAdd.n2)
            bHasBeenAdded = true;

    // should we add all records with this Gi?
    bool bAddAllForGi =
        (m_bAllowMultipleIds_RNAGis &&
            m_mapGiToType[recordToAdd.n1] == eRNAGi) ||
        (m_bAllowMultipleIds_ProtGis &&
            m_mapGiToType[recordToAdd.n1] == eProtGi) ||
        (m_bAllowMultipleIds_GenomicGis &&
            m_mapGiToType[recordToAdd.n1] == eGenomicGi);

    // is this record the last one in its group of same-Gi records?
    bool bLastInGroup = false;
    if (iRec < vecRecords.size())
        bLastInGroup = vecRecords[iRec].n1 != recordToAdd.n1;
    else
        bLastInGroup = true;

    // should we add this record?
    bool bAddPrev = false;
    if (bAddAllForGi)
        bAddPrev = !bHasBeenAdded;
    else
        bAddPrev = bUnique && bLastInGroup;

    if (bAddPrev)
        vecFiltered.push_back(recordToAdd);
    
    if (iRec < vecRecords.size())
    {
        // update parameters for the next coming record (iRec)
        if (bLastInGroup)
        {
            bUnique = true;
        }
        else if (bUnique)
        {
            bUnique = vecRecords[iRec].n2 == recordToAdd.n2;
        }
    }
}

void CGeneFileWriter::x_Gene2Accn_ProcessFile(bool bOverwrite)
{
    if (!bOverwrite &&
        CheckExistence(m_strGi2GeneFile) &&
        CheckExistence(m_strGi2OffsetFile))
    {
        return; // files exist, do not overwrite
    }

    // open the original and processed files

    CNcbiIfstream inGene2Accn;
    CNcbiOfstream outGi2Gene, outGi2Offset, outGene2Gi;

    if (!OpenTextInputFile(m_strGene2AccessionFile, inGene2Accn))
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
                   "Cannot open Gene2Accession file for reading.");
    }
    if (!OpenBinaryOutputFile(m_strGi2GeneFile, outGi2Gene))
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
                   "Cannot open Gi2Gene file for writing.");
    }
    if (!OpenBinaryOutputFile(m_strGi2OffsetFile, outGi2Offset))
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
                   "Cannot open Gi2Offset file for writing.");
    }
    if (!OpenBinaryOutputFile(m_strGene2GiFile, outGene2Gi))
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
                   "Cannot open Gene2Gi file for writing.");
    }

    // estimate the number of records we will have

    Int8 nTotalLenght = GetLength(m_strGene2AccessionFile);
    TSeqPos nNumLinesEstimate = (TSeqPos)nTotalLenght / GENE_2_ACCN_LINE_MIN;

    // create the array of (gi, geneId) records

    TTwoIntRecordVec vecRecords;
    vecRecords.reserve(nNumLinesEstimate);

    m_vecGeneIdToGiRecords.reserve(nNumLinesEstimate);

    // parse each line and populate the records array
    // also populate the m_mapGiToType map with the Gi types

    auto_ptr<CLineProcessor> proc(new CGene2AccnProcessor(this));
    x_ReadAndProcessFile
                (inGene2Accn,
                 proc.get(),
                 vecRecords,
                 GENE_2_ACCN_LINE_MIN,
                 GENE_2_ACCN_LINE_MAX);

    // sort the records, remove all those Gis linking to multiple Gene Ids

    sort(vecRecords.begin(), vecRecords.end(),
         x_CompareTwoIntRecords);

    TTwoIntRecordVec vecFiltered;
    vecFiltered.reserve(vecRecords.size());

    if (vecRecords.size() <= 1)
    {
        NCBI_THROW(CGeneInfoException, eDataFormatError,
                   "Less than 2 records in the Gene2Accession file.");
    }

    size_t iRec;
    bool bUnique = true;
    for (iRec = 1; iRec <= vecRecords.size(); iRec++)   // (!) yes, <=
    {
         x_Gene2Accn_Filter(vecRecords, iRec,
                            bUnique, vecFiltered);
    }

    // write the filtered records to the gi->geneId file
    // and the corresponding (gi, offset) pairs to the gi->offset file

    STwoIntRecord recordGiToOffset;
    for (iRec = 0; iRec < vecFiltered.size(); iRec++)
    {
        WriteRecord(outGi2Gene, vecFiltered[iRec]);

        if (x_GetOffsetForGeneId(vecFiltered[iRec].n2,
                                 recordGiToOffset.n2))
        {
            recordGiToOffset.n1 = vecFiltered[iRec].n1;
            WriteRecord(outGi2Offset, recordGiToOffset);
        }
        else
        {
            NCBI_THROW(CGeneInfoException, eDataFormatError,
                "Offset not found for gene Id: " + 
                NStr::IntToString(vecFiltered[iRec].n2));
        }
    }

    // sort the (GeneId, RNAGi, ProteinGi, GenomicGi) records

    sort(m_vecGeneIdToGiRecords.begin(), m_vecGeneIdToGiRecords.end(),
         x_CompareFourIntRecords);

    // write the (GeneId, RNAGi, ProteinGi, GenomicGi) records
    // to the GeneId->Gi file

    for (iRec = 0; iRec < m_vecGeneIdToGiRecords.size(); iRec++)
    {
        WriteRecord(outGene2Gi, m_vecGeneIdToGiRecords[iRec]);
    }
}

//==========================================================================//
// Gene Info file processing

bool CGeneFileWriter::
        x_GeneInfo_ParseLine(const string& strLine,
                             SGeneInfoLine& lineData)
{
    if (NStr::StartsWith(strLine, "#"))
        return false;

    vector<string> strItems;
    NStr::Tokenize(strLine, "\t", strItems);

    if (strItems.size() != GENE_INFO_NUM_ITEMS)
    {
        NCBI_THROW(CGeneInfoException, eDataFormatError,
            "GeneInfo file format not recognized.\nLine: " + strLine +
            "\nFound " + NStr::SizetToString(strItems.size()) + " items.");
    }

    // read geneId
    if (strItems[GENE_INFO_GENE_ID_INDEX] != "-")
        lineData.geneId =
            NStr::StringToInt(strItems[GENE_INFO_GENE_ID_INDEX]);
    else
        lineData.geneId = 0;

    // read taxId
    if (strItems[GENE_INFO_TAX_ID_INDEX] != "-")
        lineData.nTaxId =
            NStr::StringToInt(strItems[GENE_INFO_TAX_ID_INDEX]);
    else
        lineData.nTaxId = 0;

    // read gene name
    if (strItems[GENE_INFO_SYMBOL_INDEX] != "-")
        lineData.strSymbol = strItems[GENE_INFO_SYMBOL_INDEX];
    else
        lineData.strSymbol = "n/a";

    // read gene description
    if (strItems[GENE_INFO_DESCRIPTION_INDEX] != "-")
        lineData.strDescription = strItems[GENE_INFO_DESCRIPTION_INDEX];
    else
        lineData.strDescription = "n/a";

    return true;
}

void CGeneFileWriter::
        x_GeneInfo_LineToRecord(const SGeneInfoLine& lineData,
                                TTwoIntRecordVec& vecRecords)
{
    if (lineData.nTaxId > 0 && lineData.geneId > 0)
    {
        STwoIntRecord record;
        record.n1 = lineData.geneId;
        record.n2 = m_nCurrentOffset;
        vecRecords.push_back(record);

        m_mapIdToOffset.insert(make_pair(record.n1, record.n2));

        string strOrgname;
        x_GetOrgnameForTaxId(lineData.nTaxId, strOrgname);

        int nPubMedLinks =
            x_GetNumPubMedLinksForGeneId(lineData.geneId);

        CRef<CGeneInfo> info(
            new CGeneInfo(lineData.geneId,
                          lineData.strSymbol,
                          lineData.strDescription,
                          strOrgname,
                          nPubMedLinks));

        WriteGeneInfo(m_outAllData, info, m_nCurrentOffset);

        m_nGeneIds++;
    }
}

void CGeneFileWriter::CGeneInfoProcessor::
        Process(const string& strLine,
                TTwoIntRecordVec& vecRecords)
{
    SGeneInfoLine lineData;
    if (m_pThis->x_GeneInfo_ParseLine(strLine, lineData))
    {
        m_pThis->x_GeneInfo_LineToRecord(lineData, vecRecords);
    }
}

void CGeneFileWriter::x_GeneInfo_ProcessFile(bool bOverwrite)
{
    if (!bOverwrite &&
        CheckExistence(m_strGene2OffsetFile) &&
        CheckExistence(m_strAllGeneDataFile))
    {
        return; // files exist, do not overwrite
    }

    // open the original and processed files

    CNcbiIfstream inGeneInfo;
    CNcbiOfstream outGene2Offset;

    if (!OpenTextInputFile(m_strGeneInfoFile, inGeneInfo))
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
                   "Cannot open Gene Info file for reading.");
    }
    if (!OpenBinaryOutputFile(m_strGene2OffsetFile, outGene2Offset))
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
                   "Cannot open Gene2Offset file for writing.");
    }
    if (!OpenBinaryOutputFile(m_strAllGeneDataFile, m_outAllData))
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
                   "Cannot open the Gene Data file for writing.");
    }

    // estimate the number of records we will have

    Int8 nTotalLenght = GetLength(m_strGeneInfoFile);
    TSeqPos nNumLinesEstimate = (TSeqPos)nTotalLenght / GENE_INFO_LINE_MIN;

    // create the array of (geneId, offset) records
    // and clear the corresponding map

    TTwoIntRecordVec vecRecords;
    vecRecords.reserve(nNumLinesEstimate);

    m_mapIdToOffset.clear();

    // parse each line and populate the records array
    // also, write the combined gene data to m_outAllData
    // and populate the m_mapIdToOffset map with records
    // (side effects within x_GeneInfo_LineToRecord)

    m_nCurrentOffset = 0;
    auto_ptr<CLineProcessor> proc(new CGeneInfoProcessor(this));
    x_ReadAndProcessFile
                (inGeneInfo,
                 proc.get(),
                 vecRecords,
                 GENE_INFO_LINE_MIN,
                 GENE_INFO_LINE_MAX);

    // sort the vector of records and output them to the file

    sort(vecRecords.begin(), vecRecords.end(),
         x_CompareTwoIntRecords);

    for (size_t iRec = 0; iRec < vecRecords.size(); iRec++)
    {
        WriteRecord(outGene2Offset, vecRecords[iRec]);   
    }
}

//==========================================================================//
// Gene->PubMed file processing

bool CGeneFileWriter::
        x_Gene2PM_ParseLine(const string& strLine,
                            SGene2PMLine& lineData)
{
    if (NStr::StartsWith(strLine, "#"))
        return false;

    vector<string> strItems;
    NStr::Tokenize(strLine, "\t", strItems);

    if (strItems.size() != GENE_2_PM_NUM_ITEMS)
    {
        NCBI_THROW(CGeneInfoException, eDataFormatError,
                   "Gene2PubMed file format not recognized.");
    }

    // read geneId
    if (strItems[GENE_2_PM_GENE_ID_INDEX] != "-")
        lineData.geneId =
            NStr::StringToInt(strItems[GENE_2_PM_GENE_ID_INDEX]);
    else
        lineData.geneId = 0;

    // read PMID
    if (strItems[GENE_2_PM_PMID_INDEX] != "-")
        lineData.nPMID =
            NStr::StringToInt(strItems[GENE_2_PM_PMID_INDEX]);
    else
        lineData.nPMID = 0;

    return true;
}

void CGeneFileWriter::
        x_Gene2PM_LineToRecord(const SGene2PMLine& lineData,
                               TTwoIntRecordVec& vecRecords)
{
    if (lineData.geneId > 0)
    {
        STwoIntRecord record;
        record.n1 = lineData.geneId;
        record.n2 = lineData.nPMID;
        vecRecords.push_back(record);
    }
}

void CGeneFileWriter::CGene2PMProcessor::
        Process(const string& strLine,
                TTwoIntRecordVec& vecRecords)
{
    SGene2PMLine lineData;
    if (m_pThis->x_Gene2PM_ParseLine(strLine, lineData))
    {
        m_pThis->x_Gene2PM_LineToRecord(lineData, vecRecords);
    }
}

void CGeneFileWriter::
        x_Gene2PM_ProcessFile()
{
    // open the original file

    CNcbiIfstream inGene2PM;
    if (!OpenTextInputFile(m_strGene2PubMedFile, inGene2PM))
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
                   "Cannot open Gene2PubMed file for reading.");
    }

    // estimate the number of records we will have

    Int8 nTotalLenght = GetLength(m_strGene2PubMedFile);
    TSeqPos nNumLinesEstimate = (TSeqPos)nTotalLenght / GENE_2_PM_LINE_MIN;

    // create the array of Gene Id to PMID lines

    TTwoIntRecordVec vecRecords;
    vecRecords.reserve(nNumLinesEstimate);

    // parse each line and populate the records array

    auto_ptr<CLineProcessor> proc(new CGene2PMProcessor(this));
    x_ReadAndProcessFile
                (inGene2PM,
                 proc.get(),
                 vecRecords,
                 GENE_2_PM_LINE_MIN,
                 GENE_2_PM_LINE_MAX);

    if (vecRecords.size() == 0)
        return;     // no PubMed data

    // sort the records

    sort(vecRecords.begin(), vecRecords.end(),
         x_CompareTwoIntRecords);

    // write the records to the geneId->PMIDs map

    m_mapIdToNumPMIDs.clear();

    int geneIdCur = vecRecords[0].n1;
    int nCountCur = 1;
    for (size_t iRec = 1; iRec < vecRecords.size(); iRec++)
    {
        if (vecRecords[iRec].n1 == geneIdCur)
        {
            nCountCur++;
        }
        else
        {
            m_mapIdToNumPMIDs.insert(make_pair(geneIdCur, nCountCur));
            geneIdCur = vecRecords[iRec].n1;
            nCountCur = 1;
        }
    }
    m_mapIdToNumPMIDs.insert(make_pair(geneIdCur, nCountCur));
}

//==========================================================================//
// Main interface

CGeneFileWriter::
    CGeneFileWriter(const string& strGene2AccessionFile,
                     const string& strGeneInfoFile,
                     const string& strGene2PubMedFile,
                     const string& strOutputDirPath)
: m_strGene2AccessionFile(strGene2AccessionFile),
  m_strGeneInfoFile(strGeneInfoFile),
  m_strGene2PubMedFile(strGene2PubMedFile)
{
    if (!CheckExistence(m_strGene2AccessionFile))
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
                   "Gene2Accession file not found.");
    }
    if (!CheckExistence(m_strGeneInfoFile))
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
                   "GeneInfo file not found.");
    }
    if (!CheckExistence(m_strGene2PubMedFile))
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
                   "Gene2PubMed file not found.");
    }

    m_strGi2GeneFile = strOutputDirPath +
                        GENE_GI2GENE_FILE_NAME;
    m_strGene2OffsetFile = strOutputDirPath +
                        GENE_GENE2OFFSET_FILE_NAME;
    m_strGi2OffsetFile = strOutputDirPath +
                        GENE_GI2OFFSET_FILE_NAME;
    m_strGene2GiFile = strOutputDirPath +
                        GENE_GENE2GI_FILE_NAME;
    m_strAllGeneDataFile = strOutputDirPath +
                        GENE_ALL_GENE_DATA_FILE_NAME;
    m_strInfoFile = strOutputDirPath +
                        GENE_GENERAL_INFO_FILE_NAME;

    m_bAllowMultipleIds_RNAGis = false;
    m_bAllowMultipleIds_ProtGis = false;
    m_bAllowMultipleIds_GenomicGis = false;

    if (!OpenTextOutputFile(m_strInfoFile, m_outInfo))
    {
        NCBI_THROW(CGeneInfoException, eFileNotFoundError,
                   "Cannot open the info/stats text file for writing.");
    }

    m_nTotalGis = 0;
    m_nRNAGis = 0;
    m_nProtGis = 0;
    m_nGenomicGis = 0;
    m_nGeneIds = 0;
}

CGeneFileWriter::~CGeneFileWriter()
{}

void CGeneFileWriter::
        EnableMultipleGeneIdsForRNAGis(bool bEnable)
{
    m_bAllowMultipleIds_RNAGis = bEnable;

    if (bEnable)
        m_outInfo << "Multiple GeneID's for RNA Gi's are enabled."
                  << endl;
}

void CGeneFileWriter::
        EnableMultipleGeneIdsForProteinGis(bool bEnable)
{
    m_bAllowMultipleIds_ProtGis = bEnable;

    if (bEnable)
        m_outInfo << "Multiple GeneID's for Protein Gi's are enabled."
                  << endl;
}

void CGeneFileWriter::
        EnableMultipleGeneIdsForGenomicGis(bool bEnable)
{
    m_bAllowMultipleIds_GenomicGis = bEnable;

    if (bEnable)
        m_outInfo << "Multiple GeneID's for Genomic Gi's are enabled."
                  << endl;
}

void CGeneFileWriter::ProcessFiles(bool bOverwrite)
{
    x_Gene2PM_ProcessFile();
    x_GeneInfo_ProcessFile(bOverwrite);
    x_Gene2Accn_ProcessFile(bOverwrite);

    m_outInfo << "\nTotal number of GeneID's accepted: "
              << m_nGeneIds << endl;
    m_outInfo << "Total number of Gi's processed: "
              << m_nTotalGis << endl;
    m_outInfo << "\nGi types encountered:" << endl;
    m_outInfo << "\tRNA - " << m_nRNAGis << endl;
    m_outInfo << "\tProtein - " << m_nProtGis << endl;
    m_outInfo << "\tGenomic - " << m_nGenomicGis << endl;
}

//==========================================================================//

END_NCBI_SCOPE
