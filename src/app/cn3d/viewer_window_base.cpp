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
*      base GUI functionality for viewers
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2001/03/02 03:26:59  thiessen
* fix dangling pointer upon app close
*
* Revision 1.1  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* ===========================================================================
*/

#include "cn3d/viewer_window_base.hpp"
#include "cn3d/viewer_base.hpp"
#include "cn3d/sequence_display.hpp"
#include "cn3d/messenger.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

ViewerWindowBase::ViewerWindowBase(ViewerBase *parentViewer) :
    wxFrame(NULL, -1, "Cn3D++ Sequence Viewer", wxPoint(0,500), wxSize(1000,200)),
    viewerWidget(NULL), viewer(parentViewer)
{
    if (!parentViewer) ERR_POST(Error << "ViewerWindowBase::ViewerWindowBase() - got NULL pointer");

    SetSizeHints(200, 150);

    // status bar with a single field
    CreateStatusBar(2);
    int widths[2] = { 150, -1 };
    SetStatusWidths(2, widths);

    viewerWidget = new SequenceViewerWidget(this);

    menuBar = new wxMenuBar;
    viewMenu = new wxMenu;
    viewMenu->Append(MID_SHOW_TITLES, "&Show Titles");
    //menu->Append(MID_HIDE_TITLES, "&Hide Titles");
    menuBar->Append(viewMenu, "&View");

    editMenu = new wxMenu;
    editMenu->Append(MID_ENABLE_EDIT, "&Enable Editor", "", true);
    editMenu->Append(MID_UNDO, "&Undo\tCtrl-Z");
    editMenu->AppendSeparator();
    editMenu->Append(MID_SPLIT_BLOCK, "&Split Block", "", true);
    editMenu->Append(MID_MERGE_BLOCKS, "&Merge Blocks", "", true);
    editMenu->Append(MID_CREATE_BLOCK, "&Create Block", "", true);
    editMenu->Append(MID_DELETE_BLOCK, "&Delete Block", "", true);
    editMenu->AppendSeparator();
    editMenu->Append(MID_SYNC_STRUCS, "Sync Structure &Colors");
    editMenu->Append(MID_SYNC_STRUCS_ON, "&Always Sync Structure Colors", "", true);
    menuBar->Append(editMenu, "&Edit");

    mouseModeMenu = new wxMenu;
    mouseModeMenu->Append(MID_SELECT_RECT, "&Select Rectangle", "", true);
    mouseModeMenu->Append(MID_SELECT_COLS, "Select &Columns", "", true);
    mouseModeMenu->Append(MID_SELECT_ROWS, "Select &Rows", "", true);
    mouseModeMenu->Append(MID_DRAG_HORIZ, "&Horizontal Drag", "", true);
    menuBar->Append(mouseModeMenu, "&Mouse Mode");

    justificationMenu = new wxMenu;
    justificationMenu->Append(MID_LEFT, "&Left", "", true);
    justificationMenu->Append(MID_RIGHT, "&Right", "", true);
    justificationMenu->Append(MID_CENTER, "&Center", "", true);
    justificationMenu->Append(MID_SPLIT, "&Split", "", true);
    menuBar->Append(justificationMenu, "Unaligned &Justification");

    // set default initial modes
    viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectRectangle);
    menuBar->Check(MID_SELECT_RECT, true);
    menuBar->Check(MID_SPLIT, true);
    currentJustification = BlockMultipleAlignment::eSplit;
    viewerWidget->TitleAreaOn();
    menuBar->Check(MID_SYNC_STRUCS_ON, true);
    EnableEditorMenuItems(false);

    SetMenuBar(menuBar);
}

ViewerWindowBase::~ViewerWindowBase(void)
{
    if (viewer) viewer->GUIDestroyed();
}

void ViewerWindowBase::EnableEditorMenuItems(bool enabled)
{
    int i;
    for (i=MID_SPLIT_BLOCK; i<=MID_SYNC_STRUCS_ON; i++)
        menuBar->Enable(i, enabled);
    menuBar->Enable(MID_DRAG_HORIZ, enabled);
    if (!enabled) {
        if (DoSplitBlock()) SplitBlockOff();
        if (DoMergeBlocks()) MergeBlocksOff();
        if (DoCreateBlock()) CreateBlockOff();
        if (DoDeleteBlock()) DeleteBlockOff();
    }
    menuBar->Enable(MID_UNDO, false);
    EnableDerivedEditorMenuItems(enabled);
}

void ViewerWindowBase::NewDisplay(SequenceDisplay *display)
{
    viewerWidget->AttachAlignment(display);
    if (display->IsEditable()) {
        menuBar->Enable(MID_ENABLE_EDIT, true);
        menuBar->EnableTop(1, true);    // edit menu
        menuBar->EnableTop(3, true);    // justification menu
    } else {
        menuBar->Enable(MID_ENABLE_EDIT, false);
        menuBar->EnableTop(1, false);
        menuBar->EnableTop(3, false);
    }
    EnableEditorMenuItems(false); // start with editor off
}

void ViewerWindowBase::UpdateDisplay(SequenceDisplay *display)
{
    int vsX, vsY;   // to preserve scroll position
    viewerWidget->GetScroll(&vsX, &vsY);
    viewerWidget->AttachAlignment(display, vsX, vsY);
    GlobalMessenger()->PostRedrawSequenceViewer(viewer);
}

