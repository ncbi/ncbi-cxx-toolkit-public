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
*      custom wx GUI controls
*
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

#include "wx_tools.hpp"
#include "cn3d_tools.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

const int WX_TOOLS_NOTIFY_CHANGED = wxNewEventType();

#define SEND_CHANGED_EVENT do { \
    wxCommandEvent notify(WX_TOOLS_NOTIFY_CHANGED); \
    AddPendingEvent(notify); } while (0)


/////////////////////////////////////////////////////////////////////////////////
// NotifyingSpinButton implementation
/////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(NotifyingSpinButton, wxSpinButton)
    EVT_SPIN_UP  (-1, NotifyingSpinButton::OnSpinButtonUp)
    EVT_SPIN_DOWN(-1, NotifyingSpinButton::OnSpinButtonDown)
END_EVENT_TABLE()

void NotifyingSpinButton::OnSpinButtonUp(wxSpinEvent& event)
{
    notify->OnSpinButtonUp(event);
    SEND_CHANGED_EVENT;
}

void NotifyingSpinButton::OnSpinButtonDown(wxSpinEvent& event)
{
    notify->OnSpinButtonDown(event);
    SEND_CHANGED_EVENT;
}


/////////////////////////////////////////////////////////////////////////////////
// IntegerTextCtrl implementation
/////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(IntegerTextCtrl, wxTextCtrl)
    EVT_TEXT        (-1, IntegerTextCtrl::Validate)
    EVT_TEXT_ENTER  (-1, IntegerTextCtrl::OnChange)
END_EVENT_TABLE()

IntegerTextCtrl::IntegerTextCtrl(wxWindow* parent, wxWindowID id, const wxString& value,
    const wxPoint& pos, const wxSize& size, long style,
    const wxValidator& validator, const wxString& name) :
        wxTextCtrl(parent, id, value, pos, size, style | wxTE_PROCESS_ENTER, validator, name),
        minVal(kMin_Long), maxVal(kMax_Long)
{
}

void IntegerTextCtrl::Validate(wxCommandEvent& event)
{
    if (IsValidInteger())
        SetBackgroundColour(*wxWHITE);
    else
        SetBackgroundColour(*wxRED);
    Refresh();
}

void IntegerTextCtrl::OnChange(wxCommandEvent& event)
{
    SEND_CHANGED_EVENT;
}

void IntegerTextCtrl::SetAllowedRange(int min, int max, int incr)
{
    minVal = min;
    maxVal = max;
    incrVal = incr;
}

bool IntegerTextCtrl::IsValidInteger(void) const
{
    long longVal;
    int intVal;
    if (!GetValue().ToLong(&longVal)) return false;
    intVal = (int) longVal;
    return (intVal >= minVal && intVal <= maxVal);
}


/////////////////////////////////////////////////////////////////////////////////
// IntegerSpinCtrl implementation
/////////////////////////////////////////////////////////////////////////////////

IntegerSpinCtrl::IntegerSpinCtrl(wxWindow* parent,
    int min, int max, int increment, int initial,
    const wxPoint& textCtrlPos, const wxSize& textCtrlSize, long textCtrlStyle,
    const wxPoint& spinCtrlPos, const wxSize& spinCtrlSize) :
        minVal(min), maxVal(max), incrVal(increment)
{
    iTextCtrl = new IntegerTextCtrl(parent, -1, "", textCtrlPos, textCtrlSize, textCtrlStyle);
    iTextCtrl->SetAllowedRange(min, max, increment);

    spinButton = new NotifyingSpinButton(this,
        parent, -1, spinCtrlPos, spinCtrlSize, wxSP_VERTICAL | wxSP_ARROW_KEYS);
    spinButton->SetRange(-1, 1);    // position irrelevant; just need the button GUI

    // clamp and set initial value
    if (initial < min) initial = min;
    if (initial > max) initial = max;
    SetInteger(initial);
}

bool IntegerSpinCtrl::SetInteger(int value)
{
    // check for allowed value
    if (value < minVal || value > maxVal) return false;

    wxString strVal;
    strVal.Printf("%i", value);
    iTextCtrl->SetValue(strVal);
    spinButton->SetValue(0);
    return true;
}

void IntegerSpinCtrl::OnSpinButtonUp(wxSpinEvent& event)
{
    int value;
    if (!GetInteger(&value)) return;
    value += incrVal;
    if (value > maxVal) value = maxVal;
    SetInteger(value);
}

