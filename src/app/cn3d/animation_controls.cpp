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
*      dialog for setting animation controls
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>

#include "remove_header_conflicts.hpp"

#include "animation_controls.hpp"
#include <algo/structure/wx_tools/wx_tools.hpp>
#include "cn3d_tools.hpp"


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ header from animation_controls.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

#include <wx/image.h>
#include <wx/statline.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <wx/notebook.h>
#include <wx/grid.h>

// Declare window functions

#define ID_TEXT 10000
#define ID_TEXTCTRL 10001
#define ID_SPINBUTTON 10002
#define ID_B_DONE 10003
wxSizer *SetupAnimationDialog( wxWindow *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

////////////////////////////////////////////////////////////////////////////////////////////////

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

static IntegerSpinCtrl *giSpinDelay, *giFrameDelay;
static FloatingPointSpinCtrl *gfSpinIncrement;

#define DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERRORMSG("Can't find window with id " << id); \
        return; \
    }

#define SET_ISPINCTRL_FROM_REGISTRY_VALUE(section, name, iSpinCtrl) \
    do { \
        int value; \
        if (!RegistryGetInteger((section), (name), &value) || !((iSpinCtrl)->SetInteger(value))) \
            WARNINGMSG("PreferencesDialog::PreferencesDialog() - error with " << (name)); \
    } while (0)

#define SET_FSPINCTRL_FROM_REGISTRY_VALUE(section, name, fSpinCtrl) \
    do { \
        double value; \
        if (!RegistryGetDouble((section), (name), &value) || !((fSpinCtrl)->SetDouble(value))) \
            WARNINGMSG("PreferencesDialog::PreferencesDialog() - error with " << (name)); \
    } while (0)

BEGIN_EVENT_TABLE(AnimationControls, wxDialog)
    EVT_CLOSE       (       AnimationControls::OnCloseWindow)
    EVT_BUTTON      (-1,    AnimationControls::OnButton)
END_EVENT_TABLE()

AnimationControls::AnimationControls(wxWindow *parent) :
    wxDialog(parent, -1, "Animation Controls", wxPoint(400, 100), wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
    // construct the panel
    wxSizer *topSizer = SetupAnimationDialog(this, false);

    // get SpinCtrl pointers
    iSpinDelay = giSpinDelay;
    iFrameDelay = giFrameDelay;
    fSpinIncrement = gfSpinIncrement;

    // set initial values
    SET_ISPINCTRL_FROM_REGISTRY_VALUE(REG_ANIMATION_SECTION, REG_SPIN_DELAY, iSpinDelay);
    SET_ISPINCTRL_FROM_REGISTRY_VALUE(REG_ANIMATION_SECTION, REG_FRAME_DELAY, iFrameDelay);
    SET_FSPINCTRL_FROM_REGISTRY_VALUE(REG_ANIMATION_SECTION, REG_SPIN_INCREMENT, fSpinIncrement);

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->SetSizeHints(this);
}

AnimationControls::~AnimationControls(void)
{
    delete iSpinDelay;
    delete iFrameDelay;
    delete fSpinIncrement;
}

#define SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(section, name, iSpinCtrl) \
    do { \
        int oldValue, newValue; \
        if (!RegistryGetInteger((section), (name), &oldValue)) throw "RegistryGetInteger() failed"; \
        if (!((iSpinCtrl)->GetInteger(&newValue))) throw "GetInteger() failed"; \
        if (newValue != oldValue && !RegistrySetInteger((section), (name), newValue)) \
            throw "RegistrySetInteger() failed"; \
    } while (0)

#define SET_DOUBLE_REGISTRY_VALUE_IF_DIFFERENT(section, name, fSpinCtrl) \
    do { \
        double oldValue, newValue; \
        if (!RegistryGetDouble((section), (name), &oldValue)) throw "RegistryGetInteger() failed"; \
        if (!((fSpinCtrl)->GetDouble(&newValue))) throw "GetInteger() failed"; \
        if (newValue != oldValue && !RegistrySetDouble((section), (name), newValue)) \
            throw "RegistrySetInteger() failed"; \
    } while (0)

// same as hitting done button
void AnimationControls::OnCloseWindow(wxCloseEvent& event)
{
    bool okay = true;

    // set values if changed
    try {
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_ANIMATION_SECTION, REG_SPIN_DELAY, iSpinDelay);
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_ANIMATION_SECTION, REG_FRAME_DELAY, iFrameDelay);
        SET_DOUBLE_REGISTRY_VALUE_IF_DIFFERENT(REG_ANIMATION_SECTION, REG_SPIN_INCREMENT, fSpinIncrement);
    } catch (const char *err) {
        ERRORMSG("Error setting registry values - " << err);
        okay = false;
    }

    // close dialog only if all user values are legit
    if (okay) {
        EndModal(wxOK);
    } else {
        if (event.CanVeto())
            event.Veto();
    }
}

void AnimationControls::OnButton(wxCommandEvent& event)
{
    switch (event.GetId()) {

        case ID_B_DONE: {
            wxCloseEvent fake;
            OnCloseWindow(fake); // handle on-exit stuff there
            break;
        }

        default:
            event.Skip();
    }
}

END_SCOPE(Cn3D)

USING_SCOPE(Cn3D);


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is modified from wxDesigner's C++ code from animation_controls.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *SetupAnimationDialog( wxWindow *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, wxT("Spin") );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxFlexGridSizer *item3 = new wxFlexGridSizer( 2, 0, 0, 0 );
    item3->AddGrowableCol( 0 );

    wxStaticText *item4 = new wxStaticText( parent, ID_TEXT, wxT("Increment (deg):"), wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item4, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    gfSpinIncrement = new FloatingPointSpinCtrl(parent,
        0.01, 30, 0.1, 2,
        wxDefaultPosition, wxSize(50,SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1,SPIN_CTRL_HEIGHT));
    item3->Add(gfSpinIncrement->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(gfSpinIncrement->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item7 = new wxStaticText( parent, ID_TEXT, wxT("Delay (ms):"), wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item7, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    giSpinDelay = new IntegerSpinCtrl(parent,
        1, 1000, 10, 50,
        wxDefaultPosition, wxSize(50,SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1,SPIN_CTRL_HEIGHT));
    item3->Add(giSpinDelay->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(giSpinDelay->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    item1->Add( item3, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    item0->Add( item1, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxStaticBox *item11 = new wxStaticBox( parent, -1, wxT("Frames") );
    wxStaticBoxSizer *item10 = new wxStaticBoxSizer( item11, wxVERTICAL );

    wxFlexGridSizer *item12 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item12->AddGrowableCol( 0 );

    wxStaticText *item13 = new wxStaticText( parent, ID_TEXT, wxT("Delay (ms):"), wxDefaultPosition, wxDefaultSize, 0 );
    item12->Add( item13, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    giFrameDelay = new IntegerSpinCtrl(parent,
        1, 5000, 100, 500,
        wxDefaultPosition, wxSize(50,SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1,SPIN_CTRL_HEIGHT));
    item12->Add(giFrameDelay->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item12->Add(giFrameDelay->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    item10->Add( item12, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    item0->Add( item10, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item16 = new wxButton( parent, ID_B_DONE, wxT("Done"), wxDefaultPosition, wxDefaultSize, 0 );
    item0->Add( item16, 0, wxALIGN_CENTRE|wxALL, 5 );

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
