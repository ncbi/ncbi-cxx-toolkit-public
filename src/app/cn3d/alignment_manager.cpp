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
*      Classes to manipulate alignments
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/scoremat/scoremat__.hpp>
#include <objects/cdd/Cdd_id.hpp>
#include <objects/cdd/Cdd_id_set.hpp>

#include "remove_header_conflicts.hpp"

#include "alignment_manager.hpp"
#include "sequence_set.hpp"
#include "alignment_set.hpp"
#include "block_multiple_alignment.hpp"
#include "messenger.hpp"
#include "structure_set.hpp"
#include "sequence_viewer.hpp"
#include "molecule.hpp"
#include "show_hide_manager.hpp"
#include "cn3d_threader.hpp"
#include "update_viewer.hpp"
#include "sequence_display.hpp"
#include "cn3d_tools.hpp"
#include "molecule_identifier.hpp"
#include "cn3d_blast.hpp"
#include "style_manager.hpp"
#include "cn3d_ba_interface.hpp"
#include "cn3d_pssm.hpp"

#include <algo/structure/bma_refine/Interface.hpp>
#include <algo/structure/bma_refine/InterfaceGUI.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

///// AlignmentManager methods /////

void AlignmentManager::Init(void)
{
    sequenceViewer = new SequenceViewer(this);
    GlobalMessenger()->AddSequenceViewer(sequenceViewer);

    updateViewer = new UpdateViewer(this);
    GlobalMessenger()->AddSequenceViewer(updateViewer);

    threader = new Threader(this);
    blaster = new BLASTer();
    blockAligner = new BlockAligner();
//    bmaRefiner = new BMARefiner();

    originalMultiple = NULL;
}

AlignmentManager::AlignmentManager(const SequenceSet *sSet, const AlignmentSet *aSet)
{
    Init();
    NewAlignments(sSet, aSet);
}

AlignmentManager::AlignmentManager(const SequenceSet *sSet,
    const AlignmentSet *aSet, const UpdateAlignList& updates)
{
    Init();
    NewAlignments(sSet, aSet);

    // create BlockMultipleAlignments from updates; add to update viewer
    PairwiseAlignmentList pairwise(1);
    UpdateViewer::AlignmentList updateAlignments;
    UpdateAlignList::const_iterator u, ue = updates.end();
    for (u=updates.begin(); u!=ue; ++u) {
        if (u->GetObject().IsSetSeqannot() && u->GetObject().GetSeqannot().GetData().IsAlign()) {
            CSeq_annot::C_Data::TAlign::const_iterator
                s, se = u->GetObject().GetSeqannot().GetData().GetAlign().end();
            for (s=u->GetObject().GetSeqannot().GetData().GetAlign().begin(); s!=se; ++s) {

                // determine master sequence
                const Sequence *master = NULL;
                if (aSet) {
                    master = aSet->master;
                } else if (updateAlignments.size() > 0) {
                    master = updateAlignments.front()->GetMaster();
                } else {
                    SequenceSet::SequenceList::const_iterator q, qe = sSet->sequences.end();
                    for (q=sSet->sequences.begin(); q!=qe; ++q) {
                        if ((*q)->identifier->MatchesSeqId(
                                (*s)->GetSegs().IsDendiag() ?
                                    (*s)->GetSegs().GetDendiag().front()->GetIds().front().GetObject() :
                                    (*s)->GetSegs().GetDenseg().GetIds().front().GetObject()
                            )) {
                            master = *q;
                            break;
                        }
                    }
                }
                if (!master) {
                    ERRORMSG("AlignmentManager::AlignmentManager() - "
                        << "can't determine master sequence for updates");
                    return;
                }

                const MasterDependentAlignment *alignment =
                    new MasterDependentAlignment(NULL, master, s->GetObject());
                pairwise.front() = alignment;
                BlockMultipleAlignment *multiple = CreateMultipleFromPairwiseWithIBM(pairwise);
                multiple->updateOrigin = *u;    // to keep track of which Update-align this came from
                updateAlignments.push_back(multiple);
                delete alignment;
            }
        }
    }
    updateViewer->AddAlignments(updateAlignments);

    // set this set of updates as the initial state of the editor's undo stack
    updateViewer->SetInitialState();
}

AlignmentManager::~AlignmentManager(void)
{
    GlobalMessenger()->RemoveSequenceViewer(sequenceViewer);
    GlobalMessenger()->RemoveSequenceViewer(updateViewer);
    delete sequenceViewer;
    delete updateViewer;
    delete threader;
    delete blaster;
    delete blockAligner;
//    delete bmaRefiner;
    if (originalMultiple) delete originalMultiple;
}

void AlignmentManager::NewAlignments(const SequenceSet *sSet, const AlignmentSet *aSet)
{
    sequenceSet = sSet;
    alignmentSet = aSet;

    if (!alignmentSet) {
        sequenceViewer->DisplaySequences(&(sequenceSet->sequences));
        return;
    }

    // all dependents start out visible
    dependentsVisible.resize(alignmentSet->alignments.size());
    for (unsigned int i=0; i<dependentsVisible.size(); ++i)
        dependentsVisible[i] = true;

    NewMultipleWithRows(dependentsVisible);
    originalMultiple = GetCurrentMultipleAlignment()->Clone();
    originalRowOrder.resize(originalMultiple->NRows());
    for (unsigned int r=0; r<originalMultiple->NRows(); ++r)
        originalRowOrder[r] = r;
}

