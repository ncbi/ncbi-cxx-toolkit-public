/*
 * $Id$
 *
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
 * Authors:  Colleen Bollin
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/alnread.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include "aln_errors.hpp"
#include "aln_peek_ahead.hpp"
#include "aln_scanner_nexus.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xImportAlignmentData(
    CSequenceInfo& sequenceInfo,
    CLineInput& iStr)
//  ----------------------------------------------------------------------------
{
    string line;
    int lineCount(0);

    int dataLineCount(0);
    int blockLineLength(0);
    int sequenceCharCount(0);
    int unmatchedLeftBracketCount(0);
    int commentStartLine(-1);

    while (iStr.ReadLine(line, lineCount)) {

        NStr::TruncateSpacesInPlace(line);
        if (line.empty()) {
            continue;
        }

        if (mState == EState::SKIPPING) {
            NStr::ToLower(line);
            if (NStr::StartsWith(line, "matrix")) {
                mState = EState::DATA;
                continue;
            }
            if (NStr::StartsWith(line, "dimensions")) {
                xProcessDimensionLine(line, lineCount);
                continue;
            }
            if (NStr::StartsWith(line, "format")) {
                xProcessFormatLine(line, lineCount);
                continue;
            }
            if (NStr::StartsWith(line, "sequin")) {
                mState = EState::DEFLINES;
                continue;
            }
            continue;
        }
        if (mState == EState::DEFLINES) {
            xProcessDefinitionLine(line, lineCount);
            continue;
        }
        if (mState == EState::DATA) {
            if (mNumSequences == 0  ||  mSequenceSize == 0) {
                //error: data before info necessary to interpret it
            }
            auto lineStrLower(line);
            auto previousBracketCount = unmatchedLeftBracketCount;
            sStripNexusComments(line, unmatchedLeftBracketCount);
            if (previousBracketCount == 0 &&
                unmatchedLeftBracketCount == 1) {
                commentStartLine = lineCount;
            }

            if (line.empty()) {
                continue;
            }
            NStr::ToLower(lineStrLower);

            if (NStr::StartsWith(lineStrLower, "end;")) {
                theErrorReporter->Warn(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataLine,
                    "Unexpected \"end;\". Appending \';\' to prior command");
                mState = EState::SKIPPING;
                continue;
            }

            bool isLastLineOfData = NStr::EndsWith(line, ";");
            if (isLastLineOfData) {
                line = line.substr(0, line.size() -1 );
                NStr::TruncateSpacesInPlace(line);
                if (line.empty()) {
                    mState = EState::SKIPPING;
                    continue;
                }
            }
            
            vector<string> tokens;
            NStr::Split(line, " \t", tokens, 0);
            if (tokens.size() < 2) {
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataLine,
                    "In data line, expected seqID followed by sequence data"); 
            }

            string seqId = tokens[0];
            if (dataLineCount < mNumSequences) {
                if (std::find(mSeqIds.begin(), mSeqIds.end(), seqId) != mSeqIds.end()) {
                    return; //ERROR: duplicate ID
                }
                mSeqIds.push_back(seqId);
                mSequences.push_back(vector<TLineInfo>());
            }
            else {
                if (mSeqIds[dataLineCount % mNumSequences] != seqId) {
                    string description;
                    if (std::find(mSeqIds.begin(), mSeqIds.end(), seqId) == mSeqIds.end()) {
                        description = ErrorPrintf(
                            "Expected %d sequences, but finding data for another.",
                            mNumSequences);
                    }
                    else {
                        description = ErrorPrintf(
                            "Finding data for sequence \"%s\" out of order.",
                            seqId.c_str());
                    }
                    throw SShowStopper(
                        lineCount,
                        EAlnSubcode::eAlnSubcode_BadSequenceCount,
                        description);
                }
            }

            string seqData = NStr::Join(tokens.begin()+1, tokens.end(), "");
            auto dataSize = seqData.size();
            auto dataIndex = dataLineCount % mNumSequences;
            if (dataIndex == 0) {
                sequenceCharCount += dataSize;
                if (sequenceCharCount > mSequenceSize) {
                    string description = ErrorPrintf(
                        "Expected %d symbols per sequence but finding already %d",
                        mSequenceSize,
                        sequenceCharCount);
                    throw SShowStopper(
                        lineCount,
                        EAlnSubcode::eAlnSubcode_BadDataCount,
                        description); 
                }
                blockLineLength = dataSize;
            }
            else {
                if (dataSize != blockLineLength) {
                    string description = ErrorPrintf(
                        "In data line, expected %d symbols but finding %d",
                        blockLineLength,
                        dataSize);
                    throw SShowStopper(
                        lineCount,
                        EAlnSubcode::eAlnSubcode_BadDataCount,
                        description); 
                }
            }

            mSequences[dataIndex].push_back({seqData, lineCount});

            dataLineCount += 1;
            if (isLastLineOfData) {
                mState = EState::SKIPPING;
            }
            continue;
        }
    }

    if (unmatchedLeftBracketCount>0) {
        string description = ErrorPrintf(
                "Unterminated comment beginning on line %d",
                commentStartLine);
        throw SShowStopper(
                commentStartLine,
                EAlnSubcode::eAlnSubcode_UnterminatedComment,
                description);
    }

    //submit collected data to a final sanity check:
    if (sequenceCharCount != mSequenceSize) {
        string description = ErrorPrintf(
            "Expected %d symbols per sequence but finding only %d",
            mSequenceSize,
            sequenceCharCount);
        throw SShowStopper(
            -1,
            EAlnSubcode::eAlnSubcode_BadDataCount,
            description); 
    }
}

//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xVerifySingleSequenceData(
    const CSequenceInfo& sequenceInfo,
    const string& seqId,
    const vector<TLineInfo> lineInfos)
//  -----------------------------------------------------------------------------
{
    const char* errTempl("Bad character [%c] found at data position %d.");

    const string& alphabet = sequenceInfo.Alphabet();
    string legalAnywhere = alphabet;
    if (!mGapChar.empty()) {
        legalAnywhere += mGapChar;
    }
    else {
        legalAnywhere += sequenceInfo.MiddleGap();
    }
    if (!mMatchChar.empty()) {
        legalAnywhere += mMatchChar;
    }
    else {
        legalAnywhere += sequenceInfo.Match();
    }
    if (!mMissingChar.empty()) {
        legalAnywhere += mMissingChar;
    }
    else {
        legalAnywhere += sequenceInfo.Missing();
    }

    for (auto lineInfo: lineInfos) {
        if (lineInfo.mData.empty()) {
            continue;
        }
        string seqData(lineInfo.mData);
        auto illegalChar = seqData.find_first_not_of(legalAnywhere);
        if (illegalChar != string::npos) {
            string description = ErrorPrintf(
                errTempl, seqData[illegalChar], illegalChar);
            throw SShowStopper(
                lineInfo.mNumLine,
                EAlnSubcode::eAlnSubcode_BadDataChars,
                description,
                seqId);
        }
    }
}

//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xProcessDimensionLine(
    const string& line,
    int lineCount)
//  ----------------------------------------------------------------------------
{
    const string prefixNtax("ntax=");
    const string prefixNchar("nchar=");
    list<string> tokens;
    NStr::Split(line, " \t;", tokens, NStr::fSplit_MergeDelimiters);
    tokens.pop_front();
    for (auto token: tokens) {
        if (NStr::StartsWith(token, prefixNtax)) {
            try {
                mNumSequences = NStr::StringToInt(token.substr(prefixNtax.size()));
            }
            catch(...) {
                string description = ErrorPrintf("Invalid nTax setting \"%s\"", 
                    token.substr(prefixNtax.size()).c_str());
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataDescription,
                    description);
            }
            continue;
        }
        if (NStr::StartsWith(token, prefixNchar)) {
            try {
                mSequenceSize = NStr::StringToInt(token.substr(prefixNchar.size()));
            }
            catch(...) {
                string description = ErrorPrintf("Invalid nChar setting \"%s\"", 
                    token.substr(prefixNchar.size()).c_str());
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataDescription,
                    description);
            }
            continue;
        }
    }
}

//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xProcessFormatLine(
    const string& line,
    int lineCount)
//  ----------------------------------------------------------------------------
{
    const string prefixMissing("missing=");
    const string prefixGap("gap=");
    const string prefixMatch("matchchar=");
    list<string> tokens;
    NStr::Split(line, " \t;", tokens, NStr::fSplit_MergeDelimiters);
    tokens.pop_front();
    for (auto token: tokens) {
        if (NStr::StartsWith(token, prefixMissing)) {
            mMissingChar = token.substr(prefixMissing.size());
            continue;
        }
        if (NStr::StartsWith(token, prefixGap)) {
            mGapChar = token.substr(prefixGap.size());
            continue;
        }
        if (NStr::StartsWith(token, prefixMatch)) {
            mMatchChar = token.substr(prefixMatch.size());
            continue;
        }
    }
}

//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xProcessDefinitionLine(
    const string& line,
    int lineCount)
//  ----------------------------------------------------------------------------
{
    if (NStr::StartsWith(line, ">")) {
        string defLine = line.substr(1);
        NStr::TruncateSpacesInPlace(defLine);
        mDeflines.push_back({defLine, lineCount});
        return;
    }
    string lineLowerCase(line);
    NStr::ToLower(lineLowerCase);
    if (NStr::StartsWith(lineLowerCase, "end")) {
        mState = EState::SKIPPING;
        return;
    }
    string description(
        "Illegal definition line in NEXUS sequin command");
    throw SShowStopper(
        lineCount,
        eAlnSubcode_IllegalDefinitionLine,
        description);
}

//  ----------------------------------------------------------------------------
void 
CAlnScannerNexus::sStripNexusComments(
    string& line, 
    int &numUnmatchedLeftBrackets)
//  ----------------------------------------------------------------------------
{
    if (NStr::IsBlank(line)) {
        return;
    }

    list<pair<int, int>> commentLimits;
    int index=0;
    int start=0;
    int stop;
    while (index < line.size()) {
        const auto& c = line[index];
        if (c == '[') {
            ++numUnmatchedLeftBrackets;
            if (numUnmatchedLeftBrackets==1) {
                start = index;
            }
        }
        else 
        if (c == ']') {
            if (numUnmatchedLeftBrackets==1) {
                stop = index;
                commentLimits.push_back(make_pair(start, stop));
                --numUnmatchedLeftBrackets;
            }
        }
        ++index;
    }

    if (numUnmatchedLeftBrackets) {
        commentLimits.push_back(make_pair(start, index-1));
    }

    for (auto it = commentLimits.crbegin();
         it != commentLimits.crend();
         ++it) {
        line.erase(it->first, (it->second-it->first)+1);
    }
    NStr::TruncateSpacesInPlace(line);
}


END_SCOPE(objects)
END_NCBI_SCOPE
