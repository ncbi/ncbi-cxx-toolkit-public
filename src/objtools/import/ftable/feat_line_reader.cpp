/*  $Id$
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
 * Author: Frank Ludwig
 *
 * File Description:  Iterate through file names matching a given glob pattern
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>

#include <objtools/import/import_error.hpp>
#include "feat_line_reader.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
CFeatLineReader::CFeatLineReader(
    CImportMessageHandler& errorReporter):
//  ============================================================================
    mErrorReporter(errorReporter),
    mLineCount(0),
    mRecordCount(0),
    mProgressFreq(0),
    mLastProgress(0)
{
}

//  ============================================================================
void
CFeatLineReader::SetInputStream(
    CNcbiIstream& istr,
    bool force)
//  ============================================================================
{
    mRecordCount = 0;
}

//  ============================================================================
unsigned int
CFeatLineReader::LineCount() const
//  ============================================================================
{
    return mLineCount;
}

//  ============================================================================
bool
CFeatLineReader::xIgnoreLine(
    const string& line) const
//  ============================================================================
{
    if (line.empty()) {
        return true;
    }
    if (NStr::StartsWith(line, "#")) {
        return true;
    }
    return false;
}

//  ============================================================================
void
CFeatLineReader::xReportProgress()
//  ============================================================================
{
    if (0 == mProgressFreq) { //don't
        return;
    }
    if (LineCount() < mLastProgress + mProgressFreq) { //don't yet
        return;
    }
    mLastProgress += mProgressFreq;
    mErrorReporter.ReportProgress(
        CImportProgress(RecordCount(), LineCount()));
}
