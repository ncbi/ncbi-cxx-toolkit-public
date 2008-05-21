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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>

#include "remove_header_conflicts.hpp"

#include "preferences_dialog.hpp"
#include <algo/structure/wx_tools/wx_tools.hpp>
#include "opengl_renderer.hpp"
#include "cn3d_tools.hpp"
#include "messenger.hpp"
#include "cn3d_cache.hpp"


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
#include <wx/grid.h>

// Declare window functions

#define ID_NOTEBOOK 10000
#define ID_B_DONE 10001
#define ID_B_CANCEL 10002
wxSizer *SetupPreferencesNotebook( wxWindow *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

#define ID_TEXT 10003
#define ID_TEXTCTRL 10004
#define ID_SPINBUTTON 10005
#define ID_C_HIGHLIGHT 10006
#define ID_RADIOBOX 10007
#define ID_B_Q_LOW 10008
#define ID_B_Q_MED 10009
#define ID_B_Q_HIGH 10010
wxSizer *SetupQualityPage( wxWindow *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

#define ID_C_CACHE_ON 10011
#define ID_LINE 10012
#define ID_T_CACHE_1 10013
#define ID_B_CACHE_BROWSE 10014
#define ID_T_CACHE_FOLDER 10015
#define ID_T_CACHE_2 10016
#define ID_B_CACHE_CLEAR 10017
wxSizer *SetupCachePage( wxWindow *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

#define ID_C_ANNOT_RO 10018
#define ID_T_LAUNCH 10019
#define ID_T_NSTRUCT 10020
#define ID_T_FOOT 10021
#define ID_T_SEPARATION 10022
#define ID_C_PROXIMAL 10023
wxSizer *SetupAdvancedPage( wxWindow *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

////////////////////////////////////////////////////////////////////////////////////////////////

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

static IntegerSpinCtrl
    *giWormSegments, *giWormSides, *giBondSides, *giHelixSides, *giAtomSlices, *giAtomStacks,
    *giCacheSize, *giMaxStructs, *giFootRes;
static FloatingPointSpinCtrl *gfSeparation;

#define DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERRORMSG("Can't find window with id " << id); \
        return; \
    }

BEGIN_EVENT_TABLE(PreferencesDialog, wxDialog)
    EVT_CLOSE       (       PreferencesDialog::OnCloseWindow)
    EVT_BUTTON      (-1,    PreferencesDialog::OnButton)
    EVT_CHECKBOX    (-1,    PreferencesDialog::OnCheckbox)
END_EVENT_TABLE()

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

#define SET_CHECKBOX_FROM_REGISTRY_VALUE(section, name, id) \
    do { \
        bool on; \
        wxCheckBox *box = wxDynamicCast(FindWindow(id), wxCheckBox); \
        if (!box || !RegistryGetBoolean((section), (name), &on)) \
            WARNINGMSG("PreferencesDialog::PreferencesDialog() - error with " << (name)); \
        else \
            box->SetValue(on); \
    } while (0)

#define SET_RADIOBOX_FROM_REGISTRY_VALUE(section, name, id) \
    do { \
        string value; \
        wxRadioBox *radio = wxDynamicCast(FindWindow(id), wxRadioBox); \
        if (!radio || !RegistryGetString((section), (name), &value)) \
            WARNINGMSG("PreferencesDialog::PreferencesDialog() - error with " << (name)); \
        else \
            radio->SetStringSelection(value.c_str()); \
    } while (0)

#define SET_TEXTCTRL_FROM_REGISTRY_VALUE(section, name, id) \
    do { \
        string value; \
        wxTextCtrl *text = wxDynamicCast(FindWindow(id), wxTextCtrl); \
        if (!text || !RegistryGetString((section), (name), &value)) \
            WARNINGMSG("PreferencesDialog::PreferencesDialog() - error with " << (name)); \
        else \
            text->SetValue(value.c_str()); \
    } while (0)


PreferencesDialog::PreferencesDialog(wxWindow *parent) :
    wxDialog(parent, -1, "Preferences", wxPoint(400, 100), wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
    // construct the panel
    wxSizer *topSizer = SetupPreferencesNotebook(this, false);

    // get SpinCtrl pointers
    iWormSegments = giWormSegments;
    iWormSides = giWormSides;
    iBondSides = giBondSides;
    iHelixSides = giHelixSides;
    iAtomSlices = giAtomSlices;
    iAtomStacks = giAtomStacks;
    iCacheSize = giCacheSize;
    iMaxStructs = giMaxStructs;
    iFootRes = giFootRes;
    fSeparation = gfSeparation;

    // set initial values
    SET_ISPINCTRL_FROM_REGISTRY_VALUE(REG_QUALITY_SECTION, REG_QUALITY_ATOM_SLICES, iAtomSlices);
    SET_ISPINCTRL_FROM_REGISTRY_VALUE(REG_QUALITY_SECTION, REG_QUALITY_ATOM_STACKS, iAtomStacks);
    SET_ISPINCTRL_FROM_REGISTRY_VALUE(REG_QUALITY_SECTION, REG_QUALITY_BOND_SIDES, iBondSides);
    SET_ISPINCTRL_FROM_REGISTRY_VALUE(REG_QUALITY_SECTION, REG_QUALITY_WORM_SIDES, iWormSides);
    SET_ISPINCTRL_FROM_REGISTRY_VALUE(REG_QUALITY_SECTION, REG_QUALITY_WORM_SEGMENTS, iWormSegments);
    SET_ISPINCTRL_FROM_REGISTRY_VALUE(REG_QUALITY_SECTION, REG_QUALITY_HELIX_SIDES, iHelixSides);
    SET_CHECKBOX_FROM_REGISTRY_VALUE(REG_QUALITY_SECTION, REG_HIGHLIGHTS_ON, ID_C_HIGHLIGHT);
    SET_RADIOBOX_FROM_REGISTRY_VALUE(REG_QUALITY_SECTION, REG_PROJECTION_TYPE, ID_RADIOBOX);

    SET_CHECKBOX_FROM_REGISTRY_VALUE(REG_CACHE_SECTION, REG_CACHE_ENABLED, ID_C_CACHE_ON);
    SET_TEXTCTRL_FROM_REGISTRY_VALUE(REG_CACHE_SECTION, REG_CACHE_FOLDER, ID_T_CACHE_FOLDER);

    SET_ISPINCTRL_FROM_REGISTRY_VALUE(REG_CACHE_SECTION, REG_CACHE_MAX_SIZE, iCacheSize);
    wxCommandEvent fakeCheck(wxEVT_COMMAND_CHECKBOX_CLICKED, ID_C_CACHE_ON);
    OnCheckbox(fakeCheck);  // set initial GUI enabled state

    SET_CHECKBOX_FROM_REGISTRY_VALUE(REG_ADVANCED_SECTION, REG_CDD_ANNOT_READONLY, ID_C_ANNOT_RO);
#ifdef __WXGTK__
    SET_TEXTCTRL_FROM_REGISTRY_VALUE(REG_ADVANCED_SECTION, REG_BROWSER_LAUNCH, ID_T_LAUNCH);
#endif
    SET_ISPINCTRL_FROM_REGISTRY_VALUE(REG_ADVANCED_SECTION, REG_MAX_N_STRUCTS, iMaxStructs);
    SET_ISPINCTRL_FROM_REGISTRY_VALUE(REG_ADVANCED_SECTION, REG_FOOTPRINT_RES, iFootRes);

    SET_FSPINCTRL_FROM_REGISTRY_VALUE(REG_ADVANCED_SECTION, REG_STEREO_SEPARATION, fSeparation);
    SET_CHECKBOX_FROM_REGISTRY_VALUE(REG_ADVANCED_SECTION, REG_PROXIMAL_STEREO, ID_C_PROXIMAL);

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->SetSizeHints(this);
}

PreferencesDialog::~PreferencesDialog(void)
{
    delete iWormSegments;
    delete iWormSides;
    delete iBondSides;
    delete iHelixSides;
    delete iAtomSlices;
    delete iAtomStacks;
    delete iCacheSize;
    delete iMaxStructs;
    delete iFootRes;
    delete fSeparation;
}

#define SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(section, name, iSpinCtrl, changedPtr) \
    do { \
        int oldValue, newValue; \
        if (!RegistryGetInteger((section), (name), &oldValue)) throw "RegistryGetInteger() failed"; \
        if (!((iSpinCtrl)->GetInteger(&newValue))) throw "GetInteger() failed"; \
        if (newValue != oldValue) { \
            if (!RegistrySetInteger((section), (name), newValue)) \
                throw "RegistrySetInteger() failed"; \
            if (changedPtr) *((bool*) changedPtr) = true; \
        } \
    } while (0)

#define SET_DOUBLE_REGISTRY_VALUE_IF_DIFFERENT(section, name, fSpinCtrl, changedPtr) \
    do { \
        double oldValue, newValue; \
        if (!RegistryGetDouble((section), (name), &oldValue)) throw "RegistryGetInteger() failed"; \
        if (!((fSpinCtrl)->GetDouble(&newValue))) throw "GetInteger() failed"; \
        if (newValue != oldValue) { \
            if (!RegistrySetDouble((section), (name), newValue)) \
                throw "RegistrySetInteger() failed"; \
            if (changedPtr) *((bool*) changedPtr) = true; \
        } \
    } while (0)

#define SET_BOOL_REGISTRY_VALUE_IF_DIFFERENT(section, name, id, changedPtr) \
    do { \
        bool oldValue, newValue; \
        if (!RegistryGetBoolean((section), (name), &oldValue)) throw "RegistryGetBoolean() failed"; \
        wxCheckBox *box = wxDynamicCast(FindWindow(id), wxCheckBox); \
        if (!box) throw "Can't get wxCheckBox*"; \
        newValue = box->GetValue(); \
        if (newValue != oldValue) { \
            if (!RegistrySetBoolean((section), (name), newValue, true)) \
                throw "RegistrySetBoolean() failed"; \
            if (changedPtr) *((bool*) changedPtr) = true; \
        } \
    } while (0)

#define SET_RADIO_REGISTRY_VALUE_IF_DIFFERENT(section, name, id, changedPtr) \
    do { \
        string oldValue, newValue; \
        if (!RegistryGetString((section), (name), &oldValue)) throw "RegistryGetString() failed"; \
        wxRadioBox *radio = wxDynamicCast(FindWindow(id), wxRadioBox); \
        if (!radio) throw "Can't get wxRadioBox*"; \
        newValue = radio->GetStringSelection().c_str(); \
        if (newValue != oldValue) { \
            if (!RegistrySetString((section), (name), newValue)) \
                throw "RegistrySetString() failed"; \
            if (changedPtr) *((bool*) changedPtr) = true; \
        } \
    } while (0)

#define SET_STRING_REGISTRY_VALUE_IF_DIFFERENT(section, name, textCtrl) \
    do { \
        string oldValue, newValue; \
        if (!RegistryGetString((section), (name), &oldValue)) throw "RegistryGetString() failed"; \
        newValue = (textCtrl)->GetValue().c_str(); \
        if (newValue != oldValue) { \
            if (!RegistrySetString((section), (name), newValue)) \
                throw "RegistrySetString() failed"; \
        } \
    } while (0)

// same as hitting done button
void PreferencesDialog::OnCloseWindow(wxCloseEvent& event)
{
    bool okay = true, qualityChanged = false;

    // set values if changed
    try {
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_QUALITY_SECTION,
            REG_QUALITY_ATOM_SLICES, iAtomSlices, &qualityChanged);
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_QUALITY_SECTION,
            REG_QUALITY_ATOM_STACKS, iAtomStacks, &qualityChanged);
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_QUALITY_SECTION,
            REG_QUALITY_BOND_SIDES, iBondSides, &qualityChanged);
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_QUALITY_SECTION,
            REG_QUALITY_WORM_SIDES, iWormSides, &qualityChanged);
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_QUALITY_SECTION,
            REG_QUALITY_WORM_SEGMENTS, iWormSegments, &qualityChanged);
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_QUALITY_SECTION,
            REG_QUALITY_HELIX_SIDES, iHelixSides, &qualityChanged);
        SET_BOOL_REGISTRY_VALUE_IF_DIFFERENT(REG_QUALITY_SECTION,
            REG_HIGHLIGHTS_ON, ID_C_HIGHLIGHT, &qualityChanged);
        SET_RADIO_REGISTRY_VALUE_IF_DIFFERENT(REG_QUALITY_SECTION,
            REG_PROJECTION_TYPE, ID_RADIOBOX, &qualityChanged);

        SET_BOOL_REGISTRY_VALUE_IF_DIFFERENT(REG_CACHE_SECTION, REG_CACHE_ENABLED, ID_C_CACHE_ON, 0);
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tCache, ID_T_CACHE_FOLDER, wxTextCtrl)
        SET_STRING_REGISTRY_VALUE_IF_DIFFERENT(REG_CACHE_SECTION, REG_CACHE_FOLDER, tCache);
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_CACHE_SECTION, REG_CACHE_MAX_SIZE, iCacheSize, 0);

        SET_BOOL_REGISTRY_VALUE_IF_DIFFERENT(REG_ADVANCED_SECTION, REG_CDD_ANNOT_READONLY, ID_C_ANNOT_RO, 0);
