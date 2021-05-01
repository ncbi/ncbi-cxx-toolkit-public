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
 * Authors:
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


// -----------------------------------------------------------------------------
bool
CAlnScannerNexus::xUnexpectedEndBlock(TCommand& command)
// -----------------------------------------------------------------------------
{
    auto& args = command.args;
    string lastLine = args.back().mData;
    auto lastWhiteSpacePos = lastLine.find_last_of(" \t");
    string lastToken = (lastWhiteSpacePos == NPOS) ?
                        lastLine :
                        lastLine.substr(lastWhiteSpacePos);

    string lastTokenLower(lastToken);
    NStr::ToLower(lastTokenLower);

    if (lastTokenLower == "end") {
        const bool onlyArg = (args.size() == 1 &&
                              lastWhiteSpacePos == NPOS);
        if (onlyArg) {
            throw SShowStopper(
                args.back().mNumLine,
                EAlnSubcode::eAlnSubcode_UnexpectedCommandArgs,
                "\"" + lastToken + "\" is not a valid argument for the \"" +
                command.name + "\" command.");
        }


        theErrorReporter->Warn(
        command.args.back().mNumLine,
        EAlnSubcode::eAlnSubcode_UnterminatedCommand,
                "File format autocorrected to comply with Nexus rules. Unexpected \"end;\". Appending \';\' to prior command. No action required.");

        if (lastWhiteSpacePos == NPOS) {
            args.pop_back();
        }
        else {
            args.back().mData = NStr::TruncateSpaces(
                args.back().mData.substr(0, lastWhiteSpacePos));
        }
        return true;
    }
    return false;
}


//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xProcessCommand(
    const TCommandTokens& commandTokens,
    CSequenceInfo& sequenceInfo)
//  ----------------------------------------------------------------------------
{
    SNexusCommand command;
    command.args = commandTokens;

    auto nameEnd =
        commandTokens.front().mData.find_first_of(" \t[");
    if (nameEnd == NPOS) {
        command.name = command.args.front().mData;
        command.args.pop_front();
    }
    else {
        command.name = command.args.front().mData.substr(0, nameEnd);
        command.args.front().mData =
            NStr::TruncateSpaces(command.args.front().mData.substr(nameEnd));
    }
    command.startLineNum = commandTokens.front().mNumLine;

    string nameLower = command.name;
    NStr::ToLower(nameLower);


    if (nameLower == "begin") {
        sStripNexusCommentsFromCommand(command.args);
        const bool applyEnd = xUnexpectedEndBlock(command);
        xBeginBlock(command.args);
        if (applyEnd) {
            xEndBlock(command.args.back().mNumLine);
        }
        return;
    }


    if (!mInBlock) {
        throw SShowStopper(
                command.startLineNum,
                EAlnSubcode::eAlnSubcode_UnexpectedCommand,
                "\"" + command.name + "\" command appears outside of block.");
    }

    string block = mCurrentBlock;
    NStr::ToLower(block);
    if (block == "ncbi") {
        xProcessNCBIBlockCommand(command, sequenceInfo);
        return;
    }

    if (nameLower == "end") {
        if (!command.args.empty()) {
            throw SShowStopper(
                command.startLineNum,
                EAlnSubcode::eAlnSubcode_UnexpectedCommandArgs,
                "\"" + command.name +
                "\" command terminates a block and does not take any arguments.");
        }
        xEndBlock(command.startLineNum);
        return;
    }

    if (block ==  "data" ||
        block == "characters") {
        xProcessDataBlockCommand(command, sequenceInfo);
        return;
    }

    if (block == "taxa") {
        xProcessTaxaBlockCommand(command, sequenceInfo);
        return;
    }
}


//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xProcessDataBlockCommand(
        TCommand& command,
        CSequenceInfo& sequenceInfo)
