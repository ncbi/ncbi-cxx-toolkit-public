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
*      base functionality for non-GUI part of viewers
*
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <map>

#include "viewer_base.hpp"
#include "viewer_window_base.hpp"
#include "sequence_display.hpp"
#include "messenger.hpp"
#include "cn3d_tools.hpp"
#include "alignment_manager.hpp"
#include "cn3d_blast.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

// limits the size of the stack (set to -1 for unlimited)
const int ViewerBase::MAX_UNDO_STACK_SIZE = 50;

ViewerBase::ViewerBase(ViewerWindowBase* *window, AlignmentManager *alnMgr) :
    viewerWindow(window), alignmentManager(alnMgr), currentDisplay(NULL)
{
    if (!window) ERRORMSG("ViewerBase::ViewerBase() - got NULL handle");
}

ViewerBase::~ViewerBase(void)
{
    DestroyGUI();
    ClearAllData();
}

void ViewerBase::DestroyGUI(void)
{
    if ((*viewerWindow)) {
        (*viewerWindow)->KillWindow();
        GUIDestroyed();
    }
}

void ViewerBase::SetWindowTitle(void) const
{
    if (*viewerWindow)
        (*viewerWindow)->SetWindowTitle();
}

void ViewerBase::InitData(const AlignmentList *alignments, SequenceDisplay *display)
{
    ClearAllData();
    if (alignments) currentAlignments = *alignments;    // copy list
    currentDisplay = display;
    stacksEnabled = false;
    SetUndoRedoMenuStates();
}

void ViewerBase::EnableStacks(void)
{
    if (stacksEnabled) {
        ERRORMSG("ViewerBase::EnableStacks() - already enabled!");
        return;
    }

    stacksEnabled = true;
    nRedosStored = 0;
    Save();
}

void ViewerBase::Save(void)
{
    if (!currentDisplay || !stacksEnabled) {
        ERRORMSG("ViewerBase::Save() - stacks not enabled, or no alignment/display data");
        return;
    }

    // clear out any data in the stack above the current position (deletes "redo" list)
    if (nRedosStored > 0) {
        TRACEMSG("deleting " << nRedosStored << " redo elements from the stack");
        for (; nRedosStored>0; --nRedosStored) {
            DELETE_ALL_AND_CLEAR(alignmentStack.back(), AlignmentList);
            alignmentStack.pop_back();
            delete displayStack.back();
            displayStack.pop_back();
        }
    }

    // remove the one-up-from-bottom of the stack if it's too big (so original isn't lost)
    if (alignmentStack.size() == MAX_UNDO_STACK_SIZE) {
        WARNINGMSG("max undo stack size exceeded - deleting next-from-bottom item");
        DELETE_ALL_AND_CLEAR(*(++(alignmentStack.begin())), AlignmentList);
        alignmentStack.erase(++(alignmentStack.begin()));
        delete *(++(displayStack.begin()));
        displayStack.erase(++(displayStack.begin()));
    }

    // clone current alignments onto top of stack
    alignmentStack.resize(alignmentStack.size() + 1);
    Old2NewAlignmentMap newAlignmentMap;
    AlignmentList::const_iterator a, ae = currentAlignments.end();
    for (a=currentAlignments.begin(); a!=ae; ++a) {
        BlockMultipleAlignment *newAlignment = (*a)->Clone();
        alignmentStack.back().push_back(newAlignment);
        newAlignmentMap[*a] = newAlignment;
    }

    // clone display
    displayStack.push_back(currentDisplay->Clone(newAlignmentMap));

    SetUndoRedoMenuStates();
}

void ViewerBase::Undo(void)
{
    if ((alignmentStack.size() - nRedosStored) <= 1 || !stacksEnabled) {
        ERRORMSG("ViewerBase::Undo() - stacks disabled, or no more undo data");
        return;
    }

    ++nRedosStored;
    CopyDataFromStack();
    SetUndoRedoMenuStates();
}

void ViewerBase::Redo(void)
{
    if (nRedosStored == 0 || !stacksEnabled) {
        ERRORMSG("ViewerBase::Redo() - stacks disabled, or no more redo data");
        return;
    }

    --nRedosStored;
    CopyDataFromStack();
    SetUndoRedoMenuStates();
}

void ViewerBase::SetUndoRedoMenuStates(void)
{
    if (*viewerWindow) {
        (*viewerWindow)->EnableUndo((alignmentStack.size() - nRedosStored) > 1 && stacksEnabled);
        (*viewerWindow)->EnableRedo(nRedosStored > 0 && stacksEnabled);
    }
}

void ViewerBase::CopyDataFromStack(void)
{
    // delete current objects
    DELETE_ALL_AND_CLEAR(currentAlignments, AlignmentList);
    delete currentDisplay;

    // move to appropriate stack object
    AlignmentStack::reverse_iterator as = alignmentStack.rbegin();
    DisplayStack::reverse_iterator ds = displayStack.rbegin();
    for (int i=0; i<nRedosStored; ++i) {
        ++as;
        ++ds;
    }

    // clone alignments into current
    Old2NewAlignmentMap newAlignmentMap;
    AlignmentList::const_iterator a, ae = as->end();
    for (a=as->begin(); a!=ae; ++a) {
        BlockMultipleAlignment *newAlignment = (*a)->Clone();
        currentAlignments.push_back(newAlignment);
        newAlignmentMap[*a] = newAlignment;
    }

    // clone display
    currentDisplay = (*ds)->Clone(newAlignmentMap);
}

