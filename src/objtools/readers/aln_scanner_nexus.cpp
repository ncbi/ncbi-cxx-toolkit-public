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
#include "aln_scanner_nexus.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

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


//  ----------------------------------------------------------------------------
void 
CAlnScannerNexus::ProcessAlignmentFile(
    TAlignRawFilePtr afrp)
//  ----------------------------------------------------------------------------
{
    const int NUM_SEQUENCES(afrp->expected_num_sequence);
    const int SIZE_SEQUENCE(afrp->expected_sequence_len);

    int dataLineCount(0);
    int blockLineLength(0);
    int sequenceCharCount(0);
    int unmatchedLeftBracketCount(0);
    int commentStartLine(-1);

    //at this point we already know the number of sequences to expect, and the
    // number of symbols each sequence is supposed to have.
    // we have also already extracted any defines that were in this file.

    //what remains is extraction of the actual sequence data:
    for (auto linePtr = afrp->line_list; linePtr; linePtr = linePtr->next) {
        if (!linePtr->data  ||  linePtr->data[0] == 0) {
            continue;
        }
        string line(linePtr->data);
        if (mState == EState::SKIPPING) {
            NStr::ToLower(line);
        }

        if (mState == EState::SKIPPING) {
            if (NStr::StartsWith(line, "matrix")) {
                mState = READING;
            }
            continue;
        }

        if (mState == EState::READING) {

            auto lineStrLower(line);
            auto previousBracketCount = unmatchedLeftBracketCount;
            sStripNexusComments(line, unmatchedLeftBracketCount);
            if (previousBracketCount == 0 &&
                unmatchedLeftBracketCount == 1) {
                commentStartLine = linePtr->line_num;
            }


            strncpy(linePtr->data, line.c_str(), line.size()+1);
            if (line.empty()) {
                continue;
            }
            NStr::ToLower(lineStrLower);

            if (NStr::StartsWith(lineStrLower, "end;")) {
                theErrorReporter->Warn(
                    linePtr->line_num,
                    EAlnSubcode::eAlnSubcode_IllegalDataLine,
                    "Unexpected \"end;\". Appending \';\' to prior command");
                mState = EState::SKIPPING;
                continue;
            }

            bool isLastLineOfData = NStr::EndsWith(line, ";");
            if (isLastLineOfData) {
                line = line.substr(0, line.size() -1 );
                linePtr->data[line.size()] = 0;
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
                    linePtr->line_num,
                    EAlnSubcode::eAlnSubcode_IllegalDataLine,
                    "In data line, expected seqID followed by sequence data"); 
            }

            string seqId = tokens[0];
            if (dataLineCount < NUM_SEQUENCES) {
                if (std::find(mSeqIds.begin(), mSeqIds.end(), seqId) != mSeqIds.end()) {
                    return; //ERROR: duplicate ID
                }
                mSeqIds.push_back(seqId);
                mSequences.push_back(vector<TLineInfoPtr>());
            }
            else {
                if (mSeqIds[dataLineCount % NUM_SEQUENCES] != seqId) {
                    string description;
                    if (std::find(mSeqIds.begin(), mSeqIds.end(), seqId) == mSeqIds.end()) {
                        description = StrPrintf(
                            "Expected %d sequences, but finding data for another.",
                            NUM_SEQUENCES);
                    }
                    else {
                        description = StrPrintf(
                            "Finding data for sequence \"%s\" out of order.",
                            seqId.c_str());
                    }
                    throw SShowStopper(
                        linePtr->line_num,
                        EAlnSubcode::eAlnSubcode_BadSequenceCount,
                        description);
                }
            }

            string seqData = NStr::Join(tokens.begin()+1, tokens.end(), "");
            auto dataSize = seqData.size();
            auto dataIndex = dataLineCount % NUM_SEQUENCES;
            if (dataIndex == 0) {
                sequenceCharCount += dataSize;
                if (sequenceCharCount > SIZE_SEQUENCE) {
                    string description = StrPrintf(
                        "Expected %d symbols per sequence but finding already %d",
                        SIZE_SEQUENCE,
                        sequenceCharCount);
                    throw SShowStopper(
                        linePtr->line_num,
                        EAlnSubcode::eAlnSubcode_BadDataCount,
                        description); 
                }
                blockLineLength = dataSize;
            }
            else {
                if (dataSize != blockLineLength) {
                    string description = StrPrintf(
                        "In data line, expected %d symbols but finding %d",
                        blockLineLength,
                        dataSize);
                    throw SShowStopper(
                        linePtr->line_num,
                        EAlnSubcode::eAlnSubcode_BadDataCount,
                        description); 
                }
            }

            mSequences[dataIndex].push_back(linePtr);

            dataLineCount += 1;
            if (isLastLineOfData) {
                mState = EState::SKIPPING;
            }
            continue;
        }
    }

    if (unmatchedLeftBracketCount>0) {
        string description = StrPrintf(
                "Unterminated comment beginning on line %d",
                commentStartLine);
        throw SShowStopper(
                commentStartLine,
                EAlnSubcode::eAlnSubcode_UnterminatedComment,
                description);
    }

    //submit collected data to a final sanity check:
    if (sequenceCharCount != SIZE_SEQUENCE) {
        string description = StrPrintf(
            "Expected %d symbols per sequence but finding only %d",
            SIZE_SEQUENCE,
            sequenceCharCount);
        throw SShowStopper(
            -1,
            EAlnSubcode::eAlnSubcode_BadDataCount,
            description); 
    }

    //looks good- populate approprate afrp data structures:
    for (auto idIndex = 0; idIndex < mSeqIds.size(); ++idIndex) {
        for (auto seqPart = 0; seqPart < mSequences[idIndex].size(); ++seqPart) {
            auto liPtr = mSequences[idIndex][seqPart];
            string liData = string(liPtr->data);
            auto endOfId = liData.find_first_of(" \t");
            auto startOfData = liData.find_first_not_of(" \t", endOfId);
                afrp->sequences = SAlignRawSeq::sAddSeqById(
                    afrp->sequences, mSeqIds[idIndex],
                    liPtr->data + startOfData, liPtr->line_num, liPtr->line_num,
                    startOfData);
        }
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
