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
#include "5col_line_reader.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
C5ColLineReader::C5ColLineReader(
    CNcbiIstream& istr,
    CFeatMessageHandler& errorReporter):
//  ============================================================================
    CFeatLineReader(istr, errorReporter),
    mCurrentSeqId(""),
    mLastTypeSeen(eLineTypeNone),
    mCurrentOffset("0")
{
}

//  ============================================================================
bool
C5ColLineReader::GetNextRecord(
    CFeatImportData& record)
//  ============================================================================
{
    CFeatImportError errorEofNoData(
        CFeatImportError::CRITICAL, 
        "Stream does not contain feature data", 
        0, CFeatImportError::eEOF_NO_DATA);
    CFeatImportError errorDataLineOutOfOrder(
        CFeatImportError::ERROR, 
        "Data line out of order");
    CFeatImportError errorBadDataLine(
        CFeatImportError::ERROR, 
        "Bad data line", mLineNumber);

    xReportProgress();

    string nextLine = "";
    while (!AtEOF()) {
        nextLine = *(++(*this));
        NStr::TruncateSpacesInPlace(nextLine, NStr::eTrunc_End);
        ++mLineNumber;
        if (xIgnoreLine(nextLine)) {
            continue;
        }
        vector<string> columns;
        xSplitLine(nextLine, columns);

        switch (xLineTypeOf(columns)) {
        default:
            mLastTypeSeen = eLineTypeNone;
            errorBadDataLine.SetLineNumber(mLineNumber);
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
                mCurrentOffset = NStr::IntToString(NStr::StringToInt(
                    tail, 
                    NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSymbols));
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
                mCollectedLines.push_back(mCurrentSeqId);
                mCollectedLines.push_back(mCurrentOffset);
                continue;
            }
            if (mCollectedLines.size() < 3) {
                errorDataLineOutOfOrder.SetLineNumber(mLineNumber);
                throw errorDataLineOutOfOrder;
            }
            record.InitializeFrom(mCollectedLines, mLineNumber);
            ++mRecordNumber;
            mCollectedLines.clear();
            mCurrentSeqId = columns[1];
            mCollectedLines.push_back(mCurrentSeqId);
            mCollectedLines.push_back(mCurrentOffset);
            return true;

        case eLineTypeIntervalAndType:
            if (mLastTypeSeen == eLineTypeIntervalAndType  ||
                    mLastTypeSeen == eLineTypeBareInterval) {
                errorDataLineOutOfOrder.SetLineNumber(mLineNumber);
                throw errorDataLineOutOfOrder;
            }
            mLastTypeSeen = eLineTypeIntervalAndType;

            if (mCollectedLines.size() < 3) {
                mCollectedLines.push_back(nextLine);
                continue;
            }
            record.InitializeFrom(mCollectedLines, mLineNumber);
            ++mRecordNumber;
            mCollectedLines.clear();
            mCollectedLines.push_back(mCurrentSeqId);
            mCollectedLines.push_back(mCurrentOffset);
            mCollectedLines.push_back(nextLine);
            return true;

        case eLineTypeBareInterval:
            if (mLastTypeSeen != eLineTypeIntervalAndType) {
                errorDataLineOutOfOrder.SetLineNumber(mLineNumber);
                throw errorDataLineOutOfOrder;
            }
            mLastTypeSeen = eLineTypeIntervalAndType;
            mCollectedLines.push_back(nextLine);
            continue;

        case eLineTypeAttribute:
            if (mLastTypeSeen == eLineTypeSeqId  ||
                    mLastTypeSeen == eLineTypeNone) {
                errorDataLineOutOfOrder.SetLineNumber(mLineNumber);
                throw errorDataLineOutOfOrder;
            }
            mLastTypeSeen = eLineTypeAttribute;
            mCollectedLines.push_back(nextLine);
            continue;
        }
    }
    if (0 == mRecordNumber) {
        errorEofNoData.SetLineNumber(mLineNumber);
        throw errorEofNoData;
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
    CFeatImportError errorBadDataLine(
        CFeatImportError::ERROR, 
        "Bad data line", mLineNumber);

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
    CFeatImportError errorBadDataLine(
        CFeatImportError::ERROR, 
        "Bad data line", mLineNumber);

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