void ViewerBase::ClearAllData(void)
{
    DELETE_ALL_AND_CLEAR(currentAlignments, AlignmentList);
    while (alignmentStack.size() > 0) {
        DELETE_ALL_AND_CLEAR(alignmentStack.back(), AlignmentList);
        alignmentStack.pop_back();
    }

    delete currentDisplay;
    currentDisplay = NULL;
    while (displayStack.size() > 0) {
        delete displayStack.back();
        displayStack.pop_back();
    }
}

void ViewerBase::Revert(void)
{
    if (!stacksEnabled) {
        ERRORMSG("ViewerBase::Revert() - stacks disabled!");
        return;
    }

    nRedosStored = 0;

    // revert to the bottom of the stack; delete the rest
    while (alignmentStack.size() > 0) {

        if (alignmentStack.size() == 1)
            CopyDataFromStack();

        DELETE_ALL_AND_CLEAR(alignmentStack.back(), AlignmentList);
        alignmentStack.pop_back();
        delete displayStack.back();
        displayStack.pop_back();
    }
    stacksEnabled = false;
    SetUndoRedoMenuStates();
}

void ViewerBase::KeepCurrent(void)
{
    if (!stacksEnabled) return;

    // delete all stack data (leaving current stuff unchanged)
    while (alignmentStack.size() > 0) {
        DELETE_ALL_AND_CLEAR(alignmentStack.back(), AlignmentList);
        alignmentStack.pop_back();
    }
    while (displayStack.size() > 0) {
        delete displayStack.back();
        displayStack.pop_back();
    }
    nRedosStored = 0;
    stacksEnabled = false;
    SetUndoRedoMenuStates();
}

bool ViewerBase::EditorIsOn(void) const
{
    return (*viewerWindow && (*viewerWindow)->EditorIsOn());
}

void ViewerBase::NewFont(void)
{
    if (*viewerWindow) (*viewerWindow)->SetupFontFromRegistry();
}

void ViewerBase::MakeResidueVisible(const Molecule *molecule, int seqIndex)
{
    if (!(*viewerWindow) || !currentDisplay) return;

    int column, row;
    if (currentDisplay->GetDisplayCoordinates(molecule, seqIndex,
            (*viewerWindow)->GetCurrentJustification(), &column, &row))
        (*viewerWindow)->MakeCellVisible(column, row);
}

void ViewerBase::MakeSequenceVisible(const MoleculeIdentifier *identifier)
{
    if (*viewerWindow) (*viewerWindow)->MakeSequenceVisible(identifier);
}

void ViewerBase::SaveDialog(bool prompt)
{
    if (*viewerWindow) (*viewerWindow)->SaveDialog(prompt, false);
}

void ViewerBase::Refresh(void)
{
    if (*viewerWindow) (*viewerWindow)->Refresh();
}

void ViewerBase::CalculateSelfHitScores(const BlockMultipleAlignment *multiple)
{
    alignmentManager->blaster->CalculateSelfHitScores(multiple);
}

void ViewerBase::RemoveBlockBoundaryRows(void)
{
    currentDisplay->RemoveBlockBoundaryRows();
}

END_SCOPE(Cn3D)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.22  2004/05/21 21:41:40  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.21  2004/03/15 18:11:01  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.20  2004/02/19 17:05:22  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.19  2003/02/03 19:20:08  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.18  2002/10/25 19:00:02  thiessen
* retrieve VAST alignment from vastalign.cgi on structure import
*
* Revision 1.17  2002/10/13 22:58:08  thiessen
* add redo ability to editor
*
* Revision 1.16  2002/10/07 13:29:32  thiessen
* add double-click -> show row to taxonomy tree
*
* Revision 1.15  2002/09/09 13:38:23  thiessen
* separate save and save-as
*
* Revision 1.14  2002/08/15 22:13:18  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.13  2002/06/05 14:28:42  thiessen
* reorganize handling of window titles
*
* Revision 1.12  2002/02/05 18:53:26  thiessen
* scroll to residue in sequence windows when selected in structure window
*
* Revision 1.11  2001/08/14 17:18:22  thiessen
* add user font selection, store in registry
*
* Revision 1.10  2001/05/31 18:47:11  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.9  2001/05/02 13:46:29  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.8  2001/04/05 22:55:36  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.7  2001/04/04 00:27:16  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.6  2001/03/30 14:43:41  thiessen
* show threader scores in status line; misc UI tweaks
*
* Revision 1.5  2001/03/22 00:33:18  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.4  2001/03/13 01:25:06  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.3  2001/03/02 15:32:52  thiessen
* minor fixes to save & show/hide dialogs, wx string headers
*
* Revision 1.2  2001/03/02 03:26:59  thiessen
* fix dangling pointer upon app close
*
* Revision 1.1  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
*/
