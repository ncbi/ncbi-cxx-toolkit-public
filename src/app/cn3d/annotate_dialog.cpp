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
*      dialog for user annotations
*
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "annotate_dialog.hpp"
#include "messenger.hpp"
#include "style_manager.hpp"
#include "style_dialog.hpp"
#include "structure_set.hpp"
#include "show_hide_manager.hpp"
#include "cn3d_tools.hpp"


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

// Declare window functions

#define ID_TEXT 10000
#define ID_L_AVAILABLE 10001
#define ID_B_TURN_ON 10002
#define ID_B_TURN_OFF 10003
#define ID_B_MOVE_UP 10004
#define ID_B_MOVE_DOWN 10005
#define ID_L_DISPLAYED 10006
#define ID_LINE 10007
#define ID_ST_NAME 10008
#define ID_B_NEW 10009
#define ID_ST_DESCR 10010
#define ID_B_SHOW 10011
#define ID_B_EDIT 10012
#define ID_B_MOVE 10013
#define ID_B_DELETE 10014
#define ID_B_DONE 10015
wxSizer *SetupAnnotationControlDialog( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

#define ID_T_NAME 10016
#define ID_B_EDIT_STYLE 10017
#define ID_T_DESCR 10018
#define ID_B_EDIT_DONE 10019
#define ID_B_EDIT_CANCEL 10020
wxSizer *SetupAnnotationEditorDialog( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

////////////////////////////////////////////////////////////////////////////////////////////////

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

#define DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERRORMSG("Can't find window with id " << id); \
        return; \
    }

#define ANNOT_FROM_CLIENT_DATA(listbox) \
    (reinterpret_cast < StyleManager::UserAnnotation * > \
        ((listbox)->GetClientData((listbox)->GetSelection())))

BEGIN_EVENT_TABLE(AnnotateDialog, wxDialog)
    EVT_CLOSE       (       AnnotateDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    AnnotateDialog::OnButton)
    EVT_LISTBOX     (-1,    AnnotateDialog::OnSelection)
END_EVENT_TABLE()

AnnotateDialog::AnnotateDialog(wxWindow *parent, StyleManager *manager, const StructureSet *set) :
    wxDialog(parent, -1, "User Annotations", wxPoint(400, 100), wxDefaultSize,
        wxCAPTION | wxSYSTEM_MENU), // not resizable
    styleManager(manager), structureSet(set)
{
    // get the structure highlights present when this dialog is created
    GlobalMessenger()->GetHighlightedResiduesWithStructure(&highlightedResidues);

    // construct the panel
    wxPanel *panel = new wxPanel(this, -1);
    wxSizer *topSizer = SetupAnnotationControlDialog(panel, false);

    // fill in list boxes with available and displayed styles
    ResetListBoxes();

    // set initial button states
    SetButtonStates();

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->Fit(panel);
    topSizer->SetSizeHints(this);
}

// same as hitting done button
void AnnotateDialog::OnCloseWindow(wxCloseEvent& event)
{
    EndModal(wxOK);
}

void AnnotateDialog::OnButton(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case ID_B_NEW:
            NewAnnotation();
            break;
        case ID_B_EDIT:
            EditAnnotation();
            break;
        case ID_B_MOVE:
            MoveAnnotation();
            break;
        case ID_B_DELETE:
            DeleteAnnotation();
            break;
        case ID_B_SHOW: {
            DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(available, ID_L_AVAILABLE, wxListBox)
            if (available->GetSelection() >= 0) {
                StyleManager::UserAnnotation *annotation = ANNOT_FROM_CLIENT_DATA(available);
                if (annotation) {
                    GlobalMessenger()->SetHighlights(annotation->residues);
                    structureSet->showHideManager->ShowDomainsWithHighlights(structureSet);
                    highlightedResidues = annotation->residues;
                } else
                    ERRORMSG("AnnotateDialog::OnButton() - error highlighting annotation #"
                        << available->GetSelection());
            }
            break;
        }
        case ID_B_MOVE_UP: case ID_B_MOVE_DOWN: {
            DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(available, ID_L_AVAILABLE, wxListBox)
            if (available->GetSelection() >= 0) {
                StyleManager::UserAnnotation *annotation = ANNOT_FROM_CLIENT_DATA(available);
                if (annotation && styleManager->
                        MoveUserAnnotation(annotation, (event.GetId() == ID_B_MOVE_UP)))
                    ResetListBoxes();
                else
                    ERRORMSG("AnnotateDialog::OnButton() - error reprioritizing annotation #"
                        << available->GetSelection());
            }
            break;
        }
        case ID_B_TURN_OFF: case ID_B_TURN_ON: {
			int listID = (event.GetId() == ID_B_TURN_OFF) ? ID_L_DISPLAYED : ID_L_AVAILABLE;
            DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(listBox, listID, wxListBox)
            if (listBox->GetSelection() >= 0) {
                StyleManager::UserAnnotation *annotation = ANNOT_FROM_CLIENT_DATA(listBox);
                if (annotation && styleManager->
                        DisplayUserAnnotation(annotation, (event.GetId() == ID_B_TURN_ON)))
                    ResetListBoxes();
                else
                    ERRORMSG("AnnotateDialog::OnButton() - error toggling annotation #"
                        << listBox->GetSelection());
            }
            break;
        }
        case ID_B_DONE:
            EndModal(wxOK);
            break;
        default:
            event.Skip();
    }

    SetButtonStates();
}

