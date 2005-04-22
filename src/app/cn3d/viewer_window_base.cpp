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

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <memory>

#include "viewer_window_base.hpp"
#include "viewer_base.hpp"
#include "sequence_display.hpp"
#include "messenger.hpp"
#include "cn3d_threader.hpp"
#include "alignment_manager.hpp"
#include "cn3d_tools.hpp"
#include "pattern_dialog.hpp"

// the application icon (under Windows it is in resources)
#if defined(__WXGTK__) || defined(__WXMAC__)
    #include "cn3d.xpm"
#endif

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

ViewerWindowBase::ViewerWindowBase(ViewerBase *parentViewer, const wxPoint& pos, const wxSize& size) :
    wxFrame(GlobalTopWindow(), wxID_HIGHEST + 10, "", pos, size,
        wxDEFAULT_FRAME_STYLE
#if defined(__WXMSW__)
            | wxFRAME_TOOL_WINDOW
            | wxFRAME_NO_TASKBAR
            | wxFRAME_FLOAT_ON_PARENT
#endif
        ),
    viewerWidget(NULL), viewer(parentViewer)
{
    if (!parentViewer) ERRORMSG("ViewerWindowBase::ViewerWindowBase() - got NULL pointer");

    SetSizeHints(200, 150);
    SetIcon(wxICON(cn3d));

    // status bar with two fields - first is for id/loc, second is for general status line
    CreateStatusBar(2);
    int widths[2] = { 175, -1 };
    SetStatusWidths(2, widths);

    viewerWidget = new SequenceViewerWidget(this);
    SetupFontFromRegistry();

    menuBar = new wxMenuBar;
    viewMenu = new wxMenu;
    viewMenu->Append(MID_SHOW_TITLES, "Show &Titles");
    //menu->Append(MID_HIDE_TITLES, "&Hide Titles");
    viewMenu->Append(MID_SHOW_GEOM_VLTNS, "Show &Geometry Violations", "", true);
    viewMenu->Append(MID_FIND_PATTERN, "Find &Pattern");
    viewMenu->AppendSeparator();
    viewMenu->Append(MID_CACHE_HIGHLIGHTS, "&Cache Highlights");
    viewMenu->Append(MID_RESTORE_CACHED_HIGHLIGHTS, "Rest&ore Cached Highlights");
    menuBar->Append(viewMenu, "&View");

    editMenu = new wxMenu;
    editMenu->Append(MID_ENABLE_EDIT, "&Enable Editor", "", true);
    editMenu->Append(MID_UNDO, "Undo\tCtrl-Z");
#ifndef __WXMAC__
    editMenu->Append(MID_REDO, "Redo\tShift-Ctrl-Z");
#else
    // mac commands apparently don't recognize shift?
    editMenu->Append(MID_REDO, "Redo");
#endif
    editMenu->AppendSeparator();
    editMenu->Append(MID_SPLIT_BLOCK, "&Split Block", "", true);
    editMenu->Append(MID_MERGE_BLOCKS, "&Merge Blocks", "", true);
    editMenu->Append(MID_CREATE_BLOCK, "&Create Block", "", true);
    editMenu->Append(MID_DELETE_BLOCK, "&Delete Block", "", true);
    editMenu->AppendSeparator();
    editMenu->Append(MID_SYNC_STRUCS, "Sy&nc Structure Colors");
    editMenu->Append(MID_SYNC_STRUCS_ON, "&Always Sync Structure Colors", "", true);
    menuBar->Append(editMenu, "&Edit");

    mouseModeMenu = new wxMenu;
    mouseModeMenu->Append(MID_SELECT_RECT, "&Select Rectangle", "", true);
    mouseModeMenu->Append(MID_SELECT_COLS, "Select &Columns", "", true);
    mouseModeMenu->Append(MID_SELECT_ROWS, "Select &Rows", "", true);
    mouseModeMenu->Append(MID_SELECT_BLOCKS, "Select &Blocks", "", true);
    mouseModeMenu->Append(MID_DRAG_HORIZ, "&Horizontal Drag", "", true);
    menuBar->Append(mouseModeMenu, "&Mouse Mode");

    // accelerators for special mouse mode keys
    wxAcceleratorEntry entries[5];
    entries[0].Set(wxACCEL_NORMAL, 's', MID_SELECT_RECT);
    entries[1].Set(wxACCEL_NORMAL, 'c', MID_SELECT_COLS);
    entries[2].Set(wxACCEL_NORMAL, 'r', MID_SELECT_ROWS);
    entries[3].Set(wxACCEL_NORMAL, 'b', MID_SELECT_BLOCKS);
    entries[4].Set(wxACCEL_NORMAL, 'h', MID_DRAG_HORIZ);
    wxAcceleratorTable accel(5, entries);
    SetAcceleratorTable(accel);

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
    EnableBaseEditorMenuItems(false);
    menuBar->Check(MID_SHOW_GEOM_VLTNS, false);   // start with gv's off
}

