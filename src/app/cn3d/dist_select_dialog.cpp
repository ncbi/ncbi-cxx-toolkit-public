/////////////////////////////////////////////////////////////////////////////
// Name:        dist_select_dialog.cpp
// Purpose:     
// Author:      Paul Thiessen
// Modified by: 
// Created:     10/05/04 08:47:09
// RCS-ID:      $Id$
// Copyright:   public domain
// Licence:     
/////////////////////////////////////////////////////////////////////////////

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma implementation "dist_select_dialog.hpp"
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "remove_header_conflicts.hpp"


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

#include "dist_select_dialog.hpp"

#include <algo/structure/wx_tools/wx_tools.hpp>
using namespace ncbi;

////@begin XPM images
////@end XPM images

/*!
 * DistanceSelectDialog type definition
 */

IMPLEMENT_DYNAMIC_CLASS( DistanceSelectDialog, wxDialog )

/*!
 * DistanceSelectDialog event table definition
 */

BEGIN_EVENT_TABLE( DistanceSelectDialog, wxDialog )

////@begin DistanceSelectDialog event table entries
    EVT_BUTTON( ID_B_OK, DistanceSelectDialog::OnBOkClick )

    EVT_BUTTON( ID_B_CANCEL, DistanceSelectDialog::OnBCancelClick )

////@end DistanceSelectDialog event table entries

END_EVENT_TABLE()

/*!
 * DistanceSelectDialog constructors
 */

DistanceSelectDialog::DistanceSelectDialog( )
{
}

DistanceSelectDialog::DistanceSelectDialog( wxWindow* parent, wxWindowID id, const wxString& caption, const wxPoint& pos, const wxSize& size, long style )
{
    Create(parent, id, caption, pos, size, style);
}

/*!
 * DistanceSelectDialog creator
 */

bool DistanceSelectDialog::Create( wxWindow* parent, wxWindowID id, const wxString& caption, const wxPoint& pos, const wxSize& size, long style )
{
////@begin DistanceSelectDialog member initialisation
    m_Protein = NULL;
    m_Nucleotide = NULL;
    m_Heterogen = NULL;
    m_Solvent = NULL;
    m_Other = NULL;
////@end DistanceSelectDialog member initialisation

////@begin DistanceSelectDialog creation
    SetExtraStyle(GetExtraStyle()|wxWS_EX_BLOCK_EVENTS);
    wxDialog::Create( parent, id, caption, pos, size, style );

    CreateControls();
    GetSizer()->Fit(this);
    GetSizer()->SetSizeHints(this);
    Centre();
////@end DistanceSelectDialog creation
    return TRUE;
}

/*!
 * Control creation for DistanceSelectDialog
 */