void AnnotateDialog::OnSelection(wxCommandEvent& event)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(eventBox, event.GetId(), wxListBox)
    int otherID = (event.GetId() == ID_L_DISPLAYED) ? ID_L_AVAILABLE : ID_L_DISPLAYED;
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(otherBox, otherID, wxListBox)

    // deselect everything in the other box
    int e, o;
    for (o=0; o<otherBox->GetCount(); ++o) otherBox->SetSelection(o, false);

    // turn on corresponding item in other box
    for (e=0; e<eventBox->GetCount(); ++e) {
        if (eventBox->Selected(e)) {
            for (o=0; o<otherBox->GetCount(); ++o) {
                if (otherBox->GetClientData(o) == eventBox->GetClientData(e)) {
                    otherBox->SetSelection(o, true);
                    break;
                }
            }
            break;
        }
    }

    SetButtonStates();
}

void AnnotateDialog::SetButtonStates(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(available, ID_L_AVAILABLE, wxListBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(displayed, ID_L_DISPLAYED, wxListBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bTurnOn, ID_B_TURN_ON, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bTurnOff, ID_B_TURN_OFF, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bMoveUp, ID_B_MOVE_UP, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bMoveDown, ID_B_MOVE_DOWN, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bNew, ID_B_NEW, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bEdit, ID_B_EDIT, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bMove, ID_B_MOVE, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bDelete, ID_B_DELETE, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(bShow, ID_B_SHOW, wxButton)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tName, ID_ST_NAME, wxStaticText)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tDescr, ID_ST_DESCR, wxStaticText)

    bool availableSelected = (available->GetSelection() >= 0);
    bool displayedSelected = (displayed->GetSelection() >= 0);
    bTurnOn->Enable(availableSelected && !displayedSelected);
    bTurnOff->Enable(displayedSelected);
    bMoveUp->Enable(availableSelected && available->GetSelection() > 0);
    bMoveDown->Enable(availableSelected && available->GetSelection() < available->GetCount() - 1);

    bNew->Enable(HighlightsPresent());
    bEdit->Enable(availableSelected);
    bMove->Enable(availableSelected && HighlightsPresent());
    bDelete->Enable(availableSelected);

    bShow->Enable(availableSelected);
    const StyleManager::UserAnnotation *annotation = NULL;
    if (availableSelected) {
        annotation = ANNOT_FROM_CLIENT_DATA(available);
        if (!annotation)
            ERRORMSG("AnnotateDialog::SetButtonStates() - NULL annotation pointer");
    }
    tName->SetLabel(annotation ? annotation->name.c_str() : "");
    tDescr->SetLabel(annotation ? annotation->description.c_str() : "");
}