ViewerWindowBase::~ViewerWindowBase(void)
{
    if (viewer) viewer->GUIDestroyed();
}

void ViewerWindowBase::SetupFontFromRegistry(void)
{
    // get font info from registry, and create wxFont
    string nativeFont;
    RegistryGetString(REG_SEQUENCE_FONT_SECTION, REG_FONT_NATIVE_FONT_INFO, &nativeFont);
    auto_ptr<wxFont> font(wxFont::New(wxString(nativeFont.c_str())));
    if (!font.get() || !font->Ok())
    {
        ERRORMSG("ViewerWindowBase::SetupFontFromRegistry() - error setting up font");
        return;
    }
    viewerWidget->SetCharacterFont(font.release());
}

void ViewerWindowBase::EnableBaseEditorMenuItems(bool enabled)
{
    int i;
    for (i=MID_SPLIT_BLOCK; i<=MID_SYNC_STRUCS_ON; ++i)
        menuBar->Enable(i, enabled);
    menuBar->Enable(MID_DRAG_HORIZ, enabled);
    if (!enabled) CancelBaseSpecialModesExcept(-1);
    menuBar->Enable(MID_UNDO, false);
    menuBar->Enable(MID_REDO, false);
    menuBar->Enable(MID_SHOW_GEOM_VLTNS,
        viewer->GetCurrentDisplay() && viewer->GetCurrentDisplay()->IsEditable());
    EnableDerivedEditorMenuItems(enabled);
}

void ViewerWindowBase::NewDisplay(SequenceDisplay *display, bool enableSelectByColumn)
{
    viewerWidget->AttachAlignment(display);
    menuBar->EnableTop(menuBar->FindMenu("Edit"), display->IsEditable());
    menuBar->EnableTop(menuBar->FindMenu("Unaligned Justification"), display->IsEditable());
    menuBar->Enable(MID_SELECT_COLS, enableSelectByColumn);
    viewer->SetUndoRedoMenuStates();
}

void ViewerWindowBase::UpdateDisplay(SequenceDisplay *display)
{
    int vsX, vsY;   // to preserve scroll position
    viewerWidget->GetScroll(&vsX, &vsY);
    viewerWidget->AttachAlignment(display, vsX, vsY);
    menuBar->EnableTop(menuBar->FindMenu("Edit"), display->IsEditable());
    menuBar->EnableTop(menuBar->FindMenu("Unaligned Justification"), display->IsEditable());
    GlobalMessenger()->PostRedrawAllSequenceViewers();
    viewer->SetUndoRedoMenuStates();
    UpdateGeometryViolations();
}

