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
*      implementation of non-GUI part of update viewer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.49  2002/10/13 22:58:08  thiessen
* add redo ability to editor
*
* Revision 1.48  2002/09/30 17:13:02  thiessen
* change structure import to do sequences as well; change cache to hold mimes; change block aligner vocabulary; fix block aligner dialog bugs
*
* Revision 1.47  2002/09/26 18:31:24  thiessen
* allow simultaneous import of multiple chains from single PDB
*
* Revision 1.46  2002/09/16 21:24:58  thiessen
* add block freezing to block aligner
*
* Revision 1.45  2002/09/09 13:38:23  thiessen
* separate save and save-as
*
* Revision 1.44  2002/08/15 22:13:18  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.43  2002/08/13 20:46:37  thiessen
* add global block aligner
*
* Revision 1.42  2002/07/27 12:29:52  thiessen
* fix block aligner crash
*
* Revision 1.41  2002/07/26 15:28:48  thiessen
* add Alejandro's block alignment algorithm
*
* Revision 1.40  2002/07/26 13:07:01  thiessen
* fix const object problem
*
* Revision 1.39  2002/07/01 23:17:04  thiessen
* skip warning if master choice canceled
*
* Revision 1.38  2002/06/06 01:30:02  thiessen
* fixes for linux/gcc
*
* Revision 1.37  2002/06/05 14:28:41  thiessen
* reorganize handling of window titles
*
* Revision 1.36  2002/06/04 12:48:56  thiessen
* tweaks for release ; fill out help menu
*
* Revision 1.35  2002/05/22 17:17:09  thiessen
* progress on BLAST interface ; change custom spin ctrl implementation
*
* Revision 1.34  2002/05/17 19:10:27  thiessen
* preliminary range restriction for BLAST/PSSM
*
* Revision 1.33  2002/04/26 19:01:00  thiessen
* fix display delete bug
*
* Revision 1.32  2002/03/28 14:06:02  thiessen
* preliminary BLAST/PSSM ; new CD startup style
*
* Revision 1.31  2002/03/07 19:16:04  thiessen
* don't auto-show sequence windows
*
* Revision 1.30  2002/03/04 15:52:15  thiessen
* hide sequence windows instead of destroying ; add perspective/orthographic projection choice
*
* Revision 1.29  2002/02/27 16:29:42  thiessen
* add model type flag to general mime type
*
* Revision 1.28  2002/02/22 14:24:01  thiessen
* sort sequences in reject dialog ; general identifier comparison
*
* Revision 1.27  2002/02/13 14:53:30  thiessen
* add update sort
*
* Revision 1.26  2002/02/12 17:19:22  thiessen
* first working structure import
*
* Revision 1.25  2002/02/01 00:41:21  thiessen
* tweaks
*
* Revision 1.24  2002/01/24 20:07:57  thiessen
* read multiple FAST sequences
*
* Revision 1.23  2002/01/02 02:08:29  thiessen
* go back to viewer.cgi to test http/301 forwarding
*
* Revision 1.22  2001/12/15 03:15:59  thiessen
* adjustments for slightly changed object loader Set...() API
*
* Revision 1.21  2001/12/12 14:58:10  thiessen
* change URL to viewer.fcgi
*
* Revision 1.20  2001/12/06 23:13:47  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.19  2001/11/30 14:02:05  thiessen
* progress on sequence imports to single structures
*
* Revision 1.18  2001/11/27 16:26:10  thiessen
* major update to data management system
*
* Revision 1.17  2001/10/01 16:04:25  thiessen
* make CDD annotation window non-modal; add SetWindowTitle to viewers
*
* Revision 1.16  2001/09/27 15:38:00  thiessen
* decouple sequence import and BLAST
*
* Revision 1.15  2001/09/20 19:31:30  thiessen
* fixes for SGI and wxWin 2.3.2
*
* Revision 1.14  2001/09/19 22:55:39  thiessen
* add preliminary net import and BLAST
*
* Revision 1.13  2001/09/18 03:10:46  thiessen
* add preliminary sequence import pipeline
*
* Revision 1.12  2001/06/02 17:22:46  thiessen
* fixes for GCC
*
* Revision 1.11  2001/05/17 18:34:26  thiessen
* spelling fixes; change dialogs to inherit from wxDialog
*
* Revision 1.10  2001/05/02 13:46:28  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.9  2001/04/20 18:02:41  thiessen
* don't open update viewer right away
*
* Revision 1.8  2001/04/19 12:58:32  thiessen
* allow merge and delete of individual updates
*
* Revision 1.7  2001/04/05 22:55:36  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.6  2001/04/04 00:27:15  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.5  2001/03/30 03:07:34  thiessen
* add threader score calculation & sorting
*
* Revision 1.4  2001/03/22 00:33:17  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.3  2001/03/17 14:06:49  thiessen
* more workarounds for namespace/#define conflicts
*
* Revision 1.2  2001/03/13 01:25:06  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.1  2001/03/09 15:49:06  thiessen
* major changes to add initial update viewer
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#include <objects/cdd/Cdd.hpp>
#include <objects/cdd/Update_align.hpp>
#include <objects/cdd/Update_comment.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/ncbimime/Biostruc_seq.hpp>
#include <objects/mmdb2/Model_type.hpp>
#include <objects/mmdb2/Biostruc_model.hpp>
#include <objects/mmdb1/Biostruc_graph.hpp>
#include <objects/mmdb1/Biostruc_descr.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/mmdb1/Biostruc_id.hpp>
#include <objects/mmdb1/Mmdb_id.hpp>

