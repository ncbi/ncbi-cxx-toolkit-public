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
*       dialog for editing CDD book references
*
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/cdd/Cdd_descr.hpp>
#include <objects/cdd/Cdd_book_ref.hpp>

#include "cdd_book_ref_dialog.hpp"
#include "structure_set.hpp"
#include "cn3d_tools.hpp"

#include <wx/clipbrd.h>
#include <wx/tokenzr.h>


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ header from cdd_book_ref_dialog.wdr
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

#define ID_LISTBOX 10000
#define ID_B_UP 10001
#define ID_B_LAUNCH 10002
#define ID_B_NEW 10003
#define ID_B_SAVE 10004
#define ID_B_DOWN 10005
#define ID_B_EDIT 10006
#define ID_B_PASTE 10007
#define ID_B_DELETE 10008
#define ID_LINE 10009
#define ID_TEXT 10010
#define ID_T_NAME 10011
#define ID_C_TYPE 10012
#define ID_T_ADDRESS 10013
#define ID_T_SUBADDRESS 10014
#define ID_B_DONE 10015
wxSizer *SetupBookRefDialog( wxWindow *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

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

template < class T >
class TypeStringAssociator
{
private:
    typename std::map < T , std::string > type2string;
    typename std::map < std::string , T > string2type;
public:
    void Associate(const T& type, const std::string& name)
    {
        type2string[type] = name;
        string2type[name] = type;
    }
    const T * Find(const std::string& name) const
    {
        std::map < std::string , T >::const_iterator i = string2type.find(name);
        return ((i != string2type.end()) ? &(i->second) : NULL);
    }
    bool Get(const std::string& name, T *type) const
    {
        const T *found = Find(name);
        if (found) *type = *found;
        return (found != NULL);
    }
    const std::string * Find(const T& type) const
    {
        std::map < T , std::string >::const_iterator i = type2string.find(type);
        return ((i != type2string.end()) ? &(i->second) : NULL);
    }
    bool Get(const T& type, std::string *name) const
    {
        const std::string *found = Find(type);
        if (found) *name = *found;
        return (found != NULL);
    }
    int Size(void) const { return type2string.size(); }
};

BEGIN_EVENT_TABLE(CDDBookRefDialog, wxDialog)
    EVT_CLOSE       (       CDDBookRefDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    CDDBookRefDialog::OnClick)
    EVT_LISTBOX     (-1,    CDDBookRefDialog::OnClick)
END_EVENT_TABLE()

static TypeStringAssociator < CCdd_book_ref::ETextelement > enum2str;

CDDBookRefDialog::CDDBookRefDialog(StructureSet *structureSet, CDDBookRefDialog **handle,
    wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos) :
        wxDialog(parent, id, title, pos, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
        sSet(structureSet), dialogHandle(handle), selectedItem(-1), editOn(false)
{
    if (enum2str.Size() == 0) {
        enum2str.Associate(CCdd_book_ref::eTextelement_unassigned, "unassigned");
        enum2str.Associate(CCdd_book_ref::eTextelement_section, "section");
        enum2str.Associate(CCdd_book_ref::eTextelement_figgrp, "figgrp");
        enum2str.Associate(CCdd_book_ref::eTextelement_table, "table");
        enum2str.Associate(CCdd_book_ref::eTextelement_chapter, "chapter");
        enum2str.Associate(CCdd_book_ref::eTextelement_biblist, "biblist");
        enum2str.Associate(CCdd_book_ref::eTextelement_box, "box");
        enum2str.Associate(CCdd_book_ref::eTextelement_glossary, "glossary");
        enum2str.Associate(CCdd_book_ref::eTextelement_appendix, "appendix");
        enum2str.Associate(CCdd_book_ref::eTextelement_other, "other");
    }

    if (!structureSet || !(descrSet = structureSet->GetCDDDescrSet())) {
        ERRORMSG("CDDBookRefDialog::CDDBookRefDialog() - error getting descr set data");
        Destroy();
        return;
    }

    // construct the dialog
    wxSizer *topSizer = SetupBookRefDialog(this, false);

    // fill in list box with available references
    SetWidgetStates();

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->SetSizeHints(this);
}

CDDBookRefDialog::~CDDBookRefDialog(void)
{
    // so owner knows that this dialog has been destroyed
    if (dialogHandle && *dialogHandle) *dialogHandle = NULL;
    TRACEMSG("CDD book references dialog destroyed");
}

// same as hitting done
void CDDBookRefDialog::OnCloseWindow(wxCloseEvent& event)
{
    // see what user wants to do about unsaved edit
    if (editOn) {
        int option = wxYES_NO | wxYES_DEFAULT | wxICON_EXCLAMATION | wxCENTRE;
        if (event.CanVeto()) option |= wxCANCEL;
        wxMessageDialog dialog(NULL, "Do you want to save your edited item?", "", option);
        option = dialog.ShowModal();
        if (option == wxID_CANCEL) {
            event.Veto();
            return;
        }
        if (option == wxID_YES) {
            wxCommandEvent fake(wxEVT_COMMAND_BUTTON_CLICKED, ID_B_SAVE);
            OnClick(fake);
        }
    }
    Destroy();
}

static wxString MakeRID(const CCdd_book_ref& bref)
{
    wxString uid;
    if (bref.IsSetSubelementid())
        uid.Printf("%s.%s.%i#%i",
            bref.GetBookname().c_str(), enum2str.Find(bref.GetTextelement())->c_str(),
            bref.GetElementid(), bref.GetSubelementid());
    else
        uid.Printf("%s.%s.%i",
            bref.GetBookname().c_str(), enum2str.Find(bref.GetTextelement())->c_str(), bref.GetElementid());
    return uid;
}

void CDDBookRefDialog::SetWidgetStates(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(listbox, ID_LISTBOX, wxListBox)
    listbox->Clear();

    // set items in listbox
    CCdd_descr_set::Tdata::iterator d, de = descrSet->Set().end();
    for (d=descrSet->Set().begin(); d!=de; ++d) {
        if ((*d)->IsBook_ref()) {
            // make client data of menu item a pointer to the CRef containing the CCdd_descr object
            listbox->Append(MakeRID((*d)->GetBook_ref()), &(*d));
        }
    }
    if (selectedItem < 0 && listbox->GetCount() > 0)
        selectedItem = 0;

    // set info fields
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tName, ID_T_NAME, wxTextCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(cType, ID_C_TYPE, wxChoice)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tAddress, ID_T_ADDRESS, wxTextCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tSubaddress, ID_T_SUBADDRESS, wxTextCtrl)
    if (selectedItem >= 0 && selectedItem < listbox->GetCount()) {
        listbox->SetSelection(selectedItem, true);
        const CCdd_book_ref& bref =
            (*(reinterpret_cast<CRef<CCdd_descr>*>(listbox->GetClientData(selectedItem))))->GetBook_ref();
        tName->SetValue(bref.GetBookname().c_str());
        cType->SetStringSelection(enum2str.Find(bref.GetTextelement())->c_str());
        wxString s;
        s.Printf("%i", bref.GetElementid());
        tAddress->SetValue(s);
        if (bref.IsSetSubelementid())
            s.Printf("%i", bref.GetSubelementid());
        else
            s.Clear();
        tSubaddress->SetValue(s);
    } else {
        tName->Clear();
        cType->SetSelection(0);
        tAddress->Clear();
        tSubaddress->Clear();
        // special case for new but as yet unsaved item, always last item
        if (editOn && selectedItem == listbox->GetCount()) {
            listbox->Append(wxString("(new item - hit 'Save' when finished)"), (void *) NULL);
            listbox->SetSelection(selectedItem, true);
        }
    }


    // set widget states
    listbox->Enable(!editOn);
    tName->Enable(editOn);
    cType->Enable(editOn);
    tAddress->Enable(editOn);
    tSubaddress->Enable(editOn);
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bUp, ID_B_UP, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bDown, ID_B_DOWN, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bEdit, ID_B_EDIT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bSave, ID_B_SAVE, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bNew, ID_B_NEW, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bDelete, ID_B_DELETE, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bLaunch, ID_B_LAUNCH, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bPaste, ID_B_PASTE, wxButton)
    bool readOnly;
    RegistryGetBoolean(REG_ADVANCED_SECTION, REG_CDD_ANNOT_READONLY, &readOnly);
    bUp->Enable(!readOnly && !editOn && selectedItem > 0);
    bDown->Enable(!readOnly && !editOn && selectedItem < listbox->GetCount() - 1);
    bEdit->Enable(!readOnly && !editOn && selectedItem >= 0 && selectedItem < listbox->GetCount());
    bSave->Enable(!readOnly && editOn);
    bNew->Enable(!readOnly && !editOn);
    bDelete->Enable(!readOnly && selectedItem >= 0 && selectedItem < listbox->GetCount());
    bLaunch->Enable(!editOn);
    bPaste->Enable(!readOnly && !editOn);
}

