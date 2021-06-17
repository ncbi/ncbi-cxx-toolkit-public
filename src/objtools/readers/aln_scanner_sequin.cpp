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
#include "aln_scanner_sequin.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ============================================================================
//  Sequin info:
//  Nucleotide alphabet includes all ambiguity characters as well as 'U'.
//  Makes use of the match character and it is '.'.
//  Gap character is '-'.
//
//  Reference: Colleen Bollin
//  ============================================================================

//  ----------------------------------------------------------------------------
void
CAlnScannerSequin::xImportAlignmentData(
    CSequenceInfo& sequenceInfo,
    CLineInput& iStr)
//  ----------------------------------------------------------------------------
{
    string line;
    int lineCount(0);

    bool processingData = true;
    bool inFirstBlock = false;
    int lineInBlock = 0;
    string refSeqData;

    while (iStr.ReadLine(line, lineCount)) {

        if (line.empty()) {
            if (processingData) {
                if (lineInBlock != mSeqIds.size()) {
                    string description = ErrorPrintf(
                        "Missing data line. Expected sequence data for seqID \"%s\"",
                        mSeqIds[lineInBlock].mData.c_str());
                    throw SShowStopper(
                        lineCount,
                        EAlnSubcode::eAlnSubcode_MissingDataLine,
                        description);
                }
                processingData = false;
                if (mSeqIds.size() != 0) {
                    inFirstBlock = false;
                }
            }
            continue;
        }

        NStr::TruncateSpacesInPlace(line);

        if (xIsSequinOffsetsLine(line)) {
            if (processingData) {
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_MissingDataLine,
                    "Missing data line. Expected sequence data, not sequence offsets");
            }
            continue;
        }

        if (xIsSequinTerminationLine(line)) {
            if (processingData) {
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_MissingDataLine,
                    "Missing data line. Expected sequence data, not termination line");
            }
            break;
        }

        //process sequence data
        if (!processingData) {
            processingData = true;
            lineInBlock = 0;
            refSeqData.clear();
            if (mSeqIds.empty()) {
                inFirstBlock = true;
            }
        }
        string seqId, seqData;
        if (!xExtractSequinSequenceData(line, seqId, seqData)) {
            throw SShowStopper(
                lineCount,
                EAlnSubcode::eAlnSubcode_IllegalDataLine,
                "Malformatted data line");
        }

        // verify and process sequence ID:
        if (inFirstBlock) {
            TLineInfo existingInfo;

            auto idComparison =
                xGetExistingSeqIdInfo(seqId, existingInfo);
            if (idComparison != ESeqIdComparison::eDifferentChars) {
                string description;
                if (idComparison == ESeqIdComparison::eIdentical) {
                description = ErrorPrintf(
                    "Duplicate sequence ID \"%s\", first seen on line %d",
                    seqId.c_str(), existingInfo.mNumLine);
                }
                else { // ESeqIdComparison::eDifferByCase
                    description = ErrorPrintf(
                        "Conflicting IDs: \"%s\" differs only in case from \"%s\" on line %d.",
                        seqId.c_str(), existingInfo.mData.c_str(), existingInfo.mNumLine);
                }
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_UnexpectedSeqId,
                    description);
            }
            mSeqIds.push_back({seqId, lineCount});
            mSequences.push_back(vector<TLineInfo>());
        }
        else {
            if (lineInBlock >= mSeqIds.size()) {
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataLine,
                    "Extraneous data line in interleaved data block");
            }
            if (!xSeqIdIsEqualToInfoAt(seqId, lineInBlock)) {
                string description = ErrorPrintf(
                    "Unexpected sequence ID \"%s\"", seqId.c_str());
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_UnexpectedSeqId,
                    description);
            }
        }

        // verify and process sequence data:
        if (refSeqData.empty()) {
            refSeqData = seqData;
        }
        else {
            if (seqData.size() != refSeqData.size()) {
                string description = ErrorPrintf(
                    "Unexpected number of sequence symbols (expected %d but found %d)",
                    refSeqData.size(), seqData.size());
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_BadDataCount,
                    description);
            }
            for (auto i = 0; i < seqData.size(); ++i) {
                if (seqData[i] == '.') {
                    seqData[i] = refSeqData[i];
                }
            }
        }

        mSequences[lineInBlock].push_back({seqData, lineCount});
        ++lineInBlock;
    }
}

//  ----------------------------------------------------------------------------
void
CAlnScannerSequin::xAdjustSequenceInfo(
    CSequenceInfo& sequenceInfo)
//  ----------------------------------------------------------------------------
{
    sequenceInfo
        .SetBeginningGap("-").SetMiddleGap("-").SetEndGap("-")
        .SetMatch(".");
}

//  ----------------------------------------------------------------------------
bool
CAlnScannerSequin::xIsSequinOffsetsLine(
    const string& line)
//  ----------------------------------------------------------------------------
{
    vector<string> tokens;
    NStr::Split(line, " ", tokens, NStr::fSplit_MergeDelimiters);
    if (tokens.size() > 5) {
        return false;
    }
    for (auto token: tokens) {
        if (!NStr::EndsWith(token, '0')) {
            return false;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool
CAlnScannerSequin::xIsSequinTerminationLine(
    const string& line)
//  ----------------------------------------------------------------------------
{
    return (line == "//");
}

//  ----------------------------------------------------------------------------
bool
CAlnScannerSequin::xExtractSequinSequenceData(
    const string& line,
    string& seqId,
    string& seqData)
    //  ----------------------------------------------------------------------------
{
    vector<string> tokens;
    NStr::Split(line, " ", tokens, NStr::fSplit_MergeDelimiters);
    if (tokens.size() < 2) {
        // error: need at least ID and one block of sequence data
        return false;
    }

    seqId = tokens[0];

    if (tokens[1] == ">") {
        if (tokens.size() < 5) {
            // error: need at least ID and one block of sequence data and bounds
            return false;
        }
        for (auto curBlock = 3; curBlock < tokens.size() - 1; ++curBlock) {
            seqData += tokens[curBlock];
        }
    }
    else {
        for (auto curBlock = 1; curBlock < tokens.size(); ++curBlock) {
            seqData += tokens[curBlock];
        }
    }
    return true;
}


END_SCOPE(objects)
END_NCBI_SCOPE
