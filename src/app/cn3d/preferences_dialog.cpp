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
*      dialogs for editing program preferences
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2001/09/24 14:37:52  thiessen
* more wxPanel stuff - fix for new heirarchy in wx 2.3.2+
*
* Revision 1.4  2001/09/24 13:29:54  thiessen
* fix wxPanel issues
*
* Revision 1.3  2001/09/20 19:31:30  thiessen
* fixes for SGI and wxWin 2.3.2
*
* Revision 1.2  2001/08/13 22:30:59  thiessen
* add structure window mouse drag/zoom; add highlight option to render settings
*
* Revision 1.1  2001/08/06 20:22:01  thiessen
* add preferences dialog ; make sure OnCloseWindow get wxCloseEvent
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>

#include "cn3d/preferences_dialog.hpp"
#include "cn3d/wx_tools.hpp"
#include "cn3d/opengl_renderer.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/messenger.hpp"

#if defined(__WXMSW__)
#include <wx/msw/winundef.h>
#endif


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ header from preferences_dialog.wdr
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
#define ID_B_DONE 10001
#define ID_B_CANCEL 10002
wxSizer *SetupPreferencesNotebook( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

#define ID_TEXT 10003
#define ID_TEXTCTRL 10004
#define ID_SPINBUTTON 10005
#define ID_C_HIGHLIGHT 10006
#define ID_B_Q_LOW 10007
#define ID_B_Q_MED 10008
#define ID_B_Q_HIGH 10009
wxSizer *SetupQualityPage( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

////////////////////////////////////////////////////////////////////////////////////////////////

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

static IntegerSpinCtrl
    *giWormSegments, *giWormSides, *giBondSides, *giHelixSides, *giAtomSlices, *giAtomStacks;

#define DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERR_POST(Error << "Can't find window with id " << id); \
        return; \
    }

BEGIN_EVENT_TABLE(PreferencesDialog, wxDialog)
    EVT_CLOSE       (       PreferencesDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    PreferencesDialog::OnButton)
END_EVENT_TABLE()

#define SET_SPINCRTL_FROM_REGISTRY_VALUE(name, iSpinCtrl) \
    do { \
        int value; \
        if (!RegistryGetInteger(REG_QUALITY_SECTION, (name), &value) || !((iSpinCtrl)->SetInteger(value))) \
            ERR_POST(Warning << "PreferencesDialog::PreferencesDialog() - error with " << (name)); \
    } while (0)

#define SET_CHECKBOX_FROM_REGISTRY_VALUE(name, id) \
    do { \
        bool on; \
        wxCheckBox *box = wxDynamicCast(FindWindow(id), wxCheckBox); \
        if (!box || !RegistryGetBoolean(REG_QUALITY_SECTION, (name), &on)) \
            ERR_POST(Warning << "PreferencesDialog::PreferencesDialog() - error with " << (name)); \
        else \
            box->SetValue(on); \
    } while (0)

PreferencesDialog::PreferencesDialog(wxWindow *parent) :
    wxDialog(parent, -1, "Cn3D++ Preferences", wxPoint(400, 100), wxDefaultSize,
        wxCAPTION | wxSYSTEM_MENU) // not resizable
{
    // construct the panel
    wxPanel *panel = new wxPanel(this, -1);
    wxSizer *topSizer = SetupPreferencesNotebook(panel, false);

    // get IntegerSpinCtrl pointers
    iWormSegments = giWormSegments;
    iWormSides = giWormSides;
    iBondSides = giBondSides;
    iHelixSides = giHelixSides;
    iAtomSlices = giAtomSlices;
    iAtomStacks = giAtomStacks;

    // set initial values
    SET_SPINCRTL_FROM_REGISTRY_VALUE(REG_QUALITY_ATOM_SLICES, iAtomSlices);
    SET_SPINCRTL_FROM_REGISTRY_VALUE(REG_QUALITY_ATOM_STACKS, iAtomStacks);
    SET_SPINCRTL_FROM_REGISTRY_VALUE(REG_QUALITY_BOND_SIDES, iBondSides);
    SET_SPINCRTL_FROM_REGISTRY_VALUE(REG_QUALITY_WORM_SIDES, iWormSides);
    SET_SPINCRTL_FROM_REGISTRY_VALUE(REG_QUALITY_WORM_SEGMENTS, iWormSegments);
    SET_SPINCRTL_FROM_REGISTRY_VALUE(REG_QUALITY_HELIX_SIDES, iHelixSides);
    SET_CHECKBOX_FROM_REGISTRY_VALUE(REG_HIGHLIGHTS_ON, ID_C_HIGHLIGHT);

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->Fit(panel);
    topSizer->SetSizeHints(this);
}

#define SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(name, iSpinCtrl) \
    do { \
        int oldValue, newValue; \
        if (!RegistryGetInteger(REG_QUALITY_SECTION, (name), &oldValue)) throw "RegistryGetInteger() failed"; \
        if (!((iSpinCtrl)->GetInteger(&newValue))) throw "GetInteger() failed"; \
        if (newValue != oldValue) { \
            if (!RegistrySetInteger(REG_QUALITY_SECTION, (name), newValue)) \
                throw "RegistrySetInteger() failed"; \
            qualityChanged = true; \
        } \
    } while (0)

#define SET_BOOL_REGISTRY_VALUE_IF_DIFFERENT(name, id) \
    do { \
        bool oldValue, newValue; \
        if (!RegistryGetBoolean(REG_QUALITY_SECTION, (name), &oldValue)) throw "RegistryGetBoolean() failed"; \
        wxCheckBox *box = wxDynamicCast(FindWindow(id), wxCheckBox); \
        if (!box) throw "Can't get wxCheckBox*"; \
        newValue = box->GetValue(); \
        if (newValue != oldValue) { \
            if (!RegistrySetBoolean(REG_QUALITY_SECTION, (name), newValue, true)) \
                throw "RegistrySetBoolean() failed"; \
            qualityChanged = true; \
        } \
    } while (0)

// same as hitting done button
void PreferencesDialog::OnCloseWindow(wxCloseEvent& event)
{
    bool okay = true, qualityChanged = false;

    // set values if changed
    try {
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_QUALITY_ATOM_SLICES, iAtomSlices);
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_QUALITY_ATOM_STACKS, iAtomStacks);
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_QUALITY_BOND_SIDES, iBondSides);
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_QUALITY_WORM_SIDES, iWormSides);
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_QUALITY_WORM_SEGMENTS, iWormSegments);
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_QUALITY_HELIX_SIDES, iHelixSides);
        SET_BOOL_REGISTRY_VALUE_IF_DIFFERENT(REG_HIGHLIGHTS_ON, ID_C_HIGHLIGHT);
    } catch (const char *err) {
        ERR_POST(Error << "Error setting registry values - " << err);
        okay = false;
    }

    // close dialog only if all user values are legit
    if (okay) {
        if (qualityChanged) GlobalMessenger()->PostRedrawAllStructures();
        EndModal(wxOK);
    } else {
        if (event.CanVeto())
            event.Veto();
    }
}