// recreates the list box entries, trying to keep current selection
void AnnotateDialog::ResetListBoxes(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(available, ID_L_AVAILABLE, wxListBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(displayed, ID_L_DISPLAYED, wxListBox)
    StyleManager::UserAnnotationList& annotations = styleManager->GetUserAnnotations();
    StyleManager::UserAnnotationList::iterator l, le = annotations.end();

    // determine what should be selected initially
    void *selection = NULL;
    if (available->GetSelection() >= 0)
        selection = available->GetClientData(available->GetSelection());
    else if (annotations.size() > 0) {
        for (l=annotations.begin(); l!=le; ++l) {
            if ((*l)->isDisplayed) {
                selection = l->GetPointer();
                break;
            }
        }
        if (l == le)
            selection = annotations.front().GetPointer();
    }

    // recreate lists
    available->Clear();
    displayed->Clear();
    int i, id = 0;
    for (i=0, l=annotations.begin(); l!=le; ++i, ++l) {
        available->Append((*l)->name.c_str(), l->GetPointer());
        if (selection == l->GetPointer())
            available->SetSelection(i, true);
        if ((*l)->isDisplayed) {
            displayed->Append((*l)->name.c_str(), l->GetPointer());
            if (selection == l->GetPointer())
                displayed->SetSelection(id, true);
            ++id;
        }
    }
}

void AnnotateDialog::NewAnnotation(void)
{
    // create new StyleSettings and UserAnnotation structures
    StyleManager::UserAnnotation *newAnnotation = styleManager->AddUserAnnotation();
    if (!newAnnotation) return;
    newAnnotation->name = "(new)";
    newAnnotation->residues = highlightedResidues;   // copy list of stuff highlighted at dialog creation
    StyleSettings *style;
    if (!styleManager->AddUserStyle(&(newAnnotation->styleID), &style) || !style ||
        !styleManager->DisplayUserAnnotation(newAnnotation, true)) { // turn on new annotation
        ERRORMSG("AnnotateDialog::NewAnnotation() - error setting up new annotation");
        return;
    }
    *style = styleManager->GetGlobalStyle();    // set initial style to same as current global style
    ResetListBoxes();

    // bring up the annotation editor dialog; hide highlights during this process, so the user
    // can see the color changes as the style is edited
    GlobalMessenger()->SuspendHighlighting(true);
    AnnotationEditorDialog dialog(this, style, structureSet, *newAnnotation);
    int result = dialog.ShowModal();
    GlobalMessenger()->SuspendHighlighting(false);

    if (result == wxOK) {
        // copy name and description from dialog
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tName, ID_T_NAME, wxTextCtrl)
        newAnnotation->name = tName->GetValue().c_str();
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tDescr, ID_T_DESCR, wxTextCtrl)
        newAnnotation->description = tDescr->GetValue().c_str();

        // set selection to new annotation
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(available, ID_L_AVAILABLE, wxListBox)
        available->SetSelection(available->GetCount() - 1);

    } else { // wxCANCEL
        if (!styleManager->RemoveUserAnnotation(newAnnotation))
            ERRORMSG("AnnotateDialog::NewAnnotation() - error removing new annotation");
    }
    ResetListBoxes();
    SetButtonStates();
}

void AnnotateDialog::EditAnnotation(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(available, ID_L_AVAILABLE, wxListBox)
    if (available->GetSelection() < 0) return;
    StyleManager::UserAnnotation *annotation = ANNOT_FROM_CLIENT_DATA(available);
    StyleSettings *style = NULL;
    if (!annotation || (style=styleManager->GetUserStyle(annotation->styleID)) == NULL) {
        ERRORMSG("AnnotateDialog::EditAnnotation() - error getting annotation and style");
        return;
    }

    // bring up the annotation editor dialog
    StyleSettings copy = *style;
    AnnotationEditorDialog dialog(this, style, structureSet, *annotation);
    int result = dialog.ShowModal();

    if (result == wxOK) {
        // copy name and description from dialog
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tName, ID_T_NAME, wxTextCtrl)
        annotation->name = tName->GetValue().c_str();
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tDescr, ID_T_DESCR, wxTextCtrl)
        annotation->description = tDescr->GetValue().c_str();
        structureSet->SetDataChanged(StructureSet::eStyleData);
    } else {    // wxCANCEL
        // restore original settings
        *style = copy;
        GlobalMessenger()->PostRedrawAllStructures();
        GlobalMessenger()->PostRedrawAllSequenceViewers();
    }
}