#include <memory>
#include <algorithm>

// C stuff
#include <stdio.h>
#include <tofasta.h>
#include <objseq.h>
#include <objsset.h>

#include "cn3d/update_viewer.hpp"
#include "cn3d/update_viewer_window.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/sequence_display.hpp"
#include "cn3d/cn3d_colors.hpp"
#include "cn3d/alignment_manager.hpp"
#include "cn3d/cn3d_threader.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/asn_converter.hpp"
#include "cn3d/cn3d_blast.hpp"
#include "cn3d/asn_reader.hpp"
#include "cn3d/molecule_identifier.hpp"
#include "cn3d/cn3d_cache.hpp"
#include "cn3d/cn3d_ba_interface.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

UpdateViewer::UpdateViewer(AlignmentManager *alnMgr) :
    // not sure why this cast is necessary, but MSVC requires it...
    ViewerBase(reinterpret_cast<ViewerWindowBase**>(&updateWindow), alnMgr),
    updateWindow(NULL)
{
    // when first created, start with blank display
    AlignmentList emptyAlignmentList;
    SequenceDisplay *blankDisplay = new SequenceDisplay(true, viewerWindow);
    InitData(&emptyAlignmentList, blankDisplay);
    EnableStacks();
}

UpdateViewer::~UpdateViewer(void)
{
}

void UpdateViewer::SetInitialState(void)
{
    KeepCurrent();
    EnableStacks();
}

void UpdateViewer::SaveDialog(bool prompt)
{
    if (updateWindow) updateWindow->SaveDialog(prompt, false);
}

void UpdateViewer::CreateUpdateWindow(void)
{
    if (updateWindow) {
        updateWindow->Show(true);
        GlobalMessenger()->PostRedrawSequenceViewer(this);
    } else {
        SequenceDisplay *display = GetCurrentDisplay();
        if (display) {
            updateWindow = new UpdateViewerWindow(this);
            updateWindow->NewDisplay(display, false);
            updateWindow->ScrollToColumn(display->GetStartingColumn());
            updateWindow->Show(true);
            // ScrollTo causes immediate redraw, so don't need a second one
            GlobalMessenger()->UnPostRedrawSequenceViewer(this);
        }
    }
}

void UpdateViewer::AddAlignments(const AlignmentList& newAlignments)
{
    AlignmentList& alignments = GetCurrentAlignments();
    SequenceDisplay *display = GetCurrentDisplay();

    // populate successive lines of the display with each alignment, with blank lines inbetween
    AlignmentList::const_iterator a, ae = newAlignments.end();
    for (a=newAlignments.begin(); a!=ae; a++) {
        if ((*a)->NRows() != 2) {
            ERR_POST(Error << "UpdateViewer::AddAlignments() - got alignment with "
                << (*a)->NRows() << " rows");
            continue;
        }

        // add alignment to stack list
        alignments.push_back(*a);

        // add alignment to the display, including block row since editor is always on
        if (display->NRows() != 0) display->AddRowFromString("");
        display->AddBlockBoundaryRow(*a);
        for (int row=0; row<2; row++)
            display->AddRowFromAlignment(row, *a);

        // always show geometry violations in updates
        if ((*a)->GetMaster()->molecule && !(*a)->GetMaster()->molecule->parentSet->isAlphaOnly) {
            Threader::GeometryViolationsForRow violations;
            alignmentManager->threader->GetGeometryViolations(*a, &violations);
            (*a)->ShowGeometryViolations(violations);
        }
    }

    if (alignments.size() > 0)
        display->SetStartingColumn(alignments.front()->GetFirstAlignedBlockPosition() - 5);

    Save();    // make this an undoable operation
	if (updateWindow) updateWindow->UpdateDisplay(display);
}

