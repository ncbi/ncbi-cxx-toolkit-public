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
public:
    UpdateViewerWindow(UpdateViewer *parentUpdateViewer);
    ~UpdateViewerWindow(void);

    void EnableDerivedEditorMenuItems(bool enabled);

    DECLARE_EVENT_TABLE()

private:
    UpdateViewer *updateViewer;

    // menu identifiers - additional items beyond base class items
    enum {
        MID_RUN_THREADER = START_VIEWER_WINDOW_DERIVED_MID,
        MID_MERGE_ALL
    };

    void OnCloseWindow(wxCloseEvent& event);
    void OnRunThreader(wxCommandEvent& event);
    void OnMerge(wxCommandEvent& event);

public:
};


///// dialog used to get threader options from user /////

class ThreaderOptions;
class FloatingPointSpinCtrl;
class IntegerSpinCtrl;

class ThreaderOptionsDialog : public wxFrame
{
public:
    ThreaderOptionsDialog(wxWindow* parent, const ThreaderOptions& initialOptions);

    // show the dialog; returns true if user hits "OK" and all values are valid
    bool Activate(void);

    // set the ThreaderOptions from values in the panel; returns true if all values are valid
    bool GetValues(ThreaderOptions *options);

private:
    wxStaticBox *box;
    wxButton *bOK, *bCancel;
    FloatingPointSpinCtrl *fpWeight, *fpLoops;
    IntegerSpinCtrl *iStarts, *iResults;
    wxCheckBox *bMerge;

    bool dialogActive;
    bool returnOK;

    void EndEventLoop(void);
    void OnCloseWindow(wxCommandEvent& event);
    void OnButton(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_UPDATE_VIEWER_WINDOW__HPP
