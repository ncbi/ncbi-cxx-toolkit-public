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
* ---------------------------------------------------------------------------
* $Log$
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
* ===========================================================================
*/

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

#include "cn3d/cn3d_ba_interface.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/asn_converter.hpp"
#include "cn3d/molecule_identifier.hpp"
#include "cn3d/wx_tools.hpp"
#include "cn3d/cn3d_blast.hpp"
#include "cn3d/cn3d_tools.hpp"

// necessary C-toolkit headers
#include <ncbi.h>
#include <blastkar.h>
#include <posit.h>
#include "cn3d/cn3d_blocka.h"

// functions from cn3d_blockalign.c accessed herein (no header for these yet...)
extern "C" {
extern void allocateAlignPieceMemory(Int4 numBlocks);
void findAllowedGaps(SeqAlign *listOfSeqAligns, Int4 numBlocks, Int4 *allowedGaps,
    Nlm_FloatHi percentile, Int4 gapAddition);
extern void findAlignPieces(Uint1Ptr convertedQuery, Int4 queryLength,
    Int4 startQueryPosition, Int4 endQueryPosition, Int4 numBlocks, Int4 *blockStarts, Int4 *blockEnds,
    Int4 masterLength, BLAST_Score **posMatrix, BLAST_Score scoreThreshold,
    Int4 *frozenBlocks, Boolean localAlignment);
extern void LIBCALL sortAlignPieces(Int4 numBlocks);
extern SeqAlign *makeMultiPieceAlignments(Uint1Ptr query, Int4 numBlocks, Int4 queryLength, Uint1Ptr seq,
    Int4 seqLength, Int4 *blockStarts, Int4 *blockEnds, Int4 *allowedGaps, Int4 scoreThresholdMultipleBlock,
    SeqIdPtr subject_id, SeqIdPtr query_id, Int4* bestFirstBlock, Int4 *bestLastBlock, Nlm_FloatHi Lambda,
    Nlm_FloatHi K, Int4 searchSpaceSize, Boolean localAlignment);
extern void freeBestPairs(Int4 numBlocks);
extern void freeAlignPieceLists(Int4 numBlocks);
extern void freeBestScores(Int4 numBlocks);
}

// hack so I can catch memory leaks specific to this module, at the line where allocation occurs
#ifdef _DEBUG
#ifdef MemNew
#undef MemNew
#endif
#define MemNew(sz) memset(malloc(sz), 0, (sz))
#endif

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
    IntegerSpinCtrl *iSingle, *iMultiple, *iExtend, *iSize;
    FloatingPointSpinCtrl *fpPercent, *fpLambda, *fpK;
    wxCheckBox *cGlobal, *cMerge, *cAlignAll;

    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);
    DECLARE_EVENT_TABLE()
};

BlockAligner::BlockAligner(void)
{
    // default options
    currentOptions.singleBlockThreshold = 7;
    currentOptions.multipleBlockThreshold = 7;
    currentOptions.allowedGapExtension = 10;
    currentOptions.gapLengthPercentile = 0.6;
    currentOptions.lambda = 0.0;
    currentOptions.K = 0.0;
    currentOptions.searchSpaceSize = 0;
    currentOptions.globalAlignment = true;
    currentOptions.mergeAfterEachSequence = false;
    currentOptions.alignAllBlocks = true;
}

static Int4 Round(double d)
{
    if (d >= 0.0)
        return (Int4) (d + 0.5);
    else
        return (Int4) (d - 0.5);
}

