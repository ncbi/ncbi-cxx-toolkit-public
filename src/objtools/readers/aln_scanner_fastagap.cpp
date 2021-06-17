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

//  ===========================================================================
//  Fasta+Gap info:
//  Specs vary across the web. All specs agree that sequence characters plus
//    characters must add up to the same sequence length for each sequence
//    incuded in the input.
//  NCBI accepts all ambiguity characters as part of the nucleotide alphabet.
//  NCBI does not use match or missing characters (use ambiguity character 'N'
//    for missing).
//  NCBI uses '-' as its gap character but I can't find that mandated anywhere.
//  Other sources will allow for anything that's not alphanumeric as a gap
//    character. The gap character may even change depending on whether it's
//    in the beginning, the middle, or the send of a sequence.
//
//  Reference:
//  www.cgl.ucsf.edu/chimera/docs/ContributedSoftware/multalignviewer/afasta.html
//  blast.ncbi.nlm.nih.gov/Blast.cgi?CMD=Web&PAGE_TYPE=BlastDocs&DOC_TYPE=BlastHelp
//  ============================================================================


#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/alnread.hpp>
#include <objtools/readers/reader_error_codes.hpp>
#include "aln_errors.hpp"
#include "aln_util.hpp"
#include "aln_peek_ahead.hpp"
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
CAlnScannerFastaGap::xImportAlignmentData(
    CSequenceInfo& sequenceInfo,
    CLineInput& iStr)
//  ----------------------------------------------------------------------------
{
    bool waitingForSeqData = false;
    vector<int> expectedDataSizes;
    int currentDataLineIndex = 0;
    bool processingFirstSequence = true;
    int expectedNumLines = 0;

    string line;
    int lineNumber = 0;
    while(iStr.ReadLine(line, lineNumber)) {
        NStr::TruncateSpacesInPlace(line);

        string seqId;
        string defLine;
        string seqData;

        if (waitingForSeqData) {
            if (line.empty()  ||  NStr::StartsWith(line, ">")) {
                waitingForSeqData = false;
                processingFirstSequence = false;
            }
            else {
                string seqData;
                AlnUtil::StripBlanks(line, seqData);
                if (processingFirstSequence) {
                    expectedDataSizes.push_back(seqData.size());
                }
                else {

                    if (!expectedNumLines) {
                        expectedNumLines = expectedDataSizes.size();
                    }
                    auto currentDataSize = seqData.size();
                    auto expectedDataSize = (currentDataLineIndex < expectedNumLines) ?
                        expectedDataSizes[currentDataLineIndex] :
                        0;
                    if (currentDataSize != expectedDataSize) {
                        string description =
                            BadCharCountPrintf(expectedDataSize, currentDataSize);
                        throw SShowStopper(
                            lineNumber,
                            eAlnSubcode_BadDataCount,
                            description,
                            mSeqIds.back().mData);
                    }
                }
                mSequences.back().push_back({seqData, lineNumber});
                currentDataLineIndex++;
                continue;
            }
        }

        if (line.empty()) {
            continue;
        }

        sSplitFastaDef(line, seqId, defLine);
        if (seqId.empty()) {
            throw SShowStopper(
                lineNumber,
                EAlnSubcode::eAlnSubcode_IllegalDefinitionLine,
                "Invalid Fasta definition line. \">\" must be followed by a sequence_ID.");
        }

        TLineInfo existingInfo;
        auto idComparison
            = xGetExistingSeqIdInfo(seqId, existingInfo);
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
                lineNumber,
                EAlnSubcode::eAlnSubcode_UnexpectedSeqId,
                description);
        }
        mSeqIds.push_back({seqId, lineNumber});
        mDeflines.push_back({defLine, lineNumber});
        mSequences.push_back(vector<TLineInfo>());
        waitingForSeqData = true;
        currentDataLineIndex = 0;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