void ViewerWindowBase::OnTitleView(wxCommandEvent& event)
{
    TRACEMSG("in OnTitleView()");
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
                TRACEMSG("turning on editor");
                if (!RequestEditorEnable(true)) {
                    menuBar->Check(MID_ENABLE_EDIT, false);
                    break;
                }
                EnableBaseEditorMenuItems(true);
                viewer->GetCurrentDisplay()->AddBlockBoundaryRows();    // add before push!
                viewer->EnableStacks();     // start up undo/redo stack system
                Command(MID_DRAG_HORIZ);    // switch to drag mode
            } else {
                TRACEMSG("turning off editor");
                if (!RequestEditorEnable(false)) {   // cancelled
                    menuBar->Check(MID_ENABLE_EDIT, true);
                    break;
                }
                EnableBaseEditorMenuItems(false);
                viewer->GetCurrentDisplay()->RemoveBlockBoundaryRows();
                if (!(menuBar->IsChecked(MID_SELECT_COLS) || menuBar->IsChecked(MID_SELECT_ROWS) ||
                        menuBar->IsChecked(MID_SELECT_BLOCKS)))
                    Command(MID_SELECT_RECT);
            }
            break;

        case MID_UNDO:
            TRACEMSG("undoing...");
            viewer->Undo();
            UpdateDisplay(viewer->GetCurrentDisplay());
            if (AlwaysSyncStructures()) SyncStructures();
            break;

        case MID_REDO:
            TRACEMSG("redoing...");
            viewer->Redo();
            UpdateDisplay(viewer->GetCurrentDisplay());
            if (AlwaysSyncStructures()) SyncStructures();
            break;

        case MID_SPLIT_BLOCK:
            CancelAllSpecialModesExcept(MID_SPLIT_BLOCK);
            if (DoSplitBlock())
                SetCursor(*wxCROSS_CURSOR);
            else
                SplitBlockOff();
            break;

        case MID_MERGE_BLOCKS:
            CancelAllSpecialModesExcept(MID_MERGE_BLOCKS);
            if (DoMergeBlocks()) {
                SetCursor(*wxCROSS_CURSOR);
                prevMouseMode = viewerWidget->GetMouseMode();
                viewerWidget->SetMouseMode(GetMouseModeForCreateAndMerge());
            } else
                MergeBlocksOff();
            break;

        case MID_CREATE_BLOCK:
            CancelAllSpecialModesExcept(MID_CREATE_BLOCK);
            if (DoCreateBlock()) {
                SetCursor(*wxCROSS_CURSOR);
                prevMouseMode = viewerWidget->GetMouseMode();
                viewerWidget->SetMouseMode(GetMouseModeForCreateAndMerge());
            } else
                CreateBlockOff();
            break;

        case MID_DELETE_BLOCK:
            CancelAllSpecialModesExcept(MID_DELETE_BLOCK);
            if (DoDeleteBlock())
                SetCursor(*wxCROSS_CURSOR);
            else
                DeleteBlockOff();
            break;

        case MID_SYNC_STRUCS:
            viewer->GetCurrentDisplay()->RedrawAlignedMolecules();
            break;
    }
}

void ViewerWindowBase::OnMouseMode(wxCommandEvent& event)
{
    const wxMenuItemList& items = mouseModeMenu->GetMenuItems();
    for (int i=0; i<items.GetCount(); ++i)
        items.Item(i)->GetData()->Check(
            (items.Item(i)->GetData()->GetId() == event.GetId()) ? true : false);

    switch (event.GetId()) {
        case MID_SELECT_RECT:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectRectangle); break;
        case MID_SELECT_COLS:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectColumns); break;
        case MID_SELECT_ROWS:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectRows); break;
        case MID_SELECT_BLOCKS:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eSelectBlocks); break;
        case MID_DRAG_HORIZ:
            viewerWidget->SetMouseMode(SequenceViewerWidget::eDragHorizontal); break;
    }
}