void AlignmentManager::SavePairwiseFromMultiple(const BlockMultipleAlignment *multiple,
    const vector < unsigned int >& rowOrder)
{
    // create new AlignmentSet based on this multiple alignment, feed back into StructureSet
    AlignmentSet *newAlignmentSet =
        AlignmentSet::CreateFromMultiple(multiple->GetMaster()->parentSet, multiple, rowOrder);
    if (newAlignmentSet) {
        multiple->GetMaster()->parentSet->ReplaceAlignmentSet(newAlignmentSet);
        alignmentSet = newAlignmentSet;
    } else {
        ERRORMSG("Couldn't create pairwise alignments from the current multiple!\n"
            << "Alignment data in output file will be left unchanged.");
        return;
    }

    // see whether PSSM and/or row order have changed
    if (!originalMultiple) {
        originalMultiple = multiple->Clone();
        originalRowOrder = rowOrder;
        alignmentSet->parentSet->SetDataChanged(StructureSet::ePSSMData);
        alignmentSet->parentSet->SetDataChanged(StructureSet::eRowOrderData);
    } else {

        // check for row order change
        TRACEMSG("checking for row order changes..." << originalMultiple << ' ' << multiple);
        if (originalMultiple->NRows() != multiple->NRows()) {
            TRACEMSG("row order changed");
            alignmentSet->parentSet->SetDataChanged(StructureSet::eRowOrderData);
        } else {
            for (unsigned int row=0; row<originalMultiple->NRows(); ++row) {
                if (originalMultiple->GetSequenceOfRow(originalRowOrder[row]) !=
                        multiple->GetSequenceOfRow(rowOrder[row]) ||
                    originalRowOrder[row] != rowOrder[row])
                {
                    TRACEMSG("row order changed");
                    alignmentSet->parentSet->SetDataChanged(StructureSet::eRowOrderData);
                    break;
                }
            }
        }

        // check for PSSM change
        if ((multiple->HasNoAlignedBlocks() && !originalMultiple->HasNoAlignedBlocks()) ||
                (originalMultiple->HasNoAlignedBlocks() && !multiple->HasNoAlignedBlocks())) {
            TRACEMSG("PSSM changed - zero blocks before or after");
            alignmentSet->parentSet->SetDataChanged(StructureSet::ePSSMData);
        } else if (!multiple->HasNoAlignedBlocks() && !originalMultiple->HasNoAlignedBlocks()) {
            const CPssmWithParameters
                &originalPSSM = originalMultiple->GetPSSM().GetPSSM(),
                &currentPSSM = multiple->GetPSSM().GetPSSM();
            TRACEMSG("checking for PSSM changes... ");
            if (originalPSSM.GetPssm().GetNumRows() != currentPSSM.GetPssm().GetNumRows() ||
                originalPSSM.GetPssm().GetNumColumns() != currentPSSM.GetPssm().GetNumColumns() ||
                originalPSSM.GetPssm().GetByRow() != currentPSSM.GetPssm().GetByRow() ||
                !originalPSSM.GetPssm().IsSetFinalData() || !currentPSSM.GetPssm().IsSetFinalData() ||
                originalPSSM.GetPssm().GetFinalData().GetLambda() != currentPSSM.GetPssm().GetFinalData().GetLambda() ||
                originalPSSM.GetPssm().GetFinalData().GetKappa() != currentPSSM.GetPssm().GetFinalData().GetKappa() ||
                originalPSSM.GetPssm().GetFinalData().GetH() != currentPSSM.GetPssm().GetFinalData().GetH() ||
                originalPSSM.GetPssm().GetFinalData().GetScalingFactor() != currentPSSM.GetPssm().GetFinalData().GetScalingFactor()
            ) {
                TRACEMSG("PSSM changed");
                alignmentSet->parentSet->SetDataChanged(StructureSet::ePSSMData);
            } else {
                CPssmFinalData::TScores::const_iterator
                    o = originalPSSM.GetPssm().GetFinalData().GetScores().begin(),
                    oe = originalPSSM.GetPssm().GetFinalData().GetScores().end(),
                    c = currentPSSM.GetPssm().GetFinalData().GetScores().begin();
                for (; o!=oe; ++o, ++c) {
                    if ((*o) != (*c)) {
                        TRACEMSG("PSSM changed");
                        alignmentSet->parentSet->SetDataChanged(StructureSet::ePSSMData);
                        break;
                    }
                }
            }
        }

        // keep for comparison on next save
        delete originalMultiple;
        originalMultiple = multiple->Clone();
        originalMultiple->RemovePSSM();
        originalRowOrder = rowOrder;
    }
}

const BlockMultipleAlignment * AlignmentManager::GetCurrentMultipleAlignment(void) const
{
    const ViewerBase::AlignmentList& currentAlignments = sequenceViewer->GetCurrentAlignments();
    return ((currentAlignments.size() > 0) ? currentAlignments.front() : NULL);
}

static bool AlignedToAllDependents(int masterResidue,
    const AlignmentManager::PairwiseAlignmentList& alignments)
{
    AlignmentManager::PairwiseAlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; ++a) {
        if ((*a)->masterToDependent[masterResidue] == -1) return false;
    }
    return true;
}

static bool NoDependentInsertionsBetween(int masterFrom, int masterTo,
    const AlignmentManager::PairwiseAlignmentList& alignments)
{
    AlignmentManager::PairwiseAlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; ++a) {
        if (((*a)->masterToDependent[masterTo] - (*a)->masterToDependent[masterFrom]) != (masterTo - masterFrom))
            return false;
    }
    return true;
}

static bool NoBlockBoundariesBetween(int masterFrom, int masterTo,
    const AlignmentManager::PairwiseAlignmentList& alignments)
{
    AlignmentManager::PairwiseAlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; ++a) {
        if ((*a)->blockStructure[masterTo] != (*a)->blockStructure[masterFrom])
            return false;
    }
    return true;
}

BlockMultipleAlignment *
AlignmentManager::CreateMultipleFromPairwiseWithIBM(const PairwiseAlignmentList& alignments)
{
    PairwiseAlignmentList::const_iterator a, ae = alignments.end();

    // create sequence list; fill with sequences of master + dependents
    BlockMultipleAlignment::SequenceList
        *sequenceList = new BlockMultipleAlignment::SequenceList(alignments.size() + 1);
    BlockMultipleAlignment::SequenceList::iterator s = sequenceList->begin();
    *(s++) = alignments.front()->master;
    for (a=alignments.begin(); a!=ae; ++a) {
        *(s++) = (*a)->dependent;
        if ((*a)->master != sequenceList->front()) {
            ERRORMSG("AlignmentManager::CreateMultipleFromPairwiseWithIBM() -\n"
                << "all pairwise alignments must have the same master sequence");
            return NULL;
        }
    }
    BlockMultipleAlignment *multipleAlignment = new BlockMultipleAlignment(sequenceList, this);

    // each block is a continuous region on the master, over which each master
    // residue is aligned to a residue of each dependent, and where there are no
    // insertions relative to the master in any of the dependents
    int masterFrom = 0, masterTo;
    unsigned int row;
    UngappedAlignedBlock *newBlock;

    while (masterFrom < (int)multipleAlignment->GetMaster()->Length()) {

        // look for first all-aligned residue
        if (!AlignedToAllDependents(masterFrom, alignments)) {
            ++masterFrom;
            continue;
        }

        // find all next continuous all-aligned residues, but checking for
        // block boundaries from the original master-dependent pairs, so that
        // blocks don't get merged
        for (masterTo=masterFrom+1;
                masterTo < (int)multipleAlignment->GetMaster()->Length() &&
                AlignedToAllDependents(masterTo, alignments) &&
                NoDependentInsertionsBetween(masterFrom, masterTo, alignments) &&
                NoBlockBoundariesBetween(masterFrom, masterTo, alignments);
             ++masterTo) ;
        --masterTo; // after loop, masterTo = first residue past block

        // create new block with ranges from master and all dependents
        newBlock = new UngappedAlignedBlock(multipleAlignment);
        newBlock->SetRangeOfRow(0, masterFrom, masterTo);
        newBlock->width = masterTo - masterFrom + 1;

        //TESTMSG("masterFrom " << masterFrom+1 << ", masterTo " << masterTo+1);
        for (a=alignments.begin(), row=1; a!=ae; ++a, ++row) {
            newBlock->SetRangeOfRow(row,
                (*a)->masterToDependent[masterFrom],
                (*a)->masterToDependent[masterTo]);
            //TESTMSG("dependent->from " << b->from+1 << ", dependent->to " << b->to+1);
        }

        // copy new block into alignment
        multipleAlignment->AddAlignedBlockAtEnd(newBlock);

        // start looking for next block
        masterFrom = masterTo + 1;
    }

    if (!multipleAlignment->AddUnalignedBlocks() ||
        !multipleAlignment->UpdateBlockMapAndColors()) {
        ERRORMSG("AlignmentManager::CreateMultipleFromPairwiseWithIBM() - error finalizing alignment");
        return NULL;
    }

    return multipleAlignment;
}

