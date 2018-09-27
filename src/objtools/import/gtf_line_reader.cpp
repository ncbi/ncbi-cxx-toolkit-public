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

#include <objtools/import/feat_import_error.hpp>
#include "gtf_line_reader.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
CGtfLineReader::CGtfLineReader(
    CNcbiIstream& istr):
//  ============================================================================
    CFeatLineReader(istr),
    mColumnDelimiter(""),
    mSplitFlags(0)
{
}

//  ============================================================================
bool
CGtfLineReader::GetNextRecord(
    CFeatImportData& record)
//  ============================================================================
{
    string nextLine = "";

    while (!AtEOF()) {
        nextLine = *(++(*this));
        ++mLineNumber;
        if (xIgnoreLine(nextLine)) {
            continue;
        }
        vector<string> columns;
        xSplitLine(nextLine, columns);
        record.InitializeFrom(columns);
        ++mRecordNumber;
        return true;
    }
    return false;
}

//  ============================================================================
void
CGtfLineReader::xSplitLine(
    const string& line,
    vector<string>& columns)
//  ============================================================================
{
    CFeatureImportError errorInvalidColumnCount(
        CFeatureImportError::CRITICAL, "Invalid column count");

    columns.clear();
    if (mColumnDelimiter.empty()) {
        mColumnDelimiter = "\t";
        mSplitFlags = 0;
        NStr::Split(line, mColumnDelimiter, columns, mSplitFlags);
        if (columns.size() == 9) {
            return;
        }
        columns.clear();
        mColumnDelimiter = " \t";
        mSplitFlags = NStr::fSplit_MergeDelimiters;
    }
    NStr::Split(line, mColumnDelimiter, columns, mSplitFlags);
    if (columns.size() == 9) {
        return;
    }
    if (mColumnDelimiter == " \t"  &&  columns.size() > 9) {
        vector<string> attributes(columns.begin() + 8, columns.end());
        columns[8] = NStr::Join(attributes, " ");
        columns.erase(columns.begin() + 9, columns.end());
        return;
    }
    throw errorInvalidColumnCount;
}
