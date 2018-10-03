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
 * Author: Frank Ludwig
 *
 * File Description:  Iterate through file names matching a given glob pattern
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>

#include <objtools/import/feat_import_error.hpp>
#include "bed_line_reader.hpp"
#include "bed_import_data.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
CBedLineReader::CBedLineReader(
    CNcbiIstream& istr,
    CFeatMessageHandler& errorReporter):
//  ============================================================================
    CFeatLineReader(istr, errorReporter),
    mColumnDelimiter(""),
    mSplitFlags(0)
{
}

//  ============================================================================
bool
CBedLineReader::GetNextRecord(
    CFeatImportData& record)
//  ============================================================================
{
    CFeatImportError errorEofNoData(
        CFeatImportError::CRITICAL, 
        "Stream does not contain feature data", 
        0, CFeatImportError::eEOF_NO_DATA);

    xReportProgress();

    string nextLine = "";
    while (!AtEOF()) {
        nextLine = *(++(*this));
        ++mLineNumber;
        if (xIgnoreLine(nextLine)) {
            continue;
        }
        if (xProcessTrackLine(nextLine)) {
            continue;
        } 
        vector<string> columns;
        xSplitLine(nextLine, columns);
        xInitializeRecord(columns, record);
        ++mRecordNumber;
        return true;
    }
    if (0 == mRecordNumber) {
        errorEofNoData.SetLineNumber(mLineNumber);
        throw errorEofNoData;
    }
    return false;
}

//  ============================================================================
bool
CBedLineReader::xProcessTrackLine(
    const string& line)
//  ============================================================================
{
    if (!NStr::StartsWith(line, "track")) {
        return false;
    }
    return true;
}

//  ============================================================================
void
CBedLineReader::xSplitLine(
    const string& line,
    vector<string>& columns)
//  ============================================================================
{
    CFeatImportError errorInvalidColumnCount(
        CFeatImportError::CRITICAL, "Invalid column count");

    columns.clear();
    if (mColumnDelimiter.empty()) {
        mColumnDelimiter = "\t";
        mSplitFlags = 0;
        NStr::Split(line, mColumnDelimiter, columns, mSplitFlags);
        if (columns.size() == 12) {
            return;
        }
        columns.clear();
        mColumnDelimiter = " \t";
        mSplitFlags = NStr::fSplit_MergeDelimiters;
    }
    NStr::Split(line, mColumnDelimiter, columns, mSplitFlags);
    if (columns.size() == 12) {
        return;
    }
    throw errorInvalidColumnCount;
}

//  ============================================================================
void
CBedLineReader::xInitializeChromInterval(
    const vector<string>& columns,
    string& chromId,
    TSeqPos& chromStart,
    TSeqPos& chromEnd,
    ENa_strand& chromStrand)
//  ============================================================================
{
    CFeatImportError errorInvalidChromStartValue(
        CFeatImportError::ERROR, "Invalid chromStart value",
        mLineNumber);
    CFeatImportError errorInvalidChromEndValue(
        CFeatImportError::ERROR, "Invalid chromEnd value",
        mLineNumber);
    CFeatImportError errorInvalidStrandValue(
        CFeatImportError::ERROR, "Invalid strand value",
        mLineNumber);

    chromId = columns[0];

    try {
        chromStart = NStr::StringToInt(columns[1]);
    }
    catch(CException&) {
        throw errorInvalidChromStartValue;
    }

    try {
        chromEnd = NStr::StringToInt(columns[2]);
    }
    catch(CException&) {
        throw errorInvalidChromEndValue;
    }

    chromStrand = eNa_strand_plus;
    if (columns.size() > 5  &&  columns[5] == "-") {
        chromStrand = eNa_strand_minus;
    }
}

//  ============================================================================
void
CBedLineReader::xInitializeScore(
    const vector<string>& columns,
    double& score)
//  ============================================================================
{
    CFeatImportError errorInvalidScoreValue(
        CFeatImportError::WARNING, "Invalid score value- omitting from output.",
        mLineNumber);

    if (columns.size() < 5  ||  columns[4] == ".") {
        score = -1.0;
        return;
    }
    try {
        score = NStr::StringToDouble(columns[4]);
    }
    catch(CException&) {
        mErrorReporter.ReportError(errorInvalidScoreValue);
        score = -1.0;
        return;
    }
}

//  ============================================================================
void
CBedLineReader::xInitializeRgb(
    const vector<string>& columns,
    CBedImportData::RgbValue& rgbValue)
//  ============================================================================
{
    CFeatImportError errorInvalidRgbValue(
        CFeatImportError::WARNING, "Invalid RGB value- defaulting to BLACK",
        mLineNumber);

    rgbValue.R = rgbValue.G = rgbValue.B = 0;
    if (columns.size() < 9  ||  columns[8] == ".") {
        return;
    }

    string rgb = columns[8];
    try {
        vector<string > rgbSplits;
        NStr::Split(rgb, ",", rgbSplits);
        switch(rgbSplits.size()) {
        default:
            throw errorInvalidRgbValue;
        case 1: {
            unsigned long rgbInt = 0;
            if (NStr::StartsWith(rgb, "0x")) {
                auto hexDigits = rgb.substr(2, string::npos);
                rgbInt = NStr::StringToULong(hexDigits, 0, 16);
            }
            else if (NStr::StartsWith(rgb, "#")) {
                auto hexDigits = rgb.substr(1, string::npos);
                rgbInt = NStr::StringToULong(hexDigits, 0, 16);
            }
            else {
                rgbInt = NStr::StringToULong(rgbSplits[0]);
            }
            rgbInt &= 0xffffff;
            rgbValue.R = (rgbInt & 0xff0000) >> 16;
            rgbValue.G = (rgbInt & 0x00ff00) >> 8;
            rgbValue.B = (rgbInt & 0x0000ff);
            break;
        }
        case 3: {
            rgbValue.R = NStr::StringToInt(rgbSplits[0]);
            rgbValue.G = NStr::StringToInt(rgbSplits[1]);
            rgbValue.B = NStr::StringToInt(rgbSplits[2]);
            break;
        }
        }
    }
    catch(CException&) {
        rgbValue.R = rgbValue.G = rgbValue.B = 0;
        mErrorReporter.ReportError(errorInvalidRgbValue);
        return;
    }
    if (rgbValue.R < 0  ||  255 < rgbValue.R) {
        rgbValue.R = rgbValue.G = rgbValue.B = 0;
        mErrorReporter.ReportError(errorInvalidRgbValue);
        return;
    }
    if (rgbValue.G < 0  ||  255 < rgbValue.G) {
        rgbValue.R = rgbValue.G = rgbValue.B = 0;
        mErrorReporter.ReportError(errorInvalidRgbValue);
        return;
    }
    if (rgbValue.B < 0  ||  255 < rgbValue.B) {
        rgbValue.R = rgbValue.G = rgbValue.B = 0;
        mErrorReporter.ReportError(errorInvalidRgbValue);
        return;
    }
}

