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
* Revision 1.7  2001/03/30 03:07:09  thiessen
* add threader score calculation & sorting
*
* Revision 1.6  2001/03/22 00:32:37  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.5  2001/03/17 14:06:53  thiessen
* more workarounds for namespace/#define conflicts
*
* Revision 1.4  2001/03/13 01:24:17  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.3  2001/03/09 15:48:44  thiessen
* major changes to add initial update viewer
*
* Revision 1.2  2001/03/02 03:26:36  thiessen
* fix dangling pointer upon app close
*
* Revision 1.1  2001/03/01 20:15:30  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
\* ===========================================================================
*/

#ifndef CN3D_VIEWER_WINDOW_BASE__HPP
#define CN3D_VIEWER_WINDOW_BASE__HPP

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#if defined(__WXMSW__)
#include <wx/msw/winundef.h>
#endif

#include <wx/wx.h>

#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/sequence_viewer_widget.hpp"


BEGIN_SCOPE(Cn3D)

class ViewerBase;
class SequenceDisplay;

// this must be included in the event table for the derived class, so that the base class's
// menu item callbacks are accessible
#define INCLUDE_VIEWER_WINDOW_BASE_EVENTS \
    EVT_MENU_RANGE(MID_SHOW_TITLES, MID_HIDE_TITLES,    ViewerWindowBase::OnTitleView) \
    EVT_MENU_RANGE(MID_ENABLE_EDIT, MID_SYNC_STRUCS_ON, ViewerWindowBase::OnEditMenu) \
    EVT_MENU_RANGE(MID_SELECT_RECT, MID_DRAG_HORIZ,     ViewerWindowBase::OnMouseMode) \
    EVT_MENU_RANGE(MID_LEFT,        MID_SPLIT,          ViewerWindowBase::OnJustification)

class ViewerWindowBase : public wxFrame
{
    friend class SequenceDisplay;

public:

    // displays a new alignment, and whether to enable the editor and column selection
    void NewDisplay(SequenceDisplay *display, bool enableEditor, bool enableSelectByColumn);

    // updates alignment (e.g. if width or # rows has changed); doesn't change scroll
    void UpdateDisplay(SequenceDisplay *display);

    // scroll to specific column
    void ScrollToColumn(int column) { viewerWidget->ScrollTo(column, -1); }

    // communicates to the derived class that the user wants to turn on/off the editor;
    // should return true if derived class wants to allow the state change
    virtual bool RequestEditorEnable(bool enable) { return false; }
    virtual void EnableDerivedEditorMenuItems(bool enable) { }

    // allows the derived class to set up special mouse/cursor modes, e.g. for delete row
    virtual void CancelDerivedSpecialModes(void) { }

    // menu callbacks
    void OnTitleView(wxCommandEvent& event);
    void OnEditMenu(wxCommandEvent& event);
    void OnMouseMode(wxCommandEvent& event);
    void OnJustification(wxCommandEvent& event);

    // menu identifiers
    enum {
        // view menu
        MID_SHOW_TITLES,
        MID_HIDE_TITLES,

        // edit menu
        MID_ENABLE_EDIT,
        MID_UNDO,
        MID_SPLIT_BLOCK,
        MID_MERGE_BLOCKS,
        MID_CREATE_BLOCK,
        MID_DELETE_BLOCK,
        MID_SYNC_STRUCS,
        MID_SYNC_STRUCS_ON,

        // mouse mode
        MID_SELECT_RECT,
        MID_SELECT_COLS,
        MID_SELECT_ROWS,
        MID_DRAG_HORIZ,

        // unaligned justification
        MID_LEFT,
        MID_RIGHT,
        MID_CENTER,
        MID_SPLIT,

        // so derived classes can use non-conflicting MID's
        START_VIEWER_WINDOW_DERIVED_MID
    };

protected:

    // can't instantiate base class
    ViewerWindowBase(ViewerBase *parentViewer, const char* title,
        const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
    virtual ~ViewerWindowBase(void);

    SequenceViewerWidget *viewerWidget;
    ViewerBase *viewer;

    void EnableBaseEditorMenuItems(bool enabled);

    // so derived classes can add menu stuff
    wxMenuBar *menuBar;
    wxMenu *viewMenu, *editMenu, *mouseModeMenu, *justificationMenu;

    SequenceViewerWidget::eMouseMode prevMouseMode;

    BlockMultipleAlignment::eUnalignedJustification currentJustification;

    void SplitBlockOff(void)
    {
        menuBar->Check(MID_SPLIT_BLOCK, false);
        SetCursor(wxNullCursor);
    }
    void MergeBlocksOff(void)
    {
        menuBar->Check(MID_MERGE_BLOCKS, false);
        viewerWidget->SetMouseMode(prevMouseMode);
        SetCursor(wxNullCursor);
    }
    void CreateBlockOff(void)
    {
        menuBar->Check(MID_CREATE_BLOCK, false);
        viewerWidget->SetMouseMode(prevMouseMode);
        SetCursor(wxNullCursor);
    }
    void DeleteBlockOff(void)
    {
        menuBar->Check(MID_DELETE_BLOCK, false);
        SetCursor(wxNullCursor);
    }

    void CancelBaseSpecialModes(void)
    {
        if (DoSplitBlock()) SplitBlockOff();
        if (DoMergeBlocks()) MergeBlocksOff();
        if (DoCreateBlock()) CreateBlockOff();
        if (DoDeleteBlock()) DeleteBlockOff();
    }

public:

    BlockMultipleAlignment::eUnalignedJustification GetCurrentJustification(void) const
        { return currentJustification; }

    void Refresh(void) { viewerWidget->Refresh(false); }
    void KillWindow(void)
    {
        viewer = NULL;
        Destroy();
    }

    void EnableUndo(bool enabled) { menuBar->Enable(MID_UNDO, enabled); }

    bool DoSplitBlock(void) const { return menuBar->IsChecked(MID_SPLIT_BLOCK); }
    bool DoMergeBlocks(void) const { return menuBar->IsChecked(MID_MERGE_BLOCKS); }
    bool DoCreateBlock(void) const { return menuBar->IsChecked(MID_CREATE_BLOCK); }
    bool DoDeleteBlock(void) const { return menuBar->IsChecked(MID_DELETE_BLOCK); }

    void SyncStructures(void) { Command(MID_SYNC_STRUCS); }
    bool AlwaysSyncStructures(void) const { return menuBar->IsChecked(MID_SYNC_STRUCS_ON); }
};

END_SCOPE(Cn3D)

#endif // CN3D_VIEWER_WINDOW_BASE__HPP