void PreferencesDialog::OnButton(wxCommandEvent& event)
{
    switch (event.GetId()) {

        case ID_B_DONE: {
            wxCloseEvent fake;
            OnCloseWindow(fake); // handle on-exit stuff there
            break;
        }
        case ID_B_CANCEL:
            EndModal(wxCANCEL);
            break;

        // quality page stuff
        case ID_B_Q_LOW:
            iWormSegments->SetInteger(2);
            iWormSides->SetInteger(4);
            iBondSides->SetInteger(4);
            iHelixSides->SetInteger(8);
            iAtomSlices->SetInteger(5);
            iAtomStacks->SetInteger(3);
            break;
        case ID_B_Q_MED:
            iWormSegments->SetInteger(6);
            iWormSides->SetInteger(6);
            iBondSides->SetInteger(6);
            iHelixSides->SetInteger(12);
            iAtomSlices->SetInteger(10);
            iAtomStacks->SetInteger(8);
            break;
        case ID_B_Q_HIGH:
            iWormSegments->SetInteger(10);
            iWormSides->SetInteger(20);
            iBondSides->SetInteger(16);
            iHelixSides->SetInteger(30);
            iAtomSlices->SetInteger(20);
            iAtomStacks->SetInteger(14);
            break;

        default:
            event.Skip();
    }
}

END_SCOPE(Cn3D)