void UpdateViewer::ReplaceAlignments(const AlignmentList& alignmentList)
{
    // empty out the current alignment list and display (but not the undo stacks!)
    AlignmentList::const_iterator a, ae = GetCurrentAlignments().end();
    for (a=GetCurrentAlignments().begin(); a!=ae; a++) delete *a;
    GetCurrentAlignments().clear();

    GetCurrentDisplay()->Empty();

    AddAlignments(alignmentList);
}

void UpdateViewer::DeleteAlignment(BlockMultipleAlignment *toDelete)
{
    AlignmentList keepAlignments;
    AlignmentList::const_iterator a, ae = GetCurrentAlignments().end();
    for (a=GetCurrentAlignments().begin(); a!=ae; a++)
        if (*a != toDelete)
            keepAlignments.push_back((*a)->Clone());

    ReplaceAlignments(keepAlignments);
}

void UpdateViewer::SaveAlignments(void)
{
    SetInitialState();

    // construct a new list of ASN Update-aligns
    CCdd::TPending updates;
    std::map < CUpdate_align * , bool > usedUpdateAligns;

    AlignmentList::const_iterator a, ae = GetCurrentAlignments().end();
    for (a=GetCurrentAlignments().begin(); a!=ae; a++) {

        // create a Seq-align (with Dense-diags) out of this update
        if ((*a)->NRows() != 2) {
            ERR_POST(Error << "CreateSeqAlignFromUpdate() - can only save pairwise updates");
            continue;
        }
        auto_ptr<BlockMultipleAlignment::UngappedAlignedBlockList> blocks((*a)->GetUngappedAlignedBlocks());
        CSeq_align *newSeqAlign = CreatePairwiseSeqAlignFromMultipleRow(*a, blocks.get(), 1);
        if (!newSeqAlign) continue;

        // the Update-align and Seq-annot's list of Seq-aligns where this new Seq-align will go
        CUpdate_align *updateAlign = (*a)->updateOrigin.GetPointer();
        CSeq_annot::C_Data::TAlign *seqAlignList = NULL;

        // if this alignment came from an existing Update-align, then replace just the Seq-align
        // so that the rest of the Update-align's comments/annotations are preserved
        if (updateAlign) {
            if (!updateAlign->IsSetSeqannot() || !updateAlign->GetSeqannot().GetData().IsAlign()) {
                ERR_POST(Error << "UpdateViewer::SaveAlignments() - confused by Update-align format");
                continue;
            }
        }

        // if this is a new update, create a new Update-align and tag it
        else {
            updateAlign = new CUpdate_align();

            // set type field depending on whether demoted sequence has structure
            updateAlign->SetType((*a)->GetSequenceOfRow(1)->molecule ?
                CUpdate_align::eType_demoted_3d : CUpdate_align::eType_demoted);

            // add a text comment
            CUpdate_comment *comment = new CUpdate_comment();
            comment->SetComment("Created by demotion or import in Cn3D 4.0");
            updateAlign->SetDescription().resize(updateAlign->GetDescription().size() + 1);
            updateAlign->SetDescription().back().Reset(comment);

            // create a new Seq-annot
            CRef < CSeq_annot > seqAnnotRef(new CSeq_annot());
            seqAnnotRef->SetData().SetAlign();
            updateAlign->SetSeqannot(*seqAnnotRef);
        }

        // get Seq-align list
        if (!updateAlign || !(seqAlignList = &(updateAlign->SetSeqannot().SetData().SetAlign()))) {
            ERR_POST(Error << "UpdateViewer::SaveAlignments() - can't find Update-align and/or Seq-align list");
            continue;
        }

        // if this is the first re-use of this Update-align, then empty out all existing
        // Seq-aligns and push it onto the new update list
        if (usedUpdateAligns.find(updateAlign) == usedUpdateAligns.end()) {
            seqAlignList->clear();
            updates.resize(updates.size() + 1);
            updates.back().Reset(updateAlign);
            usedUpdateAligns[updateAlign] = true;
        }

        // finally, add the new Seq-align to the list
        seqAlignList->resize(seqAlignList->size() + 1);
        seqAlignList->back().Reset(newSeqAlign);
    }

    // save these changes to the ASN data
    alignmentManager->ReplaceUpdatesInASN(updates);
}

