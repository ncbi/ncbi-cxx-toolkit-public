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

#include <objtools/import/import_error.hpp>
#include "bed_line_reader.hpp"
#include "bed_import_data.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ****************************************************************************
//  Implementation notes:
//  - This implementation follows the UCSC version of BED as described at
//    https://genome.ucsc.edu/FAQ/FAQformat.html#format1.
//  - Per spec, arbitrary whitespace (i.e. any combination of tabs and spaces)
//    is permissible as column separators.
//  - Per spec, there can only be a single instance of browser/track lines,
//    right at the beginning of the file. There is no provision for multiple
//    tracks in a single file.
//  ****************************************************************************

//  ============================================================================
CBedLineReader::CBedLineReader(
    CImportMessageHandler& errorReporter):
//  ============================================================================
    CFeatLineReader(errorReporter),
    mColumnCount(0),
    mColumnDelimiter(" \t"),
    mSplitFlags(NStr::fSplit_MergeDelimiters),
    mUseScore(false),
    mItemRgb(false),
    mColorByStrand(false)
{
}

//  ============================================================================
bool
CBedLineReader::GetNextRecord(
    CStreamLineReader& lineReader,
    CFeatImportData& record)
//
//  Error disposition:
//  Abort on:
//    Bad column count.
//  Terminate on:
//    Track line out of order.
//  Discard record on:
//    Bad topology (block description off, text in numeric fields)
//  Fix up data with default values on:
//    Bad score values, bad RGB values.
//  ============================================================================
{
    xReportProgress();

    string nextLine = "";
    while (!lineReader.AtEOF()) {
        nextLine = *(++lineReader);
        ++mLineCount;
        if (xIgnoreLine(nextLine)) {
            continue;
        }
        if (xProcessTrackLine(nextLine)) {
            continue;
        }
        vector<string> columns;
        xSplitLine(nextLine, columns);
        xInitializeRecord(columns, record);
        ++mRecordCount;
        return true;
    }
    return false;
}

//  ============================================================================
void
CBedLineReader::SetInputStream(
    CNcbiIstream& istr,
    bool force)
//  ============================================================================
{
    CFeatLineReader::SetInputStream(istr, force);
}

//  ============================================================================
bool
CBedLineReader::xIgnoreLine(
    const string& line) const
//  ============================================================================
{
    if (CFeatLineReader::xIgnoreLine(line)) {
        return true;
    }
    return NStr::StartsWith(line, "browser");
}

//  ============================================================================
bool
CBedLineReader::xProcessTrackLine(
    const string& line)
//  ============================================================================
{
    CImportError errorInvalidTrackValue(
        CImportError::WARNING, "Invalid track value", 
        LineCount());

    CImportError errorTrackLineOutOfOrder(
        CImportError::CRITICAL, "Track line out of order",
        LineCount());

    string track, values;
    NStr::SplitInTwo(line, " \t", track, values);
    if (track != "track") {
        return false;
    }
    if (this->mRecordCount > 0) { //track line not before any data
        throw errorTrackLineOutOfOrder;
    }

    mAnnotInfo.Clear();
    if (values.empty()) {
        return true;
    }

    vector<string> trackPieces;
    NStr::Split(values, "=", trackPieces);
    string key, value;
    key = NStr::TruncateSpaces_Unsafe(trackPieces[0]);
    for (int i = 1; i < trackPieces.size() - 1; ++i) {
        vector<string> words;
        NStr::Split(trackPieces[i], " \t", words);
        auto pendingKey = words.back();
        words.pop_back();
        value = NStr::Join(words, " ");
        mAnnotInfo.SetValue(key, NStr::Replace(value, "\"", ""));
        key = pendingKey;
    }
    mAnnotInfo.SetValue(key, NStr::Replace(trackPieces.back(), "\"", ""));

    //cache some often accessed values:
    const string& useScore = mAnnotInfo.ValueOf("useScore");
    mUseScore = (!useScore.empty()  &&  useScore != "0"  &&  useScore != "false");

    const string& itemRgb = mAnnotInfo.ValueOf("itemRgb");
    mItemRgb = (itemRgb == "on"  ||  itemRgb == "On");
    if (!itemRgb.empty()  &&  !mItemRgb) {
        errorInvalidTrackValue.AmendMessage("Bad itemRgb value --- ignored");
        mErrorReporter.ReportError(errorInvalidTrackValue);
    }

    const string& colorByStrand = mAnnotInfo.ValueOf("colorByStrand");
    if (!colorByStrand.empty()) {
        try {
            string colorStrandPlus, colorStrandMinus;
            NStr::SplitInTwo(
                colorByStrand, " ", colorStrandPlus, colorStrandMinus);
            vector<string> rgbComponents;
            NStr::Split(colorStrandPlus, ",", rgbComponents);
            if (rgbComponents.size() != 3) {
                throw std::exception();
            }
            mRgbStrandPlus.R = NStr::StringToInt(rgbComponents[0]);
            mRgbStrandPlus.G = NStr::StringToInt(rgbComponents[1]);
            mRgbStrandPlus.B = NStr::StringToInt(rgbComponents[2]);
            rgbComponents.clear();
            NStr::Split(colorStrandMinus, ",", rgbComponents);
            if (rgbComponents.size() != 3) {
                throw std::exception();
            }
            mRgbStrandMinus.R = NStr::StringToInt(rgbComponents[0]);
            mRgbStrandMinus.G = NStr::StringToInt(rgbComponents[1]);
            mRgbStrandMinus.B = NStr::StringToInt(rgbComponents[2]);
        }
        catch(std::exception&) {
            errorInvalidTrackValue.AmendMessage(
                "Bad colorByStrand value --- ignored");
            mErrorReporter.ReportError(errorInvalidTrackValue);
        }
        mColorByStrand = true;
    }
    return true;
}