void DistanceSelectDialog::CreateControls()
{    
////@begin DistanceSelectDialog content construction

    DistanceSelectDialog* itemDialog1 = this;

    wxBoxSizer* itemBoxSizer2 = new wxBoxSizer(wxVERTICAL);
    itemDialog1->SetSizer(itemBoxSizer2);
    itemDialog1->SetAutoLayout(TRUE);
    wxStaticBox* itemStaticBoxSizer3Static = new wxStaticBox(itemDialog1, wxID_ANY, _("Distance"));
    wxStaticBoxSizer* itemStaticBoxSizer3 = new wxStaticBoxSizer(itemStaticBoxSizer3Static, wxHORIZONTAL);
    itemBoxSizer2->Add(itemStaticBoxSizer3, 0, wxGROW|wxALL, 5);
    wxStaticText* itemStaticText4 = new wxStaticText( itemDialog1, wxID_STATIC, _("Enter a distance cutoff (in Angstroms):"), wxDefaultPosition, wxDefaultSize, 0 );
    itemStaticBoxSizer3->Add(itemStaticText4, 0, wxGROW|wxALL|wxADJUST_MINSIZE, 5);

    wxStaticBox* itemStaticBoxSizer5Static = new wxStaticBox(itemDialog1, wxID_ANY, _("Options"));
    wxStaticBoxSizer* itemStaticBoxSizer5 = new wxStaticBoxSizer(itemStaticBoxSizer5Static, wxHORIZONTAL);
    itemBoxSizer2->Add(itemStaticBoxSizer5, 0, wxGROW|wxALL, 5);
    wxFlexGridSizer* itemFlexGridSizer6 = new wxFlexGridSizer(0, 2, 0, 0);
    itemFlexGridSizer6->AddGrowableCol(0);
    itemStaticBoxSizer5->Add(itemFlexGridSizer6, 100, wxGROW|wxALL, 5);
    wxStaticText* itemStaticText7 = new wxStaticText( itemDialog1, wxID_STATIC, _("Select protein residues:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer6->Add(itemStaticText7, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

    wxCheckBox* itemCheckBox8 = new wxCheckBox( itemDialog1, ID_CHECKBOX, _T(""), wxDefaultPosition, wxDefaultSize, 0 );
    m_Protein = itemCheckBox8;
    itemCheckBox8->SetValue(FALSE);
    itemFlexGridSizer6->Add(itemCheckBox8, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText9 = new wxStaticText( itemDialog1, wxID_STATIC, _("Select nucleotide residues:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer6->Add(itemStaticText9, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

    wxCheckBox* itemCheckBox10 = new wxCheckBox( itemDialog1, ID_CHECKBOX1, _T(""), wxDefaultPosition, wxDefaultSize, 0 );
    m_Nucleotide = itemCheckBox10;
    itemCheckBox10->SetValue(FALSE);
    itemFlexGridSizer6->Add(itemCheckBox10, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText11 = new wxStaticText( itemDialog1, wxID_STATIC, _("Select heterogens:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer6->Add(itemStaticText11, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

    wxCheckBox* itemCheckBox12 = new wxCheckBox( itemDialog1, ID_CHECKBOX2, _T(""), wxDefaultPosition, wxDefaultSize, 0 );
    m_Heterogen = itemCheckBox12;
    itemCheckBox12->SetValue(FALSE);
    itemFlexGridSizer6->Add(itemCheckBox12, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText13 = new wxStaticText( itemDialog1, wxID_STATIC, _("Select solvent:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer6->Add(itemStaticText13, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

    wxCheckBox* itemCheckBox14 = new wxCheckBox( itemDialog1, ID_CHECKBOX3, _T(""), wxDefaultPosition, wxDefaultSize, 0 );
    m_Solvent = itemCheckBox14;
    itemCheckBox14->SetValue(FALSE);
    itemFlexGridSizer6->Add(itemCheckBox14, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemFlexGridSizer6->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    itemFlexGridSizer6->Add(5, 5, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxStaticText* itemStaticText17 = new wxStaticText( itemDialog1, wxID_STATIC, _("Select other molecules only:"), wxDefaultPosition, wxDefaultSize, 0 );
    itemFlexGridSizer6->Add(itemStaticText17, 0, wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL|wxALL|wxADJUST_MINSIZE, 5);

    wxCheckBox* itemCheckBox18 = new wxCheckBox( itemDialog1, ID_CHECKBOX4, _T(""), wxDefaultPosition, wxDefaultSize, 0 );
    m_Other = itemCheckBox18;
    itemCheckBox18->SetValue(FALSE);
    itemFlexGridSizer6->Add(itemCheckBox18, 0, wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxBoxSizer* itemBoxSizer19 = new wxBoxSizer(wxHORIZONTAL);
    itemBoxSizer2->Add(itemBoxSizer19, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);
    wxButton* itemButton20 = new wxButton( itemDialog1, ID_B_OK, _("Ok"), wxDefaultPosition, wxDefaultSize, 0 );
    itemButton20->SetDefault();
    itemBoxSizer19->Add(itemButton20, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

    wxButton* itemButton21 = new wxButton( itemDialog1, ID_B_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer19->Add(itemButton21, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

////@end DistanceSelectDialog content construction

    fpSpinCtrl = new FloatingPointSpinCtrl(this,
        0.0, 1000.0, 0.5, 5.0,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    itemStaticBoxSizer3->Add(fpSpinCtrl->GetTextCtrl(), 0, wxALIGN_CENTER_VERTICAL|wxLEFT|wxTOP|wxBOTTOM, 5);
    itemStaticBoxSizer3->Add(fpSpinCtrl->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);
}

/*!
 * Should we show tooltips?
 */

bool DistanceSelectDialog::ShowToolTips()
{
    return TRUE;
}

/*!
 * Get bitmap resources
 */

wxBitmap DistanceSelectDialog::GetBitmapResource( const wxString& name )
{
    // Bitmap retrieval
////@begin DistanceSelectDialog bitmap retrieval
    return wxNullBitmap;
////@end DistanceSelectDialog bitmap retrieval
}

/*!
 * Get icon resources
 */

wxIcon DistanceSelectDialog::GetIconResource( const wxString& name )
{
    // Icon retrieval
////@begin DistanceSelectDialog icon retrieval
    return wxNullIcon;
////@end DistanceSelectDialog icon retrieval
}
/*!
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_B_OK
 */

void DistanceSelectDialog::OnBOkClick( wxCommandEvent& event )
{
	double test;
    if (fpSpinCtrl->GetDouble(&test))
        EndModal(wxID_OK);
    else
        wxBell();
}

/*!
 * wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_B_CANCEL
 */

void DistanceSelectDialog::OnBCancelClick( wxCommandEvent& event )
{
    EndModal(wxID_CANCEL);
}