//  ============================================================================
void
CBedLineReader::xInitializeThickInterval(
    const vector<string>& columns,
    TSeqPos& thickStart,
    TSeqPos& thickEnd)
//  ============================================================================
{
    CFeatImportError errorInvalidThickStartValue(
        CFeatImportError::ERROR, "Invalid thickStart value",
        mLineNumber);
    CFeatImportError errorInvalidThickEndValue(
        CFeatImportError::ERROR, "Invalid thickEnd value",
        mLineNumber);

    if (columns.size() < 8) {
        return;
    }

    try {
        thickStart = NStr::StringToInt(columns[6]);
    }
    catch(CException&) {
        throw errorInvalidThickStartValue;
    }

    try {
        thickEnd = NStr::StringToInt(columns[7]);
    }
    catch(CException&) {
        throw errorInvalidThickEndValue;
    }
}

//  ============================================================================
void
CBedLineReader::xInitializeChromName(
    const vector<string>& columns,
    string& chromName)
//  ============================================================================
{
    if (columns.size() < 4) {
        chromName.clear();
        return;
    }
    chromName = columns[3];
}

//  ============================================================================
void
CBedLineReader::xInitializeBlocks(
    const vector<string>& columns,
    unsigned int& blockCount,
    vector<int>& blockStarts,
    vector<int>& blockSizes)
//  ============================================================================
{
    CFeatImportError errorInvalidBlockCountValue(
        CFeatImportError::ERROR, "Invalid blockCount value",
        mLineNumber);
    CFeatImportError errorInvalidBlockStartsValue(
        CFeatImportError::ERROR, "Invalid blockStarts value",
        mLineNumber);
    CFeatImportError errorInvalidBlockSizesValue(
        CFeatImportError::ERROR, "Invalid blockStarts value",
        mLineNumber);
    CFeatImportError errorInconsistentBlocksInformation(
        CFeatImportError::ERROR, "Inconsistent blocks information",
        mLineNumber);

    if (columns.size() < 12) {
        blockCount = 1;
        return;
    }
    try {
        blockCount = NStr::StringToInt(columns[9]);
    }
    catch(std::exception&) {
        throw errorInvalidBlockCountValue;
    }

    try {
        vector<string> blockStartsSplits;
        NStr::Split(columns[10], ",", blockStartsSplits);
        if (blockStartsSplits.back().empty()) {
            blockStartsSplits.pop_back();
        }
        for (auto blockStart: blockStartsSplits) {
            blockStarts.push_back(NStr::StringToInt(blockStart));
        }
    }
    catch(std::exception&) {
        throw errorInvalidBlockCountValue;
    }
    if (blockCount != blockStarts.size()) {
        throw errorInconsistentBlocksInformation;
    }

    try {
        vector<string> blockSizesSplits;
        NStr::Split(columns[11], ",", blockSizesSplits);
        if (blockSizesSplits.back().empty()) {
            blockSizesSplits.pop_back();
        }
        for (auto blockSize: blockSizesSplits) {
            blockSizes.push_back(NStr::StringToInt(blockSize));
        }
    }
    catch(std::exception&) {
        throw errorInvalidBlockCountValue;
    }
    if (blockCount != blockSizes.size()) {
        throw errorInconsistentBlocksInformation;
    }
}

//  ============================================================================
void
CBedLineReader::xInitializeRecord(
    const vector<string>& columns,
    CFeatImportData& record_)
//  ============================================================================
{
    assert(dynamic_cast<CBedImportData*>(&record_));
    CBedImportData& record = static_cast<CBedImportData&>(record_);
    //record.InitializeFrom(columns, mLineNumber);

    string chromId;
    TSeqPos chromStart, chromEnd;
    ENa_strand chromStrand;
    xInitializeChromInterval(columns, chromId, chromStart, chromEnd, chromStrand);

    string name;
    xInitializeChromName(columns, name);

    double score;
    xInitializeScore(columns, score);

    TSeqPos thickStart = chromStart, thickEnd = chromStart; //!!
    xInitializeThickInterval(columns, thickStart, thickEnd);

    CBedImportData::RgbValue rgbValue;
    xInitializeRgb(columns, rgbValue);

    unsigned int blockCount;
    vector<int> blockStarts, blockSizes;
    xInitializeBlocks(columns, blockCount, blockStarts, blockSizes);

    record.Initialize(chromId, chromStart, chromEnd, name, score, chromStrand, 
        thickStart, thickEnd, rgbValue, blockCount, blockStarts, blockSizes); 
}