static int GetAlignedResidueIndexes(
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator& b,
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator& be,
    int row, int *seqIndexes, bool countHighlights = false, const BlockMultipleAlignment *multiple = NULL)
{
    unsigned int i = 0, c, highlighted = 0;
    const Block::Range *range;
    for (; b!=be; ++b) {
        range = (*b)->GetRangeOfRow(row);
        for (c=0; c<(*b)->width; ++c, ++i) {
            seqIndexes[i] = range->from + c;
            if (countHighlights) {
                if (GlobalMessenger()->IsHighlighted(multiple->GetSequenceOfRow(row), seqIndexes[i]))
                    ++highlighted;
                else
                    seqIndexes[i] = -1;
            }
        }
    }
    return highlighted;
}

void AlignmentManager::RealignAllDependentStructures(bool highlightedOnly) const
{
    const BlockMultipleAlignment *multiple = GetCurrentMultipleAlignment();
    if (!multiple) return;
    BlockMultipleAlignment::UngappedAlignedBlockList blocks;
    multiple->GetUngappedAlignedBlocks(&blocks);
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks.end();
    int nResidues = 0;
    for (b=blocks.begin(); b!=be; ++b)
        nResidues += (*b)->width;
    if (nResidues == 0) {
        WARNINGMSG("Can't realign dependents with no aligned residues!");
        return;
    }

    const Sequence *masterSeq = multiple->GetSequenceOfRow(0), *dependentSeq;
    const Molecule *masterMol, *dependentMol;
    if (!masterSeq || !(masterMol = masterSeq->molecule)) {
        WARNINGMSG("Can't realign dependents to non-structured master!");
        return;
    }

    int *masterSeqIndexes = new int[nResidues], *dependentSeqIndexes = new int[nResidues];
    b = blocks.begin();
    int nHighlightedAligned = GetAlignedResidueIndexes(b, be, 0, masterSeqIndexes, highlightedOnly, multiple);
    if ((highlightedOnly ? nHighlightedAligned : nResidues) < 3) {
        WARNINGMSG("Can't realign dependents using < 3 residues!");
        delete[] masterSeqIndexes;
        delete[] dependentSeqIndexes;
        return;
    }

    double *weights = new double[nResidues];
    const StructureObject *dependentObj;

    typedef const Vector * CVP;
    CVP *masterCoords = new CVP[nResidues], *dependentCoords = new CVP[nResidues];
    if (!masterMol->GetAlphaCoords(nResidues, masterSeqIndexes, masterCoords)) {
        WARNINGMSG("Can't get master alpha coords");
    } else if (masterSeq->GetOrSetMMDBLink() == MoleculeIdentifier::VALUE_NOT_SET) {
        WARNINGMSG("Don't know master MMDB ID");
    } else {

        masterMol->parentSet->InitStructureAlignments(masterSeq->identifier->mmdbID);

        unsigned int nStructureAlignments = 0;
        for (unsigned int i=1; i<multiple->NRows(); ++i) {
            dependentSeq = multiple->GetSequenceOfRow(i);
            if (!dependentSeq || !(dependentMol = dependentSeq->molecule)) continue;

            b = blocks.begin();
            GetAlignedResidueIndexes(b, be, i, dependentSeqIndexes);
            if (dependentMol->GetAlphaCoords(nResidues, dependentSeqIndexes, dependentCoords) < 3) {
                ERRORMSG("can't realign dependent " << dependentSeq->identifier->pdbID << ", not enough coordinates in aligned region");
                continue;
            }

            if (!dependentMol->GetParentOfType(&dependentObj)) continue;

            // if any Vector* is NULL, make sure that weight is 0 so the pointer won't be accessed
            int nWeighted = 0;
            for (int j=0; j<nResidues; ++j) {
                if (!masterCoords[j] || !dependentCoords[j] || (highlightedOnly && masterSeqIndexes[j] < 0)) {
                    weights[j] = 0.0;
               } else {
                    weights[j] = 1.0; // for now, just use flat weighting
                    ++nWeighted;
               }
            }
            if (nWeighted < 3) {
                WARNINGMSG("Can't realign dependent #" << (i+1) << " using < 3 residues!");
                continue;
            }

            INFOMSG("realigning dependent " << dependentSeq->identifier->pdbID << " against master " << masterSeq->identifier->pdbID
                << " using coordinates of " << nWeighted << " residues");
            (const_cast<StructureObject*>(dependentObj))->RealignStructure(nResidues, masterCoords, dependentCoords, weights, i);
            ++nStructureAlignments;
        }

        // if no structure alignments, remove the list entirely
        if (nStructureAlignments == 0) masterMol->parentSet->RemoveStructureAlignments();
    }

    delete[] masterSeqIndexes;
    delete[] dependentSeqIndexes;
    delete[] masterCoords;
    delete[] dependentCoords;
    delete[] weights;
    return;
}

void AlignmentManager::GetAlignmentSetDependentSequences(vector < const Sequence * > *sequences) const
{
    sequences->resize(alignmentSet->alignments.size());

    AlignmentSet::AlignmentList::const_iterator a, ae = alignmentSet->alignments.end();
    int i = 0;
    for (a=alignmentSet->alignments.begin(); a!=ae; ++a, ++i) {
        (*sequences)[i] = (*a)->dependent;
    }
}

void AlignmentManager::GetAlignmentSetDependentVisibilities(vector < bool > *visibilities) const
{
    if (dependentsVisible.size() != alignmentSet->alignments.size()) // can happen if row is added/deleted
        dependentsVisible.resize(alignmentSet->alignments.size(), true);

    // copy visibility list
    *visibilities = dependentsVisible;
}

