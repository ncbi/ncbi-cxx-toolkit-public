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
*      implementation of GUI part of update viewer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2001/06/01 21:48:02  thiessen
* add terminal cutoff to threading
*
* Revision 1.10  2001/05/31 18:46:28  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.9  2001/05/23 17:43:29  thiessen
* change dialog implementation to wxDesigner; interface changes
*
* Revision 1.8  2001/05/17 18:34:01  thiessen
* spelling fixes; change dialogs to inherit from wxDialog
*
* Revision 1.7  2001/05/02 13:46:16  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.6  2001/04/19 12:58:25  thiessen
* allow merge and delete of individual updates
*
* Revision 1.5  2001/04/12 18:09:40  thiessen
* add block freezing
*
* Revision 1.4  2001/04/04 00:27:22  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.3  2001/03/22 00:32:36  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.2  2001/03/13 01:24:16  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.1  2001/03/09 15:48:44  thiessen
* major changes to add initial update viewer
*
* ===========================================================================
*/

#ifndef CN3D_UPDATE_VIEWER_WINDOW__HPP
#define CN3D_UPDATE_VIEWER_WINDOW__HPP

#include "cn3d/viewer_window_base.hpp"

#include <wx/spinctrl.h>


BEGIN_SCOPE(Cn3D)

class UpdateViewer;
class SequenceDisplay;

class UpdateViewerWindow : public ViewerWindowBase
{
    friend SequenceDisplay;

public:
    UpdateViewerWindow(UpdateViewer *parentUpdateViewer);
    ~UpdateViewerWindow(void);

    void EnableDerivedEditorMenuItems(bool enabled);

    // ask if user wants to save edits; return value indicates whether program should
    // continue after this dialog - i.e., returns false if user hits 'cancel';
    // program should then abort the operation that engendered this function call.
    // 'canCancel' tells whether or not a 'cancel' button is even displayed - thus
    // if 'canCancel' is false, the function will always return true.
    bool SaveDialog(bool canCancel);

private:
    UpdateViewer *updateViewer;

    // menu identifiers - additional items beyond base class items
    enum {
        MID_THREAD_ONE = START_VIEWER_WINDOW_DERIVED_MID,
        MID_THREAD_ALL,
        MID_MERGE_ONE,
        MID_MERGE_ALL,
        MID_DELETE_ONE,
        MID_DELETE_ALL
    };

    void OnCloseWindow(wxCloseEvent& event);
    void OnRunThreader(wxCommandEvent& event);
    void OnMerge(wxCommandEvent& event);
    void OnDelete(wxCommandEvent& event);

    void ThreadSingleOff(void)
    {
        menuBar->Check(MID_THREAD_ONE, false);
        SetCursor(wxNullCursor);
    }
    void MergeSingleOff(void)
    {
        menuBar->Check(MID_MERGE_ONE, false);
        SetCursor(wxNullCursor);
    }
    void DeleteSingleOff(void)
    {
        menuBar->Check(MID_DELETE_ONE, false);
        SetCursor(wxNullCursor);
    }

    SequenceViewerWidget::eMouseMode GetMouseModeForCreateAndMerge(void)
    {
        return SequenceViewerWidget::eSelectRectangle;
    }

    DECLARE_EVENT_TABLE()

public:
    bool DoThreadSingle(void) const { return menuBar->IsChecked(MID_THREAD_ONE); }
    bool DoMergeSingle(void) const { return menuBar->IsChecked(MID_MERGE_ONE); }
    bool DoDeleteSingle(void) const { return menuBar->IsChecked(MID_DELETE_ONE); }

    void CancelDerivedSpecialModes(void)
    {
        if (DoThreadSingle()) ThreadSingleOff();
        if (DoDeleteSingle()) DeleteSingleOff();
    }
};


///// dialog used to get threader options from user /////

class ThreaderOptions;
class FloatingPointSpinCtrl;
class IntegerSpinCtrl;

class ThreaderOptionsDialog : public wxDialog
{
public:
    ThreaderOptionsDialog(wxWindow* parent, const ThreaderOptions& initialOptions);

    // set the ThreaderOptions from values in the panel; returns true if all values are valid
    bool GetValues(ThreaderOptions *options);

private:
    wxStaticBox *box;
    wxButton *bOK, *bCancel;
    FloatingPointSpinCtrl *fpWeight, *fpLoops;
    IntegerSpinCtrl *iStarts, *iResults, *iCutoff;
    wxCheckBox *bMerge, *bFreeze;

    void OnCloseWindow(wxCommandEvent& event);
    void OnButton(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_UPDATE_VIEWER_WINDOW__HPP