const Sequence * UpdateViewer::GetMasterSequence(void) const
{
    const Sequence *master = NULL;

    // if there's a multiple alignment in the sequence viewer, use same master as that
    if (alignmentManager->GetCurrentMultipleAlignment()) {
        master = alignmentManager->GetCurrentMultipleAlignment()->GetMaster();
    }
    // if there's already an update present, use same master as that
    else if (GetCurrentAlignments().size() > 0) {
        master = GetCurrentAlignments().front()->GetMaster();
    }
    // otherwise, this must be a single structure with no updates, so we need to pick one of its
    // chains as the new master
    else {
        std::vector < const Sequence * > chains;
        if (alignmentManager->GetStructureProteins(&chains)) {
            if (chains.size() == 1) {
                master = chains[0];
            } else {
                wxString *titles = new wxString[chains.size()];
                int choice;
                for (choice=0; choice<chains.size(); choice++)
                    titles[choice] = chains[choice]->identifier->ToString().c_str();
                choice = wxGetSingleChoiceIndex("Align to which protein chain?",
                    "Select Chain", chains.size(), titles);
                if (choice >= 0)
                    master = chains[choice];
                else    // cancelled
                    return NULL;
            }
        }
    }
    if (!master)
        ERR_POST(Error << "UpdateViewer::GetMasterSequence() - no master sequence defined");
    return master;
}

void UpdateViewer::FetchSequenceViaHTTP(SequenceList *newSequences, StructureSet *sSet) const
{
    wxString id =
        wxGetTextFromUser("Enter a protein GI or Accession:", "Input Identifier", "", *viewerWindow);

    if (id.size() > 0) {
        CSeq_entry seqEntry;
        std::string err;
        static const std::string host("www.ncbi.nlm.nih.gov"), path("/entrez/viewer.cgi");

        std::string args = std::string("view=0&maxplex=1&save=idf&val=") + id.c_str();
        TESTMSG("Trying to load sequence from " << host << path << '?' << args);
        if (GetAsnDataViaHTTP(host, path, args, &seqEntry, &err)) {
            CRef < CBioseq > bioseq;
            if (seqEntry.IsSeq())
                bioseq.Reset(&(seqEntry.SetSeq()));
            else if (seqEntry.IsSet() && seqEntry.GetSet().GetSeq_set().front()->IsSeq())
                bioseq.Reset(&(seqEntry.SetSet().SetSeq_set().front()->SetSeq()));
            else
                ERR_POST(Error << "UpdateViewer::FetchSequenceViaHTTP() - confused by SeqEntry format");
            if (!bioseq.Empty()) {
                // create Sequence
                const Sequence *sequence = sSet->CreateNewSequence(*bioseq);
                if (sequence->isProtein)
                    newSequences->push_back(sequence);
                else
                    ERR_POST(Error << "The sequence must be a protein");
            }
        } else {
            ERR_POST(Error << "UpdateViewer::FetchSequenceViaHTTP() - HTTP Bioseq retrieval failed");
        }
    }
}

void UpdateViewer::ReadSequencesFromFile(SequenceList *newSequences, StructureSet *sSet) const
{
    newSequences->clear();

    wxString fastaFile = wxFileSelector("Choose a FASTA file from which to import",
        "", "", "", "*.*", wxOPEN | wxFILE_MUST_EXIST, *viewerWindow);
    if (fastaFile.size() > 0) {
        FILE *fp = FileOpen(fastaFile.c_str(), "r");
        if (fp) {
            SeqEntry *sep = NULL;
			std::string err;
            while ((sep=FastaToSeqEntry(fp, FALSE)) != NULL) {

                // convert C to C++ SeqEntry
                CSeq_entry se;
                if (!ConvertAsnFromCToCPP(sep, (AsnWriteFunc) SeqEntryAsnWrite, &se, &err)) {
                    ERR_POST(Error << "UpdateViewer::ImportSequence() - error converting to C++ object: "
                        << err);
                } else {
                    // create Sequence - just one from each Seq-entry for now
                    if (se.IsSeq()) {
                        newSequences->push_back(sSet->CreateNewSequence(se.SetSeq()));
                    } else {
                        ERR_POST(Error << "FastaToSeqEntry() returned Bioseq-set in Seq-entry");
                    }
                }
                SeqEntryFree(sep);
            }
            FileClose(fp);
        } else
            ERR_POST(Error << "Unable to open " << fastaFile.c_str());
    }
}