//  ============================================================================
void
CBedLineReader::xSplitLine(
    const string& line,
    vector<string>& columns)
//
//  See implementation notes!
//  ============================================================================
{
    CImportError errorInvalidColumnCount(
        CImportError::FATAL, "Invalid column count",
        LineCount());

    columns.clear();
    NStr::Split(line, mColumnDelimiter, columns, mSplitFlags);
	if (mColumnCount == 0) {
		if (columns.size() < 3  ||  columns.size() > 12) {
			throw errorInvalidColumnCount;
		}
        mColumnCount = columns.size();
        return;
	}
    if (columns.size() != mColumnCount) {
        throw errorInvalidColumnCount;
    }
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
    CImportError errorInvalidChromStartValue(
        CImportError::ERROR, "Invalid chromStart value",
        LineCount());
    CImportError errorInvalidChromEndValue(
        CImportError::ERROR, "Invalid chromEnd value",
        LineCount());
    CImportError errorInvalidStrandValue(
        CImportError::ERROR, "Invalid strand value",
        LineCount());

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
    if (columns.size() > 5) {
        const auto& strand = columns[5];
        if (strand != "+"  &&  strand != "-"  &&  strand != ".") {
            throw errorInvalidStrandValue;
        }
        if (strand == "-") {
            chromStrand = eNa_strand_minus;
        }
    }
}

//  ============================================================================
void
CBedLineReader::xInitializeScore(
    const vector<string>& columns,
    double& score)
