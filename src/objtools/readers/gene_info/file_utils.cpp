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

/// @file file_utils.cpp
/// Implementation of Gene info file processing routines.

//==========================================================================//

#include <ncbi_pch.hpp>
#include <objtools/readers/gene_info/file_utils.hpp>

BEGIN_NCBI_SCOPE

//==========================================================================//
// Constants

/// Minimum number of characters on a valid Gene Data line.
static const unsigned int k_nGeneAllDataLineMin = 10;

/// Maximum number of characters on a valid Gene Data line.
static const unsigned int k_nGeneAllDataLineMax = 15000;

/// Number of items on a valid Gene Data line.
static const unsigned int k_nGeneAllDataNumItems = 5;

//==========================================================================//
// General file processing routines

bool CGeneFileUtils::CheckDirExistence(const string& strDir)
{
    CDir dir(strDir);
    return dir.Exists();
}

bool CGeneFileUtils::CheckExistence(const string& strFile)
{
    CFile file(strFile);
    return file.Exists();
}

int CGeneFileUtils::GetLength(const string& strFile)
{
    CFile file(strFile);
    if (!file.Exists())
        return -1;
    return file.GetLength();
}


bool CGeneFileUtils::OpenTextInputFile(const string& strFileName,
                                       CNcbiIfstream& in)
{
    if (!CheckExistence(strFileName))
        return false;

    if (in.is_open())
        in.close();

    in.open(strFileName.c_str(), IOS_BASE::in);
    return in.is_open(); 
}

bool CGeneFileUtils::OpenBinaryInputFile(const string& strFileName,
                                         CNcbiIfstream& in)
{
    if (!CheckExistence(strFileName))
        return false;

    if (in.is_open())
        in.close();

    in.open(strFileName.c_str(), IOS_BASE::in | IOS_BASE::binary);
    return in.is_open(); 
}

bool CGeneFileUtils::OpenTextOutputFile(const string& strFileName,
                                        CNcbiOfstream& out)
{
    if (out.is_open())
        out.close();

    out.open(strFileName.c_str(),
             IOS_BASE::out | IOS_BASE::trunc);
    return out.is_open(); 
}

bool CGeneFileUtils::OpenBinaryOutputFile(const string& strFileName,
                                          CNcbiOfstream& out)
{
    if (out.is_open())
        out.close();

    out.open(strFileName.c_str(),
             IOS_BASE::out | IOS_BASE::trunc | IOS_BASE::binary);
    return out.is_open(); 
}

void CGeneFileUtils::WriteGeneInfo(CNcbiOfstream& out,
                                   CRef<CGeneInfo> info,
                                   int& nCurrentOffset)
{
    string strAllData = NStr::IntToString(info->GetGeneId()) + "\t";
    strAllData += info->GetSymbol() + "\t";
    strAllData += info->GetDescription() + "\t";
    strAllData += info->GetOrganismName() + "\t";
    strAllData += NStr::IntToString(info->GetNumPubMedLinks()) + "\n";
    out << strAllData;

    nCurrentOffset += strAllData.length();
}

void CGeneFileUtils::ReadGeneInfo(CNcbiIfstream& in,
                                  int nOffset,
                                  CRef<CGeneInfo>& info)
{
    in.seekg(nOffset, IOS_BASE::beg);
    if (!in)
    {
        NCBI_THROW(CGeneInfoException, eDataFormatError,
            "Cannot read gene data at the offset: " +
            NStr::IntToString(nOffset));
    }

    int nBufSize = k_nGeneAllDataLineMax;
    char* pBuf = new char[nBufSize + 1];
    in.getline(pBuf, nBufSize);
    string strBuf = string(pBuf);

    if (strBuf.length() < k_nGeneAllDataLineMin)
    {
        NCBI_THROW(CGeneInfoException, eDataFormatError,
            "Gene data line appears to be too short: " + strBuf);
    }

    vector<string> strItems;
    NStr::Tokenize(strBuf, "\t", strItems);

    if (strItems.size() != k_nGeneAllDataNumItems)
    {
        NCBI_THROW(CGeneInfoException, eDataFormatError,
            "Unexpected number of entries on a gene data line: " + strBuf);
    }

    int nGeneId = NStr::StringToInt(strItems[0]);
    string strName = strItems[1];
    string strDescription = strItems[2];
    string strOrgname = strItems[3];
    int nPubMedLinks = NStr::StringToInt(strItems[4]);

    info.Reset(new CGeneInfo(nGeneId,
                             strName,
                             strDescription,
                             strOrgname,
                             nPubMedLinks));
}

//==========================================================================//

END_NCBI_SCOPE
