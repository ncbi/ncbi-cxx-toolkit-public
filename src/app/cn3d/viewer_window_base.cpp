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
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include "cn3d/viewer_window_base.hpp"
#include "cn3d/viewer_base.hpp"
#include "cn3d/sequence_display.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/cn3d_threader.hpp"
#include "cn3d/alignment_manager.hpp"
#include "cn3d/cn3d_tools.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

ViewerWindowBase::ViewerWindowBase(ViewerBase *parentViewer,
        const char* title, const wxPoint& pos, const wxSize& size) :
    wxFrame(GlobalTopWindow(), wxID_HIGHEST + 10, title, pos, size,
        wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT
#if wxVERSION_NUMBER >= 2302
            | wxFRAME_NO_TASKBAR
#endif
        ),
    viewerWidget(NULL), viewer(parentViewer)
{
    if (!parentViewer) ERR_POST(Error << "ViewerWindowBase::ViewerWindowBase() - got NULL pointer");

    SetSizeHints(200, 150);

    // status bar with two fields - first is for id/loc, second is for general status line
    CreateStatusBar(2);
    int widths[2] = { 175, -1 };
    SetStatusWidths(2, widths);

    viewerWidget = new SequenceViewerWidget(this);
    SetupFontFromRegistry();

    menuBar = new wxMenuBar;
    viewMenu = new wxMenu;
    viewMenu->Append(MID_SHOW_TITLES, "Show Tit&les");
    //menu->Append(MID_HIDE_TITLES, "&Hide Titles");
    viewMenu->Append(MID_SHOW_GEOM_VLTNS, "Show &Geometry Violations");
    viewMenu->Append(MID_FIND_PATTERN, "Find &Pattern");
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
    editMenu->Append(MID_SYNC_STRUCS, "Sy&nc Structure Colors");
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
    EnableBaseEditorMenuItems(false);

    SetMenuBar(menuBar);
}

ViewerWindowBase::~ViewerWindowBase(void)
{
    if (viewer) viewer->GUIDestroyed();
}

void ViewerWindowBase::SetupFontFromRegistry(void)
{
    // get font info from registry, and create wxFont
    int size, family, style, weight;
    bool underlined;
    std::string faceName;
    wxFont *font;
    if (!RegistryGetInteger(REG_SEQUENCE_FONT_SECTION, REG_FONT_SIZE, &size) ||
        !RegistryGetInteger(REG_SEQUENCE_FONT_SECTION, REG_FONT_FAMILY, &family) ||
        !RegistryGetInteger(REG_SEQUENCE_FONT_SECTION, REG_FONT_STYLE, &style) ||
        !RegistryGetInteger(REG_SEQUENCE_FONT_SECTION, REG_FONT_WEIGHT, &weight) ||
        !RegistryGetBoolean(REG_SEQUENCE_FONT_SECTION, REG_FONT_UNDERLINED, &underlined) ||
        !RegistryGetString(REG_SEQUENCE_FONT_SECTION, REG_FONT_FACENAME, &faceName) ||
        !(font = new wxFont(size, family, style, weight, underlined,
            (faceName == FONT_FACENAME_UNKNOWN) ? "" : faceName.c_str())))
    {
        ERR_POST(Error << "ViewerWindowBase::SetupFontFromRegistry() - error setting up font");
        return;
    }
    viewerWidget->SetCharacterFont(font);
}

void ViewerWindowBase::EnableBaseEditorMenuItems(bool enabled)
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

void ViewerWindowBase::NewDisplay(SequenceDisplay *display, bool enableEditor, bool enableSelectByColumn)
{
    viewerWidget->AttachAlignment(display);
    if (!display->IsEditable()) {
        enableEditor = false;
        enableSelectByColumn = false;
    }
    menuBar->EnableTop(1, enableEditor);    // edit menu
    menuBar->EnableTop(3, enableEditor);    // justification menu
    menuBar->Enable(MID_SELECT_COLS, enableSelectByColumn);
}

