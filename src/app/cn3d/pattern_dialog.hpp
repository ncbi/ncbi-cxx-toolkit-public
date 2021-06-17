/////////////////////////////////////////////////////////////////////////////
// Name:        pattern_dialog.hpp
// Purpose:     
// Author:      
// Modified by: 
// Created:     10/01/04 10:47:06
// RCS-ID:      $Id$
// Copyright:   
// Licence:     
/////////////////////////////////////////////////////////////////////////////

#ifndef _PATTERN_DIALOG_H_
#define _PATTERN_DIALOG_H_

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma interface "pattern_dialog.cpp"
#endif

/*!
 * Includes
 */

////@begin includes
#include "wx/statline.h"
////@end includes

/*!
 * Forward declarations
 */

////@begin forward declarations
////@end forward declarations

/*!
 * Control identifiers
 */

////@begin control identifiers
#define ID_DIALOG 10000
#define SYMBOL_PATTERNDIALOG_STYLE wxCAPTION|wxSYSTEM_MENU|wxCLOSE_BOX
#define SYMBOL_PATTERNDIALOG_TITLE _("Pattern Search")
#define SYMBOL_PATTERNDIALOG_IDNAME ID_DIALOG
#define SYMBOL_PATTERNDIALOG_SIZE wxSize(400, 300)
#define SYMBOL_PATTERNDIALOG_POSITION wxDefaultPosition
#define ID_T_PATTERN 10001
#define ID_RADIOBOX 10002
#define ID_OK 10003
#define ID_CANCEL 10004
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
 * PatternDialog class declaration
 */

class PatternDialog: public wxDialog
{    
    DECLARE_DYNAMIC_CLASS( PatternDialog )
    DECLARE_EVENT_TABLE()

public:
    /// Constructors
    PatternDialog( );
    PatternDialog( wxWindow* parent, wxWindowID id = SYMBOL_PATTERNDIALOG_IDNAME, const wxString& caption = SYMBOL_PATTERNDIALOG_TITLE, const wxPoint& pos = SYMBOL_PATTERNDIALOG_POSITION, const wxSize& size = SYMBOL_PATTERNDIALOG_SIZE, long style = SYMBOL_PATTERNDIALOG_STYLE );

    /// Creation
    bool Create( wxWindow* parent, wxWindowID id = SYMBOL_PATTERNDIALOG_IDNAME, const wxString& caption = SYMBOL_PATTERNDIALOG_TITLE, const wxPoint& pos = SYMBOL_PATTERNDIALOG_POSITION, const wxSize& size = SYMBOL_PATTERNDIALOG_SIZE, long style = SYMBOL_PATTERNDIALOG_STYLE );

    /// Creates the controls and sizers
    void CreateControls();

////@begin PatternDialog event handler declarations

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_OK
    void OnOkClick( wxCommandEvent& event );

    /// wxEVT_COMMAND_BUTTON_CLICKED event handler for ID_CANCEL
    void OnCancelClick( wxCommandEvent& event );

////@end PatternDialog event handler declarations

////@begin PatternDialog member function declarations


    /// Retrieves bitmap resources
    wxBitmap GetBitmapResource( const wxString& name );

    /// Retrieves icon resources
    wxIcon GetIconResource( const wxString& name );
////@end PatternDialog member function declarations

    /// Should we show tooltips?
    static bool ShowToolTips();

////@begin PatternDialog member variables
    wxTextCtrl* m_Pattern;
    wxRadioBox* m_Mode;
    wxStaticText* m_Text;
////@end PatternDialog member variables
};

#endif
    // _PATTERN_DIALOG_H_
