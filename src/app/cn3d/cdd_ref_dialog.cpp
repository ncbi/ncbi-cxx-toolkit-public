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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/cdd/Cdd_descr.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/PubMedId.hpp>

#include "remove_header_conflicts.hpp"

#include "cdd_ref_dialog.hpp"
#include "structure_set.hpp"
#include "cn3d_tools.hpp"

#include <wx/tokenzr.h>

////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ header from cdd_ref_dialog.wdr
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
#define ID_B_LAUNCH 10001
#define ID_B_UP 10002
#define ID_B_EDIT 10003
#define ID_B_DOWN 10004
#define ID_B_ADD 10005
#define ID_B_DELETE 10006
#define ID_B_DONE 10007
wxSizer *SetupReferencesDialog( wxWindow *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

////////////////////////////////////////////////////////////////////////////////////////////////

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

#define DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERRORMSG("Can't find window with id " << id); \
        return; \
    }

BEGIN_EVENT_TABLE(CDDRefDialog, wxDialog)
    EVT_CLOSE       (       CDDRefDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    CDDRefDialog::OnButton)
END_EVENT_TABLE()

CDDRefDialog::CDDRefDialog(StructureSet *structureSet, CDDRefDialog **handle,
    wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos) :
        wxDialog(parent, id, title, pos, wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
        sSet(structureSet), dialogHandle(handle), selectItem(0)
{
    if (!structureSet || !(descrSet = structureSet->GetCDDDescrSet())) {
        ERRORMSG("CDDRefDialog::CDDRefDialog() - error getting descr set data");
        Destroy();
        return;
    }

    // construct the dialog
    wxSizer *topSizer = SetupReferencesDialog(this, false);

    // fill in list box with available references
    ResetListBox();

    bool readOnly;
    RegistryGetBoolean(REG_ADVANCED_SECTION, REG_CDD_ANNOT_READONLY, &readOnly);
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bAdd, ID_B_ADD, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bEdit, ID_B_EDIT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bDelete, ID_B_DELETE, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bUp, ID_B_UP, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bDown, ID_B_DOWN, wxButton)
    bAdd->Enable(!readOnly);
    bEdit->Enable(!readOnly);
    bDelete->Enable(!readOnly);
    bUp->Enable(!readOnly);
    bDown->Enable(!readOnly);

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->SetSizeHints(this);
}

CDDRefDialog::~CDDRefDialog(void)
{
    // so owner knows that this dialog has been destroyed
    if (dialogHandle && *dialogHandle) *dialogHandle = NULL;
    TRACEMSG("CDD references dialog destroyed");
}

// same as hitting done
void CDDRefDialog::OnCloseWindow(wxCloseEvent& event)
{
    Destroy();
}

