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
* Authors:  Paul Thiessen
*
* File Description:
*      Interface to Alejandro's block alignment algorithm
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2002/07/27 12:29:51  thiessen
* fix block aligner crash
*
* Revision 1.1  2002/07/26 15:28:45  thiessen
* add Alejandro's block alignment algorithm
*
* ===========================================================================
*/

#include <wx/string.h>
#include <corelib/ncbistd.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>

#include "cn3d/cn3d_ba_interface.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/asn_converter.hpp"
#include "cn3d/molecule_identifier.hpp"

// necessary C-toolkit headers
#include <ncbi.h>
#include <blastkar.h>
#include <posit.h>

// functions from cn3d_blockalign.c (no header for these yet...)
extern "C" {
extern void allocateAlignPieceMemory(Int4 numBlocks);
void findAllowedGaps(SeqAlign *listOfSeqAligns, Int4 numBlocks, Int4 *allowedGaps,
    Nlm_FloatHi percentile, Int4 gapAddition);
extern void findAlignPieces(Uint1Ptr convertedQuery, Int4 queryLength, Int4 numBlocks, Int4 *blockStarts,
    Int4 *blockEnds, Int4 masterLength, BLAST_Score **posMatrix, BLAST_Score scoreThresholdSingleBlock);
extern void LIBCALL sortAlignPieces(Int4 numBlocks);
extern SeqAlign *makeMultiPieceAlignments(Uint1Ptr query, Int4 numBlocks, Int4 queryLength, Uint1Ptr seq,
    Int4 seqLength, Int4 *blockStarts, Int4 *blockEnds, Int4 *allowedGaps, Int4 scoreThresholdMultipleBlock,
    SeqIdPtr subject_id, SeqIdPtr query_id, Int4* bestFirstBlock, Int4 *bestLastBlock, Nlm_FloatHi Lambda,
    Nlm_FloatHi K, Int4 searchSpaceSize);
extern void freeBestPairs(Int4 numBlocks);
extern void freeAlignPieceLists(Int4 numBlocks);
extern void freeBestScores(Int4 numBlocks);
}

// hack so I can catch memory leaks specific to this module, at the line where allocation occurs
#ifdef _DEBUG
#ifdef MemNew
#undef MemNew
#endif
#define MemNew(sz) memset(malloc(sz), 0, (sz))
#endif

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

// from cn3d_blast.cpp
extern BLAST_Matrix * CreateBLASTMatrix(const BlockMultipleAlignment *multipleForPSSM);

static Int4 Round(double d)
{
    if (d >= 0.0)
        return (Int4) (d + 0.5);
    else
        return (Int4) (d - 0.5);
}

