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
#include "aln_formatguess.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ----------------------------------------------------------------------------
EAlignFormat
CAlnFormatGuesser::GetFormat(
    CPeekAheadStream& iStr)
{
    vector<string> testSample;

    xInitSample(iStr, testSample);
    if (testSample.empty()) {
        return EAlignFormat::UNKNOWN;
    }
    if (xSampleIsNexus(testSample)) {
        return EAlignFormat::NEXUS;
    }
    if (xSampleIsClustal(testSample, iStr)) {
        return EAlignFormat::CLUSTAL;
    }
    if (xSampleIsFastaGap(testSample)) {
        return EAlignFormat::FASTAGAP;
    }
    if (xSampleIsPhylip(testSample)) {
        return EAlignFormat::PHYLIP;
    }
    if (xSampleIsSequin(testSample)) {
        return EAlignFormat::SEQUIN;
    }
    if (xSampleIsMultAlign(testSample)) {
        return EAlignFormat::MULTALIN;
    }
    return EAlignFormat::UNKNOWN;
};


//  ----------------------------------------------------------------------------
void
CAlnFormatGuesser::xInitSample(
    CPeekAheadStream& iStr,
    vector<string>& testSample)
//  ----------------------------------------------------------------------------
{
    string testLine;
    for (auto i=0; i < SAMPLE_SIZE; ++i) {
        if (!iStr.PeekLine(testLine)) {
            break;
        }
        NStr::TruncateSpacesInPlace(testLine);
        testSample.push_back(testLine);
    }
};


//  -----------------------------------------------------------------------------
bool
CAlnFormatGuesser::xSampleIsNexus(
    const TSample& sample)
//  -----------------------------------------------------------------------------
{
    // reminder:
    //  all sample lines are space truncated
    //  there are at least one and most SAMPLE_SIZE sample lines

    string testLine = sample[0];
    NStr::ToLower(testLine);
    return NStr::StartsWith(testLine, "#nexus");
}


//  -----------------------------------------------------------------------------
bool
CAlnFormatGuesser::xSampleIsClustal(
    TSample& sample,
    CPeekAheadStream& iStr)
//  -----------------------------------------------------------------------------
{
    // reminder:
    //  all sample lines are space truncated
    //  there are at least one and most SAMPLE_SIZE sample lines

    const string CONSENSUS_CHARS = " .:*";

    string testLine = sample[0];
    NStr::ToLower(testLine);
    if (NStr::StartsWith(testLine, "clustalw")) {
        return true;
    }
    if (NStr::StartsWith(testLine, "clustal w")) {
        return true;
    }
    int lineCount = 0;
    int blockCount = 0;
    while (blockCount < 3) {
        string line;
        if (lineCount < sample.size()) {
            line = sample[lineCount];
        }
        else {
            iStr.PeekLine(line);
            sample.push_back(line);
        }
        if (lineCount > 0  &&  line.empty()) {
            auto maybeConsensus = sample[lineCount-1];

            auto firstConsensusChar = maybeConsensus.find_first_of(
                CONSENSUS_CHARS.substr(1));
            if (firstConsensusChar == string::npos) {
                return false;
            }
            auto firstNonConsensusChar = maybeConsensus.find_first_not_of(
                CONSENSUS_CHARS);
            if (firstNonConsensusChar != string::npos) {
                return false;
            }
            ++blockCount;
        }
        ++lineCount;
    }
    return true;
}


//  -----------------------------------------------------------------------------
bool
CAlnFormatGuesser::xSampleIsFastaGap(
    const TSample& sample)
//  -----------------------------------------------------------------------------
{
    // reminder:
    //  all sample lines are space truncated
    //  there are at least one and most SAMPLE_SIZE sample lines

    int i=0;
    for ( ; i < sample.size(); ++i) {
        if (!NStr::StartsWith(sample[i], ";")) {
            break;
        }
    }
    if (i < sample.size()) {
        return NStr::StartsWith(sample[i], ">");
    }
    //FIX ME: look at more sample lines rather than jump to conclusion
    return false;
}


//  -----------------------------------------------------------------------------
bool
CAlnFormatGuesser::xSampleIsPhylip(
    const TSample& sample)
//  -----------------------------------------------------------------------------
{
    // reminder:
    //  all sample lines are space truncated
    //  there are at least one and most SAMPLE_SIZE sample lines

    string testLine = sample[0];
    vector<string> tokens;
    NStr::Split(testLine, " \t", tokens, NStr::fSplit_MergeDelimiters);
    if (tokens.size() != 2) {
        return false;
    }
    if (tokens.front().find_first_not_of("0123456789") != string::npos) {
        return false;
    }
    if (tokens.back().find_first_not_of("0123456789") != string::npos) {
        return false;
    }
    return true;
}


//  -----------------------------------------------------------------------------
bool
CAlnFormatGuesser::xSampleIsSequin(
    const TSample& sample)
//  -----------------------------------------------------------------------------
{
    // reminder:
    //  all sample lines are space truncated
    //  there are at least one and most SAMPLE_SIZE sample lines

    if (!sample[0].empty()) {
        return false;
    }
    string offsets = sample[1];
    vector<string> tokens;
    NStr::Split(offsets, " \t", tokens, NStr::fSplit_MergeDelimiters);
    if (tokens.empty()) {
        return false;
    }
    for (int index=0; index < tokens.size(); ++index) {
        auto offset = NStr::StringToInt(tokens[index], NStr::fConvErr_NoThrow);
        if (offset != 10 + 10*index) {
            return false;
        }
    }
    return true;
}


//  -----------------------------------------------------------------------------
bool
CAlnFormatGuesser::xSampleIsMultAlign(
    const TSample& sample)
//  -----------------------------------------------------------------------------
{
    // reminder:
    //  all sample lines are space truncated
    //  there are at least one and most SAMPLE_SIZE sample lines

    int lineIndex = 0;
    if (NStr::StartsWith(sample[0], "//")) {
        ++lineIndex;
    }
    if (sample.size() < lineIndex + 4) {
        return false;
    }
    if (!sample[lineIndex].empty()) {
        return false;
    }

    ++lineIndex;
    vector<string> tokens;
    NStr::Split(sample[lineIndex], " \t", tokens, NStr::fSplit_MergeDelimiters);
    if (tokens.size() != 2) {
        return false;
    }
    int startOffset(0), endOffset(0);
    try {
        startOffset = NStr::StringToInt(tokens[0]);
        endOffset = NStr::StringToInt(tokens[1]);
    }
    catch (std::exception&) {
        return false;
    }
    if (startOffset != 1) {
        return false;
    }
    if (endOffset > 50) {
        return false;
    }

    ++lineIndex;
    list<string> dataColumns;
    NStr::Split(sample[lineIndex], " \t", dataColumns, NStr::fSplit_MergeDelimiters);
    if (dataColumns.size() < 2) {
        return false;
    }
    dataColumns.pop_front();
    string seqData = NStr::Join(dataColumns.begin(), dataColumns.end(), "");
    auto dataSize = seqData.size();
    return (dataSize == (endOffset - startOffset + 1));
}


END_SCOPE(objects)
END_NCBI_SCOPE
