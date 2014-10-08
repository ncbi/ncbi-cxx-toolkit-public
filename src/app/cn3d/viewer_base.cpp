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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <map>

#include "remove_header_conflicts.hpp"

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
const unsigned int ViewerBase::MAX_UNDO_STACK_SIZE = 50;

ViewerBase::ViewerBase(ViewerWindowBase* *window, AlignmentManager *alnMgr) :
    alignmentManager(alnMgr), viewerWindow(window), currentDisplay(NULL)
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

    unsigned int column, row;
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
    if (*viewerWindow) (*viewerWindow)->RefreshWidget();
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
