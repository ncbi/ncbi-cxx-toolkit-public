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
* Authors:  Chris Lanczycki
*
* File Description:
*      Interface to blocked multiple alignment refiner algorithm
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

#include <algo/structure/struct_util/struct_util.hpp>
#include <algo/structure/bma_refine/RefinerEngine.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>

#include "cn3d_refiner_interface.hpp"
#include "wx_tools.hpp"
#include "cn3d_tools.hpp"
#include "progress_meter.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

//  These listed in order of enum BlockBoundaryAlgorithmMethod
static const wxString blockEditAlgStrings[] = {"No", "Only expand", "Only shrink", "Expand and shrink"};

class BMARefinerOptionsDialog : public wxDialog
{

public:

    struct GeneralRefinerParams {
        unsigned int nTrials;   //  number of independent refinement trials
        unsigned int nCycles;   //  number of cycles per trial (cycle = LOO + block boundary editing)
        bool verbose;
        double trialConvThold;  //  trial convergence threshold:  % change < this value

        GeneralRefinerParams() {
            nTrials = 1;
            nCycles = 1;
            verbose = false;
            trialConvThold = 0.001;
        }
        GeneralRefinerParams(const GeneralRefinerParams& rhs) {
            Copy(rhs);
        }

        GeneralRefinerParams& operator=(const GeneralRefinerParams& rhs) {
            if (this != &rhs) {
                Copy(rhs);
            }
            return *this;
        }

        void Copy(const GeneralRefinerParams& rhs) {
            nTrials = rhs.nTrials;
            nCycles = rhs.nCycles;
            verbose = rhs.verbose;
            trialConvThold = rhs.trialConvThold;
        }
    };


    BMARefinerOptionsDialog(wxWindow* parent, const GeneralRefinerParams& current_genl, const align_refine::LeaveOneOutParams& current_loo, const align_refine::BlockEditingParams& current_be);
    ~BMARefinerOptionsDialog(void);

    bool GetParameters(GeneralRefinerParams* genl_params, align_refine::LeaveOneOutParams* loo_params, align_refine::BlockEditingParams* be_params);
    unsigned int GetNCycles();  //  out of range -> returns 1
    unsigned int GetNTrials();  //  out of range -> returns 1

private:

    IntegerSpinCtrl *nCyclesSpin, *nTrialsSpin;
    IntegerSpinCtrl *lnoSpin, *loopExtensionSpin, *loopCutoffSpin, *rngSpin, *nExtSpin, *cExtSpin;
    IntegerSpinCtrl *minBlockSizeSpin, *medianSpin;
    FloatingPointSpinCtrl *loopPercentSpin, *rawVoteSpin, *weightedVoteSpin ;
    wxCheckBox *fixStructCheck, *fullSeqCheck, *extendFirstCheck;
    wxComboBox *esCombo;

    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);
    void OnCombo(wxCommandEvent& event);
    DECLARE_EVENT_TABLE()
};


//
//  BMARefiner class declarations
//

BMARefiner::BMARefiner(void)
{
    m_refinerEngine = NULL;
}

BMARefiner::~BMARefiner(void)
{
    delete m_refinerEngine;
}