void UpdateViewer::MakeNewAlignments(const SequenceList& newSequences,
    const Sequence *master, AlignmentList *newAlignments) const
{
    newAlignments->clear();
    SequenceList::const_iterator s, se = newSequences.end();
    for (s=newSequences.begin(); s!=se; s++) {
        BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
        (*seqs)[0] = master;
        (*seqs)[1] = *s;
        BlockMultipleAlignment *newAlignment =
            new BlockMultipleAlignment(seqs, master->parentSet->alignmentManager);
        if (newAlignment->AddUnalignedBlocks() && newAlignment->UpdateBlockMapAndColors(false)) {
            newAlignments->push_back(newAlignment);
        } else {
            ERR_POST(Error << "UpdateViewer::MakeNewAlignments() - error finalizing alignment");
            delete newAlignment;
        }
    }
}

void UpdateViewer::FetchSequences(StructureSet *sSet, SequenceList *newSequences) const
{
    // choose import type
    static const wxString choiceStrings[] = { "Network via GI/Accession", "From a FASTA File" };
    enum choiceValues { FROM_GI=0, FROM_FASTA, N_CHOICES };
    int importFrom = wxGetSingleChoiceIndex("From what source would you like to import sequences?",
        "Select Import Source", N_CHOICES, choiceStrings, *viewerWindow);
    if (importFrom < 0) return;     // cancelled

    // network import
    if (importFrom == FROM_GI)
        FetchSequenceViaHTTP(newSequences, sSet);

    // FASTA import
    else if (importFrom == FROM_FASTA)
        ReadSequencesFromFile(newSequences, sSet);
}

void UpdateViewer::ImportSequences(void)
{
    // determine the master sequence for new alignments
    const Sequence *master = GetMasterSequence();
    if (!master) return;

    // import the new sequence(s)
    SequenceList newSequences;
    FetchSequences(master->parentSet, &newSequences);
    if (newSequences.size() == 0) {
        ERR_POST(Warning << "UpdateViewer::ImportSequence() - no sequences were imported");
        return;
    }

    // create null-alignments for each sequence
    AlignmentList newAlignments;
    MakeNewAlignments(newSequences, master, &newAlignments);

    // add new alignments to update list
    if (newAlignments.size() > 0)
        AddAlignments(newAlignments);
    else
        ERR_POST(Error << "UpdateViewer::ImportSequence() - no new alignments were created");
}

