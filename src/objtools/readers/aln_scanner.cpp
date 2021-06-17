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
#include "aln_scanner_fastagap.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ----------------------------------------------------------------------------
void
CAlnScanner::ProcessAlignmentFile(
    CSequenceInfo& sequenceInfo,
    CLineInput& iStr,
    SAlignmentFile& alignInfo)
//  ----------------------------------------------------------------------------
{
    xImportAlignmentData(sequenceInfo, iStr);
    xAdjustSequenceInfo(sequenceInfo);
    xVerifyAlignmentData(sequenceInfo);
    xExportAlignmentData(alignInfo);
}

//  ----------------------------------------------------------------------------
void
CAlnScanner::xImportAlignmentData(
    CSequenceInfo& sequenceInfo,
    CLineInput& iStr)
//  ----------------------------------------------------------------------------
{
    throw SShowStopper(
        -1,
        eAlnSubcode_UnsupportedFileFormat,
        "Input file format not recognized.");
}

//  ----------------------------------------------------------------------------
void
CAlnScanner::xAdjustSequenceInfo(
    CSequenceInfo& sequenceInfo)
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
void
CAlnScanner::xVerifyAlignmentData(
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
CAlnScanner::xVerifySingleSequenceData(
    const CSequenceInfo& sequenceInfo,
    const TLineInfo& seqIdInfo,
    const vector<TLineInfo> lineInfos)
//  -----------------------------------------------------------------------------
{
    const char* errTempl("Bad character [%c] found at data position %d.");

    enum ESeqPart {
        HEAD, BODY, TAIL
    };
    const string& alphabet = sequenceInfo.Alphabet();
    const string legalInHead =
        sequenceInfo.BeginningGap() + sequenceInfo.Missing();
    const string legalInBody =
        alphabet
        + sequenceInfo.MiddleGap()
        + sequenceInfo.Missing()
        + sequenceInfo.Match();
    const string legalInTail =
        sequenceInfo.EndGap() + sequenceInfo.Missing();

    ESeqPart seqPart = ESeqPart::HEAD;

    for (auto lineInfo: lineInfos) {
        if (lineInfo.mData.empty()) {
            continue;
        }
        string seqData(lineInfo.mData);

        if (seqPart == ESeqPart::HEAD) {
            auto startBody = seqData.find_first_not_of(legalInHead);
            if (startBody == string::npos) {
                continue;
            }
            seqPart = ESeqPart::BODY;
            seqData = seqData.substr(startBody);
            if (alphabet.find(seqData[0]) == string::npos) {
                auto linePos = lineInfo.mData.size() - seqData.size();
                string description = ErrorPrintf(errTempl, seqData[0], linePos);
                throw SShowStopper(
                    lineInfo.mNumLine,
                    EAlnSubcode::eAlnSubcode_BadDataChars,
                    description,
                    seqIdInfo.mData);
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
            auto linePos = lineInfo.mData.size() - seqData.size() + startBad;
            string description = ErrorPrintf(
                errTempl, seqData[startBad], linePos);
            throw SShowStopper(
                lineInfo.mNumLine,
                EAlnSubcode::eAlnSubcode_BadDataChars,
                description,
                seqIdInfo.mData);
        }
    }
}

//  ----------------------------------------------------------------------------
void
CAlnScanner::xExportAlignmentData(
    SAlignmentFile& alignInfo)
//  ----------------------------------------------------------------------------
{
    alignInfo.mIds.reserve(mSeqIds.size());
    for (auto seqId: mSeqIds) {
        alignInfo.mIds.push_back(seqId);
    }

    alignInfo.mDeflines.assign(mDeflines.begin(), mDeflines.end());

    auto numSequences = mSequences.size();
    alignInfo.mSequences.resize(numSequences);
    auto index = 0;
    for (auto sequence: mSequences) {
        for (auto seqPart: sequence) {
            alignInfo.mSequences[index] += seqPart.mData;
        }
        ++index;
    }
}

//  ----------------------------------------------------------------------------
CAlnScanner::ESeqIdComparison
CAlnScanner::xGetExistingSeqIdInfo(
    const string& seqId,
    TLineInfo& existingInfo)
//  ----------------------------------------------------------------------------
{

    for (const auto& idInfo : mSeqIds) {
        if (seqId == idInfo.mData) {
            existingInfo = idInfo;
            return ESeqIdComparison::eIdentical;
        }
    }

    string seqIdLower(seqId);
    NStr::ToLower(seqIdLower);
    for (const auto& idInfo : mSeqIds) {
        string idLower(idInfo.mData);
        NStr::ToLower(idLower);
        if (seqIdLower == idLower) {
            existingInfo = idInfo;
            return ESeqIdComparison::eDifferByCase;
        }
    }

    return ESeqIdComparison::eDifferentChars;
};

//  ----------------------------------------------------------------------------
bool
//  ----------------------------------------------------------------------------
CAlnScanner::xSeqIdIsEqualToInfoAt(
    const string& seqId,
    int index)
{
    if (index >= mSeqIds.size()) {
        return false;
    }
    auto seqIdCompare(mSeqIds[index].mData);
    if (seqId.size() != seqIdCompare.size()) {
        return false;
    }

    return (seqId == seqIdCompare);
}

END_SCOPE(objects)
END_NCBI_SCOPE
