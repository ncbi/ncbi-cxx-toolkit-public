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
        if (i < seqMtf->n) {
            for (j=0; j<seqMtf->AlphabetSize; j++) {
                matrix->matrix[i][LookupBLASTResidueNumberFromThreaderResidueNumber(j)] =
                    seqMtf->ww[i][j] / Threader::SCALING_FACTOR;
            }
            matrix->matrix[i][25] = BLAST_SCORE_MIN;
        } else {
            // fill in last row with BLAST_SCORE_MIN
            for (j=0; j<matrix->columns; j++)
                matrix->matrix[i][j] = BLAST_SCORE_MIN;
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

void BLASTer::CreateNewPairwiseAlignmentsByBlast(const Sequence *master,
    const SequenceList& newSequences, AlignmentList *newAlignments,
    const BlockMultipleAlignment *multipleForPSSM)
{
    // set up BLAST stuff
    BLAST_OptionsBlkPtr options = BLASTOptionNew("blastp", true);
    if (!options) {
        ERR_POST(Error << "BLASTOptionNew() failed");
        return;
    }

    // get C Bioseq for master
    Bioseq *masterBioseq = master->parentSet->GetOrCreateBioseq(master);

    // create BLAST_Matrix if using PSSM from multiple
    BLAST_Matrix *BLASTmatrix = NULL;
    SeqLocPtr masterSeqLoc = NULL, slaveSeqLoc = NULL;
    if (multipleForPSSM) {
        BLASTmatrix = CreateBLASTMatrix(multipleForPSSM);
        masterSeqLoc = (SeqLocPtr) MemNew(sizeof(SeqLoc));
        masterSeqLoc->choice = SEQLOC_WHOLE;
        masterSeqLoc->data.ptrvalue = masterBioseq->id;
        slaveSeqLoc = (SeqLocPtr) MemNew(sizeof(SeqLoc));
    }

    std::string err;
    SequenceList::const_iterator s, se = newSequences.end();
    for (s=newSequences.begin(); s!=se; s++) {

        // get C Bioseq for slave
        Bioseq *slaveBioseq = master->parentSet->GetOrCreateBioseq(*s);

        // actually do the BLAST alignment
        SeqAlign *salp = NULL;
        if (multipleForPSSM) {
            TESTMSG("calling BlastTwoSequencesByLocWithCallback()");
            slaveSeqLoc->choice = SEQLOC_WHOLE;
            slaveSeqLoc->data.ptrvalue = slaveBioseq->id;
            salp = BlastTwoSequencesByLocWithCallback(
                masterSeqLoc, slaveSeqLoc,
                "blastp", options,
                NULL, NULL,
                NULL,
                BLASTmatrix);
        } else {
            TESTMSG("calling BlastTwoSequences()");
            salp = BlastTwoSequences(masterBioseq, slaveBioseq, "blastp", options);
        }

        // create new alignment structure
        BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
        (*seqs)[0] = master;
        (*seqs)[1] = *s;
        BlockMultipleAlignment *newAlignment =
            new BlockMultipleAlignment(seqs, master->parentSet->alignmentManager);
        TESTMSG("sizeof BlockMultipleAlignment : " << sizeof(BlockMultipleAlignment));

        // process the result
        if (!salp) {
            ERR_POST(Warning << "BLAST failed to find a significant alignment");
        } else {

            // convert C SeqAlign to C++ for convenience
//            AsnIoPtr aip = AsnIoOpen("seqalign.txt", "w");
//            SeqAlignAsnWrite(salp, aip, NULL);
//            AsnIoFree(aip, true);
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
                !master->identifier->MatchesSeqId(*(sa.GetSegs().GetDenseg().GetIds().front())) ||
                !(*s)->identifier->MatchesSeqId(*(sa.GetSegs().GetDenseg().GetIds().back()))) {
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
            ERR_POST(Error << "CreateNewPairwiseAlignments() - error finalizing alignment");
            delete newAlignment;
        }
    }

    BLASTOptionDelete(options);
    if (BLASTmatrix) BLAST_MatrixDestruct(BLASTmatrix);
    if (masterSeqLoc) MemFree(masterSeqLoc);
    if (slaveSeqLoc) MemFree(slaveSeqLoc);
}

END_SCOPE(Cn3D)

