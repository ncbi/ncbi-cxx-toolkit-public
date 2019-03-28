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
#include "aln_peek_ahead.hpp"
#include "aln_scanner_nexus.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ============================================================================
//  Nexus Info:
//  Nexus files consist of blocks which contain commands. Every command is in
//    a block, and there are no nested blocks.
//  The alphabet is determined by the "datatype" parameter of the "format"
//    command. For DNA, the safest route is to expect ACGT and all ambiguity
//    characters.
//  The gap character is defined by the "gap" parameter of the format command.
//  The missing character is defined by the "missing" parameter of the format
//    command.
//  There are bunch of other settings in the format command that can affect the
//    form and meaning of the data.
//
//  Reference: informatics.nescent.org/w/images/8/8b/NEXUS_Final.pdf
//  ============================================================================

//  ----------------------------------------------------------------------------

void sPrintCommand(const list<SLineInfo>& command)
{
    for (auto lineInfo : command) {
        cout << lineInfo.mData << endl;
    }
}

//  ----------------------------------------------------------------------------
void 
CAlnScannerNexus::xProcessCommand(
    TCommand command) 
//  ----------------------------------------------------------------------------
{
    if (command.empty()) {
        return;
    }

    string firstLine = command.front().mData;
    NStr::ToLower(firstLine);
    string commandName;
    string remainder;

    NStr::SplitInTwo(firstLine, 
            " \t", 
            commandName, 
            remainder);

    if (commandName != "sequin") {
        sStripNexusComments(command);
    }


    if (commandName != "end") {
        string lastLine = command.back().mData;
        NStr::ToLower(lastLine);
        int lastWhiteSpacePos = lastLine.find_last_of(" \t");
        string lastToken = (lastWhiteSpacePos == NPOS) ?
                            lastLine :
                            lastLine.substr(lastWhiteSpacePos);
        if (lastToken == "end") {
            theErrorReporter->Warn(
                command.back().mNumLine,
                EAlnSubcode::eAlnSubcode_IllegalDataLine,
                "Unexpected \"end;\". Appending \';\' to prior command");

            if (lastWhiteSpacePos == NPOS) {
                command.pop_back();
            }
            else {
                command.back().mData = NStr::TruncateSpaces(
                        command.back().mData.substr(0, lastWhiteSpacePos));
            }
        }
    }

    

    if (commandName == "dimensions") {
        xProcessDimensions(command);
        return;
    }
    if (commandName == "format") {
        xProcessFormat(command); 
        return;
    }

    if (commandName == "matrix") {
        xProcessMatrix(command);
        return;
    }

    if (commandName == "sequin") {
        xProcessSequin(command);
        return;
    }

    if (commandName == "begin") {
        // xBeginBlock(command);
        return;
    }

    if (commandName == "end") {
        // xEndBlock(command);
        return;
    }
    
}

//  ----------------------------------------------------------------------------
void 
CAlnScannerNexus::xProcessMatrix(
    const TCommand& command)
