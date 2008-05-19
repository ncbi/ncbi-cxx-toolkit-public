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
*      Interface to Alejandro's block alignment algorithm
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>

#include "remove_header_conflicts.hpp"

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/numdlg.h>

#include <algo/structure/struct_dp/struct_dp.h>

#include "cn3d_ba_interface.hpp"
#include "block_multiple_alignment.hpp"
#include "sequence_set.hpp"
#include "structure_set.hpp"
#include "molecule_identifier.hpp"
#include "wx_tools.hpp"
#include "cn3d_tools.hpp"
#include "sequence_viewer.hpp"
#include "cn3d_pssm.hpp"
#include "progress_meter.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

class BlockAlignerOptionsDialog : public wxDialog
{
public:
    BlockAlignerOptionsDialog(wxWindow* parent, const BlockAligner::BlockAlignerOptions& init);
    ~BlockAlignerOptionsDialog(void);

    bool GetValues(BlockAligner::BlockAlignerOptions *options);

private:
    IntegerSpinCtrl *iExtension, *iCutoff;
    FloatingPointSpinCtrl *fpPercent;
    wxCheckBox *cGlobal, *cKeep;
    wxChoice *cMerge;

    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);
    DECLARE_EVENT_TABLE()
};

BlockAligner::BlockAligner(void)
{
    // default options
    currentOptions.loopPercentile = 1.0;
    currentOptions.loopExtension = 10;
    currentOptions.loopCutoff = 0;
    currentOptions.globalAlignment = false;
    currentOptions.keepExistingBlocks = false;
    currentOptions.mergeType = mergeNone;
}

static void FreezeBlocks(const BlockMultipleAlignment *multiple,
    const BlockMultipleAlignment *pairwise, DP_BlockInfo *blockInfo)
{
    BlockMultipleAlignment::UngappedAlignedBlockList multipleABlocks, pairwiseABlocks;
    multiple->GetUngappedAlignedBlocks(&multipleABlocks);
    pairwise->GetUngappedAlignedBlocks(&pairwiseABlocks);

    // if a block in the multiple is contained in the pairwise (looking at master coords),
    // then add a constraint to keep it there
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator
        m, me = multipleABlocks.end(), p, pe = pairwiseABlocks.end();
    const Block::Range *multipleRangeMaster, *pairwiseRangeMaster, *pairwiseRangeDependent;
    int i;
    for (i=0, m=multipleABlocks.begin(); m!=me; ++i, ++m) {
        multipleRangeMaster = (*m)->GetRangeOfRow(0);
        for (p=pairwiseABlocks.begin(); p!=pe; ++p) {
            pairwiseRangeMaster = (*p)->GetRangeOfRow(0);
            if (pairwiseRangeMaster->from <= multipleRangeMaster->from &&
                    pairwiseRangeMaster->to >= multipleRangeMaster->to) {
                pairwiseRangeDependent = (*p)->GetRangeOfRow(1);
                blockInfo->freezeBlocks[i] = pairwiseRangeDependent->from +
                    (multipleRangeMaster->from - pairwiseRangeMaster->from);
                break;
            }
        }
        if (p == pe)
            blockInfo->freezeBlocks[i] = DP_UNFROZEN_BLOCK;
//        if (blockInfo->freezeBlocks[i] != DP_UNFROZEN_BLOCK)
//            TESTMSG("block " << (i+1) << " frozen at query pos " << (blockInfo->freezeBlocks[i]+1));
//        else
//            TESTMSG("block " << (i+1) << " unfrozen");
    }
}

// global stuff for DP block aligner score callback
DP_BlockInfo *dpBlocks = NULL;
auto_ptr < BlockMultipleAlignment > dpMultiple;
const Sequence *dpQuery = NULL;