void ViewerWindowBase::OnJustification(wxCommandEvent& event)
{
    for (int i=MID_LEFT; i<=MID_SPLIT; ++i)
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

void ViewerWindowBase::OnShowGeomVltns(wxCommandEvent& event)
{
    UpdateGeometryViolations();
    GlobalMessenger()->PostRedrawSequenceViewer(viewer);
}

void ViewerWindowBase::UpdateGeometryViolations(void) const
{
    const ViewerBase::AlignmentList& alignments = viewer->GetCurrentAlignments();
    ViewerBase::AlignmentList::const_iterator a, ae = alignments.end();
    int nViolations = 0;
    for (a=alignments.begin(); a!=ae; ++a)
        nViolations += (*a)->ShowGeometryViolations(GeometryViolationsShown());
    if (GeometryViolationsShown())
        INFOMSG("Found " << nViolations << " geometry violation"
            << ((nViolations == 1) ? "" : "s") << " in this window");
}

void ViewerWindowBase::OnFindPattern(wxCommandEvent& event)
{
    // remember previous pattern
    static wxString previousPattern;
    static wxString previousMode("Replace");

    // get pattern from user
    PatternDialog dialog(this);
    dialog.m_Pattern->SetValue(previousPattern);
    dialog.m_Pattern->SetSelection(-1, -1);
    dialog.m_Mode->SetStringSelection(previousMode);
    int status = dialog.ShowModal();
    wxString pattern = dialog.m_Pattern->GetValue();
    if (status != wxID_OK || pattern.size() == 0)
        return;

    // add trailing period if not present (convenience for the user)
    if (pattern[pattern.size() - 1] != '.') pattern += '.';
    previousPattern = pattern;
    previousMode = dialog.m_Mode->GetStringSelection();

    if (dialog.m_Mode->GetStringSelection() == "Replace")
        GlobalMessenger()->RemoveAllHighlights(true);

    // highlight pattern from each (unique) sequence in the display
    map < const Sequence * , bool > usedSequences;
    const SequenceDisplay *display = viewer->GetCurrentDisplay();

    for (int i=0; i<display->NRows(); ++i) {

        const Sequence *sequence = display->GetSequenceForRow(i);
        if (!sequence || usedSequences.find(sequence) != usedSequences.end()) continue;
        usedSequences[sequence] = true;

        if (!sequence->HighlightPattern(pattern.c_str())) break;
    }
}

void ViewerWindowBase::MakeSequenceVisible(const MoleculeIdentifier *identifier)
{
    const SequenceDisplay *display = viewer->GetCurrentDisplay();
    for (int i=0; i<display->NRows(); ++i) {
        const Sequence *sequence = display->GetSequenceForRow(i);
        if (sequence && sequence->identifier == identifier) {
            viewerWidget->MakeCharacterVisible(-1, i);
            break;
        }
    }
}

void ViewerWindowBase::OnCacheHighlights(wxCommandEvent& event)
{
    if (event.GetId() == MID_CACHE_HIGHLIGHTS)
        GlobalMessenger()->CacheHighlights();
    else if (event.GetId() == MID_RESTORE_CACHED_HIGHLIGHTS)
        GlobalMessenger()->RestoreCachedHighlights();
}

END_SCOPE(Cn3D)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.55  2005/04/22 13:43:01  thiessen
* add block highlighting and structure alignment based on highlighted positions only
*
* Revision 1.54  2004/10/01 15:13:20  thiessen
* use new pattern dialog from DialogBlocks
*
* Revision 1.53  2004/09/27 21:40:46  thiessen
* add highlight cache
*
* Revision 1.52  2004/09/27 18:31:20  thiessen
* continually track GV's in message log when on
*
* Revision 1.51  2004/06/23 20:34:53  thiessen
* sho geometry violations in alignments added to import window
*
* Revision 1.50  2004/05/21 21:41:40  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.49  2004/03/15 18:38:52  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.48  2004/02/19 17:05:22  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.47  2003/11/06 18:52:32  thiessen
* make geometry violations shown on/off; allow multiple pmid entry in ref dialog
*
* Revision 1.46  2003/07/17 16:52:34  thiessen
* add FileSaved message with edit typing
*
* Revision 1.45  2003/02/03 19:20:09  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.44  2003/01/31 17:18:59  thiessen
* many small additions and changes...
*
* Revision 1.43  2003/01/23 20:03:05  thiessen
* add BLAST Neighbor algorithm
*
* Revision 1.42  2002/12/19 14:15:37  thiessen
* mac fixes to menus, add icon
*
* Revision 1.41  2002/11/22 19:54:29  thiessen
* fixes for wxMac/OSX
*
* Revision 1.40  2002/10/18 17:31:10  thiessen
* fix for gcc
*
* Revision 1.39  2002/10/18 17:15:33  thiessen
* use wxNativeEncodingInfo to store fonts in registry
*
* Revision 1.38  2002/10/15 22:04:09  thiessen
* fix geom vltns bug
*
* Revision 1.37  2002/10/13 22:58:08  thiessen
* add redo ability to editor
*
* Revision 1.36  2002/10/07 13:29:32  thiessen
* add double-click -> show row to taxonomy tree
*
* Revision 1.35  2002/09/06 13:06:31  thiessen
* fix menu accelerator conflicts
*
* Revision 1.34  2002/08/15 22:13:18  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.33  2002/06/13 14:54:07  thiessen
* add sort by self-hit
*
* Revision 1.32  2002/06/05 14:28:42  thiessen
* reorganize handling of window titles
*
* Revision 1.31  2002/05/26 21:59:16  thiessen
* tweaks for new window styles
*
* Revision 1.30  2002/05/17 19:10:27  thiessen
* preliminary range restriction for BLAST/PSSM
*
* Revision 1.29  2002/03/01 19:21:00  thiessen
* add icon to all frames
*
* Revision 1.28  2002/03/01 15:47:46  thiessen
* try tool window style for sequence/log viewers
*
* Revision 1.27  2002/02/21 12:26:30  thiessen
* fix row delete bug ; remember threader options
*
* Revision 1.26  2001/12/06 23:13:47  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.25  2001/11/27 16:26:10  thiessen
* major update to data management system
*
* Revision 1.24  2001/10/16 21:49:07  thiessen
* restructure MultiTextDialog; allow virtual bonds for alpha-only PDB's
*
* Revision 1.23  2001/08/16 19:21:02  thiessen
* add face name info to fonts
*
* Revision 1.22  2001/08/14 17:18:22  thiessen
* add user font selection, store in registry
*
* Revision 1.21  2001/07/27 13:52:47  thiessen
* make sure domains are assigned in order of molecule id; tweak pattern dialog
*
* Revision 1.20  2001/07/26 13:41:53  thiessen
* add pattern memory, make period optional
*
* Revision 1.19  2001/07/24 15:02:59  thiessen
* use ProSite syntax for pattern searches
*
* Revision 1.18  2001/07/23 20:24:23  thiessen
* need redraw if no matches found
*
* Revision 1.17  2001/07/23 20:09:23  thiessen
* add regex pattern search
*
* Revision 1.16  2001/05/31 14:32:33  thiessen
* tweak font handling
*
* Revision 1.15  2001/05/23 17:45:41  thiessen
* change dialog implementation to wxDesigner; interface changes
*
* Revision 1.14  2001/05/15 14:57:56  thiessen
* add cn3d_tools; bring up log window when threading starts
*
* Revision 1.13  2001/05/11 13:45:06  thiessen
* set up data directory
*
* Revision 1.12  2001/05/11 02:10:42  thiessen
* add better merge fail indicators; tweaks to windowing/taskbar
*
* Revision 1.11  2001/04/18 15:46:54  thiessen
* show description, length, and PDB numbering in status line
*
* Revision 1.10  2001/04/05 22:55:37  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.9  2001/03/30 14:43:41  thiessen
* show threader scores in status line; misc UI tweaks
*
* Revision 1.8  2001/03/30 03:07:35  thiessen
* add threader score calculation & sorting
*
* Revision 1.7  2001/03/19 15:50:40  thiessen
* add sort rows by identifier
*
* Revision 1.6  2001/03/13 01:25:07  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.5  2001/03/09 15:49:06  thiessen
* major changes to add initial update viewer
*
* Revision 1.4  2001/03/07 13:39:12  thiessen
* damn string namespace weirdness again
*
* Revision 1.3  2001/03/06 20:20:51  thiessen
* progress towards >1 alignment in a SequenceDisplay ; misc minor fixes
*
* Revision 1.2  2001/03/02 03:26:59  thiessen
* fix dangling pointer upon app close
*
* Revision 1.1  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
*/
