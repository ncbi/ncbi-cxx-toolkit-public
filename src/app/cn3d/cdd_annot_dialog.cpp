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
*      dialogs for annotating CDD's
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/07/12 17:35:15  thiessen
* change domain mapping ; add preliminary cdd annotation GUI
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include "cn3d/cdd_annot_dialog.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/messenger.hpp"


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ header from cdd_annot_dialog.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

#include <wx/image.h>
#include <wx/statline.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <wx/notebook.h>

// Declare window functions

#define ID_L_ANNOT 10000
#define ID_B_NEW_ANNOT 10001
#define ID_B_DEL_ANNOT 10002
#define ID_B_RENAME 10003
#define ID_B_HIGHLIGHT 10004
#define ID_L_EVID 10005
#define ID_B_NEW_EVID 10006
#define ID_B_DEL_EVID 10007
#define ID_B_EDIT 10008
#define ID_B_LAUNCH 10009
#define ID_B_DONE 10010
#define ID_B_CANCEL 10011
wxSizer *SetupCDDAnnotDialog( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

#define ID_R_COMMENT 10012
#define ID_ST_COMMENT 10013
#define ID_T_COMMENT 10014
#define ID_LINE 10015
#define ID_R_PMID 10016
#define ID_ST_PMID 10017
#define ID_T_PMID 10018
#define ID_B_EDIT_OK 10019
#define ID_B_EDIT_CANCEL 10020
wxSizer *SetupEvidenceDialog( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

////////////////////////////////////////////////////////////////////////////////////////////////

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

#define DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERR_POST(Error << "Can't find window with id " << id); \
        return; \
    }

BEGIN_EVENT_TABLE(CDDAnnotateDialog, wxDialog)
    EVT_CLOSE       (       CDDAnnotateDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    CDDAnnotateDialog::OnButton)
    EVT_LISTBOX     (-1,    CDDAnnotateDialog::OnSelection)
END_EVENT_TABLE()

CDDAnnotateDialog::CDDAnnotateDialog(wxWindow *parent, StructureSet *set) :
    wxDialog(parent, -1, "CDD Annotations", wxPoint(400, 100), wxDefaultSize,
        wxCAPTION | wxSYSTEM_MENU), // not resizable
    structureSet(set), annotSet(set->GetCopyOfCDDAnnotSet()), changed(false)
{
    // construct the panel
    wxSizer *topSizer = SetupCDDAnnotDialog(this, false);

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->SetSizeHints(this);
}

// same as hitting done button
void CDDAnnotateDialog::OnCloseWindow(wxCommandEvent& event)
{
    // set data in structureSet if changed
    if (changed)
        structureSet->SetCDDAnnotSet((annotSet->Get().size() > 0) ? annotSet.GetPointer() : NULL);

    // close dialog
    EndModal(wxOK);
}

void CDDAnnotateDialog::OnButton(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case ID_B_DONE:
            OnCloseWindow(event);   // handle on-exit stuff there
            break;
        case ID_B_CANCEL:
            EndModal(wxCANCEL);
            break;

        // annotation buttons
        case ID_B_NEW_ANNOT: {
            break;
        }

        // evidence buttons
        case ID_B_NEW_EVID: {
            CDDEvidenceDialog dialog(this);
            dialog.ShowModal();
            break;
        }

        default:
            event.Skip();
    }
}

void CDDAnnotateDialog::OnSelection(wxCommandEvent& event)
{
}


BEGIN_EVENT_TABLE(CDDEvidenceDialog, wxDialog)
    EVT_CLOSE       (       CDDEvidenceDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    CDDEvidenceDialog::OnButton)
    EVT_RADIOBUTTON (-1,    CDDEvidenceDialog::OnSelection)
END_EVENT_TABLE()

CDDEvidenceDialog::CDDEvidenceDialog(wxWindow *parent) :
    wxDialog(parent, -1, "CDD Annotations", wxPoint(400, 100), wxDefaultSize,
        wxCAPTION | wxSYSTEM_MENU), // not resizable
    changed(false)
{
    // construct the panel
    wxSizer *topSizer = SetupEvidenceDialog(this, false);

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->SetSizeHints(this);
}

// same as hitting cancel button
void CDDEvidenceDialog::OnCloseWindow(wxCommandEvent& event)
{
    EndModal(wxCANCEL);
}

void CDDEvidenceDialog::OnButton(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case ID_B_EDIT_OK:
            EndModal(wxOK);
            break;
        case ID_B_EDIT_CANCEL:
            EndModal(wxCANCEL);
            break;
        default:
            event.Skip();
    }
}

void CDDEvidenceDialog::OnSelection(wxCommandEvent& event)
{
}

bool CDDEvidenceDialog::GetData(ncbi::objects::CFeature_evidence *evidence) const
{
    return true;
}