void AlignmentManager::ShowHideCallbackFunction(const vector < bool >& itemsEnabled)
{
    if (itemsEnabled.size() != dependentsVisible.size() ||
        itemsEnabled.size() != alignmentSet->alignments.size()) {
        ERRORMSG("AlignmentManager::ShowHideCallbackFunction() - wrong size list");
        return;
    }

    dependentsVisible = itemsEnabled;
    NewMultipleWithRows(dependentsVisible);

    AlignmentSet::AlignmentList::const_iterator
        a = alignmentSet->alignments.begin(), ae = alignmentSet->alignments.end();
    const StructureObject *object;

    if ((*a)->master->molecule) {
        // Show() redraws whole StructureObject only if necessary
        if ((*a)->master->molecule->GetParentOfType(&object))
            object->parentSet->showHideManager->Show(object, true);
        // always redraw aligned molecule, in case alignment colors change
        GlobalMessenger()->PostRedrawMolecule((*a)->master->molecule);
    }
    for (int i=0; a!=ae; ++a, ++i) {
        if ((*a)->dependent->molecule) {
            if ((*a)->dependent->molecule->GetParentOfType(&object))
                object->parentSet->showHideManager->Show(object, dependentsVisible[i]);
            GlobalMessenger()->PostRedrawMolecule((*a)->dependent->molecule);
        }
    }

    // do necessary redraws + show/hides: sequences + chains in the alignment
    sequenceViewer->Refresh();
    GlobalMessenger()->PostRedrawAllSequenceViewers();
    GlobalMessenger()->UnPostRedrawSequenceViewer(sequenceViewer);  // Refresh() does this already
}

void AlignmentManager::NewMultipleWithRows(const vector < bool >& visibilities)
{
    if (visibilities.size() != alignmentSet->alignments.size()) {
        ERRORMSG("AlignmentManager::NewMultipleWithRows() - wrong size visibility vector");
        return;
    }

    // make a multiple from all visible rows
    PairwiseAlignmentList alignments;
    AlignmentSet::AlignmentList::const_iterator a, ae=alignmentSet->alignments.end();
    int i = 0;
    for (a=alignmentSet->alignments.begin(); a!=ae; ++a, ++i) {
        if (visibilities[i])
            alignments.push_back(*a);
    }

    // sequenceViewer will own the resulting alignment
    sequenceViewer->DisplayAlignment(CreateMultipleFromPairwiseWithIBM(alignments));
}

bool AlignmentManager::IsAligned(const Sequence *sequence, unsigned int seqIndex) const
{
    const BlockMultipleAlignment *currentAlignment = GetCurrentMultipleAlignment();
    if (currentAlignment)
        return currentAlignment->IsAligned(sequence, seqIndex);
    else
        return false;
}

bool AlignmentManager::IsInAlignment(const Sequence *sequence) const
{
    if (!sequence) return false;
    const BlockMultipleAlignment *currentAlignment = GetCurrentMultipleAlignment();
    if (currentAlignment) {
        for (unsigned int i=0; i<currentAlignment->GetSequences()->size(); ++i) {
            if ((*(currentAlignment->GetSequences()))[i] == sequence)
                return true;
        }
    }
    return false;
}

const Vector * AlignmentManager::GetAlignmentColor(const Sequence *sequence, unsigned int seqIndex,
    StyleSettings::eColorScheme colorScheme) const
{
    const BlockMultipleAlignment *currentAlignment = GetCurrentMultipleAlignment();
    if (currentAlignment)
        return currentAlignment->GetAlignmentColor(sequence, seqIndex, colorScheme);
    else
        return NULL;
}

void AlignmentManager::ShowSequenceViewer(bool showNow) const
{
    sequenceViewer->CreateSequenceWindow(showNow);
}

void AlignmentManager::ShowUpdateWindow(void) const
{
    updateViewer->CreateUpdateWindow();
}

void AlignmentManager::RealignDependentSequences(
    BlockMultipleAlignment *multiple, const vector < unsigned int >& dependentsToRealign)
{
    if (!multiple || sequenceViewer->GetCurrentAlignments().size() == 0 ||
            multiple != sequenceViewer->GetCurrentAlignments().front()) {
        ERRORMSG("AlignmentManager::RealignDependentSequences() - wrong multiple alignment");
        return;
    }
    if (dependentsToRealign.size() == 0) return;

    // create alignments for each master/dependent pair, then update displays
    UpdateViewer::AlignmentList alignments;
    TRACEMSG("extracting rows");
    if (multiple->ExtractRows(dependentsToRealign, &alignments)) {
        TRACEMSG("recreating display");
        sequenceViewer->GetCurrentDisplay()->RowsRemoved(dependentsToRealign, multiple);
        TRACEMSG("adding to update window");
        SetDiagPostLevel(eDiag_Warning);    // otherwise, info messages take a long time if lots of rows
        updateViewer->AddAlignments(alignments);
        SetDiagPostLevel(eDiag_Info);
        TRACEMSG("done");
        updateViewer->CreateUpdateWindow();
    }
}

void AlignmentManager::ThreadUpdate(const ThreaderOptions& options, BlockMultipleAlignment *single)
{
    const ViewerBase::AlignmentList& currentAlignments = sequenceViewer->GetCurrentAlignments();
    if (currentAlignments.size() == 0) return;

    // run the threader on the given alignment
    UpdateViewer::AlignmentList singleList, replacedList;
    Threader::AlignmentList newAlignments;
    unsigned int nRowsAddedToMultiple;
    bool foundSingle = false;   // sanity check

    singleList.push_back(single);
    if (threader->Realign(
            options, currentAlignments.front(), &singleList, &newAlignments, &nRowsAddedToMultiple, sequenceViewer)) {

        // replace threaded alignment with new one(s) (or leftover where threader/merge failed)
        UpdateViewer::AlignmentList::const_iterator a, ae = updateViewer->GetCurrentAlignments().end();
        for (a=updateViewer->GetCurrentAlignments().begin(); a!=ae; ++a) {
            if (*a == single) {
                Threader::AlignmentList::const_iterator n, ne = newAlignments.end();
                for (n=newAlignments.begin(); n!=ne; ++n)
                    replacedList.push_back(*n);
                foundSingle = true;
            } else
                replacedList.push_back((*a)->Clone());
        }
        if (!foundSingle) ERRORMSG(
            "AlignmentManager::ThreadUpdate() - threaded alignment not found in update viewer!");
        updateViewer->ReplaceAlignments(replacedList);

        // tell the sequenceViewer that rows have been merged into the multiple
        if (nRowsAddedToMultiple > 0)
            sequenceViewer->GetCurrentDisplay()->RowsAdded(nRowsAddedToMultiple, currentAlignments.front());
    }
}