//  ----------------------------------------------------------------------------
{
    auto nameLower = command.name;
    NStr::ToLower(nameLower);

    sStripNexusCommentsFromCommand(command.args);
    bool unexpectedEnd = xUnexpectedEndBlock(command);

    if (nameLower == "dimensions") {
        xProcessDimensions(command.args);
    }
    else
    if (nameLower == "format") {
        xProcessFormat(command.args);
    }
    else
    if (nameLower == "matrix") {
        xProcessMatrix(command.args);
    }
    // Don't report an error.
    // There are many other possible commands

    if (unexpectedEnd) {
        xEndBlock(command.args.back().mNumLine);
    }
}


//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xProcessTaxaBlockCommand(
        TCommand& command,
        CSequenceInfo& sequenceInfo)
//  ----------------------------------------------------------------------------
{
    auto nameLower = command.name;
    NStr::ToLower(nameLower);

    sStripNexusCommentsFromCommand(command.args);
    bool unexpectedEnd = xUnexpectedEndBlock(command);

    if (nameLower == "dimensions") {
        xProcessDimensions(command.args);
    }
    // Don't report an error.
    // There are other possible commands that we currently ignore,
    // such as taxlabels

    if (unexpectedEnd) {
        xEndBlock(command.args.back().mNumLine);
    }
}


//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xProcessNCBIBlockCommand(
        TCommand& command,
        CSequenceInfo& sequenceInfo)
//  ----------------------------------------------------------------------------
{
    static string previousCommand;

    auto nameLower = command.name;
    NStr::ToLower(nameLower);
    if (nameLower == "end") {
        if (previousCommand != "sequin") {
            auto description =
                "Exiting empty \"NCBI\" block. Expected a \"sequin\" command.";
            theErrorReporter->Error(
                command.startLineNum,
                EAlnSubcode::eAlnSubcode_UnexpectedCommand,
                description);
        }
        previousCommand.clear();
        xEndBlock(command.startLineNum);
        return;
    }


    bool unexpectedEnd = xUnexpectedEndBlock(command);
    if (nameLower == "sequin") {
        xProcessSequin(command.args);
        previousCommand = "sequin";
    }
    else {
        throw SShowStopper(
            command.startLineNum,
            EAlnSubcode::eAlnSubcode_UnexpectedCommand,
            "Unexpected \"" + command.name  + "\" command inside \"NCBI\" block. The \"NCBI\" block must contain a \"sequin\" command and no other commands.");
    }

    if (unexpectedEnd) {
        previousCommand.clear();
        xEndBlock(command.args.back().mNumLine);
    }
}



//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xProcessSequin(
        const TCommandArgs& args)
//  ----------------------------------------------------------------------------
{
    for (auto lineInfo : args) {
        auto line = lineInfo.mData;
        auto lineNum = lineInfo.mNumLine;
        string dummy, defLine;
        try {
            AlnUtil::ProcessDefline(line, dummy, defLine);
        }
        catch (SShowStopper& showStopper) {
            showStopper.mLineNumber = lineNum;
            throw;
        }

        if (!dummy.empty()) {
            string description =
                "The definition lines in the Nexus file are not correctly formatted. "
                "Definition lines are optional, "
                "but if included, must start with \">\" followed by modifiers in square brackets. "
                "The sequences have been imported "
                "but the information in the definition lines will be ignored.";
            throw SShowStopper(
                lineNum,
                EAlnSubcode::eAlnSubcode_IllegalDefinitionLine,
                description);
        }
        mDeflines.push_back({defLine, lineNum});
    }
}



//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xBeginBlock(
    const TCommandArgs& command)
//  ----------------------------------------------------------------------------
{
    auto lineNum = command.front().mNumLine;
    auto newBlockName = command.front().mData;
    if (mInBlock) {
        auto description = ErrorPrintf(
                "Nested blocks detected. New block \"%s\" while still in \"%s\" block. \"%s\" block begins on line %d",
                newBlockName.c_str(),
                mCurrentBlock.c_str(),
                mCurrentBlock.c_str(),
                mBlockStartLine);

            throw SShowStopper(
                lineNum,
                EAlnSubcode::eAlnSubcode_IllegalDataLine,
                description);

    }
    mInBlock = true;
    mBlockStartLine = lineNum;
    mCurrentBlock = newBlockName;
}


