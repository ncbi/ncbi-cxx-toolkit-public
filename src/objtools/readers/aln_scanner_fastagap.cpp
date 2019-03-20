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
#include "aln_scanner_fastagap.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ----------------------------------------------------------------------------
void
CAlnScannerFastaGap::sSplitFastaDef(
    const string& rawDefStr,
    string& seqId,
    string& defLine)
//  ----------------------------------------------------------------------------
{
    string defStr = rawDefStr.substr(1);
    NStr::TruncateSpacesInPlace(defStr);
    NStr::SplitInTwo(defStr, " \t", seqId, defLine, NStr::fSplit_MergeDelimiters);
}

//  ----------------------------------------------------------------------------
void
CAlnScannerFastaGap::ProcessAlignmentFile(
    const CSequenceInfo& sequenceInfo,
    TAlignRawFilePtr afrp)
//  ----------------------------------------------------------------------------
{
    bool waitingForSeqData = false;
    vector<int> expectedDataSizes;
    int currentDataLineIndex = 0;
    bool processingFirstSequence = true;

    for (auto linePtr = afrp->line_list; linePtr; linePtr = linePtr->next) {

        string seqId;
        string defLine;
        string seqData;

        string line(linePtr->data);
        NStr::TruncateSpacesInPlace(line);

        if (waitingForSeqData) {
            if (line.empty()  ||  NStr::StartsWith(line, ">")) {
                waitingForSeqData = false;
                processingFirstSequence = false;
            }
            else {
                if (processingFirstSequence) {
                    expectedDataSizes.push_back(strlen(linePtr->data));
                }
                else {
                    auto currentDataSize = strlen(linePtr->data);
                    auto expectedDataSize = expectedDataSizes[currentDataLineIndex];
                    if (currentDataSize != expectedDataSize) {
                        string description = StrPrintf(
                            "Expected line length %d, actual length %d", 
                            expectedDataSize, currentDataSize);
                        throw SShowStopper(
                            linePtr->line_num,
                            eAlnSubcode_BadDataCount,
                            description,
                            mSeqIds.back());
                    }
                }
                mSequences.back().push_back(linePtr);
                currentDataLineIndex++;
                continue;
            }
        }

        if (line.empty()) {
            continue;
        }
        if (!NStr::StartsWith(line, ">")) {
            string description = StrPrintf(
                "Unexpected data line. Expected FASTA defline.");
            throw SShowStopper(
                linePtr->line_num,
                EAlnSubcode::eAlnSubcode_IllegalDataLine,
                description);
        }
        sSplitFastaDef(line, seqId, defLine);
        mSeqIds.push_back(seqId);
        mSequences.push_back(vector<TLineInfoPtr>());
        waitingForSeqData = true;
        currentDataLineIndex = 0;

    }

    xVerifySequenceData(sequenceInfo);

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

//  ----------------------------------------------------------------------------
void
CAlnScannerFastaGap::xVerifySequenceData(
    const CSequenceInfo& sequenceInfo)
//  ----------------------------------------------------------------------------
{
    // make sure all sequence are of the same length(once we no longer enforce
    //  harmonized data sizes):

    // make sure all sequence characters are legal, and legal in the places where
    //  they show up:
    for (auto i=0; i < mSequences.size(); ++i) {
        const auto& seqData = mSequences[i];
        const auto& seqId = mSeqIds[i];
         xVerifySingleSequenceData(sequenceInfo, seqId, seqData);
    }
}

//  ----------------------------------------------------------------------------
void
CAlnScannerFastaGap::xVerifySingleSequenceData(
    const CSequenceInfo& sequenceInfo,
    const string& seqId,
    const vector<TLineInfoPtr> seqDataPtrs)
//  -----------------------------------------------------------------------------
{
    const char* errTempl("Bad character [%c] found at character position %d.");

    enum ESeqPart {
        HEAD, BODY, TAIL
    };
    const string& alphabet = sequenceInfo.Alphabet();
    const string legalInHead = sequenceInfo.BeginningGap();
    const string legalInBody = alphabet + sequenceInfo.MiddleGap();
    const string legalInTail = sequenceInfo.EndGap();

    ESeqPart seqPart = ESeqPart::HEAD;

    for (auto linePtr: seqDataPtrs) {
        if (!linePtr->data) {
            continue;
        }
        string seqData(linePtr->data);

        if (seqPart == ESeqPart::HEAD) {
            auto startBody = seqData.find_first_not_of(legalInHead);
            if (startBody == string::npos) {
                continue;
            }
            seqPart = ESeqPart::BODY;
            seqData = seqData.substr(startBody);
            if (alphabet.find(seqData[0]) == string::npos) {
                int linePos = strlen(linePtr->data) - seqData.size();
                string description = StrPrintf(errTempl, seqData[0], linePos);
                throw SShowStopper(
                    linePtr->line_num,
                    EAlnSubcode::eAlnSubcode_BadDataChars,
                    description,
                    seqId);
            }
        }
        if (seqPart == ESeqPart::BODY) {
            auto startTail = seqData.find_first_not_of(legalInBody);
            if (startTail == string::npos) {
                continue;
            }
            seqPart = ESeqPart::TAIL;
            seqData = seqData.substr(startTail);
        }
        if (seqPart == ESeqPart::TAIL) {
            auto startBad = seqData.find_first_not_of(legalInTail);
            if (startBad == string::npos) {
                continue;
            }
            int linePos = strlen(linePtr->data) - seqData.size() + startBad;
            string description = StrPrintf(
                errTempl, seqData[startBad], linePos);
            throw SShowStopper(
                linePtr->line_num,
                EAlnSubcode::eAlnSubcode_BadDataChars,
                description,
                seqId);
        }
    }
}



END_SCOPE(objects)
END_NCBI_SCOPE