void ViewerWindowBase::OnTitleView(wxCommandEvent& event)
{
    TESTMSG("in OnTitleView()");
    switch (event.GetId()) {
        case MID_SHOW_TITLES:
            viewerWidget->TitleAreaOn(); break;
        case MID_HIDE_TITLES:
            viewerWidget->TitleAreaOff(); break;
    }
}

void ViewerWindowBase::OnEditMenu(wxCommandEvent& event)
{
    bool turnEditorOn = menuBar->IsChecked(MID_ENABLE_EDIT);

    switch (event.GetId()) {

        case MID_ENABLE_EDIT:
            if (turnEditorOn) {
                if (!RequestEditorEnable(true)) {
                    menuBar->Check(MID_ENABLE_EDIT, false);
                    break;
                }
                TESTMSG("turning on editor");
                viewer->PushAlignment();    // keep copy of original at bottom of the stack
                viewer->GetCurrentDisplay()->AddBlockBoundaryRow();
                Command(MID_DRAG_HORIZ);    // switch to drag mode
            } else {
                if (!RequestEditorEnable(false)) {   // cancelled
                    menuBar->Check(MID_ENABLE_EDIT, true);
                    break;
                }
                TESTMSG("turning off editor");
                viewer->GetCurrentDisplay()->RemoveBlockBoundaryRow();
                if (!menuBar->IsChecked(MID_SELECT_COLS) || !menuBar->IsChecked(MID_SELECT_ROWS))
                    Command(MID_SELECT_RECT);
            }
            EnableEditorMenuItems(turnEditorOn);
            break;

        case MID_UNDO:
            TESTMSG("undoing...");
            viewer->PopAlignment();
            if (AlwaysSyncStructures()) SyncStructures();
            break;

        case MID_SPLIT_BLOCK:
            if (DoCreateBlock()) CreateBlockOff();
            if (DoMergeBlocks()) MergeBlocksOff();
            if (DoDeleteBlock()) DeleteBlockOff();
            if (DoDeleteRow()) DeleteRowOff();
            if (DoSplitBlock())
                SetCursor(*wxCROSS_CURSOR);
            else
                SplitBlockOff();
            break;

        case MID_MERGE_BLOCKS:
            if (DoSplitBlock()) SplitBlockOff();
            if (DoCreateBlock()) CreateBlockOff();
            if (DoDeleteBlock()) DeleteBlockOff();
            if (DoDeleteRow()) DeleteRowOff();
            if (DoMergeBlocks()) {
                SetCursor(*wxCROSS_CURSOR);
                prevMouseMode = viewerWidget->GetMouseMode();
                viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectColumns);
            } else
                MergeBlocksOff();
            break;

        case MID_CREATE_BLOCK:
            if (DoSplitBlock()) SplitBlockOff();
            if (DoMergeBlocks()) MergeBlocksOff();
            if (DoDeleteBlock()) DeleteBlockOff();
            if (DoDeleteRow()) DeleteRowOff();
            if (DoCreateBlock()) {
                SetCursor(*wxCROSS_CURSOR);
                prevMouseMode = viewerWidget->GetMouseMode();
                viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectColumns);
            } else
                CreateBlockOff();
            break;

        case MID_DELETE_BLOCK:
            if (DoSplitBlock()) SplitBlockOff();
            if (DoMergeBlocks()) MergeBlocksOff();
            if (DoCreateBlock()) CreateBlockOff();
            if (DoDeleteRow()) DeleteRowOff();
            if (DoDeleteBlock())
                SetCursor(*wxCROSS_CURSOR);
            else
                DeleteBlockOff();
            break;

        case MID_SYNC_STRUCS:
            viewer->GetCurrentDisplay()->RedrawAlignedMolecules();
            break;

        // for implementational reasons, this is left as part of the base class; but the 'delete row'
        // menu item isn't included in the edit menu in the base class
        case MID_DELETE_ROW:
            if (DoSplitBlock()) SplitBlockOff();
            if (DoMergeBlocks()) MergeBlocksOff();
            if (DoCreateBlock()) CreateBlockOff();
            if (DoDeleteBlock()) DeleteBlockOff();
            if (DoDeleteRow())
                SetCursor(*wxCROSS_CURSOR);
            else
                DeleteRowOff();
            break;
    }
}

void ViewerWindowBase::OnMouseMode(wxCommandEvent& event)
{
    const wxMenuItemList& items = mouseModeMenu->GetMenuItems();
    for (int i=0; i<items.GetCount(); i++)
        items.Item(i)->GetData()->Check((items.Item(i)->GetData()->GetId() == event.GetId()) ? true : false);

    switch (event.GetId()) {
        case MID_SELECT_RECT:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectRectangle); break;
        case MID_SELECT_COLS:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectColumns); break;
        case MID_SELECT_ROWS:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectRows); break;
        case MID_DRAG_HORIZ:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eDragHorizontal); break;
    }
}

void ViewerWindowBase::OnJustification(wxCommandEvent& event)
{
    for (int i=MID_LEFT; i<=MID_SPLIT; i++)
        menuBar->Check(i, (i == event.GetId()) ? true : false);

    switch (event.GetId()) {
        case MID_LEFT:
            currentJustification = BlockMultipleAlignment::eLeft; break;
        case MID_RIGHT:
            currentJustification = BlockMultipleAlignment::eRight; break;
        case MID_CENTER:
            currentJustification = BlockMultipleAlignment::eCenter; break;
        case MID_SPLIT:
            currentJustification = BlockMultipleAlignment::eSplit; break;
    }
    GlobalMessenger()->PostRedrawSequenceViewer(viewer);
}

END_SCOPE(Cn3D)