// sum of scores for residue vs. PSSM
extern "C" {
int dpScoreFunction(unsigned int block, unsigned int queryPos)
{
    if (!dpBlocks || !dpMultiple.get() || !dpQuery || block >= dpBlocks->nBlocks ||
        queryPos > dpQuery->Length() - dpBlocks->blockSizes[block])
    {
        ERRORMSG("dpScoreFunction() - bad parameters: block " << block << " queryPos " << queryPos <<
            " query sequence " << dpQuery->identifier->ToString());
        return DP_NEGATIVE_INFINITY;
    }

    unsigned int i, masterPos = dpBlocks->blockPositions[block], score = 0;
    for (i=0; i<dpBlocks->blockSizes[block]; ++i)
        score += dpMultiple->GetPSSM().GetPSSMScore(
            LookupNCBIStdaaNumberFromCharacter(dpQuery->sequenceString[queryPos + i]),
            masterPos + i);

    return score;
}
}

static BlockMultipleAlignment * UnpackDPResult(DP_BlockInfo *blocks, DP_AlignmentResult *result,
    const Sequence *master, const Sequence *query)
{
    // create new alignment structure
    BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
    (*seqs)[0] = master;
    (*seqs)[1] = query;
    auto_ptr<BlockMultipleAlignment>
        bma(new BlockMultipleAlignment(seqs, master->parentSet->alignmentManager));

    // unpack result blocks
    TRACEMSG("result nBlocks " << result->nBlocks << " firstBlock " << result->firstBlock);
    int s, blockScoreSum = 0;
    for (unsigned int b=0; b<result->nBlocks; ++b) {
        UngappedAlignedBlock *newBlock = new UngappedAlignedBlock(bma.get());
        newBlock->width = blocks->blockSizes[b + result->firstBlock];
        newBlock->SetRangeOfRow(0,
            blocks->blockPositions[b + result->firstBlock],
            blocks->blockPositions[b + result->firstBlock] + newBlock->width - 1);
        newBlock->SetRangeOfRow(1,
            result->blockPositions[b],
            result->blockPositions[b] + newBlock->width - 1);
        bma->AddAlignedBlockAtEnd(newBlock);
        s = dpScoreFunction(b + result->firstBlock, result->blockPositions[b]);
        TRACEMSG("block " << (b + result->firstBlock + 1)
            << " position: " << (result->blockPositions[b] + 1)
            << " score: " << s);
        blockScoreSum += s;
    }
    if (blockScoreSum != result->score)
        ERRORMSG("block aligner reported score doesn't match sum of block scores");

    // finalize the alignment
    if (!bma->AddUnalignedBlocks() || !bma->UpdateBlockMapAndColors(false)) {
        ERRORMSG("Error finalizing new alignment!");
        return NULL;
    }

    // get scores
    wxString score;
    score.Printf(" raw score: %i", result->score);
    bma->SetRowStatusLine(0, score.c_str());
    bma->SetRowStatusLine(1, score.c_str());

    // success
    return bma.release();
}

