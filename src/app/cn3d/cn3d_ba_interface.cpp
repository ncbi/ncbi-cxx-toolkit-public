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

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

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

#include <algo/structure/struct_dp/struct_dp.h>

#include "cn3d_ba_interface.hpp"
#include "block_multiple_alignment.hpp"
#include "sequence_set.hpp"
#include "structure_set.hpp"
#include "asn_converter.hpp"
#include "molecule_identifier.hpp"
#include "wx_tools.hpp"
#include "cn3d_tools.hpp"
#include "cn3d_blast.hpp"
#include "sequence_viewer.hpp"

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
    wxCheckBox *cGlobal, *cKeep, *cMerge;

    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);
    DECLARE_EVENT_TABLE()
};

BlockAligner::BlockAligner(void)
{
    // default options
    currentOptions.loopPercentile = 0.6;
    currentOptions.loopExtension = 10;
    currentOptions.loopCutoff = 0;
    currentOptions.globalAlignment = false;
    currentOptions.keepExistingBlocks = false;
    currentOptions.mergeAfterEachSequence = false;
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
    const Block::Range *multipleRangeMaster, *pairwiseRangeMaster, *pairwiseRangeSlave;
    int i;
    for (i=0, m=multipleABlocks.begin(); m!=me; ++i, ++m) {
        multipleRangeMaster = (*m)->GetRangeOfRow(0);
        for (p=pairwiseABlocks.begin(); p!=pe; ++p) {
            pairwiseRangeMaster = (*p)->GetRangeOfRow(0);
            if (pairwiseRangeMaster->from <= multipleRangeMaster->from &&
                    pairwiseRangeMaster->to >= multipleRangeMaster->to) {
                pairwiseRangeSlave = (*p)->GetRangeOfRow(1);
                blockInfo->freezeBlocks[i] = pairwiseRangeSlave->from +
                    (multipleRangeMaster->from - pairwiseRangeMaster->from);
                break;
            }
        }
        if (p == pe)
            blockInfo->freezeBlocks[i] = -1;
//        if (blockInfo->freezeBlocks[i] >= 0)
//            TESTMSG("block " << (i+1) << " frozen at query pos " << (blockInfo->freezeBlocks[i]+1));
//        else
//            TESTMSG("block " << (i+1) << " unfrozen");
    }
}

// global stuff for DP block aligner score callback
DP_BlockInfo *dpBlocks = NULL;
const BLAST_Matrix *dpPSSM = NULL;
const Sequence *dpQuery = NULL;

