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
public:
    SequenceViewerWindow(SequenceViewer *parentSequenceViewer);
    ~SequenceViewerWindow(void);

    void OnShowHideRows(wxCommandEvent& event);
    void OnDeleteRow(wxCommandEvent& event);
    void OnMoveRow(wxCommandEvent& event);

    bool RequestEditorEnable(bool enable);
    void EnableDerivedEditorMenuItems(bool enabled);

    // menu identifiers - additional items beyond base class items
    enum {
        // view menu
        MID_SHOW_HIDE_ROWS = START_VIEWER_WINDOW_DERIVED_MID,

        // mouse mode
        MID_MOVE_ROW,
    };

    DECLARE_EVENT_TABLE()

private:
    SequenceViewer *sequenceViewer;

    // called before an operation (e.g., alignment editor enable) that requires
    // all rows of an alignment to be visible; 'false' return should abort that operation
    bool QueryShowAllRows(void);

    void OnCloseWindow(wxCloseEvent& event);

public:
    // ask if user wants to save edits; return value indicates whether program should
    // continue after this dialog - i.e., returns false if user hits 'cancel';
    // program should then abort the operation that engendered this function call.
    // 'canCancel' tells whether or not a 'cancel' button is even displayed - thus
    // if 'canCancel' is false, the function will always return true.
    bool SaveDialog(bool canCancel);
};

END_SCOPE(Cn3D)

#endif // CN3D_SEQUENCE_VIEWER_WINDOW__HPP
