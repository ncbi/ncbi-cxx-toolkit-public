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
*      implementation of GUI part of update viewer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2001/04/04 00:27:15  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.5  2001/03/28 23:02:17  thiessen
* first working full threading
*
* Revision 1.4  2001/03/22 00:33:18  thiessen
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

#if defined(__WXMSW__)
#include <wx/msw/winundef.h>
#endif

#include <wx/wx.h>

#include "cn3d/update_viewer_window.hpp"
#include "cn3d/update_viewer.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/sequence_display.hpp"
#include "cn3d/alignment_manager.hpp"
#include "cn3d/cn3d_threader.hpp"
#include "cn3d/wx_tools.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

BEGIN_EVENT_TABLE(UpdateViewerWindow, wxFrame)
    INCLUDE_VIEWER_WINDOW_BASE_EVENTS
    EVT_CLOSE     (                                     UpdateViewerWindow::OnCloseWindow)
    EVT_MENU      (MID_RUN_THREADER,                    UpdateViewerWindow::OnRunThreader)
    EVT_MENU      (MID_MERGE_ALL,                       UpdateViewerWindow::OnMerge)
END_EVENT_TABLE()

UpdateViewerWindow::UpdateViewerWindow(UpdateViewer *parentUpdateViewer) :
    ViewerWindowBase(parentUpdateViewer, "Cn3D++ Update Viewer", wxPoint(0,50), wxSize(1000,300)),
    updateViewer(parentUpdateViewer)
{
    wxMenu *menu = new wxMenu;
    menu->Append(MID_RUN_THREADER, "&Run Threader");
    menuBar->Append(menu, "&Threader");

    menuBar->Enable(MID_SELECT_COLS, false);

    menu = new wxMenu;
    menu->Append(MID_MERGE_ALL, "Merge &All");
    menuBar->Append(menu, "&Merge");

    // editor always on
    EnableBaseEditorMenuItems(true);
    menuBar->Check(MID_ENABLE_EDIT, true);
    menuBar->Enable(MID_ENABLE_EDIT, false);
}

UpdateViewerWindow::~UpdateViewerWindow(void)
{
}

void UpdateViewerWindow::OnCloseWindow(wxCloseEvent& event)
{
    if (viewer) {
        viewer->GUIDestroyed(); // make sure UpdateViewer knows the GUI is gone
        GlobalMessenger()->UnPostRedrawSequenceViewer(viewer);  // don't try to redraw after destroyed!
    }
    Destroy();
}

void UpdateViewerWindow::EnableDerivedEditorMenuItems(bool enabled)
{
}

void UpdateViewerWindow::OnRunThreader(wxCommandEvent& event)
{
    ThreaderOptions options;

    ThreaderOptionsDialog optDialog(this, options);
    if (!optDialog.Activate()) return;  // user cancelled

    if (!optDialog.GetValues(&options)) {
        ERR_POST(Error << "Error retrieving options values from dialog");
        return;
    }

    SetCursor(*wxHOURGLASS_CURSOR);
    updateViewer->alignmentManager->ThreadUpdates(options);
    SetCursor(wxNullCursor);
}

void UpdateViewerWindow::OnMerge(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case MID_MERGE_ALL:
            UpdateViewerWindow::updateViewer->alignmentManager->MergeUpdates();
            break;
    }
}


/////////////////////////////////////////////////////////////////////////////////
// ThreaderOptionsDialog implementation
/////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(ThreaderOptionsDialog, wxFrame)
    EVT_BUTTON(-1, ThreaderOptionsDialog::OnButton)
    EVT_CLOSE (    ThreaderOptionsDialog::OnCloseWindow)
END_EVENT_TABLE()

