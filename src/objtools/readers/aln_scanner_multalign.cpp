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
#include "aln_peek_ahead.hpp"
#include "aln_scanner_multalign.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

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
        // error: short file
    }

    if (NStr::StartsWith(line, "//")) {
        gotLine = iStr.ReadLine(line, lineCount);
        if (!gotLine) {
            // error: short file
        }
    }
    if (!line.empty()) {
        // error: unexpected
    }

    enum EExpecting {
        OFFSETS,
        DATA
    };
    EExpecting expecting = EExpecting::OFFSETS;
    bool inFirstBlock = true;
    int expectedDataSize = 0;
    int expectedNumSequences = 0;
    int lineInBlock = 0;
    while (iStr.ReadLine(line, lineCount)) {
        NStr::TruncateSpacesInPlace(line);

        if (expecting == EExpecting::OFFSETS) {
            if (line.empty()) {
                continue;
            }
            xGetExpectedDataSize(line, expectedDataSize);
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
                        // error: not enough sequence data
                    }
                }
                expecting = EExpecting::OFFSETS;
                continue;
            }    
            if (!inFirstBlock  &&  lineInBlock == expectedNumSequences) {
                // error: too much sequence data
            }
            string seqId, seqData;
            xGetSeqIdAndData(line, seqId, seqData);
            if (expectedDataSize == 0) {
                expectedDataSize = seqData.size();
            }
            else {
                if (seqData.size() != expectedDataSize) {
                    // error: data size off
                }
            }
            if (inFirstBlock) {
                mSeqIds.push_back(seqId);
                mSequences.push_back(vector<TLineInfo>({{seqData, lineCount}}));
            }
            else {
                if (seqId != mSeqIds[lineInBlock]) {
                    // error: unexpected ID
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
    sequenceInfo.SetMiddleGap('.').SetBeginningGap('.').SetEndGap('.');
    sequenceInfo.SetMatch(0).SetMissing(0);
}

//  ----------------------------------------------------------------------------
void
CAlnScannerMultAlign::xVerifySingleSequenceData(
    const CSequenceInfo& sequenceInfo,
    const string& seqId,
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
                seqId);
        }
    }
}

//  ----------------------------------------------------------------------------
void
CAlnScannerMultAlign::xGetExpectedDataSize(
    const string& line,
    int& dataSize)
//  ----------------------------------------------------------------------------
{
    vector<string> tokens;
    NStr::Split(line, " ", tokens, NStr::fSplit_MergeDelimiters);
    if (tokens.size() != 1  &&  tokens.size() != 2) {
        // error: bad offset line
    }
    int startOffset(0), endOffset(-1);
    try {
        startOffset = NStr::StringToInt(tokens[0]);
        if (tokens.size() == 2) {
            endOffset = NStr::StringToInt(tokens[1]);
        }
    }
    catch (std::exception&) {
        // error: bad offset values
    }
    dataSize = (tokens.size() == 2 ? (endOffset - startOffset + 1) : 0);
}

//  ----------------------------------------------------------------------------
void
CAlnScannerMultAlign::xGetSeqIdAndData(
    const string& line,
    string& seqId,
    string& seqData)
//  ----------------------------------------------------------------------------
{
    vector<string> tokens;
    NStr::Split(line, " ", tokens, NStr::fSplit_MergeDelimiters);
    if (tokens.size() < 2) {
        // error: bad offset line
    }
    seqId = tokens[0];
    seqData = NStr::Join(tokens.begin() + 1, tokens.end(), "");
}

END_SCOPE(objects)
END_NCBI_SCOPE
