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
#include "aln_scanner_multalign.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ----------------------------------------------------------------------------
void
CAlnScannerMultAlign::xImportAlignmentData(
    CLineInput& iStr)
//  ----------------------------------------------------------------------------
{
    string line;
    int lineCount(0);

    bool processingData = true;
    bool inFirstBlock = false;
    int lineInBlock = 0;
    string refSeqData;

    while (iStr.ReadLine(line)) {
        ++lineCount;
    }
}

//  ----------------------------------------------------------------------------
bool
CAlnScannerMultAlign::xIsOffsetsLine(
    const string& line)
//  ----------------------------------------------------------------------------
{
    list<string> tokens;
    NStr::Split(line, " ", tokens, NStr::fSplit_MergeDelimiters);
    if (tokens.front().empty()) {
        tokens.pop_front();
    }
    if (tokens.back().empty()) {
        tokens.pop_back();
    }
    return false;
}

END_SCOPE(objects)
END_NCBI_SCOPE
