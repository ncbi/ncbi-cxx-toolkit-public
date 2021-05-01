#ifndef _ALN_SCANNER_NEXUS_HPP_
#define _ALN_SCANNER_NEXUS_HPP_

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
#include <corelib/ncbistd.hpp>
#include "aln_scanner.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

struct SAlignFileRaw;

//  ============================================================================
struct SNexusCommand
//  ============================================================================
{
    using TArgs = list<SLineInfo>;
    string name;
    int startLineNum=-1;
    TArgs args;
};


//  ============================================================================
class CAlnScannerNexus:
    public CAlnScanner
//  ============================================================================
{
public:
//    CAlnScannerNexus():
//        mGapChar(0), mMissingChar(0), mMatchChar(0) {};
    ~CAlnScannerNexus() {};

    TDeflines& SetDeflines(void) { return mDeflines; }

    using TCommand = SNexusCommand;
    using TCommandArgs = TCommand::TArgs;
    using TCommandTokens = TCommandArgs;
protected:

    void
    xImportAlignmentData(
        CSequenceInfo&,
        CLineInput&) override;

    virtual void
    xAdjustSequenceInfo(
        CSequenceInfo&) override;
/*
    virtual void
    xVerifySingleSequenceData(
        const CSequenceInfo&,
        const TLineInfo& seqId,
        const vector<TLineInfo> seqData) override;
    using TCommand = SNexusCommand;
    using TCommandArgs = TCommand::TArgs;
    using TCommandTokens = TCommandArgs;

*/
    void
    xProcessCommand(const TCommandTokens& commandTokens,
            CSequenceInfo& sequenceInfo);

    void
    xProcessDimensions(const TCommandArgs& args);

    void
    xProcessFormat(const TCommandArgs& args);

    void
    xProcessSequin(const TCommandArgs& args);

    void
    xProcessMatrix(const TCommandArgs& args);

    void
    xProcessNCBIBlockCommand(TCommand& command,
            CSequenceInfo& sequenceInfo);

    void
    xProcessDataBlockCommand(TCommand& command,
            CSequenceInfo& sequenceInfo);

    void
    xProcessTaxaBlockCommand(TCommand& command,
            CSequenceInfo& sequenceInfo);

    void
    xBeginBlock(const TCommandArgs& command);

    void
    xEndBlock(int lineNum);

    bool
    xUnexpectedEndBlock(TCommand& command);

    pair<TCommandArgs::const_iterator, size_t>
    xGetArgPos(const TCommandArgs& args,
            const string& token) const;

    pair<string, int>
    xGetKeyVal(const TCommandArgs& command,
        const string& key);

    static void sStripCommentsOutsideCommand(
        string& line,
        int &numUnmatchedLeftBrackets,
        bool &inCommand);

    static size_t sFindCharOutsideComment(
        char c,
        const string& line,
        int &numUnmatchedLeftBrackets,
        size_t startPos=0);


    static void sStripNexusCommentsFromCommand(
        TCommandArgs& command);

    int mNumSequences = 0;
    int mSequenceSize = 0;
    char mMatchChar=0;
    char mMissingChar=0;
    char mGapChar=0;
    bool mInBlock=false;
    string mCurrentBlock;
    int mBlockStartLine;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _ALN_SCANNER_NEXUS_HPP_
