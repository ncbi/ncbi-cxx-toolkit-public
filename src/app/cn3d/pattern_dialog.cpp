/////////////////////////////////////////////////////////////////////////////
// Name:        pattern_dialog.cpp
// Purpose:
// Author:
// Modified by:
// Created:     10/01/04 10:47:06
// RCS-ID:      $Id$
// Copyright:
// Licence:
/////////////////////////////////////////////////////////////////////////////

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma implementation "pattern_dialog.hpp"
#endif

#include <ncbi_pch.hpp>

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

////@begin includes
////@end includes

#include "pattern_dialog.hpp"

////@begin XPM images
////@end XPM images

/*!
 * PatternDialog type definition
 */

IMPLEMENT_DYNAMIC_CLASS( PatternDialog, wxDialog )

/*!
 * PatternDialog event table definition
 */

BEGIN_EVENT_TABLE( PatternDialog, wxDialog )

////@begin PatternDialog event table entries
    EVT_BUTTON( ID_OK, PatternDialog::OnOkClick )

    EVT_BUTTON( ID_CANCEL, PatternDialog::OnCancelClick )

////@end PatternDialog event table entries

END_EVENT_TABLE()

/*!
 * PatternDialog constructors
 */

PatternDialog::PatternDialog( )
{
}

PatternDialog::PatternDialog( wxWindow* parent, wxWindowID id, const wxString& caption, const wxPoint& pos, const wxSize& size, long style )
{
    Create(parent, id, caption, pos, size, style);
}

/*!
 * PatternDialog creator
 */

bool PatternDialog::Create( wxWindow* parent, wxWindowID id, const wxString& caption, const wxPoint& pos, const wxSize& size, long style )
{
////@begin PatternDialog member initialisation
    m_Pattern = NULL;
    m_Mode = NULL;
////@end PatternDialog member initialisation

////@begin PatternDialog creation
    SetExtraStyle(GetExtraStyle()|wxWS_EX_BLOCK_EVENTS);
    wxDialog::Create( parent, id, caption, pos, size, style );

    CreateControls();
    GetSizer()->Fit(this);
    GetSizer()->SetSizeHints(this);
    Centre();
////@end PatternDialog creation
    return TRUE;
}

/*!
 * Control creation for PatternDialog
 */

void PatternDialog::CreateControls()
{
////@begin PatternDialog content construction

    PatternDialog* itemDialog1 = this;

    wxBoxSizer* itemBoxSizer2 = new wxBoxSizer(wxVERTICAL);
    itemDialog1->SetSizer(itemBoxSizer2);
    itemDialog1->SetAutoLayout(TRUE);
    wxStaticText* itemStaticText3 = new wxStaticText( itemDialog1, wxID_STATIC, _("Enter a pattern using ProSite syntax:"), wxDefaultPosition, wxDefaultSize, 0 );
    m_Text = itemStaticText3;
    itemBoxSizer2->Add(itemStaticText3, 0, wxALIGN_LEFT|wxALL|wxADJUST_MINSIZE, 5);

    wxTextCtrl* itemTextCtrl4 = new wxTextCtrl( itemDialog1, ID_T_PATTERN, _T(""), wxDefaultPosition, wxSize(300, -1), 0 );
    m_Pattern = itemTextCtrl4;
    itemBoxSizer2->Add(itemTextCtrl4, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxString itemRadioBox5Strings[] = {
        _("Replace"),
        _("Add"),
        _("Within")
    };
    wxRadioBox* itemRadioBox5 = new wxRadioBox( itemDialog1, ID_RADIOBOX, _("Highlight mode"), wxDefaultPosition, wxDefaultSize, 3, itemRadioBox5Strings, 1, wxRA_SPECIFY_ROWS );
    m_Mode = itemRadioBox5;
    itemBoxSizer2->Add(itemRadioBox5, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

    wxStaticLine* itemStaticLine6 = new wxStaticLine( itemDialog1, wxID_STATIC, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
    itemBoxSizer2->Add(itemStaticLine6, 0, wxGROW|wxALL, 5);

    wxBoxSizer* itemBoxSizer7 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer2->Add(itemBoxSizer7, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
    wxButton* itemButton8 = new wxButton( itemDialog1, ID_OK, _("Ok"), wxDefaultPosition, wxDefaultSize, 0 );
    itemButton8->SetDefault();
    itemBoxSizer7->Add(itemButton8, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxButton* itemButton9 = new wxButton( itemDialog1, ID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer7->Add(itemButton9, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

////@end PatternDialog content construction
}

/*!
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_OK
 */

void PatternDialog::OnOkClick( wxCommandEvent& event )
{
    // Insert custom code here
    EndModal(wxID_OK);
}

/*!
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_CANCEL
 */

void PatternDialog::OnCancelClick( wxCommandEvent& event )
{
    // Insert custom code here
    EndModal(wxID_CANCEL);
}

/*!
 * Should we show tooltips?
 */

bool PatternDialog::ShowToolTips()
{
    return TRUE;
}

/*!
 * Get bitmap resources
 */

wxBitmap PatternDialog::GetBitmapResource( const wxString& name )
{
    // Bitmap retrieval
////@begin PatternDialog bitmap retrieval
    return wxNullBitmap;
////@end PatternDialog bitmap retrieval
}

/*!
 * Get icon resources
 */

wxIcon PatternDialog::GetIconResource( const wxString& name )
{
    // Icon retrieval
////@begin PatternDialog icon retrieval
    return wxNullIcon;
////@end PatternDialog icon retrieval
}