bool BMARefiner::RefineMultipleAlignment(AlignmentUtility *originalMultiple,
    AlignmentUtilityList *refinedMultiples, wxWindow *parent)
{
    if (!originalMultiple) return false;

    unsigned int nRows;
    unsigned int nAlignedBlocks = originalMultiple->GetBlockMultipleAlignment()->NAlignedBlocks();
    if (nAlignedBlocks == 0) {
        ERRORMSG("Must have at least one aligned block to use the block aligner");
        return false;
    }

    //  Empty results list of any prior alignments.
    if (refinedMultiples) {
        for (AlignmentUtilityListIt it = refinedMultiples->begin();
                                    it != refinedMultiples->end(); ++it) {
            delete *it;
        }
        refinedMultiples->clear();
    }

    string warnMsg;
    set<unsigned int> sanityCheckFailures;
    set<unsigned int>::iterator sit, sitEnd;
    unsigned int j, nToRefine = m_blocksToRefine.size(), nToExclude = m_rowsToExclude.size();

    // Make sure exclude rows are valid row IDs.
    if (nToExclude > 0) {
        nRows = originalMultiple->GetBlockMultipleAlignment()->NRows();
        for (j = 0; j < nToExclude; ++j) {
            if (m_rowsToExclude[j] >= nRows) {
                sanityCheckFailures.insert(m_rowsToExclude[j]);
            }
        }
        
        sitEnd = sanityCheckFailures.end();
        for (sit = sanityCheckFailures.begin(); sit != sitEnd; ++sit) {
            warnMsg.append("   Request to exclude invalid row number " + NStr::UIntToString(*sit + 1) + " ignored./n");
            m_rowsToExclude.erase(remove(m_rowsToExclude.begin(), m_rowsToExclude.end(), *sit));
        }
    }

    // Make sure all blocks are valid.
    if (nToRefine > 0) {

        sanityCheckFailures.clear();
        for (j = 0; j < nToRefine; ++j) {
            if (m_blocksToRefine[j] >= nAlignedBlocks) {
                sanityCheckFailures.insert(m_blocksToRefine[j]);
            }
        }
        sitEnd = sanityCheckFailures.end();
        for (sit = sanityCheckFailures.begin(); sit != sitEnd; ++sit) {
            warnMsg.append("   Request to exclude invalid block number " + NStr::UIntToString(*sit + 1) + " ignored./n");
            m_blocksToRefine.erase(remove(m_blocksToRefine.begin(), m_blocksToRefine.end(), *sit));
        }
        nToRefine = m_blocksToRefine.size();
    }

    // If no blocks explicitly set, refine 'em all.
    if (nToRefine == 0) {
        SetBlocksToRealign(nAlignedBlocks);
    }

    // Show options dialog each time block aligner is run; recreates & initializes the refiner engine.
    // Frozen blocks and excluded rows must have been defined by this point!!!
    if (!ConfigureRefiner(parent)) return true;

    if (!m_refinerEngine) {
        ERRORMSG("Error initializing alignment refinement engine");
        return false;
    }

    bool errorsEncountered = true;

    wxSetCursor(*wxHOURGLASS_CURSOR);
    ProgressMeter wait(NULL, "Refinement in progress...", "Wait", 100);
    wait.SetValue(50);

    //  Execute the refinement...
    SetDialogSevereErrors(false);
    align_refine::RefinerResultCode result = m_refinerEngine->Refine(originalMultiple);
    SetDialogSevereErrors(true);

    wait.SetValue(90);

    //  If requested, store results (in order of decreasing score so best alignment is first).
    if (result == align_refine::eRefinerResultOK) {
        if (refinedMultiples) {
            const align_refine::RefinedAlignments& refinedAlignments = m_refinerEngine->GetAllResults();
            align_refine::RefinedAlignmentsRevCIt rcit = refinedAlignments.rbegin(), rend = refinedAlignments.rend();
            for (; rcit != rend; ++rcit) {
                errorsEncountered &= (rcit->second.au == NULL);  // ends up true only if all are null
                refinedMultiples->push_back(rcit->second.au);
            }
        } else {
            errorsEncountered = false;  //  it's ok if we don't want the results now
        }
    }

    wait.Show(false);
    wxSetCursor(wxNullCursor);
    SetDiagPostLevel(eDiag_Info);   // may be changed by refiner

//    else {
//        WARNINGMSG("alignment refiner encountered a problem (code " << result << ") - current alignment unchanged");
//    }

    // no alignment or block aligner failed (caller should revert to using original alignment)
    if (errorsEncountered || result != align_refine::eRefinerResultOK)
        ERRORMSG("Alignment refiner encountered problem(s) (code " << result << ") - current alignment unchanged.\nSee message log for details");

    return !errorsEncountered;
}

void BMARefiner::SetBlocksToRealign(unsigned int nAlignedBlocks) {

    m_blocksToRefine.clear();
    for (unsigned int i = 0; i < nAlignedBlocks; ++i) {
        m_blocksToRefine.push_back(i);
    }
}

