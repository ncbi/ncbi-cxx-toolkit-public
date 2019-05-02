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
#include "5col_line_reader.hpp"
#include "5col_import_data.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
C5ColLineReader::C5ColLineReader(
    CImportMessageHandler& errorReporter):
//  ============================================================================
    CFeatLineReader(errorReporter),
    mCurrentSeqId(""),
    mLastTypeSeen(eLineTypeNone),
    mCurrentOffset(0)
{
}

//  ============================================================================
bool
C5ColLineReader::GetNextRecord(
    CStreamLineReader& lineReader,
    CFeatImportData& record)
//  ============================================================================
{
    CImportError errorDataLineOutOfOrder(
        CImportError::ERROR, 
        "Data line out of order");
    CImportError errorBadDataLine(
        CImportError::ERROR, 
        "Bad data line", LineCount());

    xReportProgress();

    string nextLine = "";
    while (!lineReader.AtEOF()) {
        nextLine = *(++lineReader);
        ++mLineCount;
        NStr::TruncateSpacesInPlace(nextLine, NStr::eTrunc_End);
        if (xIgnoreLine(nextLine)) {
            continue;
        }
        vector<string> columns;
        xSplitLine(nextLine, columns);

        switch (xLineTypeOf(columns)) {
        default:
            mLastTypeSeen = eLineTypeNone;
            errorBadDataLine.SetLineNumber(LineCount());
            mErrorReporter.ReportError(errorBadDataLine);
            continue;

        case eLineTypeOffset: {
            string head, tail;
            NStr::SplitInTwo(columns[0], "=", head, tail);
            if (tail.empty()) {
                mErrorReporter.ReportError(errorBadDataLine);
                continue;
            }
            try {
                mCurrentOffset = NStr::StringToInt(
                    tail, 
                    NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSymbols);
            }
            catch(CException&) {
                mErrorReporter.ReportError(errorBadDataLine);
            }
            continue;
        }
            
        case eLineTypeSeqId:
            mLastTypeSeen = eLineTypeSeqId;
            if (mCollectedLines.empty()) {
                mCurrentSeqId = columns[1];
                continue;
            }
            xInitializeRecord(mCollectedLines, record);
            ++mRecordCount;
            mCollectedLines.clear();
            mCurrentSeqId = columns[1];
            return true;

        case eLineTypeIntervalAndType:
            if (mLastTypeSeen == eLineTypeIntervalAndType  ||
                    mLastTypeSeen == eLineTypeBareInterval) {
                errorDataLineOutOfOrder.SetLineNumber(LineCount());
                throw errorDataLineOutOfOrder;
            }
            mLastTypeSeen = eLineTypeIntervalAndType;

            if (mCollectedLines.empty()) {
                mCollectedLines.push_back(nextLine);
                continue;
            }
            xInitializeRecord(mCollectedLines, record);
            ++mRecordCount;
            mCollectedLines.clear();
            mCollectedLines.push_back(nextLine);
            return true;

        case eLineTypeBareInterval:
            if (mLastTypeSeen != eLineTypeIntervalAndType) {
                errorDataLineOutOfOrder.SetLineNumber(LineCount());
                throw errorDataLineOutOfOrder;
            }
            mLastTypeSeen = eLineTypeIntervalAndType;
            mCollectedLines.push_back(nextLine);
            continue;

        case eLineTypeAttribute:
            if (mLastTypeSeen == eLineTypeSeqId  ||
                    mLastTypeSeen == eLineTypeNone) {
                errorDataLineOutOfOrder.SetLineNumber(LineCount());
                throw errorDataLineOutOfOrder;
            }
            mLastTypeSeen = eLineTypeAttribute;
            mCollectedLines.push_back(nextLine);
            continue;
        }
    }
    return false;
}

//  ============================================================================
void
C5ColLineReader::xSplitLine(
    const string& line_,
    vector<string>& columns)