static BlockMultipleAlignment * UnpackBlockAlignerSeqAlign(const CSeq_align& sa,
    const Sequence *master, const Sequence *query)
{
    auto_ptr<BlockMultipleAlignment> bma;

    // make sure the overall structure of this SeqAlign is what we expect
    if (!sa.IsSetDim() || sa.GetDim() != 2 ||
        !((sa.GetSegs().IsDisc() && sa.GetSegs().GetDisc().Get().size() > 0) || sa.GetSegs().IsDenseg()))
    {
        ERR_POST(Error << "Confused by block aligner's result format");
        return NULL;
    }

    // create new alignment structure
    BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
    (*seqs)[0] = master;
    (*seqs)[1] = query;
    bma.reset(new BlockMultipleAlignment(seqs, master->parentSet->alignmentManager));

    // get list of segs (can be a single or a set)
    CSeq_align_set::Tdata segs;
    if (sa.GetSegs().IsDisc())
        segs = sa.GetSegs().GetDisc().Get();
    else
        segs.push_back(CRef<CSeq_align>(const_cast<CSeq_align*>(&sa)));

    // loop through segs, adding aligned block for each starts pair that doesn't describe a gap
    CSeq_align_set::Tdata::const_iterator s, se = segs.end();
    CDense_seg::TStarts::const_iterator i_start;
    CDense_seg::TLens::const_iterator i_len;
    for (s=segs.begin(); s!=se; s++) {

        // check to make sure query is first id, master is second id
        if ((*s)->GetDim() != 2 || !(*s)->GetSegs().IsDenseg() || (*s)->GetSegs().GetDenseg().GetDim() != 2 ||
            (*s)->GetSegs().GetDenseg().GetIds().size() != 2 ||
            !query->identifier->MatchesSeqId(*((*s)->GetSegs().GetDenseg().GetIds().front())) ||
            !master->identifier->MatchesSeqId(*((*s)->GetSegs().GetDenseg().GetIds().back())))
        {
            ERR_POST(Error << "Confused by seg format in block aligner's result");
            return NULL;
        }

        int i, queryStart, masterStart, length;
        i_start = (*s)->GetSegs().GetDenseg().GetStarts().begin();
        i_len = (*s)->GetSegs().GetDenseg().GetLens().begin();
        for (i=0; i<(*s)->GetSegs().GetDenseg().GetNumseg(); i++) {

            // if either start is -1, this is a gap -> skip
            queryStart = *(i_start++);
            masterStart = *(i_start++);
            length = *(i_len++);
            if (queryStart < 0 || masterStart < 0 || length <= 0) continue;

            // add aligned block
            UngappedAlignedBlock *newBlock = new UngappedAlignedBlock(bma.get());
            newBlock->SetRangeOfRow(0, masterStart, masterStart + length - 1);
            newBlock->SetRangeOfRow(1, queryStart, queryStart + length - 1);
            newBlock->width = length;
            bma->AddAlignedBlockAtEnd(newBlock);
        }
    }

    // finalize the alignment
    if (!bma->AddUnalignedBlocks() || !bma->UpdateBlockMapAndColors(false)) {
        ERR_POST(Error << "Error finalizing alignment!");
        return NULL;
    }

    // get scores
    wxString score;
    CSeq_align::TScore::const_iterator c, ce = sa.GetScore().end();
    for (c=sa.GetScore().begin(); c!=ce; c++) {
        if ((*c)->IsSetId() && (*c)->GetId().IsStr()) {

            // raw score
            if ((*c)->GetValue().IsInt() && (*c)->GetId().GetStr() == "score")
                score.Printf("%s raw score: %i", score.c_str(), (*c)->GetValue().GetInt());

            // E-value
            if ((*c)->GetValue().IsReal() && (*c)->GetId().GetStr() == "e_value"
                    && (*c)->GetValue().GetReal() > 0.0)
                score.Printf("%s E-value: %g", score.c_str(), (*c)->GetValue().GetReal());
        }
    }
    if (score.size() > 0) {
        bma->SetRowStatusLine(0, score.c_str());
        bma->SetRowStatusLine(1, score.c_str());
    }

    // success
    return bma.release();
}

