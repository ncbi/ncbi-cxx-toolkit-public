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
*      module for aligning with BLAST and related algorithms
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2002/06/17 19:31:54  thiessen
* set db_length in blast options
*
* Revision 1.12  2002/06/13 14:54:07  thiessen
* add sort by self-hit
*
* Revision 1.11  2002/06/13 13:32:39  thiessen
* add self-hit calculation
*
* Revision 1.10  2002/05/22 17:17:08  thiessen
* progress on BLAST interface ; change custom spin ctrl implementation
*
* Revision 1.9  2002/05/17 19:10:27  thiessen
* preliminary range restriction for BLAST/PSSM
*
* Revision 1.8  2002/05/07 20:22:47  thiessen
* fix for BLAST/PSSM
*
* Revision 1.7  2002/05/02 18:40:25  thiessen
* do BLAST/PSSM for debug builds only, for testing
*
* Revision 1.6  2002/03/28 14:06:02  thiessen
* preliminary BLAST/PSSM ; new CD startup style
*
* Revision 1.5  2001/11/27 16:26:07  thiessen
* major update to data management system
*
* Revision 1.4  2001/10/18 19:57:32  thiessen
* fix redundant creation of C bioseqs
*
* Revision 1.3  2001/09/27 15:37:58  thiessen
* decouple sequence import and BLAST
*
* Revision 1.2  2001/09/19 22:55:39  thiessen
* add preliminary net import and BLAST
*
* Revision 1.1  2001/09/18 03:10:45  thiessen
* add preliminary sequence import pipeline
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#if defined(__WXMSW__)
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>

// C stuff
#include <objseq.h>
#include <blast.h>
#include <blastkar.h>
#include <thrdatd.h>
#include <thrddecl.h>

#include "cn3d/cn3d_blast.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/asn_converter.hpp"
#include "cn3d/molecule_identifier.hpp"
#include "cn3d/cn3d_threader.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

const std::string BLASTer::BLASTResidues = "-ABCDEFGHIKLMNPQRSTVWXYZU*";

// gives BLAST residue number for a character (or # for 'X' if char == -1)
static int LookupBLASTResidueNumberFromThreaderResidueNumber(char r)
{
    typedef std::map < char, int > Char2Int;
    static Char2Int charMap;

    if (charMap.size() == 0) {
        for (int i=0; i<BLASTer::BLASTResidues.size(); i++)
            charMap[BLASTer::BLASTResidues[i]] = i;
    }

    return charMap.find(
            (r >= 0 && r < Threader::ThreaderResidues.size()) ? Threader::ThreaderResidues[r] : 'X'
        )->second;
}

#ifdef _DEBUG
#define PRINT_PSSM // for testing/debugging
#endif

