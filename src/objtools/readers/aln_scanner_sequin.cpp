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
#include "aln_data.hpp"
#include "aln_errors.hpp"
#include "aln_scanner_sequin.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);


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

//  ----------------------------------------------------------------------------
void
CAlnScannerSequin::sProcessAlignmentFile(
    TAlignRawFilePtr afrp)
    //  ----------------------------------------------------------------------------
{
    bool processingData = false;
    bool inFirstBlock = false;
    int lineInBlock = 0;
    string refSeqData;

    for (auto linePtr = afrp->line_list; linePtr; linePtr = linePtr->next) {

        if (!linePtr->data || linePtr->data[0] == 0) {
            if (processingData) {
                if (lineInBlock != mSeqIds.size()) {
                    string description = StrPrintf(
                        "Missing data line. Expected sequence data for seqID \"%s\"",
                        mSeqIds[lineInBlock].c_str());
                    throw SShowStopper(
                        linePtr->line_num,
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

        string line(linePtr->data);
        NStr::TruncateSpacesInPlace(line);

        if (xIsSequinOffsetsLine(line)) {
            if (processingData) {
                throw SShowStopper(
                    linePtr->line_num,
                    EAlnSubcode::eAlnSubcode_MissingDataLine,
                    "Missing data line. Expected sequence data, not sequence offsets");
            }
            continue;
        }

        if (xIsSequinTerminationLine(line)) {
            if (processingData) {
                throw SShowStopper(
                    linePtr->line_num,
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
                linePtr->line_num,
                EAlnSubcode::eAlnSubcode_IllegalDataLine,
                "Malformatted data line");
        }

        // verify and process sequence ID:
        if (inFirstBlock) {
            if (std::find(mSeqIds.begin(), mSeqIds.end(), seqId) != mSeqIds.end()) {
                // error: duplicate sequence ID
                string description = StrPrintf(
                    "Duplicate sequence ID \"%s\"", seqId.c_str());
                throw SShowStopper(
                    linePtr->line_num,
                    EAlnSubcode::eAlnSubcode_UnexpectedSeqId,
                    description);
            }
            mSeqIds.push_back(seqId);
            mSequences.push_back(vector<TLineInfoPtr>());
        }
        else {
            if (lineInBlock >= mSeqIds.size()) {
                throw SShowStopper(
                    linePtr->line_num,
                    EAlnSubcode::eAlnSubcode_IllegalDataLine,
                    "Extraneous data line in interleaved data block");
            }
            if (seqId != mSeqIds[lineInBlock]) {
                string description = StrPrintf(
                    "Unexpected sequence ID \"%s\"", seqId.c_str());
                throw SShowStopper(
                    linePtr->line_num,
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
                string description = StrPrintf(
                    "Unexpected number of sequence symbols (expected %d but found %d)",
                    refSeqData.size(), seqData.size());
                throw SShowStopper(
                    linePtr->line_num,
                    EAlnSubcode::eAlnSubcode_BadDataCount,
                    description);
            }
            for (auto i = 0; i < seqData.size(); ++i) {
                if (seqData[i] == '.') {
                    seqData[i] = refSeqData[i];
                }
            }
        }

        strcpy(linePtr->data, seqData.c_str());
        mSequences[lineInBlock].push_back(linePtr);
        ++lineInBlock;
    }
    for (auto idIndex = 0; idIndex < mSeqIds.size(); ++idIndex) {
        auto& curSeq = mSequences[idIndex];
        for (auto seqPart = 0; seqPart < curSeq.size(); ++seqPart) {
            auto liPtr = curSeq[seqPart];
            afrp->sequences = SAlignRawSeq::sAddSeqById(
                afrp->sequences, mSeqIds[idIndex],
                liPtr->data, liPtr->line_num, liPtr->line_num, 0);
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