void AlignmentManager::ThreadAllUpdates(const ThreaderOptions& options)
{
    const ViewerBase::AlignmentList& currentAlignments = sequenceViewer->GetCurrentAlignments();
    if (currentAlignments.size() == 0) return;

    // run the threader on update pairwise alignments
    Threader::AlignmentList newAlignments;
    unsigned int nRowsAddedToMultiple;
    if (threader->Realign(
            options, currentAlignments.front(), &(updateViewer->GetCurrentAlignments()),
            &newAlignments, &nRowsAddedToMultiple, sequenceViewer)) {

        // replace update alignments with new ones (or leftovers where threader/merge failed)
        updateViewer->ReplaceAlignments(newAlignments);

        // tell the sequenceViewer that rows have been merged into the multiple
        if (nRowsAddedToMultiple > 0)
            sequenceViewer->GetCurrentDisplay()->
                RowsAdded(nRowsAddedToMultiple, currentAlignments.front());
    }
}

void AlignmentManager::MergeUpdates(const AlignmentManager::UpdateMap& updatesToMerge, bool mergeToNeighbor)
{
    if (updatesToMerge.size() == 0) return;
    const ViewerBase::AlignmentList& currentUpdates = updateViewer->GetCurrentAlignments();
    if (currentUpdates.size() == 0) return;

    // transform this structure view into an alignment view, and turn on the editor.
    ViewerBase::AlignmentList::const_iterator u, ue = currentUpdates.end();
    const BlockMultipleAlignment *newMultiple = NULL;
    if (sequenceViewer->GetCurrentAlignments().size() == 0) {

        for (u=currentUpdates.begin(); u!=ue; ++u) {   // find first update alignment
            if (updatesToMerge.find(*u) != updatesToMerge.end()) {
                newMultiple = *u;

                // create new alignment, then call SavePairwiseFromMultiple to create
                // an AlignmentSet and the initial ASN data
                sequenceViewer->DisplayAlignment(newMultiple->Clone());
                vector < unsigned int > rowOrder(newMultiple->NRows());
                for (unsigned int i=0; i<newMultiple->NRows(); ++i) rowOrder[i] = i;
                SavePairwiseFromMultiple(newMultiple, rowOrder);

                // editor needs to be on if >1 update is to be merged in
                sequenceViewer->TurnOnEditor();

                // set default alignment-type style
                newMultiple->GetMaster()->parentSet->styleManager->
                    SetGlobalRenderingStyle(StyleSettings::eTubeShortcut);
                newMultiple->GetMaster()->parentSet->styleManager->
                    SetGlobalColorScheme(StyleSettings::eAlignedShortcut);
                GlobalMessenger()->PostRedrawAllStructures();
                break;
            }
        }
    }

    BlockMultipleAlignment *multiple =
        (sequenceViewer->GetCurrentAlignments().size() > 0) ?
			sequenceViewer->GetCurrentAlignments().front() : NULL;
    if (!multiple) {
        ERRORMSG("Must have an alignment in the sequence viewer to merge with");
        return;
    }

    // make sure the editor is on in the sequenceViewer
    if (!sequenceViewer->EditorIsOn())
        sequenceViewer->TurnOnEditor();

    int nSuccessfulMerges = 0;
    ViewerBase::AlignmentList updatesToKeep;
    for (u=currentUpdates.begin(); u!=ue; ++u) {
        if (*u == newMultiple) continue;
        bool merged = false;
        if (updatesToMerge.find(*u) != updatesToMerge.end()) {
            merged = multiple->MergeAlignment(*u);
            if (merged) {
                nSuccessfulMerges += (*u)->NRows() - 1;
            } else {
                for (unsigned int i=0; i<(*u)->NRows(); ++i) {
                    string status = (*u)->GetRowStatusLine(i);
                    if (status.size() > 0)
                        status += "; merge failed!";
                    else
                        status = "Merge failed!";
                    (*u)->SetRowStatusLine(i, status);
                }
            }
        }
        if (!merged) {
            BlockMultipleAlignment *keep = (*u)->Clone();
            updatesToKeep.push_back(keep);
        }
    }

    updateViewer->ReplaceAlignments(updatesToKeep);
    if (nSuccessfulMerges > 0) {
        int where = -1;

        // if necessary, find nearest neighbor to merged sequence, and add new row after it
        if (mergeToNeighbor && nSuccessfulMerges == 1) {
            BlockMultipleAlignment::UngappedAlignedBlockList blocks;
            multiple->GetUngappedAlignedBlocks(&blocks);
            BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks.end();
            unsigned int col, row, rowScore, bestScore=0, lastRow = multiple->NRows() - 1;
            const Sequence *mergeSeq = multiple->GetSequenceOfRow(lastRow);
            for (row=0; row<lastRow; ++row) {
                const Sequence *otherSeq = multiple->GetSequenceOfRow(row);
                rowScore = 0;
                for (b=blocks.begin(); b!=be; ++b) {
                    for (col=0; col<(*b)->width; ++col) {
                        rowScore += GetBLOSUM62Score(
                            mergeSeq->sequenceString[(*b)->GetRangeOfRow(lastRow)->from + col],
                            otherSeq->sequenceString[(*b)->GetRangeOfRow(row)->from + col]);
                    }
                }
                if (row == 0 || rowScore > bestScore) {
                    where = row;
                    bestScore = rowScore;
                }
            }
            INFOMSG("Closest row is #" << (where+1) << ", "
                << multiple->GetSequenceOfRow(where)->identifier->ToString());
        }

        sequenceViewer->GetCurrentDisplay()->RowsAdded(nSuccessfulMerges, multiple, where);
    }

    // add pending imported structures to asn data
    updateViewer->SavePendingStructures();
}

void AlignmentManager::CalculateRowScoresWithThreader(double weightPSSM)
{
    threader->CalculateScores(GetCurrentMultipleAlignment(), weightPSSM);
}

unsigned int AlignmentManager::NUpdates(void) const
{
    return updateViewer->GetCurrentAlignments().size();
}

void AlignmentManager::GetUpdateSequences(list < const Sequence * > *updateSequences) const
{
    updateSequences->clear();
    const ViewerBase::AlignmentList& currentUpdates = updateViewer->GetCurrentAlignments();
    if (currentUpdates.size() == 0) return;
    ViewerBase::AlignmentList::const_iterator u, ue = currentUpdates.end();
    for (u=currentUpdates.begin(); u!=ue; ++u)
        updateSequences->push_back((*u)->GetSequenceOfRow(1));  // assume update aln has just one dependent...
}

bool AlignmentManager::GetStructureProteins(vector < const Sequence * > *chains) const
{
    if (!chains || GetCurrentMultipleAlignment() != NULL ||
        !sequenceViewer || !sequenceViewer->GetCurrentDisplay()) return false;

    sequenceViewer->GetCurrentDisplay()->GetProteinSequences(chains);
    return (chains->size() > 0);
}

void AlignmentManager::ReplaceUpdatesInASN(ncbi::objects::CCdd::TPending& newUpdates) const
{
    if (sequenceSet)
        sequenceSet->parentSet->ReplaceUpdates(newUpdates);
    else
        ERRORMSG("AlignmentManager::ReplaceUpdatesInASN() - can't get StructureSet");
}