static void FreezeBlocks(const BlockMultipleAlignment *multiple,
    const BlockMultipleAlignment *pairwise, Int4 *frozenBlocks)
{
    auto_ptr<BlockMultipleAlignment::UngappedAlignedBlockList>
        multipleABlocks(multiple->GetUngappedAlignedBlocks()),
        pairwiseABlocks(pairwise->GetUngappedAlignedBlocks());

    // if a block in the multiple is contained in the pairwise (looking at master coords),
    // then add a constraint to keep it there
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator
        m, me = multipleABlocks->end(), p, pe = pairwiseABlocks->end();
    const Block::Range *multipleRangeMaster, *pairwiseRangeMaster, *pairwiseRangeSlave;
    int i;
    for (i=0, m=multipleABlocks->begin(); m!=me; i++, m++) {
        multipleRangeMaster = (*m)->GetRangeOfRow(0);
        for (p=pairwiseABlocks->begin(); p!=pe; p++) {
            pairwiseRangeMaster = (*p)->GetRangeOfRow(0);
            if (pairwiseRangeMaster->from <= multipleRangeMaster->from &&
                    pairwiseRangeMaster->to >= multipleRangeMaster->to) {
                pairwiseRangeSlave = (*p)->GetRangeOfRow(1);
                frozenBlocks[i] = pairwiseRangeSlave->from +
                    (multipleRangeMaster->from - pairwiseRangeMaster->from);
                break;
            }
        }
        if (p == pe) frozenBlocks[i] = -1;
//        TESTMSG("block " << (i+1) << " frozen at query " << (frozenBlocks[i]+1));
    }
}

