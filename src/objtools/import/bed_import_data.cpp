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
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objtools/import/feat_import_error.hpp>
#include "bed_import_data.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
CBedImportData::CBedImportData(
    const CIdResolver& idResolver,
    CFeatMessageHandler& errorReporter):
//  ============================================================================
    CFeatImportData(idResolver, errorReporter),
    mName(""),
    mScore(0),
    mRgb(0, 0, 0)
{
    mChromLocation.SetNull();
    mThickLocation.SetNull();
    mBlocksLocation.SetNull();
}

//  ============================================================================
CBedImportData::CBedImportData(
    const CBedImportData& rhs):
//  ============================================================================
    CFeatImportData(rhs),
    mName(rhs.mName),
    mScore(rhs.mScore),
    mRgb(rhs.mRgb)
{
    mChromLocation.Assign(rhs.mChromLocation);
    mThickLocation.Assign(rhs.mThickLocation);
    mBlocksLocation.Assign(rhs.mBlocksLocation);
}

//  ============================================================================
void
CBedImportData::InitializeFrom(
    const vector<string>& columns)
//  ============================================================================
{
    // note: ordering is important!
    xSetChromLocation(columns[0], columns[1], columns[2]);
    xSetName(columns[3]);
    xSetScore(columns[4]);
    xSetStrand(columns[5]);
    xSetThickLocation(columns[6], columns[7]);
    xSetRgb(columns[4], columns[8]);
    xSetBlocks(columns[9], columns[10], columns[11]);
}

//  ============================================================================
void
CBedImportData::Serialize(
    CNcbiOstream& out)
//  ============================================================================
{
    string chrom = "---";
    string chromStart = "---";
    string chromEnd = "---";
    string chromStrand = "---";
    if (mChromLocation.IsInt()) {
        chrom = mChromLocation.GetInt().GetId().GetSeqIdString();
        chromStart = NStr::IntToString(mChromLocation.GetInt().GetFrom());
        chromEnd = NStr::IntToString(mChromLocation.GetInt().GetTo());
        auto strand = mChromLocation.GetInt().GetStrand();
        chromStrand = (strand == eNa_strand_minus ? "minus" : "plus");
    }
    string thickStart = "---";
    string thickEnd = "---";
    if (mThickLocation.IsInt()) {
        thickStart = NStr::IntToString(mThickLocation.GetInt().GetFrom());
        thickEnd = NStr::IntToString(mThickLocation.GetInt().GetTo());
    }

    string blocks = "---";
    if (!mBlocksLocation.IsNull()) {
        vector<string> blockStrs;
        for (auto pBlock: mBlocksLocation.GetPacked_int().Get()) {
            string blockStr = "[";
            blockStr += NStr::IntToString(pBlock->GetFrom());
            blockStr += "..";
            blockStr += NStr::IntToString(pBlock->GetTo());
            blockStr += "]";
            blockStrs.push_back(blockStr);
        }
        blocks = NStr::Join(blockStrs, ", ");
    }

    out << "CBedImportData:\n";
    out << "  Chrom = \"" << chrom << "\"\n";
    out << "  ChromStart = " << chromStart << "\n";
    out << "  ChromEnd = " << chromEnd << "\n";
    out << "  Name = \"" << mName << "\"\n";
    out << "  Score = " << mScore << "\n";
    out << "  Strand = " << chromStrand << "\n";
    out << "  ThickStart = " << thickStart << "\n";
    out << "  ThickEnd = " << thickEnd << "\n";
    out << "  Rgb = (" << mRgb.R << "," << mRgb.G << "," << mRgb.B << ")\n"; 
    out << "  Blocks = " << blocks << "\n";
    out << "\n";
}

//  =============================================================================
void
CBedImportData::xSetChromLocation(
    const string& chrom,
    const string& chromStartAsStr,
    const string& chromEndAsStr)
