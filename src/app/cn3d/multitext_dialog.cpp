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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2001/10/11 14:18:57  thiessen
* make MultiTextDialog non-modal
*
* Revision 1.2  2001/08/06 20:22:00  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* Revision 1.1  2001/07/10 16:39:54  thiessen
* change selection control keys; add CDD name/notes dialogs
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include "cn3d/multitext_dialog.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

BEGIN_EVENT_TABLE(MultiTextDialog, wxDialog)
    EVT_CLOSE       (       MultiTextDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    MultiTextDialog::OnButton)
    EVT_TEXT        (-1,    MultiTextDialog::OnTextChange)
END_EVENT_TABLE()

MultiTextDialog::MultiTextDialog(MultiTextDialogOwner *owner, const TextLines& initialText,
        wxWindow* parent, wxWindowID id, const wxString& title,
        const wxPoint& pos, const wxSize& size) :
    wxDialog(parent, id, title, pos, size, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
    myOwner(owner)
{
    textCtrl = new wxTextCtrl(this, -1, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxHSCROLL);
    bDone = new wxButton(this, -1, "Done");

    wxLayoutConstraints *c = new wxLayoutConstraints;
    c->bottom.SameAs    (this, wxBottom, 10);
    c->centreX.SameAs   (this, wxCentreX, 10);
    c->width.AsIs       ();
    c->height.AsIs      ();
    bDone->SetConstraints(c);

    c = new wxLayoutConstraints;
    c->top.SameAs       (this, wxTop);
    c->left.SameAs      (this, wxLeft);
    c->right.SameAs     (this, wxRight);
    c->bottom.SameAs    (bDone, wxTop, 10);
    textCtrl->SetConstraints(c);

    SetSizeHints(200, 150);
    SetAutoLayout(true);
    Layout();

    // set initial text - if single line, break up into smaller lines, otherwise leave as-is
    if (initialText.size() == 1) {
        int i, j;
        for (i=j=0; i<initialText[0].size(); i++, j++) {
            if (j > 60 && initialText[0][i] == ' ') {
                *textCtrl << '\n';
                j = 0;
            } else
                *textCtrl << initialText[0][i];
        }
    } else {
        for (int i=0; i<initialText.size(); i++)
            *textCtrl << initialText[i].c_str() << '\n';
    }
}

void MultiTextDialog::OnCloseWindow(wxCloseEvent& event)
{
    if (event.CanVeto()) {
        event.Veto();
        Show(false);
    } else {
        Destroy();
    }
}

void MultiTextDialog::OnButton(wxCommandEvent& event)
{
    if (event.GetEventObject() == bDone) Show(false);
}

void MultiTextDialog::OnTextChange(wxCommandEvent& event)
{
    if (myOwner) myOwner->DialogTextChanged(this);
}

bool MultiTextDialog::GetLines(TextLines *lines) const
{
    if (!lines) return false;
    lines->clear();
    for (int i=0; i<textCtrl->GetNumberOfLines(); i++)
        // don't append empty last line
        if (i < textCtrl->GetNumberOfLines() - 1 || textCtrl->GetLineText(i).size() > 0)
            lines->push_back(textCtrl->GetLineText(i).c_str());
    return true;
}

bool MultiTextDialog::GetLine(std::string *singleString) const
{
    singleString->erase();
    for (int i=0; i<textCtrl->GetNumberOfLines(); i++) {
        // don't append empty last line
        if (i < textCtrl->GetNumberOfLines() - 1 || textCtrl->GetLineText(i).size() > 0) {
            if (i > 0) singleString->append(1, ' ');
            singleString->append(textCtrl->GetLineText(i).Strip(wxString::both).c_str());
        }
    }
    return true;
}

END_SCOPE(Cn3D)