//  ============================================================================
{
    CImportError errorInvalidScoreValue(
        CImportError::WARNING, "Invalid score value- omitting from output.",
        LineCount());

    if (columns.size() < 5  ||  columns[4] == "."  ||  mUseScore) {
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
    if (mUseScore) {
        xInitializeRgbFromScoreColumn(columns, rgbValue);
        return;
    }
    if (mItemRgb) {
        xInitializeRgbFromRgbColumn(columns, rgbValue);
        return;
    }
    if (mColorByStrand) {
        xInitializeRgbFromStrandColumn(columns, rgbValue);
        return;
    }
    rgbValue.R = rgbValue.B = rgbValue.G = -1;
}

//  ============================================================================
void
CBedLineReader::xInitializeRgbFromStrandColumn(
    const vector<string>& columns,
    CBedImportData::RgbValue& rgbValue)
//  ============================================================================
{
    CImportError errorInvalidStrandValue(
        CImportError::WARNING, 
        "Invalid strand value- setting color to BLACK.",
        LineCount());

    if (columns.size() < 6  || 
            (columns[5] != "+"  &&  columns[5] != "-"  &&  columns[5] != ".")) {
        mErrorReporter.ReportError(errorInvalidStrandValue);
        rgbValue.R = rgbValue.G = rgbValue.B = 0;
        return;
    }
    if (columns[5] == "-") {
        rgbValue.R = mRgbStrandMinus.R;
        rgbValue.B = mRgbStrandMinus.B;
        rgbValue.G = mRgbStrandMinus.G;
    }
    else {
        rgbValue.R = mRgbStrandPlus.R;
        rgbValue.B = mRgbStrandPlus.B;
        rgbValue.G = mRgbStrandPlus.G;
    }
}

//  ============================================================================
void
CBedLineReader::xInitializeRgbFromScoreColumn(
    const vector<string>& columns,
    CBedImportData::RgbValue& rgbValue)
//  ============================================================================
{
    CImportError errorInvalidScoreValue(
        CImportError::WARNING, "Invalid score value- setting color to BLACK.",
        LineCount());
    CImportError errorScoreValueTooLow(
        CImportError::WARNING, "Invalid score value- clipping to 0.",
        LineCount());
    CImportError errorScoreValueTooHigh(
        CImportError::WARNING, "Invalid score value- clipping to 1000.",
        LineCount());

    if (columns.size() < 5 || columns[4] == ".") {
        mErrorReporter.ReportError(errorInvalidScoreValue);
        rgbValue.R = rgbValue.G = rgbValue.B = 0;
        return;
    }
    auto scoreValue = 0;
    try {
        scoreValue = static_cast<int>(NStr::StringToDouble(columns[4]));
    }
    catch(CException&) {
        mErrorReporter.ReportError(errorInvalidScoreValue);
        rgbValue.R = rgbValue.G = rgbValue.B = 0;
        return;
    }

    if (scoreValue < 0) {
        mErrorReporter.ReportError(errorScoreValueTooLow);
        scoreValue = 0;
    }
    else if (scoreValue > 1000) {
        mErrorReporter.ReportError(errorScoreValueTooHigh);
        scoreValue = 1000;
    }
    if (scoreValue == 0) {
        rgbValue.R = rgbValue.G = rgbValue.B = 0;
        return;
    }
    if (scoreValue > 998) {
        rgbValue.R = rgbValue.G = rgbValue.B = 255;
        return;
    }

    scoreValue = static_cast<int>(scoreValue / 111);
    rgbValue.R = rgbValue.G = rgbValue.B = (13 + 29*scoreValue);
    return;
}

//  ============================================================================
void
CBedLineReader::xInitializeRgbFromRgbColumn(
    const vector<string>& columns,
    CBedImportData::RgbValue& rgbValue)
//  ============================================================================
{
    CImportError errorInvalidRgbValue(
        CImportError::WARNING, "Invalid RGB value- defaulting to BLACK",
        LineCount());

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
    CImportError errorInvalidThickStartValue(
        CImportError::ERROR, "Invalid thickStart value",
        LineCount());
    CImportError errorInvalidThickEndValue(
        CImportError::ERROR, "Invalid thickEnd value",
        LineCount());
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
    CImportError errorInvalidBlockCountValue(
        CImportError::ERROR, "Invalid blockCount value",
        LineCount());
    CImportError errorInvalidBlockStartsValue(
        CImportError::ERROR, "Invalid blockStarts value",
        LineCount());
    CImportError errorInvalidBlockSizesValue(
        CImportError::ERROR, "Invalid blockSizes value",
        LineCount());
    CImportError errorInconsistentBlocksInformation(
        CImportError::ERROR, "Inconsistent blocks information",
        LineCount());

    if (columns.size() < 12) {
        blockCount = 0;
        return;
    }
    try {
        blockCount = NStr::StringToInt(columns[9]);
    }
    catch(std::exception&) {
        throw errorInvalidBlockCountValue;
    }

    blockStarts.clear();
    blockSizes.clear();
    try {
        vector<string> blockSizesSplits;
        NStr::Split(columns[10], ",", blockSizesSplits);
        if (blockSizesSplits.back().empty()) {
            blockSizesSplits.pop_back();
        }
        for (auto blockSize: blockSizesSplits) {
            blockSizes.push_back(NStr::StringToInt(blockSize));
        }
    }
    catch(std::exception&) {
        throw errorInvalidBlockSizesValue;
    }
    if (blockCount != blockSizes.size()) {
        throw errorInconsistentBlocksInformation;
    }

    try {
        vector<string> blockStartsSplits;
        NStr::Split(columns[11], ",", blockStartsSplits);
        if (blockStartsSplits.back().empty()) {
            blockStartsSplits.pop_back();
        }
        for (auto blockStart: blockStartsSplits) {
            blockStarts.push_back(NStr::StringToInt(blockStart));
        }
    }
    catch(std::exception&) {
        throw errorInvalidBlockStartsValue;
    }
    if (blockCount != blockStarts.size()) {
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
    CImportError errorInvalidThickInterval(
        CImportError::ERROR, "thickInterval extending beyond chrom feature",
        LineCount());

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
    if (thickStart < chromStart  ||  thickEnd > chromEnd) {
        throw errorInvalidThickInterval;
    }

    CBedImportData::RgbValue rgbValue;
    xInitializeRgb(columns, rgbValue);

    unsigned int blockCount;
    vector<int> blockStarts, blockSizes;
    xInitializeBlocks(columns, blockCount, blockStarts, blockSizes);

    record.Initialize(chromId, chromStart, chromEnd, name, score, chromStrand, 
        thickStart, thickEnd, rgbValue, blockCount, blockStarts, blockSizes); 
}