static BLAST_Matrix * CreateBLASTMatrix(const BlockMultipleAlignment *multipleForPSSM)
{
    // for now, use threader's SeqMtf
    Seq_Mtf *seqMtf = Threader::CreateSeqMtf(multipleForPSSM, 1.0);

    BLAST_Matrix *matrix = (BLAST_Matrix *) MemNew(sizeof(BLAST_Matrix));
    matrix->is_prot = TRUE;
    matrix->name = StringSave("BLOSUM62");
    matrix->karlinK = 0.0410; // default for blosum62
    matrix->rows = seqMtf->n + 1;
    matrix->columns = 26;

#ifdef PRINT_PSSM
    FILE *f = fopen("blast_matrix.txt", "w");
#endif

    int i, j;
    matrix->matrix = (Int4 **) MemNew(matrix->rows * sizeof(Int4 *));
    for (i=0; i<matrix->rows; i++) {
        matrix->matrix[i] = (Int4 *) MemNew(matrix->columns * sizeof(Int4));
#ifdef PRINT_PSSM
        fprintf(f, "matrix %i : ", i);
#endif
        // initialize all rows with BLAST_SCORE_MIN
        for (j=0; j<matrix->columns; j++)
            matrix->matrix[i][j] = BLAST_SCORE_MIN;
        // set scores from threader matrix
        if (i < seqMtf->n) {
            for (j=0; j<seqMtf->AlphabetSize; j++) {
                matrix->matrix[i][LookupBLASTResidueNumberFromThreaderResidueNumber(j)] =
                    seqMtf->ww[i][j] / Threader::SCALING_FACTOR;
            }
        }
#ifdef PRINT_PSSM
        for (j=0; j<seqMtf->AlphabetSize; j++)
            fprintf(f, "%i ", matrix->matrix[i][LookupBLASTResidueNumberFromThreaderResidueNumber(j)]);
        fprintf(f, "\n");
#endif
    }

    matrix->posFreqs = (Nlm_FloatHi **) MemNew(matrix->rows * sizeof(Nlm_FloatHi *));
    for (i=0; i<matrix->rows; i++) {
        matrix->posFreqs[i] = (Nlm_FloatHi *) MemNew(matrix->columns * sizeof(Nlm_FloatHi));
#ifdef PRINT_PSSM
        fprintf(f, "freqs %i : ", i);
#endif
        if (i < seqMtf->n) {
            for (j=0; j<seqMtf->AlphabetSize; j++) {
                matrix->posFreqs[i][LookupBLASTResidueNumberFromThreaderResidueNumber(j)] =
                    seqMtf->freqs[i][j] / Threader::SCALING_FACTOR;
            }
        }
#ifdef PRINT_PSSM
        for (j=0; j<matrix->columns; j++)
            fprintf(f, "%g ", matrix->posFreqs[i][j]);
        fprintf(f, "\n");
#endif
    }

#ifdef PRINT_PSSM
    fclose(f);
#endif

    FreeSeqMtf(seqMtf);
    return matrix;
}

static BLAST_OptionsBlkPtr CreateBlastOptionsBlk(void)
{
    BLAST_OptionsBlkPtr bob = BLASTOptionNew("blastp", true);
    bob->db_length = 1000000;   // approx. # aligned columns in CDD database
    return bob;
}

