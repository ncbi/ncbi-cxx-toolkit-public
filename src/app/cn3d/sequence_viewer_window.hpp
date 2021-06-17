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
*      implementation of GUI part of main sequence/alignment viewer
*
* ===========================================================================
*/

#ifndef CN3D_SEQUENCE_VIEWER_WINDOW__HPP
#define CN3D_SEQUENCE_VIEWER_WINDOW__HPP

#include "viewer_window_base.hpp"


BEGIN_SCOPE(Cn3D)

class SequenceViewer;
class SequenceDisplay;
class TaxonomyTree;

class SequenceViewerWindow : public ViewerWindowBase
{
    friend class SequenceDisplay;

public:
    SequenceViewerWindow(SequenceViewer *parentSequenceViewer);
    ~SequenceViewerWindow(void);

    bool RequestEditorEnable(bool enable);
    void EnableDerivedEditorMenuItems(bool enabled);
    void TurnOnEditor(void);
    void SetWindowTitle(void);

private:
    SequenceViewer *sequenceViewer;
    TaxonomyTree *taxonomyTree;

    // menu identifiers - additional items beyond base class items
    enum {
        // view menu
        MID_SHOW_HIDE_ROWS = START_VIEWER_WINDOW_DERIVED_MID,
        MID_SCORE_THREADER,
        MID_SELF_HIT,
        MID_TAXONOMY,
            MID_TAXONOMY_FULL,
            MID_TAXONOMY_ABBR,
        MID_EXPORT,
            MID_EXPORT_FASTA,
            MID_EXPORT_A2M,
            MID_EXPORT_TEXT,
            MID_EXPORT_HTML,
            MID_EXPORT_PSSM,
        MID_HIGHLIGHT_BLOCKS,
        MID_EXPAND_HIGHLIGHTS,
        MID_RESTRICT_HIGHLIGHTS,
        MID_CLEAR_HIGHLIGHTS,
        // edit menu
        MID_DELETE_ROW,
        MID_SORT_ROWS,   // sort rows submenu
            MID_SORT_IDENT,
            MID_SORT_THREADER,
            MID_FLOAT_PDBS,
            MID_FLOAT_HIGHLIGHTS,
            MID_FLOAT_G_V,
            MID_SORT_SELF_HIT,
            MID_PROXIMITY_SORT,
            MID_SORT_LOOPS,
        MID_SQ_REFINER_OPTIONS,
        MID_SQ_REFINER_RUN,
        // mouse mode
        MID_MOVE_ROW,
        // update menu
        MID_SHOW_UPDATES,
        MID_REALIGN_ROW,
        MID_REALIGN_ROWS,
        MID_REALIGN_HLIT_ROWS,
        MID_MARK_BLOCK,
        MID_CLEAR_MARKS
    };

    void OnShowHideRows(wxCommandEvent& event);
    void OnDeleteRow(wxCommandEvent& event);
    void OnMoveRow(wxCommandEvent& event);
    void OnShowUpdates(wxCommandEvent& event);
    void OnRealign(wxCommandEvent& event);
    void OnSort(wxCommandEvent& event);
    void OnScoreThreader(wxCommandEvent& event);
    void OnMarkBlock(wxCommandEvent& event);
    void OnExport(wxCommandEvent& event);
    void OnSelfHit(wxCommandEvent& event);
    void OnTaxonomy(wxCommandEvent& event);
    void OnHighlight(wxCommandEvent& event);
    void OnRefineAlignment(wxCommandEvent& event);

    // called before an operation (e.g., alignment editor enable) that requires
    // all rows of an alignment to be visible; 'false' return should abort that operation
    bool QueryShowAllRows(void);

    void OnCloseWindow(wxCloseEvent& event);

    wxMenu *updateMenu;

    void RestrictHighlightsOff(void)
    {
        menuBar->Check(MID_RESTRICT_HIGHLIGHTS, false);
        SetCursor(wxNullCursor);
    }
    void DeleteRowOff(void)
    {
        menuBar->Check(MID_DELETE_ROW, false);
        SetCursor(wxNullCursor);
    }
    void RealignRowOff(void)
    {
        menuBar->Check(MID_REALIGN_ROW, false);
        SetCursor(wxNullCursor);
    }
    void MarkBlockOff(void)
    {
        menuBar->Check(MID_MARK_BLOCK, false);
        SetCursor(wxNullCursor);
    }
    void ProximitySortOff(void)
    {
        menuBar->Check(MID_PROXIMITY_SORT, false);
        SetCursor(wxNullCursor);
    }
    void SortLoopsOff(void)
    {
        menuBar->Check(MID_SORT_LOOPS, false);
        SetCursor(wxNullCursor);
    }

    SequenceViewerWidget::eMouseMode GetMouseModeForCreateAndMerge(void)
    {
        return SequenceViewerWidget::eSelectColumns;
    }

    DECLARE_EVENT_TABLE()

public:

    bool SaveDialog(bool prompt, bool canCancel);

    bool DoRestrictHighlights(void) const { return menuBar->IsChecked(MID_RESTRICT_HIGHLIGHTS); }
    bool DoDeleteRow(void) const { return menuBar->IsChecked(MID_DELETE_ROW); }
    bool DoRealignRow(void) const { return menuBar->IsChecked(MID_REALIGN_ROW); }
    bool DoMarkBlock(void) const { return menuBar->IsChecked(MID_MARK_BLOCK); }
    bool DoProximitySort(void) const { return menuBar->IsChecked(MID_PROXIMITY_SORT); }
    bool DoSortLoops(void) const { return menuBar->IsChecked(MID_SORT_LOOPS); }

    void CancelDerivedSpecialModesExcept(int id)
    {
        if (id != MID_RESTRICT_HIGHLIGHTS && DoRestrictHighlights()) RestrictHighlightsOff();
        if (id != MID_DELETE_ROW && DoDeleteRow()) DeleteRowOff();
        if (id != MID_REALIGN_ROW && DoRealignRow()) RealignRowOff();
        if (id != MID_MARK_BLOCK && DoMarkBlock()) MarkBlockOff();
        if (id != MID_PROXIMITY_SORT && DoProximitySort()) ProximitySortOff();
        if (id != MID_SORT_LOOPS && DoSortLoops()) SortLoopsOff();
    }
};

END_SCOPE(Cn3D)

#endif // CN3D_SEQUENCE_VIEWER_WINDOW__HPP
