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
 * Authors: Justin Foley
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/alnread.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include "aln_peek_ahead.hpp"
#include "aln_errors.hpp"
#include "aln_scanner_clustal.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ============================================================================
//  Clustal/Clustalw info:
//  Clustal is loosely defined with pieces that may be there or not. Clustalw
//    gets rid of the ambiguities and sets firm rules as what the data should 
//    look like.
//  The following pertains to Clustalw:
//  It starts with a header line stating it's clustalw, followed by one or more
//    empties.
//  It contains one or more blocks, each holding the same number of data lines,
//    a conservation line, and at least one empty line.
//  Each data line consists of a seqId, sequence data in one continuous run up
//    to 60 characters long, and an -optional- trailing offset count.
//  
//  The gap character is '-'.
//  The alphabet can be anything, so for nucleotide sequences, we must accept
//    ambiguity characters as well.
//
//  Reference: tools.genouest.org/tools/meme/doc/clustalw-format.html
//  ============================================================================


//  ----------------------------------------------------------------------------
void
CAlnScannerClustal::xImportAlignmentData(
    CSequenceInfo& sequenceInfo,
    CLineInput& iStr)
//  ----------------------------------------------------------------------------
{
    bool inBlock = false;
    int  blockLineLength = 0;
    int  blockCount = 0;
    int  numSeqs = 0;
    int  seqCount = 0;

    string line;
    int lineCount(0);

    while (iStr.ReadLine(line, lineCount)) {
        if (lineCount == 1) {
            string toLower(line);
            NStr::ToLower(toLower);
            if (NStr::StartsWith(toLower, "clustal")) {
                continue;
            }
        }

        if (line.empty()) {
            continue;
        }
        NStr::TruncateSpacesInPlace(line);
        if (line.empty()) {
            if (inBlock) {
                if (blockCount == 1) {
                    numSeqs = seqCount;
                }
                sResetBlockInfo(
                    seqCount, blockLineLength, inBlock);
            }
            continue;
        }

        if (sIsConservationLine(line)) {
            if (!inBlock) {
                string description = "Clustal conservation characters (e.g. *.: characters) were detected in the alignment file, but are out of the expected order. Conservation characters, if included, must appear after sequence data lines.";
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataLine,
                    description); 
            }
            if (blockCount == 1) {
                numSeqs = seqCount;
            }
            sResetBlockInfo(seqCount, blockLineLength, inBlock);
            continue;
        }
        // Search for end of block
        vector<string> tokens;
        NStr::Split(line, " \t", tokens, NStr::fSplit_Tokenize);
        const auto num_tokens = tokens.size();
        if (num_tokens < 2 || num_tokens > 3) {
            throw SShowStopper(
                lineCount,
                EAlnSubcode::eAlnSubcode_IllegalDataLine,
                "Date line does not follow the expected pattern of sequence_ID followed by sequence data and (optionally) data count. Each data line should conform tyo the same expected pattern"); 
        }

        int seqLength = 0;
        if (num_tokens == 3) {
            seqLength = NStr::StringToInt(tokens[2], NStr::fConvErr_NoThrow);
            if (!seqLength) { 
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataLine,
                    "In data line, expected seqID followed by sequence data and (optionally) data count"); 
            }
        }

        if (!inBlock) {
            inBlock = true;
            ++blockCount;
        }

        if (!sProcessClustalDataLine(
             tokens, lineCount,
             seqCount,
             numSeqs, 
             seqLength,
             blockCount, 
             blockLineLength)) {
            return;
        }
        mSequences[seqCount].push_back({tokens[1], lineCount});
        ++seqCount;
    }
}

//  ----------------------------------------------------------------------------
bool 
CAlnScannerClustal::sIsConservationLine(
    const string& line)
//  ----------------------------------------------------------------------------
{
    for (const auto& c : line) {
        if (!isspace(c) &&
            c != ':' &&
            c != '.' &&
            c != '*') {
            return false;
        }
    }
    return true;
};

//  ----------------------------------------------------------------------------
bool
CAlnScannerClustal::sProcessClustalDataLine(
    const vector<string>& tokens,
    const int lineNum,
    const int seqCount,
    const int numSeqs,
    const int seqLength,
    const int blockCount,
    int& blockLineLength)
//  ----------------------------------------------------------------------------
{
    auto seqId = tokens[0];
    if (blockCount == 1) {
        TLineInfo existingInfo;
        auto idComparison = xGetExistingSeqIdInfo(seqId, existingInfo);
        if (idComparison != ESeqIdComparison::eDifferentChars) {
            string description;
            if (idComparison == ESeqIdComparison::eIdentical) { 
                description = ErrorPrintf(
                "Duplicate ID: \"%s\" has already appeared in this block, on line %d.", 
                seqId.c_str(), existingInfo.mNumLine);
            }
            else { // ESeqIdComparison::eDifferByCase
                description = ErrorPrintf(
                "Conflicting IDs: \"%s\" differs only in case from \"%s\", which has already appeared in this block, on line %d.", seqId.c_str(), existingInfo.mData.c_str(), existingInfo.mNumLine);
            }
            throw SShowStopper(
                lineNum,
                EAlnSubcode::eAlnSubcode_UnexpectedSeqId,
                description);
        }
        mSeqIds.push_back({seqId, lineNum});
        mSequences.push_back(vector<TLineInfo>());
    }
    else {
        if (seqCount >= numSeqs) {
            string description = ErrorPrintf(
                "Expected %d sequences, but finding data for for another.",
                numSeqs);
            throw SShowStopper(
                lineNum,
                EAlnSubcode::eAlnSubcode_BadSequenceCount,
                description);
        }

        if (seqId != mSeqIds[seqCount].mData) {
            string seqIdLower(seqId);
            NStr::ToLower(seqIdLower);

            string description;
            auto it = mSeqIds.begin();
            bool exactCopy = false;
            while (it != mSeqIds.end()) {
                if (it->mData == seqId) {
                    exactCopy = true;
                    break;
                }
                auto idLower(it->mData);
                NStr::ToLower(idLower);
                if (idLower == seqIdLower) {
                    break;
                }
                ++it;
            }

            if (it == mSeqIds.end()) {
                description = ErrorPrintf(
                    "Expected %d sequences, but finding data for for another.",
                    numSeqs);
                throw SShowStopper(
                    lineNum,
                    EAlnSubcode::eAlnSubcode_BadSequenceCount,
                    description);
            }
            
            auto idPos = distance(mSeqIds.begin(), it);
            if (idPos < seqCount) {
                description = ErrorPrintf(
                    "Duplicate ID: \"%s\" has already appeared in this block, on line %d.",
                    seqId.c_str(),
                    it->mNumLine);
            }
            else {
                description = ErrorPrintf(
                    "Finding data for sequence \"%s\" out of order.",
                    seqId.c_str());
            }
            throw SShowStopper(
                lineNum,
                EAlnSubcode::eAlnSubcode_UnexpectedSeqId,
                description);
        }
    }
    
    if (seqCount==0) {
        blockLineLength = tokens[1].size();
        return true;
    }

    auto currentLineLength = tokens[1].size();
    if (currentLineLength != blockLineLength) {
        string description = ErrorPrintf(
            "In data line, expected %d symbols but finding %d",
            blockLineLength,
            currentLineLength);
        throw SShowStopper(
            lineNum,
            EAlnSubcode::eAlnSubcode_BadDataCount,
            description);
    }
    return true;
};

END_SCOPE(objects)
END_NCBI_SCOPE