bool BlockAligner::CreateNewPairwiseAlignmentsByBlockAlignment(BlockMultipleAlignment *multiple,
    const AlignmentList& toRealign, AlignmentList *newAlignments, int *nRowsAddedToMultiple)
{
    // parameters passed to Alejandro's functions
    Int4 numBlocks;
    Uint1Ptr convertedQuery;
    Int4 queryLength;
    Int4 *blockStarts;
    Int4 *blockEnds;
    Int4 masterLength;
    BLAST_Score **thisScoreMat;
    Uint1Ptr masterSequence;
    Int4 *allowedGaps;
    SeqIdPtr subject_id;
    SeqIdPtr query_id;
    Int4 bestFirstBlock, bestLastBlock;
    SeqAlignPtr results;
    SeqAlignPtr listOfSeqAligns = NULL;

    // show options dialog each time block aligner is run
    SetOptions(NULL);

    // the following would be command-line arguments to Alejandro's standalone program
    Boolean localAlignment = currentOptions.globalAlignment ? FALSE : TRUE;
    BLAST_Score scoreThresholdSingleBlock =
        localAlignment ? currentOptions.singleBlockThreshold : NEG_INFINITY;
    BLAST_Score scoreThresholdMultipleBlock = currentOptions.multipleBlockThreshold;
    Nlm_FloatHi Lambda = currentOptions.lambda;
    Nlm_FloatHi K = currentOptions.K;
    Nlm_FloatHi percentile = currentOptions.gapLengthPercentile;
    Int4 gapAddition = currentOptions.allowedGapExtension;
    Int4 searchSpaceSize = currentOptions.searchSpaceSize;
    Nlm_FloatHi scaleMult = 1.0;
    Int4 startQueryPosition;
    Int4 endQueryPosition;

    newAlignments->clear();
    auto_ptr<BlockMultipleAlignment::UngappedAlignedBlockList> blocks(multiple->GetUngappedAlignedBlocks());
    numBlocks = blocks->size();
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks->end();
    if (nRowsAddedToMultiple) *nRowsAddedToMultiple = 0;

    // master sequence info
    masterLength = multiple->GetMaster()->Length();
    masterSequence = (Uint1Ptr) MemNew(sizeof(Uint1) * masterLength);
    int i;
    for (i=0; i<masterLength; i++)
        masterSequence[i] = ResToInt(multiple->GetMaster()->sequenceString[i]);
    subject_id = multiple->GetMaster()->parentSet->GetOrCreateBioseq(multiple->GetMaster())->id;

    // convert all sequences to Bioseqs
    multiple->GetMaster()->parentSet->CreateAllBioseqs(multiple);

    // create SeqAlign from this BlockMultipleAlignment
    listOfSeqAligns = multiple->CreateCSeqAlign();

    // set up block info
    blockStarts = (Int4*) MemNew(sizeof(Int4) * masterLength);
    blockEnds = (Int4*) MemNew(sizeof(Int4) * masterLength);
    allowedGaps = (Int4*) MemNew(sizeof(Int4) * (numBlocks - 1));
    for (i=0, b=blocks->begin(); b!=be; i++, b++) {
        const Block::Range *range = (*b)->GetRangeOfRow(0);
        blockStarts[i] = range->from;
        blockEnds[i] = range->to;
    }
    if (listOfSeqAligns) {
        findAllowedGaps(listOfSeqAligns, numBlocks, allowedGaps, percentile, gapAddition);
    } else {
        for (i=0; i<numBlocks-1; i++)
            allowedGaps[i] = blockStarts[i+1] - blockEnds[i] - 1 + gapAddition;
    }

    // set up PSSM
    BLAST_Matrix *matrix = BLASTer::CreateBLASTMatrix(multiple, NULL);
    thisScoreMat = matrix->matrix;

    Int4 *frozenBlocks = (Int4*) MemNew(numBlocks * sizeof(Int4));

    AlignmentList::const_iterator s, se = toRealign.end();
    for (s=toRealign.begin(); s!=se; s++) {
        if (multiple && (*s)->GetMaster() != multiple->GetMaster())
            ERR_POST(Error << "master sequence mismatch");

        // slave sequence info
        const Sequence *query = (*s)->GetSequenceOfRow(1);
        queryLength = query->Length();
        convertedQuery = (Uint1Ptr) MemNew(sizeof(Uint1) * queryLength);
        for (i=0; i<queryLength; i++)
            convertedQuery[i] = ResToInt(query->sequenceString[i]);
        query_id = query->parentSet->GetOrCreateBioseq(query)->id;
        startQueryPosition =
            ((*s)->alignFrom >= 0 && (*s)->alignFrom < query->Length()) ?
                (*s)->alignFrom : 0;
        endQueryPosition =
            ((*s)->alignTo >= 0 && (*s)->alignTo < query->Length()) ?
                ((*s)->alignTo + 1) : query->Length();

        // set frozen blocks
        if (currentOptions.alignAllBlocks)
            for (i=0; i<numBlocks; i++) frozenBlocks[i] = -1;
        else
            FreezeBlocks(multiple, *s, frozenBlocks);

        // actually do the block alignment
        TESTMSG("doing " << (localAlignment ? "local" : "global") << " block alignment of "
            << query->identifier->ToString());
        allocateAlignPieceMemory(numBlocks);
        findAlignPieces(convertedQuery, queryLength, startQueryPosition, endQueryPosition,
            numBlocks, blockStarts, blockEnds, masterLength, thisScoreMat,
            scoreThresholdSingleBlock, frozenBlocks, localAlignment);
        sortAlignPieces(numBlocks);
        results = makeMultiPieceAlignments(convertedQuery, numBlocks,
            queryLength, masterSequence, masterLength,
            blockStarts, blockEnds, allowedGaps, scoreThresholdMultipleBlock,
            subject_id, query_id, &bestFirstBlock, &bestLastBlock,
            Lambda, K, searchSpaceSize, localAlignment);

        // process results; assume first result SeqAlign is the highest scoring
        BlockMultipleAlignment *newAlignment;
        if (results) {
#ifdef _DEBUG
            AsnIoPtr aip = AsnIoOpen("seqalign-ba.txt", "w");
            SeqAlignAsnWrite(results, aip, NULL);
            AsnIoFree(aip, true);
#endif

            CSeq_align best;
            std::string err;
            if (!ConvertAsnFromCToCPP(results, (AsnWriteFunc) SeqAlignAsnWrite, &best, &err) ||
                (newAlignment=UnpackBlockAlignerSeqAlign(best, multiple->GetMaster(), query)) == NULL) {
                ERR_POST(Error << "conversion of results to C++ object failed: " << err);
            } else {

                if (currentOptions.mergeAfterEachSequence) {
                    if (multiple->MergeAlignment(newAlignment)) {
                        delete newAlignment; // if merge is successful, we can delete this alignment;
                        if (nRowsAddedToMultiple)
                            (*nRowsAddedToMultiple)++;
                        else
                            ERR_POST(Error << "BlockAligner::CreateNewPairwiseAlignmentsByBlockAlignment() "
                                "called with merge on, but NULL nRowsAddedToMultiple pointer");
                        // recalculate PSSM
                        BLAST_MatrixDestruct(matrix);
                        matrix = BLASTer::CreateBLASTMatrix(multiple, NULL);
                        thisScoreMat = matrix->matrix;

                    } else {                 // otherwise keep it
                        newAlignments->push_back(newAlignment);
                    }
                } else {
                    newAlignments->push_back(newAlignment);
                }
            }

            SeqAlignSetFree(results);
        }

        // no alignment block aligner failed - add old alignment to list so it doesn't get lost
        else {
            ERR_POST(Warning <<
                "block aligner found no significant alignment - current alignment unchanged");
            newAlignment = (*s)->Clone();
            newAlignment->SetRowDouble(0, -1.0);
            newAlignment->SetRowDouble(1, -1.0);
            newAlignment->SetRowStatusLine(0, "block aligner found no significant alignment");
            newAlignment->SetRowStatusLine(1, "block aligner found no significant alignment");
            newAlignments->push_back(newAlignment);
        }

        // cleanup
        MemFree(convertedQuery);
        freeBestPairs(numBlocks);
        freeAlignPieceLists(numBlocks);
        freeBestScores(numBlocks);
    }

    // cleanup
    BLAST_MatrixDestruct(matrix);
    MemFree(blockStarts);
    MemFree(blockEnds);
    MemFree(allowedGaps);
    if (listOfSeqAligns) SeqAlignSetFree(listOfSeqAligns);
    MemFree(masterSequence);
    MemFree(frozenBlocks);

    return true;
}