void CDDBookRefDialog::OnClick(wxCommandEvent& event)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(listbox, ID_LISTBOX, wxListBox)
    CRef < CCdd_descr > *selDescr = reinterpret_cast<CRef<CCdd_descr>*>(listbox->GetClientData(selectedItem));

    if (event.GetId() == ID_B_DONE) {
        Close(false);
        return;
    }

    else if (event.GetId() == ID_B_LAUNCH) {
        if (selDescr) {
            wxString url;
            url.Printf("http://www.ncbi.nlm.nih.gov/books/bv.fcgi?rid=%s",
                MakeRID((*selDescr)->GetBook_ref()).c_str());
            LaunchWebPage(url.c_str());
        }
    }

    else if (event.GetId() == ID_B_UP || event.GetId() == ID_B_DOWN) {
        CRef < CCdd_descr > *switchWith = NULL;
        if (event.GetId() == ID_B_UP && selectedItem > 0) {
            switchWith = reinterpret_cast<CRef<CCdd_descr>*>(listbox->GetClientData(selectedItem - 1));
            --selectedItem;
        } else if (event.GetId() == ID_B_DOWN && selectedItem < listbox->GetCount() - 1){
            switchWith = reinterpret_cast<CRef<CCdd_descr>*>(listbox->GetClientData(selectedItem + 1));
            ++selectedItem;
        } else {
            WARNINGMSG("CDDBookRefDialog::OnClick() - invalid up/down request");
            return;
        }
        CRef < CCdd_descr > tmp = *switchWith;
        *switchWith = *selDescr;
        *selDescr = tmp;
        sSet->SetDataChanged(StructureSet::eCDDData);
    }


    else if (event.GetId() == ID_B_EDIT) {
        editOn = true;
    }

    else if (event.GetId() == ID_B_SAVE) {
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tName, ID_T_NAME, wxTextCtrl)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(cType, ID_C_TYPE, wxChoice)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tAddress, ID_T_ADDRESS, wxTextCtrl)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tSubaddress, ID_T_SUBADDRESS, wxTextCtrl)
        long address, subaddress;
        if (!tAddress->GetValue().ToLong(&address) ||
            (tSubaddress->GetValue().size() > 0 && !tSubaddress->GetValue().ToLong(&subaddress))) {
            ERRORMSG("Address (required) and sub-address (optional) must be integers");
            return;
        }
        CCdd_descr *descr;
        if (selDescr) {
            // editing existing item
            descr = selDescr->GetPointer();
        } else {
            // create new item
            CRef < CCdd_descr > newDescr(new CCdd_descr);
            descrSet->Set().push_back(newDescr);
            descr = newDescr.GetPointer();
        }
        descr->SetBook_ref().SetBookname(tName->GetValue().c_str());
        descr->SetBook_ref().SetTextelement(*(enum2str.Find(string(cType->GetStringSelection().c_str()))));
        descr->SetBook_ref().SetElementid((int) address);
        if (tSubaddress->GetValue().size() > 0)
            descr->SetBook_ref().SetSubelementid((int) subaddress);
        else
            descr->SetBook_ref().ResetSubelementid();
        sSet->SetDataChanged(StructureSet::eCDDData);
        editOn = false;
    }

    else if (event.GetId() == ID_B_NEW) {
        selectedItem = listbox->GetCount();
        editOn = true;
    }

    else if (event.GetId() == ID_B_PASTE) {
        if (wxTheClipboard->Open()) {
            if (wxTheClipboard->IsSupported(wxDF_TEXT)) {
                wxTextDataObject data;
                wxTheClipboard->GetData(data);
                wxStringTokenizer tkz(data.GetText(), ".#", wxTOKEN_STRTOK);
                if (tkz.CountTokens() == 3 || tkz.CountTokens() == 4) {
                    bool haveSubaddr = (tkz.CountTokens() == 4);
                    CRef < CCdd_descr > descr(new CCdd_descr);
                    descr->SetBook_ref().SetBookname(tkz.GetNextToken().c_str());
                    const CCdd_book_ref::ETextelement *type =
                        enum2str.Find(string(tkz.GetNextToken().c_str()));
                    long address, subaddress;
                    bool
                        gotAddr = tkz.GetNextToken().ToLong(&address),
                        gotSubaddr = haveSubaddr ? tkz.GetNextToken().ToLong(&subaddress) : false;
                    if (type && gotAddr && (!haveSubaddr || gotSubaddr)) {
                        descr->SetBook_ref().SetTextelement(*type);
                        descr->SetBook_ref().SetElementid((int) address);
                        if (haveSubaddr)
                            descr->SetBook_ref().SetSubelementid((int) subaddress);
                        else
                            descr->SetBook_ref().ResetSubelementid();
                        descrSet->Set().push_back(descr);
                        sSet->SetDataChanged(StructureSet::eCDDData);
                        selectedItem = listbox->GetCount();
                    }
                }
            }
            wxTheClipboard->Close();
        }
    }

    else if (event.GetId() == ID_B_DELETE) {
        if (selDescr) {
            CCdd_descr_set::Tdata::iterator d, de = descrSet->Set().end();
            for (d=descrSet->Set().begin(); d!=de; ++d) {
                if (&(*d) == selDescr) {
                    descrSet->Set().erase(d);
                    sSet->SetDataChanged(StructureSet::eCDDData);
                    if (selectedItem == listbox->GetCount() - 1)
                        --selectedItem;
                    break;
                }
            }
        } else
            --selectedItem;
        editOn = false;
    }

    else if (event.GetId() == ID_LISTBOX) {
        selectedItem = listbox->GetSelection();
    }

    SetWidgetStates();
}