//  =============================================================================
{
    CFeatImportError errorInvalidChromStartValue(
        CFeatImportError::ERROR, "Invalid chromStart value");
    CFeatImportError errorInvalidChromEndValue(
        CFeatImportError::ERROR, "Invalid chromEnd value");

    int chromStart = 0, chromEnd = 0;
    try {
        chromStart = NStr::StringToInt(chromStartAsStr);
    }
    catch(std::exception&) {
        throw errorInvalidChromStartValue;
    }
    try {
        chromEnd = NStr::StringToInt(chromEndAsStr);
    }
    catch(std::exception&) {
        throw errorInvalidChromEndValue;
    }
    mChromLocation.SetInt().Assign(
        CSeq_interval(*mIdResolver(chrom), chromStart, chromEnd)); 
}

//  =============================================================================
void
CBedImportData::xSetName(
    const string& name)
//  =============================================================================
{
    mName = name;
}

//  =============================================================================
void
CBedImportData::xSetScore(
    const string& score)
//  =============================================================================
{
    CFeatImportError errorInvalidScoreValue(
        CFeatImportError::WARNING, "Invalid score value- defaulting to 0.");

    try {
        mScore = NStr::StringToInt(score);
    }
    catch(std::exception&) {
        mScore = 0;
        mErrorReporter.ReportError(errorInvalidScoreValue);
        return;
    }
    if (mScore < 0  ||  mScore > 1000) {
        mScore = 0;
        mErrorReporter.ReportError(errorInvalidScoreValue);
        return;
    }
}

//  ============================================================================
void
CBedImportData::xSetStrand(
    const string& strand)
    //  ============================================================================
{
    assert(mChromLocation.IsInt());

    CFeatImportError errorInvalidStrandValue(
        CFeatImportError::ERROR, "Invalid strand value");

    vector<string> validSettings = {".", "+", "-"};
    if (find(validSettings.begin(), validSettings.end(), strand) ==
        validSettings.end()) {
        throw errorInvalidStrandValue;
    }
    auto chromStrand = ((strand == "-") ? eNa_strand_minus : eNa_strand_plus);
    mChromLocation.SetInt().SetStrand(chromStrand);
}

//  =============================================================================
void
CBedImportData::xSetThickLocation(
    const string& thickStartAsStr,
    const string& thickEndAsStr)
//  =============================================================================
{
    assert(mChromLocation.IsInt());

    CFeatImportError errorInvalidThickStartValue(
        CFeatImportError::ERROR, "Invalid thickStart value");
    CFeatImportError errorInvalidThickEndValue(
        CFeatImportError::ERROR, "Invalid thickEnd value");

    int thickStart = 0, thickEnd = 0;
    try {
        thickStart = NStr::StringToInt(thickStartAsStr);
    }
    catch(std::exception&) {
        throw errorInvalidThickStartValue;
    }
    try {
        thickEnd = NStr::StringToInt(thickEndAsStr);
    }
    catch(std::exception&) {
        throw errorInvalidThickEndValue;
    }
    
    mThickLocation.SetInt().Assign(
        CSeq_interval(mChromLocation.SetInt().SetId(), thickStart, thickEnd));
    mThickLocation.SetStrand(mChromLocation.GetInt().GetStrand());
}

//  ============================================================================
void
CBedImportData::xSetRgb(
    const string& score,
    const string& rgb)