bool BlockAligner::CreateNewPairwiseAlignmentsByBlockAlignment(BlockMultipleAlignment *multiple,
    const AlignmentList& toRealign, AlignmentList *newAlignments,
    int *nRowsAddedToMultiple, SequenceViewer *sequenceViewer)
{
    newAlignments->clear();
    *nRowsAddedToMultiple = 0;

    // show options dialog each time block aligner is run
    if (!SetOptions(NULL)) return false;
    if (currentOptions.mergeType != mergeNone && !sequenceViewer) {
        ERRORMSG("merge selected but NULL sequenceViewer");
        return false;
    }

    unsigned int nAln = toRealign.size(), a = 0;

    BlockMultipleAlignment::UngappedAlignedBlockList blocks;
    multiple->GetUngappedAlignedBlocks(&blocks);
    if (blocks.size() == 0) {
        ERRORMSG("Must have at least one aligned block to use the block aligner");
        return false;
    }

    //  Make the progress meter after the above check so don't have to clean it up 
    //  on the Mac in the event the above test returns prematurely.
    auto_ptr < ProgressMeter > progress;
    if (nAln > 1) {
        long u = wxGetNumberFromUser("How many sequences do you want to realign?", "Max:", "Alignments...", nAln, 1, nAln, NULL);
        if (u <= 0)
            return false;
        nAln = (unsigned int) u;
        if (nAln > 1)
            progress.reset(new ProgressMeter(NULL, "Running block alignment...", "Working", nAln));
    }

    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks.end();
    unsigned int i;

    // set up block info
    dpBlocks = DP_CreateBlockInfo(blocks.size());
    unsigned int *loopLengths = new unsigned int[multiple->NRows()];
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator n;
    const Block::Range *range, *nextRange;
    for (i=0, b=blocks.begin(); b!=be; ++i, ++b) {
        range = (*b)->GetRangeOfRow(0);
        dpBlocks->blockPositions[i] = range->from;
        dpBlocks->blockSizes[i] = range->to - range->from + 1;

        // calculate max loop lengths
        if (i < blocks.size() - 1) {
            n = b;
            ++n;
            for (unsigned int r=0; r<multiple->NRows(); ++r) {
                range = (*b)->GetRangeOfRow(r);
                nextRange = (*n)->GetRangeOfRow(r);
                loopLengths[r] = nextRange->from - range->to - 1;
            }
            dpBlocks->maxLoops[i] = DP_CalculateMaxLoopLength(multiple->NRows(), loopLengths,
                currentOptions.loopPercentile, currentOptions.loopExtension, currentOptions.loopCutoff);
            TRACEMSG("allowed gap after block " << (i+1) << ": " << dpBlocks->maxLoops[i]);
        }
    }
    delete loopLengths;

    // store a copy of the multiple alignment used for DP scoring
    dpMultiple.reset(multiple->Clone());

    bool errorsEncountered = false;

    AlignmentList::const_iterator s, se = toRealign.end();
    for (s=toRealign.begin(); s!=se; ++s, ++a) {
        if (multiple && (*s)->GetMaster() != multiple->GetMaster())
            ERRORMSG("master sequence mismatch");

        if (a >= nAln) {
            BlockMultipleAlignment *newAlignment = (*s)->Clone();
            newAlignment->SetRowDouble(0, -1.0);
            newAlignment->SetRowDouble(1, -1.0);
            newAlignment->SetRowStatusLine(0, kEmptyStr);
            newAlignment->SetRowStatusLine(1, kEmptyStr);
            newAlignments->push_back(newAlignment);
            continue;
        }

        if (progress.get())
            progress->SetValue(a);

        // dependent sequence info
        dpQuery = (*s)->GetSequenceOfRow(1);
        int startQueryPosition =
            ((*s)->alignDependentFrom >= 0 && (*s)->alignDependentFrom < (int)dpQuery->Length()) ?
                (*s)->alignDependentFrom : 0;
        int endQueryPosition =
            ((*s)->alignDependentTo >= 0 && (*s)->alignDependentTo < (int)dpQuery->Length()) ?
                (*s)->alignDependentTo : dpQuery->Length() - 1;
        TRACEMSG("query region: " << (startQueryPosition+1) << " to " << (endQueryPosition+1));

        // set frozen blocks
        if (!currentOptions.keepExistingBlocks) {
            for (i=0; i<blocks.size(); ++i)
                dpBlocks->freezeBlocks[i] = DP_UNFROZEN_BLOCK;
        } else {
            FreezeBlocks(multiple, *s, dpBlocks);
        }

        SetDialogSevereErrors(false);

        // actually do the block alignment
        DP_AlignmentResult *dpResult = NULL;
        int dpStatus;
        if (currentOptions.globalAlignment)
            dpStatus = DP_GlobalBlockAlign(dpBlocks, dpScoreFunction,
                startQueryPosition, endQueryPosition, &dpResult);
        else
            dpStatus = DP_LocalBlockAlign(dpBlocks, dpScoreFunction,
                startQueryPosition, endQueryPosition, &dpResult);

        SetDialogSevereErrors(true);

        // create new alignment from DP result
        BlockMultipleAlignment *dpAlignment = NULL;
        if (dpResult) {
            dpAlignment = UnpackDPResult(dpBlocks, dpResult, multiple->GetMaster(), dpQuery);
            if (!dpAlignment)
                ERRORMSG("Couldn't create BlockMultipleAlignment from DP result");
        }

        if (dpStatus == STRUCT_DP_FOUND_ALIGNMENT && dpAlignment) {

            // merge or add alignment to list
            if (currentOptions.mergeType != mergeNone) {
                if (!sequenceViewer->EditorIsOn())
                    sequenceViewer->TurnOnEditor();
                if (multiple->MergeAlignment(dpAlignment)) {
                    // if merge is successful, we can delete this alignment;
                    delete dpAlignment;
                    dpAlignment = NULL;
                    ++(*nRowsAddedToMultiple);
                    // update scoring alignment/PSSM only if asked to do so after each sequence
                    if (currentOptions.mergeType == mergeAfterEach)
                        dpMultiple.reset(multiple->Clone());
                }
            }
            if (dpAlignment)
                newAlignments->push_back(dpAlignment);
        }

        // no alignment or block aligner failed - add old alignment to list so it doesn't get lost
        else {
            string status;
            if (dpStatus == STRUCT_DP_NO_ALIGNMENT) {
                WARNINGMSG("block aligner found no significant alignment - current alignment unchanged");
                status = "alignment unchanged";
            } else {
                WARNINGMSG("block aligner encountered a problem - current alignment unchanged");
                errorsEncountered = true;
                status = "block aligner error!";
            }
            BlockMultipleAlignment *newAlignment = (*s)->Clone();
            newAlignment->SetRowDouble(0, -1.0);
            newAlignment->SetRowDouble(1, -1.0);
            newAlignment->SetRowStatusLine(0, status);
            newAlignment->SetRowStatusLine(1, status);
            newAlignments->push_back(newAlignment);
        }

        // cleanup
        DP_DestroyAlignmentResult(dpResult);
    }

//  Bug fix:  the Mac does not like the use of auto_ptr for some reason.
//  Control is not returned to the parent window properly and most events
//  involving mouse clicks, menus are subsequently ignored in all windows.
//  Destroy() and reseting to null do not appear necessary, but I'll leave
//  them commented out here as a reminder they may be needed later.
#ifdef __WXMAC__
    if (progress.get()) {
        progress->Close(true);
//        progressMeter->Destroy();
//        progressMeter = NULL;
    }
#endif

    if (errorsEncountered)
        ERRORMSG("Block aligner encountered problem(s) - see message log for details");

    // cleanup
    DP_DestroyBlockInfo(dpBlocks);
    dpBlocks = NULL;
    dpMultiple.reset();
    dpQuery = NULL;

    return true;
}

