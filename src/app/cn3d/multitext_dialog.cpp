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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "multitext_dialog.hpp"
#include "cn3d_tools.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

BEGIN_EVENT_TABLE(MultiTextDialog, wxDialog)
    EVT_CLOSE       (       MultiTextDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    MultiTextDialog::OnButton)
    EVT_TEXT        (-1,    MultiTextDialog::OnTextChange)
END_EVENT_TABLE()

MultiTextDialog::MultiTextDialog(MultiTextDialogOwner *owner,
        const TextLines& initialText,
        wxWindow* parent, wxWindowID id, const wxString& title) :
    wxDialog(parent, id, title, wxDefaultPosition, wxDefaultSize,
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
    myOwner(owner)
{
    // set size/position from registry
    int posX, posY, sizeW, sizeH;
    if (!RegistryGetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_POS_X, &posX) ||
        !RegistryGetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_POS_Y, &posY) ||
        !RegistryGetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_SIZE_W, &sizeW) ||
        !RegistryGetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_SIZE_H, &sizeH))
    {
        ERRORMSG("Failed to get dialog position+size from registry, using default");
        SetSize(100, 100, 500, 400);
    } else {
        SetSize(posX, posY, sizeW, sizeH);
    }

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
        unsigned int i, j;
        for (i=j=0; i<initialText[0].size(); ++i, ++j) {
            if (j > 60 && initialText[0][i] == ' ') {
                *textCtrl << '\n';
                j = 0;
            } else
                *textCtrl << initialText[0][i];
        }
    } else {
        for (unsigned int i=0; i<initialText.size(); ++i)
            *textCtrl << initialText[i].c_str() << '\n';
    }

    SetSizeHints(200, 150); // min size
    SetAutoLayout(true);
    Layout();
}

MultiTextDialog::~MultiTextDialog(void)
{
    if (myOwner) myOwner->DialogDestroyed(this);

    int posX, posY, sizeW, sizeH;
    GetPosition(&posX, &posY);
    GetSize(&sizeW, &sizeH);
    if (!RegistrySetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_POS_X, posX) ||
        !RegistrySetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_POS_Y, posY) ||
        !RegistrySetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_SIZE_W, sizeW) ||
        !RegistrySetInteger(REG_CONFIG_SECTION, REG_MT_DIALOG_SIZE_H, sizeH))
    {
        ERRORMSG("Failed to set dialog position+size in registry");
    }
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
