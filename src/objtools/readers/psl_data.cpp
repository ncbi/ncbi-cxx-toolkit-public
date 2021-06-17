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
 *   PSL data handling
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objtools/readers/read_util.hpp>
#include "psl_data.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CPslData::CPslData(
    CReaderMessageHandler* pEL):
    mpEL(pEL)
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
    const CPslReader::TReaderLine& readerLine)
//
//  https://genome.ucsc.edu/FAQ/FAQformat.html#format2
//  ----------------------------------------------------------------------------
{
    const int PSL_COLUMN_COUNT(21);

    vector<string> columns;
    NStr::Split(readerLine.mData, "\t", columns, NStr::fSplit_Tokenize);
    if (-1 == mFirstDataColumn) {
        auto columnCount = columns.size();
        if (columnCount != PSL_COLUMN_COUNT  &&  columnCount != PSL_COLUMN_COUNT+1) {
        throw CReaderMessage(
            eDiag_Error,
            readerLine.mLine,
            "PSL Error: Record has invalid column count.");
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
        throw CReaderMessage(
            eDiag_Error,
            readerLine.mLine,
            "PSL Error: Number of blockSizes does not match blockCount.");
    }
    if (mBlockCount != mBlockStartsQ.size()) {
        throw CReaderMessage(
            eDiag_Error,
            readerLine.mLine,
            "PSL Error: Number of blockStartsQ does not match blockCount.");
    }
    if (mBlockCount != mBlockStartsT.size()) {
        throw CReaderMessage(
            eDiag_Error,
            readerLine.mLine,
            "PSL Error: Number of blockStartsT does not match blockCount.");
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
    if (mBlockCount < 5) {
        cerr << "";
    }
}

//  ----------------------------------------------------------------------------
void
CPslData::ExportToSeqAlign(
    CPslReader::SeqIdResolver idResolver,
    CSeq_align& seqAlign)
//  ----------------------------------------------------------------------------
{
    seqAlign.SetType(CSeq_align::eType_partial);
    CDense_seg& denseSeg = seqAlign.SetSegs().SetDenseg();

    auto& ids = denseSeg.SetIds();
    ids.push_back(idResolver(mNameQ, 0, true));
    ids.push_back(idResolver(mNameT, 0, true));

    vector<SAlignSegment> segments;
    xConvertBlocksToSegments(segments);
    for (const auto& segment: segments) {
        denseSeg.SetLens().push_back(segment.mLen);
        denseSeg.SetStarts().push_back(segment.mStartQ);
        denseSeg.SetStarts().push_back(segment.mStartT);
        denseSeg.SetStrands().push_back(segment.mStrandQ);
        denseSeg.SetStrands().push_back(segment.mStrandT);
    }
    denseSeg.SetNumseg(static_cast<int>(segments.size()));

    // cram whatever hasn't been disposed of otherwise into
    //  scores
    CRef<CScore>  pMatches(new CScore);
    pMatches->SetId().SetStr("num_match");
    pMatches->SetValue().SetInt(mMatches);
    denseSeg.SetScores().push_back(pMatches);
    CRef<CScore>  pMisMatches(new CScore);
    pMisMatches->SetId().SetStr("num_mismatch");
    pMisMatches->SetValue().SetInt(mMisMatches);
    denseSeg.SetScores().push_back(pMisMatches);
    CRef<CScore> pRepMatches(new CScore);
    pRepMatches->SetId().SetStr("num_repmatch");
    pRepMatches->SetValue().SetInt(mRepMatches);
    denseSeg.SetScores().push_back(pRepMatches);
    CRef<CScore> pCountN(new CScore);
    pCountN->SetId().SetStr("num_n");
    pCountN->SetValue().SetInt(mCountN);
    denseSeg.SetScores().push_back(pCountN);
}

//  ----------------------------------------------------------------------------
void
CPslData::xConvertBlocksToSegments(
    vector<SAlignSegment>& segments) const
//  ----------------------------------------------------------------------------
{
    segments.clear();
    if (mBlockCount == 0) {
        return;
    }
    segments.push_back(SAlignSegment{
        mBlockSizes[0],
        mBlockStartsQ[0], mBlockStartsT[0],
        eNa_strand_plus, mStrandT});
    int currentPosQ = mBlockStartsQ[0] + mBlockSizes[0];
    int currentPosT = mBlockStartsT[0] + mBlockSizes[0];
    for (int i=1; i < mBlockCount; ++i) {
        auto diffQ = mBlockStartsQ[i] - currentPosQ;
        if (diffQ) {
            segments.push_back(SAlignSegment{
                diffQ, currentPosQ, -1, eNa_strand_plus, mStrandT});
        }
        auto diffT = mBlockStartsT[i] - currentPosT;
        if (diffT) {
            segments.push_back(SAlignSegment{
                diffT, -1, currentPosT, eNa_strand_plus, mStrandT});
        }
        segments.push_back(SAlignSegment{
            mBlockSizes[i],
            mBlockStartsQ[i], mBlockStartsT[i],
            eNa_strand_plus, mStrandT});
        currentPosQ = mBlockStartsQ[i] + mBlockSizes[i];
        currentPosT = mBlockStartsT[i] + mBlockSizes[i];
    }
}

END_objects_SCOPE
END_NCBI_SCOPE