#ifdef __WXGTK__
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tLaunch, ID_T_LAUNCH, wxTextCtrl)
        SET_STRING_REGISTRY_VALUE_IF_DIFFERENT(REG_ADVANCED_SECTION, REG_BROWSER_LAUNCH, tLaunch);
#endif
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_ADVANCED_SECTION, REG_MAX_N_STRUCTS, iMaxStructs, 0);
        SET_INTEGER_REGISTRY_VALUE_IF_DIFFERENT(REG_ADVANCED_SECTION, REG_FOOTPRINT_RES, iFootRes, 0);

        SET_DOUBLE_REGISTRY_VALUE_IF_DIFFERENT(REG_ADVANCED_SECTION, REG_STEREO_SEPARATION, fSeparation, 0);
        SET_BOOL_REGISTRY_VALUE_IF_DIFFERENT(REG_ADVANCED_SECTION, REG_PROXIMAL_STEREO, ID_C_PROXIMAL, 0);

        // Limit cache size to current value now
        int size;
        if (iCacheSize->GetInteger(&size)) TruncateCache(size);

    } catch (const char *err) {
        ERRORMSG("Error setting registry values - " << err);
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

        // cache page stuff
        case ID_B_CACHE_BROWSE: {
            DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tCache, ID_T_CACHE_FOLDER, wxTextCtrl)
            wxString path;
            path = wxDirSelector("Select a cache folder:", tCache->GetValue());
            if (path.size() > 0 && wxDirExists(path.c_str()))
                tCache->SetValue(path);
            break;
        }
        case ID_B_CACHE_CLEAR:
            TruncateCache(0);
            break;

        default:
            event.Skip();
    }
}

