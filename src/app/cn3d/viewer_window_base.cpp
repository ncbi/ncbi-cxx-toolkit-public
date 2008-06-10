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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <memory>

#include "remove_header_conflicts.hpp"

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
    #include "cn3d42App.xpm"
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
    GlobalMessenger()->PostRedrawAllSequenceViewers();
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
                ProcessCommand(MID_DRAG_HORIZ);    // switch to drag mode
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
                    ProcessCommand(MID_SELECT_RECT);
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
    for (unsigned int i=0; i<items.GetCount(); ++i)
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

    // set up highlighting
    Messenger::MoleculeHighlightMap restrictTo;
    if (dialog.m_Mode->GetStringSelection() == "Within")
        GlobalMessenger()->GetHighlights(&restrictTo);
    if (dialog.m_Mode->GetStringSelection() == "Replace" || dialog.m_Mode->GetStringSelection() == "Within")
        GlobalMessenger()->RemoveAllHighlights(true);

    // highlight pattern from each (unique) sequence in the display
    map < const Sequence * , bool > usedSequences;
    const SequenceDisplay *display = viewer->GetCurrentDisplay();

    for (unsigned int i=0; i<display->NRows(); ++i) {

        const Sequence *sequence = display->GetSequenceForRow(i);
        if (!sequence || usedSequences.find(sequence) != usedSequences.end()) continue;
        usedSequences[sequence] = true;

        if (!sequence->HighlightPattern(pattern.c_str(), restrictTo)) break;
    }
}

void ViewerWindowBase::MakeSequenceVisible(const MoleculeIdentifier *identifier)
{
    const SequenceDisplay *display = viewer->GetCurrentDisplay();
    for (unsigned int i=0; i<display->NRows(); ++i) {
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
