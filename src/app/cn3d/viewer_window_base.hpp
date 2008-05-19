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
* ===========================================================================
*/

#ifndef CN3D_VIEWER_WINDOW_BASE__HPP
#define CN3D_VIEWER_WINDOW_BASE__HPP

#include <corelib/ncbistd.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>

#include "block_multiple_alignment.hpp"
#include "sequence_viewer_widget.hpp"


BEGIN_SCOPE(Cn3D)

class ViewerBase;
class SequenceDisplay;
class MoleculeIdentifier;

// this must be included in the event table for the derived class, so that the base class's
// menu item callbacks are accessible
#define INCLUDE_VIEWER_WINDOW_BASE_EVENTS \
    EVT_MENU_RANGE(MID_SHOW_TITLES, MID_HIDE_TITLES,    ViewerWindowBase::OnTitleView) \
    EVT_MENU_RANGE(MID_ENABLE_EDIT, MID_SYNC_STRUCS_ON, ViewerWindowBase::OnEditMenu) \
    EVT_MENU_RANGE(MID_SELECT_RECT, MID_DRAG_HORIZ,     ViewerWindowBase::OnMouseMode) \
    EVT_MENU_RANGE(MID_LEFT,        MID_SPLIT,          ViewerWindowBase::OnJustification) \
    EVT_MENU      (MID_SHOW_GEOM_VLTNS,                 ViewerWindowBase::OnShowGeomVltns) \
    EVT_MENU      (MID_FIND_PATTERN,                    ViewerWindowBase::OnFindPattern) \
    EVT_MENU_RANGE(MID_CACHE_HIGHLIGHTS, MID_RESTORE_CACHED_HIGHLIGHTS, ViewerWindowBase::OnCacheHighlights)

class ViewerWindowBase : public wxFrame
{
    friend class SequenceDisplay;

public:

    // displays a new alignment, and whether to enable the editor and column selection
    void NewDisplay(SequenceDisplay *display, bool enableSelectByColumn);

    // if 'prompt', ask if user wants to save edits; return value indicates whether program
    // should continue after this dialog - i.e., returns false if user hits 'cancel';
    // program should then abort the operation that engendered this function call.
    // 'canCancel' tells whether or not a 'cancel' button is even displayed - thus
    // if 'canCancel' is false, the function will always return true.
    virtual bool SaveDialog(bool prompt, bool canCancel) = 0;

    // updates alignment (e.g. if width or # rows has changed); doesn't change scroll
    void UpdateDisplay(SequenceDisplay *display);

    // scroll to specific column
    void ScrollToColumn(int column) { viewerWidget->ScrollTo(column, -1); }
    void ScrollToRow(int row) { viewerWidget->ScrollTo(-1, row); }

    // scroll so that this cell is visible, if it's not already
    void MakeCellVisible(int column, int row) { viewerWidget->MakeCharacterVisible(column, row); }
    void MakeSequenceVisible(const MoleculeIdentifier *identifier);

    // set the font for the characters from registry values; refreshes automatically.
    void SetupFontFromRegistry(void);

    // communicates to the derived class that the user wants to turn on/off the editor;
    // should return true if derived class wants to allow the state change
    virtual bool RequestEditorEnable(bool enable) { return false; }
    virtual void EnableDerivedEditorMenuItems(bool enable) { }

    // allows the derived class to set up special mouse/cursor modes, e.g. for delete row
    virtual void CancelDerivedSpecialModesExcept(int id) { }

    // override to set customized window title
    virtual void SetWindowTitle(void) = 0;

    // are geometry violations on?
    bool GeometryViolationsShown(void) const { return menuBar->IsChecked(MID_SHOW_GEOM_VLTNS); }
    void UpdateGeometryViolations(void) const;

    // menu callbacks
    void OnTitleView(wxCommandEvent& event);
    void OnEditMenu(wxCommandEvent& event);
    void OnMouseMode(wxCommandEvent& event);
    void OnJustification(wxCommandEvent& event);
    void OnShowGeomVltns(wxCommandEvent& event);
    void OnFindPattern(wxCommandEvent& event);
    void OnCacheHighlights(wxCommandEvent& event);

protected:

    // menu identifiers
    enum {
        // view menu
#ifdef __WXMAC__
        MID_SHOW_TITLES = 1,   //  avoid debug warning when have menuitem ID = 0
#else
        MID_SHOW_TITLES,
#endif
        MID_HIDE_TITLES,
        MID_SHOW_GEOM_VLTNS,
        MID_FIND_PATTERN,
        MID_CACHE_HIGHLIGHTS,
        MID_RESTORE_CACHED_HIGHLIGHTS,

        // edit menu
        MID_ENABLE_EDIT,
        MID_UNDO,
        MID_REDO,
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
        MID_SELECT_BLOCKS,
        MID_DRAG_HORIZ,

        // unaligned justification
        MID_LEFT,
        MID_RIGHT,
        MID_CENTER,
        MID_SPLIT,

        // so derived classes can use non-conflicting MID's
        START_VIEWER_WINDOW_DERIVED_MID
    };

    // can't instantiate base class
    ViewerWindowBase(ViewerBase *parentViewer,
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

    void CancelBaseSpecialModesExcept(int id)
    {
        if (id != MID_SPLIT_BLOCK && DoSplitBlock()) SplitBlockOff();
        if (id != MID_MERGE_BLOCKS && DoMergeBlocks()) MergeBlocksOff();
        if (id != MID_CREATE_BLOCK && DoCreateBlock()) CreateBlockOff();
        if (id != MID_DELETE_BLOCK && DoDeleteBlock()) DeleteBlockOff();
    }

    void CancelAllSpecialModesExcept(int id)
    {
        CancelBaseSpecialModesExcept(id);
        CancelDerivedSpecialModesExcept(id);
    }

    virtual SequenceViewerWidget::eMouseMode GetMouseModeForCreateAndMerge(void) = 0;

public:

    BlockMultipleAlignment::eUnalignedJustification GetCurrentJustification(void) const
        { return currentJustification; }

    void Refresh(void) { viewerWidget->Refresh(false); }
    void KillWindow(void)
    {
        viewer = NULL;
        viewerWidget->AttachAlignment(NULL);
        Destroy();
    }

    bool EditorIsOn(void) const { return menuBar->IsChecked(MID_ENABLE_EDIT); }
    void EnableUndo(bool enabled) { menuBar->Enable(MID_UNDO, enabled); }
    void EnableRedo(bool enabled) { menuBar->Enable(MID_REDO, enabled); }

    bool DoSplitBlock(void) const { return menuBar->IsChecked(MID_SPLIT_BLOCK); }
    bool DoMergeBlocks(void) const { return menuBar->IsChecked(MID_MERGE_BLOCKS); }
    bool DoCreateBlock(void) const { return menuBar->IsChecked(MID_CREATE_BLOCK); }
    bool DoDeleteBlock(void) const { return menuBar->IsChecked(MID_DELETE_BLOCK); }

    bool SelectBlocksIsOn(void) const { return (viewerWidget->GetMouseMode() == SequenceViewerWidget::eSelectBlocks); }

    void SyncStructures(void) { ProcessCommand(MID_SYNC_STRUCS); }
    bool AlwaysSyncStructures(void) const { return menuBar->IsChecked(MID_SYNC_STRUCS_ON); }
};

END_SCOPE(Cn3D)

#endif // CN3D_VIEWER_WINDOW_BASE__HPP