void BlockAligner::SetOptions(wxWindow* parent)
{
    BlockAlignerOptionsDialog dialog(parent, currentOptions);
    if (dialog.ShowModal() == wxOK)
        if (!dialog.GetValues(&currentOptions))
            ERR_POST(Error << "Error getting options from dialog!");
}


/////////////////////////////////////////////////////////////////////////////////////
////// BlockAlignerOptionsDialog stuff
////// taken (and modified) from block_aligner_dialog.wdr code
/////////////////////////////////////////////////////////////////////////////////////

#define ID_TEXT 10000
#define ID_T_SINGLE 10001
#define ID_S_SINGLE 10002
#define ID_T_MULT 10003
#define ID_S_MULT 10004
#define ID_T_EXT 10005
#define ID_S_EXT 10006
#define ID_T_PERCENT 10007
#define ID_S_PERCENT 10008
#define ID_T_LAMBDA 10009
#define ID_S_LAMBDA 10010
#define ID_T_K 10011
#define ID_S_K 10012
#define ID_T_SIZE 10013
#define ID_S_SIZE 10014
#define ID_C_GLOBAL 10015
#define ID_C_MERGE 10016
#define ID_C_ALIGN_ALL 10017
#define ID_B_OK 10018
#define ID_B_CANCEL 10019

BEGIN_EVENT_TABLE(BlockAlignerOptionsDialog, wxDialog)
    EVT_BUTTON(-1,  BlockAlignerOptionsDialog::OnButton)
    EVT_CLOSE (     BlockAlignerOptionsDialog::OnCloseWindow)
END_EVENT_TABLE()

