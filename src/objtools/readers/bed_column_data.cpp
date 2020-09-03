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
#include "bed_column_data.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

//  ============================================================================
CBedColumnData::CBedColumnData(
    const CReaderBase::TReaderLine& lineData,
    int flags):
//  ============================================================================
    mLineNo(lineData.mLine),
    mColumnSeparator(""),
    mColumnSplitFlags(0)
{
    xSplitColumns(lineData.mData);
    xCleanColumnValues();
    if (flags & CBedReader::EBedFlags::fAddDefaultColumns) {
        xAddDefaultColumns();
    }
}

//  =============================================================================
const string&
CBedColumnData::operator[](
    size_t index) const
//  =============================================================================
{
    return mData[index];
}

//  ----------------------------------------------------------------------------
void CBedColumnData::xSplitColumns(
    const string& line)
//  ----------------------------------------------------------------------------
{
    bool splitSuccessful = false;
    if (mColumnSeparator.empty()) {
        mData.clear();
        mColumnSeparator = "\t";
        NStr::Split(line, mColumnSeparator, mData, mColumnSplitFlags);
        if (mData.size() > 2) {
            splitSuccessful = true;
        }
        else {
            mColumnSeparator = " \t";
            mColumnSplitFlags = NStr::fSplit_MergeDelimiters;
        }
    }
    if (!splitSuccessful) {
        mData.clear();
        NStr::Split(line, mColumnSeparator, mData, mColumnSplitFlags);
        if (mData.size() > 2) {
            splitSuccessful = true;
        }
    }
    if (!splitSuccessful) {
        CReaderMessage error(
            eDiag_Error,
            0,
            "Invalid data line: Unable to split data line into data columns");
        throw error;
    }
    for (auto& column: mData) {
        NStr::TruncateSpacesInPlace(column);
    }
}

//  ----------------------------------------------------------------------------
void
CBedColumnData::xCleanColumnValues()
//  ----------------------------------------------------------------------------
{
    string fixup;

    if (NStr::EqualNocase(mData[0], "chr")  &&  mData.size() > 1) {
        mData[1] = mData[0] + mData[1];
        mData.erase(mData.begin());
    }
    if (mData.size() < 3) {
        CReaderMessage error(
            eDiag_Error,
            0,
            "Invalid data line: Insufficient column count.");
        throw error;
    }

    try {
        NStr::Replace(mData[1], ",", "", fixup);
        mData[1] = fixup;
    }
    catch(std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            0,
            "Invalid data line: Invalid \"SeqStart\" (column 2) value.");
        throw error;
    }

    try {
        NStr::Replace(mData[2], ",", "", fixup);
        mData[2] = fixup;
    }
    catch(std::exception&) {
        CReaderMessage error(
            eDiag_Error,
            0,
            "Invalid data line: Invalid \"SeqStop\" (column 3) value.");
        throw error;
    }
}

//  ----------------------------------------------------------------------------
void 
CBedColumnData::xAddDefaultColumns()
//  ----------------------------------------------------------------------------
{
    auto columnCount = mData.size();
    if (columnCount > 4  &&  mData[4].empty()) {
        mData[4] = "0";
    }
    if (columnCount > 5  &&  mData[5].empty()) {
        mData[5] = ".";
    }
    if (columnCount > 6  &&  mData[6].empty()) {
        mData[6] = mData[1];
    }
    if (columnCount > 7  &&  mData[7].empty()) {
        mData[7] = mData[2];
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