//  ============================================================================
{
    CImportError errorBadDataLine(
        CImportError::ERROR, 
        "Bad data line", LineCount());

    string line = NStr::TruncateSpaces(line_, NStr::eTrunc_End);
    NStr::Split(line, "\t", columns);
    if (columns.size() == 1) {
        if (!NStr::StartsWith(columns[0], ">Feature")) {
            throw errorBadDataLine;
        }
        string head, tail;
        NStr::SplitInTwo(columns[0], " ", head, tail);
        columns[0] = head;
        columns.push_back(tail);
        return;
    }
    if (columns[0] == ">Feature") {
        auto seqId = NStr::Join(columns.begin() + 1, columns.end(), " ");
        auto lead = columns[0];
        columns.erase(columns.begin() + 1, columns.end());
        columns.push_back(seqId);
    }
}
    
//  ============================================================================
C5ColLineReader::ELineType
C5ColLineReader::xLineTypeOf(
    const vector<string>& columns)
//  ============================================================================
{
    CImportError errorBadDataLine(
        CImportError::ERROR, 
        "Bad data line", LineCount());

    if (columns.empty()) {
        throw errorBadDataLine;
    }
    if (NStr::StartsWith(columns[0], "[offset")) {
        return eLineTypeOffset;
    } 
    switch(columns.size()) {
    case 2:
        if (columns[0] == ">Feature") {
            return eLineTypeSeqId;
        }
        return eLineTypeBareInterval;
    case 3:
        return eLineTypeIntervalAndType;
    case 5:
        if (columns[0].empty()  &&  columns[1].empty()  &&  columns[2].empty()) {
            return eLineTypeAttribute;
        }
        break;
    }
    throw errorBadDataLine;
}


//  ============================================================================
void
C5ColLineReader::xInitializeRecord(
    const vector<string>& lines,
    CFeatImportData& record_)
    //  ============================================================================
{
    CImportError errorBadIntervalBoundaries(
        CImportError::ERROR, 
        "Invalid interval boundaries",
        LineCount());

    assert(dynamic_cast<C5ColImportData*>(&record_));
    C5ColImportData& record = static_cast<C5ColImportData&>(record_);
    //record.InitializeFrom(columns, mLineNumber);

    string seqId;
    string featType;
    vector<pair<int, int>> vecIntervals;
    vector<pair<bool, bool>> vecPartials;
    vector<pair<string, string>> vecAttributes;

    seqId = mCurrentSeqId;
    
    vector<string> columns;
    NStr::Split(lines[0], "\t", columns);
    featType = columns[2];
    
    int i = 0;
    for (/**/; !NStr::StartsWith(lines[i], "\t"); ++i) {
        columns.clear();
        int intervalFrom(0), intervalTo(0);
        bool partialFrom(false), partialTo(false);
        NStr::Split(lines[i], "\t", columns);
        auto fromStr = columns[0];
        if (fromStr[0] == '<'  ||  fromStr[0] == '>') {
            fromStr = fromStr.substr(1, string::npos);
            partialFrom = true;
        }
        auto toStr = columns[1];
        if (toStr[0] == '<'  || toStr[0] == '>') {
            toStr = toStr.substr(1, string::npos);
            partialTo = true;
        }
        try {
            intervalFrom = NStr::StringToInt(fromStr);
            intervalTo = NStr::StringToInt(toStr);
        }
        catch(CException&) {
            throw errorBadIntervalBoundaries;
        }
        vecIntervals.push_back(pair<int, int>(intervalFrom, intervalTo));
        vecPartials.push_back(pair<bool, bool>(partialFrom, partialTo));
    }
    for (/**/; i < lines.size(); ++i) {
        columns.clear();
        NStr::Split(lines[i], "\t", columns);
        vecAttributes.push_back(pair<string, string>(columns[3], columns[4]));
    }

    if (mCurrentOffset) {
        for (auto interval: vecIntervals) {
            interval.first += mCurrentOffset;
            interval.second += mCurrentOffset;
        }
    }
    record.Initialize(seqId, featType, vecIntervals, vecPartials, vecAttributes);
}
