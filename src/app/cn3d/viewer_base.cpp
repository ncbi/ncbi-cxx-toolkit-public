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
* ---------------------------------------------------------------------------
* $Log$
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
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include <map>

#include "cn3d/viewer_base.hpp"
#include "cn3d/viewer_window_base.hpp"
#include "cn3d/sequence_display.hpp"
#include "cn3d/messenger.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

// limits the size of the stack (set to zero for unlimited)
static const int MAX_UNDO_STACK_SIZE = 50;


ViewerBase::ViewerBase(ViewerWindowBase* *window) :
    viewerWindow(window)
{
    if (!window) ERR_POST(Error << "ViewerBase::ViewerBase() - got NULL handle");
}

ViewerBase::~ViewerBase(void)
{
    DestroyGUI();
    ClearStacks();
}

void ViewerBase::DestroyGUI(void)
{
    if ((*viewerWindow)) {
        (*viewerWindow)->KillWindow();
        GUIDestroyed();
    }
}

void ViewerBase::InitStacks(const AlignmentList *alignments, SequenceDisplay *display)
{
    ClearStacks();
    if (alignments) {
        alignmentStack.resize(alignmentStack.size() + 1);
        alignmentStack.back() = *alignments;    // copy list
    }
    displayStack.push_back(display);
}

// this works like the OpenGL transform stack: when data is to be saved by pushing,
// the current data (display + alignment) is copied and put on the top of the stack,
// becoming the current viewed/edited alignment.
void ViewerBase::PushAlignment(void)
{
    TESTMSG("ViewerBase::PushAlignment() - stack size before push: " << displayStack.size());
    if (alignmentStack.size() == 0) {
        ERR_POST(Error << "ViewerBase::PushAlignment() - can't be called with empty alignment stack");
        return;
    }

    // clone alignments
    Old2NewAlignmentMap newAlignmentMap;
    AlignmentList::const_iterator a = alignmentStack.back().begin(), ae = alignmentStack.back().end();
    alignmentStack.resize(alignmentStack.size() + 1);
    for (; a!=ae; a++) {
        BlockMultipleAlignment *newAlignment = (*a)->Clone();
        alignmentStack.back().push_back(newAlignment);
        newAlignmentMap[*a] = newAlignment;
    }

    // clone and update display
    displayStack.push_back(displayStack.back()->Clone(newAlignmentMap));
    if (*viewerWindow) {
        (*viewerWindow)->UpdateDisplay(displayStack.back());
        if (displayStack.size() > 2) (*viewerWindow)->EnableUndo(true);
    }

    // trim the stack to some max size; but don't delete the bottom of the stack, which is
    // the original before editing was begun.
    if (MAX_UNDO_STACK_SIZE > 0 && alignmentStack.size() > MAX_UNDO_STACK_SIZE) {
        AlignmentStack::iterator al = alignmentStack.begin();
        al++;
        for (a=al->begin(), ae=al->end(); a!=ae; a++) delete *a;
        alignmentStack.erase(al);
        DisplayStack::iterator d = displayStack.begin();
        d++;
        delete *d;
        displayStack.erase(d);
    }
}

// there can be a miminum of two items on the stack - the bottom is always the original
// before editing; the top, when the editor is on, is always the current data, and just beneath
// the top is a copy of the current data
void ViewerBase::PopAlignment(void)
{
    if (alignmentStack.size() < 3 || displayStack.size() < 3) {
        ERR_POST(Error << "ViewerBase::PopAlignment() - no more data to pop off the stack");
        return;
    }

    // top two stack items are identical; delete both...
    for (int i=0; i<2; i++) {
        AlignmentList::const_iterator a, ae = alignmentStack.back().end();
        for (a=alignmentStack.back().begin(); a!=ae; a++) delete *a;
        alignmentStack.pop_back();
        delete displayStack.back();
        displayStack.pop_back();
    }

    // ... then add a copy of what's underneath, making that copy the current data
    PushAlignment();
    if (*viewerWindow && displayStack.size() == 2)
        (*viewerWindow)->EnableUndo(false);
}

void ViewerBase::ClearStacks(void)
{
    while (alignmentStack.size() > 0) {
        AlignmentList::const_iterator a, ae = alignmentStack.back().end();
        for (a=alignmentStack.back().begin(); a!=ae; a++) delete *a;
        alignmentStack.pop_back();
    }
    while (displayStack.size() > 0) {
        delete displayStack.back();
        displayStack.pop_back();
    }
}

void ViewerBase::RevertAlignment(void)
{
    // revert to the bottom of the stack
    while (alignmentStack.size() > 1) {
        AlignmentList::const_iterator a, ae = alignmentStack.back().end();
        for (a=alignmentStack.back().begin(); a!=ae; a++) delete *a;
        alignmentStack.pop_back();
        delete displayStack.back();
        displayStack.pop_back();
    }

    if (*viewerWindow) {
        (*viewerWindow)->UpdateDisplay(displayStack.back());
        if ((*viewerWindow)->AlwaysSyncStructures()) (*viewerWindow)->SyncStructures();
    }
}

void ViewerBase::KeepOnlyStackTop(void)
{
    // keep only the top of the stack
    while (alignmentStack.size() > 1) {
        AlignmentList::const_iterator a, ae = alignmentStack.front().end();
        for (a=alignmentStack.front().begin(); a!=ae; a++) delete *a;
        alignmentStack.pop_front();
        delete displayStack.front();
        displayStack.pop_front();
    }
}

END_SCOPE(Cn3D)

