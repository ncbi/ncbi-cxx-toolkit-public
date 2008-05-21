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
*      dialog for setting styles
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "remove_header_conflicts.hpp"

#include "style_dialog.hpp"
#include "style_manager.hpp"
#include "cn3d_tools.hpp"
#include "structure_set.hpp"
#include "messenger.hpp"
#include <algo/structure/wx_tools/wx_tools.hpp>

#include <wx/colordlg.h>


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ header from render_settings.wdr
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
#define ID_DONE 10001
#define ID_CANCEL 10002
#define ID_TEXT 10003
#define ID_ALWAYS_APPLY 10004
#define ID_APPLY 10005
wxSizer *LayoutNotebook( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

#define ID_PBB_SHOW 10006
#define ID_PBB_RENDER 10007
#define ID_PBB_COLOR 10008
#define ID_PBB_USER 10009
#define ID_PSIDE_SHOW 10010
#define ID_PSIDE_RENDER 10011
#define ID_PSIDE_COLOR 10012
#define ID_PSIDE_USER 10013
#define ID_NUC_SHOW 10014
#define ID_NUC_RENDER 10015
#define ID_NUC_COLOR 10016
#define ID_NUC_USER 10017
#define ID_NSIDE_SHOW 10018
#define ID_NSIDE_RENDER 10019
#define ID_NSIDE_COLOR 10020
#define ID_NSIDE_USER 10021
#define ID_HET_SHOW 10022
#define ID_HET_RENDER 10023
#define ID_HET_COLOR 10024
#define ID_HET_USER 10025
#define ID_SOLV_SHOW 10026
#define ID_SOLV_RENDER 10027
#define ID_SOLV_COLOR 10028
#define ID_SOLV_USER 10029
#define ID_CONN_SHOW 10030
#define ID_CONN_RENDER 10031
#define ID_CONN_COLOR 10032
#define ID_CONN_USER 10033
#define ID_HELX_SHOW 10034
#define ID_HELX_RENDER 10035
#define ID_HELX_COLOR 10036
#define ID_HELX_USER 10037
#define ID_STRN_SHOW 10038
#define ID_STRN_RENDER 10039
#define ID_STRN_COLOR 10040
#define ID_STRN_USER 10041
#define ID_VSS_SHOW 10042
#define ID_VSS_USER 10043
#define ID_HYD_SHOW 10044
#define ID_BG_USER 10045
wxSizer *LayoutSettingsPage( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

#define ID_TEXTCTRL 10046
#define ID_SPINBUTTON 10047
wxSizer *LayoutDetailsPage( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

#define ID_S_PROT 10048
#define ID_S_NUC 10049
#define ID_C_PROT_TYPE 10050
#define ID_C_NUC_TYPE 10051
#define ID_C_PROT_NUM 10052
#define ID_C_NUC_NUM 10053
#define ID_K_PROT_CONTRAST 10054
#define ID_K_NUC_CONTRAST 10055
#define ID_K_PROT_TERM 10056
#define ID_K_NUC_TERM 10057
#define ID_LINE 10058
#define ID_K_ION 10059
wxSizer *LayoutLabelsPage( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

////////////////////////////////////////////////////////////////////////////////////////////////

USING_NCBI_SCOPE;

// a bit of a kludge - but necessary to be able to use most of the wxDesigner code as is
static FloatingPointSpinCtrl *gfpSpaceFill, *gfpBallRadius, *gfpStickRadius, *gfpTubeRadius,
    *gfpTubeWormRadius, *gfpHelixRadius, *gfpStrandWidth, *gfpStrandThickness;


BEGIN_SCOPE(Cn3D)

TypeStringAssociator < StyleSettings::eBackboneType > StyleDialog::BackboneTypeStrings;
TypeStringAssociator < StyleSettings::eDrawingStyle > StyleDialog::DrawingStyleStrings;
TypeStringAssociator < StyleSettings::eColorScheme > StyleDialog::ColorSchemeStrings;
TypeStringAssociator < StyleSettings::eLabelType > StyleDialog::LabelTypeStrings;
TypeStringAssociator < StyleSettings::eNumberType > StyleDialog::NumberTypeStrings;

BEGIN_EVENT_TABLE(StyleDialog, wxDialog)
    EVT_CLOSE       (       StyleDialog::OnCloseWindow)
    EVT_CHOICE      (-1,    StyleDialog::OnChange)
    EVT_CHECKBOX    (-1,    StyleDialog::OnChange)
    EVT_SPINCTRL    (-1,    StyleDialog::OnSpin)
    EVT_BUTTON      (-1,    StyleDialog::OnButton)
    // generated when {Integer,FloatingPoint}SpinCtrl changes
    EVT_COMMAND     (-1, WX_TOOLS_NOTIFY_CHANGED, StyleDialog::OnChange)
END_EVENT_TABLE()

StyleDialog::StyleDialog(wxWindow* parent, StyleSettings *settingsToEdit, const StructureSet *set) :
    wxDialog(parent, -1, "Style Options", wxPoint(400, 100), wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
    editedSettings(settingsToEdit), originalSettings(*settingsToEdit),
    structureSet(set), changedSinceApply(false), changedEver(false), initialized(false)
{
    // setup maps for associating style types with strings
    SetupStyleStrings();

    // construct the panel
    wxPanel *panel = new wxPanel(this, -1);
    wxSizer *topSizer = LayoutNotebook(panel, false);

    // hook in the stuff that the wxDesigner-based code (LayoutPage2()) created
    fpSpaceFill = gfpSpaceFill;
    fpBallRadius = gfpBallRadius;
    fpStickRadius = gfpStickRadius;
    fpTubeRadius = gfpTubeRadius;
    fpTubeWormRadius = gfpTubeWormRadius;
    fpHelixRadius = gfpHelixRadius;
    fpStrandWidth = gfpStrandWidth;
    fpStrandThickness = gfpStrandThickness;

    // set defaults
    SetControls(*editedSettings);

    // call sizer stuff
    topSizer->Fit(this);
    topSizer->Fit(panel);
    topSizer->SetSizeHints(this);

    initialized = true;
}

StyleDialog::~StyleDialog(void)
{
    delete fpSpaceFill;
    delete fpBallRadius;
    delete fpStickRadius;
    delete fpTubeRadius;
    delete fpTubeWormRadius;
    delete fpHelixRadius;
    delete fpStrandWidth;
    delete fpStrandThickness;
}

void StyleDialog::SetupStyleStrings(void)
{
    if (BackboneTypeStrings.Size() == 0) {

        BackboneTypeStrings.Associate(StyleSettings::eOff, "None");
        BackboneTypeStrings.Associate(StyleSettings::eTrace, "Trace");
        BackboneTypeStrings.Associate(StyleSettings::ePartial, "Partial");
        BackboneTypeStrings.Associate(StyleSettings::eComplete, "Complete");

        DrawingStyleStrings.Associate(StyleSettings::eWire, "Wire");
        DrawingStyleStrings.Associate(StyleSettings::eTubes, "Tubes");
        DrawingStyleStrings.Associate(StyleSettings::eBallAndStick, "Ball and Stick");
        DrawingStyleStrings.Associate(StyleSettings::eSpaceFill, "Space Fill");
        DrawingStyleStrings.Associate(StyleSettings::eWireWorm, "Wire Worm");
        DrawingStyleStrings.Associate(StyleSettings::eTubeWorm, "Tube Worm");
        DrawingStyleStrings.Associate(StyleSettings::eWithArrows, "With Arrows");
        DrawingStyleStrings.Associate(StyleSettings::eWithoutArrows, "Without Arrows");

        ColorSchemeStrings.Associate(StyleSettings::eElement, "Element");
        ColorSchemeStrings.Associate(StyleSettings::eObject, "Object");
        ColorSchemeStrings.Associate(StyleSettings::eMolecule, "Molecule");
        ColorSchemeStrings.Associate(StyleSettings::eResidue, "Residue");
        ColorSchemeStrings.Associate(StyleSettings::eDomain, "Domain");
        ColorSchemeStrings.Associate(StyleSettings::eSecondaryStructure, "Secondary Structure");
        ColorSchemeStrings.Associate(StyleSettings::eUserSelect, "User Selection");
        ColorSchemeStrings.Associate(StyleSettings::eAligned, "Aligned");
        ColorSchemeStrings.Associate(StyleSettings::eIdentity, "Identity");
        ColorSchemeStrings.Associate(StyleSettings::eVariety, "Variety");
        ColorSchemeStrings.Associate(StyleSettings::eWeightedVariety, "Weighted Variety");
        ColorSchemeStrings.Associate(StyleSettings::eInformationContent, "Information Content");
        ColorSchemeStrings.Associate(StyleSettings::eFit, "Fit");
        ColorSchemeStrings.Associate(StyleSettings::eBlockFit, "Block Fit");
        ColorSchemeStrings.Associate(StyleSettings::eBlockZFit, "Normalized Block Fit");
        ColorSchemeStrings.Associate(StyleSettings::eBlockRowFit, "Block Row Fit");
        ColorSchemeStrings.Associate(StyleSettings::eTemperature, "Temperature");
        ColorSchemeStrings.Associate(StyleSettings::eHydrophobicity, "Hydrophobicity");
        ColorSchemeStrings.Associate(StyleSettings::eRainbow, "Rainbow");
        ColorSchemeStrings.Associate(StyleSettings::eCharge, "Charge");

        LabelTypeStrings.Associate(StyleSettings::eOneLetter, "One Letter");
        LabelTypeStrings.Associate(StyleSettings::eThreeLetter, "Three Letter");

        NumberTypeStrings.Associate(StyleSettings::eNoNumbers, "None");
        NumberTypeStrings.Associate(StyleSettings::eSequentialNumbering, "Sequential");
        NumberTypeStrings.Associate(StyleSettings::ePDBNumbering, "From PDB");
    }
}

static bool ConvertColor(const wxColour& wxcol, Vector *vec)
{
    vec->Set(((double) wxcol.Red())/255.0, ((double) wxcol.Green())/255.0, ((double) wxcol.Blue())/255.0);
    return true;
}

bool StyleDialog::GetBackboneStyle(StyleSettings::BackboneStyle *bbStyle,
    int showID, int renderID, int colorID, int userID)
{
    wxChoice *choice;
    wxButton *button;
    return (
        (choice = wxDynamicCast(FindWindow(showID), wxChoice)) != NULL &&
        BackboneTypeStrings.Get(choice->GetStringSelection().c_str(), &(bbStyle->type)) &&
        (choice = wxDynamicCast(FindWindow(renderID), wxChoice)) != NULL &&
        DrawingStyleStrings.Get(choice->GetStringSelection().c_str(), &(bbStyle->style)) &&
        (choice = wxDynamicCast(FindWindow(colorID), wxChoice)) != NULL &&
        ColorSchemeStrings.Get(choice->GetStringSelection().c_str(), &(bbStyle->colorScheme)) &&
        (button = wxDynamicCast(FindWindow(userID), wxButton)) != NULL &&
        ConvertColor(button->GetBackgroundColour(), &(bbStyle->userColor))
    );
}

static bool GetChecked(wxCheckBox *checkbox, bool *isChecked)
{
    if (!checkbox) return false;
    *isChecked = checkbox->GetValue();
    return true;
}

bool StyleDialog::GetGeneralStyle(StyleSettings::GeneralStyle *gStyle,
    int showID, int renderID, int colorID, int userID)
{
    wxCheckBox *checkbox;
    wxChoice *choice;
    wxButton *button;
    return (
        (checkbox = wxDynamicCast(FindWindow(showID), wxCheckBox)) != NULL &&
        GetChecked(checkbox, &(gStyle->isOn)) &&
        (choice = wxDynamicCast(FindWindow(renderID), wxChoice)) != NULL &&
        DrawingStyleStrings.Get(choice->GetStringSelection().c_str(), &(gStyle->style)) &&
        (choice = wxDynamicCast(FindWindow(colorID), wxChoice)) != NULL &&
        ColorSchemeStrings.Get(choice->GetStringSelection().c_str(), &(gStyle->colorScheme)) &&
        (button = wxDynamicCast(FindWindow(userID), wxButton)) != NULL &&
        ConvertColor(button->GetBackgroundColour(), &(gStyle->userColor))
    );
}

static bool GetInteger(wxSpinCtrl *spinctrl, int *value)
{
    if (!spinctrl) return false;
    *value = spinctrl->GetValue();
    return true;
}

bool StyleDialog::GetLabelStyle(StyleSettings::LabelStyle *lStyle,
    int spacingID, int typeID, int numberingID, int terminiID, int whiteID)
{
    wxSpinCtrl *spinctrl;
    wxChoice *choice;
    wxCheckBox *checkbox;
    return (
        (spinctrl = wxDynamicCast(FindWindow(spacingID), wxSpinCtrl)) != NULL &&
        GetInteger(spinctrl, &(lStyle->spacing)) &&
        (choice = wxDynamicCast(FindWindow(typeID), wxChoice)) != NULL &&
        LabelTypeStrings.Get(choice->GetStringSelection().c_str(), &(lStyle->type)) &&
        (choice = wxDynamicCast(FindWindow(numberingID), wxChoice)) != NULL &&
        NumberTypeStrings.Get(choice->GetStringSelection().c_str(), &(lStyle->numbering)) &&
        (checkbox = wxDynamicCast(FindWindow(terminiID), wxCheckBox)) != NULL &&
        GetChecked(checkbox, &(lStyle->terminiOn)) &&
        (checkbox = wxDynamicCast(FindWindow(whiteID), wxCheckBox)) != NULL &&
        GetChecked(checkbox, &(lStyle->white))
    );
}

bool StyleDialog::GetValues(StyleSettings *settings)
{
    wxCheckBox *checkbox;
    wxButton *button;
    bool okay = (
        settings != NULL &&
        GetBackboneStyle(&(settings->proteinBackbone),
            ID_PBB_SHOW, ID_PBB_RENDER, ID_PBB_COLOR, ID_PBB_USER) &&
        GetGeneralStyle(&(settings->proteinSidechains),
            ID_PSIDE_SHOW, ID_PSIDE_RENDER, ID_PSIDE_COLOR, ID_PSIDE_USER) &&
        GetBackboneStyle(&(settings->nucleotideBackbone),
            ID_NUC_SHOW, ID_NUC_RENDER, ID_NUC_COLOR, ID_NUC_USER) &&
        GetGeneralStyle(&(settings->nucleotideSidechains),
            ID_NSIDE_SHOW, ID_NSIDE_RENDER, ID_NSIDE_COLOR, ID_NSIDE_USER) &&
        GetGeneralStyle(&(settings->heterogens),
            ID_HET_SHOW, ID_HET_RENDER, ID_HET_COLOR, ID_HET_USER) &&
        GetGeneralStyle(&(settings->solvents),
            ID_SOLV_SHOW, ID_SOLV_RENDER, ID_SOLV_COLOR, ID_SOLV_USER) &&
        GetGeneralStyle(&(settings->connections),
            ID_CONN_SHOW, ID_CONN_RENDER, ID_CONN_COLOR, ID_CONN_USER) &&
        GetGeneralStyle(&(settings->helixObjects),
            ID_HELX_SHOW, ID_HELX_RENDER, ID_HELX_COLOR, ID_HELX_USER) &&
        GetGeneralStyle(&(settings->strandObjects),
            ID_STRN_SHOW, ID_STRN_RENDER, ID_STRN_COLOR, ID_STRN_USER) &&
        (checkbox = wxDynamicCast(FindWindow(ID_VSS_SHOW), wxCheckBox)) != NULL &&
        GetChecked(checkbox, &(settings->virtualDisulfidesOn)) &&
        (button = wxDynamicCast(FindWindow(ID_VSS_USER), wxButton)) != NULL &&
        ConvertColor(button->GetBackgroundColour(), &(settings->virtualDisulfideColor)) &&
        (checkbox = wxDynamicCast(FindWindow(ID_HYD_SHOW), wxCheckBox)) != NULL &&
        GetChecked(checkbox, &(settings->hydrogensOn)) &&
        (button = wxDynamicCast(FindWindow(ID_BG_USER), wxButton)) != NULL &&
        ConvertColor(button->GetBackgroundColour(), &(settings->backgroundColor)) &&
        fpSpaceFill->GetDouble(&(settings->spaceFillProportion)) &&
        fpBallRadius->GetDouble(&(settings->ballRadius)) &&
        fpStickRadius->GetDouble(&(settings->stickRadius)) &&
        fpTubeRadius->GetDouble(&(settings->tubeRadius)) &&
        fpTubeWormRadius->GetDouble(&(settings->tubeWormRadius)) &&
        fpHelixRadius->GetDouble(&(settings->helixRadius)) &&
        fpStrandWidth->GetDouble(&(settings->strandWidth)) &&
        fpStrandThickness->GetDouble(&(settings->strandThickness)) &&
        GetLabelStyle(&(settings->proteinLabels),
            ID_S_PROT, ID_C_PROT_TYPE, ID_C_PROT_NUM, ID_K_PROT_TERM, ID_K_PROT_CONTRAST) &&
        GetLabelStyle(&(settings->nucleotideLabels),
            ID_S_NUC, ID_C_NUC_TYPE, ID_C_NUC_NUM, ID_K_NUC_TERM, ID_K_NUC_CONTRAST) &&
        (checkbox = wxDynamicCast(FindWindow(ID_K_ION), wxCheckBox)) != NULL &&
        GetChecked(checkbox, &(settings->ionLabelsOn))
    );
    if (!okay) ERRORMSG("StyleDialog::GetValues() - control/parameter mismatch: invalid style?");
    return okay;
}

static bool SetChoiceToString(wxChoice *choice, const string& name)
{
    int field = choice->FindString(name.c_str());
    if (field < 0) return false;
    choice->SetSelection(field);
    return true;
}

static bool SetButtonColor(wxButton *button, const Vector& color)
{
    if (!button) return false;
    wxColour wxcol(
        static_cast<unsigned char>((color[0] + 0.000001) * 255),
        static_cast<unsigned char>((color[1] + 0.000001) * 255),
        static_cast<unsigned char>((color[2] + 0.000001) * 255)
    );
    // set both foreground and background colors, so that if e.g. on wxGTK themes are
    // used (and thus backgrounds are not user-selectable), at least the text (foreground)
    // color will indicate the user's color choice
    button->SetForegroundColour(wxcol);
    button->SetBackgroundColour(wxcol);
    return true;
}

bool StyleDialog::SetBackboneStyle(const StyleSettings::BackboneStyle& bbStyle,
    int showID, int renderID, int colorID, int userID)
{
    string name;
    wxChoice *choice;
    wxButton *button;
    return (
        BackboneTypeStrings.Get(bbStyle.type, &name) &&
        (choice = wxDynamicCast(FindWindow(showID), wxChoice)) != NULL &&
        SetChoiceToString(choice, name) &&
        DrawingStyleStrings.Get(bbStyle.style, &name) &&
        (choice = wxDynamicCast(FindWindow(renderID), wxChoice)) != NULL &&
        SetChoiceToString(choice, name) &&
        ColorSchemeStrings.Get(bbStyle.colorScheme, &name) &&
        (choice = wxDynamicCast(FindWindow(colorID), wxChoice)) != NULL &&
        SetChoiceToString(choice, name) &&
        (button = wxDynamicCast(FindWindow(userID), wxButton)) != NULL &&
        SetButtonColor(button, bbStyle.userColor)
    );
}

static bool SetChecked(wxCheckBox *checkbox, bool isChecked)
{
    if (!checkbox) return false;
    checkbox->SetValue(isChecked);
    return true;
}

bool StyleDialog::SetGeneralStyle(const StyleSettings::GeneralStyle& gStyle,
    int showID, int renderID, int colorID, int userID)
{
    string name;
    wxCheckBox *checkbox;
    wxChoice *choice;
    wxButton *button;
    return (
        (checkbox = wxDynamicCast(FindWindow(showID), wxCheckBox)) != NULL &&
        SetChecked(checkbox, gStyle.isOn) &&
        DrawingStyleStrings.Get(gStyle.style, &name) &&
        (choice = wxDynamicCast(FindWindow(renderID), wxChoice)) != NULL &&
        SetChoiceToString(choice, name) &&
        ColorSchemeStrings.Get(gStyle.colorScheme, &name) &&
        (choice = wxDynamicCast(FindWindow(colorID), wxChoice)) != NULL &&
        SetChoiceToString(choice, name) &&
        (button = wxDynamicCast(FindWindow(userID), wxButton)) != NULL &&
        SetButtonColor(button, gStyle.userColor)
    );
}

static bool SetInteger(wxSpinCtrl *spinctrl, int value)
{
    if (!spinctrl) return false;
    spinctrl->SetValue(value);
    return true;
}

bool StyleDialog::SetLabelStyle(const StyleSettings::LabelStyle& lStyle,
    int spacingID, int typeID, int numberingID, int terminiID, int whiteID)
{
    string name;
    wxSpinCtrl *spinctrl;
    wxCheckBox *checkbox;
    wxChoice *choice;
    return (
        (spinctrl = wxDynamicCast(FindWindow(spacingID), wxSpinCtrl)) != NULL &&
        SetInteger(spinctrl, lStyle.spacing) &&
        LabelTypeStrings.Get(lStyle.type, &name) &&
        (choice = wxDynamicCast(FindWindow(typeID), wxChoice)) != NULL &&
        SetChoiceToString(choice, name) &&
        NumberTypeStrings.Get(lStyle.numbering, &name) &&
        (choice = wxDynamicCast(FindWindow(numberingID), wxChoice)) != NULL &&
        SetChoiceToString(choice, name) &&
        (checkbox = wxDynamicCast(FindWindow(terminiID), wxCheckBox)) != NULL &&
        SetChecked(checkbox, lStyle.terminiOn) &&
        (checkbox = wxDynamicCast(FindWindow(whiteID), wxCheckBox)) != NULL &&
        SetChecked(checkbox, lStyle.white)
    );
}

bool StyleDialog::SetControls(const StyleSettings& settings)
{
    wxCheckBox *checkbox;
    wxButton *button;
    bool okay = (
        SetBackboneStyle(settings.proteinBackbone,
            ID_PBB_SHOW, ID_PBB_RENDER, ID_PBB_COLOR, ID_PBB_USER) &&
        SetGeneralStyle(settings.proteinSidechains,
            ID_PSIDE_SHOW, ID_PSIDE_RENDER, ID_PSIDE_COLOR, ID_PSIDE_USER) &&
        SetBackboneStyle(settings.nucleotideBackbone,
            ID_NUC_SHOW, ID_NUC_RENDER, ID_NUC_COLOR, ID_NUC_USER) &&
        SetGeneralStyle(settings.nucleotideSidechains,
            ID_NSIDE_SHOW, ID_NSIDE_RENDER, ID_NSIDE_COLOR, ID_NSIDE_USER) &&
        SetGeneralStyle(settings.heterogens,
            ID_HET_SHOW, ID_HET_RENDER, ID_HET_COLOR, ID_HET_USER) &&
        SetGeneralStyle(settings.solvents,
            ID_SOLV_SHOW, ID_SOLV_RENDER, ID_SOLV_COLOR, ID_SOLV_USER) &&
        SetGeneralStyle(settings.connections,
            ID_CONN_SHOW, ID_CONN_RENDER, ID_CONN_COLOR, ID_CONN_USER) &&
        SetGeneralStyle(settings.helixObjects,
            ID_HELX_SHOW, ID_HELX_RENDER, ID_HELX_COLOR, ID_HELX_USER) &&
        SetGeneralStyle(settings.strandObjects,
            ID_STRN_SHOW, ID_STRN_RENDER, ID_STRN_COLOR, ID_STRN_USER) &&
        (checkbox = wxDynamicCast(FindWindow(ID_VSS_SHOW), wxCheckBox)) != NULL &&
        SetChecked(checkbox, settings.virtualDisulfidesOn) &&
        (button = wxDynamicCast(FindWindow(ID_VSS_USER), wxButton)) != NULL &&
        SetButtonColor(button, settings.virtualDisulfideColor) &&
        (checkbox = wxDynamicCast(FindWindow(ID_HYD_SHOW), wxCheckBox)) != NULL &&
        SetChecked(checkbox, settings.hydrogensOn) &&
        (button = wxDynamicCast(FindWindow(ID_BG_USER), wxButton)) != NULL &&
        SetButtonColor(button, settings.backgroundColor) &&
        fpSpaceFill->SetDouble(settings.spaceFillProportion) &&
        fpBallRadius->SetDouble(settings.ballRadius) &&
        fpStickRadius->SetDouble(settings.stickRadius) &&
        fpTubeRadius->SetDouble(settings.tubeRadius) &&
        fpTubeWormRadius->SetDouble(settings.tubeWormRadius) &&
        fpHelixRadius->SetDouble(settings.helixRadius) &&
        fpStrandWidth->SetDouble(settings.strandWidth) &&
        fpStrandThickness->SetDouble(settings.strandThickness) &&
        SetLabelStyle(settings.proteinLabels,
            ID_S_PROT, ID_C_PROT_TYPE, ID_C_PROT_NUM, ID_K_PROT_TERM, ID_K_PROT_CONTRAST) &&
        SetLabelStyle(settings.nucleotideLabels,
            ID_S_NUC, ID_C_NUC_TYPE, ID_C_NUC_NUM, ID_K_NUC_TERM, ID_K_NUC_CONTRAST) &&
        (checkbox = wxDynamicCast(FindWindow(ID_K_ION), wxCheckBox)) != NULL &&
        SetChecked(checkbox, settings.ionLabelsOn)
    );
    if (!okay) ERRORMSG("StyleDialog::SetControls() - control/parameter mismatch: invalid style?");
    return okay;
}

// same as hitting done button
void StyleDialog::OnCloseWindow(wxCloseEvent& event)
{
    StyleSettings dummy;
    if (GetValues(&dummy)) {
        if (changedSinceApply) {
            *editedSettings = dummy;
            GlobalMessenger()->PostRedrawAllStructures();
            GlobalMessenger()->PostRedrawAllSequenceViewers();
//            structureSet->SetDataChanged(StructureSet::eStyleData);
        }
    } else
        wxBell();
    EndModal(wxOK);
}

void StyleDialog::OnButton(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case ID_DONE: {
            StyleSettings dummy;
            if (GetValues(&dummy)) {
                if (changedSinceApply) {
                    *editedSettings = dummy;
                    GlobalMessenger()->PostRedrawAllStructures();
                    GlobalMessenger()->PostRedrawAllSequenceViewers();
                }
//                if (changedEver)
//                    structureSet->SetDataChanged(StructureSet::eStyleData);
                EndModal(wxOK);
            } else
                wxBell();
            break;
        }
        case ID_APPLY: {
            StyleSettings dummy;
            if (GetValues(&dummy)) {
                *editedSettings = dummy;
                GlobalMessenger()->PostRedrawAllStructures();
                GlobalMessenger()->PostRedrawAllSequenceViewers();
                changedSinceApply = false;
//                structureSet->SetDataChanged(StructureSet::eStyleData);
            } else
                wxBell();
            break;
        }
        case ID_CANCEL:
            if (changedEver) {
                *editedSettings = originalSettings;
                GlobalMessenger()->PostRedrawAllStructures();
                GlobalMessenger()->PostRedrawAllSequenceViewers();
            }
            EndModal(wxCANCEL);
            break;
        default:
            if (!HandleColorButton(event.GetId()))
                event.Skip();
    }
}

// return true if bID does actually correspond to a valid color button
bool StyleDialog::HandleColorButton(int bID)
{
    // just a filter to make sure the button pushed is really a user color button
    switch (bID) {
        case ID_PBB_USER: case ID_PSIDE_USER: case ID_NUC_USER: case ID_NSIDE_USER:
        case ID_HET_USER: case ID_SOLV_USER: case ID_CONN_USER: case ID_HELX_USER:
        case ID_STRN_USER: case ID_VSS_USER: case ID_BG_USER:
            break;
        default:
            return false;
    }

    wxButton *button = wxDynamicCast(FindWindow(bID), wxButton);
    if (!button) {
        ERRORMSG("StyleDialog::HandleColorButton() - can't find button of given ID");
        return false;
    }

    wxColour userColor = wxGetColourFromUser(this, button->GetBackgroundColour());
    if (userColor.Ok()) {
        button->SetBackgroundColour(userColor);
        wxCommandEvent fake;
        OnChange(fake);
    }
    return true;
}

void StyleDialog::OnChange(wxCommandEvent& event)
{
    TRACEMSG("control changed");
    StyleSettings tmpSettings;
    if (!GetValues(&tmpSettings) ||
        !structureSet->styleManager->CheckStyleSettings(&tmpSettings) ||
        !SetControls(tmpSettings)) {
        ERRORMSG("StyleDialog::OnChange() - error adjusting settings/controls");
        return;
    }
    changedSinceApply = changedEver = true;
    if ((wxDynamicCast(FindWindow(ID_ALWAYS_APPLY), wxCheckBox))->GetValue()) {
        *editedSettings = tmpSettings;
        GlobalMessenger()->PostRedrawAllStructures();
        GlobalMessenger()->PostRedrawAllSequenceViewers();
        changedSinceApply = false;
    }
}

void StyleDialog::OnSpin(wxSpinEvent& event)
{
    if (!initialized) {
        event.Skip();
        return;
    }
    wxCommandEvent fake;
    OnChange(fake);
}

END_SCOPE(Cn3D)


//////////////////////////////////////////////////////////////////////////////////////////////////
// Three of four of these following functions are taken *without* modification
// from wxDesigner C++ code generated from render_settings.wdr. The function LayoutDetailsPage()
// is modified from wxDesigner output in order to use custom FloatingPointSpinCtrl's
//////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *LayoutNotebook( wxPanel *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxNotebook *item2 = new wxNotebook( parent, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize, 0 );
//    wxNotebookSizer *item1 = new wxNotebookSizer( item2 );

    wxPanel *item3 = new wxPanel( item2, -1 );
    LayoutSettingsPage( item3, FALSE );
    item2->AddPage( item3, "Settings" );

    wxPanel *item4 = new wxPanel( item2, -1 );
    LayoutLabelsPage( item4, FALSE );
    item2->AddPage( item4, "Labels" );

    wxPanel *item5 = new wxPanel( item2, -1 );
    LayoutDetailsPage( item5, FALSE );
    item2->AddPage( item5, "Details" );

    item0->Add( item2, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxBoxSizer *item6 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item7 = new wxButton( parent, ID_DONE, "Done", wxDefaultPosition, wxDefaultSize, 0 );
    item7->SetDefault();
    item6->Add( item7, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item8 = new wxButton( parent, ID_CANCEL, "Cancel", wxDefaultPosition, wxDefaultSize, 0 );
    item6->Add( item8, 0, wxALIGN_CENTRE|wxALL, 5 );

    item6->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item9 = new wxStaticText( parent, ID_TEXT, "Apply after each change?", wxDefaultPosition, wxDefaultSize, 0 );
    item6->Add( item9, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxCheckBox *item10 = new wxCheckBox( parent, ID_ALWAYS_APPLY, "", wxDefaultPosition, wxDefaultSize, 0 );
    item10->SetValue( TRUE );
    item6->Add( item10, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item11 = new wxButton( parent, ID_APPLY, "Apply", wxDefaultPosition, wxDefaultSize, 0 );
    item6->Add( item11, 0, wxALIGN_CENTRE|wxALL, 5 );

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

wxSizer *LayoutSettingsPage( wxPanel *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, "Rendering Settings" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxFlexGridSizer *item3 = new wxFlexGridSizer( 5, 0, 0 );

    wxStaticText *item4 = new wxStaticText( parent, ID_TEXT, "Group", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item3->Add( item4, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item5 = new wxStaticText( parent, ID_TEXT, "Show", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item3->Add( item5, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item6 = new wxStaticText( parent, ID_TEXT, "Rendering", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item3->Add( item6, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item7 = new wxStaticText( parent, ID_TEXT, "Color Scheme", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item3->Add( item7, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item8 = new wxStaticText( parent, ID_TEXT, "User Color", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item3->Add( item8, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item9 = new wxStaticText( parent, ID_TEXT, "Protein backbone:", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item3->Add( item9, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxString strs10[] =
    {
        "None",
        "Trace",
        "Partial",
        "Complete"
    };
    wxChoice *item10 = new wxChoice( parent, ID_PBB_SHOW, wxDefaultPosition, wxDefaultSize, 4, strs10, 0 );
    item3->Add( item10, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs11[] =
    {
        "Wire Worm",
        "Tube Worm",
        "Wire",
        "Tubes",
        "Ball and Stick",
        "Space Fill"
    };
    wxChoice *item11 = new wxChoice( parent, ID_PBB_RENDER, wxDefaultPosition, wxDefaultSize, 6, strs11, 0 );
    item3->Add( item11, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs12[] =
    {
        "Element",
        "Object",
        "Molecule",
        "Residue",
        "Domain",
        "Secondary Structure",
        "Temperature",
        "Rainbow",
        "Charge",
        "Hydrophobicity",
        "User Selection",
        "Aligned",
        "Identity",
        "Variety",
        "Weighted Variety",
        "Information Content",
        "Fit",
        "Block Fit",
        "Normalized Block Fit",
        "Block Row Fit"
    };
    wxChoice *item12 = new wxChoice( parent, ID_PBB_COLOR, wxDefaultPosition, wxDefaultSize, 20, strs12, 0 );
    item3->Add( item12, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item13 = new wxButton( parent, ID_PBB_USER, "Set Color", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item13, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item14 = new wxStaticText( parent, ID_TEXT, "Protein sidechains:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item14, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxCheckBox *item15 = new wxCheckBox( parent, ID_PSIDE_SHOW, "", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item15, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxString strs16[] =
    {
        "Wire",
        "Tubes",
        "Ball and Stick",
        "Space Fill"
    };
    wxChoice *item16 = new wxChoice( parent, ID_PSIDE_RENDER, wxDefaultPosition, wxDefaultSize, 4, strs16, 0 );
    item3->Add( item16, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs17[] =
    {
        "Element",
        "Object",
        "Molecule",
        "Residue",
        "Domain",
        "Temperature",
        "Rainbow",
        "Charge",
        "Hydrophobicity",
        "User Selection",
        "Aligned",
        "Identity",
        "Variety",
        "Weighted Variety",
        "Information Content",
        "Fit",
        "Block Fit",
        "Normalized Block Fit",
        "Block Row Fit"
    };
    wxChoice *item17 = new wxChoice( parent, ID_PSIDE_COLOR, wxDefaultPosition, wxDefaultSize, 19, strs17, 0 );
    item3->Add( item17, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item18 = new wxButton( parent, ID_PSIDE_USER, "Set Color", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item18, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item19 = new wxStaticText( parent, ID_TEXT, "Nucleotide backbone:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item19, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxString strs20[] =
    {
        "None",
        "Trace",
        "Partial",
        "Complete"
    };
    wxChoice *item20 = new wxChoice( parent, ID_NUC_SHOW, wxDefaultPosition, wxDefaultSize, 4, strs20, 0 );
    item3->Add( item20, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs21[] =
    {
        "Wire Worm",
        "Tube Worm",
        "Wire",
        "Tubes",
        "Ball and Stick",
        "Space Fill"
    };
    wxChoice *item21 = new wxChoice( parent, ID_NUC_RENDER, wxDefaultPosition, wxDefaultSize, 6, strs21, 0 );
    item3->Add( item21, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs22[] =
    {
        "Element",
        "Object",
        "Molecule",
        "Residue",
        "Domain",
        "Temperature",
        "Rainbow",
        "User Selection"
    };
    wxChoice *item22 = new wxChoice( parent, ID_NUC_COLOR, wxDefaultPosition, wxDefaultSize, 8, strs22, 0 );
    item3->Add( item22, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item23 = new wxButton( parent, ID_NUC_USER, "Set Color", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item23, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item24 = new wxStaticText( parent, ID_TEXT, "Nucleotide sidechains:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item24, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxCheckBox *item25 = new wxCheckBox( parent, ID_NSIDE_SHOW, "", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item25, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxString strs26[] =
    {
        "Wire",
        "Tubes",
        "Ball and Stick",
        "Space Fill"
    };
    wxChoice *item26 = new wxChoice( parent, ID_NSIDE_RENDER, wxDefaultPosition, wxDefaultSize, 4, strs26, 0 );
    item3->Add( item26, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs27[] =
    {
        "Element",
        "Object",
        "Molecule",
        "Residue",
        "Domain",
        "Temperature",
        "Rainbow",
        "User Selection"
    };
    wxChoice *item27 = new wxChoice( parent, ID_NSIDE_COLOR, wxDefaultPosition, wxDefaultSize, 8, strs27, 0 );
    item3->Add( item27, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item28 = new wxButton( parent, ID_NSIDE_USER, "Set Color", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item28, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item29 = new wxStaticText( parent, ID_TEXT, "Heterogens:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item29, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxCheckBox *item30 = new wxCheckBox( parent, ID_HET_SHOW, "", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item30, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxString strs31[] =
    {
        "Wire",
        "Tubes",
        "Ball and Stick",
        "Space Fill"
    };
    wxChoice *item31 = new wxChoice( parent, ID_HET_RENDER, wxDefaultPosition, wxDefaultSize, 4, strs31, 0 );
    item3->Add( item31, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs32[] =
    {
        "Element",
        "Object",
        "Molecule",
        "Temperature",
        "User Selection"
    };
    wxChoice *item32 = new wxChoice( parent, ID_HET_COLOR, wxDefaultPosition, wxDefaultSize, 5, strs32, 0 );
    item3->Add( item32, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item33 = new wxButton( parent, ID_HET_USER, "Set Color", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item33, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item34 = new wxStaticText( parent, ID_TEXT, "Solvents:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item34, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxCheckBox *item35 = new wxCheckBox( parent, ID_SOLV_SHOW, "", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item35, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxString strs36[] =
    {
        "Wire",
        "Tubes",
        "Ball and Stick",
        "Space Fill"
    };
    wxChoice *item36 = new wxChoice( parent, ID_SOLV_RENDER, wxDefaultPosition, wxDefaultSize, 4, strs36, 0 );
    item3->Add( item36, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs37[] =
    {
        "Element",
        "Object",
        "Molecule",
        "Temperature",
        "User Selection"
    };
    wxChoice *item37 = new wxChoice( parent, ID_SOLV_COLOR, wxDefaultPosition, wxDefaultSize, 5, strs37, 0 );
    item3->Add( item37, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item38 = new wxButton( parent, ID_SOLV_USER, "Set Color", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item38, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item39 = new wxStaticText( parent, ID_TEXT, "Connections:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item39, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxCheckBox *item40 = new wxCheckBox( parent, ID_CONN_SHOW, "", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item40, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxString strs41[] =
    {
        "Wire",
        "Tubes"
    };
    wxChoice *item41 = new wxChoice( parent, ID_CONN_RENDER, wxDefaultPosition, wxDefaultSize, 2, strs41, 0 );
    item3->Add( item41, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs42[] =
    {
        "User Selection"
    };
    wxChoice *item42 = new wxChoice( parent, ID_CONN_COLOR, wxDefaultPosition, wxDefaultSize, 1, strs42, 0 );
    item3->Add( item42, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item43 = new wxButton( parent, ID_CONN_USER, "Set Color", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item43, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item44 = new wxStaticText( parent, ID_TEXT, "Helix objects:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item44, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxCheckBox *item45 = new wxCheckBox( parent, ID_HELX_SHOW, "", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item45, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxString strs46[] =
    {
        "With Arrows",
        "Without Arrows"
    };
    wxChoice *item46 = new wxChoice( parent, ID_HELX_RENDER, wxDefaultPosition, wxDefaultSize, 2, strs46, 0 );
    item3->Add( item46, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs47[] =
    {
        "Object",
        "Molecule",
        "Domain",
        "Secondary Structure",
        "User Selection"
    };
    wxChoice *item47 = new wxChoice( parent, ID_HELX_COLOR, wxDefaultPosition, wxDefaultSize, 5, strs47, 0 );
    item3->Add( item47, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item48 = new wxButton( parent, ID_HELX_USER, "Set Color", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item48, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item49 = new wxStaticText( parent, ID_TEXT, "Strand objects:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item49, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxCheckBox *item50 = new wxCheckBox( parent, ID_STRN_SHOW, "", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item50, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxString strs51[] =
    {
        "With Arrows",
        "Without Arrows"
    };
    wxChoice *item51 = new wxChoice( parent, ID_STRN_RENDER, wxDefaultPosition, wxDefaultSize, 2, strs51, 0 );
    item3->Add( item51, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs52[] =
    {
        "Object",
        "Molecule",
        "Domain",
        "Secondary Structure",
        "User Selection"
    };
    wxChoice *item52 = new wxChoice( parent, ID_STRN_COLOR, wxDefaultPosition, wxDefaultSize, 5, strs52, 0 );
    item3->Add( item52, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item53 = new wxButton( parent, ID_STRN_USER, "Set Color", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item53, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item54 = new wxStaticText( parent, ID_TEXT, "Virtual disulfides:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item54, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxCheckBox *item55 = new wxCheckBox( parent, ID_VSS_SHOW, "", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item55, 0, wxALIGN_CENTRE|wxALL, 5 );

    item3->Add( 5, 5, 0, wxALIGN_CENTRE|wxALL, 5 );

    item3->Add( 5, 5, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item56 = new wxButton( parent, ID_VSS_USER, "Set Color", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item56, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item57 = new wxStaticText( parent, ID_TEXT, "Hydrogens:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item57, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxCheckBox *item58 = new wxCheckBox( parent, ID_HYD_SHOW, "", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item58, 0, wxALIGN_CENTRE|wxALL, 5 );

    item3->Add( 5, 5, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item59 = new wxStaticText( parent, ID_TEXT, "Background:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item59, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxButton *item60 = new wxButton( parent, ID_BG_USER, "Set Color", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item60, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item3, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item1, 0, wxALIGN_CENTRE|wxALL, 5 );

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

wxSizer *LayoutLabelsPage( wxPanel *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, "Labeling Settings" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxBoxSizer *item3 = new wxBoxSizer( wxVERTICAL );

    wxGridSizer *item4 = new wxGridSizer( 3, 5, 0 );

    item4->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item5 = new wxStaticText( parent, ID_TEXT, "Protein backbone:", wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item5, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item6 = new wxStaticText( parent, ID_TEXT, "Nucleotide backbone:", wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item6, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item7 = new wxStaticText( parent, ID_TEXT, "Spacing (0 = none):", wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item7, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxSpinCtrl *item8 = new wxSpinCtrl( parent, ID_S_PROT, "0", wxDefaultPosition, wxDefaultSize, 0, 0, 100, 0 );
    item4->Add( item8, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxSpinCtrl *item9 = new wxSpinCtrl( parent, ID_S_NUC, "0", wxDefaultPosition, wxDefaultSize, 0, 0, 100, 0 );
    item4->Add( item9, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxStaticText *item10 = new wxStaticText( parent, ID_TEXT, "Type:", wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item10, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs11[] =
    {
        "One Letter",
        "Three Letter"
    };
    wxChoice *item11 = new wxChoice( parent, ID_C_PROT_TYPE, wxDefaultPosition, wxDefaultSize, 2, strs11, 0 );
    item4->Add( item11, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs12[] =
    {
        "One Letter",
        "Three Letter"
    };
    wxChoice *item12 = new wxChoice( parent, ID_C_NUC_TYPE, wxDefaultPosition, wxDefaultSize, 2, strs12, 0 );
    item4->Add( item12, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxStaticText *item13 = new wxStaticText( parent, ID_TEXT, "Numbering:", wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item13, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs14[] =
    {
        "None",
        "Sequential",
        "From PDB"
    };
    wxChoice *item14 = new wxChoice( parent, ID_C_PROT_NUM, wxDefaultPosition, wxDefaultSize, 3, strs14, 0 );
    item4->Add( item14, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxString strs15[] =
    {
        "None",
        "Sequential",
        "From PDB"
    };
    wxChoice *item15 = new wxChoice( parent, ID_C_NUC_NUM, wxDefaultPosition, wxDefaultSize, 3, strs15, 0 );
    item4->Add( item15, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxStaticText *item16 = new wxStaticText( parent, ID_TEXT, "Contrast with background:", wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item16, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxCheckBox *item17 = new wxCheckBox( parent, ID_K_PROT_CONTRAST, "", wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item17, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxCheckBox *item18 = new wxCheckBox( parent, ID_K_NUC_CONTRAST, "", wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item18, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item19 = new wxStaticText( parent, ID_TEXT, "Termini:", wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item19, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxCheckBox *item20 = new wxCheckBox( parent, ID_K_PROT_TERM, "", wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxCheckBox *item21 = new wxCheckBox( parent, ID_K_NUC_TERM, "", wxDefaultPosition, wxDefaultSize, 0 );
    item4->Add( item21, 0, wxALIGN_CENTRE|wxALL, 5 );

    item3->Add( item4, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    wxStaticLine *item22 = new wxStaticLine( parent, ID_LINE, wxDefaultPosition, wxSize(20,-1), wxLI_HORIZONTAL );
    item3->Add( item22, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxBoxSizer *item23 = new wxBoxSizer( wxHORIZONTAL );

    wxStaticText *item24 = new wxStaticText( parent, ID_TEXT, "Metal ion labels:", wxDefaultPosition, wxDefaultSize, 0 );
    item23->Add( item24, 0, wxALIGN_CENTRE|wxALL, 5 );

    item23->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxCheckBox *item25 = new wxCheckBox( parent, ID_K_ION, "", wxDefaultPosition, wxDefaultSize, 0 );
    item23->Add( item25, 0, wxALIGN_CENTRE|wxALL, 5 );

    item3->Add( item23, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item3, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item1, 0, wxALIGN_CENTRE|wxALL, 5 );

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


/////
// this is modified from wxDesigner output in order to use custom SpinCtrl's
/////

wxSizer *LayoutDetailsPage(wxPanel *parent, bool call_fit, bool set_sizer)
{
    wxBoxSizer *item0 = new wxBoxSizer(wxVERTICAL);
    wxStaticBox *item2 = new wxStaticBox(parent, -1, "Rendering Details");
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer(item2, wxVERTICAL);
    wxFlexGridSizer *grid = new wxFlexGridSizer(3, 0, 0);

    // space fill proportion
    wxStaticText *item4 = new wxStaticText(parent, ID_TEXT, "Space fill size:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    grid->Add(item4, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    gfpSpaceFill = new FloatingPointSpinCtrl(parent,
        0.0, 10.0, 0.1, 1.0,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    grid->Add(gfpSpaceFill->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    grid->Add(gfpSpaceFill->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    // tube radius
    wxStaticText *item7 = new wxStaticText(parent, ID_TEXT, "Tube radius:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    grid->Add(item7, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    gfpTubeRadius = new FloatingPointSpinCtrl(parent,
        0.0, 5.0, 0.02, 0.3,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    grid->Add(gfpTubeRadius->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    grid->Add(gfpTubeRadius->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    // worm tube
    wxStaticText *item10 = new wxStaticText(parent, ID_TEXT, "Worm tube radius:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    grid->Add(item10, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    gfpTubeWormRadius = new FloatingPointSpinCtrl(parent,
        0.0, 5.0, 0.02, 0.3,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    grid->Add(gfpTubeWormRadius->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    grid->Add(gfpTubeWormRadius->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    // ball radius
    wxStaticText *item13 = new wxStaticText(parent, ID_TEXT, "Ball radius:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    grid->Add(item13, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    gfpBallRadius = new FloatingPointSpinCtrl(parent,
        0.0, 10.0, 0.05, 0.4,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    grid->Add(gfpBallRadius->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    grid->Add(gfpBallRadius->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    // stick radius
    wxStaticText *item16 = new wxStaticText(parent, ID_TEXT, "Stick radius:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    grid->Add(item16, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    gfpStickRadius = new FloatingPointSpinCtrl(parent,
        0.0, 5.0, 0.01, 0.2,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    grid->Add(gfpStickRadius->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    grid->Add(gfpStickRadius->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    // spacer
    grid->Add(1, 1, 0, wxALIGN_CENTRE|wxALL, 5);
    grid->Add(1, 1, 0, wxALIGN_CENTRE|wxALL, 5);
    grid->Add(1, 1, 0, wxALIGN_CENTRE|wxALL, 5);

    // helix radius
    wxStaticText *item19 = new wxStaticText(parent, ID_TEXT, "Helix radius:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    grid->Add(item19, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    gfpHelixRadius = new FloatingPointSpinCtrl(parent,
        0.0, 10.0, 0.1, 1.8,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    grid->Add(gfpHelixRadius->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    grid->Add(gfpHelixRadius->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    // strand width
    wxStaticText *item22 = new wxStaticText(parent, ID_TEXT, "Strand width:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    grid->Add(item22, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    gfpStrandWidth = new FloatingPointSpinCtrl(parent,
        0.0, 10.0, 0.1, 2.0,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    grid->Add(gfpStrandWidth->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    grid->Add(gfpStrandWidth->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    // strand thickness
    wxStaticText *item25 = new wxStaticText(parent, ID_TEXT, "Strand thickness:", wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT);
    grid->Add(item25, 0, wxALIGN_RIGHT|wxALIGN_CENTER_VERTICAL|wxALL, 5);
    gfpStrandThickness = new FloatingPointSpinCtrl(parent,
        0.0, 5.0, 0.05, 0.5,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    grid->Add(gfpStrandThickness->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    grid->Add(gfpStrandThickness->GetSpinButton(), 0, wxALIGN_CENTRE|wxRIGHT|wxTOP|wxBOTTOM, 5);

    item1->Add(grid, 0, wxALIGN_CENTRE|wxALL, 5);
    item0->Add(item1, 0, wxALIGN_CENTRE|wxALL, 5);
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
