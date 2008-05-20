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

#ifndef CN3D_WX_TOOLS__HPP
#define CN3D_WX_TOOLS__HPP

#include <corelib/ncbistd.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>


BEGIN_SCOPE(Cn3D)

// "spin control" height
#if defined(__WXMSW__)
static const int SPIN_CTRL_HEIGHT = 20;
#elif defined(__WXGTK__)
static const int SPIN_CTRL_HEIGHT = 20;
#elif defined(__WXMAC__)
static const int SPIN_CTRL_HEIGHT = 24;
#endif

extern const int WX_TOOLS_NOTIFY_CHANGED;


/////////////////////////////////////////////////////////////////////////////////////
// the class from which my custom integer/float spin ctrls are derived
/////////////////////////////////////////////////////////////////////////////////////

class NotifyingSpinButton;

class CustomSpinCtrl
{
    friend class NotifyingSpinButton;

private:
    virtual void OnSpinButtonUp(wxSpinEvent& event) = 0;
    virtual void OnSpinButtonDown(wxSpinEvent& event) = 0;

public:
    virtual ~CustomSpinCtrl(void) { }
};


/////////////////////////////////////////////////////////////////////////////////////
// a spin button that notifies a parent when button pressed
/////////////////////////////////////////////////////////////////////////////////////

class NotifyingSpinButton : public wxSpinButton
{
private:
    CustomSpinCtrl *notify;
    void OnSpinButtonUp(wxSpinEvent& event);
    void OnSpinButtonDown(wxSpinEvent& event);

    DECLARE_EVENT_TABLE()

public:
    NotifyingSpinButton(CustomSpinCtrl* toNotify,
            wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition,
            const wxSize& size = wxDefaultSize, long style = wxSP_HORIZONTAL) :
        wxSpinButton(parent, id, pos, size, style), notify(toNotify) { }
};


/////////////////////////////////////////////////////////////////////////////////////
// a wxTextCtrl that only accepts valid floating point values (turns red otherwise)
/////////////////////////////////////////////////////////////////////////////////////

class IntegerTextCtrl : public wxTextCtrl
{
public:
    IntegerTextCtrl(wxWindow* parent, wxWindowID id, const wxString& value = wxEmptyString,
        const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0,
        const wxValidator& validator = wxDefaultValidator, const wxString& name = wxTextCtrlNameStr);

    void SetAllowedRange(int min, int max, int incr);
    bool IsValidInteger(void) const;

private:
    int minVal, maxVal, incrVal;

    void DoValidate(wxCommandEvent& event);
    void OnChange(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};


/////////////////////////////////////////////////////////////////////////////////
// my own special integer spin control - for more control over text box
/////////////////////////////////////////////////////////////////////////////////

class IntegerSpinCtrl : public CustomSpinCtrl
{
public:
    IntegerSpinCtrl(wxWindow* parent,
        int min, int max, int increment, int initial,
        const wxPoint& textCtrlPos, const wxSize& textCtrlSize, long textCtrlStyle,
        const wxPoint& spinCtrlPos, const wxSize& spinCtrlSize);
    virtual ~IntegerSpinCtrl(void) { }

    bool GetInteger(int *value) const;
    bool GetUnsignedInteger(unsigned int *value) const;
    bool SetInteger(int value);

private:
    IntegerTextCtrl *iTextCtrl;
    NotifyingSpinButton *spinButton;
    int minVal, maxVal, incrVal;

    void OnSpinButtonUp(wxSpinEvent& event);
    void OnSpinButtonDown(wxSpinEvent& event);

public:
    wxTextCtrl * GetTextCtrl(void) const { return iTextCtrl; }
    wxSpinButton * GetSpinButton(void) const { return spinButton; }
};


/////////////////////////////////////////////////////////////////////////////////////
// a wxTextCtrl that only accepts valid floating point values (turns red otherwise)
/////////////////////////////////////////////////////////////////////////////////////

class FloatingPointTextCtrl : public wxTextCtrl
{
public:
    FloatingPointTextCtrl(wxWindow* parent, wxWindowID id, const wxString& value = wxEmptyString,
        const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = 0,
        const wxValidator& validator = wxDefaultValidator, const wxString& name = wxTextCtrlNameStr);

    void SetAllowedRange(double min, double max);
    bool IsValidDouble(void) const;

private:
    double minVal, maxVal;

    void DoValidate(wxCommandEvent& event);
    void OnChange(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};


/////////////////////////////////////////////////////////////////////////////////
// like wxSpinCtrl, except works on floating point values
/////////////////////////////////////////////////////////////////////////////////

class FloatingPointSpinCtrl : public CustomSpinCtrl
{
public:
    FloatingPointSpinCtrl(wxWindow* parent,
        double min, double max, double increment, double initial,
        const wxPoint& textCtrlPos, const wxSize& textCtrlSize, long textCtrlStyle,
        const wxPoint& spinCtrlPos, const wxSize& spinCtrlSize);
    virtual ~FloatingPointSpinCtrl(void) { }

    bool GetDouble(double *value) const;
    bool SetDouble(double value);

private:
    FloatingPointTextCtrl *fpTextCtrl;
    NotifyingSpinButton *spinButton;
    double minVal, maxVal, incrVal;

    void OnSpinButtonUp(wxSpinEvent& event);
    void OnSpinButtonDown(wxSpinEvent& event);

public:
    wxTextCtrl * GetTextCtrl(void) const { return fpTextCtrl; }
    wxSpinButton * GetSpinButton(void) const { return spinButton; }
};


/////////////////////////////////////////////////////////////////////////////////
// dialog that asks user for a floating point value
/////////////////////////////////////////////////////////////////////////////////

class GetFloatingPointDialog : public wxDialog
{
public:
    GetFloatingPointDialog(wxWindow* parent, const wxString& message, const wxString& title,
        double min, double max, double increment, double initial);
    ~GetFloatingPointDialog(void);

    double GetValue(void);

private:
    wxButton *buttonOK;
    FloatingPointSpinCtrl *fpSpinCtrl;

    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);

    DECLARE_EVENT_TABLE()
};

END_SCOPE(Cn3D)

#endif // CN3D_WX_TOOLS__HPP