END_SCOPE(Cn3D)

//////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken *without* modification from wxDesigner C++ code generated from
// cdd_book_ref_dialog.wdr.
//////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *SetupBookRefDialog( wxWindow *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, wxT("Book References") );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxString *strs3 = (wxString*) NULL;
    wxListBox *item3 = new wxListBox( parent, ID_LISTBOX, wxDefaultPosition, wxSize(80,100), 0, strs3, wxLB_SINGLE );
    item1->Add( item3, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxGridSizer *item4 = new wxGridSizer( 2, 0, 0, 0 );

    wxButton *item5 = new wxButton( parent, ID_B_UP, wxT("Move Up"), wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item5, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item6 = new wxButton( parent, ID_B_LAUNCH, wxT("Launch"), wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item6, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item7 = new wxButton( parent, ID_B_NEW, wxT("New"), wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item7, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item8 = new wxButton( parent, ID_B_SAVE, wxT("Save"), wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item8, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item9 = new wxButton( parent, ID_B_DOWN, wxT("Move Down"), wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item9, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item10 = new wxButton( parent, ID_B_EDIT, wxT("Edit"), wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item10, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item11 = new wxButton( parent, ID_B_PASTE, wxT("Paste"), wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item11, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item12 = new wxButton( parent, ID_B_DELETE, wxT("Delete"), wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item12, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item4, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxStaticLine *item13 = new wxStaticLine( parent, ID_LINE, wxDefaultPosition, wxSize(20,-1), wxLI_HORIZONTAL );
    item1->Add( item13, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item14 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item14->AddGrowableCol( 1 );

    wxStaticText *item15 = new wxStaticText( parent, ID_TEXT, wxT("Name:"), wxDefaultPosition, wxDefaultSize, 0 );
    item14->Add( item15, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxTextCtrl *item16 = new wxTextCtrl( parent, ID_T_NAME, wxT(""), wxDefaultPosition, wxSize(80,-1), 0 );
    item14->Add( item16, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxStaticText *item17 = new wxStaticText( parent, ID_TEXT, wxT("Type:"), wxDefaultPosition, wxDefaultSize, 0 );
    item14->Add( item17, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxString strs18[] =
    {
        wxT("unassigned"),
        wxT("section"),
        wxT("figgrp"),
        wxT("table"),
        wxT("chapter"),
        wxT("biblist"),
        wxT("box"),
        wxT("glossary"),
        wxT("appendix"),
        wxT("other")
    };
    wxChoice *item18 = new wxChoice( parent, ID_C_TYPE, wxDefaultPosition, wxSize(100,-1), 10, strs18, 0 );
    item14->Add( item18, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item14, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item19 = new wxFlexGridSizer( 1, 0, 0, 0 );

    wxStaticText *item20 = new wxStaticText( parent, ID_TEXT, wxT("Address:"), wxDefaultPosition, wxDefaultSize, 0 );
    item19->Add( item20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxTextCtrl *item21 = new wxTextCtrl( parent, ID_T_ADDRESS, wxT(""), wxDefaultPosition, wxSize(80,-1), 0 );
    item19->Add( item21, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item22 = new wxStaticText( parent, ID_TEXT, wxT("Sub-address:"), wxDefaultPosition, wxDefaultSize, 0 );
    item19->Add( item22, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxTextCtrl *item23 = new wxTextCtrl( parent, ID_T_SUBADDRESS, wxT(""), wxDefaultPosition, wxSize(80,-1), 0 );
    item19->Add( item23, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item19, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item1, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item24 = new wxButton( parent, ID_B_DONE, wxT("Done"), wxDefaultPosition, wxDefaultSize, 0 );
    item24->SetDefault();
    item0->Add( item24, 0, wxALIGN_CENTRE|wxALL, 5 );

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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2004/05/21 21:41:38  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.5  2004/03/15 17:27:02  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.4  2004/02/19 17:04:45  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.3  2003/09/29 23:34:05  thiessen
* add launch, paste buttons
*
* Revision 1.2  2003/09/26 17:39:14  thiessen
* fixes for button states
*
* Revision 1.1  2003/09/26 17:12:46  thiessen
* add book reference dialog
*
*/
