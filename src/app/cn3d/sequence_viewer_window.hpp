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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2001/03/19 15:47:38  thiessen
* add row sorting by identifier
*
* Revision 1.3  2001/03/13 01:24:16  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.2  2001/03/09 15:48:43  thiessen
* major changes to add initial update viewer
*
* Revision 1.1  2001/03/01 20:15:29  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* ===========================================================================
*/

#ifndef CN3D_SEQUENCE_VIEWER_WINDOW__HPP
#define CN3D_SEQUENCE_VIEWER_WINDOW__HPP

#include "cn3d/viewer_window_base.hpp"


BEGIN_SCOPE(Cn3D)

class SequenceViewer;
class SequenceDisplay;

class SequenceViewerWindow : public ViewerWindowBase
{
    friend class SequenceDisplay;

public:
    SequenceViewerWindow(SequenceViewer *parentSequenceViewer);
    ~SequenceViewerWindow(void);

    bool RequestEditorEnable(bool enable);
    void EnableDerivedEditorMenuItems(bool enabled);

    DECLARE_EVENT_TABLE()

private:
    SequenceViewer *sequenceViewer;

    // menu identifiers - additional items beyond base class items
    enum {
        // view menu
        MID_SHOW_HIDE_ROWS = START_VIEWER_WINDOW_DERIVED_MID,
        // edit menu
        MID_DELETE_ROW,
        MID_SORT_ROWS,   // sort rows submenu
            MID_SORT_IDENT,
        // mouse mode
        MID_MOVE_ROW,
        // update menu
        MID_SHOW_UPDATES,
        MID_REALIGN_ROW,
        MID_REALIGN_ROWS
    };

    void OnShowHideRows(wxCommandEvent& event);
    void OnDeleteRow(wxCommandEvent& event);
    void OnMoveRow(wxCommandEvent& event);
    void OnShowUpdates(wxCommandEvent& event);
    void OnRealign(wxCommandEvent& event);
    void OnSort(wxCommandEvent& event);

    // called before an operation (e.g., alignment editor enable) that requires
    // all rows of an alignment to be visible; 'false' return should abort that operation
    bool QueryShowAllRows(void);

    void OnCloseWindow(wxCloseEvent& event);

    wxMenu *updateMenu;

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

public:
    // ask if user wants to save edits; return value indicates whether program should
    // continue after this dialog - i.e., returns false if user hits 'cancel';
    // program should then abort the operation that engendered this function call.
    // 'canCancel' tells whether or not a 'cancel' button is even displayed - thus
    // if 'canCancel' is false, the function will always return true.
    bool SaveDialog(bool canCancel);

    bool DoDeleteRow(void) const { return menuBar->IsChecked(MID_DELETE_ROW); }
    bool DoRealignRow(void) const { return menuBar->IsChecked(MID_REALIGN_ROW); }

    void CancelDerivedSpecialModes(void)
    {
        if (DoDeleteRow()) DeleteRowOff();
        if (DoRealignRow()) RealignRowOff();
    }
};

END_SCOPE(Cn3D)

#endif // CN3D_SEQUENCE_VIEWER_WINDOW__HPP