void CDDRefDialog::OnButton(wxCommandEvent& event)
{
    // get listbox and descr from selected item
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(listbox, ID_L_REFS, wxListBox)
    CCdd_descr *descr = NULL;
    wxArrayInt selections;
    int nSelected = listbox->GetSelections(selections);
    selectItem = (nSelected > 0) ? selections.Item(selections.GetCount() - 1) : -1;
    if (selectItem >= 0 && selectItem < (int) listbox->GetCount())    // pointer to last descr selected
        descr = dynamic_cast<CCdd_descr*>(reinterpret_cast<CObject*>(listbox->GetClientData(selectItem)));

    // launch URL(s) given PMID
    if (event.GetId() == ID_B_LAUNCH && nSelected > 0) {
        string url("https://www.ncbi.nlm.nih.gov/pubmed/");
        for (int i=0; i<nSelected; ++i) {
            descr = dynamic_cast<CCdd_descr*>(reinterpret_cast<CObject*>(listbox->GetClientData(selections.Item(i))));
            if (i > 0)
                url += ",";
            url += NStr::IntToString(descr->GetReference().GetPmid().Get());
        }
        LaunchWebPage(url.c_str());
    }

    // add reference(s)
    else if (event.GetId() == ID_B_ADD) {
        wxString ids = wxGetTextFromUser("Enter a list of PubMed IDs:", "Input PMIDs", "", this);
        wxStringTokenizer tkz(ids, " ,;\t\r\n", wxTOKEN_STRTOK);
        long pmidVal;
        while (tkz.HasMoreTokens()) {
            wxString id = tkz.GetNextToken();
            if (id.size() > 0 && id.ToLong(&pmidVal) && pmidVal > 0) {
                CRef < CCdd_descr > ref(new CCdd_descr());
                ref->SetReference().SetPmid().Set((int) pmidVal);
                CCdd_descr_set::Tdata::iterator d, de = descrSet->Set().end();
                for (d=descrSet->Set().begin(); d!=de; ++d) {
                    if (d->GetPointer() == descr) {
                        ++d;
                        break;
                    }
                }
                descrSet->Set().insert(d, ref);
                sSet->SetDataChanged(StructureSet::eCDDData);
                ++selectItem;
                ResetListBox();
            } else
                ERRORMSG("Invalid PMID: '" << id.c_str() << "'");
        }
    }

    // edit reference
    else if (event.GetId() == ID_B_EDIT && nSelected == 1) {
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

    // delete reference(s)
    else if (event.GetId() == ID_B_DELETE && nSelected > 0) {
        CCdd_descr_set keepDescrs;
        CCdd_descr_set::Tdata::iterator d, de = descrSet->Set().end();
        for (d=descrSet->Set().begin(); d!=de; ++d) {
            int i;
            for (i=0; i<nSelected; ++i)
                if (d->GetPointer() ==
                        dynamic_cast<CCdd_descr*>(reinterpret_cast<CObject*>(listbox->GetClientData(selections.Item(i)))))
                    break;
            if (i == nSelected) // this descr is not one of those selected in the listbox
                keepDescrs.Set().push_back(*d);
        }
        if (keepDescrs.Get().size() != descrSet->Get().size()) {
            descrSet->Set() = keepDescrs.Set(); // copy list
            sSet->SetDataChanged(StructureSet::eCDDData);
            ResetListBox();
        }
    }

    else if ((event.GetId() == ID_B_UP || event.GetId() == ID_B_DOWN) && nSelected == 1) {
        CCdd_descr_set::Tdata::iterator d, de = descrSet->Set().end(), p = descrSet->Set().end(), n;
        for (d=descrSet->Set().begin(); d!=de; ++d) {
            if (d->GetPointer() == descr) {
                CRef < CCdd_descr > tmp(*d);
                n = d;
                do {        // find next pmid ref
                    ++n;
                } while (n != descrSet->Set().end() && !((*n)->IsReference() && (*n)->GetReference().IsPmid()));
                if (event.GetId() == ID_B_DOWN && n != descrSet->Set().end()) {
                    *d = *n;
                    *n = tmp;
                    ++selectItem;
                } else if (event.GetId() == ID_B_UP && p != descrSet->Set().end()) {
                    *d = *p;
                    *p = tmp;
                    --selectItem;
                } else
                    break;
                sSet->SetDataChanged(StructureSet::eCDDData);
                ResetListBox();
                break;
            }
            if ((*d)->IsReference() && (*d)->GetReference().IsPmid())
                p = d;      // keep prev pmid ref
        }
    }

    else if (event.GetId() == ID_B_DONE) {
        Destroy();
    }
}

void CDDRefDialog::ResetListBox(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(listbox, ID_L_REFS, wxListBox)

    listbox->Clear();
    CCdd_descr_set::Tdata::iterator d, de = descrSet->Set().end();
    for (d=descrSet->Set().begin(); d!=de; ++d) {
        if ((*d)->IsReference() && (*d)->GetReference().IsPmid()) {
            wxString title;
            title.Printf("%i", (*d)->GetReference().GetPmid().Get());
            // make client data of menu item a pointer to the CCdd_descr object
            listbox->Append(title, d->GetPointer());
        }
    }
    if (selectItem >= 0 && selectItem < (int) listbox->GetCount())
        listbox->SetSelection(selectItem, true);
}

END_SCOPE(Cn3D)

//////////////////////////////////////////////////////////////////////////////////////////////////
// The following are taken *without* modification from wxDesigner C++ code generated from
// cdd_ref_dialog.wdr.
//////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *SetupReferencesDialog( wxWindow *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, wxT("PubMed IDs") );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxFlexGridSizer *item3 = new wxFlexGridSizer( 1, 0, 0 );

    wxString *strs4 = (wxString*) NULL;
    wxListBox *item4 = new wxListBox( parent, ID_L_REFS, wxDefaultPosition, wxSize(80,100), 0, strs4, wxLB_EXTENDED|wxLB_NEEDED_SB );
    item3->Add( item4, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxGridSizer *item5 = new wxGridSizer( 2, 0, 0 );

    wxButton *item6 = new wxButton( parent, ID_B_LAUNCH, wxT("Launch"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item6, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item7 = new wxButton( parent, ID_B_UP, wxT("Move Up"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item7, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item8 = new wxButton( parent, ID_B_EDIT, wxT("Edit"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item8, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item9 = new wxButton( parent, ID_B_DOWN, wxT("Move Down"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item9, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item10 = new wxButton( parent, ID_B_ADD, wxT("Add"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item10, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item11 = new wxButton( parent, ID_B_DELETE, wxT("Delete"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item11, 0, wxALIGN_CENTRE|wxALL, 5 );

    item3->Add( item5, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item3, 0, wxALIGN_CENTRE, 5 );

    item0->Add( item1, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxBoxSizer *item12 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item13 = new wxButton( parent, ID_B_DONE, wxT("Done"), wxDefaultPosition, wxDefaultSize, 0 );
    item12->Add( item13, 0, wxALIGN_CENTRE|wxALL, 5 );

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