//  ============================================================================
{
    CFeatImportError errorInvalidRgbValue(
        CFeatImportError::WARNING, "Invalid RGB value- defaulting to BLACK");

    mRgb.R = mRgb.G = mRgb.B = 0;
    try {
        vector<string > rgbSplits;
        NStr::Split(rgb, ",", rgbSplits);
        switch(rgbSplits.size()) {
        default:
            throw errorInvalidRgbValue;
        case 1: {
                unsigned int rgbValue = NStr::StringToInt(rgbSplits[0]);
                rgbValue &= 0xffffff;
                mRgb.R = (rgbValue & 0xff0000) >> 16;
                mRgb.G = (rgbValue & 0x00ff00) >> 8;
                mRgb.B = (rgbValue & 0x0000ff);
                break;
        }
        case 3: {
                mRgb.R = NStr::StringToInt(rgbSplits[0]);
                mRgb.G = NStr::StringToInt(rgbSplits[1]);
                mRgb.B = NStr::StringToInt(rgbSplits[2]);
                break;
            }
        }
    }
    catch(std::exception&) {
        mRgb.R = mRgb.G = mRgb.B = 0;
        mErrorReporter.ReportError(errorInvalidRgbValue);
        return;
    }
    if (mRgb.R < 0  ||  255 < mRgb.R) {
        mRgb.R = mRgb.G = mRgb.B = 0;
        mErrorReporter.ReportError(errorInvalidRgbValue);
        return;
    }
    if (mRgb.G < 0  ||  255 < mRgb.G) {
        mRgb.R = mRgb.G = mRgb.B = 0;
        mErrorReporter.ReportError(errorInvalidRgbValue);
        return;
    }
    if (mRgb.B < 0  ||  255 < mRgb.B) {
        mRgb.R = mRgb.G = mRgb.B = 0;
        mErrorReporter.ReportError(errorInvalidRgbValue);
        return;
    }
}

//  ============================================================================
void
CBedImportData::xSetBlocks(
    const string& blockCountAsStr,
    const string& blockStartsAsStr,
    const string& blockSizesAsStr)
//  ============================================================================
{
    assert(mChromLocation.IsInt());

    CFeatImportError errorInvalidBlockCountValue(
        CFeatImportError::ERROR, "Invalid blockCount value");
    CFeatImportError errorInvalidBlockStartsValue(
        CFeatImportError::ERROR, "Invalid blockStarts value");
    CFeatImportError errorInvalidBlockSizesValue(
        CFeatImportError::ERROR, "Invalid blockStarts value");
    CFeatImportError errorInconsistentBlocksInformation(
        CFeatImportError::ERROR, "Inconsistent blocks information");

    int blockCounts = 0;
    try {
        blockCounts = NStr::StringToInt(blockCountAsStr);
    }
    catch(std::exception&) {
        throw errorInvalidBlockCountValue;
    }

    vector<int> blockStarts;
    try {
        vector<string> blockStartsSplits;
        NStr::Split(blockStartsAsStr, ",", blockStartsSplits);
        if (blockStartsSplits.back().empty()) {
            blockStartsSplits.pop_back();
        }
        for (auto blockStart: blockStartsSplits) {
            blockStarts.push_back(NStr::StringToInt(blockStart));
        }
    }
    catch(std::exception&) {
        throw errorInvalidBlockCountValue;
    }
    if (blockCounts != blockStarts.size()) {
        throw errorInconsistentBlocksInformation;
    }

    vector<int> blockSizes;
    try {
        vector<string> blockSizesSplits;
        NStr::Split(blockSizesAsStr, ",", blockSizesSplits);
        if (blockSizesSplits.back().empty()) {
            blockSizesSplits.pop_back();
        }
        for (auto blockSize: blockSizesSplits) {
            blockSizes.push_back(NStr::StringToInt(blockSize));
        }
    }
    catch(std::exception&) {
        throw errorInvalidBlockCountValue;
    }
    if (blockCounts != blockSizes.size()) {
        throw errorInconsistentBlocksInformation;
    }

    auto blockOffset = mChromLocation.GetInt().GetFrom();
    mBlocksLocation.SetPacked_int();
    for (int i=0; i < blockCounts; ++i) {
        CRef<CSeq_interval> pBlockInterval(new CSeq_interval);
        pBlockInterval->SetFrom(blockOffset + blockStarts[i]);
        pBlockInterval->SetTo(blockOffset + blockStarts[i] + blockSizes[i]);
        pBlockInterval->SetId(mChromLocation.SetInt().SetId());
        pBlockInterval->SetStrand(mChromLocation.GetInt().GetStrand());
        mBlocksLocation.SetPacked_int().AddInterval(*pBlockInterval);
    }
}