//  ----------------------------------------------------------------------------
{
    int dataLineCount(0);
    int seqCount(0);
    int blockLineLength(0);
    int sequenceCharCount(0);

    auto cit = command.begin();

    for(++cit; cit != command.end(); ++dataLineCount, ++cit) {

        seqCount = dataLineCount % mNumSequences;
        vector<string> tokens;
        NStr::Split(cit->mData, " \t", tokens, NStr::fSplit_Tokenize);
        if (tokens.size() < 2) {
            throw SShowStopper(
                cit->mNumLine,
                eAlnSubcode_IllegalDataLine,
                "In data line, expected seqID followed by sequence data");
        }

        string seqId = tokens[0];

        if (dataLineCount < mNumSequences) {
            if (find(mSeqIds.begin(), mSeqIds.end(), seqId) != mSeqIds.end()) {
                string description = ErrorPrintf(
                    "Duplicate ID: \"%s\" has already appeared in this block.", seqId.c_str());
                throw SShowStopper(
                    cit->mNumLine,
                    eAlnSubcode_UnexpectedSeqId,
                    description);
            }
            mSeqIds.push_back(seqId);
            mSequences.push_back(vector<TLineInfo>());
        }
        else if (seqId != mSeqIds[seqCount]) {
            string description;
            const auto it = find(mSeqIds.begin(), mSeqIds.end(), seqId);
            if (it == mSeqIds.end()) {
                description = ErrorPrintf(
                    "Expected %d sequences, but finding data for another.",
                    mNumSequences);
                throw SShowStopper(
                    cit->mNumLine,
                    eAlnSubcode_BadSequenceCount,
                    description);
            }

            if (distance(mSeqIds.begin(), it) < seqCount) {
                description = ErrorPrintf(
                    "Duplicate ID: \"%s \" has already appeared in this block.",
                    seqId.c_str());
            }
            else {
                description = ErrorPrintf(
                    "Finding data for sequence \"%s\" out of order.",
                    seqId.c_str());
            }
            throw SShowStopper(
                cit->mNumLine,
                eAlnSubcode_UnexpectedSeqId,
                description);
        }
            
        string seqData = NStr::Join(tokens.begin()+1, tokens.end(), "");
        auto dataSize = seqData.size();
        auto dataIndex = dataLineCount % mNumSequences;
        if (dataIndex == 0) {
            sequenceCharCount += dataSize;
            if (sequenceCharCount > mSequenceSize) {
                string description = ErrorPrintf(
                    "Expected %d symbols per sequence but finding already %d",
                    mSequenceSize,
                    sequenceCharCount);
                throw SShowStopper(
                    cit->mNumLine,
                    EAlnSubcode::eAlnSubcode_BadDataCount,
                    description); 
            }
            blockLineLength = dataSize;
        }
        else {
            if (dataSize != blockLineLength) {
                string description = ErrorPrintf(
                    "In data line, expected %d symbols but finding %d",
                    blockLineLength,
                    dataSize);
                throw SShowStopper(
                        cit->mNumLine,
                        EAlnSubcode::eAlnSubcode_BadDataCount,
                        description); 
            }
        }

        mSequences[seqCount].push_back({tokens[1], cit->mNumLine});
    }

    if (sequenceCharCount != mSequenceSize) {
        string description = ErrorPrintf(
            "Expected %d symbols per sequence but finding only %d",
            mSequenceSize,
            sequenceCharCount);
        throw SShowStopper(
            -1,
            EAlnSubcode::eAlnSubcode_BadDataCount,
            description); 
    }
}

//  ----------------------------------------------------------------------------
void 
CAlnScannerNexus::xProcessDimensions(
    const TCommand& command)
//  ----------------------------------------------------------------------------
{
    auto ntax = xGetKeyVal(command, "ntax");
    if (!ntax.first.empty()) {
        try {
            mNumSequences = NStr::StringToInt(ntax.first);
        }
        catch(...) {
            string description = ErrorPrintf("Invalid nTax setting \"%s\"", 
                    ntax.first.c_str());
                throw SShowStopper(
                    ntax.second,
                    EAlnSubcode::eAlnSubcode_IllegalDataDescription,
                    description);
        }
    }
    
    auto nchar = xGetKeyVal(command, "nchar"); 
    try {
        mSequenceSize = NStr::StringToInt(nchar.first);
    }
    catch(...) {
        string description = ErrorPrintf("Invalid nChar setting \"%s\"", 
                    nchar.first.c_str());
                throw SShowStopper(
                    nchar.second,
                    EAlnSubcode::eAlnSubcode_IllegalDataDescription,
                    description);
    }
}


//  ----------------------------------------------------------------------------
void 
CAlnScannerNexus::xProcessFormat(
    const TCommand& command)
//  ----------------------------------------------------------------------------
{
    const auto missing = xGetKeyVal(command, "missing");
    const auto gap = xGetKeyVal(command, "gap");
    const auto matchchar = xGetKeyVal(command, "matchchar");

    if (!missing.first.empty()) {
        mMissingChar = missing.first[0];
    }

    if (!gap.first.empty()) {
        mGapChar = gap.first[0];
    }

    if (!matchchar.first.empty()) {
        mMatchChar = matchchar.first[0];
    }
}