void BLASTer::CreateNewPairwiseAlignmentsByBlast(const BlockMultipleAlignment *multiple,
    const AlignmentList& toRealign, AlignmentList *newAlignments, bool usePSSM)
{
    if (usePSSM && !multiple) {
        ERR_POST(Error << "usePSSM true, but NULL multiple alignment");
        return;
    }

    // set up BLAST stuff
    BLAST_OptionsBlkPtr options = CreateBlastOptionsBlk();
    if (!options) {
        ERR_POST(Error << "BLASTOptionNew() failed");
        return;
    }

    // get master sequence
    const Sequence *masterSeq = NULL;
    if (multiple)
        masterSeq = multiple->GetMaster();
    else if (toRealign.size() > 0)
        masterSeq = toRealign.front()->GetMaster();
    if (!masterSeq) {
        ERR_POST(Error << "can't get master sequence");
        return;
    }

    // get C Bioseq for master
    Bioseq *masterBioseq = masterSeq->parentSet->GetOrCreateBioseq(masterSeq);

    // create Seq-loc interval for master
    SeqLocPtr masterSeqLoc = (SeqLocPtr) MemNew(sizeof(SeqLoc));
    masterSeqLoc->choice = SEQLOC_INT;
    SeqIntPtr masterSeqInt = SeqIntNew();
    masterSeqLoc->data.ptrvalue = masterSeqInt;
    masterSeqInt->id = masterBioseq->id;
    auto_ptr<BlockMultipleAlignment::UngappedAlignedBlockList> uaBlocks;
    if (multiple)
        uaBlocks.reset(multiple->GetUngappedAlignedBlocks());
    if (multiple && uaBlocks->size() > 0) {
        masterSeqInt->from = uaBlocks->front()->GetRangeOfRow(0)->from;
        masterSeqInt->to = uaBlocks->back()->GetRangeOfRow(0)->to;
    } else {
        masterSeqInt->from = 0;
        masterSeqInt->to = masterSeq->Length() - 1;
    }
    SeqLocPtr slaveSeqLoc = (SeqLocPtr) MemNew(sizeof(SeqLoc));
    slaveSeqLoc->choice = SEQLOC_INT;
    SeqIntPtr slaveSeqInt = SeqIntNew();
    slaveSeqLoc->data.ptrvalue = slaveSeqInt;

    // create BLAST_Matrix if using PSSM from multiple
    BLAST_Matrix *BLASTmatrix = NULL;
    if (usePSSM)
        BLASTmatrix = CreateBLASTMatrix(multiple);

    std::string err;
    AlignmentList::const_iterator s, se = toRealign.end();
    for (s=toRealign.begin(); s!=se; s++) {
        if (multiple && (*s)->GetMaster() != multiple->GetMaster())
            ERR_POST(Error << "master sequence mismatch");

        // get C Bioseq for slave of each incoming (pairwise) alignment
        const Sequence *slaveSeq = (*s)->GetSequenceOfRow(1);
        Bioseq *slaveBioseq = slaveSeq->parentSet->GetOrCreateBioseq(slaveSeq);

        // setup Seq-loc interval for slave
        slaveSeqInt->id = slaveBioseq->id;
        slaveSeqInt->from =
            ((*s)->alignFrom >= 0 && (*s)->alignFrom < slaveSeq->Length()) ?
                (*s)->alignFrom : 0;
        slaveSeqInt->to =
            ((*s)->alignTo >= 0 && (*s)->alignTo < slaveSeq->Length()) ?
                (*s)->alignTo : slaveSeq->Length() - 1;
        TESTMSG("for BLAST: master range " <<
                (masterSeqInt->from + 1) << " to " << (masterSeqInt->to + 1) << ", slave range " <<
                (slaveSeqInt->from + 1) << " to " << (slaveSeqInt->to + 1));

        // actually do the BLAST alignment
        SeqAlign *salp = NULL;
        if (usePSSM) {
            TESTMSG("calling BlastTwoSequencesByLocWithCallback()");
            salp = BlastTwoSequencesByLocWithCallback(
                masterSeqLoc, slaveSeqLoc,
                "blastp", options,
                NULL, NULL, NULL,
                BLASTmatrix);
        } else {
            TESTMSG("calling BlastTwoSequencesyLoc()");
            salp = BlastTwoSequencesByLoc(masterSeqLoc, slaveSeqLoc, "blastp", options);
        }

        // create new alignment structure
        BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
        (*seqs)[0] = masterSeq;
        (*seqs)[1] = slaveSeq;
        BlockMultipleAlignment *newAlignment =
            new BlockMultipleAlignment(seqs, slaveSeq->parentSet->alignmentManager);

        // process the result
        if (!salp) {
            ERR_POST(Warning << "BLAST failed to find a significant alignment");
        } else {

            // convert C SeqAlign to C++ for convenience
#ifdef _DEBUG
            AsnIoPtr aip = AsnIoOpen("seqalign.txt", "w");
            SeqAlignAsnWrite(salp, aip, NULL);
            AsnIoFree(aip, true);
#endif
            CSeq_align sa;
            bool okay = ConvertAsnFromCToCPP(salp, (AsnWriteFunc) SeqAlignAsnWrite, &sa, &err);
            SeqAlignFree(salp);
            if (!okay) {
                ERR_POST(Error << "Conversion of SeqAlign to C++ object failed");
                continue;
            }

            // make sure the structure of this SeqAlign is what we expect
            if (!sa.IsSetDim() || sa.GetDim() != 2 ||
                !sa.GetSegs().IsDenseg() || sa.GetSegs().GetDenseg().GetDim() != 2 ||
                !masterSeq->identifier->MatchesSeqId(*(sa.GetSegs().GetDenseg().GetIds().front())) ||
                !slaveSeq->identifier->MatchesSeqId(*(sa.GetSegs().GetDenseg().GetIds().back()))) {
                ERR_POST(Error << "Confused by BLAST result format");
                continue;
            }

            // fill out with blocks from BLAST alignment
            CDense_seg::TStarts::const_iterator iStart = sa.GetSegs().GetDenseg().GetStarts().begin();
            CDense_seg::TLens::const_iterator iLen = sa.GetSegs().GetDenseg().GetLens().begin();
            for (int i=0; i<sa.GetSegs().GetDenseg().GetNumseg(); i++) {
                int masterStart = *(iStart++), slaveStart = *(iStart++), len = *(iLen++);
                if (masterStart >= 0 && slaveStart >= 0 && len > 0) {
                    UngappedAlignedBlock *newBlock = new UngappedAlignedBlock(newAlignment);
                    newBlock->SetRangeOfRow(0, masterStart, masterStart + len - 1);
                    newBlock->SetRangeOfRow(1, slaveStart, slaveStart + len - 1);
                    newBlock->width = len;
                    newAlignment->AddAlignedBlockAtEnd(newBlock);
                }
            }

            // put score in status
            if (sa.IsSetScore()) {
                CSeq_align::TScore::const_iterator sc, sce = sa.GetScore().end();
                for (sc=sa.GetScore().begin(); sc!=sce; sc++) {
                    if ((*sc)->GetValue().IsReal() && (*sc)->IsSetId() &&
                        (*sc)->GetId().IsStr() && (*sc)->GetId().GetStr() == "e_value") {
                        wxString status;
                        status.Printf("E-value %g", (*sc)->GetValue().GetReal());
                        newAlignment->SetRowStatusLine(0, status.c_str());
                        newAlignment->SetRowStatusLine(1, status.c_str());
                        break;
                    }
                }
            }
        }

        // finalize and and add new alignment to list
        if (newAlignment->AddUnalignedBlocks() && newAlignment->UpdateBlockMapAndColors(false)) {
            newAlignments->push_back(newAlignment);
        } else {
            ERR_POST(Error << "error finalizing alignment");
            delete newAlignment;
        }
    }

    BLASTOptionDelete(options);
    if (BLASTmatrix) BLAST_MatrixDestruct(BLASTmatrix);
    masterSeqInt->id = NULL;    // don't free Seq-id, since it belongs to the Bioseq
    SeqIntFree(masterSeqInt);
    MemFree(masterSeqLoc);
    slaveSeqInt->id = NULL;
    SeqIntFree(slaveSeqInt);
    MemFree(slaveSeqLoc);
}

