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

#include "cn3d/cn3d_blast.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/asn_converter.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

void BLASTer::CreateNewPairwiseAlignmentsByBlast(const Sequence *master,
    const SequenceList& newSequences, AlignmentList *newAlignments)
{
    // set up BLAST stuff
    BLAST_OptionsBlkPtr options = BLASTOptionNew("blastp", true);
    if (!options) {
        ERR_POST(Error << "BLASTOptionNew() failed");
        return;
    }
//    options->discontinuous = FALSE;

    // get C Bioseq for master
    Bioseq *masterBioseq = master->parentSet->GetOrCreateBioseq(master);

    std::string err;
    SequenceList::const_iterator s, se = newSequences.end();
    for (s=newSequences.begin(); s!=se; s++) {

        // get C Bioseq for slave
        Bioseq *slaveBioseq = master->parentSet->GetOrCreateBioseq(*s);

        // actually do the BLAST alignment
        SeqAlign *salp = BlastTwoSequences(masterBioseq, slaveBioseq, "blastp", options);

        // create new alignment structure
        BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
        (*seqs)[0] = master;
        (*seqs)[1] = *s;
        BlockMultipleAlignment *newAlignment =
            new BlockMultipleAlignment(seqs, master->parentSet->alignmentManager);

        // process the result
        if (!salp) {
            ERR_POST(Warning << "BlastTwoSequences() failed");
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
                !IsAMatch(master, *(sa.GetSegs().GetDenseg().GetIds().front())) ||
                !IsAMatch(*s, *(sa.GetSegs().GetDenseg().GetIds().back()))) {
                ERR_POST(Error << "Confused by BlastTwoSequences() result format");
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
}

END_SCOPE(Cn3D)

