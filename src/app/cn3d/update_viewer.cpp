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
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
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
#include <objects/mmdb1/Biostruc_annot_set.hpp>
#include <objects/mmdb3/Biostruc_feature_set.hpp>
#include <objects/mmdb3/Chem_graph_alignment.hpp>
#include <objects/mmdb3/Chem_graph_pntrs.hpp>
#include <objects/mmdb3/Residue_pntrs.hpp>
#include <objects/mmdb3/Residue_interval_pntr.hpp>
#include <objects/mmdb1/Molecule_id.hpp>
#include <objects/mmdb1/Residue_id.hpp>
#include <objects/mmdb3/Biostruc_feature_set_id.hpp>
#include <objects/mmdb3/Biostruc_feature_id.hpp>

#include <memory>
#include <algorithm>

#include "update_viewer.hpp"
#include "asn_reader.hpp"
#include "update_viewer_window.hpp"
#include "messenger.hpp"
#include "sequence_display.hpp"
#include "cn3d_colors.hpp"
#include "alignment_manager.hpp"
#include "cn3d_threader.hpp"
#include "structure_set.hpp"
#include "molecule.hpp"
#include "cn3d_tools.hpp"
#include "asn_converter.hpp"
#include "cn3d_blast.hpp"
#include "molecule_identifier.hpp"
#include "cn3d_cache.hpp"
#include "cn3d_ba_interface.hpp"

#include <wx/tokenzr.h>

// C stuff
#include <stdio.h>
#include <tofasta.h>
#include <objseq.h>
#include <objsset.h>

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
    int nViolations = 0;
    for (a=newAlignments.begin(); a!=ae; ++a) {
        if ((*a)->NRows() != 2) {
            ERRORMSG("UpdateViewer::AddAlignments() - got alignment with "
                << (*a)->NRows() << " rows");
            continue;
        }

        // add alignment to stack list
        alignments.push_back(*a);
        (*a)->ShowGeometryViolations(updateWindow ? updateWindow->GeometryViolationsShown() : false);

        // add alignment to the display, including block row since editor is always on
        if (display->NRows() != 0) display->AddRowFromString("");
        display->AddBlockBoundaryRow(*a);
        for (int row=0; row<2; ++row)
            display->AddRowFromAlignment(row, *a);
    }

    if (alignments.size() > 0)
        display->SetStartingColumn(alignments.front()->GetFirstAlignedBlockPosition() - 5);

    Save();    // make this an undoable operation
	if (updateWindow) updateWindow->UpdateDisplay(display);
}

void UpdateViewer::ReplaceAlignments(const AlignmentList& alignmentList)
{
    // empty out the current alignment list and display (but not the undo stacks!)
    DELETE_ALL_AND_CLEAR(GetCurrentAlignments(), AlignmentList);
    GetCurrentDisplay()->Empty();

    AddAlignments(alignmentList);
}

void UpdateViewer::DeleteAlignment(BlockMultipleAlignment *toDelete)
{
    AlignmentList keepAlignments;
    AlignmentList::const_iterator a, ae = GetCurrentAlignments().end();
    for (a=GetCurrentAlignments().begin(); a!=ae; ++a)
        if (*a != toDelete)
            keepAlignments.push_back((*a)->Clone());

    ReplaceAlignments(keepAlignments);
}

