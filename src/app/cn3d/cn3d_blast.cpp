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
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>

// C stuff
#include <objseq.h>
#include <blast.h>
#include <blastkar.h>

#include "cn3d_blast.hpp"
#include "structure_set.hpp"
#include "sequence_set.hpp"
#include "block_multiple_alignment.hpp"
#include "cn3d_tools.hpp"
#include "asn_converter.hpp"
#include "molecule_identifier.hpp"

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

#ifdef _DEBUG
#define PRINT_PSSM // for testing/debugging
#endif

static BLAST_OptionsBlkPtr CreateBlastOptionsBlk(void)
{
    BLAST_OptionsBlkPtr bob = BLASTOptionNew("blastp", true);
    bob->db_length = 2717223;    // size of CDD database v1.62
    bob->scalingFactor = 1.0;
    return bob;
}

static void SetEffectiveSearchSpaceSize(BLAST_OptionsBlkPtr bob, Int4 queryLength)
{
    bob->searchsp_eff = BLASTCalculateSearchSpace(bob, 1, bob->db_length, queryLength);
}

void BLASTer::CreateNewPairwiseAlignmentsByBlast(const BlockMultipleAlignment *multiple,
    const AlignmentList& toRealign, AlignmentList *newAlignments, bool usePSSM)
{
    if (usePSSM && (!multiple || multiple->HasNoAlignedBlocks())) {
        ERRORMSG("usePSSM true, but NULL or zero-aligned block multiple alignment");
        return;
    }

    // set up BLAST stuff
    BLAST_OptionsBlkPtr options = CreateBlastOptionsBlk();
    if (!options) {
        ERRORMSG("BLASTOptionNew() failed");
        return;
    }

    // create Seq-loc intervals
    SeqLocPtr masterSeqLoc = (SeqLocPtr) MemNew(sizeof(SeqLoc));
    masterSeqLoc->choice = SEQLOC_INT;
    SeqIntPtr masterSeqInt = SeqIntNew();
    masterSeqLoc->data.ptrvalue = masterSeqInt;
    if (multiple) {
        BlockMultipleAlignment::UngappedAlignedBlockList uaBlocks;
        multiple->GetUngappedAlignedBlocks(&uaBlocks);
        if (uaBlocks.size() > 0) {
            int excess = 0;
            if (!RegistryGetInteger(REG_ADVANCED_SECTION, REG_FOOTPRINT_RES, &excess))
                WARNINGMSG("Can't get footprint excess residues from registry");
            masterSeqInt->from = uaBlocks.front()->GetRangeOfRow(0)->from - excess;
            if (masterSeqInt->from < 0)
                masterSeqInt->from = 0;
            masterSeqInt->to = uaBlocks.back()->GetRangeOfRow(0)->to + excess;
            if (masterSeqInt->to >= multiple->GetMaster()->Length())
                masterSeqInt->to = multiple->GetMaster()->Length() - 1;
        } else {
            masterSeqInt->from = 0;
            masterSeqInt->to = multiple->GetMaster()->Length() - 1;
        }
    }
    SeqLocPtr slaveSeqLoc = (SeqLocPtr) MemNew(sizeof(SeqLoc));
    slaveSeqLoc->choice = SEQLOC_INT;
    SeqIntPtr slaveSeqInt = SeqIntNew();
    slaveSeqLoc->data.ptrvalue = slaveSeqInt;

    // create BLAST_Matrix if using PSSM from multiple
    const BLAST_Matrix *BLASTmatrix = NULL;
    if (usePSSM) BLASTmatrix = multiple->GetPSSM();

    string err;
    AlignmentList::const_iterator s, se = toRealign.end();
    for (s=toRealign.begin(); s!=se; ++s) {
        if (multiple && (*s)->GetMaster() != multiple->GetMaster())
            ERRORMSG("master sequence mismatch");

        // get C Bioseq for master
        const Sequence *masterSeq = multiple ? multiple->GetMaster() : (*s)->GetMaster();
        Bioseq *masterBioseq = masterSeq->parentSet->GetOrCreateBioseq(masterSeq);
        masterSeqInt->id = masterBioseq->id;

        // override master alignment interval if specified
        if (!multiple) {
            if ((*s)->alignMasterFrom >= 0 && (*s)->alignMasterFrom < masterSeq->Length() &&
                (*s)->alignMasterTo >= 0 && (*s)->alignMasterTo < masterSeq->Length() &&
                (*s)->alignMasterFrom <= (*s)->alignMasterTo)
            {
                masterSeqInt->from = (*s)->alignMasterFrom;
                masterSeqInt->to = (*s)->alignMasterTo;
            } else {
                masterSeqInt->from = 0;
                masterSeqInt->to = masterSeq->Length() - 1;
            }
        }

        // get C Bioseq for slave of each incoming (pairwise) alignment
        const Sequence *slaveSeq = (*s)->GetSequenceOfRow(1);
        Bioseq *slaveBioseq = slaveSeq->parentSet->GetOrCreateBioseq(slaveSeq);

        // setup Seq-loc interval for slave
        slaveSeqInt->id = slaveBioseq->id;
        slaveSeqInt->from =
            ((*s)->alignSlaveFrom >= 0 && (*s)->alignSlaveFrom < slaveSeq->Length()) ?
                (*s)->alignSlaveFrom : 0;
        slaveSeqInt->to =
            ((*s)->alignSlaveTo >= 0 && (*s)->alignSlaveTo < slaveSeq->Length()) ?
                (*s)->alignSlaveTo : slaveSeq->Length() - 1;
        INFOMSG("for BLAST: master range " <<
                (masterSeqInt->from + 1) << " to " << (masterSeqInt->to + 1) << ", slave range " <<
                (slaveSeqInt->from + 1) << " to " << (slaveSeqInt->to + 1));

        // set search space size
        SetEffectiveSearchSpaceSize(options, slaveSeqInt->to - slaveSeqInt->from + 1);
        INFOMSG("effective search space size: " << ((int) options->searchsp_eff));

        // actually do the BLAST alignment
        SeqAlign *salp = NULL;
        if (usePSSM) {
            INFOMSG("calling BlastTwoSequencesByLocWithCallback(); PSSM db_length "
                << (int) options->db_length << ", K " << BLASTmatrix->karlinK);
            salp = BlastTwoSequencesByLocWithCallback(
                masterSeqLoc, slaveSeqLoc,
                "blastp", options,
                NULL, NULL, NULL,
                const_cast<BLAST_Matrix*>(BLASTmatrix));
        } else {
            INFOMSG("calling BlastTwoSequencesByLoc()");
            salp = BlastTwoSequencesByLoc(masterSeqLoc, slaveSeqLoc, "blastp", options);
        }

        // create new alignment structure
        BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
        (*seqs)[0] = masterSeq;
        (*seqs)[1] = slaveSeq;
        BlockMultipleAlignment *newAlignment =
            new BlockMultipleAlignment(seqs, slaveSeq->parentSet->alignmentManager);
        newAlignment->SetRowDouble(0, kMax_Double);
        newAlignment->SetRowDouble(1, kMax_Double);

        // process the result
        if (!salp) {
            WARNINGMSG("BLAST failed to find a significant alignment");
        } else {

            // convert C SeqAlign to C++ for convenience
#ifdef _DEBUG
            AsnIoPtr aip = AsnIoOpen("seqalign.txt", "w");
            SeqAlignAsnWrite(salp, aip, NULL);
            AsnIoFree(aip, true);
#endif
            CSeq_align sa;
            bool okay = ConvertAsnFromCToCPP(salp, (AsnWriteFunc) SeqAlignAsnWrite, &sa, &err);
            SeqAlignSetFree(salp);
            if (!okay) {
                ERRORMSG("Conversion of SeqAlign to C++ object failed");
                continue;
            }

            // make sure the structure of this SeqAlign is what we expect
            if (!sa.IsSetDim() || sa.GetDim() != 2 ||
                !sa.GetSegs().IsDenseg() || sa.GetSegs().GetDenseg().GetDim() != 2 ||
                !masterSeq->identifier->MatchesSeqId(*(sa.GetSegs().GetDenseg().GetIds().front())) ||
                !slaveSeq->identifier->MatchesSeqId(*(sa.GetSegs().GetDenseg().GetIds().back()))) {
                ERRORMSG("Confused by BLAST result format");
                continue;
            }

            // fill out with blocks from BLAST alignment
            CDense_seg::TStarts::const_iterator iStart = sa.GetSegs().GetDenseg().GetStarts().begin();
            CDense_seg::TLens::const_iterator iLen = sa.GetSegs().GetDenseg().GetLens().begin();
#ifdef _DEBUG
            int raw_pssm = 0, raw_bl62 = 0;
#endif
            for (int i=0; i<sa.GetSegs().GetDenseg().GetNumseg(); ++i) {
                int masterStart = *(iStart++), slaveStart = *(iStart++), len = *(iLen++);
                if (masterStart >= 0 && slaveStart >= 0 && len > 0) {
                    UngappedAlignedBlock *newBlock = new UngappedAlignedBlock(newAlignment);
                    newBlock->SetRangeOfRow(0, masterStart, masterStart + len - 1);
                    newBlock->SetRangeOfRow(1, slaveStart, slaveStart + len - 1);
                    newBlock->width = len;
                    newAlignment->AddAlignedBlockAtEnd(newBlock);

#ifdef _DEBUG
                    // calculate score manually, to compare with that returned by BLAST
                    for (int j=0; j<len; ++j) {
                        if (usePSSM)
                            raw_pssm += BLASTmatrix->matrix
                                [masterStart + j]
                                [LookupNCBIStdaaNumberFromCharacter(slaveSeq->sequenceString[slaveStart + j])];
                        raw_bl62 += GetBLOSUM62Score(
                            masterSeq->sequenceString[masterStart + j],
                            slaveSeq->sequenceString[slaveStart + j]);
                    }
#endif
                }
#ifdef _DEBUG
                else if ((masterStart < 0 || slaveStart < 0) && len > 0) {
                    int gap = options->gap_open + options->gap_extend * len;
                    if (usePSSM) raw_pssm -= gap;
                    raw_bl62 -= gap;
                }
#endif
            }
#ifdef _DEBUG
            TRACEMSG("Calculated raw score by PSSM: " << raw_pssm);
            TRACEMSG("Calculated raw score by BLOSUM62: " << raw_bl62);
#endif

            // get scores
            if (sa.IsSetScore()) {
                wxString scores;
                scores.Printf("BLAST result for %s vs. %s:",
                    masterSeq->identifier->ToString().c_str(),
                    slaveSeq->identifier->ToString().c_str());

                CSeq_align::TScore::const_iterator sc, sce = sa.GetScore().end();
                for (sc=sa.GetScore().begin(); sc!=sce; ++sc) {
                    if ((*sc)->IsSetId() && (*sc)->GetId().IsStr()) {

                        // E-value (put in status line and double values)
                        if ((*sc)->GetValue().IsReal() && (*sc)->GetId().GetStr() == "e_value") {
                            newAlignment->SetRowDouble(0, (*sc)->GetValue().GetReal());
                            newAlignment->SetRowDouble(1, (*sc)->GetValue().GetReal());
                            wxString status;
                            status.Printf("E-value %g", (*sc)->GetValue().GetReal());
                            newAlignment->SetRowStatusLine(0, status.c_str());
                            newAlignment->SetRowStatusLine(1, status.c_str());
                            wxString tmp = scores;
                            scores.Printf("%s E-value: %g", tmp.c_str(), (*sc)->GetValue().GetReal());
                        }

                        // raw score
                        if ((*sc)->GetValue().IsInt() && (*sc)->GetId().GetStr() == "score") {
                            wxString tmp = scores;
                            scores.Printf("%s raw: %i", tmp.c_str(), (*sc)->GetValue().GetInt());
                        }

                        // bit score
                        if ((*sc)->GetValue().IsReal() && (*sc)->GetId().GetStr() == "bit_score") {
                            wxString tmp = scores;
                            scores.Printf("%s bit score: %g", tmp.c_str(), (*sc)->GetValue().GetReal());
                        }
                    }
                }

                INFOMSG(scores.c_str());
            }
        }

        // finalize and and add new alignment to list
        if (newAlignment->AddUnalignedBlocks() && newAlignment->UpdateBlockMapAndColors(false)) {
            newAlignments->push_back(newAlignment);
        } else {
            ERRORMSG("error finalizing alignment");
            delete newAlignment;
        }
    }

    BLASTOptionDelete(options);
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
        ERRORMSG("NULL multiple alignment");
        return;
    }

    // set up BLAST stuff
    BLAST_OptionsBlkPtr options = CreateBlastOptionsBlk();
    if (!options) {
        ERRORMSG("BLASTOptionNew() failed");
        return;
    }

    // get master sequence
    const Sequence *masterSeq = multiple->GetMaster();
    Bioseq *masterBioseq = masterSeq->parentSet->GetOrCreateBioseq(masterSeq);

    // create Seq-loc interval for master and slave
    SeqLocPtr masterSeqLoc = (SeqLocPtr) MemNew(sizeof(SeqLoc));
    masterSeqLoc->choice = SEQLOC_INT;
    SeqIntPtr masterSeqInt = SeqIntNew();
    masterSeqLoc->data.ptrvalue = masterSeqInt;
    masterSeqInt->id = masterBioseq->id;
    BlockMultipleAlignment::UngappedAlignedBlockList uaBlocks;
    multiple->GetUngappedAlignedBlocks(&uaBlocks);
    if (uaBlocks.size() == 0) {
        ERRORMSG("Self-hit requires at least one aligned block");
        return;
    }
    int excess = 0;
    if (!RegistryGetInteger(REG_ADVANCED_SECTION, REG_FOOTPRINT_RES, &excess))
        WARNINGMSG("Can't get footprint excess residues from registry");
    masterSeqInt->from = uaBlocks.front()->GetRangeOfRow(0)->from - excess;
    if (masterSeqInt->from < 0) masterSeqInt->from = 0;
    masterSeqInt->to = uaBlocks.back()->GetRangeOfRow(0)->to + excess;
    if (masterSeqInt->to >= masterSeq->Length()) masterSeqInt->to = masterSeq->Length() - 1;

    SeqLocPtr slaveSeqLoc = (SeqLocPtr) MemNew(sizeof(SeqLoc));
    slaveSeqLoc->choice = SEQLOC_INT;
    SeqIntPtr slaveSeqInt = SeqIntNew();
    slaveSeqLoc->data.ptrvalue = slaveSeqInt;

    // create BLAST_Matrix if using PSSM from multiple
    const BLAST_Matrix *BLASTmatrix = multiple->GetPSSM();

    string err;
    int row;
    for (row=0; row<multiple->NRows(); ++row) {

        // get C Bioseq for each slave
        const Sequence *slaveSeq = multiple->GetSequenceOfRow(row);
        Bioseq *slaveBioseq = slaveSeq->parentSet->GetOrCreateBioseq(slaveSeq);

        // setup Seq-loc interval for slave
        slaveSeqInt->id = slaveBioseq->id;
        slaveSeqInt->from = uaBlocks.front()->GetRangeOfRow(row)->from - excess;
        if (slaveSeqInt->from < 0) slaveSeqInt->from = 0;
        slaveSeqInt->to = uaBlocks.back()->GetRangeOfRow(row)->to + excess;
        if (slaveSeqInt->to >= slaveSeq->Length()) slaveSeqInt->to = slaveSeq->Length() - 1;
//        TESTMSG("for BLAST: master range " <<
//                (masterSeqInt->from + 1) << " to " << (masterSeqInt->to + 1) << ", slave range " <<
//                (slaveSeqInt->from + 1) << " to " << (slaveSeqInt->to + 1));

        // set search space size
        SetEffectiveSearchSpaceSize(options, slaveSeqInt->to - slaveSeqInt->from + 1);

        // actually do the BLAST alignment

//        TRACEMSG("calling BlastTwoSequencesByLocWithCallback()");
        SeqAlign *salp = BlastTwoSequencesByLocWithCallback(
            masterSeqLoc, slaveSeqLoc,
            "blastp", options,
            NULL, NULL, NULL,
            const_cast<BLAST_Matrix*>(BLASTmatrix));

//        TRACEMSG("calling B2SPssmMultipleQueries()");
//        SeqLocPtr target[1];
//        target[0] = slaveSeqLoc;
//        options->searchsp_eff = 0;
//        SeqAlignPtr *psalp = B2SPssmMultipleQueries(masterSeqLoc,
//            const_cast<BLAST_Matrix*>(BLASTmatrix), target, 1, options);
//        SeqAlign *salp = psalp[0];

        // process the result
        double score = -1.0;
        if (salp) {
            // convert C SeqAlign to C++ for convenience
            CSeq_align sa;
            bool okay = ConvertAsnFromCToCPP(salp, (AsnWriteFunc) SeqAlignAsnWrite, &sa, &err);
            SeqAlignSetFree(salp);
            if (!okay) {
                ERRORMSG("Conversion of SeqAlign to C++ object failed");
                continue;
            }

            // get score
            if (sa.IsSetScore()) {
                CSeq_align::TScore::const_iterator sc, sce = sa.GetScore().end();
                for (sc=sa.GetScore().begin(); sc!=sce; ++sc) {
                    if ((*sc)->GetValue().IsReal() && (*sc)->IsSetId() &&
                        (*sc)->GetId().IsStr() && (*sc)->GetId().GetStr() == "e_value") {
                        score = (*sc)->GetValue().GetReal();
                        break;
                    }
                }
            }
            if (score < 0.0) ERRORMSG("Got back Seq-align with no E-value");

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
    for (row=0; row<multiple->NRows(); ++row) {
        if (multiple->GetRowDouble(row) >= 0.0 && multiple->GetRowDouble(row) <= threshold)
            ++nSelfHits;
    }
    INFOMSG("Self hits with E-value <= " << setprecision(3) << threshold << ": "
        << (100.0*nSelfHits/multiple->NRows()) << "% ("
        << nSelfHits << '/' << multiple->NRows() << ')' << setprecision(6));

    BLASTOptionDelete(options);
    masterSeqInt->id = NULL;    // don't free Seq-id, since it belongs to the Bioseq
    SeqIntFree(masterSeqInt);
    MemFree(masterSeqLoc);
    slaveSeqInt->id = NULL;
    SeqIntFree(slaveSeqInt);
    MemFree(slaveSeqLoc);
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.36  2005/03/08 17:22:31  thiessen
* apparently working C++ PSSM generation
*
* Revision 1.35  2004/07/27 17:38:12  thiessen
* don't call GetPSSM() w/ no aligned blocks
*
* Revision 1.34  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.33  2004/03/15 18:19:23  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.32  2004/02/19 17:04:49  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.31  2003/07/14 18:37:07  thiessen
* change GetUngappedAlignedBlocks() param types; other syntax changes
*
* Revision 1.30  2003/05/29 16:38:27  thiessen
* set db length for CDD 1.62
*
* Revision 1.29  2003/02/03 19:20:02  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.28  2003/01/31 17:18:58  thiessen
* many small additions and changes...
*
* Revision 1.27  2003/01/23 20:03:05  thiessen
* add BLAST Neighbor algorithm
*
* Revision 1.26  2003/01/22 14:47:30  thiessen
* cache PSSM in BlockMultipleAlignment
*
* Revision 1.25  2002/12/20 02:51:46  thiessen
* fix Prinf to self problems
*
* Revision 1.24  2002/10/25 19:00:02  thiessen
* retrieve VAST alignment from vastalign.cgi on structure import
*
* Revision 1.23  2002/09/19 14:21:03  thiessen
* add search space size calculation to match scores with rpsblast (finally!)
*
* Revision 1.22  2002/09/05 17:39:53  thiessen
* add bit score printout for BLAST/PSSM
*
* Revision 1.21  2002/09/05 13:04:33  thiessen
* restore output stream precision
*
* Revision 1.20  2002/09/04 15:51:52  thiessen
* turn off options->tweak_parameters
*
* Revision 1.19  2002/08/30 16:52:10  thiessen
* progress on trying to match scores with RPS-BLAST
*
* Revision 1.18  2002/08/15 22:13:13  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.17  2002/08/01 12:51:36  thiessen
* add E-value display to block aligner
*
* Revision 1.16  2002/07/26 15:28:45  thiessen
* add Alejandro's block alignment algorithm
*
* Revision 1.15  2002/07/23 15:46:49  thiessen
* print out more BLAST info; tweak save file name
*
* Revision 1.14  2002/07/12 13:24:10  thiessen
* fixes for PSSM creation to agree with cddumper/RPSBLAST
*
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
*/
