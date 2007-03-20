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
*       generic multi-line text entry dialog
*
* ===========================================================================
*/

#ifndef CN3D_MULTITEXT_DIALOG__HPP
#define CN3D_MULTITEXT_DIALOG__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>

#include <map>
#include <vector>


BEGIN_SCOPE(Cn3D)

class MultiTextDialog;

// so the owner can receive notification that dialog text has changed, or dialog destroyed

class MultiTextDialogOwner
{
public:
    virtual void DialogTextChanged(const MultiTextDialog *changed) = 0;
    virtual void DialogDestroyed(const MultiTextDialog *destroyed) = 0;
};


// this is really intended to be used as a non-modal dialog; it calls its owner's DialogTextChanged()
// method every time the user types something. But it should function modally as well.

class MultiTextDialog : private wxDialog
{
public:
    typedef std::vector < std::string > TextLines;

    MultiTextDialog(MultiTextDialogOwner *owner, const TextLines& initialText,
        wxWindow* parent, wxWindowID id, const wxString& title);
    ~MultiTextDialog(void);

    bool GetLines(TextLines *lines) const;
    bool GetLine(std::string *singleString) const;  // collapse all lines to single string

    bool ShowDialog(bool);
    int ShowModalDialog(void);
    bool DestroyDialog(void) { return Destroy(); }

private:
    MultiTextDialogOwner *myOwner;

    // GUI elements
    wxTextCtrl *textCtrl;
    wxButton *bDone, *bRevert;

    // remember initial text (for revert)
    const TextLines initialText;
    void SetToInitialText(void);

    // event callbacks
    void OnButton(wxCommandEvent& event);
    void OnTextChange(wxCommandEvent& event);
    void OnCloseWindow(wxCloseEvent& event);

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_MULTITEXT_DIALOG__HPP