void ViewerWindowBase::UpdateDisplay(SequenceDisplay *display)
{
    int vsX, vsY;   // to preserve scroll position
    viewerWidget->GetScroll(&vsX, &vsY);
    viewerWidget->AttachAlignment(display, vsX, vsY);
    GlobalMessenger()->PostRedrawAllSequenceViewers();
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
                EnableBaseEditorMenuItems(true);
                viewer->GetCurrentDisplay()->AddBlockBoundaryRows();    // add before push!
                viewer->PushAlignment();    // keep copy of original at bottom of the stack
                Command(MID_DRAG_HORIZ);    // switch to drag mode
            } else {
                if (!RequestEditorEnable(false)) {   // cancelled
                    menuBar->Check(MID_ENABLE_EDIT, true);
                    break;
                }
                TESTMSG("turning off editor");
                EnableBaseEditorMenuItems(false);
                viewer->GetCurrentDisplay()->RemoveBlockBoundaryRows();
                if (!menuBar->IsChecked(MID_SELECT_COLS) || !menuBar->IsChecked(MID_SELECT_ROWS))
                    Command(MID_SELECT_RECT);
            }
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
            CancelDerivedSpecialModes();
            if (DoSplitBlock())
                SetCursor(*wxCROSS_CURSOR);
            else
                SplitBlockOff();
            break;

        case MID_MERGE_BLOCKS:
            if (DoSplitBlock()) SplitBlockOff();
            if (DoCreateBlock()) CreateBlockOff();
            if (DoDeleteBlock()) DeleteBlockOff();
            CancelDerivedSpecialModes();
            if (DoMergeBlocks()) {
                SetCursor(*wxCROSS_CURSOR);
                prevMouseMode = viewerWidget->GetMouseMode();
                viewerWidget->SetMouseMode(GetMouseModeForCreateAndMerge());
            } else
                MergeBlocksOff();
            break;

        case MID_CREATE_BLOCK:
            if (DoSplitBlock()) SplitBlockOff();
            if (DoMergeBlocks()) MergeBlocksOff();
            if (DoDeleteBlock()) DeleteBlockOff();
            CancelDerivedSpecialModes();
            if (DoCreateBlock()) {
                SetCursor(*wxCROSS_CURSOR);
                prevMouseMode = viewerWidget->GetMouseMode();
                viewerWidget->SetMouseMode(GetMouseModeForCreateAndMerge());
            } else
                CreateBlockOff();
            break;

        case MID_DELETE_BLOCK:
            if (DoSplitBlock()) SplitBlockOff();
            if (DoMergeBlocks()) MergeBlocksOff();
            if (DoCreateBlock()) CreateBlockOff();
            CancelDerivedSpecialModes();
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
    for (int i=0; i<items.GetCount(); i++)
        items.Item(i)->GetData()->Check(
            (items.Item(i)->GetData()->GetId() == event.GetId()) ? true : false);

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

void ViewerWindowBase::OnShowGeomVltns(wxCommandEvent& event)
{
    const ViewerBase::AlignmentList *alignments = viewer->GetCurrentAlignments();
    if (!alignments) return;

    Threader::GeometryViolationsForRow violations;
    ViewerBase::AlignmentList::const_iterator a, ae = alignments->end();
    for (a=alignments->begin(); a!=ae; a++) {
        viewer->alignmentManager->threader->GetGeometryViolations(*a, &violations);
        (*a)->ShowGeometryViolations(violations);
    }
    GlobalMessenger()->PostRedrawSequenceViewer(viewer);
}

void ViewerWindowBase::OnFindPattern(wxCommandEvent& event)
{
    // remember previous pattern
    static wxString previousPattern;

    // get pattern from user
    wxString pattern = wxGetTextFromUser("Enter a pattern using ProSite syntax:",
        "Input pattern", previousPattern, this);
    if (pattern.size() == 0) return;
    // add trailing period if not present (convenience for the user)
    if (pattern[pattern.size() - 1] != '.') pattern += '.';
    previousPattern = pattern;

    GlobalMessenger()->RemoveAllHighlights(true);

    // highlight pattern from each (unique) sequence in the display
    std::map < const Sequence * , bool > usedSequences;
    const SequenceDisplay *display = viewer->GetCurrentDisplay();

    for (int i=0; i<display->NRows(); i++) {

        const Sequence *sequence = display->GetSequenceForRow(i);
        if (!sequence || usedSequences.find(sequence) != usedSequences.end()) continue;
        usedSequences[sequence] = true;

        if (!sequence->HighlightPattern(pattern.c_str())) break;
    }
}

END_SCOPE(Cn3D)