END_SCOPE(Cn3D)


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ code from cdd_annot_dialog.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *SetupCDDAnnotDialog( wxPanel *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxFlexGridSizer *item1 = new wxFlexGridSizer( 1, 0, 0, 0 );

    wxStaticBox *item3 = new wxStaticBox( parent, -1, "Annotations" );
    wxStaticBoxSizer *item2 = new wxStaticBoxSizer( item3, wxVERTICAL );

    wxString *strs4 = (wxString*) NULL;
    wxListBox *item4 = new wxListBox( parent, ID_L_ANNOT, wxDefaultPosition, wxSize(-1,100), 0, strs4, wxLB_SINGLE );
    item2->Add( item4, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxGridSizer *item5 = new wxGridSizer( 2, 0, 0 );

    wxButton *item6 = new wxButton( parent, ID_B_NEW_ANNOT, "New", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item6, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item7 = new wxButton( parent, ID_B_DEL_ANNOT, "Delete", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item7, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item8 = new wxButton( parent, ID_B_RENAME, "Rename", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item8, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item9 = new wxButton( parent, ID_B_HIGHLIGHT, "Highlight", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item9, 0, wxALIGN_CENTRE|wxALL, 5 );

    item2->Add( item5, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item2, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticBox *item11 = new wxStaticBox( parent, -1, "Evidence" );
    wxStaticBoxSizer *item10 = new wxStaticBoxSizer( item11, wxVERTICAL );

    wxString *strs12 = (wxString*) NULL;
    wxListBox *item12 = new wxListBox( parent, ID_L_EVID, wxDefaultPosition, wxSize(-1,100), 0, strs12, wxLB_SINGLE );
    item10->Add( item12, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxGridSizer *item13 = new wxGridSizer( 2, 0, 0 );

    wxButton *item14 = new wxButton( parent, ID_B_NEW_EVID, "New", wxDefaultPosition, wxDefaultSize, 0 );
    item13->Add( item14, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item15 = new wxButton( parent, ID_B_DEL_EVID, "Delete", wxDefaultPosition, wxDefaultSize, 0 );
    item13->Add( item15, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item16 = new wxButton( parent, ID_B_EDIT, "Edit", wxDefaultPosition, wxDefaultSize, 0 );
    item13->Add( item16, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item17 = new wxButton( parent, ID_B_LAUNCH, "Launch", wxDefaultPosition, wxDefaultSize, 0 );
    item13->Add( item17, 0, wxALIGN_CENTRE|wxALL, 5 );

    item10->Add( item13, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item10, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item1, 0, wxALIGN_CENTRE, 5 );

    wxBoxSizer *item18 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item19 = new wxButton( parent, ID_B_DONE, "Done", wxDefaultPosition, wxDefaultSize, 0 );
    item19->SetDefault();
    item18->Add( item19, 0, wxALIGN_CENTRE|wxALL, 5 );

    item18->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item20 = new wxButton( parent, ID_B_CANCEL, "Cancel", wxDefaultPosition, wxDefaultSize, 0 );
    item18->Add( item20, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item18, 0, wxALIGN_CENTRE|wxALL, 5 );

    if (set_sizer)
    {
        parent->SetAutoLayout( TRUE );
        parent->SetSizer( item0 );
        if (call_fit)
        {
            item0->Fit( parent );
            item0->SetSizeHints( parent );
        }
    }

    return item0;
}

wxSizer *SetupEvidenceDialog( wxPanel *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, "Evidence Options" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxFlexGridSizer *item3 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item3->AddGrowableCol( 2 );

    wxRadioButton *item4 = new wxRadioButton( parent, ID_R_COMMENT, "", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item4, 0, wxALIGN_BOTTOM|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    wxStaticText *item5 = new wxStaticText( parent, ID_ST_COMMENT, "Comment:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item5, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxTextCtrl *item6 = new wxTextCtrl( parent, ID_T_COMMENT, "", wxDefaultPosition, wxSize(150,-1), 0 );
    item3->Add( item6, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item1->Add( item3, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    wxStaticLine *item7 = new wxStaticLine( parent, ID_LINE, wxDefaultPosition, wxSize(20,-1), wxLI_HORIZONTAL );
    item1->Add( item7, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item8 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item8->AddGrowableCol( 2 );

    wxRadioButton *item9 = new wxRadioButton( parent, ID_R_PMID, "", wxDefaultPosition, wxDefaultSize, 0 );
    item8->Add( item9, 0, wxALIGN_BOTTOM|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    wxStaticText *item10 = new wxStaticText( parent, ID_ST_PMID, "Reference (PubMed ID):", wxDefaultPosition, wxDefaultSize, 0 );
    item8->Add( item10, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxTextCtrl *item11 = new wxTextCtrl( parent, ID_T_PMID, "", wxDefaultPosition, wxDefaultSize, 0 );
    item8->Add( item11, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item1->Add( item8, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    item0->Add( item1, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item12 = new wxFlexGridSizer( 1, 0, 0, 0 );

    wxButton *item13 = new wxButton( parent, ID_B_EDIT_OK, "OK", wxDefaultPosition, wxDefaultSize, 0 );
    item13->SetDefault();
    item12->Add( item13, 0, wxALIGN_CENTRE|wxALL, 5 );

    item12->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item14 = new wxButton( parent, ID_B_EDIT_CANCEL, "Cancel", wxDefaultPosition, wxDefaultSize, 0 );
    item12->Add( item14, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item12, 0, wxALIGN_CENTRE|wxALL, 5 );

    if (set_sizer)
    {
        parent->SetAutoLayout( TRUE );
        parent->SetSizer( item0 );
        if (call_fit)
        {
            item0->Fit( parent );
            item0->SetSizeHints( parent );
        }
    }

    return item0;
}