void IntegerSpinCtrl::OnSpinButtonDown(wxSpinEvent& event)
{
    int value;
    if (!GetInteger(&value)) return;
    value -= incrVal;
    if (value < minVal) value = minVal;
    SetInteger(value);
}

bool IntegerSpinCtrl::GetInteger(int *value) const
{
	long longValue;
	if (!iTextCtrl->GetValue().ToLong(&longValue)) return false;
	*value = (int) longValue;
    return (iTextCtrl->IsValidInteger());
}


/////////////////////////////////////////////////////////////////////////////////
// FloatingPointTextCtrl implementation
/////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(FloatingPointTextCtrl, wxTextCtrl)
    EVT_TEXT        (-1, FloatingPointTextCtrl::Validate)
    EVT_TEXT_ENTER  (-1, FloatingPointTextCtrl::OnChange)
END_EVENT_TABLE()

FloatingPointTextCtrl::FloatingPointTextCtrl(wxWindow* parent, wxWindowID id, const wxString& value,
    const wxPoint& pos, const wxSize& size, long style,
    const wxValidator& validator, const wxString& name) :
        wxTextCtrl(parent, id, value, pos, size, style | wxTE_PROCESS_ENTER, validator, name),
        minVal(kMin_Double), maxVal(kMax_Double)
{
}

void FloatingPointTextCtrl::Validate(wxCommandEvent& event)
{
    if (IsValidDouble())
        SetBackgroundColour(*wxWHITE);
    else
        SetBackgroundColour(*wxRED);
    Refresh();
}

void FloatingPointTextCtrl::OnChange(wxCommandEvent& event)
{
    SEND_CHANGED_EVENT;
}

void FloatingPointTextCtrl::SetAllowedRange(double min, double max)
{
    minVal = min;
    maxVal = max;
}

bool FloatingPointTextCtrl::IsValidDouble(void) const
{
    double doubleVal;
    return (GetValue().ToDouble(&doubleVal) && doubleVal >= minVal && doubleVal <= maxVal);
}


/////////////////////////////////////////////////////////////////////////////////
// FloatingPointSpinCtrl implementation
/////////////////////////////////////////////////////////////////////////////////

FloatingPointSpinCtrl::FloatingPointSpinCtrl(wxWindow* parent,
    double min, double max, double increment, double initial,
    const wxPoint& textCtrlPos, const wxSize& textCtrlSize, long textCtrlStyle,
    const wxPoint& spinCtrlPos, const wxSize& spinCtrlSize) :
        minVal(min), maxVal(max), incrVal(increment)
{
    fpTextCtrl = new FloatingPointTextCtrl(parent, -1, "", textCtrlPos, textCtrlSize, textCtrlStyle);
    fpTextCtrl->SetAllowedRange(min, max);

    spinButton = new NotifyingSpinButton(this,
        parent, -1, spinCtrlPos, spinCtrlSize, wxSP_VERTICAL | wxSP_ARROW_KEYS);
    spinButton->SetRange(-1, 1);    // position irrelevant; just need the button GUI

    // clamp and set initial value
    if (initial < min) initial = min;
    if (initial > max) initial = max;
    SetDouble(initial);
}

bool FloatingPointSpinCtrl::SetDouble(double value)
{
    // check allowed range
    if (value < minVal || value > maxVal) return false;

    wxString strVal;
    strVal.Printf("%g", value);
    fpTextCtrl->SetValue(strVal);
    spinButton->SetValue(0);
    return true;
}

void FloatingPointSpinCtrl::OnSpinButtonUp(wxSpinEvent& event)
{
    double value;
    if (!GetDouble(&value)) return;
    value += incrVal;
    if (value > maxVal) value = maxVal;
    SetDouble(value);
}

void FloatingPointSpinCtrl::OnSpinButtonDown(wxSpinEvent& event)
{
    double value;
    if (!GetDouble(&value)) return;
    value -= incrVal;
    if (value < minVal) value = minVal;
    SetDouble(value);
}

bool FloatingPointSpinCtrl::GetDouble(double *value) const
{
    return (fpTextCtrl->GetValue().ToDouble(value) && fpTextCtrl->IsValidDouble());
}