bool BlockAligner::SetOptions(wxWindow* parent)
{
    BlockAlignerOptionsDialog dialog(parent, currentOptions);
    bool ok = (dialog.ShowModal() == wxOK);
    if (ok && !dialog.GetValues(&currentOptions))
        ERRORMSG("Error getting options from dialog!");
    return ok;
}


/////////////////////////////////////////////////////////////////////////////////////
////// BlockAlignerOptionsDialog stuff
////// taken (and modified) from block_aligner_dialog.wdr code
/////////////////////////////////////////////////////////////////////////////////////

#define ID_TEXT 10000
#define ID_T_PERCENT 10001
#define ID_S_EXT 10002
#define ID_T_EXTENSION 10003
#define ID_S_PERCENT 10004
#define ID_T_CUTOFF 10005
#define ID_S_LAMBDA 10006
#define ID_C_GLOBAL 10007
#define ID_C_KEEP 10008
#define ID_C_MERGE 10009
#define ID_B_OK 10010
#define ID_B_CANCEL 10011

BEGIN_EVENT_TABLE(BlockAlignerOptionsDialog, wxDialog)
    EVT_BUTTON(-1,  BlockAlignerOptionsDialog::OnButton)
    EVT_CLOSE (     BlockAlignerOptionsDialog::OnCloseWindow)
END_EVENT_TABLE()

