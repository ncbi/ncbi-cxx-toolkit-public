
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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write alignment
 *
 */

#include <ncbi_pch.hpp>

#include "psl_record.hpp"
#include "psl_formatter.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
static string
sDebugFormatValue(
    const string& label,
    const string& data)
//  ----------------------------------------------------------------------------
{
    string formattedValue = (label + string(12, ' ')).substr(0, 12) + ": ";
    formattedValue += data;
    formattedValue += "\n";
    return formattedValue;
}

//  ----------------------------------------------------------------------------
static void
sDebugChunkArray(
    const vector<int>& origArray,
    int chunkSize,
    vector<vector<int>>& chunks)
//  ----------------------------------------------------------------------------
{
    if (origArray.empty()) {
        return;
    }
    auto numElements = origArray.size();
    for (auto index=0; index < numElements; ++index) {
        if (0 == index % chunkSize) {
            chunks.push_back(vector<int>());
        }
        chunks.back().push_back(origArray[index]);
    }
}

//  ----------------------------------------------------------------------------
static string
sFormatInt(
    int value,
    int dflt)
//  ----------------------------------------------------------------------------
{
    if (value == dflt) {
        return ".";
    }
    return NStr::IntToString(value);
}

//  ----------------------------------------------------------------------------
CPslFormatter::CPslFormatter(
    CNcbiOstream& ostr,
    bool debugMode):