/////////////////////////////////////////////////////////////////////////////////
// GetFloatingPointDialog implementation
/////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(GetFloatingPointDialog, wxDialog)
    EVT_BUTTON(-1,  GetFloatingPointDialog::OnButton)
    EVT_CLOSE (     GetFloatingPointDialog::OnCloseWindow)
END_EVENT_TABLE()

GetFloatingPointDialog::GetFloatingPointDialog(wxWindow* parent,
    const wxString& message, const wxString& title,
    double min, double max, double increment, double initial) :
        wxDialog(parent, -1, title, wxDefaultPosition, wxDefaultSize,
            wxCAPTION | wxSYSTEM_MENU) // not resizable
{
    // code modified (heavily) from wxDesigner C++ output of fp_dialog.wdr
    wxPanel *panel = new wxPanel(this, -1);
    wxBoxSizer *item0 = new wxBoxSizer(wxVERTICAL);
    wxStaticText *item1 = new wxStaticText(panel, -1, message, wxDefaultPosition, wxDefaultSize, 0);
    item0->Add(item1, 0, wxALIGN_CENTRE|wxALL, 5);
    wxFlexGridSizer *grid = new wxFlexGridSizer(1, 0, 0, 0);
    grid->AddGrowableCol(1);

    buttonOK = new wxButton(panel, -1, "OK", wxDefaultPosition, wxDefaultSize, 0);
    grid->Add(buttonOK, 0, wxGROW|wxALIGN_CENTER_HORIZONTAL|wxRIGHT, 5);

    fpSpinCtrl = new FloatingPointSpinCtrl(panel,
        min, max, increment, initial,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    grid->Add(fpSpinCtrl->GetTextCtrl(), 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5);
    grid->Add(fpSpinCtrl->GetSpinButton(), 0, wxALIGN_CENTRE, 5);

    item0->Add(grid, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    panel->SetAutoLayout(true);
    panel->SetSizer(item0);
    item0->Fit(this);
    item0->Fit(panel);
    item0->SetSizeHints(this);
}

GetFloatingPointDialog::~GetFloatingPointDialog(void)
{
    delete fpSpinCtrl;
}

double GetFloatingPointDialog::GetValue(void)
{
	double returnValue;
	fpSpinCtrl->GetDouble(&returnValue);
    return returnValue;
}

void GetFloatingPointDialog::OnButton(wxCommandEvent& event)
{
    if (event.GetEventObject() == buttonOK) {
		double test;
        if (fpSpinCtrl->GetDouble(&test))
            EndModal(wxOK);
        else
            wxBell();
    } else {
        event.Skip();
    }
}

void GetFloatingPointDialog::OnCloseWindow(wxCloseEvent& event)
{
    EndModal(wxCANCEL);
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  2004/05/21 21:41:40  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.17  2004/02/19 17:05:23  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.16  2003/02/03 19:20:09  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.15  2002/10/11 17:21:39  thiessen
* initial Mac OSX build
*
* Revision 1.14  2002/08/15 22:13:19  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.13  2002/05/22 17:17:10  thiessen
* progress on BLAST interface ; change custom spin ctrl implementation
*
* Revision 1.12  2002/04/27 16:32:16  thiessen
* fix small leaks/bugs found by BoundsChecker
*
* Revision 1.11  2002/02/21 22:01:50  thiessen
* remember alignment range on demotion
*
* Revision 1.10  2001/11/27 16:26:11  thiessen
* major update to data management system
*
* Revision 1.9  2001/10/08 00:00:10  thiessen
* estimate threader N random starts; edit CDD name
*
* Revision 1.8  2001/09/24 14:37:53  thiessen
* more wxPanel stuff - fix for new heirarchy in wx 2.3.2+
*
* Revision 1.7  2001/09/24 13:29:55  thiessen
* fix wxPanel issues
*
* Revision 1.6  2001/08/06 20:22:01  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* Revision 1.5  2001/06/08 14:47:06  thiessen
* fully functional (modal) render settings panel
*
* Revision 1.4  2001/05/23 17:45:41  thiessen
* change dialog implementation to wxDesigner; interface changes
*
* Revision 1.3  2001/05/17 18:34:27  thiessen
* spelling fixes; change dialogs to inherit from wxDialog
*
* Revision 1.2  2001/05/15 23:48:39  thiessen
* minor adjustments to compile under Solaris/wxGTK
*
* Revision 1.1  2001/04/04 00:27:16  thiessen
* major update - add merging, threader GUI controls
*
*/