void AlignmentManager::PurgeSequence(const MoleculeIdentifier *identifier)
{
    BlockMultipleAlignment *multiple =
        (sequenceViewer->GetCurrentAlignments().size() > 0) ?
			sequenceViewer->GetCurrentAlignments().front() : NULL;
    if (!multiple) return;

    // remove matching rows from multiple alignment
    vector < unsigned int > rowsToRemove;
    for (unsigned int i=1; i<multiple->NRows(); ++i)
        if (multiple->GetSequenceOfRow(i)->identifier == identifier)
            rowsToRemove.push_back(i);

    if (rowsToRemove.size() > 0) {

        // turn on editor, and update multiple pointer
        if (!sequenceViewer->EditorIsOn()) {
            sequenceViewer->TurnOnEditor();
            multiple = sequenceViewer->GetCurrentAlignments().front();
        }

        if (!multiple->ExtractRows(rowsToRemove, NULL)) {
            ERRORMSG("AlignmentManager::PurgeSequence() - ExtractRows failed!");
            return;
        }

        // remove rows from SequenceDisplay
        SequenceDisplay *display = sequenceViewer->GetCurrentDisplay();
        if (!display) {
            ERRORMSG("AlignmentManager::PurgeSequence() - can't get SequenceDisplay!");
            return;
        }
        display->RowsRemoved(rowsToRemove, multiple);
    }

    // remove matching alignments from Update window
    const ViewerBase::AlignmentList& currentUpdates = updateViewer->GetCurrentAlignments();
    if (currentUpdates.size() == 0) return;
    ViewerBase::AlignmentList::const_iterator u, ue = currentUpdates.end();

    for (u=currentUpdates.begin(); u!=ue; ++u) // quick check if any match found
        if ((*u)->GetSequenceOfRow(1)->identifier == identifier) break;

    if (u != ue) {
        ViewerBase::AlignmentList updatesToKeep;
        for (u=currentUpdates.begin(); u!=ue; ++u) {
            if ((*u)->GetSequenceOfRow(1)->identifier != identifier) {
                BlockMultipleAlignment *keep = (*u)->Clone();
                updatesToKeep.push_back(keep);
            }
        }
        updateViewer->ReplaceAlignments(updatesToKeep);
    }
}

void AlignmentManager::BlockAlignAllUpdates(void)
{
    BlockMultipleAlignment *currentMultiple =
        (sequenceViewer->GetCurrentAlignments().size() > 0) ?
			sequenceViewer->GetCurrentAlignments().front() : NULL;
    if (!currentMultiple) return;

    const UpdateViewer::AlignmentList& currentUpdates = updateViewer->GetCurrentAlignments();
    if (currentUpdates.size() == 0)
        return;

    // run the block aligner on update pairwise alignments
    BlockAligner::AlignmentList newAlignmentsList;
    int nRowsAddedToMultiple;

    if (blockAligner->CreateNewPairwiseAlignmentsByBlockAlignment(currentMultiple,
            currentUpdates, &newAlignmentsList, &nRowsAddedToMultiple, sequenceViewer)) {

        // replace update alignments with new ones (or leftovers where algorithm failed)
        updateViewer->ReplaceAlignments(newAlignmentsList);

        // tell the sequenceViewer that rows have been merged into the multiple
        if (nRowsAddedToMultiple > 0)
            sequenceViewer->GetCurrentDisplay()->RowsAdded(nRowsAddedToMultiple, currentMultiple);
    }
}

void AlignmentManager::BlockAlignUpdate(BlockMultipleAlignment *single)
{
    BlockMultipleAlignment *currentMultiple =
        (sequenceViewer->GetCurrentAlignments().size() > 0) ?
			sequenceViewer->GetCurrentAlignments().front() : NULL;
    if (!currentMultiple) return;

    // run the block aligner on the given alignment
    UpdateViewer::AlignmentList singleList, replacedList;
    BlockAligner::AlignmentList newAlignments;
    int nRowsAddedToMultiple;
    bool foundSingle = false;   // sanity check

    singleList.push_back(single);
    if (blockAligner->CreateNewPairwiseAlignmentsByBlockAlignment(
            currentMultiple, singleList, &newAlignments, &nRowsAddedToMultiple, sequenceViewer)) {

        // replace threaded alignment with new one
        UpdateViewer::AlignmentList::const_iterator a, ae = updateViewer->GetCurrentAlignments().end();
        for (a=updateViewer->GetCurrentAlignments().begin(); a!=ae; ++a) {
            if (*a == single) {
                BlockAligner::AlignmentList::const_iterator n, ne = newAlignments.end();
                for (n=newAlignments.begin(); n!=ne; ++n)
                    replacedList.push_back(*n);
                foundSingle = true;
            } else
                replacedList.push_back((*a)->Clone());
        }
        if (!foundSingle) ERRORMSG(
            "AlignmentManager::BlockAlignUpdate() - changed alignment not found in update viewer!");
        updateViewer->ReplaceAlignments(replacedList);

        // tell the sequenceViewer that rows have been merged into the multiple
        if (nRowsAddedToMultiple > 0)
            sequenceViewer->GetCurrentDisplay()->RowsAdded(nRowsAddedToMultiple, currentMultiple);
    }
}

typedef struct {
    unsigned int multBlock, pairBlock;
    bool extendPairBlockLeft;
    int nResidues;
} ExtendInfo;