void UpdateViewer::ImportStructure(void)
{
    // determine the master sequence for new alignments
    const Sequence *master = GetMasterSequence();
    if (!master) return;
    if (!master->molecule) {
        ERR_POST(Error << "Can't import another structure unless master sequence has structure");
        return;
    }
    if (master->parentSet->objects.size() == 1 && master->parentSet->frameMap.size() != 1) {
        ERR_POST(Error << "Can't import another structure when current structure has multiple models");
        return;
    }

    // choose import type for structure
    static const wxString choiceStrings[] = { "Via Network", "From a File" };
    enum choiceValues { FROM_NETWORK=0, FROM_FILE, N_CHOICES };
    int importFrom = wxGetSingleChoiceIndex(
        "From what source would you like to import the structure?", "Select Import Source",
        N_CHOICES, choiceStrings, *viewerWindow);
    if (importFrom < 0) return;     // cancelled

    int mmdbID;
    CRef < CBiostruc > biostruc;
    BioseqRefList bioseqs;

    if (importFrom == FROM_NETWORK) {
        wxString id = wxGetTextFromUser("Enter an MMDB ID:", "Input Identifier", "", *viewerWindow);
        unsigned long idL;
        if (id.size() == 0 || !id.ToULong(&idL)) return;
        mmdbID = (int) idL;
        biostruc.Reset(new CBiostruc());
        if (!LoadStructureViaCache(mmdbID,
                (master->parentSet->isAlphaOnly ? eModel_type_ncbi_backbone : eModel_type_ncbi_all_atom),
                biostruc, &bioseqs)) {
            ERR_POST(Error << "Failed to load MMDB #" << mmdbID);
            return;
        }
    }

    else if (importFrom == FROM_FILE) {
        std::string filename = wxFileSelector("Choose a single-structure file:",
            GetUserDir().c_str(), "", "", "*.*", wxOPEN | wxFILE_MUST_EXIST, *viewerWindow).c_str();
        if (filename.size() == 0) return;
        bool readOK = false;
        std::string err;
        TESTMSG("trying to read file '" << filename << "' as binary mime");
        CRef < CNcbi_mime_asn1 > mime(new CNcbi_mime_asn1());
        SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
        readOK = ReadASNFromFile(filename.c_str(), mime.GetPointer(), true, &err);
        SetDiagPostLevel(eDiag_Info);
        if (!readOK) {
            ERR_POST(Warning << "error: " << err);
            TESTMSG("trying to read file '" << filename << "' as ascii mime");
            mime.Reset(new CNcbi_mime_asn1());
            SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
            readOK = ReadASNFromFile(filename.c_str(), mime.GetPointer(), false, &err);
            SetDiagPostLevel(eDiag_Info);
        }
        if (!readOK) {
            ERR_POST(Warning << "error: " << err);
            ERR_POST(Error << "Couldn't read structure from " << filename);
            return;
        }

        ExtractBiostrucAndBioseqs(*mime, biostruc, &bioseqs);

        if (biostruc->GetId().size() == 0 || !biostruc->GetId().front()->IsMmdb_id()) {
            ERR_POST(Error << "Can't get MMDB ID from loaded structure");
            return;
        }
        mmdbID = biostruc->GetId().front()->GetMmdb_id().Get();
    }

    // make sure Biostruc is of correct type
    if (biostruc->GetModel().size() != 1 ||
        (master->parentSet->isAlphaOnly &&
            biostruc->GetModel().front()->GetType() != eModel_type_ncbi_backbone) ||
        (!master->parentSet->isAlphaOnly &&
            biostruc->GetModel().front()->GetType() != eModel_type_ncbi_all_atom)) {
        ERR_POST(Error << "Biostruc does not match current data - should be "
            << (master->parentSet->isAlphaOnly ? "alpha-only" : "one-coordinate-per-atom") << " model");
        return;
    }

    // make list of protein chains in this structure
    std::vector < std::pair < int , char > > chains;    // holds gi and chain name
    CBiostruc_graph::TDescr::const_iterator d, de;
    CBiostruc_graph::TMolecule_graphs::const_iterator
        m, me = biostruc->GetChemical_graph().GetMolecule_graphs().end();
    for (m=biostruc->GetChemical_graph().GetMolecule_graphs().begin(); m!=me; m++) {
        bool isProtein = false;
        int gi = -1;
        char name = 0;

        // check descr for chain name/type
        de = (*m)->GetDescr().end();
        for (d=(*m)->GetDescr().begin(); d!=de; d++) {
            if ((*d)->IsName())
                name = (*d)->GetName()[0];
            else if ((*d)->IsMolecule_type() &&
                     (*d)->GetMolecule_type() == CBiomol_descr::eMolecule_type_protein)
                isProtein = true;
            if (isProtein && name) break;
        }

        // get gi
        if ((*m)->IsSetSeq_id() && (*m)->GetSeq_id().IsGi())
            gi = (*m)->GetSeq_id().GetGi();

        // add protein to list
        if (isProtein && name && gi > 0)
            chains.push_back(make_pair(gi, name));
    }
    if (chains.size() == 0) {
        ERR_POST(Error << "No protein chains found in this structure!");
        return;
    }

    // get protein name (PDB identifier)
    std::string pdbID;
    CBiostruc::TDescr::const_iterator n, ne = biostruc->GetDescr().end();
    for (n=biostruc->GetDescr().begin(); n!=ne; n++) {
        if ((*n)->IsName()) {
            pdbID = (*n)->GetName();    // assume first 'name' is pdb id
            break;
        }
    }

    // which chains to align?
    std::vector < int > gis;
    if (chains.size() == 1) {
        gis.push_back(chains[0].first);
    } else {
        wxString *choices = new wxString[chains.size()];
        int choice;
        for (choice=0; choice<chains.size(); choice++)
            choices[choice].Printf("%s_%c gi %i",
                pdbID.c_str(), chains[choice].second, chains[choice].first);
        wxArrayInt selections;
        int nsel = wxGetMultipleChoices(selections, "Which chain do you want to align?",
            "Select Chain", chains.size(), choices, *viewerWindow);
        if (nsel == 0) return;
        for (choice=0; choice<nsel; choice++)
            gis.push_back(chains[selections[choice]].first);
    }

    SequenceList newSequences;
    SequenceSet::SequenceList::const_iterator s, se = master->parentSet->sequenceSet->sequences.end();
    for (int j=0; j<gis.size(); j++) {

        // first check to see if this sequence is already present
        for (s=master->parentSet->sequenceSet->sequences.begin(); s!=se; s++) {
            if ((*s)->identifier->gi == gis[j]) {
                newSequences.push_back(*s);
                TESTMSG("using existing sequence for gi " << gis[j]);
                break;
            }
        }

        if (s == se) {
            // if not, find the sequence in the list from the structure file
            BioseqRefList::const_iterator b, be = bioseqs.end();
            for (b=bioseqs.begin(); b!=be; b++) {
                CBioseq::TId::const_iterator i, ie = (*b)->GetId().end();
                for (i=(*b)->GetId().begin(); i!=ie; i++) {
                    if ((*i)->IsGi() && (*i)->GetGi() == gis[j])
                        break;
                }
                if (i != ie) {
                    TESTMSG("found Bioseq for gi " << gis[j]);
                    newSequences.push_back(master->parentSet->CreateNewSequence(**b));
                    break;
                }
            }
            if (b == be) {
                ERR_POST(Error << "ImportStructure() - can't find gi " << gis[j] << " in bioseq list!");
//                return;
            }
        }
    }

    // add MMDB id tag to Bioseq if not present already
    SequenceList::const_iterator w, we = newSequences.end();
    for (w=newSequences.begin(); w!=we; w++) {
        CBioseq::TAnnot::const_iterator a, ae = (*w)->bioseqASN->GetAnnot().end();
        CSeq_annot::C_Data::TIds::const_iterator i, ie;
        bool found = false;
        for (a=(*w)->bioseqASN->GetAnnot().begin(); a!=ae; a++) {
            if ((*a)->GetData().IsIds()) {
                for (i=(*a)->GetData().GetIds().begin(), ie=(*a)->GetData().GetIds().end(); i!=ie; i++) {
                    if ((*i)->IsGeneral() && (*i)->GetGeneral().GetDb() == "mmdb" &&
                            (*i)->GetGeneral().GetTag().IsId() &&
                            (*i)->GetGeneral().GetTag().GetId() == mmdbID) {
                        found = true;
                        TESTMSG("mmdb link already present in sequence " << (*w)->identifier->ToString());
                        break;
                    }
                }
            }
            if (found) break;
        }
        if (!found) {
            CRef < CSeq_id > seqid(new CSeq_id());
            seqid->SetGeneral().SetDb("mmdb");
            seqid->SetGeneral().SetTag().SetId(mmdbID);
            CRef < CSeq_annot > annot(new CSeq_annot());
            annot->SetData().SetIds().push_back(seqid);
            (const_cast<Sequence*>(*w))->bioseqASN->SetAnnot().push_back(annot);
        }
    }

    // create null-alignment
    AlignmentList newAlignments;
    MakeNewAlignments(newSequences, master, &newAlignments);

    // add new alignment to update list
    if (newAlignments.size() == newSequences.size())
        AddAlignments(newAlignments);
    else {
        ERR_POST(Error << "UpdateViewer::ImportStructure() - no new alignments were created");
        return;
    }

    // add Biostruc to structure list later on, after merge
    pendingStructures.push_back(biostruc);

    // inform user of success
    wxMessageBox("The structure has been successfully imported! However, it will not appear until you\n"
        "save this data to a file and then re-load it in a new session. And depending on the type\n"
        "of data, it still might not appear unless the corresponding new pairwise alignment has\n"
        "been merged into the multiple alignment.",
        "Structure Added", wxOK | wxICON_INFORMATION, *viewerWindow);
}