//  ----------------------------------------------------------------------------
void 
CAlnScannerNexus::xProcessSequin(
    const TCommand& command)
//  ----------------------------------------------------------------------------
{
    for (const auto& lineInfo : command) {
        vector<string> deflineVec;
        NStr::Split(lineInfo.mData, ">", deflineVec);
        for (int i=1; i<deflineVec.size(); ++i) {
            mDeflines.push_back({deflineVec[i], lineInfo.mNumLine});
        }
    }
}


//  ----------------------------------------------------------------------------
pair<string, int>
CAlnScannerNexus::xGetKeyVal(
    const TCommand& command,
    const string& key)
//  ----------------------------------------------------------------------------
{
    int keyPos = NPOS;
    int valPos = NPOS;
    int keyLine = 0;
    int endPos = 0;

    for (auto lineInfo : command) {
        if (keyPos == NPOS) {
            keyPos = NStr::FindNoCase(lineInfo.mData, key);
            if (keyPos != NPOS) {
                keyLine = lineInfo.mNumLine;
                endPos = lineInfo.mData.find_first_of(" \t=", keyPos);
            }
        }

        if (keyPos != NPOS) {
            int currentLine = lineInfo.mNumLine;
            if (currentLine != keyLine) {
                endPos = 0;
            }
            valPos = lineInfo.mData.find_first_not_of(" \t=", endPos);
            if (valPos != NPOS) {
                endPos = lineInfo.mData.find_first_of(" \t\n;", valPos);
                if (endPos != NPOS) {
                    return {lineInfo.mData.substr(valPos, endPos-valPos), lineInfo.mNumLine};
                }
                return {lineInfo.mData.substr(valPos), lineInfo.mNumLine};
            }
        } 
    }
    return {"", -1};
}

static 
int s_FindCharOutsideComment(
        char c,
        const string& line,
        int &numUnmatchedLeftBrackets)
{
    int index=0;
    while (index < line.size()) {
        if (line[index] == '[') {
            ++numUnmatchedLeftBrackets;
        }
        else 
        if (line[index] == ']') {
            --numUnmatchedLeftBrackets;
        }
        else 
        if ((numUnmatchedLeftBrackets == 0) &&
            line[index] == c) {
            return index;
        }

        ++index;
    }
    return NPOS;
}
                        

//  ----------------------------------------------------------------------------
void 
CAlnScannerNexus::xImportAlignmentData(
    CSequenceInfo& sequenceInfo,
    CLineInput& iStr)
//  ----------------------------------------------------------------------------
{
    string line;
    int lineCount(0);
    TCommand currentCommand;
    size_t commandEnd(0);
    int commandStart(0);
    int numOpenBrackets(0);
    int commentStartLine(-1);
    bool inCommand(false);


    while (iStr.ReadLine(line, lineCount)) {

        NStr::TruncateSpacesInPlace(line);

        if (line == "#NEXUS") {
            continue;
        }

        int previousOpenBrackets = numOpenBrackets;
        sStripNexusComments(line, numOpenBrackets, inCommand);

        if (previousOpenBrackets == 0 &&
            numOpenBrackets > 0) {
            commentStartLine = lineCount;
        }
            
        if (line.empty()) {
            continue;
        }
        previousOpenBrackets = numOpenBrackets;
        commandEnd = s_FindCharOutsideComment(';', line, numOpenBrackets);
        if (previousOpenBrackets == 0 &&
            numOpenBrackets > 0) {
            commentStartLine = lineCount;
        }


        while (commandEnd != NPOS) {
            string commandArgs = NStr::TruncateSpaces(line.substr(commandStart, commandEnd-commandStart));
            if (!commandArgs.empty()) {
                currentCommand.push_back({commandArgs, lineCount});
            }

            xProcessCommand(currentCommand);
            currentCommand.clear();

            commandStart = commandEnd+1;
            previousOpenBrackets = numOpenBrackets;
            commandEnd = s_FindCharOutsideComment(';',line.substr(commandStart), numOpenBrackets);
            if (previousOpenBrackets == 0 &&
                numOpenBrackets > 0) {
                commentStartLine = lineCount;
            }
        }

        if (commandStart < line.size()) {
            currentCommand.push_back({NStr::TruncateSpaces(line.substr(commandStart)), lineCount});
        }
        commandStart = 0;
    }

    if (numOpenBrackets > 0) {
        string description = ErrorPrintf(
                "Unterminated comment beginning on line %d",
                commentStartLine);
        throw SShowStopper(
                commentStartLine,
                EAlnSubcode::eAlnSubcode_UnterminatedComment,
                description);
    }
}