static BlockMultipleAlignment * UnpackBlockAlignerSeqAlign(const CSeq_align& sa,
    const Sequence *master, const Sequence *query)
{
    auto_ptr<BlockMultipleAlignment> bma;

    // make sure the overall structure of this SeqAlign is what we expect
    if (!sa.IsSetDim() || sa.GetDim() != 2 ||
        !((sa.GetSegs().IsDisc() && sa.GetSegs().GetDisc().Get().size() > 0) || sa.GetSegs().IsDenseg()))
    {
        ERR_POST(Error << "Confused by block aligner's result format");
        return NULL;
    }

    // create new alignment structure
    BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
    (*seqs)[0] = master;
    (*seqs)[1] = query;
    bma.reset(new BlockMultipleAlignment(seqs, master->parentSet->alignmentManager));

    // get list of segs (can be a single or a set)
    CSeq_align_set::Tdata segs;
    if (sa.GetSegs().IsDisc())
        segs = sa.GetSegs().GetDisc().Get();
    else
        segs.push_back(CRef<CSeq_align>(const_cast<CSeq_align*>(&sa)));

    // loop through segs, adding aligned block for each starts pair that doesn't describe a gap
    CSeq_align_set::Tdata::const_iterator s, se = segs.end();
    CDense_seg::TStarts::const_iterator i_start;
    CDense_seg::TLens::const_iterator i_len;
    for (s=segs.begin(); s!=se; s++) {

        // check to make sure query is first id, master is second id
        if ((*s)->GetDim() != 2 || !(*s)->GetSegs().IsDenseg() || (*s)->GetSegs().GetDenseg().GetDim() != 2 ||
            (*s)->GetSegs().GetDenseg().GetIds().size() != 2 ||
            !query->identifier->MatchesSeqId((*s)->GetSegs().GetDenseg().GetIds().front().GetObject()) ||
            !master->identifier->MatchesSeqId((*s)->GetSegs().GetDenseg().GetIds().back().GetObject()))
        {
            ERR_POST(Error << "Confused by seg format in block aligner's result");
            return NULL;
        }

        int i, queryStart, masterStart, length;
        i_start = (*s)->GetSegs().GetDenseg().GetStarts().begin();
        i_len = (*s)->GetSegs().GetDenseg().GetLens().begin();
        for (i=0; i<(*s)->GetSegs().GetDenseg().GetNumseg(); i++) {

            // if either start is -1, this is a gap -> skip
            queryStart = *(i_start++);
            masterStart = *(i_start++);
            length = *(i_len++);
            if (queryStart < 0 || masterStart < 0 || length <= 0) continue;

            // add aligned block
            UngappedAlignedBlock *newBlock = new UngappedAlignedBlock(bma.get());
            newBlock->SetRangeOfRow(0, masterStart, masterStart + length - 1);
            newBlock->SetRangeOfRow(1, queryStart, queryStart + length - 1);
            newBlock->width = length;
            bma->AddAlignedBlockAtEnd(newBlock);
        }
    }

    // finalize the alignment
    if (!bma->AddUnalignedBlocks() || !bma->UpdateBlockMapAndColors(false)) {
        ERR_POST(Error << "Error finalizing alignment!");
        return NULL;
    }

    // get scores
    CSeq_align::TScore::const_iterator c, ce = sa.GetScore().end();
    for (c=sa.GetScore().begin(); c!=ce; c++) {
        if ((*c)->IsSetId() && (*c)->GetId().IsStr()) {

            // raw score
            if ((*c)->GetValue().IsInt() && (*c)->GetId().GetStr() == "score") {
                wxString score;
                score.Printf("raw score: %i", (*c)->GetValue().GetInt());
                bma->SetRowStatusLine(0, score.c_str());
                bma->SetRowStatusLine(1, score.c_str());
                break;
            }
        }
    }

    // success
    return bma.release();
}