void UpdateViewer::SavePendingStructures(void)
{
    TESTMSG("saving pending imported structures");
    StructureSet *sSet =
        (alignmentManager->GetCurrentMultipleAlignment() &&
         alignmentManager->GetCurrentMultipleAlignment()->GetMaster()) ?
         alignmentManager->GetCurrentMultipleAlignment()->GetMaster()->parentSet : NULL;
    while (pendingStructures.size() > 0) {
        if (!sSet || !sSet->AddBiostrucToASN(pendingStructures.front().GetPointer())) {
            ERR_POST(Error << "UpdateViewer::SavePendingStructures() - error saving Biostruc");
            return;
        }
        pendingStructures.pop_front();
    }
}

void UpdateViewer::BlastUpdate(BlockMultipleAlignment *alignment, bool usePSSMFromMultiple)
{
    const BlockMultipleAlignment *multipleForPSSM = alignmentManager->GetCurrentMultipleAlignment();
    if (usePSSMFromMultiple && !multipleForPSSM) {
        ERR_POST(Error << "Can't do BLAST/PSSM when no multiple alignment is present");
        return;
    }

    // find alignment, and replace it with BLAST result
    AlignmentList::iterator a, ae = GetCurrentAlignments().end();
    for (a=GetCurrentAlignments().begin(); a!=ae; a++) {
        if (*a != alignment) continue;

        // run BLAST between master and first slave (should be only one slave...)
        BLASTer::AlignmentList toRealign;
        toRealign.push_back(alignment);
        BLASTer::AlignmentList newAlignments;
        alignmentManager->blaster->CreateNewPairwiseAlignmentsByBlast(
            multipleForPSSM, toRealign, &newAlignments, usePSSMFromMultiple);
        if (newAlignments.size() != 1) {
            ERR_POST(Error << "UpdateViewer::BlastUpdate() - CreateNewPairwiseAlignmentsByBlast() failed");
            return;
        }
        if (newAlignments.front()->NAlignedBlocks() == 0) {
            ERR_POST(Warning << "alignment unchanged");
            delete newAlignments.front();
            return;
        }

        // replace alignment with BLAST result
        TESTMSG("BLAST succeeded - replacing alignment");
        delete alignment;
        *a = newAlignments.front();
        break;
    }

    // recreate alignment display with new alignment
    AlignmentList copy = GetCurrentAlignments();
    GetCurrentAlignments().clear();
    GetCurrentDisplay()->Empty();
    AddAlignments(copy);
    (*viewerWindow)->ScrollToColumn(GetCurrentDisplay()->GetStartingColumn());
}

