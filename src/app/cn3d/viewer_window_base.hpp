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
        MID_SHOW_TITLES,
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

    void SyncStructures(void) { Command(MID_SYNC_STRUCS); }
    bool AlwaysSyncStructures(void) const { return menuBar->IsChecked(MID_SYNC_STRUCS_ON); }
};

END_SCOPE(Cn3D)

#endif // CN3D_VIEWER_WINDOW_BASE__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.30  2005/04/22 13:43:01  thiessen
* add block highlighting and structure alignment based on highlighted positions only
*
* Revision 1.29  2004/09/27 21:40:46  thiessen
* add highlight cache
*
* Revision 1.28  2004/09/27 18:31:20  thiessen
* continually track GV's in message log when on
*
* Revision 1.27  2004/06/23 20:34:53  thiessen
* sho geometry violations in alignments added to import window
*
* Revision 1.26  2004/02/19 17:05:22  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.25  2003/02/03 19:20:09  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.24  2002/10/13 22:58:08  thiessen
* add redo ability to editor
*
* Revision 1.23  2002/10/07 13:29:32  thiessen
* add double-click -> show row to taxonomy tree
*
* Revision 1.22  2002/09/09 13:38:23  thiessen
* separate save and save-as
*
* Revision 1.21  2002/08/15 22:13:19  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.20  2002/06/05 15:59:38  thiessen
* fix for Solaris
*
* Revision 1.19  2002/06/05 14:28:43  thiessen
* reorganize handling of window titles
*
* Revision 1.18  2002/05/17 19:10:27  thiessen
* preliminary range restriction for BLAST/PSSM
*
* Revision 1.17  2002/02/05 18:53:26  thiessen
* scroll to residue in sequence windows when selected in structure window
*
* Revision 1.16  2001/12/06 23:13:47  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.15  2001/08/14 17:17:48  thiessen
* add user font selection, store in registry
*
* Revision 1.14  2001/07/23 20:08:38  thiessen
* add regex pattern search
*
* Revision 1.13  2001/06/04 14:33:55  thiessen
* add proximity sort; highlight sequence on browser launch
*
* Revision 1.12  2001/05/25 19:08:14  thiessen
* fix GTK window redraw bug
*
* Revision 1.11  2001/05/23 17:43:29  thiessen
* change dialog implementation to wxDesigner; interface changes
*
* Revision 1.10  2001/04/12 18:35:01  thiessen
* fix merge GUI bug/typo
*
* Revision 1.9  2001/04/05 22:54:52  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.8  2001/04/04 00:27:22  thiessen
* major update - add merging, threader GUI controls
*
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
*/
