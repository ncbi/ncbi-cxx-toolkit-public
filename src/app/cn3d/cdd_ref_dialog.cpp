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
*       dialog for editing CDD references
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2001/12/06 23:13:44  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.2  2001/11/27 16:26:07  thiessen
* major update to data management system
*
* Revision 1.1  2001/10/09 18:57:04  thiessen
* add CDD references editing dialog
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include <objects/cdd/Cdd_descr.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/PubMedId.hpp>

#include "cn3d/cdd_ref_dialog.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/cn3d_tools.hpp"


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ header from annotate_dialog.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

#include <wx/image.h>
#include <wx/statline.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <wx/notebook.h>
#include <wx/grid.h>

// Declare window functions

#define ID_L_REFS 10000
#define ID_B_ADD 10001
#define ID_B_LAUNCH 10002
#define ID_B_EDIT 10003
#define ID_B_DELETE 10004
#define ID_B_DONE 10005
wxSizer *SetupReferencesDialog( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

////////////////////////////////////////////////////////////////////////////////////////////////

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

#define DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERR_POST(Error << "Can't find window with id " << id); \
        return; \
    }

BEGIN_EVENT_TABLE(CDDRefDialog, wxDialog)
    EVT_CLOSE       (       CDDRefDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    CDDRefDialog::OnButton)
END_EVENT_TABLE()

CDDRefDialog::CDDRefDialog(StructureSet *structureSet,
    wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos) :
        wxDialog(parent, id, title, pos, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
        sSet(structureSet)
{
    if (!structureSet || !(descrSet = structureSet->GetCDDDescrSet())) {
        ERR_POST(Error << "CDDRefDialog::CDDRefDialog() - error getting descr set data");
        Destroy();
        return;
    }

    // construct the panel
    wxPanel *panel = new wxPanel(this, -1);
    wxSizer *topSizer = SetupReferencesDialog(panel, false);

    // fill in list box with available references
    ResetListBox();

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->Fit(panel);
    topSizer->SetSizeHints(this);
}

// same as hitting done
void CDDRefDialog::OnCloseWindow(wxCloseEvent& event)
{
    EndModal(wxOK);
}

void CDDRefDialog::OnButton(wxCommandEvent& event)
{
    // get listbox and descr from selected item
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(listbox, ID_L_REFS, wxListBox)
    CCdd_descr *descr = NULL;
    if (listbox->GetSelection() >= 0 && listbox->GetSelection() < listbox->GetCount())
        descr = dynamic_cast<CCdd_descr*>(
            reinterpret_cast<CObject*>(listbox->GetClientData(listbox->GetSelection())));

    // launch URL given PMID
    if (event.GetId() == ID_B_LAUNCH && descr) {
        wxString url;
        url.Printf("http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?"
            "cmd=Retrieve&db=PubMed&dopt=Abstract&list_uids=%i",
            descr->GetReference().GetPmid().Get());
        LaunchWebPage(url);
    }

    // add reference
    else if (event.GetId() == ID_B_ADD) {
        wxString pmidStr = wxGetTextFromUser("Enter a PubMed ID:", "New PMID", "", this);
        unsigned long pmidVal;
        if (pmidStr.size() > 0 && pmidStr.ToULong(&pmidVal)) {
            CRef < CCdd_descr > ref(new CCdd_descr());
            ref->SetReference().SetPmid().Set((int) pmidVal);
            descrSet->Set().push_back(ref);
            sSet->SetDataChanged(StructureSet::eCDDData);
            ResetListBox();
        }
    }

    // edit reference
    else if (event.GetId() == ID_B_EDIT && descr) {
        wxString init;
        init.Printf("%i", descr->GetReference().GetPmid().Get());
        wxString pmidStr = wxGetTextFromUser("Enter/edit the PubMed ID:", "Edit PMID", init, this);
        unsigned long pmidVal;
        if (pmidStr.size() > 0 && pmidStr.ToULong(&pmidVal)) {
            descr->SetReference().SetPmid().Set((int) pmidVal);
            sSet->SetDataChanged(StructureSet::eCDDData);
            ResetListBox();
        }
    }

    // delete reference
    else if (event.GetId() == ID_B_DELETE && descr) {
        CCdd_descr_set::Tdata::iterator d, de = descrSet->Set().end();
        for (d=descrSet->Set().begin(); d!=de; d++) {
            if (d->GetPointer() == descr) {
                descrSet->Set().erase(d);
                sSet->SetDataChanged(StructureSet::eCDDData);
                ResetListBox();
                break;
            }
        }
    }

    else if (event.GetId() == ID_B_DONE) {
        EndModal(wxOK);
    }
}

void CDDRefDialog::ResetListBox(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(listbox, ID_L_REFS, wxListBox)

    listbox->Clear();
    CCdd_descr_set::Tdata::iterator d, de = descrSet->Set().end();
    for (d=descrSet->Set().begin(); d!=de; d++) {
        if ((*d)->IsReference() && (*d)->GetReference().IsPmid()) {
            wxString title;
            title.Printf("%i", (*d)->GetReference().GetPmid().Get());
            // make client data of menu item a pointer to the CCdd_descr object
            listbox->Append(title, d->GetPointer());
        }
    }
}

END_SCOPE(Cn3D)

//////////////////////////////////////////////////////////////////////////////////////////////////
// The following are taken *without* modification from wxDesigner C++ code generated from
// cdd_ref_dialog.wdr.
//////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *SetupReferencesDialog( wxPanel *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, "PubMed IDs" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxFlexGridSizer *item3 = new wxFlexGridSizer( 1, 0, 0 );

    wxString *strs4 = (wxString*) NULL;
    wxListBox *item4 = new wxListBox( parent, ID_L_REFS, wxDefaultPosition, wxSize(80,100), 0, strs4, wxLB_SINGLE|wxLB_NEEDED_SB );
    item3->Add( item4, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxGridSizer *item5 = new wxGridSizer( 2, 0, 0 );

    wxButton *item6 = new wxButton( parent, ID_B_ADD, "Add", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item6, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item7 = new wxButton( parent, ID_B_LAUNCH, "Launch", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item7, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item8 = new wxButton( parent, ID_B_EDIT, "Edit", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item8, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item9 = new wxButton( parent, ID_B_DELETE, "Delete", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item9, 0, wxALIGN_CENTRE|wxALL, 5 );

    item3->Add( item5, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item3, 0, wxALIGN_CENTRE, 5 );

    item0->Add( item1, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxBoxSizer *item10 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item11 = new wxButton( parent, ID_B_DONE, "Done", wxDefaultPosition, wxDefaultSize, 0 );
    item10->Add( item11, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item10, 0, wxALIGN_CENTRE|wxALL, 5 );

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

