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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <util/xregexp/regexp.hpp>
#include <util/ncbi_url.hpp>

#include <objects/cdd/Cdd_descr.hpp>
#include <objects/cdd/Cdd_book_ref.hpp>

#include "remove_header_conflicts.hpp"

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

BEGIN_EVENT_TABLE(CDDBookRefDialog, wxDialog)
    EVT_CLOSE       (       CDDBookRefDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    CDDBookRefDialog::OnClick)
    EVT_LISTBOX     (-1,    CDDBookRefDialog::OnClick)
END_EVENT_TABLE()

static TypeStringAssociator < CCdd_book_ref::ETextelement > enum2str;

CDDBookRefDialog::CDDBookRefDialog(StructureSet *structureSet, CDDBookRefDialog **handle,
    wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos) :
        wxDialog(parent, id, title, pos, wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
        sSet(structureSet), dialogHandle(handle), selectedItem(-1), editOn(false), isNew(false)
{
    if (enum2str.Size() == 0) {
        enum2str.Associate(CCdd_book_ref::eTextelement_unassigned, "unassigned");
        enum2str.Associate(CCdd_book_ref::eTextelement_section, "section");
        enum2str.Associate(CCdd_book_ref::eTextelement_figgrp, "figure");
        enum2str.Associate(CCdd_book_ref::eTextelement_table, "table");
        enum2str.Associate(CCdd_book_ref::eTextelement_chapter, "chapter");
        enum2str.Associate(CCdd_book_ref::eTextelement_biblist, "biblist");
        enum2str.Associate(CCdd_book_ref::eTextelement_box, "box");
        enum2str.Associate(CCdd_book_ref::eTextelement_glossary, "def-item");
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

//  Return true if 'bookname' starts with "NBK" (case-sensitive).
bool IsPortalDerivedBookRef(const CCdd_book_ref& bref)
{
    return (bref.GetBookname().substr(0, 3) == "NBK");
}

//  The return value is a string used in Portal style URLs based on 
//  .../books/NBK....  The only fields that should be present in 
//  book ref objects are the bookname, text-element type, and a
//  celementid.  Any other field in the spec is ignored.  Return an
//  empty string if there is any problem.
static wxString MakePortalParameterString(const CCdd_book_ref& bref)
{    
    string idStr, rendertype;
    string bookname = bref.GetBookname();
    CCdd_book_ref::ETextelement elementType = bref.GetTextelement();
    wxString paramStr;

    //  'idStr' should exist for everything except chapters.  However, 
    //  if we find a chapter with non-empty 'idStr' format it as a 
    //  section.  Conversely, format sections with no 'idStr' as a chapter.
    if (bref.IsSetCelementid()) {
        idStr = bref.GetCelementid();
    }

    if (elementType != CCdd_book_ref::eTextelement_chapter && elementType != CCdd_book_ref::eTextelement_section) {
        rendertype = *(enum2str.Find(elementType));
        paramStr.Printf("%s/%s/%s", bookname.c_str(), rendertype.c_str(), idStr.c_str());
    } else if (idStr.length() > 0) {
        paramStr.Printf("%s/#%s", bookname.c_str(), idStr.c_str());
    } else if (bookname.length() > 0) {
        paramStr.Printf("%s", bookname.c_str());
    }

    return paramStr;
}


//  The return value is a string used in "DTD2" style URLs based on br.fcgi.
//  However, note that the CCdd_book_ref could have been encoded based
//  on URLs from either br.fcgi or bv.fcgi and there's no way to deduce
//  from which service the data originated.
static wxString MakeBrParameterString(const CCdd_book_ref& bref)
{
    wxString paramStr;

    if (!bref.IsSetElementid() && !bref.IsSetCelementid())
        ERRORMSG("MakeBrParameterString() failed - neither elementid nor celementid is set");

    else if (bref.IsSetElementid() && bref.IsSetCelementid())
        ERRORMSG("MakeBrParameterString() failed - both elementid and celementid are set");

    else if (bref.IsSetSubelementid() && bref.IsSetCsubelementid())
        ERRORMSG("MakeBrParameterString() failed - both subelementid and csubelementid are set");

    else {
        CCdd_book_ref::ETextelement elementType;
        string part, id, rendertype;
        
        //  Numerical element ids need to be prefixed by 'A' in br.fcgi URLs.
        if (bref.IsSetElementid()) {
            part = "A" + NStr::IntToString(bref.GetElementid());
        } else {
            part = bref.GetCelementid();
        }

        if (bref.IsSetSubelementid() || bref.IsSetCsubelementid()) {
            //  Numerical subelement ids need to be prefixed by 'A' in br.fcgi URLs.
            if (bref.IsSetSubelementid()) {
                id = "A" + NStr::IntToString(bref.GetSubelementid());
            } else {
                id = bref.GetCsubelementid();
            }
        } else {
            id = kEmptyStr;
        }

        //  For br.fcgi, 'chapter' and 'section' are treated equivalently.
        //  Expect anything else to be encoded in the 'rendertype' attribute.
        elementType = bref.GetTextelement();
        if (elementType != CCdd_book_ref::eTextelement_chapter && elementType != CCdd_book_ref::eTextelement_section) {
            if (elementType == CCdd_book_ref::eTextelement_unassigned || elementType == CCdd_book_ref::eTextelement_other) {
                paramStr.Printf("%s&part=%s", bref.GetBookname().c_str(), part.c_str());
            } else {
                rendertype = *(enum2str.Find(elementType));
                if (id.length() == 0) {
                    id = part;
                }
                paramStr.Printf("%s&part=%s&rendertype=%s&id=%s",
                    bref.GetBookname().c_str(), part.c_str(), rendertype.c_str(), id.c_str());
            }
        } else {
            if (id.size() > 0)
                paramStr.Printf("%s&part=%s#%s",
                    bref.GetBookname().c_str(), part.c_str(), id.c_str());
            else
                paramStr.Printf("%s&part=%s",
                    bref.GetBookname().c_str(), part.c_str());
        }

    }

    return paramStr;
}

//  The 'rid' parameter is used in old "DTD1" style URLs based on bv.fcgi
/*static wxString MakeRID(const CCdd_book_ref& bref)
{
    wxString rid;

    if (!bref.IsSetElementid() && !bref.IsSetCelementid())
        ERRORMSG("MakeRID() failed - neither elementid nor celementid is set");

    else if (bref.IsSetElementid() && bref.IsSetCelementid())
        ERRORMSG("MakeRID() failed - both elementid and celementid are set");

    else if (bref.IsSetSubelementid() && bref.IsSetCsubelementid())
        ERRORMSG("MakeRID() failed - both subelementid and csubelementid are set");

    else {
        string
            elementid = (bref.IsSetElementid() ? NStr::IntToString(bref.GetElementid()) : bref.GetCelementid()),
            subelementid = ((bref.IsSetSubelementid() || bref.IsSetCsubelementid()) ?
                (bref.IsSetSubelementid() ? NStr::IntToString(bref.GetSubelementid()) : bref.GetCsubelementid()) : kEmptyStr);
        if (subelementid.size() > 0)
            rid.Printf("%s.%s.%s#%s",
                bref.GetBookname().c_str(), enum2str.Find(bref.GetTextelement())->c_str(),
                elementid.c_str(), subelementid.c_str());
        else
            rid.Printf("%s.%s.%s",
                bref.GetBookname().c_str(), enum2str.Find(bref.GetTextelement())->c_str(),
                elementid.c_str());
    }

    return rid;
}*/

void CDDBookRefDialog::SetWidgetStates(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(listbox, ID_LISTBOX, wxListBox)
    listbox->Clear();

    // set items in listbox
    CCdd_descr_set::Tdata::iterator d, de = descrSet->Set().end();
    for (d=descrSet->Set().begin(); d!=de; ++d) {
        if ((*d)->IsBook_ref()) {
            // make client data of menu item a pointer to the CRef containing the CCdd_descr object
            wxString descrText;
            if (IsPortalDerivedBookRef((*d)->GetBook_ref())) {
                descrText = MakePortalParameterString((*d)->GetBook_ref());
            } else {
                descrText = MakeBrParameterString((*d)->GetBook_ref());
            }
            if (descrText.size() > 0)
                listbox->Append(descrText, &(*d));
        }
    }
    if (selectedItem < 0 && listbox->GetCount() > 0)
        selectedItem = 0;

    // set info fields
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tName, ID_T_NAME, wxTextCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(cType, ID_C_TYPE, wxChoice)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tAddress, ID_T_ADDRESS, wxTextCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tSubaddress, ID_T_SUBADDRESS, wxTextCtrl)
    if (selectedItem >= 0 && selectedItem < (int) listbox->GetCount() && listbox->GetClientData(selectedItem) && !isNew) {
        listbox->SetSelection(selectedItem, true);
        const CCdd_book_ref& bref =
            (*(reinterpret_cast<CRef<CCdd_descr>*>(listbox->GetClientData(selectedItem))))->GetBook_ref();
        tName->SetValue(bref.GetBookname().c_str());
        cType->SetStringSelection(enum2str.Find(bref.GetTextelement())->c_str());

        //  Just display what's in the object, rather than prepend
        //  the 'A' to make everything look like a new br.fcgi URL
        //  (this way, it's obvious a numeric value corresponds to
        //   and book reference recorded prior to these changes).
        wxString s;
        if (bref.IsSetElementid())
            s.Printf("%i", bref.GetElementid());
        else
            s.Printf("%s", bref.GetCelementid().c_str());
        tAddress->SetValue(s);
        if (bref.IsSetSubelementid() || bref.IsSetCsubelementid()) {
            if (bref.IsSetSubelementid())
                s.Printf("%i", bref.GetSubelementid());
            else
                s.Printf("%s", bref.GetCsubelementid().c_str());
        } else
            s.Clear();
        tSubaddress->SetValue(s);
    } else {
        tName->Clear();
        cType->SetSelection(1);  // default is 'section'
        tAddress->Clear();
        tSubaddress->Clear();
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
    bDown->Enable(!readOnly && !editOn && selectedItem < (int) listbox->GetCount() - 1);
    bEdit->Enable(!readOnly && !editOn && selectedItem >= 0 && selectedItem < (int) listbox->GetCount());
    bSave->Enable(!readOnly && editOn);
    bNew->Enable(false);  //    bNew->Enable(!readOnly && !editOn);  //  see if anyone yells
    bDelete->Enable(!readOnly && selectedItem >= 0 && selectedItem < (int) listbox->GetCount());
    bLaunch->Enable(!editOn && (int) listbox->GetCount() > 0);
    bPaste->Enable(!readOnly && !editOn);
}

static void InsertAfter(int selectedItem, CCdd_descr_set *descrSet, CCdd_descr *descr)
{
    if (selectedItem < 0) {
        descrSet->Set().push_back(CRef < CCdd_descr >(descr));
        return;
    }

    CCdd_descr_set::Tdata::iterator d, de = descrSet->Set().end();
    int s = 0;
    for (d=descrSet->Set().begin(); d!=de; ++d) {
        if ((*d)->IsBook_ref()) {
            if (s == selectedItem) {
                ++d;
                descrSet->Set().insert(d, CRef < CCdd_descr >(descr));
                return;
            }
            ++s;
        }
    }
    ERRORMSG("InsertAfter() - selectedItem = " << selectedItem << " but there aren't that many book refs");
}

//  Break up URLs formatted as per br.fcgi conventions;
//  allow use of the entire URL.
//  
//  section/chapter:
//  book=<bookname>&part=<address>[#<subaddress>]
//
//  table/figure:
//  book=<bookname>&part=<address>&rendertype=<some_enum2strValue>&id=<subaddress>
//
//  For XML validation reasons, <address> and <subaddress> have a prepended 'A'
//  if the string would have otherwise been numeric.  However, for parsing br.fcgi 
//  URLs it is fine to leave the string intact w/o stripping off the initial 'A'
//  (i.e., all br.fcgi derived book references will use Celementid and Csubelementid
//   exclusively).
bool BrURLToBookRef(wxTextDataObject& data, CRef < CCdd_descr >& descr)
{
    bool result = false;
    string bookname, address, subaddress, typeStr, firstTokenStr;
    CRegexp regexpCommon("book=(.*)&part=(.*)");
    CRegexp regexpRendertype("&part=(.*)&rendertype=(.*)&id=(.*)");
    wxStringTokenizer sharp(data.GetText().Strip(wxString::both), "#", wxTOKEN_STRTOK);

    if (sharp.CountTokens() == 1 || sharp.CountTokens() == 2) {
        bool haveSubaddr = (sharp.CountTokens() == 2);
        wxString firstToken = sharp.GetNextToken();
//        wxStringTokenizer ampersand(firstToken, "&", wxTOKEN_STRTOK);
        
        //  All URLs have 'book' and 'part' parameters.
        firstTokenStr = firstToken.c_str();
        regexpCommon.GetMatch(firstTokenStr, 0, 0, CRegexp::fMatch_default, true);
        if (regexpCommon.NumFound() == 3) {  //  i.e., found full pattern + two subpatterns

            bookname = regexpCommon.GetSub(firstTokenStr, 1);

            regexpRendertype.GetMatch(firstTokenStr, 0, 0, CRegexp::fMatch_default, true);
            if (regexpRendertype.NumFound() == 4) {  //  i.e., expecting anything but a chapter or section 
                address = regexpRendertype.GetSub(firstTokenStr, 1);
                typeStr = regexpRendertype.GetSub(firstTokenStr, 2);
                if (enum2str.Find(typeStr) == NULL) {  
                    typeStr = kEmptyStr;  //  problem if we don't have a known type
                } else {
                    subaddress = regexpRendertype.GetSub(firstTokenStr, 3);
                }
            } else {  //  treat this as a 'section'
                address = regexpCommon.GetSub(firstTokenStr, 2);
                typeStr = *(enum2str.Find(CCdd_book_ref::eTextelement_section));

                //  If there's something after the '#', if it's an old-style
                //  URL it could be numeric -> prepend 'A' in that case.
                if (haveSubaddr) {
                    subaddress = sharp.GetNextToken().c_str();
                    if (NStr::StringToULong(subaddress, NStr::fConvErr_NoThrow) != 0) {
                        subaddress = "A" + subaddress;
                    }
                }
            }
        }

        if (typeStr.length() > 0) {
            const CCdd_book_ref::ETextelement *type = enum2str.Find(typeStr);
            if (type) {
                descr->SetBook_ref().SetBookname(bookname);
                descr->SetBook_ref().SetTextelement(*type);
                descr->SetBook_ref().SetCelementid(address);
                if (subaddress.length() > 0) {
                    descr->SetBook_ref().SetCsubelementid(subaddress);
                }
                result = true;
            }
        }
    }

    if (!result) {
        descr.Reset();
    }
    return result;
}

//  Break up URLs formatted as per Bookshelf URL scheme released 2010;
//  allow use of the entire URL.
//  
//  section/chapter:
//  books/<bookname>/#<elementid>
//
//  table/figure/box/glossary item:
//  books/<bookname>/<elementtype>/<elementid>/
//      -- OR --
//  books/<bookname>/<elementtype>/<elementid>/?report=objectonly
//      -- OR --
//  books/<bookname>/?rendertype=<elementtype>&id=<elementid>
//
//  The 'bookname' is the prefix 'NBK' plus an "article ID" - usually numeric but
//  allow for non-numeric values to be safe.  The 'elementid' is a string typically
//  starting with 'A' if the remainder of the elementid is numeric.  However, in rare
//  cases the elementid may have a different format for certain element types.
//  For 'chapter' legacy book refs, or pages refering to an entire page vs. a specific
//  location in a bookshelf document, the elementid may be undefined.
//  Redirection by Entrez can generate the alternate 'rendertype' URL format
//  for figures, tables, boxes, and glossary items (the latter being a 'def-item').
//  All derived book references will exclusively use the Celementid CCdd_book_ref field; 
//  the Csubelementid field no longer appears necessary in this URL scheme.
bool BookURLToBookRef(wxTextDataObject& data, CRef < CCdd_descr >& descr)
{
    bool result = false;
    if (descr.Empty()) return result;

    //  remove leading/trailing whitespace, and the trailing '/' if present.
    string inputStr = data.GetText().Trim(true).Trim(false).c_str();
//    if (NStr::EndsWith(inputStr, '/')) { 
//        inputStr = inputStr.substr(0, inputStr.length() - 1);
//    }

    string baseStr, nbkCode, idStr, typeStr;
    CRegexp regexpBase("/books/(NBK.+)");
    CRegexp regexpNBK("^(NBK[^/]+)");
    CRegexp regexpRendertype("^NBK[^/]+/(.+)/(.+)");

    CUrl url(inputStr);
    string urlPath = url.GetPath();
    string urlFrag = url.GetFragment();  //  fragment is text after trailing '#'
    CUrlArgs& urlArgs = url.GetArgs();

    //  may have a trailing '/' in the '.../?report=objectonly' form of URLs,
    if (NStr::EndsWith(urlPath, '/')) { 
        urlPath = urlPath.substr(0, urlPath.length() - 1);
    }

    regexpBase.GetMatch(urlPath, 0, 0, CRegexp::fMatch_default, true);
    if (regexpBase.NumFound() == 2) {  //  i.e., found full pattern + subpattern

        baseStr = regexpBase.GetSub(urlPath, 1);
        nbkCode = regexpNBK.GetMatch(baseStr);

        regexpRendertype.GetMatch(baseStr, 0, 0, CRegexp::fMatch_default, true);        
        if (regexpRendertype.NumFound() == 3) {  //  i.e., full pattern + two subpatterns
            typeStr = regexpRendertype.GetSub(baseStr, 1);
            idStr = regexpRendertype.GetSub(baseStr, 2);
        } else if (urlArgs.IsSetValue("rendertype") && urlArgs.IsSetValue("id")) {
            //  If the user somehow pasted a redirected br.fcgi URL.
            typeStr = urlArgs.GetValue("rendertype");
            idStr = urlArgs.GetValue("id");
        } else if (urlFrag.length() > 0) {
            //  A section id appears after the hash character.
            typeStr = "section";
            idStr = urlFrag;
        } else if (urlFrag.length() == 0) {
            //  If there is no URL fragment or obvious type, designate it 
            //  a chapter and point to the top of this book page.
            typeStr = "chapter";
            idStr = kEmptyStr;
        }
    }

    if (nbkCode.length() > 0) {

        if (typeStr.length() > 0) {
            const CCdd_book_ref::ETextelement *type = enum2str.Find(typeStr);
            if (type) {
                descr->SetBook_ref().SetBookname(nbkCode);
                descr->SetBook_ref().SetTextelement(*type);
                descr->SetBook_ref().SetCelementid(idStr);
//                if (subaddress.length() > 0) {
//                    descr->SetBook_ref().SetCsubelementid(subaddress);
//                }
                result = true;
            }
        }
    }

//    if (!result) {
//        descr.Reset();
//    }
    return result;
}

void CDDBookRefDialog::OnClick(wxCommandEvent& event)
{
    int eventId = event.GetId();
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(listbox, ID_LISTBOX, wxListBox)
    CRef < CCdd_descr > *selDescr = NULL;

    //  Avoid some unnecessary warning in debug-mode
    if ((eventId != ID_B_PASTE) && (eventId != ID_B_DONE) && (eventId != ID_B_EDIT) && (eventId != ID_B_NEW) && (eventId != ID_LISTBOX)) {
        selDescr = reinterpret_cast<CRef<CCdd_descr>*>(listbox->GetClientData(selectedItem));
    }

    if (event.GetId() == ID_B_DONE) {
        Close(false);
        return;
    }

    else if (event.GetId() == ID_B_LAUNCH) {
        if (selDescr) {
            //  If the book reference was derived from a Portal URL (i.e., the
            //  bookname has the 'NBK' prefix), launch with a Portal URL.
            //  Since book reference data stored in a CD may have been parsed
            //  from br.fcgi URLs, launch with a br.fcgi URL in such cases as
            //  Entrez will redirect those links to the corresponding Portal-ized URLs.
            wxString url;
            if (IsPortalDerivedBookRef((*selDescr)->GetBook_ref())) {
                url.Printf("http://www.ncbi.nlm.nih.gov/books/%s",
                    MakePortalParameterString((*selDescr)->GetBook_ref()).c_str());
            } else {
                url.Printf("http://www.ncbi.nlm.nih.gov/bookshelf/br.fcgi?book=%s",
                    MakeBrParameterString((*selDescr)->GetBook_ref()).c_str());
            }
//            url.Printf("http://www.ncbi.nlm.nih.gov/books/bv.fcgi?rid=%s",
//                MakeRID((*selDescr)->GetBook_ref()).c_str());
            LaunchWebPage(url.c_str());
        }
    }

    else if (event.GetId() == ID_B_UP || event.GetId() == ID_B_DOWN) {
        CRef < CCdd_descr > *switchWith = NULL;
        if (event.GetId() == ID_B_UP && selectedItem > 0) {
            switchWith = reinterpret_cast<CRef<CCdd_descr>*>(listbox->GetClientData(selectedItem - 1));
            --selectedItem;
        } else if (event.GetId() == ID_B_DOWN && selectedItem < (int) listbox->GetCount() - 1){
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
        isNew = false;
    }

    else if (event.GetId() == ID_B_SAVE) {
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tName, ID_T_NAME, wxTextCtrl)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(cType, ID_C_TYPE, wxChoice)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tAddress, ID_T_ADDRESS, wxTextCtrl)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tSubaddress, ID_T_SUBADDRESS, wxTextCtrl)
        if (tAddress->GetValue().size() == 0) {
            ERRORMSG("Address is required");
            return;
        }
        CCdd_descr *descr;
        if (selDescr && !isNew) {
            // editing existing item
            descr = selDescr->GetPointer();
        } else {
            // create new item
            CRef < CCdd_descr > newDescr(new CCdd_descr);
            InsertAfter(selectedItem, descrSet, newDescr.GetPointer());
            ++selectedItem;
            descr = newDescr.GetPointer();
        }
        isNew = false;
        descr->SetBook_ref().SetBookname(tName->GetValue().c_str());
        descr->SetBook_ref().SetTextelement(*(enum2str.Find(string(cType->GetStringSelection().c_str()))));
        long address, subaddress;
        if (tAddress->GetValue().ToLong(&address)) {
            descr->SetBook_ref().SetElementid((int) address);
            descr->SetBook_ref().ResetCelementid();
        } else {
            descr->SetBook_ref().SetCelementid(tAddress->GetValue().c_str());
            descr->SetBook_ref().ResetElementid();
        }
        if (tSubaddress->GetValue().size() > 0) {
            if (tSubaddress->GetValue().ToLong(&subaddress)) {
                descr->SetBook_ref().SetSubelementid((int) subaddress);
                descr->SetBook_ref().ResetCsubelementid();
            } else {
                descr->SetBook_ref().SetCsubelementid(tSubaddress->GetValue().c_str());
                descr->SetBook_ref().ResetSubelementid();
            }
        } else {
            descr->SetBook_ref().ResetSubelementid();
            descr->SetBook_ref().ResetCsubelementid();
        }
        sSet->SetDataChanged(StructureSet::eCDDData);
        editOn = false;
    }

    else if (event.GetId() == ID_B_NEW) {
        editOn = true;
        isNew = true;
    }

    else if (event.GetId() == ID_B_PASTE) {
        if (wxTheClipboard->Open()) {
            if (wxTheClipboard->IsSupported(wxDF_TEXT)) {
                CRef < CCdd_descr > descr(new CCdd_descr);
                wxTextDataObject data;
                wxTheClipboard->GetData(data);

                //  This was the *really* old method of parsing strings from a bv.fcgi URL.
                //bool madeBookRef = BvURLToBookRef(data, descr);
                //  This is the *old* method of parsing strings from a br.fcgi URL.
                //bool madeBookRef = BrURLToBookRef(data, descr);

                bool madeBookRef = BookURLToBookRef(data, descr);
                if (madeBookRef) {
                    InsertAfter(selectedItem, descrSet, descr.GetPointer());
                    ++selectedItem;
                    sSet->SetDataChanged(StructureSet::eCDDData);
                    isNew = false;
                } else {
                    INFOMSG("Book reference could not be created from pasted URL:\n" << data.GetText());
                }

            }
            wxTheClipboard->Close();
        }
    }

    else if (event.GetId() == ID_B_DELETE) {
        if (selDescr && !isNew) {
            CCdd_descr_set::Tdata::iterator d, de = descrSet->Set().end();
            for (d=descrSet->Set().begin(); d!=de; ++d) {
                if (&(*d) == selDescr) {
                    descrSet->Set().erase(d);
                    sSet->SetDataChanged(StructureSet::eCDDData);
                    if (selectedItem == (int) listbox->GetCount() - 1)
                        --selectedItem;
                    break;
                }
            }
        }
        editOn = false;
        isNew = false;
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
        wxT("figure"),
        wxT("table"),
        wxT("chapter"),
        wxT("biblist"),
        wxT("box"),
        wxT("def-item"),   //  used to be 'glossary'
        wxT("appendix"),
        wxT("other")
    };
    wxChoice *item18 = new wxChoice( parent, ID_C_TYPE, wxDefaultPosition, wxSize(100,-1), 10, strs18, 0 );
    item14->Add( item18, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item14, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item19 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item19->AddGrowableCol(3);

    wxStaticText *item20 = new wxStaticText( parent, ID_TEXT, wxT("Address:"), wxDefaultPosition, wxDefaultSize, 0 );
    item19->Add( item20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxTextCtrl *item21 = new wxTextCtrl( parent, ID_T_ADDRESS, wxT(""), wxDefaultPosition, wxSize(80,-1), 0 );
    item19->Add( item21, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item22 = new wxStaticText( parent, ID_TEXT, wxT("Sub-address:"), wxDefaultPosition, wxDefaultSize, 0 );
    item19->Add( item22, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxTextCtrl *item23 = new wxTextCtrl( parent, ID_T_SUBADDRESS, wxT(""), wxDefaultPosition, wxSize(80,-1), 0 );
    item19->Add( item23, 0, wxGROW/*wxALIGN_CENTRE*/|wxALL, 5 );

    item1->Add( item19, 0, wxGROW/*wxALIGN_CENTRE*/|wxALL, 5 );

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
