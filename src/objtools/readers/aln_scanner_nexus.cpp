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
#include "aln_util.hpp"

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
    TCommand command,
    CSequenceInfo& sequenceInfo) 
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
                EAlnSubcode::eAlnSubcode_UnterminatedCommand,
                "Unexpected \"end;\". Appending \';\' to prior command");

            xEndBlock();
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
        xAdjustSequenceInfo(sequenceInfo);
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
        xBeginBlock(command);
        return;
    }

    if (commandName == "end") {
        xEndBlock();
        return;
    }
    
}


//  ----------------------------------------------------------------------------
void 
CAlnScannerNexus::xBeginBlock(
    const TCommand& command)
//  ----------------------------------------------------------------------------
{
    auto lineNum = command.front().mNumLine;
    if (mInBlock) {
        auto description = ErrorPrintf(
                "Attempting to enter a new block, but still in the %s block that begins on line %d.",
                mCurrentBlock.c_str(), mBlockStartLine);

            throw SShowStopper(
                lineNum,
                EAlnSubcode::eAlnSubcode_IllegalDataLine,
                description);

    }
    mInBlock = true;
    mBlockStartLine = lineNum;
    if (command.size() > 1) {
        mCurrentBlock = next(command.begin())->mData;    
    }
    else {
        string commandName;
        NStr::SplitInTwo(command.front().mData, " \t", 
                commandName,
                mCurrentBlock);
    }
}


//  ----------------------------------------------------------------------------
void 
CAlnScannerNexus::xEndBlock()
//  ----------------------------------------------------------------------------
{
    mInBlock = false;
    mBlockStartLine = -1;
    mCurrentBlock.clear();
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
    ++cit;

    if (!mNumSequences) {
        for (; cit != command.end(); ++dataLineCount, ++cit) {
            vector<string> tokens;
            NStr::Split(cit->mData, " \t", tokens, NStr::fSplit_Tokenize);
            if (tokens.size() < 2) {
                throw SShowStopper(
                    cit->mNumLine,
                    eAlnSubcode_IllegalDataLine,
                    "In data line, expected seqID followed by sequence data");
            }

            string seqId = tokens[0];
            if (!mSeqIds.empty() &&
                    seqId == mSeqIds[0]) {
                mNumSequences = mSeqIds.size();
                break;
            }

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

        
            string seqData = NStr::Join(tokens.begin()+1, tokens.end(), "");
            auto dataSize = seqData.size();
        
            if (dataLineCount == 0) {
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
            mSequences[dataLineCount].push_back({tokens[1], cit->mNumLine});
        }
    }

    for(; cit != command.end(); ++dataLineCount, ++cit) {

        vector<string> tokens;
        NStr::Split(cit->mData, " \t", tokens, NStr::fSplit_Tokenize);
        if (tokens.size() < 2) {
            throw SShowStopper(
                cit->mNumLine,
                eAlnSubcode_IllegalDataLine,
                "In data line, expected seqID followed by sequence data");
        }

        string seqId = tokens[0];
        seqCount = dataLineCount % mNumSequences;

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
    if (command.size() <= 1) {
        return;
    }   

    string dummy;
    string defline;
    for (auto it = next(command.begin());
            it != command.end(); 
            ++it) {
        const auto lineInfo = *it;
        try {
            AlnUtil::ProcessDefline(lineInfo.mData, dummy, defline);
        }
        catch (SShowStopper& showStopper) {
            showStopper.mLineNumber = lineInfo.mNumLine;
            throw;
        }

        if (!dummy.empty()) {
            throw SShowStopper(
                lineInfo.mNumLine,
                eAlnSubcode_IllegalDefinitionLine,
                "Invalid NEXUS definition line, expected \">\" followed by mods.");
        }
        mDeflines.push_back({defline, lineInfo.mNumLine});
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

 
//  ----------------------------------------------------------------------------
int 
CAlnScannerNexus::sFindCharOutsideComment(
        char c,
        const string& line,
        int &numUnmatchedLeftBrackets,
        size_t startPos)
//  ----------------------------------------------------------------------------
{
    for (int index=startPos; index<line.size(); ++index) {
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
    size_t commandStart(0);
    int numOpenBrackets(0);
    size_t commentStartLine(-1);
    bool inCommand(false);


    while (iStr.ReadLine(line, lineCount)) {

        NStr::TruncateSpacesInPlace(line);

        string lineStrLower(line);
        NStr::ToLower(lineStrLower);
        if (lineStrLower == "#nexus") { // Should be an error if the first word isn't #NEXUS
            continue;
        }

        int previousOpenBrackets = numOpenBrackets;
        sStripCommentsOutsideCommand(line, numOpenBrackets, inCommand);
        if (previousOpenBrackets == 0 &&
            numOpenBrackets > 0) {
            commentStartLine = lineCount;
        }

        if (line.empty()) {
            continue;
        }

        previousOpenBrackets = numOpenBrackets;
        commandEnd = sFindCharOutsideComment(';', line, numOpenBrackets);
        if (previousOpenBrackets == 0 &&
            numOpenBrackets > 0) {
            commentStartLine = lineCount;
        }


        while (commandEnd != NPOS) {
            string commandTokens = 
                NStr::TruncateSpaces(line.substr(commandStart, commandEnd-commandStart));
            if (!commandTokens.empty()) {
                currentCommand.push_back({commandTokens, lineCount});
            }

            xProcessCommand(currentCommand, sequenceInfo);
            currentCommand.clear();

            commandStart = commandEnd+1;
            previousOpenBrackets = numOpenBrackets;
            commandEnd = 
                sFindCharOutsideComment(';',line, numOpenBrackets, commandStart);
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

    if (!currentCommand.empty()) {
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

    if (!currentCommand.empty()) {
        auto commandStartLine =  currentCommand.front().mNumLine;
        string description = 
           (commandStartLine == lineCount) ?
           ErrorPrintf(
                "Unterminated command on line %d",
                commandStartLine)
           :
            ErrorPrintf(
                "Unterminated command beginning on line %d",
                commandStartLine);
        throw SShowStopper(
            lineCount,
            EAlnSubcode::eAlnSubcode_UnterminatedCommand,
            description);
    }
}


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
CAlnScannerNexus::sStripCommentsOutsideCommand(
    string& line, 
    int &numUnmatchedLeftBrackets,
    bool &inCommand)
//  ----------------------------------------------------------------------------
{
    if (NStr::IsBlank(line)) {
        return;
    }

    list<pair<int, int>> commentLimits;
    int start=0;
    int stop;

    if (!inCommand &&
        (numUnmatchedLeftBrackets == 0) &&
        line[0] != '[') {
        inCommand = true;
    }

    const auto len = line.size();

    for (int index=0; index<len; ++index) {
        const auto& c = line[index];

        if (inCommand) {
            if (c == ';' &&
                numUnmatchedLeftBrackets==0) {
                inCommand = false;        
            }
            continue;
        }

        // else inCommand = false;
        if (c == '[') {
            ++numUnmatchedLeftBrackets;
            if (numUnmatchedLeftBrackets==1) {
                start = index;
            }
        }
        else 
        if (c == ']') {
            --numUnmatchedLeftBrackets;
            if (numUnmatchedLeftBrackets==0) {
                stop = index;
                commentLimits.push_back(make_pair(start, stop));
            }
        }
        else
        if (numUnmatchedLeftBrackets == 0 &&
            !isspace(c)) {
           inCommand = true; 
        }
    }

    if (numUnmatchedLeftBrackets && !inCommand) {
        commentLimits.push_back(make_pair(start, len-1));
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
