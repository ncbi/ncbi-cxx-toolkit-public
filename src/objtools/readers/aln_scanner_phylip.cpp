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
#include "aln_errors.hpp"
#include "aln_util.hpp"
#include "aln_peek_ahead.hpp"
#include "aln_scanner_phylip.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ============================================================================
//  Phylip info:
//  The first line contains the number of sequences and the common sequence
//    length of the sequences in the file.
//  The rest of the file is made up block containing lines of sequence data,
//    which are separated by empty lines.
//  The first block data lines starts with a seqId, followed by (optionally
//    chunked) sequence data.
//  Later blocks do not repeat the seqId and only contain (optionally chunked)
//    sequence data.
//  "Strict" phylip allocates exactly 10 characters for the seqId (padded with
//    spaces if necessary).
//
//  Gap characters are '-' and 'O', but '.' should be tolerated for historical
//    reasons.
//  There is no match character.
//  Nucleotide alphabet includes all ambiguity characters as well as 'U'.
//  Missing characters are 'X', '?', 'N'.
//
//  Reference: evolution.genetics.washington.edu/phylip/doc/sequence.html
//  ============================================================================

//  ----------------------------------------------------------------------------
void
CAlnScannerPhylip::xImportAlignmentData(
    CSequenceInfo& sequenceInfo,
    CLineInput& iStr)
//  ----------------------------------------------------------------------------
{
    string line;
    int lineCount(0);
    if (!iStr.ReadLine(line, lineCount)) {
        //error
    }

    vector<string> tokens;
    NStr::TruncateSpacesInPlace(line);
    NStr::Split(line,  " \t", tokens, NStr::fSplit_MergeDelimiters);
    mSequenceCount =  NStr::StringToInt(tokens[0]);
    mSequenceLength = NStr::StringToInt(tokens[1]);


    size_t dataLineCount(0);
    size_t blockLineLength(0);

    while (iStr.ReadLine(line, lineCount)) {
        NStr::TruncateSpacesInPlace(line);
        if (line.empty()) {
            continue;
        }
        // record potential defline
        if (line[0] == '>' || line[0] == '[') {
            string dummy, defLine;
            try {
                AlnUtil::ProcessDefline(line, dummy, defLine);
            }
            catch (SShowStopper& showStopper) {
                showStopper.mLineNumber = lineCount;
                throw;
            }
            if (!dummy.empty()) {
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDefinitionLine,
                    "Invalid Phylip definition line, expected \">\" followed by mods.");
            }
            mDeflines.push_back({defLine, lineCount});
            continue;
        }

        string lowerLine(line);
        NStr::ToLower(lowerLine);

        if (lowerLine == "begin ncbi;" ||
            lowerLine == "begin ncbi" ||
            lowerLine == "sequin" ||
            lowerLine == "end;" ||
            lowerLine == "end") {
            continue;
        }

        bool newBlock = ((dataLineCount % mSequenceCount) == 0);

        string seqData;
        if (dataLineCount < mSequenceCount) { // in first block
            string seqId;
            NStr::SplitInTwo(line, " \t", seqId, seqData, NStr::fSplit_MergeDelimiters);
            if (seqData.empty()) {
                string description =
                "Data line does not follow the expected pattern of sequence_ID followed by sequence data. Each data line in the first block should conform to this pattern.";
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataLine,
                    description);
            }

            TLineInfo existingInfo;
            auto idComparison =
                xGetExistingSeqIdInfo(seqId, existingInfo);
            if (idComparison != ESeqIdComparison::eDifferentChars) {
                string description;
                if (idComparison == ESeqIdComparison::eIdentical) {
                    description = ErrorPrintf(
                    "Duplicate ID: \"%s\" has already appeared at line %d.",
                    seqId.c_str(), existingInfo.mNumLine);
                }
                else { // ESeqIdComparison::eDifferByCase
                    description = ErrorPrintf(
                        "Conflicting IDs: \"%s\" differs only in case from \"%s\" at line %d.",
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
            seqData = line;
        }
        NStr::TruncateSpacesInPlace(seqData);
        vector<string> dataChunks;
        NStr::Split(seqData, " \t", dataChunks, NStr::fSplit_MergeDelimiters);
        seqData = NStr::Join(dataChunks, "");

        auto currentLineLength = seqData.size();
        if (newBlock) {
            blockLineLength = currentLineLength;
        }
        else
        if (currentLineLength != blockLineLength) {
            string description =
                BadCharCountPrintf(blockLineLength,currentLineLength);
            throw SShowStopper(
                lineCount,
                EAlnSubcode::eAlnSubcode_BadDataCount,
                description);
        }

        mSequences[dataLineCount % mSequenceCount].push_back({seqData, lineCount});
        ++dataLineCount;
    }

    int incompleteBlockSize = dataLineCount % mSequenceCount;
    if (incompleteBlockSize) {
        string description =
            ErrorPrintf("The final sequence block in the Phylip file is incomplete. It contains data for just %d sequences, but %d sequences are expected.",
            incompleteBlockSize,
            mSequenceCount);
        throw SShowStopper(
                -1,
                EAlnSubcode::eAlnSubcode_BadSequenceCount,
                description);
    }
}


//  ----------------------------------------------------------------------------
void
CAlnScannerPhylip::xVerifyAlignmentData(
    const CSequenceInfo& sequenceInfo)
//  ----------------------------------------------------------------------------
{
    if (mSequenceCount != mSeqIds.size()) {
        auto description = ErrorPrintf(
            "Phylip sequence count from first line (%d) does not agree with "
            "the actual sequence count (%d).",
            mSequenceCount, mSeqIds.size());
        throw SShowStopper(
            -1,
            EAlnSubcode::eAlnSubcode_BadSequenceCount,
            description);
    }
    auto actualSequenceLength = 0;
    for (auto sequenceData: mSequences[0]) {
        actualSequenceLength += sequenceData.mData.size();
    }
    if (mSequenceLength != actualSequenceLength) {
        auto description = ErrorPrintf(
            "Phylip sequence length from first line (%d) does not agree with "
            "the actual sequence length (%d).",
            mSequenceLength, actualSequenceLength);
        throw SShowStopper(
            -1,
            EAlnSubcode::eAlnSubcode_BadDataCount,
            description);
    }
    return CAlnScanner::xVerifyAlignmentData(sequenceInfo);
}

END_SCOPE(objects)
END_NCBI_SCOPE