//  ----------------------------------------------------------------------------
    mOstr(ostr),
    mDebugMode(debugMode)
{
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldMatches(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = sFormatInt(record.GetMatches(), -1);
    if (mDebugMode) {
        return sDebugFormatValue("matches", rawString);
    }
    return rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldMisMatches(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = sFormatInt(record.GetMisMatches(), -1);
    if (mDebugMode) {
        return sDebugFormatValue("misMatches", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldRepMatches(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = sFormatInt(record.GetRepMatches(), -1);
    if (mDebugMode) {
        return sDebugFormatValue("repMatches", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldCountN(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = sFormatInt(record.GetCountN(), -1);
    if (mDebugMode) {
        return sDebugFormatValue("nCount", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldNumInsertQ(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = sFormatInt(record.GetNumInsertQ(), -1);
    if (mDebugMode) {
        return sDebugFormatValue("qNumInsert", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldBaseInsertQ(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = sFormatInt(record.GetBaseInsertQ(), -1);
    if (mDebugMode) {
        return sDebugFormatValue("qBaseInsert", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldNumInsertT(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = sFormatInt(record.GetNumInsertT(), -1);
    if (mDebugMode) {
        return sDebugFormatValue("tNumInsert", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldBaseInsertT(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = sFormatInt(record.GetBaseInsertT(), -1);
    if (mDebugMode) {
        return sDebugFormatValue("tBaseInsert", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldStrand(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    string rawString = ".";
    if (record.GetStrandT() != eNa_strand_unknown) {
        rawString = (record.GetStrandT() == eNa_strand_minus ? "-" : "+");
    }
    if (mDebugMode) {
        return sDebugFormatValue("strand", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldNameQ(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = record.GetNameQ();
    if (rawString.empty()) {
        rawString = ".";
    }
    if (mDebugMode) {
        return sDebugFormatValue("qName", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldSizeQ(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = sFormatInt(record.GetSizeQ(), -1);
     if (mDebugMode) {
        return sDebugFormatValue("qSize", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldStartQ(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = sFormatInt(record.GetStartQ(), -1);
    if (mDebugMode) {
        return sDebugFormatValue("qStart", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldEndQ(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = sFormatInt(record.GetEndQ(), -1);
    if (mDebugMode) {
        return sDebugFormatValue("qEnd", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldNameT(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = record.GetNameT();
    if (rawString.empty()) {
        rawString = ".";
    }
    if (mDebugMode) {
        return sDebugFormatValue("tName", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldSizeT(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = sFormatInt(record.GetSizeT(), -1);
    if (mDebugMode) {
        return sDebugFormatValue("tSize", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldStartT(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = sFormatInt(record.GetStartT(), -1);
    if (mDebugMode) {
       return sDebugFormatValue("tStart", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldEndT(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = sFormatInt(record.GetEndT(), -1);
    if (mDebugMode) {
        return sDebugFormatValue("tEnd", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldBlockCount(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto rawString = sFormatInt(record.GetBlockCount(), -1);
    if (mDebugMode) {
        return sDebugFormatValue("blockCount", rawString);
    }
    return "\t" + rawString;
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldBlockSizes(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto blockSizes = record.GetBlockSizes();
    if (mDebugMode) {
        if (blockSizes.empty()) {
            return sDebugFormatValue("blockSizes", ".");
        }
        vector<vector<int>> chunks;
        sDebugChunkArray(blockSizes, 5, chunks);
        bool labelWritten = false;
        string field;
        for (auto& chunk: chunks) {
            auto value = NStr::JoinNumeric(chunk.begin(), chunk.end(), ", ");
            if (!labelWritten) {
                field += sDebugFormatValue("blockSizes", value);
                labelWritten = true;
            }
            else {
                field += "              ";
                field += value;
                field += "\n";
            }
        }
        return field;
    }
    return "\t" + NStr::JoinNumeric(blockSizes.begin(), blockSizes.end(), ",");
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldStartsQ(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto blockStartsQ = record.GetBlockStartsQ();
    if (mDebugMode) {
        if (blockStartsQ.empty()) {
            return sDebugFormatValue("qStarts", ".");
        }
        vector<vector<int>> chunks;
        sDebugChunkArray(blockStartsQ, 5, chunks);
        bool labelWritten = false;
        string field;
        for (auto& chunk: chunks) {
            auto value = NStr::JoinNumeric(chunk.begin(), chunk.end(), ", ");
            if (!labelWritten) {
                field += sDebugFormatValue("qStarts", value);
                labelWritten = true;
            }
            else {
                field += "              ";
                field += value;
                field += "\n";
            }
        }
        return field;
    }
    return "\t" + NStr::JoinNumeric(blockStartsQ.begin(), blockStartsQ.end(), ",");
}

//  ----------------------------------------------------------------------------
string
CPslFormatter::xFieldStartsT(
    const CPslRecord& record) const
//  ----------------------------------------------------------------------------
{
    auto blockStartsT = record.GetBlockStartsT();
    if (mDebugMode) {
        if (blockStartsT.empty()) {
            return sDebugFormatValue("tStarts", ".");
        }
        vector<vector<int>> chunks;
        sDebugChunkArray(blockStartsT, 5, chunks);
        bool labelWritten = false;
        string field;
        for (auto& chunk: chunks) {
            auto value = NStr::JoinNumeric(chunk.begin(), chunk.end(), ", ");
            if (!labelWritten) {
                field += sDebugFormatValue("tStarts", value);
                labelWritten = true;
            }
            else {
                field += "              ";
                field += value;
                field += "\n";
            }
        }
        return field;
    }
    return "\t" + NStr::JoinNumeric(blockStartsT.begin(), blockStartsT.end(), ",");
}

//  ----------------------------------------------------------------------------
void
CPslFormatter::Format(
    const CPslRecord& record)
//  ----------------------------------------------------------------------------
{
    mOstr << xFieldMatches(record)
         << xFieldMisMatches(record)
         << xFieldRepMatches(record)
         << xFieldCountN(record)
         << xFieldNumInsertQ(record)
         << xFieldBaseInsertQ(record)
         << xFieldNumInsertT(record)
         << xFieldBaseInsertT(record)
         << xFieldStrand(record)
         << xFieldNameQ(record)
         << xFieldSizeQ(record)
         << xFieldStartQ(record)
         << xFieldEndQ(record)
         << xFieldNameT(record)
         << xFieldSizeT(record)
         << xFieldStartT(record)
         << xFieldEndT(record)
         << xFieldBlockCount(record)
         << xFieldBlockSizes(record)
         << xFieldStartsQ(record)
         << xFieldStartsT(record)
         << endl;
}

END_NCBI_SCOPE

