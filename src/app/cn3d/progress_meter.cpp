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
*      progress meter window
*
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "progress_meter.hpp"
#include "cn3d_tools.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

#define DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERRORMSG("Can't find window with id " << id); \
        return; \
    }

BEGIN_EVENT_TABLE(ProgressMeter, wxDialog)
    EVT_CLOSE       (       ProgressMeter::OnCloseWindow)
END_EVENT_TABLE()

ProgressMeter::ProgressMeter(wxWindow *myParent,
        const wxString& message, const wxString& title, int maximumValue) :
    wxDialog(myParent, -1, title, wxPoint(50, 50), wxDefaultSize,
        wxCAPTION | wxDIALOG_MODELESS | wxFRAME_NO_TASKBAR) // not closeable or resizable
{
    // construct the panel
    wxPanel *parent = new wxPanel(this, -1);

////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken more or less from wxDesigner's C++ code from progress_meter.wdr,
// except changing message and setting gauge style to wxGA_HORIZONTAL | wxGA_SMOOTH
////////////////////////////////////////////////////////////////////////////////////////////////
#define ID_TEXT 10000
#define ID_GAUGE 10001
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );
    wxStaticText *item1 = new wxStaticText( parent, ID_TEXT, message,
        wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item0->Add( item1, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    wxGauge *item2 = new wxGauge( parent, ID_GAUGE, 100,
        wxDefaultPosition, wxDefaultSize, wxGA_HORIZONTAL | wxGA_SMOOTH );
    item0->Add( item2, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    parent->SetAutoLayout( TRUE );
    parent->SetSizer( item0 );

    // set message
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(text, ID_TEXT, wxStaticText)
    text->SetLabel(message);
    text->Fit();

    // setup gauge
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(gauge, ID_GAUGE, wxGauge)
    gauge->SetRange(maximumValue);
    gauge->SetValue(0);

    // call sizer stuff
    item0->Fit(parent);
    SetClientSize(item0->GetMinSize());

    // automatically bring up the window and let it be shown right away
    Show(true);

#if defined(__WXGTK__) || defined(__WXMAC__)
    // wxSafeYield seems to force redraws in wxGTK and does ugly things in
    // wxMac, so make window modal so that it's safe to call wxYield() instead
    SetFocus();
    MakeModal(true);
    wxYield();
#else
    wxSafeYield();
#endif
}

void ProgressMeter::OnCloseWindow(wxCloseEvent& event)
{
#if defined(__WXGTK__) || defined(__WXMAC__)
    TRACEMSG("can veto: " << event.CanVeto());
    if (event.CanVeto()) {
        event.Veto();
    } else {
        MakeModal(false);
        Show(false);
    }
#endif
}

void ProgressMeter::SetValue(int value, bool doYield)
{
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(gauge, ID_GAUGE, wxGauge)

    if (value != gauge->GetValue()) {
        int max = gauge->GetRange();
        gauge->SetValue((value <= 0) ? 0 : ((value >= max) ? max : value));

        // yield for window redraw
        if (doYield)
#if defined(__WXGTK__) || defined(__WXMAC__)
            wxYield();  // wxSafeYield is ugly on these platforms
#else
            wxSafeYield();
#endif
    }
}

END_SCOPE(Cn3D)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.7  2004/02/19 17:05:04  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.6  2003/02/03 19:20:05  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.5  2002/08/15 22:13:15  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.4  2001/10/25 17:17:23  thiessen
* use wxYield + modal for wxMac, too
*
* Revision 1.3  2001/10/25 00:06:29  thiessen
* fix concurrent rendering problem in wxMSW PNG output
*
* Revision 1.2  2001/10/24 17:07:30  thiessen
* add PNG output for wxGTK
*
* Revision 1.1  2001/10/23 13:53:40  thiessen
* add PNG export
*
*/
