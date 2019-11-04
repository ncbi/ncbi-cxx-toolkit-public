
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
#include <objects/general/Object_id.hpp>
#include <objects/seq/seq_id_handle.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/write_util.hpp>
#include "psl_record.hpp"

#include <util/sequtil/sequtil_manip.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
static void
sSizeExonChunk(
    const CSpliced_exon_chunk& chunk,
    int& blockSize,
    int& productInsertionSize,
    int& genomicInsertionSize)
//  ----------------------------------------------------------------------------
{
    switch(chunk.Which()) {
        case CSpliced_exon_chunk::e_Match:
            blockSize += chunk.GetMatch();
            break;
        case CSpliced_exon_chunk::e_Mismatch:
            blockSize += chunk.GetMismatch();
            break;
        case CSpliced_exon_chunk::e_Product_ins:
            if (blockSize > 0) {
                productInsertionSize = chunk.GetProduct_ins();
            }
            break;
        case CSpliced_exon_chunk::e_Genomic_ins:
            if (blockSize > 0) {
                genomicInsertionSize = chunk.GetGenomic_ins();
            }
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
CPslRecord::xInitializeInsertsQ(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (mNumInsertQ != -1  &&  mBaseInsertQ != -1) {
        return;
    }
    if (mStrandQ == eNa_strand_unknown) {
        xInitializeStrands(scope, splicedSeg);
    }
    if (mEndQ == -1) {
        xInitializeSequenceQ(scope, splicedSeg);
    }

    mNumInsertQ = mBaseInsertQ = 0;
    const auto& exonList = splicedSeg.GetExons();

    int startQQ(-1), endQQ(-1);
    for (auto pExon: exonList) {
        for (auto pPart: pExon->GetParts()) {
            if (pPart->IsProduct_ins()) {
                mNumInsertQ++;
                mBaseInsertQ += pPart->GetProduct_ins();
            }
        }
        int exonStartQ = static_cast<int>(pExon->GetProduct_start().AsSeqPos());
        if (startQQ == -1  ||  exonStartQ < startQQ) {
            startQQ = exonStartQ;
        }
        int exonEndQ = static_cast<int>(pExon->GetProduct_end().AsSeqPos());
        if (endQQ == -1  ||  exonEndQ >= endQQ) {
            endQQ = exonEndQ + 1;
        }
    }
    mEndQ = endQQ;
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeInsertsT(
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

    mNumInsertT = mBaseInsertT = 0;
    const auto& exonList = splicedSeg.GetExons();
    if (mStrandT == eNa_strand_plus) {
        int lastExonEndT = -1;
        for (auto pExon: exonList) {
             for (auto pPart: pExon->GetParts()) {
                if (pPart->IsGenomic_ins()) {
                    mNumInsertT++;
                    mBaseInsertT += pPart->GetGenomic_ins();
                }
            }
            if (lastExonEndT == -1) {
                 lastExonEndT = pExon->GetGenomic_end() + 1;
                 continue;
            }
            auto exonStart = pExon->GetGenomic_start();
            if (exonStart > lastExonEndT) {
                mNumInsertT++;
                mBaseInsertT += (exonStart - lastExonEndT);
            }
            lastExonEndT = pExon->GetGenomic_end() + 1;
        }
    }
    else { // eNa_strand_minus
        int lastExonStartT = -1;
        for (auto pExon: exonList) {
            for (auto pPart: pExon->GetParts()) {
                if (pPart->IsGenomic_ins()) {
                    mNumInsertT++;
                    mBaseInsertT += pPart->GetGenomic_ins();
                }
            }
            if (lastExonStartT == -1) {
                lastExonStartT = pExon->GetGenomic_start();
                continue;
            }
            auto exonEnd = pExon->GetGenomic_end() + 1;
            if (exonEnd < lastExonStartT) {
                mNumInsertT++;
                mBaseInsertT += (lastExonStartT - exonEnd);
            }
            lastExonStartT = pExon->GetGenomic_start();
        }
    }
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeMatchesMismatches(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (mMatches != -1  &&  mMisMatches != -1  &&  mRepMatches != -1) {
        return;
    }

    mMatches = mMisMatches = mRepMatches = 0;
    const auto& exonList = splicedSeg.GetExons();
    for (auto pExon: exonList) {
        const auto& exonParts = pExon->GetParts();
        for (auto part: exonParts) {
            if (part->IsMatch()) {
                mMatches += part->GetMatch();
            }
            else if (part->IsMismatch()) {
                mMisMatches += part->GetMismatch();
            }
        } 
    }
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeSequenceQ(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (mSizeQ != -1  &&  mStartQ != -1  &&  mEndQ != -1) {
        return;
    }
    const auto& queryId = splicedSeg.GetProduct_id();
    auto querySeqHandle = scope.GetBioseqHandle(queryId);
    CWriteUtil::GetBestId(querySeqHandle.GetSeq_id_Handle(), scope, mNameQ);
    mSizeQ = querySeqHandle.GetInst_Length();
    mStartQ = splicedSeg.GetSeqStart(0);
    mEndQ = splicedSeg.GetSeqStop(0);
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeSequenceT(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (mSizeT != -1  &&  mStartT != -1  &&  mEndT != -1) {
        return;
    }
    const auto& targetId = splicedSeg.GetGenomic_id();
    auto targetSeqHandle = scope.GetBioseqHandle(targetId);
    CWriteUtil::GetBestId(targetSeqHandle.GetSeq_id_Handle(), scope, mNameT);
    mSizeT = targetSeqHandle.GetInst_Length();

    const auto& exonList = splicedSeg.GetExons();
    for (auto pExon: exonList) {
        int exonStartT = static_cast<int>(pExon->GetGenomic_start());
        if (mStartT == -1  ||  exonStartT < mStartT) {
            mStartT = exonStartT;
        }
        int exonEndT = static_cast<int>(pExon->GetGenomic_end());
        if (mEndT == -1  ||  exonEndT >= mEndT) {
            mEndT = exonEndT + 1;
        }
    }
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeBlocksStrandPositive(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    const auto& exonList = splicedSeg.GetExons();
    mExonCount = static_cast<int>(exonList.size());

    for (auto pExon: exonList) {
        auto partCount = pExon->GetParts().size();
        if (partCount != 1) {
            cerr << "";
        }
        int exonStartQ = static_cast<int>(pExon->GetProduct_start().AsSeqPos());
        int exonStartT = static_cast<int>(pExon->GetGenomic_start());
        mExonStartsQ.push_back(exonStartQ);
        mExonStartsT.push_back(exonStartT);
        int blockSize = 0;
        int productInsertionPending = 0;
        int genomicInsertionPending = 0;
        for (auto pPart: pExon->GetParts()) {
            if (productInsertionPending  ||  genomicInsertionPending) {
                mExonCount++;
                mExonSizes.push_back(blockSize);
                mExonStartsQ.push_back(exonStartQ + blockSize + productInsertionPending);
                mExonStartsT.push_back(
                    exonStartT + blockSize + genomicInsertionPending);
                exonStartQ += blockSize + productInsertionPending;
                exonStartT += blockSize + genomicInsertionPending;
                blockSize = 0;
                productInsertionPending = 0;
                genomicInsertionPending = 0;
            }
            sSizeExonChunk(
                *pPart, blockSize, productInsertionPending, genomicInsertionPending);
        }
        mExonSizes.push_back(blockSize);
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
    mExonCount = static_cast<int>(exonList.size());

    for (auto pExon: exonList) {
        int exonEndT = static_cast<int>(pExon->GetGenomic_end() + 1);
        int exonEndQ = mSizeQ - static_cast<int>(pExon->GetProduct_end().AsSeqPos() + 1);
        int blockSize = 0;
        int productInsertionPending = 0;
        int genomicInsertionPending = 0;
        for (auto pPart: pExon->GetParts()) {
            if (productInsertionPending  ||  genomicInsertionPending) {
                mExonCount++;
                mExonSizes.push_back(blockSize);
                mExonStartsT.push_back(exonEndT - blockSize);
                mExonStartsQ.push_back(exonEndQ);
                exonEndQ -= (blockSize + productInsertionPending);
                exonEndT -= (blockSize + genomicInsertionPending);
                blockSize = 0;
                productInsertionPending = 0;
                genomicInsertionPending = 0;
            }
            sSizeExonChunk(
                *pPart, blockSize, productInsertionPending, genomicInsertionPending);
        }
        exonEndT -= blockSize;
        mExonStartsT.push_back(exonEndT);
        mExonStartsQ.push_back(exonEndQ);
        exonEndQ -= blockSize;
        mExonSizes.push_back(blockSize);
    }
    std::reverse(mExonStartsT.begin(), mExonStartsT.end());
    std::reverse(mExonSizes.begin(), mExonSizes.end());
    std::reverse(mExonStartsQ.begin(), mExonStartsQ.end());
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xInitializeBlocks(
    CScope& scope,
    const CSpliced_seg& splicedSeg)
//  ----------------------------------------------------------------------------
{
    if (mExonCount != -1) {
        return;
    }

    if (mStrandT == eNa_strand_unknown) {
        xInitializeStrands(scope, splicedSeg);
    }
    if (mBaseInsertQ == -1) {
        xInitializeInsertsQ(scope, splicedSeg);
    }

    const auto& exonList = splicedSeg.GetExons();
    mExonCount = static_cast<int>(exonList.size());

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
    const auto& exonList = splicedSeg.GetExons();
    for (auto pExon: exonList) {
        if (!pExon->CanGetProduct_start()  || !pExon->CanGetProduct_end()) {
            NCBI_THROW(CObjWriterException, 
                eBadInput, 
                "Mandatory product information missing");
        }
        if (!pExon->CanGetGenomic_start()  || !pExon->CanGetGenomic_end()) {
            NCBI_THROW(CObjWriterException, 
                eBadInput, 
                "Mandatory target information missing");
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
    xInitializeMatchesMismatches(scope, splicedSeg);
    //nCount?
    xInitializeSequenceQ(scope, splicedSeg);
    xInitializeSequenceT(scope, splicedSeg);
    xInitializeInsertsQ(scope, splicedSeg);
    xInitializeInsertsT(scope, splicedSeg);
    xInitializeBlocks(scope, splicedSeg);
}

//  ----------------------------------------------------------------------------
void
CPslRecord::xValidateSegment(
    CScope& scope,
    const CDense_seg& denseSeg)
//  ----------------------------------------------------------------------------
{
    if (denseSeg.GetDim() != 2) {
        NCBI_THROW(CObjWriterException, 
            eBadInput, 
            "PSL supports only pairwaise alignments");
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
}

END_NCBI_SCOPE