void UpdateViewer::SaveAlignments(void)
{
    SetInitialState();

    // construct a new list of ASN Update-aligns
    CCdd::TPending updates;
    map < CUpdate_align * , bool > usedUpdateAligns;

    AlignmentList::const_iterator a, ae = GetCurrentAlignments().end();
    for (a=GetCurrentAlignments().begin(); a!=ae; ++a) {

        // create a Seq-align (with Dense-diags) out of this update
        if ((*a)->NRows() != 2) {
            ERRORMSG("CreateSeqAlignFromUpdate() - can only save pairwise updates");
            continue;
        }
        BlockMultipleAlignment::UngappedAlignedBlockList blocks;
        (*a)->GetUngappedAlignedBlocks(&blocks);
        CSeq_align *newSeqAlign = CreatePairwiseSeqAlignFromMultipleRow(*a, blocks, 1);
        if (!newSeqAlign) continue;

        // the Update-align and Seq-annot's list of Seq-aligns where this new Seq-align will go
        CUpdate_align *updateAlign = (*a)->updateOrigin.GetPointer();
        CSeq_annot::C_Data::TAlign *seqAlignList = NULL;

        // if this alignment came from an existing Update-align, then replace just the Seq-align
        // so that the rest of the Update-align's comments/annotations are preserved
        if (updateAlign) {
            if (!updateAlign->IsSetSeqannot() || !updateAlign->GetSeqannot().GetData().IsAlign()) {
                ERRORMSG("UpdateViewer::SaveAlignments() - confused by Update-align format");
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
            comment->SetComment("Created by demotion or import in Cn3D");
            updateAlign->SetDescription().resize(updateAlign->GetDescription().size() + 1);
            updateAlign->SetDescription().back().Reset(comment);

            // create a new Seq-annot
            CRef < CSeq_annot > seqAnnotRef(new CSeq_annot());
            seqAnnotRef->SetData().SetAlign();
            updateAlign->SetSeqannot(*seqAnnotRef);
        }

        // get Seq-align list
        if (!updateAlign || !(seqAlignList = &(updateAlign->SetSeqannot().SetData().SetAlign()))) {
            ERRORMSG("UpdateViewer::SaveAlignments() - can't find Update-align and/or Seq-align list");
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
        vector < const Sequence * > chains;
        if (alignmentManager->GetStructureProteins(&chains)) {
            if (chains.size() == 1) {
                master = chains[0];
            } else {
                wxString *titles = new wxString[chains.size()];
                int choice;
                for (choice=0; choice<chains.size(); ++choice)
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
        ERRORMSG("UpdateViewer::GetMasterSequence() - no master sequence defined");
    return master;
}

void UpdateViewer::FetchSequencesViaHTTP(SequenceList *newSequences, StructureSet *sSet) const
{
    wxString ids =
        wxGetTextFromUser("Enter a list of protein GIs or Accessions:",
            "Input Identifier", "", *viewerWindow);
    if (ids.size() == 0) return;

    wxStringTokenizer tkz(ids, " ,;\t\r\n", wxTOKEN_STRTOK);
    while (tkz.HasMoreTokens()) {
        wxString id = tkz.GetNextToken();
        CRef < CBioseq > bioseq = FetchSequenceViaHTTP(id.c_str());
        if (bioseq.Empty())
            return;
        const Sequence *sequence = sSet->CreateNewSequence(*bioseq);
        if (sequence) {
            if (sequence->isProtein)
                newSequences->push_back(sequence);
            else
                ERRORMSG("The sequence must be a protein");
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
            string err;
            while ((sep=FastaToSeqEntry(fp, FALSE)) != NULL) {

                // convert C to C++ SeqEntry
                CSeq_entry se;
                if (!ConvertAsnFromCToCPP(sep, (AsnWriteFunc) SeqEntryAsnWrite, &se, &err)) {
                    ERRORMSG("UpdateViewer::ReadSequencesFromFile() - error converting to C++ object: "
                        << err);
                } else {
                    // create Sequence - just one from each Seq-entry for now
                    if (se.IsSeq()) {
                        const Sequence *sequence = sSet->CreateNewSequence(se.SetSeq());
                        if (sequence)
                            newSequences->push_back(sequence);
                    } else {
                        ERRORMSG("FastaToSeqEntry() returned Bioseq-set in Seq-entry");
                    }
                }
                SeqEntryFree(sep);
            }
            FileClose(fp);
        } else
            ERRORMSG("Unable to open " << fastaFile.c_str());
    }
}

static BlockMultipleAlignment * MakeEmptyAlignment(const Sequence *master, const Sequence *slave)
{
    BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
    (*seqs)[0] = master;
    (*seqs)[1] = slave;
    BlockMultipleAlignment *newAlignment =
        new BlockMultipleAlignment(seqs, master->parentSet->alignmentManager);
    if (!newAlignment->AddUnalignedBlocks() || !newAlignment->UpdateBlockMapAndColors(false)) {
        ERRORMSG("MakeEmptyAlignment() - error finalizing alignment");
        delete newAlignment;
        return NULL;
    }
    return newAlignment;
}

void UpdateViewer::MakeEmptyAlignments(const SequenceList& newSequences,
    const Sequence *master, AlignmentList *newAlignments) const
{
    newAlignments->clear();
    SequenceList::const_iterator s, se = newSequences.end();
    for (s=newSequences.begin(); s!=se; ++s) {
        BlockMultipleAlignment *newAlignment = MakeEmptyAlignment(master, *s);
        if (newAlignment) newAlignments->push_back(newAlignment);
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
        FetchSequencesViaHTTP(newSequences, sSet);

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
        WARNINGMSG("UpdateViewer::ImportSequence() - no sequences were imported");
        return;
    }

    // create null-alignments for each sequence
    AlignmentList newAlignments;
    MakeEmptyAlignments(newSequences, master, &newAlignments);

    // add new alignments to update list
    if (newAlignments.size() > 0)
        AddAlignments(newAlignments);
    else
        ERRORMSG("UpdateViewer::ImportSequence() - no new alignments were created");
}

void UpdateViewer::GetVASTAlignments(const SequenceList& newSequences,
    const Sequence *master, AlignmentList *newAlignments,
    PendingStructureAlignments *structureAlignments,
    int masterFrom, int masterTo) const
{
    if (master->identifier->pdbID.size() == 0) {
        WARNINGMSG("UpdateViewer::GetVASTAlignments() - "
            "can't be called with non-MMDB master " << master->identifier->ToString());
        return;
    }

    SequenceList::const_iterator s, se = newSequences.end();
    for (s=newSequences.begin(); s!=se; ++s) {
        if ((*s)->identifier->pdbID.size() == 0) {
            WARNINGMSG("UpdateViewer::GetVASTAlignments() - "
                "can't be called with non-MMDB slave " << (*s)->identifier->ToString());
            BlockMultipleAlignment *newAlignment = MakeEmptyAlignment(master, *s);
            if (newAlignment) newAlignments->push_back(newAlignment);
            continue;
        }

        // set up URL
        string
            host = "www.ncbi.nlm.nih.gov",
            path = "/Structure/VA/vastalign.cgi", err;
        CNcbiOstrstream argstr;
        argstr << "master=" << master->identifier->ToString()
            << "&slave=" << (*s)->identifier->ToString();
        if (masterFrom >= 0 && masterTo >= 0 && masterFrom <= masterTo &&
            masterFrom < master->Length() && masterTo < master->Length())
            argstr << "&from=" << (masterFrom+1) << "&to=" << (masterTo+1); // URL #'s are 1-based
        argstr << '\0';
        auto_ptr<char> args(argstr.str());

        // connect to vastalign.cgi
        CBiostruc_annot_set structureAlignment;
        INFOMSG("trying to load VAST alignment data from " << host << path << '?' << args.get());
        if (!GetAsnDataViaHTTP(host, path, args.get(), &structureAlignment, &err)) {
            ERRORMSG("Error calling vastalign.cgi: " << err);
            BlockMultipleAlignment *newAlignment = MakeEmptyAlignment(master, *s);
            if (newAlignment) newAlignments->push_back(newAlignment);
            continue;
        }
        INFOMSG("successfully loaded data from vastalign.cgi");
#ifdef _DEBUG
        WriteASNToFile("vastalign.dat.txt", structureAlignment, false, &err);
#endif

        // create initially empty alignment
        BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
        (*seqs)[0] = master;
        (*seqs)[1] = *s;
        BlockMultipleAlignment *newAlignment =
            new BlockMultipleAlignment(seqs, master->parentSet->alignmentManager);

        // skip if no VAST alignment found
        if (structureAlignment.IsSetId() && structureAlignment.GetId().front()->IsMmdb_id() &&
            structureAlignment.GetId().front()->GetMmdb_id().Get() == 0)
        {
            WARNINGMSG("VAST found no alignment for these chains");
        }

        else try {

            if (!structureAlignment.IsSetId() || !structureAlignment.GetId().front()->IsMmdb_id() ||
                structureAlignment.GetFeatures().size() != 1 ||
                structureAlignment.GetFeatures().front()->GetFeatures().size() != 1 ||
                !structureAlignment.GetFeatures().front()->GetFeatures().front()->IsSetLocation() ||
                !structureAlignment.GetFeatures().front()->GetFeatures().front()->GetLocation().IsAlignment())
            {
                throw "VAST data does not contain exactly one alignment of recognized format - "
                    "possibly a problem with vastalign.cgi";
            }

            if (structureAlignment.GetId().front()->GetMmdb_id().Get() != master->identifier->mmdbID)
                throw "Master structure MMDB ID mismatch - check to see if this structure been updated";

            // load in alignment, check format
            const CChem_graph_alignment& alignment =
                structureAlignment.GetFeatures().front()->GetFeatures().front()->GetLocation().GetAlignment();
            if (alignment.GetDimension() != 2 || alignment.GetAlignment().size() != 2 ||
                alignment.GetBiostruc_ids().size() != 2 ||
                !alignment.GetBiostruc_ids().front()->IsMmdb_id() ||
                alignment.GetBiostruc_ids().front()->GetMmdb_id().Get() != master->identifier->mmdbID ||
                !alignment.GetBiostruc_ids().back()->IsMmdb_id() ||
                alignment.GetBiostruc_ids().back()->GetMmdb_id().Get() != (*s)->identifier->mmdbID ||
                !alignment.GetAlignment().front()->IsResidues() ||
                !alignment.GetAlignment().front()->GetResidues().IsInterval() ||
                !alignment.GetAlignment().back()->IsResidues() ||
                !alignment.GetAlignment().back()->GetResidues().IsInterval() ||
                alignment.GetAlignment().front()->GetResidues().GetInterval().size() !=
                    alignment.GetAlignment().back()->GetResidues().GetInterval().size())
            {
                throw "Unrecognized VAST data format";
            }

            // construct alignment from residue intervals
            CResidue_pntrs::TInterval::const_iterator i, j,
                ie = alignment.GetAlignment().front()->GetResidues().GetInterval().end();
            for (i=alignment.GetAlignment().front()->GetResidues().GetInterval().begin(),
                 j=alignment.GetAlignment().back()->GetResidues().GetInterval().begin(); i!=ie; ++i, ++j)
            {
                if ((*i)->GetMolecule_id().Get() != master->identifier->moleculeID ||
                    (*j)->GetMolecule_id().Get() != (*s)->identifier->moleculeID)
                {
                    throw "Mismatch in molecule ids in alignment interval block";
                }
                UngappedAlignedBlock *newBlock = new UngappedAlignedBlock(newAlignment);
                newBlock->SetRangeOfRow(0, (*i)->GetFrom().Get() - 1, (*i)->GetTo().Get() - 1);
                newBlock->SetRangeOfRow(1, (*j)->GetFrom().Get() - 1, (*j)->GetTo().Get() - 1);
                newBlock->width = (*i)->GetTo().Get() - (*i)->GetFrom().Get() + 1;
                newAlignment->AddAlignedBlockAtEnd(newBlock);
            }

            // add structure alignment to list
            if (alignment.GetTransform().size() == 1)
            {
                structureAlignments->resize(structureAlignments->size() + 1);
                structureAlignments->back().structureAlignment =
                    structureAlignment.SetFeatures().front()->SetFeatures().front();
                structureAlignments->back().masterDomainID =
                    structureAlignment.GetFeatures().front()->GetId().Get();
                structureAlignments->back().slaveDomainID =
                    structureAlignment.GetFeatures().front()->GetFeatures().front()->GetId().Get();
            } else
                throw "No structure alignment in VAST data blob";

        } catch (const char *err) {
            ERRORMSG("Failed to import VAST alignment: " << err);
        }

        // finalize alignment
        if (!newAlignment->AddUnalignedBlocks() || !newAlignment->UpdateBlockMapAndColors(false)) {
            ERRORMSG("MakeEmptyAlignment() - error finalizing alignment");
            delete newAlignment;
            continue;
        }
        newAlignments->push_back(newAlignment);
    }
}

void UpdateViewer::ImportStructure(void)
{
    // determine the master sequence for new alignments
    const Sequence *master = GetMasterSequence();
    if (!master) return;
    if (master->parentSet->objects.size() == 1 && master->parentSet->frameMap.size() != 1) {
        ERRORMSG("Can't import another structure when current structure has multiple models");
        return;
    }

    // choose import type for structure
    static const wxString choiceStrings[] = { "Via Network", "From a File" };
    enum choiceValues { FROM_NETWORK=0, FROM_FILE, N_CHOICES };
    int importFrom = wxGetSingleChoiceIndex(
        "From what source would you like to import the structure?", "Select Import Source",
        N_CHOICES, choiceStrings, *viewerWindow);
    if (importFrom < 0) return;     // cancelled

    CRef < CBiostruc > biostruc;
    BioseqRefList bioseqs;

    if (importFrom == FROM_NETWORK) {
        wxString id = wxGetTextFromUser("Enter a PDB or MMDB ID:", "Input Identifier", "", *viewerWindow);
        biostruc.Reset(new CBiostruc());
        if (!LoadStructureViaCache(id.c_str(),
                (master->parentSet->isAlphaOnly ? eModel_type_ncbi_backbone : eModel_type_ncbi_all_atom),
                biostruc, &bioseqs)) {
            ERRORMSG("Failed to load structure " << id.c_str());
            return;
        }
    }

    else if (importFrom == FROM_FILE) {
        string filename = wxFileSelector("Choose a single-structure file:",
            GetUserDir().c_str(), "", "", "*.*", wxOPEN | wxFILE_MUST_EXIST, *viewerWindow).c_str();
        if (filename.size() == 0) return;
        bool readOK = false;
        string err;
        TRACEMSG("trying to read file '" << filename << "' as binary mime");
        CRef < CNcbi_mime_asn1 > mime(new CNcbi_mime_asn1());
        SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
        readOK = ReadASNFromFile(filename.c_str(), mime.GetPointer(), true, &err);
        SetDiagPostLevel(eDiag_Info);
        if (!readOK) {
            TRACEMSG("error: " << err);
            TRACEMSG("trying to read file '" << filename << "' as ascii mime");
            mime.Reset(new CNcbi_mime_asn1());
            SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
            readOK = ReadASNFromFile(filename.c_str(), mime.GetPointer(), false, &err);
            SetDiagPostLevel(eDiag_Info);
        }
        if (!readOK) {
            TRACEMSG("error: " << err);
            ERRORMSG("Couldn't read structure from " << filename);
            return;
        }

        ExtractBiostrucAndBioseqs(*mime, biostruc, &bioseqs);
    }

    int mmdbID;
    if (biostruc->GetId().size() == 0 || !biostruc->GetId().front()->IsMmdb_id()) {
        ERRORMSG("Can't get MMDB ID from loaded structure");
        return;
    }
    mmdbID = biostruc->GetId().front()->GetMmdb_id().Get();

    // make sure Biostruc is of correct type
    if (master->molecule) {
        if (biostruc->GetModel().size() != 1 ||
            (master->parentSet->isAlphaOnly &&
                biostruc->GetModel().front()->GetType() != eModel_type_ncbi_backbone) ||
            (!master->parentSet->isAlphaOnly &&
                biostruc->GetModel().front()->GetType() != eModel_type_ncbi_all_atom)) {
            ERRORMSG("Biostruc does not match current data - should be "
                << (master->parentSet->isAlphaOnly ? "alpha-only" : "one-coordinate-per-atom") << " model");
            return;
        }
    }

    // make list of protein chains in this structure
    vector < pair < const CSeq_id * , char > > chains;  // holds Seq-id and chain name
    map < const CSeq_id * , int > moleculeIDs;          // maps Seq-id -> molecule ID within MMDB object
    CBiostruc_graph::TDescr::const_iterator d, de;
    CBiostruc_graph::TMolecule_graphs::const_iterator
        m, me = biostruc->GetChemical_graph().GetMolecule_graphs().end();
    for (m=biostruc->GetChemical_graph().GetMolecule_graphs().begin(); m!=me; ++m) {
        bool isProtein = false;
        const CSeq_id *sid = NULL;
        char name = 0;

        // check descr for chain name/type
        de = (*m)->GetDescr().end();
        for (d=(*m)->GetDescr().begin(); d!=de; ++d) {
            if ((*d)->IsName())
                name = (*d)->GetName()[0];
            else if ((*d)->IsMolecule_type() &&
                     (*d)->GetMolecule_type() == CBiomol_descr::eMolecule_type_protein)
                isProtein = true;
            if (isProtein && name) break;
        }

        // get gi
        if ((*m)->IsSetSeq_id())
            sid = &((*m)->GetSeq_id());

        // add protein to list
        if (isProtein && name && sid != NULL) {
            moleculeIDs[sid] = (*m)->GetId().Get();
            chains.push_back(make_pair(sid, name));
        }
    }
    if (chains.size() == 0) {
        ERRORMSG("No protein chains found in this structure!");
        return;
    }

    // get protein name (PDB identifier)
    string pdbID;
    CBiostruc::TDescr::const_iterator n, ne = biostruc->GetDescr().end();
    for (n=biostruc->GetDescr().begin(); n!=ne; ++n) {
        if ((*n)->IsName()) {
            pdbID = (*n)->GetName();    // assume first 'name' is pdb id
            break;
        }
    }

    // which chains to align?
    vector < const CSeq_id * > sids;
    if (chains.size() == 1) {
        sids.push_back(chains[0].first);
    } else {
        wxString *choices = new wxString[chains.size()];
        int choice;
        for (choice=0; choice<chains.size(); ++choice)
            choices[choice].Printf("%s_%c %s",
                pdbID.c_str(), chains[choice].second, chains[choice].first->GetSeqIdString().c_str());
        wxArrayInt selections;
        int nsel = wxGetMultipleChoices(selections, "Which chain do you want to align?",
            "Select Chain", chains.size(), choices, *viewerWindow);
        if (nsel == 0) return;
        for (choice=0; choice<nsel; ++choice)
            sids.push_back(chains[selections[choice]].first);
    }

    SequenceList newSequences;
    SequenceSet::SequenceList::const_iterator s, se = master->parentSet->sequenceSet->sequences.end();
    map < const Sequence * , const CSeq_id * > seq2id;
    for (int j=0; j<sids.size(); ++j) {

        // first check to see if this sequence is already present
        for (s=master->parentSet->sequenceSet->sequences.begin(); s!=se; ++s) {
            if ((*s)->identifier->MatchesSeqId(*(sids[j]))) {
                TRACEMSG("using existing sequence for " << sids[j]->GetSeqIdString());
                newSequences.push_back(*s);
                seq2id[*s] = sids[j];
                break;
            }
        }

        if (s == se) {
            // if not, find the sequence in the list from the structure file
            BioseqRefList::iterator b, be = bioseqs.end();
            for (b=bioseqs.begin(); b!=be; ++b) {
                CBioseq::TId::const_iterator i, ie = (*b)->GetId().end();
                for (i=(*b)->GetId().begin(); i!=ie; ++i) {
                    if ((*i)->Match(*(sids[j])))
                        break;
                }
                if (i != ie) {
                    const Sequence *sequence = master->parentSet->CreateNewSequence(**b);
                    if (sequence) {
                        TRACEMSG("found Bioseq for " << sids[j]->GetSeqIdString());
                        newSequences.push_back(sequence);
                        seq2id[sequence] = sids[j];
                        break;
                    }
                }
            }
            if (b == be) {
                ERRORMSG("ImportStructure() - can't find " << sids[j]->GetSeqIdString() << " in bioseq list!");
//                return;
            }
        }
    }

    SequenceList::const_iterator w, we = newSequences.end();
    for (w=newSequences.begin(); w!=we; ++w) {

        // add MMDB id tag to Bioseq if not present already
        (*w)->AddMMDBAnnotTag(mmdbID);

        // add MMDB and molecule id to identifier if not already set
        if ((*w)->identifier->mmdbID == MoleculeIdentifier::VALUE_NOT_SET) {
            (const_cast<MoleculeIdentifier*>((*w)->identifier))->mmdbID = mmdbID;
        } else {
            if ((*w)->identifier->mmdbID != mmdbID)
                ERRORMSG("MMDB ID mismatch in sequence " << (*w)->identifier->ToString()
                    << "; " << (*w)->identifier->mmdbID << " vs " << mmdbID);
        }
        if (moleculeIDs.find(seq2id[*w]) != moleculeIDs.end()) {
            if ((*w)->identifier->moleculeID == MoleculeIdentifier::VALUE_NOT_SET) {
                (const_cast<MoleculeIdentifier*>((*w)->identifier))->moleculeID =
                    moleculeIDs[seq2id[*w]];
            } else {
                if ((*w)->identifier->moleculeID != moleculeIDs[seq2id[*w]])
                    ERRORMSG("Molecule ID mismatch in sequence " << (*w)->identifier->ToString());
            }
        } else
            ERRORMSG("No matching id for MMDB sequence " << (*w)->identifier->ToString());
    }

    // create VAST alignments (or null-alignments if master is unstructured)
    AlignmentList newAlignments;
    if (master->molecule) {
        int masterFrom = -1, masterTo = -1;
        const BlockMultipleAlignment *multiple = alignmentManager->GetCurrentMultipleAlignment();
        if (multiple) {
            BlockMultipleAlignment::UngappedAlignedBlockList aBlocks;
            multiple->GetUngappedAlignedBlocks(&aBlocks);
            if (aBlocks.size() > 0) {
                masterFrom = aBlocks.front()->GetRangeOfRow(0)->from;
                masterTo = aBlocks.back()->GetRangeOfRow(0)->to;
            }
        }
        GetVASTAlignments(newSequences, master, &newAlignments,
            &pendingStructureAlignments, masterFrom, masterTo);
    } else {
        SequenceList::const_iterator s, se = newSequences.end();
        for (s=newSequences.begin(); s!=se; ++s) {
            // create initially empty alignment
            BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
            (*seqs)[0] = master;
            (*seqs)[1] = *s;
            BlockMultipleAlignment *newAlignment =
                new BlockMultipleAlignment(seqs, master->parentSet->alignmentManager);
            // finalize alignment
            if (!newAlignment->AddUnalignedBlocks() || !newAlignment->UpdateBlockMapAndColors(false)) {
                ERRORMSG("ImportStructure() - error finalizing alignment");
                delete newAlignment;
                continue;
            }
            newAlignments.push_back(newAlignment);
        }
    }

    // add new alignment to update list
    if (newAlignments.size() == newSequences.size())
        AddAlignments(newAlignments);
    else {
        ERRORMSG("UpdateViewer::ImportStructure() - no new alignments were created");
        DELETE_ALL_AND_CLEAR(newAlignments, AlignmentList);
        return;
    }

    // will add Biostruc and structure alignments to ASN data later on, after merge
    if (master->molecule) {
        pendingStructures.push_back(biostruc);
        wxMessageBox("The structure has been successfully imported! However, it will not appear until you\n"
            "save this data to a file and then re-load it in a new session. And depending on the type\n"
            "of data, it still might not appear unless the corresponding new pairwise alignment has\n"
            "been merged into the multiple alignment.",
            "Structure Added", wxOK | wxICON_INFORMATION, *viewerWindow);
    } else {
        // sort of assumes alignment with non-structured master is CDD, or uses cache mechanism...
        wxMessageBox("The selected sequence from this structure has been imported! However, the structure\n"
            "itself can not appear in the structure window until the sequence has been merged into the\n"
            "multiple alignment and the alignment has been remastered to a structured sequence.",
            "Sequence Added", wxOK | wxICON_INFORMATION, *viewerWindow);
    }
}

void UpdateViewer::SavePendingStructures(void)
{
    TRACEMSG("saving pending imported structures and structure alignments");
    StructureSet *sSet =
        (alignmentManager->GetCurrentMultipleAlignment() &&
         alignmentManager->GetCurrentMultipleAlignment()->GetMaster()) ?
         alignmentManager->GetCurrentMultipleAlignment()->GetMaster()->parentSet : NULL;
    if (!sSet) return;
    while (pendingStructures.size() > 0) {
        if (!sSet->AddBiostrucToASN(pendingStructures.front().GetPointer())) {
            ERRORMSG("UpdateViewer::SavePendingStructures() - error saving Biostruc");
            return;
        }
        pendingStructures.pop_front();
    }
    while (pendingStructureAlignments.size() > 0) {
        sSet->AddStructureAlignment(pendingStructureAlignments.front().structureAlignment.GetPointer(),
            pendingStructureAlignments.front().masterDomainID,
            pendingStructureAlignments.front().slaveDomainID);
        pendingStructureAlignments.pop_front();
    }
}

void UpdateViewer::BlastUpdate(BlockMultipleAlignment *alignment, bool usePSSMFromMultiple)
{
    const BlockMultipleAlignment *multipleForPSSM = alignmentManager->GetCurrentMultipleAlignment();
    if (usePSSMFromMultiple && !multipleForPSSM) {
        ERRORMSG("Can't do BLAST/PSSM when no multiple alignment is present");
        return;
    }

    // find alignment, and replace it with BLAST result
    AlignmentList::iterator a, ae = GetCurrentAlignments().end();
    for (a=GetCurrentAlignments().begin(); a!=ae; ++a) {
        if (*a != alignment) continue;

        // run BLAST between master and first slave (should be only one slave...)
        BLASTer::AlignmentList toRealign;
        toRealign.push_back(alignment);
        BLASTer::AlignmentList newAlignments;
        alignmentManager->blaster->CreateNewPairwiseAlignmentsByBlast(
            multipleForPSSM, toRealign, &newAlignments, usePSSMFromMultiple);
        if (newAlignments.size() != 1) {
            ERRORMSG("UpdateViewer::BlastUpdate() - CreateNewPairwiseAlignmentsByBlast() failed");
            return;
        }
        if (newAlignments.front()->NAlignedBlocks() == 0) {
            WARNINGMSG("alignment unchanged");
            delete newAlignments.front();
            return;
        }

        // replace alignment with BLAST result
        TRACEMSG("BLAST succeeded - replacing alignment");
        delete alignment;
        *a = newAlignments.front();
        break;
    }

    // recreate alignment display with new alignment
    AlignmentList copy = GetCurrentAlignments();
    GetCurrentAlignments().clear();
    GetCurrentDisplay()->Empty();
    AddAlignments(copy);
//    (*viewerWindow)->ScrollToColumn(GetCurrentDisplay()->GetStartingColumn());
}

static void MapSlaveToMaster(const BlockMultipleAlignment *alignment,
    int slaveRow, vector < int > *slave2master)
{
    slave2master->clear();
    slave2master->resize(alignment->GetSequenceOfRow(slaveRow)->Length(), -1);
    BlockMultipleAlignment::UngappedAlignedBlockList uaBlocks;
    alignment->GetUngappedAlignedBlocks(&uaBlocks);
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = uaBlocks.end();
    for (b=uaBlocks.begin(); b!=be; ++b) {
        const Block::Range
            *masterRange = (*b)->GetRangeOfRow(0),
            *slaveRange = (*b)->GetRangeOfRow(slaveRow);
        for (int i=0; i<(*b)->width; ++i)
            (*slave2master)[slaveRange->from + i] = masterRange->from + i;
    }
}

static BlockMultipleAlignment * GetAlignmentByBestNeighbor(
    const BlockMultipleAlignment *multiple,
    const BLASTer::AlignmentList rowAlignments)
{
    if (rowAlignments.size() != multiple->NRows()) {
        ERRORMSG("GetAlignmentByBestNeighbor: wrong # alignments");
        return NULL;
    }

    // find best-scoring aligment above some threshold
    const BlockMultipleAlignment *bestMatchFromMultiple = NULL;
    int b, bestRow = -1;
    BLASTer::AlignmentList::const_iterator p, pe = rowAlignments.end();
    for (b=0, p=rowAlignments.begin(); p!=pe; ++b, ++p) {
        if (!bestMatchFromMultiple || (*p)->GetRowDouble(0) < bestMatchFromMultiple->GetRowDouble(0)) {
            bestMatchFromMultiple = *p;
            bestRow = b;
        }
    }
    if (!bestMatchFromMultiple || bestMatchFromMultiple->GetRowDouble(0) > 0.000001) {
        WARNINGMSG("GetAlignmentByBestNeighbor: no significant hit found");
        return NULL;
    }
    INFOMSG("Closest neighbor from multiple: sequence "
        << bestMatchFromMultiple->GetMaster()->identifier->ToString()
        << ", E-value: " << bestMatchFromMultiple->GetRowDouble(0));
    GlobalMessenger()->RemoveAllHighlights(true);
    GlobalMessenger()->AddHighlights(
        bestMatchFromMultiple->GetMaster(), 0, bestMatchFromMultiple->GetMaster()->Length()-1);

    // if the best match is the multiple's master, then just use that alignment
    if (bestRow == 0) return bestMatchFromMultiple->Clone();

    // otherwise, use best match as a guide alignment to align the slave with the multiple's master
    vector < int > import2slave, slave2master;
    MapSlaveToMaster(bestMatchFromMultiple, 1, &import2slave);
    MapSlaveToMaster(multiple, bestRow, &slave2master);

    const Sequence *importSeq = bestMatchFromMultiple->GetSequenceOfRow(1);
    BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
    (*seqs)[0] = multiple->GetSequenceOfRow(0);
    (*seqs)[1] = importSeq;
    BlockMultipleAlignment *newAlignment =
        new BlockMultipleAlignment(seqs, importSeq->parentSet->alignmentManager);

    // create maximally sized blocks
    int masterStart=-1, importStart, importLoc, slaveLoc, masterLoc, len=0;
    for (importStart=-1, importLoc=0; importLoc<=importSeq->Length(); ++importLoc) {

        // map import -> slave -> master
        slaveLoc = (importLoc<importSeq->Length()) ? import2slave[importLoc] : -1;
        masterLoc = (slaveLoc >= 0) ? slave2master[slaveLoc] : -1;

        // if we're currently inside a block..
        if (importStart >= 0) {

            // add another residue to a continuously aligned block
            if (masterLoc >= 0 && masterLoc-masterStart == importLoc-importStart) {
                ++len;
            }

            // terminate block
            else {
                UngappedAlignedBlock *newBlock = new UngappedAlignedBlock(newAlignment);
                newBlock->SetRangeOfRow(0, masterStart, masterStart + len - 1);
                newBlock->SetRangeOfRow(1, importStart, importStart + len - 1);
                newBlock->width = len;
                newAlignment->AddAlignedBlockAtEnd(newBlock);
                importStart = -1;
            }
        }

        // search for start of block
        if (importStart < 0) {
            if (masterLoc >= 0) {
                masterStart = masterLoc;
                importStart = importLoc;
                len = 1;
            }
        }
    }

    // finalize and and add new alignment to list
    if (!newAlignment->AddUnalignedBlocks() || !newAlignment->UpdateBlockMapAndColors(false)) {
        ERRORMSG("error finalizing alignment");
        delete newAlignment;
        return NULL;
    }

    return newAlignment;
}

void UpdateViewer::BlastNeighbor(BlockMultipleAlignment *update)
{
    const BlockMultipleAlignment *multiple = alignmentManager->GetCurrentMultipleAlignment();
    if (!multiple) {
        ERRORMSG("Can't do BLAST Neighbor when no multiple alignment is present");
        return;
    }
    BlockMultipleAlignment::UngappedAlignedBlockList uaBlocks;
    multiple->GetUngappedAlignedBlocks(&uaBlocks);
    if (uaBlocks.size() == 0) {
        ERRORMSG("Can't do BLAST Neighbor with null multiple alignment");
        return;
    }
    const Sequence *updateSeq = update->GetSequenceOfRow(1);

    // find alignment, to replace it with BLAST result
    AlignmentList::iterator a, ae = GetCurrentAlignments().end();
    for (a=GetCurrentAlignments().begin(); a!=ae; ++a)
        if (*a == update) break;
    if (a == GetCurrentAlignments().end()) return;

    // set up BLAST-2-sequences between update slave and each sequence from the multiple
    BLASTer::AlignmentList toRealign;
    for (int row=0; row<multiple->NRows(); ++row) {
        BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
        (*seqs)[0] = multiple->GetSequenceOfRow(row);
        (*seqs)[1] = updateSeq;
        BlockMultipleAlignment *newAlignment =
            new BlockMultipleAlignment(seqs, updateSeq->parentSet->alignmentManager);
        if (newAlignment->AddUnalignedBlocks() && newAlignment->UpdateBlockMapAndColors(false))
        {
            int excess = 0;
            if (!RegistryGetInteger(REG_ADVANCED_SECTION, REG_FOOTPRINT_RES, &excess))
                WARNINGMSG("Can't get footprint excess residues from registry");
            newAlignment->alignMasterFrom = uaBlocks.front()->GetRangeOfRow(row)->from - excess;
            if (newAlignment->alignMasterFrom < 0)
                newAlignment->alignMasterFrom = 0;
            newAlignment->alignMasterTo = uaBlocks.back()->GetRangeOfRow(row)->to + excess;
            if (newAlignment->alignMasterTo >= (*seqs)[0]->Length())
                newAlignment->alignMasterTo = (*seqs)[0]->Length() - 1;
            newAlignment->alignSlaveFrom = update->alignSlaveFrom;
            newAlignment->alignSlaveTo = update->alignSlaveTo;
            toRealign.push_back(newAlignment);
        } else {
            ERRORMSG("error finalizing alignment");
            delete newAlignment;
        }
    }

    // actually do BLAST alignments
    BLASTer::AlignmentList newAlignments;
    SetDiagPostLevel(eDiag_Error); // ignore all but Errors while reading data
    alignmentManager->blaster->
        CreateNewPairwiseAlignmentsByBlast(NULL, toRealign, &newAlignments, false);
    SetDiagPostLevel(eDiag_Info);
    DELETE_ALL_AND_CLEAR(toRealign, BLASTer::AlignmentList);
    if (newAlignments.size() != multiple->NRows()) {
        ERRORMSG("UpdateViewer::BlastUpdate() - CreateNewPairwiseAlignmentsByBlast() failed");
        return;
    }

    // replace alignment with result
    BlockMultipleAlignment *alignmentByNeighbor = GetAlignmentByBestNeighbor(multiple, newAlignments);
    DELETE_ALL_AND_CLEAR(newAlignments, BLASTer::AlignmentList);
    if (!alignmentByNeighbor) {
        WARNINGMSG("alignment unchanged");
        return;
    }
    TRACEMSG("BLAST Neighbor succeeded - replacing alignment");
    delete update;
    *a = alignmentByNeighbor;

    // recreate alignment display with new alignment
    AlignmentList copy = GetCurrentAlignments();
    GetCurrentAlignments().clear();
    GetCurrentDisplay()->Empty();
    AddAlignments(copy);
//    (*viewerWindow)->ScrollToColumn(GetCurrentDisplay()->GetStartingColumn());
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
    TRACEMSG("sorting updates by identifier");
    updateComparisonFunction = CompareUpdatesByIdentifier;
    SortUpdates();
}

void UpdateViewer::SortUpdates(void)
{
    if (!updateComparisonFunction) {
        ERRORMSG("UpdateViewer::SortUpdates() - must first set comparison function");
        return;
    }

    // make vector of alignments
    AlignmentList& currentAlignments = GetCurrentAlignments();
    if (currentAlignments.size() < 2) return;
    vector < BlockMultipleAlignment * > sortedVector(currentAlignments.size());
    AlignmentList::const_iterator a, ae = currentAlignments.end();
    int i = 0;
    for (a=currentAlignments.begin(); a!=ae; ++a) sortedVector[i++] = *a;

    // sort them
    stable_sort(sortedVector.begin(), sortedVector.end(), updateComparisonFunction);
    updateComparisonFunction = NULL;

    // replace window contents with sorted list
    currentAlignments.clear();
    GetCurrentDisplay()->Empty();

    AlignmentList sortedList;
    for (i=0; i<sortedVector.size(); ++i) sortedList.push_back(sortedVector[i]);
    AddAlignments(sortedList);
}

END_SCOPE(Cn3D)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.78  2005/03/07 11:47:00  thiessen
* fix workshop warnings
*
* Revision 1.77  2004/10/04 17:48:56  thiessen
* fix bug after trying to fetch w/ invalid seq id
*
* Revision 1.76  2004/06/25 14:12:22  thiessen
* allow structures to be imported into non-structured alignment
*
* Revision 1.75  2004/06/23 20:34:53  thiessen
* sho geometry violations in alignments added to import window
*
* Revision 1.74  2004/06/02 14:56:22  thiessen
* remove version number from import comment
*
* Revision 1.73  2004/05/21 21:41:40  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.72  2004/05/21 17:29:51  thiessen
* allow conversion of mime to cdd data
*
* Revision 1.71  2004/03/15 18:38:52  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.70  2004/02/19 17:05:20  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.69  2004/01/17 00:17:32  thiessen
* add Biostruc and network structure load
*
* Revision 1.68  2004/01/05 17:09:16  thiessen
* abort import and warn if same accession different gi
*
* Revision 1.67  2003/11/06 18:52:32  thiessen
* make geometry violations shown on/off; allow multiple pmid entry in ref dialog
*
* Revision 1.66  2003/09/25 14:11:43  thiessen
* don't assume gi Seq-id's for imported structure's sequences
*
* Revision 1.65  2003/08/21 18:27:40  thiessen
* change header order for Mac compilation
*
* Revision 1.64  2003/07/14 18:37:08  thiessen
* change GetUngappedAlignedBlocks() param types; other syntax changes
*
* Revision 1.63  2003/04/04 14:02:22  thiessen
* more informative error messages on structure import
*
* Revision 1.62  2003/04/02 18:03:16  thiessen
* fix wxString/string confusion
*
* Revision 1.61  2003/04/02 17:49:18  thiessen
* allow pdb id's in structure import dialog
*
* Revision 1.60  2003/03/14 19:57:07  thiessen
* adjust wxStringTokenizer mode
*
* Revision 1.59  2003/03/14 19:48:51  thiessen
* allow multiple gi's in network sequence import dialog
*
* Revision 1.58  2003/03/13 18:55:17  thiessen
* tweak file load error reporting
*
* Revision 1.57  2003/02/03 19:20:08  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.56  2003/01/31 17:18:58  thiessen
* many small additions and changes...
*
* Revision 1.55  2003/01/23 20:03:05  thiessen
* add BLAST Neighbor algorithm
*
* Revision 1.54  2002/11/19 21:19:44  thiessen
* more const changes for objects; fix user vs default style bug
*
* Revision 1.53  2002/11/06 00:18:10  thiessen
* fixes for new CRef/const rules in objects
*
* Revision 1.52  2002/10/27 22:23:51  thiessen
* save structure alignments from vastalign.cgi imports
*
* Revision 1.51  2002/10/25 19:00:02  thiessen
* retrieve VAST alignment from vastalign.cgi on structure import
*
* Revision 1.50  2002/10/15 22:04:09  thiessen
* fix geom vltns bug
*
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
*/