// sum of scores for residue vs. PSSM
extern "C" {
int dpScoreFunction(unsigned int block, unsigned int queryPos)
{
    if (!dpBlocks || !dpPSSM || !dpQuery || block >= dpBlocks->nBlocks ||
        queryPos > dpQuery->Length() - dpBlocks->blockSizes[block])
    {
        ERRORMSG("dpScoreFunction() - bad parameters: block " << block << " queryPos " << queryPos <<
            " query sequence " << dpQuery->identifier->ToString());
        return DP_NEGATIVE_INFINITY;
    }

    int i, masterPos = dpBlocks->blockPositions[block], score = 0;
    for (i=0; i<dpBlocks->blockSizes[block]; ++i)
        score += dpPSSM->matrix[masterPos + i]
            [LookupNCBIStdaaNumberFromCharacter(dpQuery->sequenceString[queryPos + i])];

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
    // show options dialog each time block aligner is run
    if (!SetOptions(NULL)) return false;
    if (currentOptions.mergeAfterEachSequence && !sequenceViewer) {
        ERRORMSG("merge selected but NULL sequenceViewer");
        return false;
    }

    newAlignments->clear();
    *nRowsAddedToMultiple = 0;
    BlockMultipleAlignment::UngappedAlignedBlockList blocks;
    multiple->GetUngappedAlignedBlocks(&blocks);
    if (blocks.size() == 0) {
        ERRORMSG("Must have at least one aligned block to use the block aligner");
        return false;
    }
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks.end();
    int i;

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
            for (int r=0; r<multiple->NRows(); ++r) {
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

    // set up PSSM
    dpPSSM = multiple->GetPSSM();

    bool errorsEncountered = false;

    AlignmentList::const_iterator s, se = toRealign.end();
    for (s=toRealign.begin(); s!=se; ++s) {
        if (multiple && (*s)->GetMaster() != multiple->GetMaster())
            ERRORMSG("master sequence mismatch");

        // slave sequence info
        dpQuery = (*s)->GetSequenceOfRow(1);
        int startQueryPosition =
            ((*s)->alignSlaveFrom >= 0 && (*s)->alignSlaveFrom < dpQuery->Length()) ?
                (*s)->alignSlaveFrom : 0;
        int endQueryPosition =
            ((*s)->alignSlaveTo >= 0 && (*s)->alignSlaveTo < dpQuery->Length()) ?
                (*s)->alignSlaveTo : dpQuery->Length() - 1;
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
            if (currentOptions.mergeAfterEachSequence) {
                if (!sequenceViewer->EditorIsOn())
                    sequenceViewer->TurnOnEditor();
                if (multiple->MergeAlignment(dpAlignment)) {
                    // if merge is successful, we can delete this alignment;
                    delete dpAlignment;
                    dpAlignment = NULL;
                    ++(*nRowsAddedToMultiple);
                    // recalculate PSSM
                    dpPSSM = multiple->GetPSSM();
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

    if (errorsEncountered)
        ERRORMSG("Block aligner encountered problem(s) - see message log for details");

    // cleanup
    DP_DestroyBlockInfo(dpBlocks);
    dpBlocks = NULL;
    dpPSSM = NULL;
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
        wxDialog(parent, -1, "Set Block Aligner Options", wxPoint(100,100), wxDefaultSize,
            wxCAPTION | wxSYSTEM_MENU) // not resizable
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
        0.0, 100.0, 0.6, init.loopPercentile,
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

    wxStaticText *item17 = new wxStaticText( panel, ID_TEXT, wxT("Merge after each row aligned:"), wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item17, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    cMerge = new wxCheckBox( panel, ID_C_MERGE, wxT(""), wxDefaultPosition, wxDefaultSize, 0 );
    cMerge->SetValue(init.mergeAfterEachSequence);
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
    options->mergeAfterEachSequence = cMerge->IsChecked();
    return (
        fpPercent->GetDouble(&(options->loopPercentile)) &&
        iExtension->GetInteger(&(options->loopExtension)) &&
        iCutoff->GetInteger(&(options->loopCutoff))
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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.37  2005/03/08 17:22:31  thiessen
* apparently working C++ PSSM generation
*
* Revision 1.36  2004/09/28 14:18:28  thiessen
* turn on editor automatically on merge
*
* Revision 1.35  2004/07/27 17:38:12  thiessen
* don't call GetPSSM() w/ no aligned blocks
*
* Revision 1.34  2004/06/14 17:58:47  thiessen
* fix error in block score reporting
*
* Revision 1.33  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.32  2004/03/15 18:19:23  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.31  2004/03/10 23:15:51  thiessen
* add ability to turn off/on error dialogs; group block aligner errors in message log
*
* Revision 1.30  2004/02/19 17:04:48  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.29  2004/02/19 02:16:06  thiessen
* fix struct_dp.h path
*
* Revision 1.28  2003/08/23 22:26:23  thiessen
* switch to new dp block aligner, remove Alejandro's
*
* Revision 1.27  2003/07/21 19:54:10  thiessen
* fix firstBlock error
*
* Revision 1.26  2003/07/14 18:35:27  thiessen
* run DP and Alejandro's block aligners, and compare results
*
* Revision 1.25  2003/03/27 18:45:59  thiessen
* update blockaligner code
*
* Revision 1.24  2003/02/03 19:20:02  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.23  2003/01/23 20:03:05  thiessen
* add BLAST Neighbor algorithm
*
* Revision 1.22  2003/01/22 14:47:30  thiessen
* cache PSSM in BlockMultipleAlignment
*
* Revision 1.21  2002/12/20 02:51:46  thiessen
* fix Prinf to self problems
*
* Revision 1.20  2002/12/09 16:25:04  thiessen
* allow negative score threshholds
*
* Revision 1.19  2002/11/06 00:18:10  thiessen
* fixes for new CRef/const rules in objects
*
* Revision 1.18  2002/09/30 17:13:02  thiessen
* change structure import to do sequences as well; change cache to hold mimes; change block aligner vocabulary; fix block aligner dialog bugs
*
* Revision 1.17  2002/09/23 19:12:32  thiessen
* add option to allow long gaps between frozen blocks
*
* Revision 1.16  2002/09/21 12:36:28  thiessen
* add frozen block position validation; add select-other-by-distance
*
* Revision 1.15  2002/09/20 17:48:39  thiessen
* fancier trace statements
*
* Revision 1.14  2002/09/19 14:09:41  thiessen
* position options dialog higher up
*
* Revision 1.13  2002/09/19 12:51:08  thiessen
* fix block aligner / update bug; add distance select for other molecules only
*
* Revision 1.12  2002/09/16 21:24:58  thiessen
* add block freezing to block aligner
*
* Revision 1.11  2002/08/15 22:13:13  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.10  2002/08/14 00:02:08  thiessen
* combined block/global aligner from Alejandro
*
* Revision 1.8  2002/08/09 18:24:08  thiessen
* improve/add magic formula to avoid Windows symbol clashes
*
* Revision 1.7  2002/08/04 21:41:05  thiessen
* fix GetObject problem
*
* Revision 1.6  2002/08/01 12:51:36  thiessen
* add E-value display to block aligner
*
* Revision 1.5  2002/08/01 01:55:16  thiessen
* add block aligner options dialog
*
* Revision 1.4  2002/07/29 19:22:46  thiessen
* another blockalign bug fix; set better parameters to block aligner
*
* Revision 1.3  2002/07/29 13:25:42  thiessen
* add range restriction to block aligner
*
* Revision 1.2  2002/07/27 12:29:51  thiessen
* fix block aligner crash
*
* Revision 1.1  2002/07/26 15:28:45  thiessen
* add Alejandro's block alignment algorithm
*
*/