ThreaderOptionsDialog::ThreaderOptionsDialog(wxWindow* parent, const ThreaderOptions& initialOptions) :
    wxFrame(parent, -1, "Set Threader Options", wxDefaultPosition, wxDefaultSize,
        wxCAPTION | wxSYSTEM_MENU // not resizable
//        wxDEFAULT_FRAME_STYLE   // for debugging/testing only - wxStaticBox doesn't work well with resize
)
{
    static const int margin = 15;
    SetBackgroundColour(*wxLIGHT_GREY);

    // create controls
    int boxClientHeight = 2*margin, maxTextWidth = 0;
    bOK = new wxButton(this, -1, "&OK");
    bCancel = new wxButton(this, -1, "&Cancel");

    box = new wxStaticBox(this, -1, "Available Threader Options");
    box->SetBackgroundColour(*wxLIGHT_GREY);
    wxStaticText
        *tWeight = new wxStaticText(box, -1, "", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT),
        *tLoops = new wxStaticText(box, -1, "", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT),
        *tStarts = new wxStaticText(box, -1, "", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT),
        *tResults = new wxStaticText(box, -1, "", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT),
        *tMerge = new wxStaticText(box, -1, "", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);

    tWeight->SetLabel("Weighting of PSSM/Contact score? [0 .. 1], 1 = PSSM only");
    if (tWeight->GetBestSize().GetWidth() > maxTextWidth) maxTextWidth = tWeight->GetBestSize().GetWidth();
    fpWeight = new FloatingPointSpinCtrl(box, 0.0, 1.0, 0.05, 0.5);
    boxClientHeight += fpWeight->GetBestSize().GetHeight() + margin;

    tLoops->SetLabel("Loop length multiplier? [0.1 .. 10]");
    if (tLoops->GetBestSize().GetWidth() > maxTextWidth) maxTextWidth = tLoops->GetBestSize().GetWidth();
    fpLoops = new FloatingPointSpinCtrl(box, 0.1, 10.0, 0.25, 1.5);
    boxClientHeight += fpLoops->GetBestSize().GetHeight() + margin;

    tStarts->SetLabel("Number of random starts? [1 .. 100]");
    if (tStarts->GetBestSize().GetWidth() > maxTextWidth) maxTextWidth = tStarts->GetBestSize().GetWidth();
    iStarts = new IntegerSpinCtrl(box, 1, 100, 5, 1);
    boxClientHeight += iStarts->GetBestSize().GetHeight() + margin;

    tResults->SetLabel("Number of result alignments per row? [1 .. 20]");
    if (tResults->GetBestSize().GetWidth() > maxTextWidth) maxTextWidth = tResults->GetBestSize().GetWidth();
    iResults = new IntegerSpinCtrl(box, 1, 20, 1, 1);
    boxClientHeight += iResults->GetBestSize().GetHeight() + margin;

    tMerge->SetLabel("Merge results after each row is threaded?");
    if (tMerge->GetBestSize().GetWidth() > maxTextWidth) maxTextWidth = tMerge->GetBestSize().GetWidth();
    bMerge = new wxCheckBox(box, -1, wxEmptyString);
    bMerge->SetValue(true);
    boxClientHeight += iResults->GetBestSize().GetHeight() + margin;

    // do layout
    wxLayoutConstraints *c;
    int textProportion = 100.0 * maxTextWidth / (maxTextWidth + 100 + 1.5*margin);

    // first, layout the controls within the box

    // PSSM weight
    c = new wxLayoutConstraints();
    c->top.SameAs           (box,       wxTop,      2*margin);
    c->height.SameAs        (fpWeight->GetTextCtrl(), wxHeight);
    c->left.SameAs          (box,       wxLeft,     margin);
    c->width.PercentOf      (box,       wxWidth,    textProportion);
    tWeight->SetConstraints(c);
    c = new wxLayoutConstraints();
    c->top.SameAs           (box,       wxTop,      2*margin);
    c->height.AsIs          ();
    c->left.RightOf         (tWeight,               margin);
    c->right.LeftOf         (fpWeight->GetSpinButton(), fpLoops->GetSpaceBetweenControls());
    fpWeight->GetTextCtrl()->SetConstraints(c);
    c = new wxLayoutConstraints();
    c->top.SameAs           (box,       wxTop,      2*margin);
    c->height.SameAs        (fpWeight->GetTextCtrl(), wxHeight);
    c->width.AsIs           ();
    c->right.SameAs         (box,       wxRight,    margin);
    fpWeight->GetSpinButton()->SetConstraints(c);

    // loop lengths
    c = new wxLayoutConstraints();
    c->top.SameAs           (tWeight,   wxBottom,   margin);
    c->height.SameAs        (fpLoops->GetTextCtrl(), wxHeight);
    c->left.SameAs          (box,       wxLeft,     margin);
    c->width.PercentOf      (box,       wxWidth,    textProportion);
    tLoops->SetConstraints(c);
    c = new wxLayoutConstraints();
    c->top.SameAs           (tWeight,   wxBottom,   margin);
    c->height.AsIs          ();
    c->left.RightOf         (tLoops,                margin);
    c->right.LeftOf         (fpLoops->GetSpinButton(), fpLoops->GetSpaceBetweenControls());
    fpLoops->GetTextCtrl()->SetConstraints(c);
    c = new wxLayoutConstraints();
    c->top.SameAs           (tWeight,   wxBottom,   margin);
    c->height.SameAs        (fpLoops->GetTextCtrl(), wxHeight);
    c->width.AsIs           ();
    c->right.SameAs         (box,       wxRight,    margin);
    fpLoops->GetSpinButton()->SetConstraints(c);

    // # random starts
    c = new wxLayoutConstraints();
    c->top.SameAs           (tLoops,    wxBottom,   margin);
    c->height.SameAs        (iStarts->GetTextCtrl(), wxHeight);
    c->left.SameAs          (box,       wxLeft,     margin);
    c->width.PercentOf      (box,       wxWidth,    textProportion);
    tStarts->SetConstraints(c);
    c = new wxLayoutConstraints();
    c->top.SameAs           (tLoops,    wxBottom,   margin);
    c->height.AsIs          ();
    c->left.RightOf         (tLoops,                margin);
    c->right.LeftOf         (iStarts->GetSpinButton(), iStarts->GetSpaceBetweenControls());
    iStarts->GetTextCtrl()->SetConstraints(c);
    c = new wxLayoutConstraints();
    c->top.SameAs           (tLoops,    wxBottom,   margin);
    c->height.SameAs        (iStarts->GetTextCtrl(), wxHeight);
    c->width.AsIs           ();
    c->right.SameAs         (box,       wxRight,    margin);
    iStarts->GetSpinButton()->SetConstraints(c);

    // # results
    c = new wxLayoutConstraints();
    c->top.SameAs           (tStarts,   wxBottom,   margin);
    c->height.SameAs        (iResults->GetTextCtrl(), wxHeight);
    c->left.SameAs          (box,       wxLeft,     margin);
    c->width.PercentOf      (box, wxWidth, textProportion);
    tResults->SetConstraints(c);
    c = new wxLayoutConstraints();
    c->top.SameAs           (tStarts,   wxBottom,   margin);
    c->height.AsIs          ();
    c->left.RightOf         (tLoops,                margin);
    c->right.LeftOf         (iResults->GetSpinButton(), iResults->GetSpaceBetweenControls());
    iResults->GetTextCtrl()->SetConstraints(c);
    c = new wxLayoutConstraints();
    c->top.SameAs           (tStarts,   wxBottom,   margin);
    c->height.SameAs        (iResults->GetTextCtrl(), wxHeight);
    c->width.AsIs           ();
    c->right.SameAs         (box,       wxRight,    margin);
    iResults->GetSpinButton()->SetConstraints(c);

    // merge?
    c = new wxLayoutConstraints();
    c->top.SameAs           (tResults,  wxBottom,   margin);
    c->height.SameAs        (bMerge,    wxHeight);
    c->left.SameAs          (box,       wxLeft,     margin);
    c->width.PercentOf      (box,       wxWidth,    textProportion);
    tMerge->SetConstraints(c);
    c = new wxLayoutConstraints();
    c->top.SameAs           (tResults,  wxBottom,   margin);
    c->height.SameAs        (tResults,  wxHeight);
    c->centreX.SameAs       (iResults->GetTextCtrl(), wxCentreX);
    c->width.AsIs           ();
    bMerge->SetConstraints(c);

    box->SetAutoLayout(true);

    // then layout box and bottom buttons
    c = new wxLayoutConstraints();
    c->top.SameAs           (this,      wxTop,      margin);
    c->bottom.Above         (bOK,                   -margin); // wx bug requires -
    c->right.SameAs         (this,      wxRight,    margin);
    c->left.SameAs          (this,      wxLeft,     margin);
    box->SetConstraints(c);

    // OK button
    c = new wxLayoutConstraints();
    c->bottom.SameAs        (this,      wxBottom,   margin);
    c->height.AsIs          ();
    c->right.SameAs         (this,      wxCentreX,  margin);
    c->width.AsIs           ();
    bOK->SetConstraints(c);

    // Cancel button
    c = new wxLayoutConstraints();
    c->bottom.SameAs        (this,      wxBottom,   margin);
    c->height.AsIs          ();
    c->left.SameAs          (this,      wxCentreX,  margin);
    c->width.AsIs           ();
    bCancel->SetConstraints(c);

    SetClientSize(
        maxTextWidth + 5*margin + 100,
        boxClientHeight + bOK->GetSize().GetHeight() + 3*margin);
    SetAutoLayout(true);
}