// comparison function: if CompareRows(a, b) == true, then row a moves up
typedef bool (*CompareUpdates)(BlockMultipleAlignment *a, BlockMultipleAlignment *b);

static bool CompareUpdatesByIdentifier(BlockMultipleAlignment *a, BlockMultipleAlignment *b)
{
    return MoleculeIdentifier::CompareIdentifiers(
        a->GetSequenceOfRow(1)->identifier, // sort by first slave row
        b->GetSequenceOfRow(1)->identifier);
}

static CompareUpdates updateComparisonFunction = NULL;

void UpdateViewer::SortByIdentifier(void)
{
    TESTMSG("sorting updates by identifier");
    updateComparisonFunction = CompareUpdatesByIdentifier;
    SortUpdates();
}

void UpdateViewer::SortUpdates(void)
{
    if (!updateComparisonFunction) {
        ERR_POST(Error << "UpdateViewer::SortUpdates() - must first set comparison function");
        return;
    }

    // make vector of alignments
    AlignmentList& currentAlignments = GetCurrentAlignments();
    if (currentAlignments.size() < 2) return;
    std::vector < BlockMultipleAlignment * > sortedVector(currentAlignments.size());
    AlignmentList::const_iterator a, ae = currentAlignments.end();
    int i = 0;
    for (a=currentAlignments.begin(); a!=ae; a++) sortedVector[i++] = *a;

    // sort them
    stable_sort(sortedVector.begin(), sortedVector.end(), updateComparisonFunction);
    updateComparisonFunction = NULL;

    // replace window contents with sorted list
    currentAlignments.clear();
    GetCurrentDisplay()->Empty();

    AlignmentList sortedList;
    for (i=0; i<sortedVector.size(); i++) sortedList.push_back(sortedVector[i]);
    AddAlignments(sortedList);
}

END_SCOPE(Cn3D)