/*
//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xImportAlignmentData(
    CSequenceInfo& sequenceInfo,
    CLineInput& iStr)
//  ----------------------------------------------------------------------------
{
    string line;
    int lineCount(0);

    int dataLineCount(0);
    int blockLineLength(0);
    int sequenceCharCount(0);
    int unmatchedLeftBracketCount(0);
    int commentStartLine(-1);

    while (iStr.ReadLine(line, lineCount)) {

        NStr::TruncateSpacesInPlace(line);
        if (line.empty()) {
            continue;
        }

        if (mState == EState::SKIPPING) {
            NStr::ToLower(line);
            if (NStr::StartsWith(line, "matrix")) {
                mState = EState::DATA;
                continue;
            }
            if (NStr::StartsWith(line, "dimensions")) {
                xProcessDimensionLine(line, lineCount);
                continue;
            }
            if (NStr::StartsWith(line, "format")) {
                xProcessFormatLine(line, lineCount);
                continue;
            }
            if (NStr::StartsWith(line, "sequin")) {
                mState = EState::DEFLINES;
                continue;
            }
            continue;
        }
        if (mState == EState::DEFLINES) {
            xProcessDefinitionLine(line, lineCount);
            continue;
        }
        if (mState == EState::DATA) {
            if (mNumSequences == 0  ||  mSequenceSize == 0) {
                //error: data before info necessary to interpret it
            }
            auto lineStrLower(line);
            auto previousBracketCount = unmatchedLeftBracketCount;
            sStripNexusComments(line, unmatchedLeftBracketCount);
            if (previousBracketCount == 0 &&
                unmatchedLeftBracketCount == 1) {
                commentStartLine = lineCount;
            }

            if (line.empty()) {
                continue;
            }
            NStr::ToLower(lineStrLower);

            if (NStr::StartsWith(lineStrLower, "end;")) {
                theErrorReporter->Warn(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataLine,
                    "Unexpected \"end;\". Appending \';\' to prior command");
                mState = EState::SKIPPING;
                continue;
            }

            bool isLastLineOfData = NStr::EndsWith(line, ";");
            if (isLastLineOfData) {
                line = line.substr(0, line.size() -1 );
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
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataLine,
                    "In data line, expected seqID followed by sequence data"); 
            }

            string seqId = tokens[0];
            if (dataLineCount < mNumSequences) {
                if (std::find(mSeqIds.begin(), mSeqIds.end(), seqId) != mSeqIds.end()) {
                    return; //ERROR: duplicate ID
                }
                mSeqIds.push_back(seqId);
                mSequences.push_back(vector<TLineInfo>());
            }
            else {
                if (mSeqIds[dataLineCount % mNumSequences] != seqId) {
                    string description;
                    if (std::find(mSeqIds.begin(), mSeqIds.end(), seqId) == mSeqIds.end()) {
                        description = ErrorPrintf(
                            "Expected %d sequences, but finding data for another.",
                            mNumSequences);
                    }
                    else {
                        description = ErrorPrintf(
                            "Finding data for sequence \"%s\" out of order.",
                            seqId.c_str());
                    }
                    throw SShowStopper(
                        lineCount,
                        EAlnSubcode::eAlnSubcode_BadSequenceCount,
                        description);
                }
            }

            string seqData = NStr::Join(tokens.begin()+1, tokens.end(), "");
            auto dataSize = seqData.size();
            auto dataIndex = dataLineCount % mNumSequences;
            if (dataIndex == 0) {
                sequenceCharCount += dataSize;
                if (sequenceCharCount > mSequenceSize) {
                    string description = ErrorPrintf(
                        "Expected %d symbols per sequence but finding already %d",
                        mSequenceSize,
                        sequenceCharCount);
                    throw SShowStopper(
                        lineCount,
                        EAlnSubcode::eAlnSubcode_BadDataCount,
                        description); 
                }
                blockLineLength = dataSize;
            }
            else {
                if (dataSize != blockLineLength) {
                    string description = ErrorPrintf(
                        "In data line, expected %d symbols but finding %d",
                        blockLineLength,
                        dataSize);
                    throw SShowStopper(
                        lineCount,
                        EAlnSubcode::eAlnSubcode_BadDataCount,
                        description); 
                }
            }

            mSequences[dataIndex].push_back({seqData, lineCount});

            dataLineCount += 1;
            if (isLastLineOfData) {
                mState = EState::SKIPPING;
            }
            continue;
        }
    }

    if (unmatchedLeftBracketCount>0) {
        string description = ErrorPrintf(
                "Unterminated comment beginning on line %d",
                commentStartLine);
        throw SShowStopper(
                commentStartLine,
                EAlnSubcode::eAlnSubcode_UnterminatedComment,
                description);
    }

    //submit collected data to a final sanity check:
    if (sequenceCharCount != mSequenceSize) {
        string description = ErrorPrintf(
            "Expected %d symbols per sequence but finding only %d",
            mSequenceSize,
            sequenceCharCount);
        throw SShowStopper(
            -1,
            EAlnSubcode::eAlnSubcode_BadDataCount,
            description); 
    }
}
*/

