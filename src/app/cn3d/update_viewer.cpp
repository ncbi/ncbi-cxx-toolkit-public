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

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include <objects/cdd/Cdd.hpp>
#include <objects/cdd/Update_align.hpp>
#include <objects/cdd/Update_comment.hpp>
#include <objects/seq/Seq_annot.hpp>

#include "cn3d/update_viewer.hpp"
#include "cn3d/update_viewer_window.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/sequence_display.hpp"
#include "cn3d/cn3d_colors.hpp"
#include "cn3d/alignment_manager.hpp"
#include "cn3d/cn3d_threader.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/molecule.hpp"

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
    InitStacks(&emptyAlignmentList, blankDisplay);
    PushAlignment();
}

UpdateViewer::~UpdateViewer(void)
{
    DestroyGUI();
    ClearStacks();
}

void UpdateViewer::SetInitialState(void)
{
    KeepOnlyStackTop();
    PushAlignment();
}

void UpdateViewer::SaveDialog(void)
{
    if (updateWindow) updateWindow->SaveDialog(false);
}

void UpdateViewer::CreateUpdateWindow(void)
{
    if (updateWindow) {
        GlobalMessenger()->PostRedrawSequenceViewer(this);
    } else {
        SequenceDisplay *display = GetCurrentDisplay();
        if (display) {
            if (!updateWindow) updateWindow = new UpdateViewerWindow(this);
            if (displayStack.size() > 2) updateWindow->EnableUndo(true);
            updateWindow->NewDisplay(display, true, false);
            updateWindow->ScrollToColumn(display->GetStartingColumn());
            updateWindow->Show(true);
            // ScrollTo causes immediate redraw, so don't need a second one
            GlobalMessenger()->UnPostRedrawSequenceViewer(this);
        }
    }
}

void UpdateViewer::Refresh(void)
{
    if (updateWindow) updateWindow->Refresh();
}

void UpdateViewer::AddAlignments(const AlignmentList& newAlignments)
{
    AlignmentList& alignments = alignmentStack.back();
    SequenceDisplay *display = displayStack.back();

    // populate succesive lines of the display with each alignment, with blank lines inbetween
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

    PushAlignment();    // make this an undoable operation
}

void UpdateViewer::ReplaceAlignments(const AlignmentList& alignmentList)
{
    // empty out the current alignment list and display (but not the undo stacks!)
    AlignmentList::const_iterator a, ae = alignmentStack.back().end();
    for (a=alignmentStack.back().begin(); a!=ae; a++) delete *a;
    alignmentStack.back().clear();

    delete displayStack.back();
    displayStack.back() = new SequenceDisplay(true, viewerWindow);

    AddAlignments(alignmentList);
}

void UpdateViewer::DeleteAlignment(BlockMultipleAlignment *toDelete)
{
    AlignmentList keepAlignments;
    AlignmentList::const_iterator a, ae = alignmentStack.back().end();
    for (a=alignmentStack.back().begin(); a!=ae; a++)
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

    AlignmentList::const_iterator a, ae = alignmentStack.back().end();
    for (a=alignmentStack.back().begin(); a!=ae; a++) {

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

        // if this is a new update, then assume that it comes from user demotion from the multiple;
        // create a new Update-align and tag it appropriately
        else {
            updateAlign = new CUpdate_align();

            // set type field depending on whether demoted sequence has structure
            updateAlign->SetType((*a)->GetSequenceOfRow(1)->molecule ?
                CUpdate_align::eType_demoted_3d : CUpdate_align::eType_demoted);

            // add a text comment
            CUpdate_comment *comment = new CUpdate_comment();
            comment->SetComment("Demoted from the multiple alignment by Cn3D++");
            updateAlign->SetDescription().resize(updateAlign->GetDescription().size() + 1);
            updateAlign->SetDescription().back().Reset(comment);

            // create a new Seq-annot
            CRef < CSeq_annot > seqAnnotRef(new CSeq_annot());
            seqAnnotRef->SetData().SetAlign();
            updateAlign->SetSeqannot(seqAnnotRef);
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
    alignmentManager->GetCurrentMultipleAlignment()->GetMaster()->parentSet->
        ReplaceUpdates(updates);
}

END_SCOPE(Cn3D)

