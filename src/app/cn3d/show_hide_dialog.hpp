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
*      Class definition for the Show/Hide dialog
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2001/03/09 15:48:43  thiessen
* major changes to add initial update viewer
*
* Revision 1.2  2000/12/15 15:52:08  thiessen
* show/hide system installed
*
* Revision 1.1  2000/11/17 19:47:38  thiessen
* working show/hide alignment row
*
* ===========================================================================
*/

#ifndef CN3D_SHOW_HIDE_DIALOG__HPP
#define CN3D_SHOW_HIDE_DIALOG__HPP

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include <wx/wx.h>

#include <vector>

#include "cn3d/show_hide_callback.hpp"


BEGIN_SCOPE(Cn3D)

class ShowHideDialog : public wxFrame
{
public:

    enum {
        // return values
        DONE,
        CANCEL,

        // control identifiers
        LISTBOX,
        B_DONE,
        B_CANCEL,
        B_APPLY
    };

    ShowHideDialog(
        const wxString items[],                 // must have items->size() wxStrings
        std::vector < bool > *itemsOn,          // modified (upon Activate()) to reflect user selection
        ShowHideCallbackObject *callback,
        wxWindow* parent,
        wxWindowID id,
        const wxString& title,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize
    );

    void Activate(void);

private:
    void OnSelection(wxCommandEvent& event);
    void OnButton(wxCommandEvent& event);
    void OnCloseWindow(wxCommandEvent& event);

    void EndEventLoop(void);

    std::vector < bool > *itemsEnabled;

    ShowHideCallbackObject *callbackObject;
    wxListBox *listBox;
    wxButton *applyB, *cancelB;
    bool dialogActive;
    std::vector < bool > beforeChange;

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_SHOW_HIDE_DIALOG__HPP
