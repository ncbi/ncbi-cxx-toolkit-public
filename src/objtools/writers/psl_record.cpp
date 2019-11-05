
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
    mMatches = mMisMatches = mRepMatches = 0;
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
    CWriteUtil::GetBestId(querySeqHandle.GetSeq_id_Handle(), scope, mNameQ);
    mSizeQ = querySeqHandle.GetInst_Length();
    mStartQ = splicedSeg.GetSeqStart(0);
    mEndQ = splicedSeg.GetSeqStop(0) + 1;

    const auto& targetId = splicedSeg.GetGenomic_id();
    auto targetSeqHandle = scope.GetBioseqHandle(targetId);
    CWriteUtil::GetBestId(targetSeqHandle.GetSeq_id_Handle(), scope, mNameT);
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
            sExonChunkAppendStats(
                *pPart, blockSize, blockSize, productInsertionPending, genomicInsertionPending);
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
            sExonChunkAppendStats(
                *pPart, blockSize, blockSize, productInsertionPending, genomicInsertionPending);
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
     //nCount?
    xInitializeSequenceInfo(scope, splicedSeg);
    xInitializeStats(scope, splicedSeg);
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

