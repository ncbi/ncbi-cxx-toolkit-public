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

#include "cn3d/update_viewer.hpp"
#include "cn3d/update_viewer_window.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/sequence_display.hpp"
#include "cn3d/cn3d_colors.hpp"
#include "cn3d/alignment_manager.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

UpdateViewer::UpdateViewer(AlignmentManager *alnMgr) :
    // not sure why this cast is necessary, but MSVC requires it...
    ViewerBase(reinterpret_cast<ViewerWindowBase**>(&updateWindow)),
    alignmentManager(alnMgr), updateWindow(NULL)
{
}

UpdateViewer::~UpdateViewer(void)
{
    DestroyGUI();
    ClearStacks();
}

void UpdateViewer::CreateUpdateWindow(void)
{
    if (updateWindow) {
        updateWindow->Refresh();
    } else {
        SequenceDisplay *display = GetCurrentDisplay();
        if (display) {
            if (!updateWindow) updateWindow = new UpdateViewerWindow(this);
            if (displayStack.size() > 2) updateWindow->EnableUndo(true);
            updateWindow->NewDisplay(display, true);
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
    if (newAlignments.size() == 0) return;

    AlignmentList *alignments = (alignmentStack.size() > 0) ? &(alignmentStack.back()) : NULL;
    SequenceDisplay *display = GetCurrentDisplay();
    if (!display) display = new SequenceDisplay(true, viewerWindow);

    // populate succesive lines of the display with each alignment, with blank lines inbetween
    AlignmentList::const_iterator a, ae = newAlignments.end();
    for (a=newAlignments.begin(); a!=ae; a++) {
        if ((*a)->NRows() != 2) {
            ERR_POST(Error << "UpdateViewer::AddAlignments() - got alignment with != 2 rows");
            continue;
        }

        // add alignment to stack list
        if (alignments) alignments->push_back(*a);

        // add alignment to the display, including block row since editor is always on
        if (display->NRows() != 0) display->AddRowFromString("");
        display->AddBlockBoundaryRow(*a);
        for (int row=0; row<2; row++)
            display->AddRowFromAlignment(row, *a);
    }

    // start undo stacks if this is the first set of alignments added
    if (!GetCurrentDisplay()) {
        InitStacks(&newAlignments, display);
        display->SetStartingColumn(GetCurrentAlignments()->front()->GetFirstAlignedBlockPosition() - 5);
    }

    if (!updateWindow) CreateUpdateWindow();
    PushAlignment();    // make this an undoable operation
}

void UpdateViewer::OverrideBackgroundColor(int column, int row, bool *drawBackground, Vector *bgColorVec)
{
    DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(GetCurrentDisplay()->rows[row]);
    if (alnRow && alnRow->row == 0) {
        const Sequence *sequence;
        int seqIndex;
        bool isAlignedInUpdate;
        if (!alnRow->alignment->GetSequenceAndIndexAt(column, 0,
                (*viewerWindow)->GetCurrentJustification(), &sequence, &seqIndex, &isAlignedInUpdate))
            return;

        // give special color if a residue residue on the master is aligned in the multiple alignment,
        // but unaligned in a master/slave pairwise update alignment
        if (!isAlignedInUpdate && seqIndex >= 0 &&
                alignmentManager->GetCurrentMultipleAlignment()->IsAligned(sequence, seqIndex)) {
            *drawBackground = true;
            *bgColorVec = GlobalColors()->Get(Colors::eUnalignedInUpdate);
        }
    }
}

END_SCOPE(Cn3D)

