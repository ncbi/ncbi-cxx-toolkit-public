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
* Revision 1.5  2001/05/17 18:34:26  thiessen
* spelling fixes; change dialogs to inherit from wxDialog
*
* Revision 1.4  2001/05/15 23:48:38  thiessen
* minor adjustments to compile under Solaris/wxGTK
*
* Revision 1.3  2001/03/09 15:49:05  thiessen
* major changes to add initial update viewer
*
* Revision 1.2  2000/12/15 15:51:47  thiessen
* show/hide system installed
*
* Revision 1.1  2000/11/17 19:48:14  thiessen
* working show/hide alignment row
*
* ===========================================================================
*/

#include "cn3d/show_hide_dialog.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

BEGIN_EVENT_TABLE(ShowHideDialog, wxDialog)
    EVT_LISTBOX     (LISTBOX,   ShowHideDialog::OnSelection)
    EVT_BUTTON      (B_APPLY,   ShowHideDialog::OnButton)
    EVT_BUTTON      (B_CANCEL,  ShowHideDialog::OnButton)
    EVT_BUTTON      (B_DONE,    ShowHideDialog::OnButton)
    EVT_CLOSE                  (ShowHideDialog::OnCloseWindow)
END_EVENT_TABLE()

ShowHideDialog::ShowHideDialog(
    const wxString items[],
    std::vector < bool > *itemsOn,
    ShowHideCallbackObject *callback,
    wxWindow* parent,
    wxWindowID id,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size
) :
    wxDialog(parent, id, title, pos, size, wxCAPTION | wxRESIZE_BORDER | wxFRAME_FLOAT_ON_PARENT),
    itemsEnabled(itemsOn), callbackObject(callback), applyB(NULL)
{
    SetAutoLayout(true);
    SetSizeHints(150, 100);

    static const int margin = 0;

	wxButton *doneB;
    if (callbackObject) {
        doneB = new wxButton(this, B_DONE, "Done");
        wxLayoutConstraints *c1 = new wxLayoutConstraints();
        c1->bottom.SameAs       (this,      wxBottom,   margin);
        c1->right.SameAs        (this,      wxRight,    margin);
        c1->width.PercentOf     (this,      wxWidth,    33);
        c1->height.AsIs         ();
        doneB->SetConstraints(c1);

        applyB = new wxButton(this, B_APPLY, "Apply");
        wxLayoutConstraints *c2 = new wxLayoutConstraints();
        c2->bottom.SameAs       (this,      wxBottom,   margin);
        c2->left.SameAs         (this,      wxLeft,     margin);
        c2->width.PercentOf     (this,      wxWidth,    33);
        c2->top.SameAs          (doneB,     wxTop);
        applyB->SetConstraints(c2);
        applyB->Enable(false);

        cancelB = new wxButton(this, B_CANCEL, "Cancel");
        wxLayoutConstraints *c3 = new wxLayoutConstraints();
        c3->bottom.SameAs       (this,      wxBottom,   margin);
        c3->left.RightOf        (applyB,                margin);
        c3->right.LeftOf        (doneB,                 margin);
        c3->top.SameAs          (doneB,     wxTop);
        cancelB->SetConstraints(c3);
        cancelB->Enable(false);
    }

    // don't include Apply button if no callback object given
    else {
        doneB = new wxButton(this, B_DONE, "Done");
        wxLayoutConstraints *c1 = new wxLayoutConstraints();
        c1->bottom.SameAs       (this,      wxBottom,   margin);
        c1->right.SameAs        (this,      wxRight,    margin);
        c1->width.PercentOf     (this,      wxWidth,    50);
        c1->height.AsIs         ();
        doneB->SetConstraints(c1);

        cancelB = new wxButton(this, B_CANCEL, "Cancel");
        wxLayoutConstraints *c3 = new wxLayoutConstraints();
        c3->bottom.SameAs       (this,      wxBottom,   margin);
        c3->left.SameAs         (this,      wxLeft,     margin);
        c3->right.LeftOf        (doneB,                 margin);
        c3->top.SameAs          (doneB,     wxTop);
        cancelB->SetConstraints(c3);
        cancelB->Enable(false);
    }

    listBox = new wxListBox(this, LISTBOX, wxDefaultPosition, wxDefaultSize,
        itemsEnabled->size(), items, wxLB_EXTENDED | wxLB_HSCROLL | wxLB_NEEDED_SB);
    wxLayoutConstraints *c4 = new wxLayoutConstraints();
    c4->top.SameAs          (this,      wxTop,      margin);
    c4->left.SameAs         (this,      wxLeft,     margin);
    c4->right.SameAs        (this,      wxRight,    margin);
    c4->bottom.Above        (doneB,                 margin);
    listBox->SetConstraints(c4);

    Layout();

    // set initial item selection (reverse order, so scroll bar is initially at the top)
    beforeChange.resize(itemsEnabled->size());
    for (int i=itemsEnabled->size()-1; i>=0; i--) {
        listBox->SetSelection(i, (*itemsEnabled)[i]);
        beforeChange[i] = (*itemsEnabled)[i];
    }
}

void ShowHideDialog::OnSelection(wxCommandEvent& event)
{
    // update selection list
    int i;
    for (i=0; i<listBox->Number(); i++)
        (*itemsEnabled)[i] = listBox->Selected(i);

    if (callbackObject) {
        // do the selection changed callback, and adjust selections accordingly
        callbackObject->SelectionChangedCallback(beforeChange, *itemsEnabled);

        // apply changes
        int pV = listBox->GetScrollPos(wxVERTICAL);
        listBox->Show(false);
        for (i=0; i<listBox->Number(); i++) {
            listBox->SetSelection(i, (*itemsEnabled)[i]);
            beforeChange[i] = (*itemsEnabled)[i];
        }
        // horrible kludge to keep vertical scroll at same position...
        if (pV >= 0 && pV < listBox->Number()) {
            listBox->SetSelection(listBox->Number() - 1, true);
            listBox->SetSelection(listBox->Number() - 1, (*itemsEnabled)[listBox->Number() - 1]);
            listBox->SetSelection(pV, true);
            listBox->SetSelection(pV, (*itemsEnabled)[pV]);
        }
        listBox->Show(true);
        applyB->Enable(true);
    }

    cancelB->Enable(true);
}

void ShowHideDialog::OnButton(wxCommandEvent& event)
{
    if (event.GetId() == B_CANCEL) {
        // restore item list to original state
        for (int i=0; i<beforeChange.size(); i++)
            (*itemsEnabled)[i] = beforeChange[i];
        EndModal(wxCANCEL);
    }

    else if (event.GetId() == B_DONE || event.GetId() == B_APPLY) {

        // only do this if selection has changed since last time
        if (callbackObject && applyB && applyB->IsEnabled()) {
            // do the callback with the current selection state
            callbackObject->ShowHideCallbackFunction(*itemsEnabled);
            applyB->Enable(false);
            cancelB->Enable(false);
        }

        if (event.GetId() == B_DONE)
            EndModal(wxOK);
    }
}

void ShowHideDialog::OnCloseWindow(wxCommandEvent& event)
{
    EndModal(wxOK);
}

END_SCOPE(Cn3D)

