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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/04/04 00:27:16  thiessen
* major update - add merging, threader GUI controls
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

#include "cn3d/wx_tools.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

/////////////////////////////////////////////////////////////////////////////////
// IntegerTextCtrl implementation
/////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(IntegerTextCtrl, wxTextCtrl)
    EVT_TEXT (-1, IntegerTextCtrl::Validate)
END_EVENT_TABLE()

IntegerTextCtrl::IntegerTextCtrl(wxWindow* parent, wxWindowID id, const wxString& value,
    const wxPoint& pos, const wxSize& size, long style,
    const wxValidator& validator, const wxString& name) :
        wxTextCtrl(parent, id, value, pos, size, style, validator, name),
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

void IntegerTextCtrl::SetAllowedRange(int min, int max)
{
    minVal = min;
    maxVal = max;
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

BEGIN_EVENT_TABLE(IntegerSpinCtrl, wxEvtHandler)
    EVT_SPIN_UP  (-1, IntegerSpinCtrl::OnSpinButtonUp)
    EVT_SPIN_DOWN(-1, IntegerSpinCtrl::OnSpinButtonDown)
END_EVENT_TABLE()

IntegerSpinCtrl::IntegerSpinCtrl(wxWindow* parent,
    int min, int max, int increment, int initial) :
        minVal(min), maxVal(max), incrVal(increment),
        spaceBetween(1)
{
    iTextCtrl = new IntegerTextCtrl(parent, -1, "", wxDefaultPosition, wxDefaultSize, 0);
    iTextCtrl->SetAllowedRange(min, max);
    preferredSize.SetHeight(iTextCtrl->GetBestSize().GetHeight());

    spinButton = new wxSpinButton(parent, -1,
        wxDefaultPosition, wxDefaultSize, wxSP_VERTICAL | wxSP_ARROW_KEYS);
    spinButton->PushEventHandler(this);
    preferredSize.SetWidth(
        iTextCtrl->GetBestSize().GetWidth() + spinButton->GetBestSize().GetWidth() + spaceBetween);

    spinButton->SetRange(-1, 1);    // position irrelevant; just need the button GUI
    SetInteger(initial);
}

void IntegerSpinCtrl::SetInteger(int value)
{
    // clamp to allowed range
    if (value < minVal) value = minVal;
    else if (value > maxVal) value = maxVal;

    wxString strVal;
    strVal.Printf("%i", value);
    iTextCtrl->SetValue(strVal);
    spinButton->SetValue(0);
}

void IntegerSpinCtrl::OnSpinButtonUp(wxSpinEvent& event)
{
    int value;
    if (!GetInteger(&value)) return;
    SetInteger(value + incrVal);
}

void IntegerSpinCtrl::OnSpinButtonDown(wxSpinEvent& event)
{
    int value;
    if (!GetInteger(&value)) return;
    SetInteger(value - incrVal);
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
    EVT_TEXT (-1, FloatingPointTextCtrl::Validate)
END_EVENT_TABLE()

FloatingPointTextCtrl::FloatingPointTextCtrl(wxWindow* parent, wxWindowID id, const wxString& value,
    const wxPoint& pos, const wxSize& size, long style,
    const wxValidator& validator, const wxString& name) :
        wxTextCtrl(parent, id, value, pos, size, style, validator, name),
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

BEGIN_EVENT_TABLE(FloatingPointSpinCtrl, wxEvtHandler)
    EVT_SPIN_UP  (-1, FloatingPointSpinCtrl::OnSpinButtonUp)
    EVT_SPIN_DOWN(-1, FloatingPointSpinCtrl::OnSpinButtonDown)
END_EVENT_TABLE()

FloatingPointSpinCtrl::FloatingPointSpinCtrl(wxWindow* parent,
    double min, double max, double increment, double initial) :
        minVal(min), maxVal(max), incrVal(increment),
        spaceBetween(1)
{
    fpTextCtrl = new FloatingPointTextCtrl(parent, -1, "", wxDefaultPosition, wxDefaultSize, 0);
    fpTextCtrl->SetAllowedRange(min, max);
    preferredSize.SetHeight(fpTextCtrl->GetBestSize().GetHeight());

    spinButton = new wxSpinButton(parent, -1,
        wxDefaultPosition, wxDefaultSize, wxSP_VERTICAL | wxSP_ARROW_KEYS);
    spinButton->PushEventHandler(this);
    preferredSize.SetWidth(
        fpTextCtrl->GetBestSize().GetWidth() + spinButton->GetBestSize().GetWidth() + spaceBetween);

    spinButton->SetRange(-1, 1);    // position irrelevant; just need the button GUI
    SetDouble(initial);
}

void FloatingPointSpinCtrl::SetDouble(double value)
{
    // clamp to allowed range
    if (value < minVal) value = minVal;
    else if (value > maxVal) value = maxVal;

    wxString strVal;
    strVal.Printf("%g", value);
    fpTextCtrl->SetValue(strVal);
    spinButton->SetValue(0);
}

void FloatingPointSpinCtrl::OnSpinButtonUp(wxSpinEvent& event)
{
    double value;
    if (!GetDouble(&value)) return;
    SetDouble(value + incrVal);
}

void FloatingPointSpinCtrl::OnSpinButtonDown(wxSpinEvent& event)
{
    double value;
    if (!GetDouble(&value)) return;
    SetDouble(value - incrVal);
}

bool FloatingPointSpinCtrl::GetDouble(double *value) const
{
    return (fpTextCtrl->GetValue().ToDouble(value) && fpTextCtrl->IsValidDouble());
}


/////////////////////////////////////////////////////////////////////////////////
// GetFloatingPointDialog implementation
/////////////////////////////////////////////////////////////////////////////////

BEGIN_EVENT_TABLE(GetFloatingPointDialog, wxFrame)
    EVT_BUTTON(-1, GetFloatingPointDialog::OnButton)
    EVT_CLOSE (    GetFloatingPointDialog::OnCloseWindow)
END_EVENT_TABLE()

GetFloatingPointDialog::GetFloatingPointDialog(wxWindow* parent,
    const wxString& message, const wxString& title,
    double min, double max, double increment, double initial) :
        wxFrame(parent, -1, title, wxDefaultPosition, wxDefaultSize,
            wxCAPTION | wxSYSTEM_MENU // not resizable
//            wxDEFAULT_FRAME_STYLE
)
{
    SetBackgroundColour(*wxLIGHT_GREY);

    wxStaticText *text = new wxStaticText(this, -1, "", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE);
    text->SetLabel(message);

    fpSpinCtrl = new FloatingPointSpinCtrl(this, min, max, increment, initial);
    buttonOK = new wxButton(this, -1, "&OK");

    static const int margin = 1;
    int minWidth = fpSpinCtrl->GetBestSize().GetWidth() + buttonOK->GetBestSize().GetWidth() + 3*margin;
    if (text->GetBestSize().GetWidth() + 30 > minWidth) minWidth = text->GetBestSize().GetWidth() + 30;
    SetClientSize(
        minWidth,
        fpSpinCtrl->GetBestSize().GetHeight() + text->GetBestSize().GetHeight() + 3*margin + 20);

    wxLayoutConstraints *c;

    c = new wxLayoutConstraints();
    c->top.SameAs           (this,      wxTop,      margin + 10);
    // bug in wxWin makes this not work, and they refuse to patch it! :(
//    c->bottom.Above         (fpSpinCtrl,                margin + 10);
    c->bottom.Above         (fpSpinCtrl->GetTextCtrl(),
                                                    -(margin + 10));    // use this to work around
    c->left.SameAs          (this,      wxLeft,     margin + 10);
    c->right.SameAs         (this,      wxRight,    margin + 10);
    text->SetConstraints(c);

    c = new wxLayoutConstraints();
    c->bottom.SameAs        (this,      wxBottom,   margin);
    c->height.SameAs        (fpSpinCtrl->GetTextCtrl(),
                                        wxHeight);
    c->left.SameAs          (this,      wxLeft,     margin);
    c->width.AsIs           ();
    buttonOK->SetConstraints(c);

    c = new wxLayoutConstraints();
    c->bottom.SameAs        (this,      wxBottom,   margin);
    c->height.AsIs          ();
    c->left.RightOf         (buttonOK,              margin);
    c->right.LeftOf         (fpSpinCtrl->GetSpinButton(),
                                        fpSpinCtrl->GetSpaceBetweenControls());
    fpSpinCtrl->GetTextCtrl()->SetConstraints(c);

    c = new wxLayoutConstraints();
    c->bottom.SameAs        (this,      wxBottom,   margin);
    c->height.SameAs        (fpSpinCtrl->GetTextCtrl(),
                                        wxHeight);
    c->right.SameAs         (this,      wxRight,    margin);
    c->width.AsIs           ();
    fpSpinCtrl->GetSpinButton()->SetConstraints(c);

    SetAutoLayout(true);
}

double GetFloatingPointDialog::GetValue(void)
{
    return returnValue;
}

void GetFloatingPointDialog::OnButton(wxCommandEvent& event)
{
    if (event.GetEventObject() == buttonOK) {
        returnOK = true;
        if (fpSpinCtrl->GetDouble(&returnValue))
            EndEventLoop();
        else
            wxBell();
    } else {
        event.Skip();
    }
}

bool GetFloatingPointDialog::Activate(void)
{
    dialogActive = true;
    Show(true);
    MakeModal(true);

    // enter the modal loop  (this code snippet borrowed from src/msw/dialog.cpp)
    while (dialogActive)
    {
#if wxUSE_THREADS
        wxMutexGuiLeaveOrEnter();
#endif // wxUSE_THREADS
        while (!wxTheApp->Pending() && wxTheApp->ProcessIdle()) ;
        // a message came or no more idle processing to do
        wxTheApp->DoMessage();
    }

    MakeModal(false);
    return returnOK;
}

void GetFloatingPointDialog::EndEventLoop(void)
{
    dialogActive = false;
}

void GetFloatingPointDialog::OnCloseWindow(wxCommandEvent& event)
{
    returnOK = false;
    EndEventLoop();
}

END_SCOPE(Cn3D)