//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xEndBlock(int lineNum)
//  ----------------------------------------------------------------------------
{

    if (!mInBlock) {
        auto description = "\"end\" command appears outside of block.";
        throw SShowStopper(
            lineNum,
            EAlnSubcode::eAlnSubcode_IllegalDataLine,
            description);
    }

    mInBlock = false;
    mBlockStartLine = -1;
    mCurrentBlock.clear();
}


//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xProcessMatrix(
    const TCommandArgs& command)
//  ----------------------------------------------------------------------------
{
    int dataLineCount(0);
    int seqCount(0);
    int blockLineLength(0);
    int sequenceCharCount(0);
    int maxSeqCount(0);

    for (auto lineInfo : command) {
        const auto& data = lineInfo.mData;
        const int lineNum = lineInfo.mNumLine;

        vector<string> tokens;
        NStr::Split(data, " \t", tokens, NStr::fSplit_Tokenize);
        if (tokens.size() < 2) {
            string description =
            "Data line does not follow the expected pattern of sequence_ID followed by sequence data. "
            "Each data line should conform to the same expected pattern.";
            throw SShowStopper(
                lineNum,
                eAlnSubcode_IllegalDataLine,
                description);
        }

        const string& seqId = tokens[0];
        if (mNumSequences == 0 &&
            !mSeqIds.empty() &&
            seqId == mSeqIds[0].mData) {
            mNumSequences = mSeqIds.size();
        }

        const bool inFirstBlock = ((mNumSequences == 0) || (dataLineCount < mNumSequences));
        seqCount = inFirstBlock ? dataLineCount : (dataLineCount % mNumSequences);
        maxSeqCount = max(seqCount, maxSeqCount);


        AlnUtil::CheckId(seqId, mSeqIds, seqCount, lineNum, inFirstBlock);

        if  (inFirstBlock) {
            mSeqIds.push_back({seqId, lineNum});
            mSequences.push_back(vector<TLineInfo>());
        }

        string seqData = NStr::Join(tokens.begin()+1, tokens.end(), "");
        const int dataSize = seqData.size();


        if (seqCount == 0) {
            sequenceCharCount += dataSize;
            if (sequenceCharCount > mSequenceSize) {
                string description =
                    ErrorPrintf(
                        "The expected number of characters per sequence specified by nChar in the Nexus file is %d. "
                        "The actual number of characters counted for the first sequence is %d. "
                        "The expected number of characters must equal the actual number of characters.",
                        mSequenceSize,
                        sequenceCharCount);
                throw SShowStopper(
                    lineNum,
                    EAlnSubcode::eAlnSubcode_BadDataCount,
                    description);
            }
            blockLineLength = dataSize;
        }
        else
        if (dataSize != blockLineLength) {
            string description =
                BadCharCountPrintf(blockLineLength,dataSize);
            throw SShowStopper(
                lineNum,
                EAlnSubcode::eAlnSubcode_BadDataCount,
                description);
        }
        mSequences[seqCount].push_back({seqData, lineNum});

        ++dataLineCount;
    }


    if (maxSeqCount != mNumSequences-1) {
        string description =
            ErrorPrintf(
                "The expected number of sequences specified by nTax in the Nexus file is %d. "
                "The actual number of sequences encountered is %d. "
                "The number of sequences in the file must equal the expected number of sequences.",
                mNumSequences,
                maxSeqCount+1);
        throw SShowStopper(
            -1,
            EAlnSubcode::eAlnSubcode_BadSequenceCount,
            description);
    }

    if (seqCount != maxSeqCount) {
        string description =
            ErrorPrintf(
                "The final sequence block in the Nexus file is incomplete. "
                "It contains data for just %d sequences, but %d sequences are expected.",
                seqCount+1, mNumSequences);
        throw SShowStopper(
            -1,
            EAlnSubcode::eAlnSubcode_BadSequenceCount,
            description);
    }

    if (sequenceCharCount != mSequenceSize) {
        string description =
            ErrorPrintf(
                "The expected number of characters per sequence specified by nChar in the Nexus file is %d. "
                "The actual number of characters counted for the first sequence is %d. "
                "The expected number of characters must equal the actual number of characters.",
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
    const TCommandArgs& args)
//  ----------------------------------------------------------------------------
{
   if (NStr::EqualNocase(mCurrentBlock, "characters")) {
       auto ntaxPos = xGetArgPos(args, "ntax");

       // If "ntax" appears in a line,
       // check that it is immediately preceded by "newtaxa".
       // "newtaxa" could be on the same line as "ntax" or at the
       // end of the preceding line.
       if (ntaxPos.second != string::npos) {
           string ntaxSubStr;
           size_t ntaxLinePos = ntaxPos.second;
           if (ntaxLinePos == 0 &&
               ntaxPos.first != args.begin()) {
                ntaxSubStr = prev(ntaxPos.first)->mData;
                ntaxLinePos += ntaxSubStr.size();
           }

           bool foundError = true;
           const auto litLength = strlen("newtaxa");
           if (ntaxLinePos > litLength) {
               ntaxSubStr += ntaxPos.first->mData;
               auto endOfPreviousToken = ntaxSubStr.find_last_not_of(" \t", ntaxLinePos-1);
               if (endOfPreviousToken != NPOS &&
                   endOfPreviousToken >= (litLength-1) &&
                   NStr::EqualNocase(
                       ntaxSubStr.substr(
                           endOfPreviousToken-(litLength-1), litLength), "newtaxa")) {
                   foundError = false;
               }
           }

            if (foundError) {
                throw SShowStopper(
                        ntaxPos.first->mNumLine,
                        EAlnSubcode::eAlnSubcode_UnexpectedCommandArgs,
                        "Invalid command arguments. \"nTax\" must be immediately preceded by \"newtaxa\" in \"" +
                        mCurrentBlock +
                        "\" block.");
            }
        }
    }



    auto ntax = xGetKeyVal(args, "ntax");
    if (!ntax.first.empty()) {
        try {
            mNumSequences = NStr::StringToInt(ntax.first);
        }
        catch(...) {
            string description =
                ErrorPrintf("Nexus file has invalid nTax setting: \"%s\". nTax must be an integer.",
                    ntax.first.c_str());
                throw SShowStopper(
                    ntax.second,
                    EAlnSubcode::eAlnSubcode_IllegalDataDescription,
                    description);
        }
    }

    auto nchar = xGetKeyVal(args, "nchar");
    if (!nchar.first.empty()) {
        try {
            mSequenceSize = NStr::StringToInt(nchar.first);
        }
        catch(...) {
            string description =
                ErrorPrintf("Nexus file has invalid nChar setting: \"%s\". nChar must be an integer.",
                        nchar.first.c_str());
                    throw SShowStopper(
                        nchar.second,
                        EAlnSubcode::eAlnSubcode_IllegalDataDescription,
                        description);
        }
    }
}


//  ----------------------------------------------------------------------------
void
CAlnScannerNexus::xProcessFormat(
    const TCommandArgs& command)
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
pair<string, int>
CAlnScannerNexus::xGetKeyVal(
    const TCommandArgs& command,
    const string& key)
//  ----------------------------------------------------------------------------
{
    auto keyPos = NPOS;
    auto valPos = NPOS;
    int keyLine = 0;
    size_t endPos = 0;

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
pair<CAlnScannerNexus::TCommandArgs::const_iterator, size_t>
CAlnScannerNexus::xGetArgPos(const TCommandArgs& args,
        const string& token) const
//  ----------------------------------------------------------------------------
{
    for (auto it = args.cbegin(); it != args.cend(); ++it) {
        string line(it->mData);
        NStr::ToLower(line);
        size_t pos = line.find(token);
        if (pos != string::npos) {
            return make_pair(it, pos);
        }
    }

    return make_pair(args.cend(), string::npos);
}


//  ----------------------------------------------------------------------------
size_t
CAlnScannerNexus::sFindCharOutsideComment(
        char c,
        const string& line,
        int &numUnmatchedLeftBrackets,
        size_t startPos)
//  ----------------------------------------------------------------------------
{
    for (auto index=startPos; index<line.size(); ++index) {
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
    TCommandArgs commandTokens;
    size_t commandEnd(0);
    size_t commandStart(0);
    int numOpenBrackets(0);
    size_t commentStartLine(-1);
    bool inCommand(false);
    bool firstToken = true;


    while (iStr.ReadLine(line, lineCount)) {

        NStr::TruncateSpacesInPlace(line);

        string lineStrLower(line);
        NStr::ToLower(lineStrLower);
        if (lineStrLower == "#nexus") {
            if (!firstToken) {
                throw SShowStopper(
                        lineCount,
                        eAlnSubcode_IllegalDataLine,
                        "Unexpected token. \"#NEXUS\" should appear once at the beginnng of the file."
                        );
            }
            firstToken = false;
            continue;
        }
    /*
      // this code is redundant because the file will not be recognised as Nexus anyway.
        else
        if (firstToken) {
            throw SShowStopper(
                    lineCount,
                    eAlnSubcode_IllegalDataLine,
                    "Unexpected line. \"#NEXUS\" should appear at the beginning of the file.");
        }
        */

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
            string commandSubstr =
                NStr::TruncateSpaces(line.substr(commandStart, commandEnd-commandStart));
            if (!commandSubstr.empty()) {
                commandTokens.push_back({commandSubstr, lineCount});
            }
            xProcessCommand(commandTokens, sequenceInfo);
            commandTokens.clear();

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
            commandTokens.push_back({NStr::TruncateSpaces(line.substr(commandStart)), lineCount});
        }
        commandStart = 0;
    }


    if (numOpenBrackets > 0) {
        string description =
                "The beginning of a comment was detected, but it is missing a closing bracket. Add the closing bracket to the end of the comment or correct if it is not a comment.";
        throw SShowStopper(
                commentStartLine,
                EAlnSubcode::eAlnSubcode_UnterminatedComment,
                description);
    }

    if (!commandTokens.empty()) {
        string description =
            "Terminating semicolon missing from command. Commands in a Nexus file must end with a semicolon.";
        throw SShowStopper(
            lineCount,
            EAlnSubcode::eAlnSubcode_UnterminatedCommand,
            description);
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
            .SetBeginningGap(string(1, mGapChar))
            .SetMiddleGap(string(1, mGapChar))
            .SetEndGap(string(1, mGapChar));
    }
    if (mMatchChar != 0) {
        sequenceInfo.SetMatch(string(1, mMatchChar));
    }
    if (mMissingChar != 0) {
        sequenceInfo.SetMissing(string(1, mMissingChar));
    }
}


//  ----------------------------------------------------------------------------
static void
sStripNexusComments(
    string& line,
    int &numUnmatchedLeftBrackets)
//  ----------------------------------------------------------------------------
{
    if (NStr::IsBlank(line)) {
        return;
    }

    list<pair<size_t, size_t>> commentLimits;
    size_t index=0;
    size_t start=0;
    size_t stop;
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

    list<pair<size_t,size_t>> commentLimits;
    size_t start=0;
    size_t stop;

    if (!inCommand &&
        (numUnmatchedLeftBrackets == 0) &&
        line[0] != '[') {
        inCommand = true;
    }

    const auto len = line.size();

    for (size_t index=0; index<len; ++index) {
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
CAlnScannerNexus::sStripNexusCommentsFromCommand(
    TCommandArgs& command)
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
