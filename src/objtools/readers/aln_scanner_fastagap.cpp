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
#include "aln_errors.hpp"
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
                if (processingFirstSequence) {
                    expectedDataSizes.push_back(line.size());
                }
                else {
                    auto currentDataSize = line.size();
                    auto expectedDataSize = expectedDataSizes[currentDataLineIndex];
                    if (currentDataSize != expectedDataSize) {
                        string description = ErrorPrintf(
                            "Expected line length %d, actual length %d", 
                            expectedDataSize, currentDataSize);
                        throw SShowStopper(
                            lineNumber,
                            eAlnSubcode_BadDataCount,
                            description,
                            mSeqIds.back());
                    }
                }
                mSequences.back().push_back({line, lineNumber});
                currentDataLineIndex++;
                continue;
            }
        }

        if (line.empty()) {
            continue;
        }

        if (!NStr::StartsWith(line, ">")) {
            string description = ErrorPrintf(
                "Unexpected data line. Expected FASTA defline.");
            throw SShowStopper(
                lineNumber,
                EAlnSubcode::eAlnSubcode_IllegalDataLine,
                description);
        }
        sSplitFastaDef(line, seqId, defLine);
        mSeqIds.push_back(seqId);
        mDeflines.push_back({defLine, lineNumber});
        mSequences.push_back(vector<TLineInfo>());
        waitingForSeqData = true;
        currentDataLineIndex = 0;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
