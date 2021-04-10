
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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write alignment
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Sparse_seg.hpp>
#include <objects/seqalign/Sparse_align.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/seq_id_handle.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/write_util.hpp>
#include <objtools/writers/genbank_id_resolve.hpp>
#include "psl_record.hpp"

#include <util/sequtil/sequtil_manip.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
static void
sExonChunkAppendStats(
    const CSpliced_exon_chunk& chunk,
    int& matchSize,
    int& misMatchSize,
    int& productInsertionSize,
    int& genomicInsertionSize)
//  ----------------------------------------------------------------------------
{
    switch(chunk.Which()) {
        case CSpliced_exon_chunk::e_Match:
            matchSize += chunk.GetMatch();
            break;
        case CSpliced_exon_chunk::e_Mismatch:
            misMatchSize += chunk.GetMismatch();
            break;
        case CSpliced_exon_chunk::e_Product_ins:
            productInsertionSize += chunk.GetProduct_ins();
            break;
        case CSpliced_exon_chunk::e_Genomic_ins:
            genomicInsertionSize += chunk.GetGenomic_ins();
            break;
        default:
            break; //for now, not interested
    }
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeStrands(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (splicedSeg.CanGetProduct_strand()) {
        mStrandQ = splicedSeg.GetProduct_strand();
    }
    if (splicedSeg.CanGetGenomic_strand()) {
        mStrandT = splicedSeg.GetGenomic_strand();
    }
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeStats(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (mNumInsertT != -1  &&  mBaseInsertT != -1) {
        return;
    }
    if (mStrandT == eNa_strand_unknown) {
        xInitializeStrands(scope, splicedSeg);
    }

    // get inserts that are part of exons:
    mMatches = mMisMatches = 0;
    mNumInsertT = mBaseInsertT = 0;
    mNumInsertQ = mBaseInsertQ = 0;
    const auto& exonList = splicedSeg.GetExons();
    for (auto pExon: exonList) {
        for (auto pPart: pExon->GetParts()) {
            sExonChunkAppendStats(
                *pPart, mMatches, mMisMatches, mBaseInsertQ, mBaseInsertT);
            if (pPart->IsProduct_ins()) {
                mNumInsertQ++;
            }
            else if (pPart->IsGenomic_ins()) {
                mNumInsertT++;
            }
        }
    }

    // for the target, introns count as inserts:
    int lastExonBoundT = -1;
    if ( mStrandT == eNa_strand_plus) {
        for (auto pExon: exonList) {
            if (lastExonBoundT == -1) {
                 lastExonBoundT = pExon->GetGenomic_end() + 1;
                 continue;
            }
            int exonStart = pExon->GetGenomic_start();
            if (exonStart > lastExonBoundT) {
                mBaseInsertT += (exonStart - lastExonBoundT);
            }
            lastExonBoundT = pExon->GetGenomic_end() + 1;
        }
    }
    else { // eNa_strand_minus
        for (auto pExon: exonList) {
            if (lastExonBoundT == -1) {
                lastExonBoundT = pExon->GetGenomic_start();
                continue;
            }
            int exonEnd = pExon->GetGenomic_end() + 1;
            if (exonEnd < lastExonBoundT) {
                mBaseInsertT += (lastExonBoundT - exonEnd);
            }
            lastExonBoundT = pExon->GetGenomic_start();
        }
    }
    mNumInsertT += static_cast<int>((exonList.size() - 1));
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeSequenceInfo(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    const auto& queryId = splicedSeg.GetProduct_id();
    auto querySeqHandle = scope.GetBioseqHandle(queryId);
    CGenbankIdResolve::Get().GetBestId(querySeqHandle.GetSeq_id_Handle(), scope, mNameQ);
    if (!querySeqHandle) {
        throw CWriterMessage(
            "Unable to resolve given query id", eDiag_Error);
    }
    mSizeQ = querySeqHandle.GetInst_Length();
    mStartQ = splicedSeg.GetSeqStart(0);
    mEndQ = splicedSeg.GetSeqStop(0) + 1;

    const auto& targetId = splicedSeg.GetGenomic_id();
    auto targetSeqHandle = scope.GetBioseqHandle(targetId);
    CGenbankIdResolve::Get().GetBestId(targetSeqHandle.GetSeq_id_Handle(), scope, mNameT);
    if (!targetSeqHandle) {
        throw CWriterMessage(
            "Unable to resolve given target id", eDiag_Error);
    }
    mSizeT = targetSeqHandle.GetInst_Length();
    mStartT = splicedSeg.GetSeqStart(1);
    mEndT = splicedSeg.GetSeqStop(1) + 1;
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeBlocksStrandPositive(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    const auto& exonList = splicedSeg.GetExons();
    mBlockCount = static_cast<int>(exonList.size());

    for (auto pExon: exonList) {
        //auto partCount = pExon->GetParts().size();
        int exonStartQ = static_cast<int>(pExon->GetProduct_start().AsSeqPos());
        int exonStartT = static_cast<int>(pExon->GetGenomic_start());
        mBlockStartsQ.push_back(exonStartQ);
        mBlockStartsT.push_back(exonStartT);
        int blockSize = 0;
        int productInsertionPending = 0;
        int genomicInsertionPending = 0;
        for (auto pPart: pExon->GetParts()) {
            if (productInsertionPending  ||  genomicInsertionPending) {
                mBlockCount++;
                mBlockSizes.push_back(blockSize);
                mBlockStartsQ.push_back(exonStartQ + blockSize + productInsertionPending);
                mBlockStartsT.push_back(
                    exonStartT + blockSize + genomicInsertionPending);
                exonStartQ += blockSize + productInsertionPending;
                exonStartT += blockSize + genomicInsertionPending;
                blockSize = 0;
                productInsertionPending = 0;
                genomicInsertionPending = 0;
            }
            sExonChunkAppendStats(
                *pPart, blockSize, blockSize, productInsertionPending, genomicInsertionPending);
        }
        mBlockSizes.push_back(blockSize);
        exonStartQ += blockSize;
        exonStartT += blockSize;
    }
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeBlocksStrandNegative(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    const auto& exonList = splicedSeg.GetExons();
    mBlockCount = static_cast<int>(exonList.size());

    for (auto pExon: exonList) {
        int exonEndT = static_cast<int>(pExon->GetGenomic_end() + 1);
        int exonEndQ = mSizeQ - static_cast<int>(pExon->GetProduct_end().AsSeqPos() + 1);
        int blockSize = 0;
        int productInsertionPending = 0;
        int genomicInsertionPending = 0;
        for (auto pPart: pExon->GetParts()) {
            if (productInsertionPending  ||  genomicInsertionPending) {
                mBlockCount++;
                mBlockSizes.push_back(blockSize);
                mBlockStartsT.push_back(exonEndT - blockSize);
                mBlockStartsQ.push_back(exonEndQ);
                exonEndQ -= (blockSize + productInsertionPending);
                exonEndT -= (blockSize + genomicInsertionPending);
                blockSize = 0;
                productInsertionPending = 0;
                genomicInsertionPending = 0;
            }
            sExonChunkAppendStats(
                *pPart, blockSize, blockSize, productInsertionPending, genomicInsertionPending);
        }
        exonEndT -= blockSize;
        mBlockStartsT.push_back(exonEndT);
        mBlockStartsQ.push_back(exonEndQ);
        exonEndQ -= blockSize;
        mBlockSizes.push_back(blockSize);
    }
    std::reverse(mBlockStartsT.begin(), mBlockStartsT.end());
    std::reverse(mBlockSizes.begin(), mBlockSizes.end());
    std::reverse(mBlockStartsQ.begin(), mBlockStartsQ.end());
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeBlocks(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (mBlockCount != -1) {
        return;
    }

    if (mStrandT == eNa_strand_unknown) {
        xInitializeStrands(scope, splicedSeg);
    }

    const auto& exonList = splicedSeg.GetExons();
    mBlockCount = static_cast<int>(exonList.size());

    if (mStrandT == eNa_strand_plus) {
        xInitializeBlocksStrandPositive(scope, splicedSeg);
    }
    else {
        xInitializeBlocksStrandNegative(scope, splicedSeg);
    }
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xValidateSegment(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (!splicedSeg.CanGetProduct_type()  ||  
            splicedSeg.GetProduct_type() == CSpliced_seg::eProduct_type_protein) {
        // would love to support but need proper sample data first!
        throw CWriterMessage(
            "Unsupported alignment product type \"protein\"", eDiag_Error);
    }

    const auto& exonList = splicedSeg.GetExons();
    for (auto pExon: exonList) {
        if (!pExon->CanGetProduct_start()  || !pExon->CanGetProduct_end()) {
            throw CWriterMessage(
                "Mandatory product information missing", eDiag_Error);
        }
        if (!pExon->CanGetGenomic_start()  || !pExon->CanGetGenomic_end()) {
            throw CWriterMessage(
                "Mandatory target information missing", eDiag_Error);
        }
    }
}

//  ----------------------------------------------------------------------------
void
CPslRecord::Initialize(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    xValidateSegment(scope, splicedSeg);

    xInitializeStrands(scope, splicedSeg);
    xInitializeSequenceInfo(scope, splicedSeg);
    xInitializeStats(scope, splicedSeg);
    xInitializeBlocks(scope, splicedSeg);
}

//  ----------------------------------------------------------------------------
void
CPslRecord::Initialize(
    CScope& scope,
    const CSeq_align::TScore& scores)
//  ----------------------------------------------------------------------------
{
    for (const auto& pScore: scores) {
        if (!pScore->CanGetId()  ||  !pScore->GetId().IsStr()) {
            continue;
        }
        if (!pScore->CanGetValue()) {
            continue;
        }
        const auto& key = pScore->GetId().GetStr();
        const auto& value = pScore->GetValue();
        if (key == "num_mismatch"  &&  value.IsInt()  &&  mMisMatches == -1) {
            mMisMatches = value.GetInt();
            continue;
        }
    }
}

//  ----------------------------------------------------------------------------
void
CPslRecord::Finalize()
//  ----------------------------------------------------------------------------
{
    if (mRepMatches == -1) {
        mRepMatches = 0;
    }
    if (mMisMatches == -1) {
        mMisMatches = 0;
    }
    if (mCountN == -1) {
        mCountN = 0;
    }
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xValidateSegment(
    CScope& scope,
    const CDense_seg& denseSeg)
//  ----------------------------------------------------------------------------
{
    if (denseSeg.GetDim() != 2) {
        throw CWriterMessage(
            "PSL supports only pairwaise alignments", eDiag_Error);
    }
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeStrands(
    CScope& scope,
    const CDense_seg& denseSeg)
//  ----------------------------------------------------------------------------
{
    mStrandQ = denseSeg.GetSeqStrand(0);
    mStrandT = denseSeg.GetSeqStrand(1);
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeSequenceInfo(
    CScope& scope,
    const CDense_seg& denseSeg)
//  ----------------------------------------------------------------------------
{
    const CSeq_id& idQ = denseSeg.GetSeq_id(0);
    auto seqHandleQ = scope.GetBioseqHandle(idQ);
    CGenbankIdResolve::Get().GetBestId(seqHandleQ.GetSeq_id_Handle(), scope, mNameQ);
    mSizeQ = seqHandleQ.GetInst_Length();
    mStartQ = denseSeg.GetSeqStart(0);
    mEndQ = denseSeg.GetSeqStop(0) + 1;

    const CSeq_id& idT = denseSeg.GetSeq_id(1);
    auto seqHandleT = scope.GetBioseqHandle(idT);
    CGenbankIdResolve::Get().GetBestId(seqHandleT.GetSeq_id_Handle(), scope, mNameT);
    mSizeT = seqHandleT.GetInst_Length();
    mStartT = denseSeg.GetSeqStart(1);
    mEndT = denseSeg.GetSeqStop(1) + 1;
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeStatsAndBlocks(
    CScope& scope,
    const CDense_seg& denseSeg)
//  ----------------------------------------------------------------------------
{
    mMatches = 0;
    for (auto length: denseSeg.GetLens()) {
        mMatches += length;
    }
    mBlockCount = static_cast<int>(denseSeg.GetLens().size());
    auto starts = denseSeg.GetStarts();
    auto lens = denseSeg.GetLens();
    for (int i=0; i < mBlockCount; ++i) {
        if (starts[2*i] != -1  &&  starts[2*i+1] != -1) {
            mBlockStartsQ.push_back(starts[2*i]);
            mBlockStartsT.push_back(starts[2*i + 1]);
            mBlockSizes.push_back(lens[i]);
        }
    }
    if (eNa_strand_minus == denseSeg.GetSeqStrand(0)) {
        std::reverse(mBlockStartsQ.begin(), mBlockStartsQ.end());
        std::reverse(mBlockSizes.begin(), mBlockSizes.end());
    }
    if (eNa_strand_minus == denseSeg.GetSeqStrand(1)) {
        std::reverse(mBlockStartsT.begin(), mBlockStartsT.end());
        std::reverse(mBlockSizes.begin(), mBlockSizes.end());
    }
    mBlockCount = static_cast<int>(mBlockSizes.size());

    mNumInsertQ = mBaseInsertQ = 0;
    mNumInsertT = mBaseInsertT = 0;
    if (mBlockStartsT[0] == -1) {
        mNumInsertQ++;
        mBaseInsertQ += mBlockSizes[0];
    }
    if (mBlockStartsQ[0] == -1) {
        mNumInsertT++;
        mBaseInsertT += mBlockSizes[0];
    }
    for (int i=1; i < mBlockCount; ++i) {
        auto endOfLastQ = mBlockStartsQ[i-1] + mBlockSizes[i-1];
        auto startOfThisQ = mBlockStartsQ[i];
        if (startOfThisQ - endOfLastQ != 0) {
            mNumInsertQ++;
            mBaseInsertQ += (startOfThisQ - endOfLastQ);
        }
        if (mBlockStartsT[i] == -1) {
            mNumInsertQ++;
            mBaseInsertQ += mBlockSizes[i];
        }
        auto endOfLastT = mBlockStartsT[i-1] + mBlockSizes[i-1];
        auto startOfThisT = mBlockStartsT[i];
        if (startOfThisT - endOfLastT != 0) {
            mNumInsertT++;
            mBaseInsertT += (startOfThisT - endOfLastT);
        }
        if (mBlockStartsQ[i] == -1) {
            mNumInsertT++;
            mBaseInsertT += mBlockSizes[i];
        }
    }
}

//  ----------------------------------------------------------------------------
void
CPslRecord::Initialize(
    CScope& scope,
    const CDense_seg& denseSeg)
//  ----------------------------------------------------------------------------
{
    xValidateSegment(scope, denseSeg);
    xInitializeStrands(scope, denseSeg);
    xInitializeSequenceInfo(scope, denseSeg);
    xInitializeStatsAndBlocks(scope, denseSeg);
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xPutMessage(
    const string& message,
    EDiagSev severity)
//  ----------------------------------------------------------------------------
{
    if (mpMessageListener) {
        mpMessageListener->PutMessage(CWriterMessage(message, severity));
        return;
    }
    NCBI_THROW(CObjWriterException, eBadInput, message);
};

END_NCBI_SCOPE

