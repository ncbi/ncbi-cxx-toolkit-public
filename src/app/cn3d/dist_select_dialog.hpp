/////////////////////////////////////////////////////////////////////////////
// Name:        dist_select_dialog.hpp
// Purpose:     
// Author:      Paul Thiessen
// Modified by: 
// Created:     10/05/04 08:47:09
// RCS-ID:      $Id$
// Copyright:   public domain
// Licence:     
/////////////////////////////////////////////////////////////////////////////

#ifndef _DIST_SELECT_DIALOG_H_
#define _DIST_SELECT_DIALOG_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "dist_select_dialog.cpp"
#endif

/*!
 * Includes
 */

////@begin includes
////@end includes

/*!
 * Forward declarations
 */

////@begin forward declarations
////@end forward declarations

namespace ncbi {
class FloatingPointSpinCtrl;
};

/*!
 * Control identifiers
 */

////@begin control identifiers
#define ID_DIALOG 10000
#define SYMBOL_DISTANCESELECTDIALOG_STYLE wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX
#define SYMBOL_DISTANCESELECTDIALOG_TITLE _("Select By Distance")
#define SYMBOL_DISTANCESELECTDIALOG_IDNAME ID_DIALOG
#define SYMBOL_DISTANCESELECTDIALOG_SIZE wxSize(400, 300)
#define SYMBOL_DISTANCESELECTDIALOG_POSITION wxDefaultPosition
#define ID_CHECKBOX 10005
#define ID_CHECKBOX1 10006
#define ID_CHECKBOX2 10007
#define ID_CHECKBOX3 10008
#define ID_CHECKBOX4 10009
#define ID_B_OK 10003
#define ID_B_CANCEL 10004
////@end control identifiers

/*!
 * Compatibility
 */

#ifndef wxCLOSE_BOX
#define wxCLOSE_BOX 0x1000
#endif
#ifndef wxFIXED_MINSIZE
#define wxFIXED_MINSIZE 0
#endif

/*!
 * DistanceSelectDialog class declaration
 */

class DistanceSelectDialog: public wxDialog
{    
    DECLARE_DYNAMIC_CLASS( DistanceSelectDialog )
    DECLARE_EVENT_TABLE()

public:
    /// Constructors
    DistanceSelectDialog( );
    DistanceSelectDialog( wxWindow* parent, wxWindowID id = SYMBOL_DISTANCESELECTDIALOG_IDNAME, const wxString& caption = SYMBOL_DISTANCESELECTDIALOG_TITLE, const wxPoint& pos = SYMBOL_DISTANCESELECTDIALOG_POSITION, const wxSize& size = SYMBOL_DISTANCESELECTDIALOG_SIZE, long style = SYMBOL_DISTANCESELECTDIALOG_STYLE );

    /// Creation
    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_DISTANCESELECTDIALOG_IDNAME, const wxString& caption = SYMBOL_DISTANCESELECTDIALOG_TITLE, const wxPoint& pos = SYMBOL_DISTANCESELECTDIALOG_POSITION, const wxSize& size = SYMBOL_DISTANCESELECTDIALOG_SIZE, long style = SYMBOL_DISTANCESELECTDIALOG_STYLE );

    /// Creates the controls and sizers
    void CreateControls();

////@begin DistanceSelectDialog event handler declarations

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_B_OK
    void OnBOkClick( wxCommandEvent& event );

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_B_CANCEL
    void OnBCancelClick( wxCommandEvent& event );

////@end DistanceSelectDialog event handler declarations

////@begin DistanceSelectDialog member function declarations

    /// Retrieves bitmap resources
    wxBitmap GetBitmapResource( const wxString& name );

    /// Retrieves icon resources
    wxIcon GetIconResource( const wxString& name );
////@end DistanceSelectDialog member function declarations

    /// Should we show tooltips?
    static bool ShowToolTips();

////@begin DistanceSelectDialog member variables
    wxCheckBox* m_Protein;
    wxCheckBox* m_Nucleotide;
    wxCheckBox* m_Heterogen;
    wxCheckBox* m_Solvent;
    wxCheckBox* m_Other;
////@end DistanceSelectDialog member variables

    ncbi::FloatingPointSpinCtrl *fpSpinCtrl;
};

#endif
    // _DIST_SELECT_DIALOG_H_