BlockAlignerOptionsDialog::BlockAlignerOptionsDialog(
    wxWindow* parent, const BlockAligner::BlockAlignerOptions& init) :
        wxDialog(parent, -1, "Set Block Aligner Options", wxDefaultPosition, wxDefaultSize,
            wxCAPTION | wxSYSTEM_MENU) // not resizable
{
    wxPanel *panel = new wxPanel(this, -1);
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );
    wxStaticBox *item2 = new wxStaticBox( panel, -1, "Block Aligner Options" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );
    wxFlexGridSizer *item3 = new wxFlexGridSizer( 3, 0, 0 );
    item3->AddGrowableCol( 1 );

    wxStaticText *item4 = new wxStaticText( panel, ID_TEXT, "Single block score threshold:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item4, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    iSingle = new IntegerSpinCtrl(panel,
        0, 100, 1, init.singleBlockThreshold,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(iSingle->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(iSingle->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item7 = new wxStaticText( panel, ID_TEXT, "Multiple block score threshold:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item7, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    iMultiple = new IntegerSpinCtrl(panel,
        0, 100, 1, init.multipleBlockThreshold,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(iMultiple->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(iMultiple->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item10 = new wxStaticText( panel, ID_TEXT, "Allowed gap extension:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item10, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    iExtend = new IntegerSpinCtrl(panel,
        0, 100, 1, init.allowedGapExtension,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(iExtend->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(iExtend->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item13 = new wxStaticText( panel, ID_TEXT, "Gap length percentile:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item13, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    fpPercent = new FloatingPointSpinCtrl(panel,
        0.0, 1.0, 0.1, init.gapLengthPercentile,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(fpPercent->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(fpPercent->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    item3->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 5, 5, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item16 = new wxStaticText( panel, ID_TEXT, "Lambda:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item16, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    fpLambda = new FloatingPointSpinCtrl(panel,
        0.0, 10.0, 0.1, init.lambda,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(fpLambda->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(fpLambda->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item19 = new wxStaticText( panel, ID_TEXT, "K:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item19, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    fpK = new FloatingPointSpinCtrl(panel,
        0.0, 1.0, 0.01, init.K,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(fpK->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(fpK->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item22 = new wxStaticText( panel, ID_TEXT, "Search space size:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item22, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    iSize = new IntegerSpinCtrl(panel,
        0, kMax_Int, 1000, init.searchSpaceSize,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(iSize->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(iSize->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    item3->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item23 = new wxStaticText( panel, ID_TEXT, "Global alignment:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item23, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    cGlobal = new wxCheckBox( panel, ID_C_GLOBAL, "", wxDefaultPosition, wxDefaultSize, 0 );
    cGlobal->SetValue(init.globalAlignment);
    item3->Add( cGlobal, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 5, 5, 0, wxALIGN_CENTRE, 5 );

    wxStaticText *item28 = new wxStaticText( panel, ID_TEXT, "Merge after each row aligned:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item28, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    cMerge = new wxCheckBox( panel, ID_C_MERGE, "", wxDefaultPosition, wxDefaultSize, 0 );
    cMerge->SetValue(init.mergeAfterEachSequence);
    item3->Add( cMerge, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 5, 5, 0, wxALIGN_CENTRE, 5 );

    wxStaticText *item24 = new wxStaticText( panel, ID_TEXT, "Realign all blocks:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item24, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    cAlignAll = new wxCheckBox( panel, ID_C_ALIGN_ALL, "", wxDefaultPosition, wxDefaultSize, 0 );
    cAlignAll->SetValue(init.alignAllBlocks);
    item3->Add( cAlignAll, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 5, 5, 0, wxALIGN_CENTRE, 5 );

    item1->Add( item3, 0, wxALIGN_CENTRE, 5 );

    item0->Add( item1, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxBoxSizer *item25 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item26 = new wxButton( panel, ID_B_OK, "OK", wxDefaultPosition, wxDefaultSize, 0 );
    item25->Add( item26, 0, wxALIGN_CENTRE|wxALL, 5 );

    item25->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item27 = new wxButton( panel, ID_B_CANCEL, "Cancel", wxDefaultPosition, wxDefaultSize, 0 );
    item25->Add( item27, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item25, 0, wxALIGN_CENTRE|wxALL, 5 );

    panel->SetAutoLayout(true);
    panel->SetSizer(item0);
    item0->Fit(this);
    item0->Fit(panel);
    item0->SetSizeHints(this);
}

BlockAlignerOptionsDialog::~BlockAlignerOptionsDialog(void)
{
    delete iSingle;
    delete iMultiple;
    delete iExtend;
    delete iSize;
    delete fpPercent;
    delete fpLambda;
    delete fpK;
}

bool BlockAlignerOptionsDialog::GetValues(BlockAligner::BlockAlignerOptions *options)
{
    options->globalAlignment = cGlobal->IsChecked();
    options->mergeAfterEachSequence = cMerge->IsChecked();
    options->alignAllBlocks = cAlignAll->IsChecked();
    return (
        iSingle->GetInteger(&(options->singleBlockThreshold)) &&
        iMultiple->GetInteger(&(options->multipleBlockThreshold)) &&
        iExtend->GetInteger(&(options->allowedGapExtension)) &&
        iSize->GetInteger(&(options->searchSpaceSize)) &&
        fpPercent->GetDouble(&(options->gapLengthPercentile)) &&
        fpLambda->GetDouble(&(options->lambda)) &&
        fpK->GetDouble(&(options->K))
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
