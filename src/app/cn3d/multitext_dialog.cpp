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

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "multitext_dialog.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

BEGIN_EVENT_TABLE(MultiTextDialog, wxDialog)
    EVT_CLOSE       (       MultiTextDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    MultiTextDialog::OnButton)
    EVT_TEXT        (-1,    MultiTextDialog::OnTextChange)
END_EVENT_TABLE()

MultiTextDialog::MultiTextDialog(MultiTextDialogOwner *owner,
        const TextLines& initialText,
        wxWindow* parent, wxWindowID id, const wxString& title,
        const wxPoint& pos, const wxSize& size) :
    wxDialog(parent, id, title, pos, size,
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxDIALOG_MODELESS | wxFRAME_NO_TASKBAR),
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

    // set initial text - if single line, break up into smaller lines, otherwise leave as-is
    if (initialText.size() == 1) {
        int i, j;
        for (i=j=0; i<initialText[0].size(); ++i, ++j) {
            if (j > 60 && initialText[0][i] == ' ') {
                *textCtrl << '\n';
                j = 0;
            } else
                *textCtrl << initialText[0][i];
        }
    } else {
        for (int i=0; i<initialText.size(); ++i)
            *textCtrl << initialText[i].c_str() << '\n';
    }

    SetClientSize(size);
    SetSizeHints(200, 150); // min size
    SetAutoLayout(true);
    Layout();
}

MultiTextDialog::~MultiTextDialog(void)
{
    if (myOwner) myOwner->DialogDestroyed(this);
}

void MultiTextDialog::OnCloseWindow(wxCloseEvent& event)
{
    Destroy();
}

// these two are possibly temporary kludges to keep text area from coming up initially all selected
int MultiTextDialog::ShowModalDialog(void)
{
    Show(true);
    textCtrl->SetSelection(0, 0);
    textCtrl->SetInsertionPointEnd();
    return ShowModal();
}

bool MultiTextDialog::ShowDialog(bool show)
{
    bool retval = Show(show);
    textCtrl->SetSelection(0, 0);
    textCtrl->SetInsertionPointEnd();
    return retval;
}

void MultiTextDialog::OnButton(wxCommandEvent& event)
{
    if (event.GetEventObject() == bDone) Destroy();
}

void MultiTextDialog::OnTextChange(wxCommandEvent& event)
{
    if (myOwner) myOwner->DialogTextChanged(this);
}

bool MultiTextDialog::GetLines(TextLines *lines) const
{
    if (!lines) return false;
    lines->clear();
    for (int i=0; i<textCtrl->GetNumberOfLines(); ++i)
        // don't append empty last line
        if (i < textCtrl->GetNumberOfLines() - 1 || textCtrl->GetLineText(i).size() > 0)
            lines->push_back(textCtrl->GetLineText(i).c_str());
    return true;
}

bool MultiTextDialog::GetLine(string *singleString) const
{
    singleString->erase();
    for (int i=0; i<textCtrl->GetNumberOfLines(); ++i) {
        // don't append empty last line
        if (i < textCtrl->GetNumberOfLines() - 1 || textCtrl->GetLineText(i).size() > 0) {
            if (i > 0) singleString->append(1, ' ');
            singleString->append(textCtrl->GetLineText(i).Strip(wxString::both).c_str());
        }
    }
    return true;
}

END_SCOPE(Cn3D)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.11  2004/03/15 18:25:36  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.10  2004/02/19 17:04:59  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.9  2003/02/03 19:20:04  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.8  2002/09/18 19:46:54  thiessen
* don't use wxTE_RICH for this
*
* Revision 1.7  2002/09/18 13:19:32  thiessen
* use wxTE_RICH style for big textctrls
*
* Revision 1.6  2002/08/15 22:13:14  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.5  2002/06/12 15:09:15  thiessen
* kludge to avoid initial selected-all state
*
* Revision 1.4  2001/10/16 21:49:07  thiessen
* restructure MultiTextDialog; allow virtual bonds for alpha-only PDB's
*
* Revision 1.3  2001/10/11 14:18:57  thiessen
* make MultiTextDialog non-modal
*
* Revision 1.2  2001/08/06 20:22:00  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* Revision 1.1  2001/07/10 16:39:54  thiessen
* change selection control keys; add CDD name/notes dialogs
*
*/