static bool CreateNewPairwiseAlignmentsByBlockExtension(const BlockMultipleAlignment& multiple,
    const UpdateViewer::AlignmentList& toExtend, UpdateViewer::AlignmentList *newAlignments)
{
    if (multiple.HasNoAlignedBlocks())
        return false;

    bool anyChanges = false;

    BlockMultipleAlignment::UngappedAlignedBlockList multBlocks, pairBlocks;
    multiple.GetUngappedAlignedBlocks(&multBlocks);

    newAlignments->clear();
    UpdateViewer::AlignmentList::const_iterator t, te = toExtend.end();
    for (t=toExtend.begin(); t!=te; ++t) {
        BlockMultipleAlignment *p = (*t)->Clone();
        newAlignments->push_back(p);

        pairBlocks.clear();
        p->GetUngappedAlignedBlocks(&pairBlocks);
        if (pairBlocks.size() == 0)
            continue;

        typedef list < ExtendInfo > ExtendList;
        typedef map < const UnalignedBlock *, ExtendList > ExtendMap;
        ExtendMap extendMap;

        // find cases where a pairwise block ends inside a multiple block
        const UnalignedBlock *ub;
        for (unsigned int mb=0; mb<multBlocks.size(); ++mb) {
            const Block::Range *multRange = multBlocks[mb]->GetRangeOfRow(0);
            for (unsigned int pb=0; pb<pairBlocks.size(); ++pb) {
                const Block::Range *pairRange = pairBlocks[pb]->GetRangeOfRow(0);

                if (pairRange->from > multRange->from && pairRange->from <= multRange->to &&
                    (ub = p->GetUnalignedBlockBefore(pairBlocks[pb])) != NULL)
                {
                    ExtendList& el = extendMap[ub];
                    el.resize(el.size() + 1);
                    el.back().multBlock = mb;
                    el.back().pairBlock = pb;
                    el.back().extendPairBlockLeft = true;
                    el.back().nResidues = pairRange->from - multRange->from;
                    TRACEMSG("block " << (pb+1) << " wants to be extended to the left");
                }

                if (pairRange->to < multRange->to && pairRange->to >= multRange->from &&
                    (ub = p->GetUnalignedBlockAfter(pairBlocks[pb])) != NULL)
                {
                    ExtendList& el = extendMap[ub];
                    el.resize(el.size() + 1);
                    el.back().multBlock = mb;
                    el.back().pairBlock = pb;
                    el.back().extendPairBlockLeft = false;
                    el.back().nResidues = multRange->to - pairRange->to;
                    TRACEMSG("block " << (pb+1) << " wants to be extended to the right");
                }
            }
        }

        ExtendMap::const_iterator u, ue = extendMap.end();
        for (u=extendMap.begin(); u!=ue; ++u) {

            // don't slurp residues into non-adjacent aligned blocks from the multiple
            if (u->second.size() == 2 && (u->second.back().multBlock - u->second.front().multBlock > 1)) {
                TRACEMSG("can't extend with intervening block(s) in multiple between blocks "
                    << (u->second.front().pairBlock+1) << " and " << (u->second.back().pairBlock+1));
                continue;
            }

            ExtendList::const_iterator e, ee;

            // extend from one side only if an unaligned block is contained entirely within a block in the multiple,
            // and this unaligned block is "complete" - that is, MinResidues == width.
            if (u->first->MinResidues() == u->first->width && u->second.size() == 2 &&
                u->second.front().multBlock == u->second.back().multBlock)
            {
                ExtendList& modList = const_cast<ExtendList&>(u->second);
                modList.front().nResidues = u->first->width;
                modList.erase(++(modList.begin()));
            }

            else {
                // check to see if there are sufficient unaligned residues to extend from both sides
                int available = u->first->MinResidues(), totalShifts = 0;
                for (e=u->second.begin(), ee=u->second.end(); e!=ee; ++e)
                    totalShifts += e->nResidues;
                if (totalShifts > available) {
                    TRACEMSG("inadequate residues to the "
                        << (u->second.front().extendPairBlockLeft ? "left" : "right")
                        << " of block " << (u->second.front().pairBlock+1) << "; no extension performed");
                    continue;
                }
            }

            // perform any allowed extensions
            for (e=u->second.begin(), ee=u->second.end(); e!=ee; ++e) {
                int alnIdx = p->GetAlignmentIndex(0,
                    (e->extendPairBlockLeft ?
                        pairBlocks[e->pairBlock]->GetRangeOfRow(0)->from :
                        pairBlocks[e->pairBlock]->GetRangeOfRow(0)->to),
                    BlockMultipleAlignment::eLeft); // justification is irrelevant since this is an aligned block
                TRACEMSG("extending " << (e->extendPairBlockLeft ? "left" : "right")
                    << " side of block " << (e->pairBlock+1) << " by " << e->nResidues << " residues");
                if (p->MoveBlockBoundary(alnIdx, alnIdx + (e->extendPairBlockLeft ? -e->nResidues : e->nResidues)))
                    anyChanges = true;
                else
                    ERRORMSG("MoveBlockBoundary() failed!");
            }
        }

        // now, merge any adjacent blocks that end within a block of the multiple
        const Block::Range *prevRange = NULL;
        vector < int > mergeIndexes;
        for (unsigned int pb=0; pb<pairBlocks.size(); ++pb) {
            const Block::Range *pairRange = pairBlocks[pb]->GetRangeOfRow(0);
            if (prevRange && prevRange->to == pairRange->from - 1 && multiple.IsAligned(0U, prevRange->to)) {
                // justification is irrelevant since this is an aligned block
                int mAlnIdx1 = multiple.GetAlignmentIndex(0, prevRange->to, BlockMultipleAlignment::eLeft),
                    mAlnIdx2 = multiple.GetAlignmentIndex(0, pairRange->from, BlockMultipleAlignment::eLeft),
                    pAlnIdx1 = p->GetAlignmentIndex(0, prevRange->to, BlockMultipleAlignment::eLeft),
                    pAlnIdx2 = p->GetAlignmentIndex(0, pairRange->from, BlockMultipleAlignment::eLeft);
                if (pAlnIdx1 == pAlnIdx2 - 1 &&
                    multiple.GetAlignedBlockNumber(mAlnIdx1) == multiple.GetAlignedBlockNumber(mAlnIdx2))
                {
                    TRACEMSG("merging blocks " << pb << " and " << (pb+1));
                    // just flag to merge later, since actually merging would mess up pb block indexes
                    mergeIndexes.push_back(pAlnIdx1);
                }
            }
            prevRange = pairRange;
        }
        for (unsigned int i=0; i<mergeIndexes.size(); ++i) {
            if (p->MergeBlocks(mergeIndexes[i], mergeIndexes[i] + 1))
                anyChanges = true;
            else
                ERRORMSG("MergeBlocks() failed!");
        }
    }

    return anyChanges;
}

void AlignmentManager::ExtendAllUpdates(void)
{
    BlockMultipleAlignment *currentMultiple =
        (sequenceViewer->GetCurrentAlignments().size() > 0) ?
            sequenceViewer->GetCurrentAlignments().front() : NULL;
    if (!currentMultiple) return;

    const UpdateViewer::AlignmentList& currentUpdates = updateViewer->GetCurrentAlignments();
    if (currentUpdates.size() == 0)
        return;

    // run the block extender on update pairwise alignments
    UpdateViewer::AlignmentList newAlignmentsList;
    if (CreateNewPairwiseAlignmentsByBlockExtension(*currentMultiple, currentUpdates, &newAlignmentsList))
        updateViewer->ReplaceAlignments(newAlignmentsList);
}