void PreferencesDialog::OnCheckbox(wxCommandEvent& event)
{
    if (event.GetId() == ID_C_CACHE_ON) {
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(c, ID_C_CACHE_ON, wxCheckBox)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(st1, ID_T_CACHE_1, wxStaticText)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(b1, ID_B_CACHE_BROWSE, wxButton)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(t, ID_T_CACHE_FOLDER, wxTextCtrl)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(st2, ID_T_CACHE_2, wxStaticText)
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(b2, ID_B_CACHE_CLEAR, wxButton)
        st1->Enable(c->GetValue());
        b1->Enable(c->GetValue());
        t->Enable(c->GetValue());
        st2->Enable(c->GetValue());
        b2->Enable(c->GetValue());
        iCacheSize->GetTextCtrl()->Enable(c->GetValue());
        iCacheSize->GetSpinButton()->Enable(c->GetValue());
    }
}

END_SCOPE(Cn3D)

USING_SCOPE(Cn3D);


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken *unmodified* from wxDesigner's C++ code from preferences_dialog.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *SetupPreferencesNotebook( wxWindow *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxNotebook *item2 = new wxNotebook( parent, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize, 0 );

    wxPanel *item3 = new wxPanel( item2, -1 );
    SetupQualityPage( item3, FALSE );
    item2->AddPage( item3, "Quality" );

    wxPanel *item4 = new wxPanel( item2, -1 );
    SetupCachePage( item4, FALSE );
    item2->AddPage( item4, "Cache" );

    wxPanel *item5 = new wxPanel( item2, -1 );
    SetupAdvancedPage( item5, FALSE );
    item2->AddPage( item5, "Advanced" );

    item0->Add( item2, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxBoxSizer *item6 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item7 = new wxButton( parent, ID_B_DONE, "Done", wxDefaultPosition, wxDefaultSize, 0 );
    item7->SetDefault();
    item6->Add( item7, 0, wxALIGN_CENTRE|wxALL, 5 );

    item6->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item8 = new wxButton( parent, ID_B_CANCEL, "Cancel", wxDefaultPosition, wxDefaultSize, 0 );
    item6->Add( item8, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item6, 0, wxALIGN_CENTRE|wxALL, 5 );

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

wxSizer *SetupAdvancedPage( wxWindow *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, "Advanced" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxCheckBox *item3 = new wxCheckBox( parent, ID_C_ANNOT_RO, "CDD annotations are read-only", wxDefaultPosition, wxDefaultSize, 0 );
    item3->SetValue( TRUE );
    item1->Add( item3, 0, wxALIGN_CENTRE|wxALL, 5 );

#ifdef __WXGTK__
    wxFlexGridSizer *item4 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item4->AddGrowableCol( 0 );

    wxTextCtrl *item5 = new wxTextCtrl( parent, ID_T_LAUNCH, "", wxDefaultPosition, wxSize(80,-1), 0 );
    item4->Add( item5, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxStaticText *item6 = new wxStaticText( parent, ID_TEXT, "Browser launch", wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item6, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item4, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );
#endif

    wxFlexGridSizer *item7 = new wxFlexGridSizer( 2, 0, 0, 0 );
    item7->AddGrowableCol( 0 );

    wxStaticText *item8 = new wxStaticText( parent, ID_TEXT, "Max structures to load:", wxDefaultPosition, wxDefaultSize, 0 );
    item7->Add( item8, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    giMaxStructs = new IntegerSpinCtrl(parent,
        0, 100, 1, 10,
        wxDefaultPosition, wxSize(80,SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1,SPIN_CTRL_HEIGHT));
    item7->Add(giMaxStructs->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item7->Add(giMaxStructs->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item11 = new wxStaticText( parent, ID_TEXT, "Footprint excess residues:", wxDefaultPosition, wxDefaultSize, 0 );
    item7->Add( item11, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    giFootRes = new IntegerSpinCtrl(parent,
        0, 1000, 5, 15,
        wxDefaultPosition, wxSize(80,SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1,SPIN_CTRL_HEIGHT));
    item7->Add(giFootRes->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item7->Add(giFootRes->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    item1->Add( item7, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item0->Add( item1, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxStaticBox *item15 = new wxStaticBox( parent, -1, wxT("Stereo Settings") );
    wxStaticBoxSizer *item14 = new wxStaticBoxSizer( item15, wxVERTICAL );

    wxFlexGridSizer *item16 = new wxFlexGridSizer( 3, 0, 0 );
    item16->AddGrowableCol( 0 );

    wxStaticText *item17 = new wxStaticText( parent, ID_TEXT, wxT("Eye separation (degrees):"), wxDefaultPosition, wxDefaultSize, 0 );
    item16->Add( item17, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    gfSeparation = new FloatingPointSpinCtrl(parent,
        0.0, 15.0, 0.1, 5.0,
        wxDefaultPosition, wxSize(80,SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1,SPIN_CTRL_HEIGHT));
    item16->Add(gfSeparation->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item16->Add(gfSeparation->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item20 = new wxStaticText( parent, ID_TEXT, wxT("Proximal (cross-eyed):"), wxDefaultPosition, wxDefaultSize, 0 );
    item16->Add( item20, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxCheckBox *item21 = new wxCheckBox( parent, ID_C_PROXIMAL, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    item16->Add( item21, 0, wxALIGN_CENTRE|wxALL, 5 );

    item16->Add( 5, 5, 0, wxALIGN_CENTRE|wxALL, 5 );

    item14->Add( item16, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item0->Add( item14, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

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

wxSizer *SetupQualityPage(wxWindow *parent, bool call_fit, bool set_sizer)
{
    wxFlexGridSizer *item0 = new wxFlexGridSizer( 2, 0, 0 );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, "Rendering Settings" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );
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

    item1->Add(item3, 0, wxALIGN_CENTRE|wxALL, 5);

    wxFlexGridSizer *item22 = new wxFlexGridSizer( 2, 0, 0, 0 );
    item22->AddGrowableCol( 1 );

    wxStaticText *item23 = new wxStaticText( parent, ID_TEXT, "Highlights:", wxDefaultPosition, wxDefaultSize, 0 );
    item22->Add( item23, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxCheckBox *item24 = new wxCheckBox( parent, ID_C_HIGHLIGHT, "", wxDefaultPosition, wxDefaultSize, 0 );
    item22->Add( item24, 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5 );

    wxStaticText *item25 = new wxStaticText( parent, ID_TEXT, "Projection:", wxDefaultPosition, wxDefaultSize, 0 );
    item22->Add( item25, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs26[] =
    {
        "Perspective",
        "Orthographic"
    };
    wxRadioBox *item26 = new wxRadioBox( parent, ID_RADIOBOX, "", wxDefaultPosition, wxDefaultSize, 2, strs26, 1, wxRA_SPECIFY_COLS );
    item22->Add( item26, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxLEFT, 5 );

    item1->Add( item22, 0, wxALIGN_CENTRE, 5 );

    item0->Add( item1, 0, wxGROW|wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    wxStaticBox *item28 = new wxStaticBox( parent, -1, "Presets" );
    wxStaticBoxSizer *item27 = new wxStaticBoxSizer( item28, wxHORIZONTAL );

    wxBoxSizer *item29 = new wxBoxSizer( wxVERTICAL );

    wxButton *item30 = new wxButton( parent, ID_B_Q_LOW, "Low", wxDefaultPosition, wxDefaultSize, 0 );
    item29->Add( item30, 0, wxALIGN_CENTRE|wxALL, 5 );

    item29->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item31 = new wxButton( parent, ID_B_Q_MED, "Medium", wxDefaultPosition, wxDefaultSize, 0 );
    item29->Add( item31, 0, wxALIGN_CENTRE|wxALL, 5 );

    item29->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item32 = new wxButton( parent, ID_B_Q_HIGH, "High", wxDefaultPosition, wxDefaultSize, 0 );
    item29->Add( item32, 0, wxALIGN_CENTRE|wxALL, 5 );

    item27->Add( item29, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

    item0->Add( item27, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5 );

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

wxSizer *SetupCachePage( wxWindow *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, "Cache Settings" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxCheckBox *item3 = new wxCheckBox( parent, ID_C_CACHE_ON, "Enable Biostruc cache", wxDefaultPosition, wxDefaultSize, 0 );
    item1->Add( item3, 0, wxALIGN_CENTER_VERTICAL|wxALL, 10 );

    wxStaticLine *item4 = new wxStaticLine( parent, ID_LINE, wxDefaultPosition, wxSize(20,-1), wxLI_HORIZONTAL );
    item1->Add( item4, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxBoxSizer *item5 = new wxBoxSizer( wxVERTICAL );

    wxFlexGridSizer *item6 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item6->AddGrowableCol( 1 );

    wxStaticText *item7 = new wxStaticText( parent, ID_T_CACHE_1, "Cache folder:", wxDefaultPosition, wxDefaultSize, 0 );
    item6->Add( item7, 0, wxALIGN_CENTRE|wxLEFT|wxRIGHT|wxTOP, 5 );

    item6->Add( 20, 20, 0, wxALIGN_CENTRE|wxLEFT|wxRIGHT|wxTOP, 5 );

    wxButton *item8 = new wxButton( parent, ID_B_CACHE_BROWSE, "Browse", wxDefaultPosition, wxDefaultSize, 0 );
    item6->Add( item8, 0, wxALIGN_CENTRE|wxLEFT|wxRIGHT|wxTOP, 5 );

    item5->Add( item6, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    wxTextCtrl *item9 = new wxTextCtrl( parent, ID_T_CACHE_FOLDER, "", wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item9, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxLEFT|wxRIGHT, 5 );

    item5->Add( 20, 20, 0, wxALIGN_CENTRE, 5 );

    wxFlexGridSizer *item10 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item10->AddGrowableCol( 2 );

    wxStaticText *item11 = new wxStaticText( parent, ID_T_CACHE_2, "Maximum folder size (MB):", wxDefaultPosition, wxDefaultSize, 0 );
    item10->Add( item11, 0, wxALIGN_CENTRE|wxALL, 5 );

    giCacheSize = new IntegerSpinCtrl(parent,
        1, 500, 1, 50,
        wxDefaultPosition, wxSize(50,SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1,SPIN_CTRL_HEIGHT));
    item10->Add(giCacheSize->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item10->Add(giCacheSize->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    item10->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item14 = new wxButton( parent, ID_B_CACHE_CLEAR, "Clear now", wxDefaultPosition, wxDefaultSize, 0 );
    item10->Add( item14, 0, wxALIGN_CENTRE|wxALL, 5 );

    item5->Add( item10, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    item1->Add( item5, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    item0->Add( item1, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

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