bool ThreaderOptionsDialog::Activate(void)
{
    dialogActive = true;
    Show(true);
    MakeModal(true);

    // enter the modal loop  (this code snippet borrowed from src/msw/dialog.cpp)
    while (dialogActive)
    {
#if wxUSE_THREADS
        wxMutexGuiLeaveOrEnter();
#endif // wxUSE_THREADS
        while (!wxTheApp->Pending() && wxTheApp->ProcessIdle()) ;
        // a message came or no more idle processing to do
        wxTheApp->DoMessage();
    }

    MakeModal(false);
    return returnOK;
}

bool ThreaderOptionsDialog::GetValues(ThreaderOptions *options)
{
    options->mergeAfterEachSequence = bMerge->GetValue();
    return (
        fpWeight->GetDouble(&options->weightPSSM) &&
        fpLoops->GetDouble(&options->loopLengthMultiplier) &&
        iStarts->GetInteger(&options->nRandomStarts) &&
        iResults->GetInteger(&options->nResultAlignments)
    );
}

void ThreaderOptionsDialog::EndEventLoop(void)
{
    dialogActive = false;
}

void ThreaderOptionsDialog::OnCloseWindow(wxCommandEvent& event)
{
    returnOK = false;
    EndEventLoop();
}

void ThreaderOptionsDialog::OnButton(wxCommandEvent& event)
{
    if (event.GetEventObject() == bOK) {
        returnOK = true;
        ThreaderOptions dummy;
        if (GetValues(&dummy))  // can't succesfully quit if values aren't valid
            EndEventLoop();
        else
            wxBell();
    } else if (event.GetEventObject() == bCancel) {
        returnOK = false;
        EndEventLoop();
    } else {
        event.Skip();
    }
}

END_SCOPE(Cn3D)