USING_SCOPE(Cn3D);


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken *unmodified* from wxDesigner's C++ code from preferences_dialog.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *SetupPreferencesNotebook( wxPanel *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxNotebook *item2 = new wxNotebook( parent, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize, 0 );
    wxNotebookSizer *item1 = new wxNotebookSizer( item2 );

    wxPanel *item3 = new wxPanel( item2, -1 );
    SetupQualityPage( item3, FALSE );
    item2->AddPage( item3, "Quality" );

    item0->Add( item1, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxBoxSizer *item4 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item5 = new wxButton( parent, ID_B_DONE, "Done", wxDefaultPosition, wxDefaultSize, 0 );
    item5->SetDefault();
    item4->Add( item5, 0, wxALIGN_CENTRE|wxALL, 5 );

    item4->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item6 = new wxButton( parent, ID_B_CANCEL, "Cancel", wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item6, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item4, 0, wxALIGN_CENTRE|wxALL, 5 );

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


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is modified from wxDesigner's C++ code from preferences_dialog.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *SetupQualityPage(wxPanel *parent, bool call_fit, bool set_sizer)
{
    wxBoxSizer *item0 = new wxBoxSizer( wxHORIZONTAL );
    wxStaticBox *item2 = new wxStaticBox( parent, -1, "Rendering Settings" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxHORIZONTAL );
    wxFlexGridSizer *item3 = new wxFlexGridSizer( 3, 0, 0 );
    item3->AddGrowableCol( 0 );

    wxStaticText *item5 = new wxStaticText(parent, ID_TEXT, "Worm segments:", wxDefaultPosition, wxDefaultSize, 0);
    item3->Add(item5, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    giWormSegments = new IntegerSpinCtrl(parent,
        2, 30, 2, 4,
        wxDefaultPosition, wxSize(80,SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1,SPIN_CTRL_HEIGHT));
    item3->Add(giWormSegments->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(giWormSegments->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item8 = new wxStaticText(parent, ID_TEXT, "Worm sides:", wxDefaultPosition, wxDefaultSize, 0);
    item3->Add(item8, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    giWormSides = new IntegerSpinCtrl(parent,
        4, 30, 2, 6,
        wxDefaultPosition, wxSize(80,SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1,SPIN_CTRL_HEIGHT));
    item3->Add(giWormSides->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(giWormSides->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item11 = new wxStaticText(parent, ID_TEXT, "Bond sides:", wxDefaultPosition, wxDefaultSize, 0);
    item3->Add(item11, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    giBondSides = new IntegerSpinCtrl(parent,
        3, 30, 1, 6,
        wxDefaultPosition, wxSize(80,SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1,SPIN_CTRL_HEIGHT));
    item3->Add(giBondSides->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(giBondSides->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item14 = new wxStaticText(parent, ID_TEXT, "Helix sides:", wxDefaultPosition, wxDefaultSize, 0);
    item3->Add(item14, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    giHelixSides = new IntegerSpinCtrl(parent,
        3, 40, 1, 12,
        wxDefaultPosition, wxSize(80,SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1,SPIN_CTRL_HEIGHT));
    item3->Add(giHelixSides->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(giHelixSides->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item17 = new wxStaticText(parent, ID_TEXT, "Atom slices:", wxDefaultPosition, wxDefaultSize, 0);
    item3->Add(item17, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    giAtomSlices = new IntegerSpinCtrl(parent,
        3, 30, 1, 8,
        wxDefaultPosition, wxSize(80,SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1,SPIN_CTRL_HEIGHT));
    item3->Add(giAtomSlices->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(giAtomSlices->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item20 = new wxStaticText(parent, ID_TEXT, "Atom stacks:", wxDefaultPosition, wxDefaultSize, 0);
    item3->Add(item20, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
    giAtomStacks = new IntegerSpinCtrl(parent,
        2, 30, 1, 6,
        wxDefaultPosition, wxSize(80,SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1,SPIN_CTRL_HEIGHT));
    item3->Add(giAtomStacks->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(giAtomStacks->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item222 = new wxStaticText( parent, ID_TEXT, "Highlights:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item222, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    wxCheckBox *item233 = new wxCheckBox( parent, ID_C_HIGHLIGHT, "", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item233, 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5 );

    item3->Add( 5, 5, 0, wxALIGN_CENTRE, 5 );
    item1->Add(item3, 0, wxALIGN_CENTRE|wxALL, 5);
    item0->Add(item1, 0, wxGROW|wxALIGN_CENTER_HORIZONTAL|wxALL, 5);


    wxStaticBox *item23 = new wxStaticBox( parent, -1, "Presets" );
    wxStaticBoxSizer *item22 = new wxStaticBoxSizer( item23, wxHORIZONTAL );

    wxBoxSizer *item24 = new wxBoxSizer( wxVERTICAL );

    wxButton *item25 = new wxButton( parent, ID_B_Q_LOW, "Low", wxDefaultPosition, wxDefaultSize, 0 );
    item24->Add( item25, 0, wxALIGN_CENTRE|wxALL, 5 );

    item24->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item26 = new wxButton( parent, ID_B_Q_MED, "Medium", wxDefaultPosition, wxDefaultSize, 0 );
    item24->Add( item26, 0, wxALIGN_CENTRE|wxALL, 5 );

    item24->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item27 = new wxButton( parent, ID_B_Q_HIGH, "High", wxDefaultPosition, wxDefaultSize, 0 );
    item24->Add( item27, 0, wxALIGN_CENTRE|wxALL, 5 );

    item22->Add( item24, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item22, 0, wxGROW|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    if (set_sizer)
    {
        parent->SetAutoLayout(TRUE);
        parent->SetSizer(item0);
        if (call_fit)
        {
            item0->Fit(parent);
            item0->SetSizeHints(parent);
        }
    }

    return item0;
}