void AnnotateDialog::MoveAnnotation(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(available, ID_L_AVAILABLE, wxListBox)
    if (available->GetSelection() < 0) return;
    StyleManager::UserAnnotation *annotation = ANNOT_FROM_CLIENT_DATA(available);
    if (!annotation) {
        ERRORMSG("AnnotateDialog::MoveAnnotation() - NULL annotation pointer");
        return;
    }

    // make sure this is what the user wants
    wxString message;
    message.Printf("This will move the coverage of annotation '%s'\n"
        "to the currently highlighted region. Is this correct?", available->GetStringSelection().c_str());
    int result = wxMessageBox(message, "Confirmation", wxOK | wxCANCEL | wxCENTRE, this);

    // copy current highlight list to selected annotation
    if (result == wxOK) {
        annotation->residues = highlightedResidues;
        GlobalMessenger()->PostRedrawAllStructures();
        GlobalMessenger()->PostRedrawAllSequenceViewers();
        structureSet->SetDataChanged(StructureSet::eStyleData);
    }
}

void AnnotateDialog::DeleteAnnotation(void)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(available, ID_L_AVAILABLE, wxListBox)
    if (available->GetSelection() < 0) return;
    StyleManager::UserAnnotation *annotation = ANNOT_FROM_CLIENT_DATA(available);
    if (!annotation) {
        ERRORMSG("AnnotateDialog::DeleteAnnotation() - NULL annotation pointer");
        return;
    }

    // make sure this is what the user wants
    wxString message;
    message.Printf("This will permanently remove the annotation\n"
        "'%s'. Is this correct?", available->GetStringSelection().c_str());
    int result = wxMessageBox(message, "Confirmation", wxOK | wxCANCEL | wxCENTRE, this);

    // copy current highlight list to selected annotation
    if (result == wxOK) {
        styleManager->RemoveUserAnnotation(annotation);
        ResetListBoxes();
        SetButtonStates();
    }
}


BEGIN_EVENT_TABLE(AnnotationEditorDialog, wxDialog)
    EVT_CLOSE       (       AnnotationEditorDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    AnnotationEditorDialog::OnButton)
END_EVENT_TABLE()

AnnotationEditorDialog::AnnotationEditorDialog(wxWindow *parent,
        StyleSettings *settings, const StructureSet *set,
        const StyleManager::UserAnnotation& initialText) :
    wxDialog(parent, -1, "Edit Annotation", wxPoint(450, 200), wxDefaultSize,
        wxCAPTION | wxSYSTEM_MENU), // not resizable
    styleSettings(settings), structureSet(set)
{
    // construct the panel
    wxPanel *panel = new wxPanel(this, -1);
    wxSizer *topSizer = SetupAnnotationEditorDialog(panel, false);

    // set initial state
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tName, ID_T_NAME, wxTextCtrl)
    tName->SetValue(initialText.name.c_str());
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tDescr, ID_T_DESCR, wxTextCtrl)
    tDescr->SetValue(initialText.description.c_str());

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->Fit(panel);
    topSizer->SetSizeHints(this);

    // select name, so user can quickly change it
    tName->SetFocus();
    tName->SetSelection(0, initialText.name.size());
}

// same as hitting cancel button
void AnnotationEditorDialog::OnCloseWindow(wxCloseEvent& event)
{
    EndModal(wxCANCEL);
}

void AnnotationEditorDialog::OnButton(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case ID_B_EDIT_STYLE: {
            StyleDialog dialog(this, styleSettings, structureSet);
            dialog.ShowModal();
            break;
        }
        case ID_B_EDIT_DONE: {
            DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tName, ID_T_NAME, wxTextCtrl)
            if (tName->GetValue().size() == 0)
                wxMessageBox("The annotation must have a name!",
                    "Input Error", wxOK | wxCENTRE | wxICON_ERROR, this);
            else
                EndModal(wxOK);
            break;
        }
        case ID_B_EDIT_CANCEL:
            EndModal(wxCANCEL);
            break;
        default:
            event.Skip();
    }
}

END_SCOPE(Cn3D)