BlockAlignerOptionsDialog::BlockAlignerOptionsDialog(
    wxWindow* parent, const BlockAligner::BlockAlignerOptions& init) :
        wxDialog(parent, -1, "Set Block Aligner Options", wxPoint(100,100), wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
{
    wxPanel *panel = new wxPanel(this, -1);
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );
    wxStaticBox *item2 = new wxStaticBox( panel, -1, wxT("Block Aligner Options") );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );
    wxFlexGridSizer *item3 = new wxFlexGridSizer( 3, 0, 0 );
    item3->AddGrowableCol( 1 );

    wxStaticText *item4 = new wxStaticText( panel, ID_TEXT, wxT("Loop percentile:"), wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item4, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    fpPercent = new FloatingPointSpinCtrl(panel,
        0.0, 100.0, 0.1, init.loopPercentile,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(fpPercent->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(fpPercent->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item7 = new wxStaticText( panel, ID_TEXT, wxT("Loop extension:"), wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item7, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    iExtension = new IntegerSpinCtrl(panel,
        0, 100000, 10, init.loopExtension,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(iExtension->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(iExtension->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item10 = new wxStaticText( panel, ID_TEXT, wxT("Loop cutoff (0=none):"), wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item10, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    iCutoff = new IntegerSpinCtrl(panel,
        0, 100000, 0, init.loopCutoff,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(iCutoff->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(iCutoff->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    item3->Add( 5, 5, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 5, 5, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 5, 5, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item13 = new wxStaticText( panel, ID_TEXT, wxT("Global alignment:"), wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item13, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    cGlobal = new wxCheckBox( panel, ID_C_GLOBAL, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    cGlobal->SetValue(init.globalAlignment);
    item3->Add( cGlobal, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 5, 5, 0, wxALIGN_CENTRE, 5 );

    wxStaticText *item15 = new wxStaticText( panel, ID_TEXT, wxT("Keep existing blocks (global only):"), wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item15, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    cKeep = new wxCheckBox( panel, ID_C_KEEP, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    cKeep->SetValue(init.keepExistingBlocks);
    item3->Add( cKeep, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 5, 5, 0, wxALIGN_CENTRE, 5 );

    wxStaticText *item17 = new wxStaticText( panel, ID_TEXT, wxT("When to merge new alignments:"), wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item17, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    wxString choices[3] = { "No Merge", "After Each", "After All" };
    cMerge = new wxChoice( panel, ID_C_MERGE, wxDefaultPosition, wxDefaultSize, 3, choices );
    cMerge->SetSelection(init.mergeType);
    item3->Add( cMerge, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 5, 5, 0, wxALIGN_CENTRE, 5 );

    item1->Add( item3, 0, wxALIGN_CENTRE, 5 );
    item0->Add( item1, 0, wxALIGN_CENTRE|wxALL, 5 );
    wxBoxSizer *item19 = new wxBoxSizer( wxHORIZONTAL );
    wxButton *item20 = new wxButton( panel, ID_B_OK, wxT("OK"), wxDefaultPosition, wxDefaultSize, 0 );
    item19->Add( item20, 0, wxALIGN_CENTRE|wxALL, 5 );
    item19->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );
    wxButton *item21 = new wxButton( panel, ID_B_CANCEL, wxT("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    item19->Add( item21, 0, wxALIGN_CENTRE|wxALL, 5 );
    item0->Add( item19, 0, wxALIGN_CENTRE|wxALL, 5 );

    panel->SetAutoLayout(true);
    panel->SetSizer(item0);
    item0->Fit(this);
    item0->Fit(panel);
    item0->SetSizeHints(this);
}

BlockAlignerOptionsDialog::~BlockAlignerOptionsDialog(void)
{
    delete iCutoff;
    delete iExtension;
    delete fpPercent;
}

bool BlockAlignerOptionsDialog::GetValues(BlockAligner::BlockAlignerOptions *options)
{
    options->globalAlignment = cGlobal->IsChecked();
    options->keepExistingBlocks = cKeep->IsChecked();
    options->mergeType = (BlockAligner::EMergeOption) cMerge->GetSelection();
    return (
        fpPercent->GetDouble(&(options->loopPercentile)) &&
        iExtension->GetUnsignedInteger(&(options->loopExtension)) &&
        iCutoff->GetUnsignedInteger(&(options->loopCutoff))
    );
}

void BlockAlignerOptionsDialog::OnCloseWindow(wxCloseEvent& event)
{
    EndModal(wxCANCEL);
}

void BlockAlignerOptionsDialog::OnButton(wxCommandEvent& event)
{
    if (event.GetId() == ID_B_OK) {
		BlockAligner::BlockAlignerOptions dummy;
        if (GetValues(&dummy))  // can't successfully quit if values aren't valid
            EndModal(wxOK);
        else
            wxBell();
    } else if (event.GetId() == ID_B_CANCEL) {
        EndModal(wxCANCEL);
    } else {
        event.Skip();
    }
}

END_SCOPE(Cn3D)
