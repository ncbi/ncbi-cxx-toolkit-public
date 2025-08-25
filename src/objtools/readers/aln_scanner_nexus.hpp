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

//  ============================================================================
class CAlnScannerNexus : public CAlnScanner
//  ============================================================================
{
public:
    TDeflines& SetDeflines(void) { return mDeflines; }

private:
    struct SNexusCommand {
        using TArgs = list<SLineInfo>;
        string name;
        int    startLineNum = -1;
        TArgs  args;
    };

    using TCommand       = SNexusCommand;
    using TCommandArgs   = TCommand::TArgs;
    using TCommandTokens = TCommandArgs;

    void
    xImportAlignmentData(
        CLineInput&) override;

    virtual void
    xAdjustSequenceInfo(
        CSequenceInfo&) override;

    void
    xProcessCommand(const TCommandTokens& commandTokens);

    void
    xProcessDimensions(const TCommandArgs& args);

    void
    xProcessFormat(const TCommandArgs& args);

    void
    xProcessSequin(const TCommandArgs& args);

    void
    xProcessMatrix(const TCommandArgs& args);

    void xProcessNCBIBlockCommand(TCommand& command);

    void xProcessDataBlockCommand(TCommand& command);

    void xProcessTaxaBlockCommand(TCommand& command);

    void
    xBeginBlock(const TCommandArgs& command);

    void
    xEndBlock(int lineNum);

    bool
    xUnexpectedEndBlock(TCommand& command);

    pair<TCommandArgs::const_iterator, size_t>
    xGetArgPos(const TCommandArgs& args,
               const string&       token) const;

    pair<string, int>
    xGetKeyVal(const TCommandArgs& command,
               const string&       key);

    static void sStripCommentsOutsideCommand(
        string& line,
        int&    numUnmatchedLeftBrackets,
        bool&   inCommand);

    static size_t sFindCharOutsideComment(
        char          c,
        const string& line,
        int&          numUnmatchedLeftBrackets,
        size_t        startPos = 0);


    static void sStripNexusCommentsFromCommand(
        TCommandArgs& command);

    size_t mNumSequences = 0;
    size_t mSequenceSize = 0;
    char   mMatchChar    = 0;
    char   mMissingChar  = 0;
    char   mGapChar      = 0;
    bool   mInBlock      = false;
    string mCurrentBlock{};
    int    mBlockStartLine = 0;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _ALN_SCANNER_NEXUS_HPP_
