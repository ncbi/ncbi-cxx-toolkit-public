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

#include <objtools/import/import_error.hpp>
#include "bed_import_data.hpp"

#include <assert.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
CBedImportData::CBedImportData(
    const CIdResolver& idResolver,
    CImportMessageHandler& errorReporter):
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
    mDisplayData.Assign(rhs.mDisplayData);
}

//  ============================================================================
void
CBedImportData::Initialize(
    const std::string& chrom,
    TSeqPos chromStart,
    TSeqPos chromEnd,
    const std::string& chromName,
    double score,
    ENa_strand chromStrand,
    TSeqPos thickStart,
    TSeqPos thickEnd,
    const RgbValue& rgb,
    unsigned int blockCount,
    const std::vector<int>& blockStarts,
    const std::vector<int>& blockSizes)
//  ============================================================================
{
    CRef<CSeq_id> pId = mIdResolver(chrom);
    mChromLocation.SetInt().Assign(
        CSeq_interval(*pId, chromStart, chromEnd, chromStrand)); 
    mName = chromName;

    mDisplayData.Reset();
    mDisplayData.SetType().SetStr("DisplaySettings");
    xInitializeScore(score);
    xInitializeRgb(rgb);

    if (chromStart == thickStart  &&  thickStart == thickEnd) {
        mThickLocation.SetNull();
    }
    else {
        mThickLocation.SetInt().Assign(
            CSeq_interval(*pId, thickStart, thickEnd, chromStrand));
    }

    //auto blockOffset = mChromLocation.GetInt().GetFrom();
    if (blockCount == 0) {
        mBlocksLocation.SetNull();
    }
    else {
        mBlocksLocation.Reset();
        mBlocksLocation.SetPacked_int();
        for (unsigned int i=0; i < blockCount; ++i) {
            CRef<CSeq_interval> pBlockInterval(new CSeq_interval);
            pBlockInterval->SetFrom(chromStart + blockStarts[i]);
            pBlockInterval->SetTo(chromStart + blockStarts[i] + blockSizes[i]);
            pBlockInterval->SetId(mChromLocation.SetInt().SetId());
            pBlockInterval->SetStrand(mChromLocation.GetInt().GetStrand());
            mBlocksLocation.SetPacked_int().AddInterval(*pBlockInterval);
        }
    }
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

//  ============================================================================
void
CBedImportData::xInitializeScore(
    double score)
//  ============================================================================
{
    if (score < 0) { // score negative indicates there is no score
        return;
    }

    int intScore = static_cast<int>(score);
    if (intScore == score) {
        mDisplayData.AddField("score", intScore);
    }
    else {
        mDisplayData.AddField("score", score);
    }
}

//  ============================================================================
void
CBedImportData::xInitializeRgb(
    const RgbValue& rgb)
    //  ============================================================================
{
    if (rgb.R == -1) {
        return;
    }
    auto r = NStr::IntToString(rgb.R);
    auto g = NStr::IntToString(rgb.G);
    auto b = NStr::IntToString(rgb.B);
    auto color = r + " " + g + " " + b;
    mDisplayData.AddField("color", color);
}
