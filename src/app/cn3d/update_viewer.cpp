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

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

UpdateViewer::UpdateViewer(void) :
    // not sure why this cast is necessary, but MSVC requires it...
    ViewerBase(reinterpret_cast<ViewerWindowBase**>(&updateWindow)),
    updateWindow(NULL)
{
}

UpdateViewer::~UpdateViewer(void)
{
}

void UpdateViewer::CreateUpdateWindow(void)
{
    if (updateWindow) {
        updateWindow->Refresh();
    } else {
        SequenceDisplay *display = GetCurrentDisplay();
        if (display) {
            if (!updateWindow) updateWindow = new UpdateViewerWindow(this);
            updateWindow->NewDisplay(display);
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

void UpdateViewer::DisplayAlignments(const AlignmentList& alignmentList)
{
    ClearStacks();
    alignments = alignmentList;

    SequenceDisplay *display = new SequenceDisplay(false, viewerWindow);

    // populate each line of the display with one alignment, with blank lines inbetween
    AlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; a++) {
        if (a != alignments.begin()) display->AddRowFromString("");
        for (int row=0; row<(*a)->NRows(); row++)
            display->AddRowFromAlignment(row, *a);
    }

    InitStacks(NULL, display);

    if (updateWindow)
        updateWindow->UpdateDisplay(display);
    else
        CreateUpdateWindow();
}

END_SCOPE(Cn3D)