//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xVerifySingleSequenceData(
    const CSequenceInfo& sequenceInfo,
    const string& seqId,
    const vector<TLineInfo> lineInfos)
//  -----------------------------------------------------------------------------
{
    const char* errTempl("Bad character [%c] found at data position %d.");

    const string& alphabet = sequenceInfo.Alphabet();
    string legalAnywhere = alphabet;
    if (mGapChar != 0) {
        legalAnywhere += mGapChar;
    }
    else {
        legalAnywhere += sequenceInfo.MiddleGap();
    }
    if (mMatchChar != 0) {
        legalAnywhere += mMatchChar;
    }
    else {
        legalAnywhere += sequenceInfo.Match();
    }
    if (mMissingChar != 0) {
        legalAnywhere += mMissingChar;
    }
    else {
        legalAnywhere += sequenceInfo.Missing();
    }

    for (auto lineInfo: lineInfos) {
        if (lineInfo.mData.empty()) {
            continue;
        }
        string seqData(lineInfo.mData);
        auto illegalChar = seqData.find_first_not_of(legalAnywhere);
        if (illegalChar != string::npos) {
            string description = ErrorPrintf(
                errTempl, seqData[illegalChar], illegalChar);
            throw SShowStopper(
                lineInfo.mNumLine,
                EAlnSubcode::eAlnSubcode_BadDataChars,
                description,
                seqId);
        }
    }
}

//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xProcessDimensionLine(
    const string& line,
    int lineCount)
//  ----------------------------------------------------------------------------
{
    const string prefixNtax("ntax=");
    const string prefixNchar("nchar=");
    list<string> tokens;
    NStr::Split(line, " \t;", tokens, NStr::fSplit_MergeDelimiters);
    tokens.pop_front();
    for (auto token: tokens) {
        if (NStr::StartsWith(token, prefixNtax)) {
            try {
                mNumSequences = NStr::StringToInt(token.substr(prefixNtax.size()));
            }
            catch(...) {
                string description = ErrorPrintf("Invalid nTax setting \"%s\"", 
                    token.substr(prefixNtax.size()).c_str());
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataDescription,
                    description);
            }
            continue;
        }
        if (NStr::StartsWith(token, prefixNchar)) {
            try {
                mSequenceSize = NStr::StringToInt(token.substr(prefixNchar.size()));
            }
            catch(...) {
                string description = ErrorPrintf("Invalid nChar setting \"%s\"", 
                    token.substr(prefixNchar.size()).c_str());
                throw SShowStopper(
                    lineCount,
                    EAlnSubcode::eAlnSubcode_IllegalDataDescription,
                    description);
            }
            continue;
        }
    }
}

