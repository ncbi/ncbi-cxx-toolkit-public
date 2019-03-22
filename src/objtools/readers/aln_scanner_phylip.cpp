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
#include "aln_data.hpp"
#include "aln_errors.hpp"
#include "aln_peek_ahead.hpp"
#include "aln_scanner_phylip.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);


//  ----------------------------------------------------------------------------
void
CAlnScannerPhylip::sExtractDefLine(
    const string& rawDefStr,
    string& defLine)
//  ----------------------------------------------------------------------------
{
    defLine = rawDefStr.substr(1);
}


//  ----------------------------------------------------------------------------
void
CAlnScannerPhylip::xImportAlignmentData(
    CLineInput& iStr)
//  ----------------------------------------------------------------------------
{
    string line;
    int lineCount(0);
    if (!iStr.ReadLine(line)) {
        //error
    }
    ++lineCount;

    vector<string> tokens;
    NStr::TruncateSpacesInPlace(line);
    NStr::Split(line,  " \t", tokens, NStr::fSplit_MergeDelimiters);
    auto numSeqs =  NStr::StringToInt(tokens[0]);
    auto dataCount = NStr::StringToInt(tokens[1]); 

    int dataLineCount(0);
    int blockLineLength(0);
    // move onto the next line

    while (iStr.ReadLine(line)) {
        ++lineCount;
        if (line.empty()) {
            continue;
        }
        // record potential defline
        if (line[0] == '>') {
            string defLine;
            sExtractDefLine(line, defLine);
            mDeflines.push_back({defLine, lineCount});
            continue;
        }
        if (line[0] == '[') {
            mDeflines.push_back({line, lineCount});
            continue;
        }

        NStr::TruncateSpacesInPlace(line);
        
        string lowerLine(line);
        NStr::ToLower(lowerLine);
        
        if (lowerLine == "begin ncbi;" ||
            lowerLine == "begin ncbi" ||
            lowerLine == "sequin" ||
            lowerLine == "end;" ||
            lowerLine == "end") {
            continue;
        }

        bool newBlock = ((dataLineCount % numSeqs) == 0);

        string seqData;
        if (dataLineCount < numSeqs) { // in first block
            string seqId;
            NStr::SplitInTwo(line, " \t", seqId, seqData, NStr::fSplit_MergeDelimiters);
            if (seqData.empty()) {
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataLine,
                    "In data line, expected seqID followed by sequence data");
            }

            const auto it = find(mSeqIds.begin(), mSeqIds.end(), seqId);
            if (it !=  mSeqIds.end()) {
                auto description = StrPrintf(
                        "Duplicate ID: \"%s\" has already appeared in this block    .",
                         seqId.c_str());
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_UnexpectedSeqId,
                    description);
            }
            mSeqIds.push_back(seqId);
            mSequences.push_back(vector<TLineInfo>());
            //seqData = tokens[1];
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
            string description = StrPrintf(
                "In data line, expected %d symbols but finding %d",
                blockLineLength,
                currentLineLength);
            throw SShowStopper(
                lineCount,
                EAlnSubcode::eAlnSubcode_BadDataCount,
                description);
        }

        mSequences[dataLineCount % numSeqs].push_back({seqData, lineCount});
        ++dataLineCount;
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
