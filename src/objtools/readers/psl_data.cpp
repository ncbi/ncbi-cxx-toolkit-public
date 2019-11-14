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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   GFF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>

#include "psl_data.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CPslData::CPslData():
    mFirstDataColumn(-1)
//  ----------------------------------------------------------------------------
{
    xReset();
}

//  ----------------------------------------------------------------------------
void
CPslData::xReset()
//  ----------------------------------------------------------------------------
{
    mMatches = mMisMatches = mRepMatches = mCountN = 0;
    mNumInsertQ = mBaseInsertQ = 0;
    mNumInsertT = mBaseInsertT = 0;
    mStrandT = eNa_strand_unknown;
    mNameQ.clear();
    mSizeQ = mStartQ = mEndQ = 0;
    mNameT.clear();
    mSizeT = mStartT = mEndT = 0;
    mBlockCount = 0;
    mBlockSizes.clear();
    mBlockStartsQ.clear();
    mBlockStartsT.clear();
}

//  ----------------------------------------------------------------------------
void
CPslData::Initialize(
    const string& pslLine)
// 
//  https://genome.ucsc.edu/FAQ/FAQformat.html#format2
//  ----------------------------------------------------------------------------
{
    const INT PSL_COLUMN_COUNT(21);

    vector<string> columns;
    NStr::Split(pslLine, "\t", columns, NStr::fSplit_Tokenize);
    if (-1 == mFirstDataColumn) {
        auto columnCount = columns.size();
        if (columnCount != PSL_COLUMN_COUNT  &&  columnCount != PSL_COLUMN_COUNT+1) {
            // error: bad column count
        }
        mFirstDataColumn = static_cast<int>(columnCount - PSL_COLUMN_COUNT);
    }

    xReset();

    mMatches = NStr::StringToInt(columns[mFirstDataColumn]);
    mMisMatches = NStr::StringToInt(columns[mFirstDataColumn + 1]);
    mRepMatches = NStr::StringToInt(columns[mFirstDataColumn + 2]);
    mCountN = NStr::StringToInt(columns[mFirstDataColumn + 3]);

    mNumInsertQ = NStr::StringToInt(columns[mFirstDataColumn + 4]);
    mBaseInsertQ = NStr::StringToInt(columns[mFirstDataColumn + 5]);
    mNumInsertT = NStr::StringToInt(columns[mFirstDataColumn + 6]);
    mBaseInsertT = NStr::StringToInt(columns[mFirstDataColumn + 7]);
    
    string strand = columns[mFirstDataColumn + 8];
    mStrandT = (strand == "-" ? eNa_strand_minus : eNa_strand_plus);

    mNameQ = columns[mFirstDataColumn + 9];
    mSizeQ = NStr::StringToInt(columns[mFirstDataColumn + 10]);
    mStartQ = NStr::StringToInt(columns[mFirstDataColumn + 11]);
    mEndQ = NStr::StringToInt(columns[mFirstDataColumn + 12]);

    mNameT = columns[mFirstDataColumn + 13];
    mSizeT = NStr::StringToInt(columns[mFirstDataColumn + 14]);
    mStartT = NStr::StringToInt(columns[mFirstDataColumn + 15]);
    mEndT = NStr::StringToInt(columns[mFirstDataColumn + 16]);
    
    mBlockCount = NStr::StringToInt(columns[mFirstDataColumn + 17]);
    mBlockSizes.reserve(mBlockCount);
    mBlockStartsQ.reserve(mBlockCount);
    mBlockStartsQ.reserve(mBlockCount);

    vector<string> values;
    values.reserve(mBlockCount);
    NStr::Split(columns[mFirstDataColumn + 18], ",", values, NStr::fSplit_Tokenize);
    for (const auto& value: values) {
        mBlockSizes.push_back(NStr::StringToInt(value));
    }
    values.clear();
    NStr::Split(columns[mFirstDataColumn + 19], ",", values, NStr::fSplit_Tokenize);
    for (const auto& value: values) {
        mBlockStartsQ.push_back(NStr::StringToInt(value));
    }
    values.clear();
    NStr::Split(columns[mFirstDataColumn + 20], ",", values, NStr::fSplit_Tokenize);
    for (const auto& value: values) {
        mBlockStartsT.push_back(NStr::StringToInt(value));
    }

    // some basic validation:
    if (mBlockCount != mBlockSizes.size()) {
        // error: bad count of blockSizes
    }
    if (mBlockCount != mBlockStartsQ.size()) {
        // error: bad count of qStarts
    }
    if (mBlockCount != mBlockStartsT.size()) {
        // error: bad count of tStarts
    } 
}

//  ----------------------------------------------------------------------------
void
CPslData::Dump(
    ostream& ostr)
//  ----------------------------------------------------------------------------
{
    string strand = (mStrandT == eNa_strand_minus ? "-" : "+");
    string nameQ = (mNameQ.empty() ? "." : mNameQ);
    string nameT = (mNameT.empty() ? "." : mNameT);

    ostr << "matches        : " << mMatches <<endl;
    ostr << "misMatches     : " << mMisMatches <<endl;
    ostr << "repMatches     : " << mRepMatches <<endl;
    ostr << "nCount         : " << mCountN <<endl;
    ostr << "qNumInsert     : " << mNumInsertQ <<endl;
    ostr << "qBaseInsert    : " << mBaseInsertQ <<endl;
    ostr << "tNumInsert     : " << mNumInsertT <<endl;
    ostr << "tBaseInsert    : " << mBaseInsertT <<endl;
    ostr << "strand         : " << strand << endl;
    ostr << "qName          : " << nameQ << endl;
    ostr << "qSize          : " << mSizeQ << endl;
    ostr << "qStart         : " << mStartQ << endl;
    ostr << "qEnd           : " << mEndQ << endl;
    ostr << "tName          : " << nameT << endl;
    ostr << "tSize          : " << mSizeT << endl;
    ostr << "tStart         : " << mStartQ << endl;
    ostr << "tEnd           : " << mEndT << endl;
    ostr << "blockCount     : " << mBlockCount << endl;
    if (mBlockCount) {
        string blockSizes = 
            NStr::JoinNumeric(mBlockSizes.begin(), mBlockSizes.end(), ",");
        string blockStartsQ = 
            NStr::JoinNumeric(mBlockStartsQ.begin(), mBlockStartsQ.end(), ",");
        string blockStartsT = 
            NStr::JoinNumeric(mBlockStartsT.begin(), mBlockStartsT.end(), ",");

        ostr << "blockSizes     : " << blockSizes << endl;
        ostr << "blockStartsQ   : " << blockStartsQ << endl;
        ostr << "blockStartsT   : " << blockStartsT << endl;
    }
    ostr << endl;
}

END_objects_SCOPE
END_NCBI_SCOPE