//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xProcessFormatLine(
    const string& line,
    int lineCount)
//  ----------------------------------------------------------------------------
{
    const string prefixMissing("missing=");
    const string prefixGap("gap=");
    const string prefixMatch("matchchar=");
    list<string> tokens;
    NStr::Split(line, " \t;", tokens, NStr::fSplit_MergeDelimiters);
    tokens.pop_front();
    for (auto token: tokens) {
        if (NStr::StartsWith(token, prefixMissing)) {
            mMissingChar = token[prefixMissing.size()];
            continue;
        }
        if (NStr::StartsWith(token, prefixGap)) {
            mGapChar = token[prefixGap.size()];
            continue;
        }
        if (NStr::StartsWith(token, prefixMatch)) {
            mMatchChar = token[prefixMatch.size()];
            continue;
        }
    }
}

//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xAdjustSequenceInfo(
    CSequenceInfo& sequenceInfo)
//  ----------------------------------------------------------------------------
{
    if (mGapChar != 0) {
        sequenceInfo
            .SetBeginningGap(mGapChar).SetMiddleGap(mGapChar).SetEndGap(mGapChar);
    }
    if (mMatchChar != 0) {
        sequenceInfo.SetMatch(mMatchChar);
    }
    if (mMissingChar != 0) {
        sequenceInfo.SetMissing(mMissingChar);
    }
    
}

//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xProcessDefinitionLine(
    const string& line,
    int lineCount)
//  ----------------------------------------------------------------------------
{
    if (NStr::StartsWith(line, ">")) {
        string defLine = line.substr(1);
        NStr::TruncateSpacesInPlace(defLine);
        mDeflines.push_back({defLine, lineCount});
        return;
    }
    string lineLowerCase(line);
    NStr::ToLower(lineLowerCase);
    if (NStr::StartsWith(lineLowerCase, "end")) {
        mState = EState::SKIPPING;
        return;
    }
    string description(
        "Illegal definition line in NEXUS sequin command");
    theErrorReporter->Error(
        lineCount,
        eAlnSubcode_IllegalDefinitionLine,
        description);
}

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
            }
            --numUnmatchedLeftBrackets;
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
CAlnScannerNexus::sStripNexusComments(
    string& line, 
    int &numUnmatchedLeftBrackets,
    bool &inCommand)
//  ----------------------------------------------------------------------------
{
    if (NStr::IsBlank(line)) {
        return;
    }

    list<pair<int, int>> commentLimits;
    int index=0;
    int start=0;
    int stop;

    if (!inCommand &&
        (numUnmatchedLeftBrackets == 0) &&
        line[0] != '[') {
        inCommand = true;
    }


    while (index < line.size()) {
        const auto& c = line[index];
        if (c == '[' && !inCommand) {
            ++numUnmatchedLeftBrackets;
            if (numUnmatchedLeftBrackets==1) {
                start = index;
            }
        }
        else 
        if (c == ']' && !inCommand) {
            if (numUnmatchedLeftBrackets==1) {
                stop = index;
                if (!inCommand) {
                    commentLimits.push_back(make_pair(start, stop));
                }
                --numUnmatchedLeftBrackets;
            }
        }
        else 
        if (c == ';' &&
            numUnmatchedLeftBrackets == 0) {
            inCommand = false;
        }
        else 
        if (!inCommand && 
            numUnmatchedLeftBrackets == 0 &&
            !isspace(c)) {
           inCommand = true; 
        }
        ++index;
    }

    if (numUnmatchedLeftBrackets && !inCommand) {
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
CAlnScannerNexus::sStripNexusComments(
    TCommand& command)
//  ----------------------------------------------------------------------------
{
    int numUnmatchedLeftBrackets = 0;
    auto it = command.begin();
    while (it != command.end()) {
        sStripNexusComments(it->mData, numUnmatchedLeftBrackets);
        if (it->mData.empty()) {
            it = command.erase(it);
            continue;
        }
        ++it;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