void BlockAligner::CreateNewPairwiseAlignmentsByBlockAlignment(const BlockMultipleAlignment *multiple,
    const AlignmentList& toRealign, AlignmentList *newAlignments)
{
    // parameters passed to Alejandro's functions
    Int4 numBlocks;
    Uint1Ptr convertedQuery;
    Int4 queryLength;
    Int4 *blockStarts;
    Int4 *blockEnds;
    Int4 masterLength;
    BLAST_Score **thisScoreMat;
    Uint1Ptr masterSequence;
    Int4 *allowedGaps;
    SeqIdPtr subject_id;
    SeqIdPtr query_id;
    Int4 bestFirstBlock, bestLastBlock;
    Int4 searchSpaceSize;
    SeqAlignPtr results;
    SeqAlignPtr listOfSeqAligns;
    // the following would be command-line arguments to Alejandro's standalone program
    BLAST_Score scoreThresholdSingleBlock = 11;
    BLAST_Score scoreThresholdMultipleBlock = 11;
    Nlm_FloatHi Lambda = 0.0;
    Nlm_FloatHi K = 0.0;
    Nlm_FloatHi percentile = 0.9;
    Int4 gapAddition = 10;
    Nlm_FloatHi scaleMult = 1.0;

    searchSpaceSize = 0;

    newAlignments->clear();
    auto_ptr<BlockMultipleAlignment::UngappedAlignedBlockList> blocks(multiple->GetUngappedAlignedBlocks());
    numBlocks = blocks->size();
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks->end();

    // master sequence info
    masterLength = multiple->GetMaster()->Length();
    masterSequence = (Uint1Ptr) MemNew(sizeof(Uint1) * masterLength);
    int i;
    for (i=0; i<masterLength; i++)
        masterSequence[i] = ResToInt(multiple->GetMaster()->sequenceString[i]);
    subject_id = multiple->GetMaster()->parentSet->GetOrCreateBioseq(multiple->GetMaster())->id;

    // convert all sequences to Bioseqs
    multiple->GetMaster()->parentSet->CreateAllBioseqs(multiple);

    // create SeqAlign from this BlockMultipleAlignment
    listOfSeqAligns = multiple->CreateCSeqAlign();

    // set up block info
    allocateAlignPieceMemory(numBlocks);
    blockStarts = (Int4*) MemNew(sizeof(Int4) * masterLength);
    blockEnds = (Int4*) MemNew(sizeof(Int4) * masterLength);
    allowedGaps = (Int4*) MemNew(sizeof(Int4) * (masterLength - 1));
    for (i=0, b=blocks->begin(); b!=be; i++, b++) {
        const Block::Range *range = (*b)->GetRangeOfRow(0);
        blockStarts[i] = range->from;
        blockEnds[i] = range->to;
    }
    findAllowedGaps(listOfSeqAligns, numBlocks, allowedGaps, percentile, gapAddition);

    // set up PSSM
    BLAST_Matrix *matrix = CreateBLASTMatrix(multiple);
    thisScoreMat = matrix->matrix;

    AlignmentList::const_iterator s, se = toRealign.end();
    for (s=toRealign.begin(); s!=se; s++) {
        if (multiple && (*s)->GetMaster() != multiple->GetMaster())
            ERR_POST(Error << "master sequence mismatch");

        // slave sequence info
        const Sequence *query = (*s)->GetSequenceOfRow(1);
        queryLength = query->Length();
        convertedQuery = (Uint1Ptr) MemNew(sizeof(Uint1) * queryLength);
        for (i=0; i<queryLength; i++)
            convertedQuery[i] = ResToInt(query->sequenceString[i]);
        query_id = query->parentSet->GetOrCreateBioseq(query)->id;

        // actually do the block alignment
        findAlignPieces(convertedQuery, queryLength, numBlocks,
            blockStarts, blockEnds, masterLength, thisScoreMat, scoreThresholdSingleBlock);
        sortAlignPieces(numBlocks);
        results = makeMultiPieceAlignments(convertedQuery, numBlocks,
            queryLength, masterSequence, masterLength,
            blockStarts, blockEnds, allowedGaps, scoreThresholdMultipleBlock,
            subject_id, query_id, &bestFirstBlock, &bestLastBlock,
            Lambda, K, searchSpaceSize);

        // process results; assume first result SeqAlign is the highest scoring
        if (results) {
#ifdef _DEBUG
            AsnIoPtr aip = AsnIoOpen("seqalign-ba.txt", "w");
            SeqAlignAsnWrite(results, aip, NULL);
            AsnIoFree(aip, true);
#endif

            CSeq_align best;
            std::string err;
            BlockMultipleAlignment *newAlignment;
            if (!ConvertAsnFromCToCPP(results, (AsnWriteFunc) SeqAlignAsnWrite, &best, &err) ||
                (newAlignment=UnpackBlockAlignerSeqAlign(best, multiple->GetMaster(), query)) == NULL) {
                ERR_POST(Error << "conversion of results to C++ object failed: " << err);
            } else {
                newAlignments->push_back(newAlignment);
            }

            SeqAlignSetFree(results);
        }

        // cleanup
        MemFree(convertedQuery);
        freeBestPairs(numBlocks);
    }

    // cleanup
    BLAST_MatrixDestruct(matrix);
    MemFree(blockStarts);
    MemFree(blockEnds);
    MemFree(allowedGaps);
    SeqAlignSetFree(listOfSeqAligns);
    MemFree(masterSequence);
    freeAlignPieceLists(numBlocks);
    freeBestScores(numBlocks);
}

END_SCOPE(Cn3D)