void BLASTer::CalculateSelfHitScores(const BlockMultipleAlignment *multiple)
{
    if (!multiple) {
        ERR_POST(Error << "NULL multiple alignment");
        return;
    }

    // set up BLAST stuff
    BLAST_OptionsBlkPtr options = CreateBlastOptionsBlk();
    if (!options) {
        ERR_POST(Error << "BLASTOptionNew() failed");
        return;
    }

    // get master sequence
    const Sequence *masterSeq = multiple->GetMaster();
    Bioseq *masterBioseq = masterSeq->parentSet->GetOrCreateBioseq(masterSeq);

    // create Seq-loc interval for master
    SeqLocPtr masterSeqLoc = (SeqLocPtr) MemNew(sizeof(SeqLoc));
    masterSeqLoc->choice = SEQLOC_INT;
    SeqIntPtr masterSeqInt = SeqIntNew();
    masterSeqLoc->data.ptrvalue = masterSeqInt;
    masterSeqInt->id = masterBioseq->id;
    auto_ptr<BlockMultipleAlignment::UngappedAlignedBlockList>
        uaBlocks(multiple->GetUngappedAlignedBlocks());
    if (uaBlocks->size() == 0) {
        ERR_POST(Error << "Self-hit requires at least one aligned block");
        return;
    }
    masterSeqInt->from = uaBlocks->front()->GetRangeOfRow(0)->from;
    masterSeqInt->to = uaBlocks->back()->GetRangeOfRow(0)->to;
    SeqLocPtr slaveSeqLoc = (SeqLocPtr) MemNew(sizeof(SeqLoc));
    slaveSeqLoc->choice = SEQLOC_INT;
    SeqIntPtr slaveSeqInt = SeqIntNew();
    slaveSeqLoc->data.ptrvalue = slaveSeqInt;

    // create BLAST_Matrix if using PSSM from multiple
    BLAST_Matrix *BLASTmatrix = CreateBLASTMatrix(multiple);

    std::string err;
    int row;
    for (row=0; row<multiple->NRows(); row++) {

        // get C Bioseq for each slave
        const Sequence *slaveSeq = multiple->GetSequenceOfRow(row);
        Bioseq *slaveBioseq = slaveSeq->parentSet->GetOrCreateBioseq(slaveSeq);

        // setup Seq-loc interval for slave
        slaveSeqInt->id = slaveBioseq->id;
        slaveSeqInt->from = uaBlocks->front()->GetRangeOfRow(row)->from;
        slaveSeqInt->to = uaBlocks->back()->GetRangeOfRow(row)->to;
//        TESTMSG("for BLAST: master range " <<
//                (masterSeqInt->from + 1) << " to " << (masterSeqInt->to + 1) << ", slave range " <<
//                (slaveSeqInt->from + 1) << " to " << (slaveSeqInt->to + 1));

        // actually do the BLAST alignment
//        TESTMSG("calling BlastTwoSequencesByLocWithCallback()");
        SeqAlign *salp = BlastTwoSequencesByLocWithCallback(
            masterSeqLoc, slaveSeqLoc,
            "blastp", options,
            NULL, NULL, NULL,
            BLASTmatrix);

        // process the result
        double score = -1.0;
        if (salp) {
            // convert C SeqAlign to C++ for convenience
            CSeq_align sa;
            bool okay = ConvertAsnFromCToCPP(salp, (AsnWriteFunc) SeqAlignAsnWrite, &sa, &err);
            SeqAlignFree(salp);
            if (!okay) {
                ERR_POST(Error << "Conversion of SeqAlign to C++ object failed");
                continue;
            }

            // get score
            if (sa.IsSetScore()) {
                CSeq_align::TScore::const_iterator sc, sce = sa.GetScore().end();
                for (sc=sa.GetScore().begin(); sc!=sce; sc++) {
                    if ((*sc)->GetValue().IsReal() && (*sc)->IsSetId() &&
                        (*sc)->GetId().IsStr() && (*sc)->GetId().GetStr() == "e_value") {
                        score = (*sc)->GetValue().GetReal();
                        break;
                    }
                }
            }
            if (score < 0.0) ERR_POST(Error << "Got back Seq-align with no E-value");

        }

        // set score in row
        multiple->SetRowDouble(row, score);
        wxString status;
        if (score >= 0.0)
            status.Printf("Self hit E-value: %g", score);
        else
            status = "No detectable self hit";
        multiple->SetRowStatusLine(row, status.c_str());
    }

    // print out overall self-hit rate
    int nSelfHits = 0;
    static const double threshold = 0.01;
    for (row=0; row<multiple->NRows(); row++) {
        if (multiple->GetRowDouble(row) >= 0.0 && multiple->GetRowDouble(row) <= threshold)
            nSelfHits++;
    }
    ERR_POST(Info << "Self hits with E-value <= " << setprecision(3) << threshold << ": "
        << (100.0*nSelfHits/multiple->NRows()) << "% ("
        << nSelfHits << '/' << multiple->NRows() << ')');

    BLASTOptionDelete(options);
    BLAST_MatrixDestruct(BLASTmatrix);
    masterSeqInt->id = NULL;    // don't free Seq-id, since it belongs to the Bioseq
    SeqIntFree(masterSeqInt);
    MemFree(masterSeqLoc);
    slaveSeqInt->id = NULL;
    SeqIntFree(slaveSeqInt);
    MemFree(slaveSeqLoc);
}

END_SCOPE(Cn3D)

