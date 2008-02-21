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
*      implementation of BMA refiner as MSAInterface
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>

#include <algo/structure/bma_refine/InterfaceGUI.hpp>
#include <algo/structure/bma_refine/Interface.hpp>
#include <algo/structure/bma_refine/RefinerDefs.hpp>


BEGIN_SCOPE(align_refine)
USING_SCOPE(Cn3D);

// diagnostic streams
#define TRACEMSG(stream) ERR_POST(ncbi::Trace << stream)
#define INFOMSG(stream) ERR_POST(ncbi::Info << stream)
#define WARNINGMSG(stream) ERR_POST(ncbi::Warning << stream)
#define ERRORMSG(stream) ERR_POST(ncbi::Error << stream)
#define FATALMSG(stream) ERR_POST(ncbi::Fatal << stream)

static const wxString phaseOrderStrings[] = {"Shift blocks first", "Expand/shrink blocks first"};
//  These listed in order of enum RefinerRowSelectorCode
static const wxString looSelectionOrderStrings[] = {"Random order", "Worst-to-best self-hit", "Best-to-worst self-hit"};
//  These listed in order of enum BlockBoundaryAlgorithmMethod
static const wxString blockEditAlgStrings[] = {"No", "Only expand", "Only shrink", "Expand and shrink"};

bool BMARefinerOptionsDialog::SetRefinerOptionsViaDialog(wxWindow* parent, BMARefinerInterface& interface)
{
    GeneralRefinerParams *genl = &(interface.options->genl);
    LeaveOneOutParams *loo = &(interface.options->loo);
    BlockEditingParams *be = &(interface.options->be);
    const vector < string >& rowTitles = interface.rowTitles;

    BMARefinerOptionsDialog dialog(parent, *genl, *loo, *be, rowTitles);
    bool ok = (dialog.ShowModal() == wxOK);
    if (ok && !dialog.GetParameters(genl, loo, be))
        ERRORMSG("SetRefinerOptionsViaDialog() - Error getting refiner options from dialog");
    return ok;
}

/////////////////////////////////////////////////////////////////////////////////////
////// BMARefinerOptionsDialog stuff
////// taken (and modified) from block_aligner_dialog.wdr code
/////////////////////////////////////////////////////////////////////////////////////

const int ID_TEXT = 12000;
const int ID_CHECKBOX = 12001;
const int ID_SPINCTRL = 12002;
const int ID_TEXTCTRL = 12003;
const int ID_FIX_STRUCT_CHECKBOX = 12004;
const int ID_FULL_SEQ_CHECKBOX = 12005;
const int ID_BE_COMBOBOX = 12006;
const int ID_BEXTEND_FIRST_CHECKBOX = 12007;
const int ID_ALL_UNST_SEQ_CHECKBOX = 12008;
const int ID_PHASE_ORDER_COMBOBOX = 12009;
const int ID_LOO_SEL_ORDER_COMBOBOX = 12010;
const int ID_DO_LOO_CHECKBOX = 12011;

BEGIN_EVENT_TABLE(BMARefinerOptionsDialog, wxDialog)
    EVT_BUTTON(-1,  BMARefinerOptionsDialog::OnButton)
    EVT_CHECKBOX(-1, BMARefinerOptionsDialog::OnCheck)
    EVT_COMBOBOX(ID_LOO_SEL_ORDER_COMBOBOX, BMARefinerOptionsDialog::OnLooSelOrder)
    EVT_COMBOBOX(ID_BE_COMBOBOX, BMARefinerOptionsDialog::OnCombo)
    EVT_CLOSE (     BMARefinerOptionsDialog::OnCloseWindow)
END_EVENT_TABLE()


