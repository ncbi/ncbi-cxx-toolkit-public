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
*      dialog for setting styles
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/05/31 18:47:10  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include "cn3d/style_dialog.hpp"
#include "cn3d/style_manager.hpp"
#include "cn3d/cn3d_tools.hpp"


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ header from render_settings.wdr
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

#define ID_NOTEBOOK 10000
#define ID_OK 10001
#define ID_CANCEL 10002
#define ID_TEXT 10003
#define ID_ALWAYS_APPLY 10004
#define ID_APPLY 10005
wxSizer *LayoutNotebook( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

#define ID_PBB_SHOW 10006
#define ID_PBB_RENDER 10007
#define ID_PBB_COLOR 10008
#define ID_PBB_USER 10009
wxSizer *LayoutPage1( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

#define ID_TEXTCTRL 10010
#define ID_SPINBUTTON 10011
wxSizer *LayoutPage2( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

////////////////////////////////////////////////////////////////////////////////////////////////

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

BEGIN_EVENT_TABLE(StyleDialog, wxDialog)
    EVT_CLOSE       (       StyleDialog::OnCloseWindow)
    EVT_CHOICE      (-1,    StyleDialog::OnChange)
    EVT_CHECKBOX    (-1,    StyleDialog::OnChange)
    EVT_BUTTON      (-1,    StyleDialog::OnButton)
END_EVENT_TABLE()

StyleDialog::StyleDialog(wxWindow* parent, const StyleSettings& initialSettings) :
    wxDialog(parent, -1, "Style Options", wxDefaultPosition, wxDefaultSize,
        wxCAPTION | wxSYSTEM_MENU) // not resizable
{
    // construct the panel
    wxSizer *topSizer = LayoutNotebook(this, false);

    // set defaults
    wxChoice *choice = wxDynamicCast(FindWindow(ID_PBB_SHOW), wxChoice);
    choice->SetStringSelection("Partial");

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->SetSizeHints(this);
}

bool StyleDialog::GetValues(StyleSettings *settings)
{
    return true;
}

void StyleDialog::OnCloseWindow(wxCommandEvent& event)
{
    EndModal(wxCANCEL);
}

void StyleDialog::OnButton(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case ID_OK: {
            StyleSettings dummy;
            if (GetValues(&dummy))  // can't successfully quit if values aren't valid
                EndModal(wxOK);
            else
                wxBell();
            break;
        }
        case ID_APPLY:
            TESTMSG("apply pressed");
            break;
        case ID_CANCEL:
            EndModal(wxCANCEL);
            break;
        case ID_PBB_USER:
            TESTMSG("PBB user color button pushed");
            break;
        default:
            event.Skip();
    }
}

void StyleDialog::OnChange(wxCommandEvent& event)
{
    TESTMSG("control changed");
}

END_SCOPE(Cn3D)


//////////////////////////////////////////////////////////////////////////////////////////////////
// The next two functions (LayoutNotebook() and LayoutPage1()) are taken *without* modification
// from wxDesigner C++ code generated from render_settings.wdr. The last function (LayoutPage2())
// is modified from wxDesigner output in order to use custom FloatingPointSpinCtrl's
//////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *LayoutNotebook( wxPanel *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxNotebook *item2 = new wxNotebook( parent, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize, 0 );
    wxNotebookSizer *item1 = new wxNotebookSizer( item2 );

    wxPanel *item3 = new wxPanel( item2, -1 );
    LayoutPage1( item3, FALSE );
    item2->AddPage( item3, "Settings" );

    wxPanel *item4 = new wxPanel( item2, -1 );
    LayoutPage2( item4, FALSE );
    item2->AddPage( item4, "Details" );

    item0->Add( item1, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxBoxSizer *item5 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item6 = new wxButton( parent, ID_OK, "OK", wxDefaultPosition, wxDefaultSize, 0 );
    item6->SetDefault();
    item5->Add( item6, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item7 = new wxButton( parent, ID_CANCEL, "Cancel", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item7, 0, wxALIGN_CENTRE|wxALL, 5 );

    item5->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item8 = new wxStaticText( parent, ID_TEXT, "Apply after each change?", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item8, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxCheckBox *item9 = new wxCheckBox( parent, ID_ALWAYS_APPLY, "", wxDefaultPosition, wxDefaultSize, 0 );
    item9->SetValue( TRUE );
    item5->Add( item9, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item10 = new wxButton( parent, ID_APPLY, "Apply", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item10, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item5, 0, wxALIGN_CENTRE|wxALL, 5 );

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

wxSizer *LayoutPage1( wxPanel *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, "Rendering Settings" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxFlexGridSizer *item3 = new wxFlexGridSizer( 5, 0, 10 );

    wxStaticText *item4 = new wxStaticText( parent, ID_TEXT, "Group", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item3->Add( item4, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item5 = new wxStaticText( parent, ID_TEXT, "Show", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item3->Add( item5, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item6 = new wxStaticText( parent, ID_TEXT, "Rendering", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item3->Add( item6, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item7 = new wxStaticText( parent, ID_TEXT, "Color Scheme", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item3->Add( item7, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item8 = new wxStaticText( parent, ID_TEXT, "User Color", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item3->Add( item8, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item9 = new wxStaticText( parent, ID_TEXT, "Protein backbone:", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item3->Add( item9, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxString strs10[] =
    {
        "None",
        "Trace",
        "Partial",
        "Complete"
    };
    wxChoice *item10 = new wxChoice( parent, ID_PBB_SHOW, wxDefaultPosition, wxDefaultSize, 4, strs10, 0 );
    item3->Add( item10, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxString strs11[] =
    {
        "Wire Worm",
        "Tube Worm",
        "Wire",
        "Tubes",
        "Ball and Stick",
        "Space Fill"
    };
    wxChoice *item11 = new wxChoice( parent, ID_PBB_RENDER, wxDefaultPosition, wxDefaultSize, 6, strs11, 0 );
    item3->Add( item11, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxString strs12[] =
    {
        "Element",
        "Object",
        "Molecule",
        "Domain",
        "Secondary Structure",
        "User Color",
        "Aligned",
        "Identity",
        "Variety",
        "Weighted Variety",
        "Information Content",
        "Fit"
    };
    wxChoice *item12 = new wxChoice( parent, ID_PBB_COLOR, wxDefaultPosition, wxDefaultSize, 12, strs12, 0 );
    item3->Add( item12, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item13 = new wxButton( parent, ID_PBB_USER, "", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item13, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item3, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item1, 0, wxALIGN_CENTRE|wxALL, 5 );

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

// this is modified from wxDesigner output in order to use custom FloatingPointSpinCtrl's

wxSizer *LayoutPage2( wxPanel *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, "Rendering Details" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxFlexGridSizer *item3 = new wxFlexGridSizer( 3, 0, 0 );

    wxStaticText *item4 = new wxStaticText( parent, ID_TEXT, "Space fill size:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
    item3->Add( item4, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxTextCtrl *item5 = new wxTextCtrl( parent, ID_TEXTCTRL, "", wxDefaultPosition, wxSize(80,-1), 0 );
    item3->Add( item5, 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5 );

    wxSpinButton *item6 = new wxSpinButton( parent, ID_SPINBUTTON, wxDefaultPosition, wxSize(-1,20), 0 );
    item6->SetRange( 0, 100 );
    item6->SetValue( 0 );
    item3->Add( item6, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxStaticText *item7 = new wxStaticText( parent, ID_TEXT, "Tube radius:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
    item3->Add( item7, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxTextCtrl *item8 = new wxTextCtrl( parent, ID_TEXTCTRL, "", wxDefaultPosition, wxSize(80,20), 0 );
    item3->Add( item8, 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5 );

    wxSpinButton *item9 = new wxSpinButton( parent, ID_SPINBUTTON, wxDefaultPosition, wxSize(-1,20), 0 );
    item9->SetRange( 0, 100 );
    item9->SetValue( 0 );
    item3->Add( item9, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxStaticText *item10 = new wxStaticText( parent, ID_TEXT, "Worm tube radius:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
    item3->Add( item10, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxTextCtrl *item11 = new wxTextCtrl( parent, ID_TEXTCTRL, "", wxDefaultPosition, wxSize(80,20), 0 );
    item3->Add( item11, 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5 );

    wxSpinButton *item12 = new wxSpinButton( parent, ID_SPINBUTTON, wxDefaultPosition, wxSize(-1,20), 0 );
    item12->SetRange( 0, 100 );
    item12->SetValue( 0 );
    item3->Add( item12, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxStaticText *item13 = new wxStaticText( parent, ID_TEXT, "Ball radius:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
    item3->Add( item13, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxTextCtrl *item14 = new wxTextCtrl( parent, ID_TEXTCTRL, "", wxDefaultPosition, wxSize(80,20), 0 );
    item3->Add( item14, 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5 );

    wxSpinButton *item15 = new wxSpinButton( parent, ID_SPINBUTTON, wxDefaultPosition, wxSize(-1,20), 0 );
    item15->SetRange( 0, 100 );
    item15->SetValue( 0 );
    item3->Add( item15, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxStaticText *item16 = new wxStaticText( parent, ID_TEXT, "Stick radius:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
    item3->Add( item16, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxTextCtrl *item17 = new wxTextCtrl( parent, ID_TEXTCTRL, "", wxDefaultPosition, wxSize(80,20), 0 );
    item3->Add( item17, 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5 );

    wxSpinButton *item18 = new wxSpinButton( parent, ID_SPINBUTTON, wxDefaultPosition, wxSize(-1,20), 0 );
    item18->SetRange( 0, 100 );
    item18->SetValue( 0 );
    item3->Add( item18, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    item3->Add( 1, 1, 0, wxALIGN_CENTRE|wxALL, 5 );

    item3->Add( 1, 1, 0, wxALIGN_CENTRE|wxALL, 5 );

    item3->Add( 1, 1, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item19 = new wxStaticText( parent, ID_TEXT, "Helix radius:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
    item3->Add( item19, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxTextCtrl *item20 = new wxTextCtrl( parent, ID_TEXTCTRL, "", wxDefaultPosition, wxSize(80,20), 0 );
    item3->Add( item20, 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5 );

    wxSpinButton *item21 = new wxSpinButton( parent, ID_SPINBUTTON, wxDefaultPosition, wxSize(-1,20), 0 );
    item21->SetRange( 0, 100 );
    item21->SetValue( 0 );
    item3->Add( item21, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxStaticText *item22 = new wxStaticText( parent, ID_TEXT, "Strand width:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
    item3->Add( item22, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxTextCtrl *item23 = new wxTextCtrl( parent, ID_TEXTCTRL, "", wxDefaultPosition, wxSize(80,20), 0 );
    item3->Add( item23, 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5 );

    wxSpinButton *item24 = new wxSpinButton( parent, ID_SPINBUTTON, wxDefaultPosition, wxSize(-1,20), 0 );
    item24->SetRange( 0, 100 );
    item24->SetValue( 0 );
    item3->Add( item24, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    wxStaticText *item25 = new wxStaticText( parent, ID_TEXT, "Strand thickness:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT );
    item3->Add( item25, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxTextCtrl *item26 = new wxTextCtrl( parent, ID_TEXTCTRL, "", wxDefaultPosition, wxSize(80,20), 0 );
    item3->Add( item26, 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5 );

    wxSpinButton *item27 = new wxSpinButton( parent, ID_SPINBUTTON, wxDefaultPosition, wxSize(-1,20), 0 );
    item27->SetRange( 0, 100 );
    item27->SetValue( 0 );
    item3->Add( item27, 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5 );

    item1->Add( item3, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item1, 0, wxALIGN_CENTRE|wxALL, 5 );

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

