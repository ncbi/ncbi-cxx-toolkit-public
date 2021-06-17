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
 * Authors:  Frank Ludwig
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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);


//  --------------------------------------------------------------------------
void
AlnUtil::CheckId(const string& seqId,
        const vector<SLineInfo>& orderedIds,
        int idCount,
        int lineNum,
        bool firstBlock)
//  --------------------------------------------------------------------------
{
    string description;

    if ((orderedIds.size() > idCount) &&
        seqId == orderedIds[idCount].mData) {
        return;
    }


    string seqIdLower(seqId);
    NStr::ToLower(seqIdLower);

    auto it = orderedIds.begin();
    bool exactCopy = false;
    while (it != orderedIds.end()) {
        if (it->mData == seqId) {
            exactCopy = true;
            break;
        }
        auto orderedIdLower(it->mData);
        NStr::ToLower(orderedIdLower);
        if (orderedIdLower == seqIdLower) {
            break;
        }
        ++it;
    }


    if (firstBlock) {
        if (it != orderedIds.end()) {
            if (exactCopy) {
                description = ErrorPrintf(
                    "Duplicate ID: \"%s\" has already appeared in this block, on line %d.", seqId.c_str(), it->mNumLine);
            }
            else {
                description = ErrorPrintf(
                "Conflicting IDs: \"%s\" differs only in case from \"%s\", which has already appeared in this block, on line %d.", seqId.c_str(), it->mData.c_str(), it->mNumLine);
            }
            throw SShowStopper(
                lineNum,
                eAlnSubcode_UnexpectedSeqId,
                description);

        }
        return;
    }

    if (it == orderedIds.end()) {
        description = ErrorPrintf(
            "Inconsistent sequence_IDs in the data blocks. Each data block must contain the same set of sequence_IDs.");
        throw SShowStopper(
            lineNum,
            eAlnSubcode_BadSequenceCount,
            description);
    }


    auto idPos = distance(orderedIds.begin(), it);
    if (idPos < idCount) {
        if (exactCopy) {
            description = ErrorPrintf(
                "Duplicate ID: \"%s \" has already appeared in this block, on line %d.",
                seqId.c_str(), it->mNumLine);
        }
        else {
            description = ErrorPrintf(
            "Conflicting IDs: \"%s\" differs only in case from \"%s\", which has already appeared in this block, on line %d.", seqId.c_str(), it->mData.c_str(), it->mNumLine);
        }
    }
    else
    if (idPos == idCount) { //
        description = ErrorPrintf(
            "Inconsistent ID case: \"%s\" differs in case from \"%s\" used to identify this sequence in the first block.",
            seqId.c_str(), it->mData.c_str());
    }
    else
    {
        description = "Sequence_IDs are in different orders in the data blocks in your file. The sequences and sequence_IDs are expected to be in the same order in each block.";
    }
    throw SShowStopper(
        lineNum,
        eAlnSubcode_UnexpectedSeqId,
        description);
}


//  --------------------------------------------------------------------------
void
AlnUtil::ProcessDefline(
    const string& line,
    string& seqId,
    string& defLineInfo)
//  --------------------------------------------------------------------------
{
    if (!NStr::StartsWith(line, ">")) {
        throw SShowStopper(
            -1,
            eAlnSubcode_IllegalDataLine,
            "Deflines were detected in your file, however some lines are missing the \'>\' character at the beginning of the line. Each defline must begin with \'>\'.");
    }
    auto dataStart = line.find_first_not_of(" \t", 1);
    if (dataStart == string::npos) {
        throw SShowStopper(
            -1,
            eAlnSubcode_IllegalDataLine,
            "Bad defline line: Should not be empty");
    }
    string defLine = line.substr(dataStart);
    if (NStr::StartsWith(defLine, "[")) {
        seqId.clear();
        defLineInfo = defLine;
    }
    else {
        NStr::SplitInTwo(defLine.substr(dataStart), " \t", seqId, defLineInfo,
            NStr::fSplit_MergeDelimiters);
    }
}

//  ----------------------------------------------------------------------------
void
AlnUtil::ProcessDataLine(
    const string& dataLine,
    string& seqId,
    string& seqData,
    int& offset)
//  ----------------------------------------------------------------------------
{
    list<string> tokens;
    NStr::Split(dataLine, " \t", tokens, NStr::fSplit_MergeDelimiters);
    if (tokens.size() < 2) {
        throw SShowStopper(
            -1,
            eAlnSubcode_IllegalDataLine,
            "Bad data line: Expected \"<seqId> <data> <offset>\"");
    }
    seqId = tokens.front();
    tokens.pop_front();
    if (tokens.back().find_first_not_of("0123456789") == string::npos) {
        // trailing token is offset
        offset = NStr::StringToInt(tokens.back());
        tokens.pop_back();
    }
    else {
        // trailing token is part of the data
    }
    seqData = NStr::Join(tokens, "");
}

//  ----------------------------------------------------------------------------
void
AlnUtil::ProcessDataLine(
    const string& dataLine,
    string& seqId,
    string& seqData)
//  ----------------------------------------------------------------------------
{
    list<string> tokens;
    NStr::Split(dataLine, " \t", tokens, NStr::fSplit_MergeDelimiters);
    if (tokens.size() < 2) {
        throw SShowStopper(
            -1,
            eAlnSubcode_IllegalDataLine,
            "Bad data line: Expected \"<seqId> <data> <offset>\"");
    }
    seqId = tokens.front();
    tokens.pop_front();
    seqData = NStr::Join(tokens, "");
}

//  ----------------------------------------------------------------------------
void
AlnUtil::StripBlanks(
    const string& line,
    string& stripped)
//  ----------------------------------------------------------------------------
{
    stripped = NStr::TruncateSpaces(line);
    vector<string> splits;
    NStr::Split(stripped, " \t", splits, NStr::fSplit_MergeDelimiters);
    stripped = NStr::Join(splits, "");
}

END_SCOPE(objects)
END_NCBI_SCOPE