BMARefinerOptionsDialog::BMARefinerOptionsDialog(wxWindow* parent,
    const GeneralRefinerParams& current_genl,
    const LeaveOneOutParams& current_loo,
    const BlockEditingParams& current_be,
    const vector < string >& titles) :
        wxDialog(parent, -1, "Alignment Refiner Options", wxPoint(100,100), wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
        rowTitles(titles)
{
    wxBoxSizer *hSizer;
    rowsToExclude = current_loo.rowsToExclude;

//    wxScrolledWindow *panel = new wxScrolledWindow(this, -1, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    wxPanel *panel = new wxPanel(this, -1);

    //  Dialog heading
//    wxBoxSizer *item1 = new wxBoxSizer( wxHORIZONTAL );
//    wxStaticText *item2 = new wxStaticText( panel, ID_TEXT, wxT("Alignment Refiner Options"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
//    item2->SetFont( wxFont( 16, wxROMAN, wxNORMAL, wxNORMAL ) );
//    item1->Add( item2, 0, wxALIGN_CENTER|wxALL, 15 );

    //  **************************************************  //
    //  Start GUI elements for general parameters

    //  Cycles/trials
    wxStaticBox *item60 = new wxStaticBox( panel, -1, wxT("General Refiner Parameters") );
    item60->SetFont( wxFont( 10, wxROMAN, wxNORMAL, wxBOLD ) );
    wxStaticBoxSizer *item64 = new wxStaticBoxSizer( item60, wxVERTICAL );

    wxFlexGridSizer *item61 = new wxFlexGridSizer( 2, 0, 0 );
    item61->AddGrowableCol( 1 );

    //  # of cycles/trial
    wxStaticText *item63 = new wxStaticText( panel, ID_TEXT, wxT("Number of refinement cycles:                    "), wxDefaultPosition, wxDefaultSize, 0 );
    item61->Add( item63, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    nCyclesSpin = new IntegerSpinCtrl(panel,
        1, 10, 1, current_genl.nCycles,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    hSizer->Add(nCyclesSpin->GetTextCtrl(), 0, wxALIGN_RIGHT|wxLEFT|wxTOP|wxBOTTOM, 5);
    hSizer->Add(nCyclesSpin->GetSpinButton(), 0, wxALIGN_LEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);
    item61->Add(hSizer, 0, wxALIGN_RIGHT);

    //  Order of phases (LOO->BE or BE->LOO)
    int initialPOIndex = (current_genl.lnoFirst) ? 0 : 1;
    wxStaticText *item65 = new wxStaticText( panel, ID_TEXT, wxT("Phase order:"), wxDefaultPosition, wxDefaultSize, 0 );
    item61->Add( item65, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    phaseOrderCombo = new wxComboBox( panel, ID_PHASE_ORDER_COMBOBOX, phaseOrderStrings[initialPOIndex], wxDefaultPosition, wxDefaultSize, sizeof(phaseOrderStrings)/sizeof(phaseOrderStrings[0]), phaseOrderStrings, wxCB_READONLY );
    item61->Add( phaseOrderCombo, 0, wxALIGN_RIGHT|wxALL, 5 );

    item64->Add( item61, 0, wxALIGN_CENTER_VERTICAL, 0 );


    //  **************************************************  //
    //  Start GUI elements for LOO/LNO block shifting parameters
    wxStaticBox *item4 = new wxStaticBox( panel, -1, wxT("Block Shifting Parameters") );
    item4->SetFont( wxFont( 10, wxROMAN, wxNORMAL, wxBOLD ) );
    wxStaticBoxSizer *item3 = new wxStaticBoxSizer( item4, wxVERTICAL );

    wxFlexGridSizer *item5 = new wxFlexGridSizer( 2, 0, 0 );
    item5->AddGrowableCol( 1 );

    //  Do LOO/LNO???
    wxStaticText *item66 = new wxStaticText( panel, ID_TEXT, wxT("Shift blocks?"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item66, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    doLooCheck = new wxCheckBox( panel, ID_DO_LOO_CHECKBOX, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    doLooCheck->SetValue(current_loo.doLOO);
    item5->Add( doLooCheck, 0, wxALIGN_RIGHT|wxALL, 5 );

    //  LNO parameter (1 == L00; >1 = group size for running LNO)
    wxStaticText *item12 = new wxStaticText( panel, ID_TEXT, wxT("Group rows in sets of:"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item5->Add( item12, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    lnoSpin = new IntegerSpinCtrl(panel,
        1, 50, 1, current_loo.lno,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    hSizer->Add(lnoSpin->GetTextCtrl(), 0, wxALIGN_RIGHT|wxLEFT|wxTOP|wxBOTTOM, 5);
    hSizer->Add(lnoSpin->GetSpinButton(), 0, wxALIGN_LEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);
    item5->Add(hSizer, 0, wxALIGN_RIGHT);

    //  Blank space
    wxStaticText *item24 = new wxStaticText( panel, ID_TEXT, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item24, 0, wxALIGN_CENTER, 5 );
    wxStaticText *item25 = new wxStaticText( panel, ID_TEXT, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item25, 0, wxALIGN_CENTER, 5 );

    //  perform LOO/LNO on structures?
    wxStaticText *item6 = new wxStaticText( panel, ID_TEXT, wxT("Refine rows with structure:"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item6, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
//    wxStaticText *item7 = new wxStaticText( panel, ID_TEXT, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
//    item5->Add( item7, 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5 );
    fixStructCheck = new wxCheckBox( panel, ID_FIX_STRUCT_CHECKBOX, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );

    unsigned int nStructs = GetNumEmptyRowTitles(false);
    bool hasNoStructures = (nStructs == 0);
    bool hasOnlyStructures = (nStructs == rowTitles.size() - 1);
    if (hasNoStructures)
        fixStructCheck->SetValue(false);
    else if (hasOnlyStructures)
        fixStructCheck->SetValue(true);
    else
        fixStructCheck->SetValue(!current_loo.fixStructures);

    item5->Add( fixStructCheck, 0, wxALIGN_RIGHT|wxALL, 5 );

    // align all non-structured rows?
    wxStaticText *item90 = new wxStaticText( panel, ID_TEXT, wxT("Refine all unstructured rows:"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item90, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
//    wxStaticText *item100 = new wxStaticText( panel, ID_TEXT, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
//    item5->Add( item100, 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5 );
    allUnstSeqCheck = new wxCheckBox( panel, ID_ALL_UNST_SEQ_CHECKBOX, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    allUnstSeqCheck->SetValue(!hasOnlyStructures);
    item5->Add( allUnstSeqCheck, 0, wxALIGN_RIGHT|wxALL, 5 );

    //  Order of row selection in LOO/LNO
    int initialSelOrderIndex = (int) current_loo.selectorCode;
//    wxStaticText *item68 = new wxStaticText( panel, ID_TEXT, wxT("Row selection order:"), wxDefaultPosition, wxDefaultSize, 0 );
//    item5->Add( item68, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    looSelectionOrderCombo = new wxComboBox( panel, ID_LOO_SEL_ORDER_COMBOBOX, looSelectionOrderStrings[initialSelOrderIndex], wxDefaultPosition, wxDefaultSize, sizeof(looSelectionOrderStrings)/sizeof(looSelectionOrderStrings[0]), looSelectionOrderStrings, wxCB_READONLY );
    looSelectionOrderCombo->Show(false);
//    item5->Add( looSelectionOrderCombo, 0, wxALIGN_RIGHT|wxALL, 5 );

    //  # of independent refinement trials
//    wxStaticText *item62 = new wxStaticText( panel, ID_TEXT, wxT("Number of refinement trials:"), wxDefaultPosition, wxDefaultSize, 0 );
//    item5->Add( item62, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    nTrialsSpin = new IntegerSpinCtrl(panel,
        1, 10, 1, current_genl.nTrials,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    hSizer->Add(nTrialsSpin->GetTextCtrl(), 0, wxALIGN_RIGHT|wxLEFT|wxTOP|wxBOTTOM, 5);
    hSizer->Add(nTrialsSpin->GetSpinButton(), 0, wxALIGN_LEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);
    nTrialsSpin->GetTextCtrl()->Show(false);
    nTrialsSpin->GetSpinButton()->Show(false);
//    item5->Add(hSizer, 0, wxALIGN_RIGHT);

    //  use entire sequence length, or restricted to aligned footprint
    wxStaticText *item9 = new wxStaticText( panel, ID_TEXT, wxT("Refine using full sequence:"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item9, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
//    wxStaticText *item10 = new wxStaticText( panel, ID_TEXT, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
//    item5->Add( item10, 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5 );
    fullSeqCheck = new wxCheckBox( panel, ID_FULL_SEQ_CHECKBOX, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    fullSeqCheck->SetValue(current_loo.fullSequence);
    item5->Add( fullSeqCheck, 0, wxALIGN_RIGHT|wxALL, 5 );

    //  Allow extension/contraction of footprint (N-terminus)
    wxStaticText *item30 = new wxStaticText( panel, ID_TEXT, wxT("N-terminal footprint extension:"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item30, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    nExtSpin = new IntegerSpinCtrl(panel,
        -1000, 1000, 5, current_loo.nExt,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    hSizer->Add(nExtSpin->GetTextCtrl(), 0, wxALIGN_RIGHT|wxLEFT|wxTOP|wxBOTTOM, 5);
    hSizer->Add(nExtSpin->GetSpinButton(), 0, wxALIGN_LEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);
    item5->Add(hSizer, 0, wxALIGN_RIGHT);

    //  Allow extension/contraction of footprint (C-terminus)
    wxStaticText *item33 = new wxStaticText( panel, ID_TEXT, wxT("C-terminal footprint extension:"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item33, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    cExtSpin = new IntegerSpinCtrl(panel,
        -1000, 1000, 5, current_loo.cExt,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    hSizer->Add(cExtSpin->GetTextCtrl(), 0, wxALIGN_RIGHT|wxLEFT|wxTOP|wxBOTTOM, 5);
    hSizer->Add(cExtSpin->GetSpinButton(), 0, wxALIGN_LEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);
    item5->Add(hSizer, 0, wxALIGN_RIGHT);

    //  Blank space
    wxStaticText *item124 = new wxStaticText( panel, ID_TEXT, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item124, 0, wxALIGN_CENTER, 5 );
    wxStaticText *item125 = new wxStaticText( panel, ID_TEXT, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item125, 0, wxALIGN_CENTER, 5 );
//    wxStaticText *item26 = new wxStaticText( panel, ID_TEXT, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
//    item5->Add( item26, 0, wxALIGN_CENTER, 5 );

    //  Block aligner's loop percentile parameter
    wxStaticText *item15 = new wxStaticText( panel, ID_TEXT, wxT("Loop percentile:"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item5->Add( item15, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    loopPercentSpin = new FloatingPointSpinCtrl(panel,
        0.0, 100.0, 0.1, current_loo.percentile,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    hSizer->Add(loopPercentSpin->GetTextCtrl(), 0, wxALIGN_RIGHT|wxLEFT|wxTOP|wxBOTTOM, 5);
    hSizer->Add(loopPercentSpin->GetSpinButton(), 0, wxALIGN_LEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);
    item5->Add(hSizer, 0, wxALIGN_RIGHT);

    //  Block aligner's loop extension parameter
    wxStaticText *item18 = new wxStaticText( panel, ID_TEXT, wxT("Loop extension:"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item5->Add( item18, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    loopExtensionSpin = new IntegerSpinCtrl(panel,
        0, 10000, 5, current_loo.extension,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    hSizer->Add(loopExtensionSpin->GetTextCtrl(), 0, wxALIGN_RIGHT|wxLEFT|wxTOP|wxBOTTOM, 5);
    hSizer->Add(loopExtensionSpin->GetSpinButton(), 0, wxALIGN_LEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);
    item5->Add(hSizer, 0, wxALIGN_RIGHT);

    //  Block aligner's loop cutoff parameter
    wxStaticText *item21 = new wxStaticText( panel, ID_TEXT, wxT("Loop cutoff (0=none):"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item5->Add( item21, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    loopCutoffSpin = new IntegerSpinCtrl(panel,
        0, 10000, 5, current_loo.cutoff,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    hSizer->Add(loopCutoffSpin->GetTextCtrl(), 0, wxALIGN_RIGHT|wxLEFT|wxTOP|wxBOTTOM, 5);
    hSizer->Add(loopCutoffSpin->GetSpinButton(), 0, wxALIGN_LEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);
    item5->Add(hSizer, 0, wxALIGN_RIGHT);

    //  Seed for RNG to define order of leaving out rows (useful for debugging!!)
//    wxStaticText *item27 = new wxStaticText( panel, ID_TEXT, wxT("Random number seed (0=no preference):"), wxDefaultPosition, wxDefaultSize, 0 );
//    item5->Add( item27, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    rngSpin = new IntegerSpinCtrl(panel,
        0, 1000000000, 3727851, current_loo.seed,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    hSizer->Add(rngSpin->GetTextCtrl(), 0, wxALIGN_RIGHT|wxLEFT|wxTOP|wxBOTTOM, 5);
    hSizer->Add(rngSpin->GetSpinButton(), 0, wxALIGN_LEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);
    rngSpin->GetTextCtrl()->Show(false);
    rngSpin->GetSpinButton()->Show(false);
//    item5->Add(hSizer, 0, wxALIGN_RIGHT);

    item3->Add( item5, 0, wxALIGN_CENTER_VERTICAL, 0 );


    //  **************************************************  //
    //  Start GUI elements for block extension parameters.
    wxStaticBox *item37 = new wxStaticBox( panel, -1, wxT("Block Modification Parameters") );
    item37->SetFont( wxFont( 10, wxROMAN, wxNORMAL, wxBOLD ) );
    wxStaticBoxSizer *item36 = new wxStaticBoxSizer( item37, wxVERTICAL );

    wxFlexGridSizer *item38 = new wxFlexGridSizer( 2, 0, 0 );
    item38->AddGrowableCol( 1 );

    //  Allow block extension?
    int initialValue = (current_be.algMethod >= 0 && current_be.algMethod < eGreedyExtend) ? (int) current_be.algMethod : 0;
    wxStaticText *item39 = new wxStaticText( panel, ID_TEXT, wxT("Change block model?"), wxDefaultPosition, wxDefaultSize, 0 );
    item38->Add( item39, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    esCombo= new wxComboBox( panel, ID_BE_COMBOBOX, wxT(""), wxDefaultPosition, wxDefaultSize, sizeof(blockEditAlgStrings)/sizeof(blockEditAlgStrings[0]), blockEditAlgStrings, wxCB_READONLY );
    esCombo->SetValue(blockEditAlgStrings[initialValue]);
    item38->Add( esCombo, 0, wxALIGN_RIGHT|wxALL, 5 );

    //  Expand first?
    wxStaticText *item39b = new wxStaticText( panel, ID_TEXT, wxT("Expand first?"), wxDefaultPosition, wxDefaultSize, 0 );
    item38->Add( item39b, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    extendFirstCheck = new wxCheckBox( panel, ID_BEXTEND_FIRST_CHECKBOX, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    extendFirstCheck->SetValue(current_be.extendFirst);
    item38->Add( extendFirstCheck, 0, wxALIGN_RIGHT|wxALL, 5 );

    //  Define a minimum block size (really only relevant for shrinking blocks...
    wxStaticText *item42 = new wxStaticText( panel, ID_TEXT, wxT("Minimum block size:"), wxDefaultPosition, wxDefaultSize, 0 );
    item38->Add( item42, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    minBlockSizeSpin = new IntegerSpinCtrl(panel,
        1, 100, 1, current_be.minBlockSize,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    hSizer->Add(minBlockSizeSpin->GetTextCtrl(), 0, wxALIGN_RIGHT|wxLEFT|wxTOP|wxBOTTOM, 5);
    hSizer->Add(minBlockSizeSpin->GetSpinButton(), 0, wxALIGN_LEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);
    item38->Add(hSizer, 0, wxALIGN_RIGHT);

    //  Block extension/shrinkage threshold 1:  column of PSSM median score >= this value
    wxStaticText *item45 = new wxStaticText( panel, ID_TEXT, wxT("Median PSSM score threshold:"), wxDefaultPosition, wxDefaultSize, 0 );
    item38->Add( item45, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    medianSpin = new IntegerSpinCtrl(panel,
        -20, 20, 1, current_be.median,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    hSizer->Add(medianSpin->GetTextCtrl(), 0, wxALIGN_RIGHT|wxLEFT|wxTOP|wxBOTTOM, 5);
    hSizer->Add(medianSpin->GetSpinButton(), 0, wxALIGN_LEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);
    item38->Add(hSizer, 0, wxALIGN_RIGHT);

    //  Block extension/shrinkage threshold 2:  % rows w/ PSSM score >= 0 must exceed this value
    wxStaticText *item48 = new wxStaticText( panel, ID_TEXT, wxT("Voting percentage (% of rows vote to extend):"), wxDefaultPosition, wxDefaultSize, 0 );
    item38->Add( item48, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    rawVoteSpin = new FloatingPointSpinCtrl(panel,
        0.0, 100.0, 1.0, 100.0*(1.0 - current_be.negRowsFraction),
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    hSizer->Add(rawVoteSpin->GetTextCtrl(), 0, wxALIGN_RIGHT|wxLEFT|wxTOP|wxBOTTOM, 5);
    hSizer->Add(rawVoteSpin->GetSpinButton(), 0, wxALIGN_LEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);
    item38->Add(hSizer, 0, wxALIGN_RIGHT);

    //  Block extension/shrinkage threshold 3:  % of weight in column of PSSM median score >= 0 must exceed this value
    wxStaticText *item51 = new wxStaticText( panel, ID_TEXT, wxT("Weighted (by PSSM score) voting percentage:"), wxDefaultPosition, wxDefaultSize, 0 );
    item38->Add( item51, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    hSizer = new wxBoxSizer(wxHORIZONTAL);
    weightedVoteSpin = new FloatingPointSpinCtrl(panel,
        0.0, 100.0, 1.0, 100.0*(1.0 - current_be.negScoreFraction),
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    hSizer->Add(weightedVoteSpin->GetTextCtrl(), 0, wxALIGN_RIGHT|wxLEFT|wxTOP|wxBOTTOM, 5);
    hSizer->Add(weightedVoteSpin->GetSpinButton(), 0, wxALIGN_LEFT|wxRIGHT|wxTOP|wxBOTTOM, 5);
    item38->Add(hSizer, 0, wxALIGN_RIGHT);

    item36->Add( item38, 0, wxALIGN_CENTER|wxALL, 5 );

    //  Enable/disable block-editing controls based on state of 'extendCheck' & 'shrinkCheck'.
    wxCommandEvent dummyEvent;
    dummyEvent.SetId(ID_BE_COMBOBOX);
    OnCombo(dummyEvent);

    //  Enable/disable loo controls based on state of shift blocks checkbox
    dummyEvent.SetId(ID_DO_LOO_CHECKBOX);
    OnCheck(dummyEvent);

    //  Enable/disable specific controls based on selected row selection order
    dummyEvent.SetId(ID_LOO_SEL_ORDER_COMBOBOX);
    OnLooSelOrder(dummyEvent);

    //  OK/Cancel buttons
    wxBoxSizer *item54 = new wxBoxSizer( wxHORIZONTAL );
    wxButton *item55 = new wxButton( panel, wxID_OK, wxT("OK"), wxDefaultPosition, wxDefaultSize, 0 );
    item55->SetDefault();
    item54->Add( item55, 0, wxALIGN_CENTER|wxALL, 5 );
    wxButton *item56 = new wxButton( panel, wxID_CANCEL, wxT("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    item54->Add( item56, 0, wxALIGN_CENTER|wxALL, 5 );

    wxBoxSizer *item103 = new wxBoxSizer( wxHORIZONTAL );

    //  Place sub-sizers in outermost sizer.
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );
    item0->Add( item64, 0, wxALIGN_CENTER|wxALL, 5 );
//    item0->Add( item3, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
//    item0->Add( item36, 0, wxALIGN_CENTER|wxALL, 5 );
    item103->Add( item3, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    item103->Add( item36, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_TOP |wxALL, 5 );
    item0->Add(item103, 0, wxALIGN_CENTER|wxALL, 2);
    item0->Add( item54, 0, wxALIGN_CENTER|wxALL, 5 );

    panel->SetAutoLayout(true);
    panel->SetSizer(item0);
    item0->Fit(this);
    item0->Fit(panel);
    item0->SetSizeHints(this);

//    return item0;
}


BMARefinerOptionsDialog::~BMARefinerOptionsDialog(void)
{
    delete nCyclesSpin;
    delete nTrialsSpin;

    delete lnoSpin;
    delete loopPercentSpin;
    delete loopExtensionSpin;
    delete loopCutoffSpin;
    delete rngSpin;
    delete nExtSpin;
    delete cExtSpin;

    delete minBlockSizeSpin;
    delete medianSpin;
    delete rawVoteSpin;
    delete weightedVoteSpin;
}

unsigned int BMARefinerOptionsDialog::GetNCycles() {
    int c;
    if (nCyclesSpin->GetInteger(&c) && c >= 1)
        return (unsigned int) c;
    else {
        nCyclesSpin->SetInteger(1);
        return 1;
    }
}

unsigned int BMARefinerOptionsDialog::GetNTrials() {
    int t;
    if (nTrialsSpin->GetInteger(&t) && t >= 1)
        return (unsigned int) t;
    else {
        nTrialsSpin->SetInteger(1);
        return 1;
    }
}


bool BMARefinerOptionsDialog::GetParameters( GeneralRefinerParams* genl_params, LeaveOneOutParams* loo_params, BlockEditingParams* be_params)
{
    if (!genl_params || !loo_params || !be_params) return false;
    bool result = true;
    int nt  = (int) genl_params->nTrials;
    int nc  = (int) genl_params->nCycles;
    int mbs = (int) be_params->minBlockSize;
    int lno = (int) loo_params->lno;
    int sd = (int) loo_params->seed;
    int lext = (int) loo_params->extension;
    int cut = (int) loo_params->cutoff;

    //  Get general parameters
    result &= nTrialsSpin->GetInteger(&nt);
    result &= nCyclesSpin->GetInteger(&nc);
    genl_params->nTrials = (unsigned int) nt;
    genl_params->nCycles = (unsigned int) nc;

    wxString phaseOrderStr = phaseOrderCombo->GetValue();
    genl_params->lnoFirst = (phaseOrderStr == phaseOrderStrings[0]);


    //  Get LOO parameters
    loo_params->doLOO = doLooCheck->IsChecked();
    loo_params->fixStructures = !fixStructCheck->IsChecked();
    if (allUnstSeqCheck->GetValue())
        loo_params->rowsToExclude.clear();
    else
        loo_params->rowsToExclude = rowsToExclude;
    loo_params->fullSequence  = fullSeqCheck->IsChecked();

    wxString selOrderStr = looSelectionOrderCombo->GetValue();
    if (selOrderStr == looSelectionOrderStrings[0]) {
        loo_params->selectorCode = eRandomSelectionOrder;
    } else if (selOrderStr == looSelectionOrderStrings[2]) {
        loo_params->selectorCode = eBestScoreFirst;
    } else {  //  worst-first is the Cn3D default
        loo_params->selectorCode = eWorstScoreFirst;
    }


    result &= loopPercentSpin->GetDouble(&(loo_params->percentile));
    result &= lnoSpin->GetInteger(&lno);
    result &= loopExtensionSpin->GetInteger(&lext);
    result &= loopCutoffSpin->GetInteger(&cut);
    result &= rngSpin->GetInteger(&sd);
    result &= nExtSpin->GetInteger(&(loo_params->nExt));
    result &= cExtSpin->GetInteger(&(loo_params->cExt));
    loo_params->lno = (unsigned int) lno;
    loo_params->seed = (unsigned int) sd;
    loo_params->extension = (unsigned int) lext;
    loo_params->cutoff = (unsigned int) cut;

    //  Get Block editing parameters
    wxString comboValue = esCombo->GetValue();
    bool doExtend = (comboValue == blockEditAlgStrings[(int)eSimpleExtend] || comboValue == blockEditAlgStrings[(int)eSimpleExtendAndShrink]);
    bool doShrink = (comboValue == blockEditAlgStrings[(int)eSimpleShrink] || comboValue == blockEditAlgStrings[(int)eSimpleExtendAndShrink]);
    be_params->editBlocks  = doExtend || doShrink;
    be_params->canShrink   = doShrink;
    be_params->extendFirst = extendFirstCheck->IsChecked();
    if (be_params->editBlocks) {
        be_params->algMethod = ((doExtend) ? (doShrink ? eSimpleExtendAndShrink : eSimpleExtend) : eSimpleShrink);
    } else {
        be_params->algMethod = eInvalidBBAMethod;
    }

    result &= minBlockSizeSpin->GetInteger(&mbs);
    be_params->minBlockSize = (unsigned int) mbs;

    //  Compound scorer parameters
    result &= medianSpin->GetInteger(&(be_params->median));
    result &= rawVoteSpin->GetDouble(&(be_params->negRowsFraction));
    result &= weightedVoteSpin->GetDouble(&(be_params->negScoreFraction));
    be_params->negRowsFraction = 1.0 - be_params->negRowsFraction/100.0;
    be_params->negScoreFraction = 1.0 - be_params->negScoreFraction/100.0;

    //  Something should be editable!!
    if (!be_params->editBlocks && !loo_params->doLOO)
        result = false;

    //  Don't allow refinement if have all structures and not refining structures.
    if (GetNumEmptyRowTitles(false) == rowTitles.size() - 1 && loo_params->fixStructures) {
        ERRORMSG("Alignment has only structured rows but 'Refine rows with structure' is unchecked.");
        result = false;
    }

    return result;
}


unsigned int BMARefinerOptionsDialog::GetNumEmptyRowTitles(bool countMasterRowTitle) const
{
    unsigned int start = (countMasterRowTitle) ? 0 : 1;
    unsigned int result = 0, n = rowTitles.size();
    for (unsigned int i = start; i < n; ++i) {
        if (rowTitles[i].length() == 0) ++result;
    }
    return result;
}

void BMARefinerOptionsDialog::OnCloseWindow(wxCloseEvent& event)
{
    EndModal(wxCANCEL);
}

void BMARefinerOptionsDialog::OnButton(wxCommandEvent& event)
{
    if (event.GetId() == wxID_OK) {
        GeneralRefinerParams genl;
        LeaveOneOutParams loo;
        BlockEditingParams be;
        if (GetParameters(&genl, &loo, &be))  // can't successfully quit if values aren't valid
            EndModal(wxOK);
        else
            wxBell();
    } else if (event.GetId() == wxID_CANCEL) {
        EndModal(wxCANCEL);
    } else {
        event.Skip();
    }
}

template < class C >
inline bool VectorContains(const vector < C >& v, const C& c)
{
    for (unsigned int i=0; i<v.size(); ++i)
        if (v[i] == c)
            return true;
    return false;
}

void BMARefinerOptionsDialog::OnCheck(wxCommandEvent& event)
{
    if (event.GetId() == ID_ALL_UNST_SEQ_CHECKBOX) {
        if (!allUnstSeqCheck->GetValue()) { // if checkbox is turned off, then we need to select rows
            map < unsigned int, unsigned int > choice2row;
            wxArrayString choices;
            wxArrayInt selected;
            for (unsigned int i=0; i<rowTitles.size(); ++i) {
                if (rowTitles[i].size() > 0) {  // only select on non-structured rows (assuming titles are set accordingly)
                    choice2row[choices.GetCount()] = i;
                    if (!VectorContains(rowsToExclude, i))
                        selected.Add(choices.GetCount());
                    choices.Add(rowTitles[i].c_str());
                }
            }
            wxMultiChoiceDialog dialog(this, "Choose unstructured rows to align:", "Row Selection", choices,
                wxCAPTION | wxOK | wxCENTRE);
            dialog.SetSelections(selected);
            while (dialog.ShowModal() == wxID_OK) {
                rowsToExclude.clear();
                selected = dialog.GetSelections();
                // convert inclusion list -> exclusions
                vector < bool > exclude(rowTitles.size(), true);
                unsigned int i;
                for (i=0; i<selected.GetCount(); ++i)
                    exclude[choice2row.find(selected.Item(i))->second] = false;
                for (i=0; i<rowTitles.size(); ++i)
                    if (exclude[i] && rowTitles[i].size() > 0)
                        rowsToExclude.push_back(i);
                if (selected.size() == 0 && !fixStructCheck->IsChecked())
                    ERRORMSG("Must select at least one unstructured row to refine when not refining structured rows");
                else
                    break;
            }
        }
    } else if (event.GetId() == ID_DO_LOO_CHECKBOX) {
        //  toggle loo controls activity
        bool doLoo = doLooCheck->IsChecked();

        unsigned int nStructs = GetNumEmptyRowTitles(false);
        bool enableFixStructCheck = (nStructs > 0 && nStructs < rowTitles.size() - 1);  //  enable if there's a  non-master structure, but not if all are structures
        bool enableAllUnstSeqCheck = (nStructs < rowTitles.size() - 1);  //  enable if there's at least one non-structure

        looSelectionOrderCombo->Enable(doLoo);
        fullSeqCheck->Enable(doLoo);
        fixStructCheck->Enable(doLoo && enableFixStructCheck);
        allUnstSeqCheck->Enable(doLoo && enableAllUnstSeqCheck);

        nExtSpin->GetTextCtrl()->Enable(doLoo);
        nExtSpin->GetSpinButton()->Enable(doLoo);
        cExtSpin->GetTextCtrl()->Enable(doLoo);
        cExtSpin->GetSpinButton()->Enable(doLoo);
        lnoSpin->GetTextCtrl()->Enable(doLoo);
        lnoSpin->GetSpinButton()->Enable(doLoo);
        loopPercentSpin->GetTextCtrl()->Enable(doLoo);
        loopPercentSpin->GetSpinButton()->Enable(doLoo);
        loopExtensionSpin->GetTextCtrl()->Enable(doLoo);
        loopExtensionSpin->GetSpinButton()->Enable(doLoo);
        loopCutoffSpin->GetTextCtrl()->Enable(doLoo);
        loopCutoffSpin->GetSpinButton()->Enable(doLoo);
        rngSpin->GetTextCtrl()->Enable(doLoo);
        rngSpin->GetSpinButton()->Enable(doLoo);

        wxString comboValue = esCombo->GetValue();
        bool editBlocks = (comboValue != blockEditAlgStrings[0]);
        phaseOrderCombo->Enable(editBlocks && doLoo);

        Refresh();

    } else
        event.Skip();
}

void BMARefinerOptionsDialog::OnCombo(wxCommandEvent& event)
{
    wxString comboValue = esCombo->GetValue();
    bool doExtend = (comboValue == blockEditAlgStrings[(int)eSimpleExtend] || comboValue == blockEditAlgStrings[(int)eSimpleExtendAndShrink]);
    bool doShrink = (comboValue == blockEditAlgStrings[(int)eSimpleShrink] || comboValue == blockEditAlgStrings[(int)eSimpleExtendAndShrink]);
    bool doBlockEdit = (doExtend || doShrink);

    phaseOrderCombo->Enable(doBlockEdit && doLooCheck->IsChecked());
    extendFirstCheck->Enable(doShrink);

    minBlockSizeSpin->GetTextCtrl()->Enable(doBlockEdit);
    minBlockSizeSpin->GetSpinButton()->Enable(doBlockEdit);
    medianSpin->GetTextCtrl()->Enable(doBlockEdit);
    medianSpin->GetSpinButton()->Enable(doBlockEdit);
    rawVoteSpin->GetTextCtrl()->Enable(doBlockEdit);
    rawVoteSpin->GetSpinButton()->Enable(doBlockEdit);
    weightedVoteSpin->GetTextCtrl()->Enable(doBlockEdit);
    weightedVoteSpin->GetSpinButton()->Enable(doBlockEdit);
    Refresh();
}

void BMARefinerOptionsDialog::OnLooSelOrder(wxCommandEvent& event)
{
    wxString selOrderStr = looSelectionOrderCombo->GetValue();
    bool isRandom = (selOrderStr == looSelectionOrderStrings[0]);

    rngSpin->GetTextCtrl()->Enable(isRandom);
    rngSpin->GetSpinButton()->Enable(isRandom);

    //  Don't do multiple trials if the selection order is deterministic.
    nTrialsSpin->GetTextCtrl()->Enable(isRandom);
    nTrialsSpin->GetSpinButton()->Enable(isRandom);
    if (!isRandom) {
        nTrialsSpin->SetInteger(1);
    }
    Refresh();
}

END_SCOPE(align_refine)