void BMARefiner::SetBlocksToRealign(const vector<unsigned int>& blocks, bool clearOldList) {

    if (clearOldList) m_blocksToRefine.clear();
    m_blocksToRefine.insert(m_blocksToRefine.begin(), blocks.begin(), blocks.end());
}

void BMARefiner::SetRowsToExcludeFromLNO(const vector<unsigned int>& excludedRows, bool clearOldList) {

    if (clearOldList) m_rowsToExclude.clear();
    m_rowsToExclude.insert(m_rowsToExclude.begin(), excludedRows.begin(), excludedRows.end());
}


bool BMARefiner::ConfigureRefiner(wxWindow* parent)
{
    BMARefinerOptionsDialog::GeneralRefinerParams genl;
    align_refine::LeaveOneOutParams loo;
    align_refine::BlockEditingParams be;

    //  By default, turn off block editing.  (Not changing in default BEP ctor in
    //  case something else assumes the default is true.)
    be.editBlocks = false;

    //  If a refiner exists, reuse it's parameters....
    if (m_refinerEngine) {
        genl.nCycles = m_refinerEngine->NumCycles();
        genl.nTrials = m_refinerEngine->NumTrials();
        m_refinerEngine->GetLOOParams(loo);
        m_refinerEngine->GetBEParams(be);
    }

    //  ... but update with the most recent info about blocks & rows to refine.
    //  Selected blocks are refined; others frozen.  
    //  Explicitly protected rows do not undergo LOO/LNO refinement phase.
    loo.blocks.clear();
    if (m_blocksToRefine.size() > 0) {
        loo.blocks.insert(loo.blocks.begin(), m_blocksToRefine.begin(), m_blocksToRefine.end());
    }

    loo.rowsToExclude.clear();
    if (m_rowsToExclude.size() > 0) {
        loo.rowsToExclude.insert(loo.rowsToExclude.begin(), m_rowsToExclude.begin(), m_rowsToExclude.end());
    }

    //  Use a fixed column scoring method for now.
    //  (the '3.3.3' method when using defaults for median, negRows & negScore, and not extension/shrinkageThresholds)
    be.columnMethod = align_refine::eCompoundScorer;
    be.columnScorerThreshold = 0;  //  this is the 'magic' PSSM score in the compound scorer

    BMARefinerOptionsDialog dialog(parent, genl, loo, be);
    bool ok = (dialog.ShowModal() == wxOK);

    //  Clear out any previous refiner run.
    delete m_refinerEngine;
    m_refinerEngine = NULL;

    if (ok && !dialog.GetParameters(&genl, &loo, &be))
        ERRORMSG("Error getting refiner options from dialog!");
    else if (ok) {
        m_refinerEngine = new align_refine::CBMARefinerEngine(loo, be, genl.nCycles, genl.nTrials, genl.verbose, genl.trialConvThold);
    }
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

BEGIN_EVENT_TABLE(BMARefinerOptionsDialog, wxDialog)
    EVT_BUTTON(-1,  BMARefinerOptionsDialog::OnButton)
//    EVT_CHECKBOX(ID_BEXTEND_CHECKBOX, BMARefinerOptionsDialog::OnCheck)
    EVT_COMBOBOX(ID_BE_COMBOBOX, BMARefinerOptionsDialog::OnCombo)
    EVT_CLOSE (     BMARefinerOptionsDialog::OnCloseWindow)
END_EVENT_TABLE()


BMARefinerOptionsDialog::BMARefinerOptionsDialog(
    wxWindow* parent,  const GeneralRefinerParams& current_genl, const align_refine::LeaveOneOutParams& current_loo, const align_refine::BlockEditingParams& current_be) :
        wxDialog(parent, -1, "Set Alignment Refiner Options", wxPoint(100,100), wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{

    wxPanel *panel = new wxPanel(this, -1);

    //  Dialog heading
    wxBoxSizer *item1 = new wxBoxSizer( wxHORIZONTAL );
    wxStaticText *item2 = new wxStaticText( panel, ID_TEXT, wxT("Alignment Refiner Options"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item2->SetFont( wxFont( 16, wxROMAN, wxNORMAL, wxNORMAL ) );
    item1->Add( item2, 0, wxALIGN_CENTER|wxALL, 15 );

    //  Cycles/trials
    wxStaticBox *item60 = new wxStaticBox( panel, -1, wxT("General Refiner Parameters") );
    item60->SetFont( wxFont( 10, wxROMAN, wxNORMAL, wxBOLD ) );
    wxStaticBoxSizer *item64 = new wxStaticBoxSizer( item60, wxVERTICAL );

    wxFlexGridSizer *item61 = new wxFlexGridSizer( 3, 0, 0 );
    item61->AddGrowableCol( 1 );

    //  # of cycles/trial
    wxStaticText *item63 = new wxStaticText( panel, ID_TEXT, wxT("Number of refinement cycles/trial:"), wxDefaultPosition, wxDefaultSize, 0 );
    item61->Add( item63, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    nCyclesSpin = new IntegerSpinCtrl(panel,
        1, 10, 1, current_genl.nCycles,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item61->Add(nCyclesSpin->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item61->Add(nCyclesSpin->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    //  # of independent refinement trials (may want to leave this out)
    wxStaticText *item62 = new wxStaticText( panel, ID_TEXT, wxT("Number of refinement trials:"), wxDefaultPosition, wxDefaultSize, 0 );
    item61->Add( item62, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    nTrialsSpin = new IntegerSpinCtrl(panel,
        1, 20, 1, current_genl.nTrials,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item61->Add(nTrialsSpin->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item61->Add(nTrialsSpin->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    item64->Add( item61, 0, wxALIGN_CENTER_VERTICAL, 0 );

    //  Start GUI elements for LOO/LNO block shifting parameters
    wxStaticBox *item4 = new wxStaticBox( panel, -1, wxT("Block Shifting Parameters") );
    item4->SetFont( wxFont( 10, wxROMAN, wxNORMAL, wxBOLD ) );
    wxStaticBoxSizer *item3 = new wxStaticBoxSizer( item4, wxVERTICAL );

    wxFlexGridSizer *item5 = new wxFlexGridSizer( 3, 0, 0 );
    item5->AddGrowableCol( 1 );

    //  perform LOO/LNO on structures?
    wxStaticText *item6 = new wxStaticText( panel, ID_TEXT, wxT("Refine rows with structure:"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item6, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    wxStaticText *item7 = new wxStaticText( panel, ID_TEXT, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item7, 0, wxALIGN_CENTER|wxALL, 5 );
    fixStructCheck = new wxCheckBox( panel, ID_FIX_STRUCT_CHECKBOX, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    fixStructCheck->SetValue(!current_loo.fixStructures);
    item5->Add( fixStructCheck, 0, wxALIGN_CENTER|wxALL, 5 );

    //  use entire sequence length, or restricted to aligned footprint
    wxStaticText *item9 = new wxStaticText( panel, ID_TEXT, wxT("Refine using full sequence:"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item9, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    wxStaticText *item10 = new wxStaticText( panel, ID_TEXT, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item10, 0, wxALIGN_CENTER|wxALL, 5 );
    fullSeqCheck = new wxCheckBox( panel, ID_FULL_SEQ_CHECKBOX, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    fullSeqCheck->SetValue(current_loo.fullSequence);
    item5->Add( fullSeqCheck, 0, wxALIGN_CENTER|wxALL, 5 );

    //  LNO parameter (1 == L00; >1 = group size for running LNO)
    wxStaticText *item12 = new wxStaticText( panel, ID_TEXT, wxT("Group rows in sets of:"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item5->Add( item12, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    lnoSpin = new IntegerSpinCtrl(panel,
        1, 50, 1, current_loo.lno,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item5->Add(lnoSpin->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item5->Add(lnoSpin->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    //  Block aligner's loop percentile parameter
    wxStaticText *item15 = new wxStaticText( panel, ID_TEXT, wxT("Loop percentile:"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item5->Add( item15, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    loopPercentSpin = new FloatingPointSpinCtrl(panel,
        0.0, 100.0, 0.5, 100.0*current_loo.percentile,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item5->Add(loopPercentSpin->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item5->Add(loopPercentSpin->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    //  Block aligner's loop extension parameter
    wxStaticText *item18 = new wxStaticText( panel, ID_TEXT, wxT("Loop extension:"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item5->Add( item18, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    loopExtensionSpin = new IntegerSpinCtrl(panel,
        0, 10000, 5, current_loo.extension,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item5->Add(loopExtensionSpin->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item5->Add(loopExtensionSpin->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    //  Block aligner's loop cutoff parameter
    wxStaticText *item21 = new wxStaticText( panel, ID_TEXT, wxT("Loop cutoff (0=none):"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
    item5->Add( item21, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    loopCutoffSpin = new IntegerSpinCtrl(panel,
        0, 10000, 5, current_loo.cutoff,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item5->Add(loopCutoffSpin->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item5->Add(loopCutoffSpin->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    //  Blank space
    wxStaticText *item24 = new wxStaticText( panel, ID_TEXT, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item24, 0, wxALIGN_CENTER, 5 );
    wxStaticText *item25 = new wxStaticText( panel, ID_TEXT, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item25, 0, wxALIGN_CENTER, 5 );
    wxStaticText *item26 = new wxStaticText( panel, ID_TEXT, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item26, 0, wxALIGN_CENTER, 5 );

    //  Seed for RNG to define order of leaving out rows (useful for debugging!!)
    wxStaticText *item27 = new wxStaticText( panel, ID_TEXT, wxT("Random number seed (0 = default):"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item27, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    rngSpin = new IntegerSpinCtrl(panel,
        0, 1000000000, 3727851, current_loo.seed,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item5->Add(rngSpin->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item5->Add(rngSpin->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    //  Allow extension/contraction of footprint (N-terminus)
    wxStaticText *item30 = new wxStaticText( panel, ID_TEXT, wxT("N-terminal footprint extension:"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item30, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    nExtSpin = new IntegerSpinCtrl(panel,
        -1000, 1000, 5, current_loo.nExt,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item5->Add(nExtSpin->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item5->Add(nExtSpin->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    //  Allow extension/contraction of footprint (C-terminus)
    wxStaticText *item33 = new wxStaticText( panel, ID_TEXT, wxT("C-terminal footprint extension:"), wxDefaultPosition, wxDefaultSize, 0 );
    item5->Add( item33, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    cExtSpin = new IntegerSpinCtrl(panel,
        -1000, 1000, 5, current_loo.cExt,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item5->Add(cExtSpin->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item5->Add(cExtSpin->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    item3->Add( item5, 0, wxALIGN_CENTER_VERTICAL, 0 );



    //  ******  Until sort out how to install a refined alignment w/ new block structure,
    //          ignore the fields for all block editing parameters!!!  *******

    //  Start GUI elements for block extension parameters.
    wxStaticBox *item37 = new wxStaticBox( panel, -1, wxT("Block Extension Parameters") );
    item37->SetFont( wxFont( 10, wxROMAN, wxNORMAL, wxBOLD ) );
    wxStaticBoxSizer *item36 = new wxStaticBoxSizer( item37, wxVERTICAL );

    wxFlexGridSizer *item38 = new wxFlexGridSizer( 3, 0, 0 );
    item38->AddGrowableCol( 1 );

    //  Allow block extension?
    int initialValue = (current_be.algMethod >= 0 && current_be.algMethod < align_refine::eGreedyExtend) ? (int) current_be.algMethod : 0;
    wxStaticText *item39 = new wxStaticText( panel, ID_TEXT, wxT("Change block model?"), wxDefaultPosition, wxDefaultSize, 0 );
    item38->Add( item39, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    wxStaticText *item40 = new wxStaticText( panel, ID_TEXT, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    item38->Add( item40, 0, wxALIGN_CENTER|wxALL, 5 );
    esCombo= new wxComboBox( panel, ID_BE_COMBOBOX, wxT(""), wxDefaultPosition, wxDefaultSize, sizeof(blockEditAlgStrings)/sizeof(blockEditAlgStrings[0]), blockEditAlgStrings, wxCB_READONLY );
    esCombo->SetValue(blockEditAlgStrings[initialValue]);
    item38->Add( esCombo, 0, wxALIGN_CENTER|wxALL, 5 );

    //  Extend first?
    wxStaticText *item39b = new wxStaticText( panel, ID_TEXT, wxT("Extend first?"), wxDefaultPosition, wxDefaultSize, 0 );
    item38->Add( item39b, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    wxStaticText *item40b = new wxStaticText( panel, ID_TEXT, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    item38->Add( item40b, 0, wxALIGN_CENTER|wxALL, 5 );
    extendFirstCheck = new wxCheckBox( panel, ID_BEXTEND_FIRST_CHECKBOX, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    extendFirstCheck->SetValue(current_be.extendFirst);
    item38->Add( extendFirstCheck, 0, wxALIGN_CENTER|wxALL, 5 );

    //  Define a minimum block size (really only relevant for shrinking blocks...
    wxStaticText *item42 = new wxStaticText( panel, ID_TEXT, wxT("Minimum block size:"), wxDefaultPosition, wxDefaultSize, 0 );
    item38->Add( item42, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    minBlockSizeSpin = new IntegerSpinCtrl(panel,
        1, 100, 1, current_be.minBlockSize,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item38->Add(minBlockSizeSpin->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item38->Add(minBlockSizeSpin->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    //  Block extension/shrinkage threshold 1:  column of PSSM median score >= this value
    wxStaticText *item45 = new wxStaticText( panel, ID_TEXT, wxT("Median PSSM score threshold:"), wxDefaultPosition, wxDefaultSize, 0 );
    item38->Add( item45, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    medianSpin = new IntegerSpinCtrl(panel,
        -20, 20, 1, current_be.median,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item38->Add(medianSpin->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item38->Add(medianSpin->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    //  Block extension/shrinkage threshold 2:  % rows w/ PSSM score >= 0 must exceed this value
    wxStaticText *item48 = new wxStaticText( panel, ID_TEXT, wxT("Voting percentage (% of rows vote to extend):"), wxDefaultPosition, wxDefaultSize, 0 );
    item38->Add( item48, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    rawVoteSpin = new FloatingPointSpinCtrl(panel,
        0.0, 100.0, 1.0, 100.0*(1.0 - current_be.negRowsFraction),
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item38->Add(rawVoteSpin->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item38->Add(rawVoteSpin->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    //  Block extension/shrinkage threshold 3:  % of weight in column of PSSM median score >= 0 must exceed this value
    wxStaticText *item51 = new wxStaticText( panel, ID_TEXT, wxT("Weighted (by PSSM score) voting percentage:"), wxDefaultPosition, wxDefaultSize, 0 );
    item38->Add( item51, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    weightedVoteSpin = new FloatingPointSpinCtrl(panel,
        0.0, 100.0, 1.0, 100.0*(1.0 - current_be.negScoreFraction),
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item38->Add(weightedVoteSpin->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item38->Add(weightedVoteSpin->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    item36->Add( item38, 0, wxALIGN_CENTER|wxALL, 5 );

    //  Enable/disable block-editing controls based on state of 'extendCheck' & 'shrinkCheck'.
    wxCommandEvent dummyEvent;
    dummyEvent.SetId(ID_BE_COMBOBOX);
    OnCombo(dummyEvent);

    //  OK/Cancel buttons
    wxBoxSizer *item54 = new wxBoxSizer( wxHORIZONTAL );
    wxButton *item55 = new wxButton( panel, wxID_OK, wxT("OK"), wxDefaultPosition, wxDefaultSize, 0 );
    item55->SetDefault();
    item54->Add( item55, 0, wxALIGN_CENTER|wxALL, 5 );
    wxButton *item56 = new wxButton( panel, wxID_CANCEL, wxT("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    item54->Add( item56, 0, wxALIGN_CENTER|wxALL, 5 );


    //  Place sub-sizers in outermost sizer.
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );
    item0->Add( item1, 0, wxALIGN_CENTER|wxALL, 5 );
    item0->Add( item64, 0, wxALIGN_LEFT|wxALL, 5 );
    item0->Add( item3, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    item0->Add( item36, 0, wxALIGN_CENTER|wxALL, 5 );
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


bool BMARefinerOptionsDialog::GetParameters( GeneralRefinerParams* genl_params, align_refine::LeaveOneOutParams* loo_params, align_refine::BlockEditingParams* be_params)
{
    if (!loo_params || !be_params) return false;
    bool result = true;
    int nt  = (int) genl_params->nTrials;
    int nc  = (int) genl_params->nCycles;
    int mbs = (int) be_params->minBlockSize;
    int lno = (int) loo_params->lno;
    int sd = (int) loo_params->seed;
    int lext = (int) loo_params->extension;
    int cut = (int) loo_params->cutoff;

    result &= nTrialsSpin->GetInteger(&nt);
    result &= nCyclesSpin->GetInteger(&nc);
    genl_params->nTrials = (unsigned int) nt;
    genl_params->nCycles = (unsigned int) nc;

    loo_params->fixStructures = !fixStructCheck->IsChecked();
    loo_params->fullSequence  = fullSeqCheck->IsChecked();
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
    loo_params->percentile /= 100.0;

    wxString comboValue = esCombo->GetValue();
    bool doExtend = (comboValue == blockEditAlgStrings[(int)align_refine::eSimpleExtend] || comboValue == blockEditAlgStrings[(int)align_refine::eSimpleExtendAndShrink]);
    bool doShrink = (comboValue == blockEditAlgStrings[(int)align_refine::eSimpleShrink] || comboValue == blockEditAlgStrings[(int)align_refine::eSimpleExtendAndShrink]);
    be_params->editBlocks  = doExtend || doShrink;
    be_params->canShrink   = doShrink;
    be_params->extendFirst = extendFirstCheck->IsChecked();
    if (be_params->editBlocks) {
        be_params->algMethod = ((doExtend) ? (doShrink ? align_refine::eSimpleExtendAndShrink : align_refine::eSimpleExtend) : align_refine::eSimpleShrink);
    } else {
        be_params->algMethod = align_refine::eInvalidBBAMethod;
    }

    result &= minBlockSizeSpin->GetInteger(&mbs);
    be_params->minBlockSize = (unsigned int) mbs;

    //  Compound scorer parameters
    result &= medianSpin->GetInteger(&(be_params->median));
    result &= rawVoteSpin->GetDouble(&(be_params->negRowsFraction));
    result &= weightedVoteSpin->GetDouble(&(be_params->negScoreFraction));
    be_params->negRowsFraction = 1.0 - be_params->negRowsFraction/100.0;
    be_params->negScoreFraction = 1.0 - be_params->negScoreFraction/100.0;

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
        align_refine::LeaveOneOutParams loo;
        align_refine::BlockEditingParams be;
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

void BMARefinerOptionsDialog::OnCombo(wxCommandEvent& event)
{
    wxString comboValue = esCombo->GetValue();
    bool doExtend = (comboValue == blockEditAlgStrings[(int)align_refine::eSimpleExtend] || comboValue == blockEditAlgStrings[(int)align_refine::eSimpleExtendAndShrink]);
    bool doShrink = (comboValue == blockEditAlgStrings[(int)align_refine::eSimpleShrink] || comboValue == blockEditAlgStrings[(int)align_refine::eSimpleExtendAndShrink]);
    bool doBlockEdit = (doExtend || doShrink);

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

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2005/11/22 19:06:35  lanczyck
* make block/row selection work properly
*
* Revision 1.7  2005/11/03 00:20:29  lanczyck
* modify refiner interface and set up of refiner engine to allow for block shrinking and/or to skip block expansion
*
* Revision 1.6  2005/11/02 20:32:43  lanczyck
* add API to specify blocks to refine and rows to exclude from refinement;
* turn block extension on
*
* Revision 1.5  2005/10/25 20:54:29  thiessen
* reset diag post level after refinement
*
* Revision 1.4  2005/10/25 17:41:35  thiessen
* fix flicker in alignment display; add progress meter and misc fixes to refiner
*
* Revision 1.3  2005/10/21 21:59:49  thiessen
* working refiner integration
*
* Revision 1.2  2005/10/19 17:28:18  thiessen
* migrate to wxWidgets 2.6.2; handle signed/unsigned issue
*
* Revision 1.1  2005/10/18 21:38:33  lanczyck
* initial versions (still containing CJL hacks incompatible w/ official cn3d)
*
*/
