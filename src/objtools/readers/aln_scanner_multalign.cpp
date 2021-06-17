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
 * Authors: Frank Ludwig
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/alnread.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include "aln_errors.hpp"
#include "aln_util.hpp"
#include "aln_peek_ahead.hpp"
#include "aln_scanner_multalign.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ============================================================================
//  Multi-align info:
//  It basically comes in two variants, scooted and unscooted.
//  Main differences are:
//    Unscooted has initial line containing only "//".
//    Unscooted is thereafter uniformly indented by 24 characters.
//    Unscooted contains a consensus line after every interleaved block.
//
//  Nucleotide alphabet includes all ambiguity characters as well as 'U'.
//  There is no match character.
//  Gap character is '.'.
//
//  Reference: Colleen Bollin
//  ============================================================================

//  ----------------------------------------------------------------------------
void
CAlnScannerMultAlign::xImportAlignmentData(
    CSequenceInfo& sequenceInfo,
    CLineInput& iStr)
//  ----------------------------------------------------------------------------
{
    string line;
    int lineCount(0);

    bool gotLine = iStr.ReadLine(line, lineCount);
    if (!gotLine) {
        throw SShowStopper(
            -1,
            EAlnSubcode::eAlnSubcode_FileTooShort,
            "Filedoes not contain data");
    }

    if (NStr::StartsWith(line, "//")) {
        gotLine = iStr.ReadLine(line, lineCount);
        if (!gotLine) {
            throw SShowStopper(
                lineCount,
                EAlnSubcode::eAlnSubcode_FileTooShort,
                "Filedoes not contain data");
        }
        if (!line.empty()) {
            throw SShowStopper(
                lineCount,
                EAlnSubcode::eAlnSubcode_FileTooShort,
                "Empty separator line expected");
        }
    }

    enum EExpecting {
        OFFSETS,
        DATA
    };
    EExpecting expecting = EExpecting::OFFSETS;
    bool inFirstBlock = true;
    size_t expectedDataSize = 0;
    size_t expectedNumSequences = 0;
    int lineInBlock = 0;
    while (iStr.ReadLine(line, lineCount)) {
        NStr::TruncateSpacesInPlace(line);

        if (expecting == EExpecting::OFFSETS) {
            if (line.empty()) {
                continue;
            }
            xGetExpectedDataSize(line, lineCount, expectedDataSize);
            expecting = EExpecting::DATA;
            lineInBlock = 0;
            continue;
        }

        if (expecting == EExpecting::DATA) {
            if (line.empty()  ||  NStr::StartsWith(line, "Consensus")) {
                // integrity check
                if (inFirstBlock) {
                    expectedNumSequences = mSeqIds.size();
                    inFirstBlock = false;
                }
                else {
                    if (lineInBlock != expectedNumSequences) {
                        throw SShowStopper(
                            lineCount,
                            EAlnSubcode::eAlnSubcode_MissingDataLine,
                            "Premature end of data block");
                    }
                }
                expecting = EExpecting::OFFSETS;
                continue;
            }
            if (!inFirstBlock  &&  lineInBlock == expectedNumSequences) {
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataLine,
                    "Extra data line found");
            }
            string seqId, seqData;
            try {
                AlnUtil::ProcessDataLine(line, seqId, seqData);
            }
            catch (SShowStopper& showStopper) {
                showStopper.mLineNumber = lineCount;
                throw;
            }
            if (expectedDataSize == 0) {
                expectedDataSize = seqData.size();
            }
            else {
                if (seqData.size() != expectedDataSize) {
                    throw SShowStopper(
                        lineCount,
                        EAlnSubcode::eAlnSubcode_BadDataCount,
                        "Too much or too little data in data line");
                }
            }
            if (inFirstBlock) {
                mSeqIds.push_back({seqId, lineCount});
                mSequences.push_back(vector<TLineInfo>({{seqData, lineCount}}));
            }
            else {
                if (!xSeqIdIsEqualToInfoAt(seqId, lineInBlock)) {
                    throw SShowStopper(
                        lineCount,
                        eAlnSubcode_UnexpectedSeqId,
                        "Data for unexpected sequence ID");
                }
                mSequences[lineInBlock].push_back({seqData, lineCount});
            }
            ++lineInBlock;
            continue;
        }
    }
}

//  ----------------------------------------------------------------------------
void
CAlnScannerMultAlign::xAdjustSequenceInfo(
    CSequenceInfo& sequenceInfo)
//  ----------------------------------------------------------------------------
{
    // set sequence info to multi-align conventions:
    sequenceInfo.SetMiddleGap(".").SetBeginningGap(".").SetEndGap(".");
    sequenceInfo.SetMatch("").SetMissing("");
}

//  ----------------------------------------------------------------------------
void
CAlnScannerMultAlign::xVerifySingleSequenceData(
    const CSequenceInfo& sequenceInfo,
    const TLineInfo& seqId,
    const vector<TLineInfo> lineInfos)
//  -----------------------------------------------------------------------------
{
    const char* errTempl("Bad character [%c] found at data position %d.");

    const string& alphabet = sequenceInfo.Alphabet();
    string legalAnywhere = alphabet + ".";

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
                seqId.mData);
        }
    }
}

//  ----------------------------------------------------------------------------
void
CAlnScannerMultAlign::xGetExpectedDataSize(
    const string& line,
    int lineNumber,
    size_t& dataSize)
//  ----------------------------------------------------------------------------
{
    vector<string> tokens;
    NStr::Split(line, " ", tokens, NStr::fSplit_MergeDelimiters);
    if (tokens.size() > 2) {
        throw SShowStopper(
            lineNumber,
            EAlnSubcode::eAlnSubcode_IllegalDataDescription,
            "Expected offsets line (at most two numbers separated by space");
    }
    int startOffset(0), endOffset(-1);
    try {
        startOffset = NStr::StringToInt(tokens[0]);
        if (tokens.size() == 2) {
            endOffset = NStr::StringToInt(tokens[1]);
        }
    }
    catch (std::exception&) {
        throw SShowStopper(
            lineNumber,
            EAlnSubcode::eAlnSubcode_IllegalDataDescription,
            "Expected offsets line (at most two numbers separated by space");
   }
    dataSize = (tokens.size() == 2 ? (endOffset - startOffset + 1) : 0);
}

END_SCOPE(objects)
END_NCBI_SCOPE