void AlignmentManager::ExtendUpdate(BlockMultipleAlignment *single)
{
    BlockMultipleAlignment *currentMultiple =
        (sequenceViewer->GetCurrentAlignments().size() > 0) ?
            sequenceViewer->GetCurrentAlignments().front() : NULL;
    if (!currentMultiple) return;

    // run the threader on the given alignment
    UpdateViewer::AlignmentList singleList, replacedList;
    BlockAligner::AlignmentList newAlignments;

    singleList.push_back(single);
    if (CreateNewPairwiseAlignmentsByBlockExtension(*currentMultiple, singleList, &newAlignments)) {
        if (newAlignments.size() != 1) {
            ERRORMSG("AlignmentManager::ExtendUpdate() - returned alignment list size != 1!");
            return;
        }

        // replace original alignment with extended one
        UpdateViewer::AlignmentList::const_iterator a, ae = updateViewer->GetCurrentAlignments().end();
        bool foundSingle = false;   // sanity check
        for (a=updateViewer->GetCurrentAlignments().begin(); a!=ae; ++a) {
            if (*a == single) {
                replacedList.push_back(newAlignments.front());
                foundSingle = true;
            } else
                replacedList.push_back((*a)->Clone());
        }
        if (!foundSingle)
            ERRORMSG("AlignmentManager::ExtendUpdate() - changed alignment not found in update viewer!");
        updateViewer->ReplaceAlignments(replacedList);
    }
}

void AlignmentManager::RefineAlignment(bool setUpOptionsOnly)
{
    // TODO:  selection dialog for realigning specific rows
    // <can add this afterwards; refiner supports this sort of thing but need to improve api>

    // get info on current multiple alignment
    const BlockMultipleAlignment *multiple = GetCurrentMultipleAlignment();
    if (!multiple || multiple->NRows() < 2 || multiple->HasNoAlignedBlocks()) {
        ERRORMSG("AlignmentManager::RefineAlignment() - can't refine alignments with < 2 rows or zero aligned blocks");
        return;
    }

    // construct CDD from inputs
    CRef < CCdd > cdd(new CCdd);
    cdd->SetName("CDD input to refiner from Cn3D");
    CRef < CCdd_id > uid(new CCdd_id);
    uid->SetUid(0);
    cdd->SetId().Set().push_back(uid);

    // construct Seq-entry from sequences in current alignment, starting with master
    CRef < CSeq_entry > seq(new CSeq_entry);
    seq->SetSeq().Assign(multiple->GetMaster()->bioseqASN.GetObject());
    cdd->SetSequences().SetSet().SetSeq_set().push_back(seq);

    // construct Seq-annot from rows in the alignment
    CRef < CSeq_annot > seqAnnot(new CSeq_annot);
    cdd->SetSeqannot().push_back(seqAnnot);
    BlockMultipleAlignment::UngappedAlignedBlockList blocks;
    multiple->GetUngappedAlignedBlocks(&blocks);
    vector < unsigned int > rowOrder;
    sequenceViewer->GetCurrentDisplay()->GetRowOrder(multiple, &rowOrder);

    // other info used by the refiner
    vector < string > rowTitles(multiple->NRows());
    vector < bool > rowIsStructured(multiple->NRows());
    vector < bool > blocksToRealign;

    // fill out Seq-entry and Seq-annot based on current row ordering of the display (which may be different from BMA)
    for (unsigned int i=1; i<multiple->NRows(); ++i) {
        seq.Reset(new CSeq_entry);
        seq->SetSeq().Assign(multiple->GetSequenceOfRow(rowOrder[i])->bioseqASN.GetObject());
        cdd->SetSequences().SetSet().SetSeq_set().push_back(seq);
        CRef < CSeq_align > seqAlign(CreatePairwiseSeqAlignFromMultipleRow(multiple, blocks, rowOrder[i]));
        seqAnnot->SetData().SetAlign().push_back(seqAlign);
        rowTitles[i] = multiple->GetSequenceOfRow(rowOrder[i])->identifier->ToString();
        rowIsStructured[i] = (multiple->GetSequenceOfRow(rowOrder[i])->identifier->pdbID.size() > 0);
    }

    // set blocks to realign, using marked blocks, if any; no marks -> realign all blocks
    vector < unsigned int > marks;
    multiple->GetMarkedBlockNumbers(&marks);
    blocksToRealign.resize(blocks.size(), (marks.size() == 0));
    for (unsigned int i=0; i<marks.size(); ++i) {
        TRACEMSG("refining block " << (marks[i]+1));
        blocksToRealign[marks[i]] = true;
    }

    // set up refiner
    align_refine::BMARefinerInterface interface;
    static auto_ptr < align_refine::BMARefinerOptions > options;
    if ((options.get() && !interface.SetOptions(*options)) ||   // set these first since some are overridden by alignment inputs
        !interface.SetInitialAlignment(*cdd, blocks.size(), multiple->NRows()) ||
        !interface.SetRowTitles(rowTitles) ||
        !interface.SetRowsWithStructure(rowIsStructured) ||
        !interface.SetBlocksToRealign(blocksToRealign))
    {
        ERRORMSG("AlignmentManager::RefineAlignment() - failed to set up BMARefinerInterface");
        return;
    }
    if (!options.get() || setUpOptionsOnly) {
        if (!align_refine::BMARefinerOptionsDialog::SetRefinerOptionsViaDialog(NULL, interface))
        {
            WARNINGMSG("AlignmentManager::RefineAlignment() - failed to set refiner options via dialog, probably cancelled");
            return;
        }
        options.reset(interface.GetOptions());
    }
    if (setUpOptionsOnly)
        return;

    // actually run the refiner algorithm
    CCdd::TSeqannot results;
    SetDialogSevereErrors(false);
    bool okay = interface.Run(results);
    SetDialogSevereErrors(true);
    SetDiagPostLevel(eDiag_Info);   // may be changed by refiner

    if (okay) {

        // unpack results
        if (results.size() > 0 && results.front().NotEmpty()) {

            // just use first returned alignment for now; convert asn data into BlockMultipleAlignment
            PairwiseAlignmentList pairs;
            if (!results.front()->IsSetData() || !results.front()->GetData().IsAlign()) {
                WARNINGMSG("AlignmentManager::RefineAlignment() - got result Seq-annot in unexpected format");
            } else {
                CSeq_annot::C_Data::TAlign::const_iterator l, le = results.front()->GetData().GetAlign().end();
                for (l=results.front()->GetData().GetAlign().begin(); l!=le; ++l)
                    pairs.push_back(new MasterDependentAlignment(NULL, multiple->GetMaster(), **l));
            }
            auto_ptr < BlockMultipleAlignment > refined(CreateMultipleFromPairwiseWithIBM(pairs));
            DELETE_ALL_AND_CLEAR(pairs, PairwiseAlignmentList);

            // feed result back into alignment window
            if (refined.get() && refined->NRows() == multiple->NRows())
                sequenceViewer->ReplaceAlignment(multiple, refined.release());
            else
                ERRORMSG("AlignmentManager::RefineAlignment() - problem converting refinement result");

        } else {
            WARNINGMSG("AlignmentManager::RefineAlignment() - failed to make a refined alignment. Alignment unchanged.");
        }

    } else {
        ERRORMSG("AlignmentManager::RefineAlignment() - refinement failed. Alignment unchanged.");
    }
}

END_SCOPE(Cn3D)