//////////////////////////////////////////////////////////////////////////////////////////////////
// The next following are taken *without* modification from wxDesigner C++ code generated from
// annotate_dialog.wdr.
//////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *SetupAnnotationControlDialog( wxPanel *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, "Annotation Control" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxFlexGridSizer *item3 = new wxFlexGridSizer( 2, 0, 0, 0 );
    item3->AddGrowableCol( 0 );
    item3->AddGrowableCol( 2 );

    wxStaticText *item4 = new wxStaticText( parent, ID_TEXT, "Available", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item4, 0, wxALIGN_CENTRE|wxLEFT|wxRIGHT|wxTOP, 5 );

    item3->Add( 5, 5, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item5 = new wxStaticText( parent, ID_TEXT, "Displayed", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item5, 0, wxALIGN_CENTRE|wxLEFT|wxRIGHT|wxTOP, 5 );

    wxString *strs6 = (wxString*) NULL;
    wxListBox *item6 = new wxListBox( parent, ID_L_AVAILABLE, wxDefaultPosition, wxSize(120,-1), 0, strs6, wxLB_SINGLE );
    item3->Add( item6, 0, wxGROW|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    wxFlexGridSizer *item7 = new wxFlexGridSizer( 1, 0, 0 );

    wxButton *item8 = new wxButton( parent, ID_B_TURN_ON, "Turn On", wxDefaultPosition, wxDefaultSize, 0 );
    item8->SetToolTip( "Display the selected annotation" );
    item8->Enable( FALSE );
    item7->Add( item8, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item9 = new wxButton( parent, ID_B_TURN_OFF, "Turn Off", wxDefaultPosition, wxDefaultSize, 0 );
    item9->SetToolTip( "Hide the selected annotation" );
    item9->Enable( FALSE );
    item7->Add( item9, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item7->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item10 = new wxButton( parent, ID_B_MOVE_UP, "Move Up", wxDefaultPosition, wxDefaultSize, 0 );
    item10->SetToolTip( "Move annotation up in priority" );
    item10->Enable( FALSE );
    item7->Add( item10, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item11 = new wxButton( parent, ID_B_MOVE_DOWN, "Move Down", wxDefaultPosition, wxDefaultSize, 0 );
    item11->SetToolTip( "Move annotation down in priority" );
    item11->Enable( FALSE );
    item7->Add( item11, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item3->Add( item7, 0, wxALIGN_CENTRE, 5 );

    wxString *strs12 = (wxString*) NULL;
    wxListBox *item12 = new wxListBox( parent, ID_L_DISPLAYED, wxDefaultPosition, wxSize(120,-1), 0, strs12, wxLB_SINGLE );
    item3->Add( item12, 0, wxGROW|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    item1->Add( item3, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxStaticLine *item13 = new wxStaticLine( parent, ID_LINE, wxDefaultPosition, wxSize(20,-1), wxLI_HORIZONTAL );
    item1->Add( item13, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item14 = new wxFlexGridSizer( 2, 0, 0, 0 );
    item14->AddGrowableCol( 1 );

    wxStaticText *item15 = new wxStaticText( parent, ID_TEXT, "Selection:", wxDefaultPosition, wxDefaultSize, 0 );
    item14->Add( item15, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxFlexGridSizer *item16 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item16->AddGrowableCol( 0 );

    wxStaticText *item17 = new wxStaticText( parent, ID_ST_NAME, "", wxDefaultPosition, wxSize(120,-1), 0 );
    item16->Add( item17, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item18 = new wxButton( parent, ID_B_NEW, "New", wxDefaultPosition, wxDefaultSize, 0 );
    item16->Add( item18, 0, wxALIGN_CENTRE|wxALL, 5 );

    item14->Add( item16, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    wxStaticText *item19 = new wxStaticText( parent, ID_TEXT, "Description:", wxDefaultPosition, wxDefaultSize, 0 );
    item14->Add( item19, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxStaticText *item20 = new wxStaticText( parent, ID_ST_DESCR, "", wxDefaultPosition, wxDefaultSize, 0 );
    item14->Add( item20, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item1->Add( item14, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxStaticLine *item21 = new wxStaticLine( parent, ID_LINE, wxDefaultPosition, wxSize(20,-1), wxLI_HORIZONTAL );
    item1->Add( item21, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxBoxSizer *item22 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item23 = new wxButton( parent, ID_B_SHOW, "Show", wxDefaultPosition, wxDefaultSize, 0 );
    item22->Add( item23, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item24 = new wxButton( parent, ID_B_EDIT, "Edit", wxDefaultPosition, wxDefaultSize, 0 );
    item24->Enable( FALSE );
    item22->Add( item24, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item25 = new wxButton( parent, ID_B_MOVE, "Move", wxDefaultPosition, wxDefaultSize, 0 );
    item25->Enable( FALSE );
    item22->Add( item25, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item26 = new wxButton( parent, ID_B_DELETE, "Delete", wxDefaultPosition, wxDefaultSize, 0 );
    item26->Enable( FALSE );
    item22->Add( item26, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item22, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item1, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxBoxSizer *item27 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item28 = new wxButton( parent, ID_B_DONE, "Done", wxDefaultPosition, wxDefaultSize, 0 );
    item28->SetDefault();
    item27->Add( item28, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item27, 0, wxALIGN_CENTRE|wxALL, 5 );

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

wxSizer *SetupAnnotationEditorDialog( wxPanel *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, "Edit Annotation" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxFlexGridSizer *item3 = new wxFlexGridSizer( 2, 0, 0, 0 );
    item3->AddGrowableCol( 1 );
    item3->AddGrowableRow( 1 );

    wxStaticText *item4 = new wxStaticText( parent, ID_TEXT, "Name:", wxDefaultPosition, wxDefaultSize, 0 );
    item4->SetToolTip( "cool!" );
    item3->Add( item4, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxLEFT|wxTOP|wxBOTTOM, 5 );

    wxFlexGridSizer *item5 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item5->AddGrowableCol( 1 );

    wxTextCtrl *item6 = new wxTextCtrl( parent, ID_T_NAME, "", wxDefaultPosition, wxSize(120,-1), 0 );
    item5->Add( item6, 0, wxALIGN_CENTRE|wxALL, 5 );

    item5->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item7 = new wxButton( parent, ID_B_EDIT_STYLE, "Edit Style", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item7, 0, wxALIGN_CENTRE|wxALL, 5 );

    item3->Add( item5, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    wxStaticText *item8 = new wxStaticText( parent, ID_TEXT, "Description:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item8, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxLEFT|wxTOP|wxBOTTOM, 5 );

    wxTextCtrl *item9 = new wxTextCtrl( parent, ID_T_DESCR, "", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item9, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item1->Add( item3, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item0->Add( item1, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxBoxSizer *item10 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item11 = new wxButton( parent, ID_B_EDIT_DONE, "OK", wxDefaultPosition, wxDefaultSize, 0 );
    item10->Add( item11, 0, wxALIGN_CENTRE|wxALL, 5 );

    item10->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item12 = new wxButton( parent, ID_B_EDIT_CANCEL, "Cancel", wxDefaultPosition, wxDefaultSize, 0 );
    item10->Add( item12, 0, wxALIGN_CENTRE|wxALL, 5 );

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


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.16  2004/06/02 21:33:13  thiessen
* reorganize user annotation storage so that reordering is saved
*
* Revision 1.15  2004/05/21 21:41:38  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.14  2004/03/15 18:16:33  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.13  2004/02/19 17:04:40  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.12  2003/02/03 19:20:00  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.11  2002/10/28 21:36:01  thiessen
* add show domains with highlights
*
* Revision 1.10  2002/08/15 22:13:11  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.9  2001/12/06 23:13:44  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.8  2001/11/27 16:26:06  thiessen
* major update to data management system
*
* Revision 1.7  2001/09/24 14:37:52  thiessen
* more wxPanel stuff - fix for new heirarchy in wx 2.3.2+
*
* Revision 1.6  2001/09/24 13:29:54  thiessen
* fix wxPanel issues
*
* Revision 1.5  2001/09/20 19:31:30  thiessen
* fixes for SGI and wxWin 2.3.2
*
* Revision 1.4  2001/08/06 20:22:00  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* Revision 1.3  2001/07/19 19:14:38  thiessen
* working CDD alignment annotator ; misc tweaks
*
* Revision 1.2  2001/07/04 19:39:16  thiessen
* finish user annotation system
*
* Revision 1.1  2001/06/29 18:13:57  thiessen
* initial (incomplete) user annotation system
*
*/
