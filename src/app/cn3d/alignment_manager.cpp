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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.31  2000/12/26 16:47:36  thiessen
* preserve block boundaries
*
* Revision 1.30  2000/12/19 16:39:08  thiessen
* tweaks to show/hide
*
* Revision 1.29  2000/12/15 15:51:46  thiessen
* show/hide system installed
*
* Revision 1.28  2000/11/30 15:49:34  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.27  2000/11/17 19:48:13  thiessen
* working show/hide alignment row
*
* Revision 1.26  2000/11/13 18:06:52  thiessen
* working structure re-superpositioning
*
* Revision 1.25  2000/11/12 04:02:59  thiessen
* working file save including alignment edits
*
* Revision 1.24  2000/11/11 21:15:53  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.23  2000/11/09 18:14:40  vasilche
* Fixed nonstandard behaviour of 'for' statement on MS VC.
*
* Revision 1.22  2000/11/03 01:12:44  thiessen
* fix memory problem with alignment cloning
*
* Revision 1.21  2000/11/02 16:56:00  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.20  2000/10/17 14:35:06  thiessen
* added row shift - editor basically complete
*
* Revision 1.19  2000/10/16 20:03:06  thiessen
* working block creation
*
* Revision 1.18  2000/10/16 14:25:47  thiessen
* working alignment fit coloring
*
* Revision 1.17  2000/10/12 19:20:45  thiessen
* working block deletion
*
* Revision 1.16  2000/10/12 16:22:45  thiessen
* working block split
*
* Revision 1.15  2000/10/12 02:14:56  thiessen
* working block boundary editing
*
* Revision 1.14  2000/10/05 18:34:43  thiessen
* first working editing operation
*
* Revision 1.13  2000/10/04 17:41:29  thiessen
* change highlight color (cell background) handling
*
* Revision 1.12  2000/10/02 23:25:19  thiessen
* working sequence identifier window in sequence viewer
*
* Revision 1.11  2000/09/20 22:22:26  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.10  2000/09/15 19:24:22  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.9  2000/09/12 01:47:38  thiessen
* fix minor but obscure bug
*
* Revision 1.8  2000/09/11 22:57:31  thiessen
* working highlighting
*
* Revision 1.7  2000/09/11 14:06:28  thiessen
* working alignment coloring
*
* Revision 1.6  2000/09/11 01:46:13  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.5  2000/09/08 20:16:55  thiessen
* working dynamic alignment views
*
* Revision 1.4  2000/09/03 18:46:47  thiessen
* working generalized sequence viewer
*
* Revision 1.3  2000/08/30 23:46:26  thiessen
* working alignment display
*
* Revision 1.2  2000/08/30 19:48:41  thiessen
* working sequence window
*
* Revision 1.1  2000/08/29 04:34:35  thiessen
* working alignment manager, IBM
*
* ===========================================================================
*/

#include <corelib/ncbidiag.hpp>

#include "cn3d/alignment_manager.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/alignment_set.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/sequence_viewer.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/show_hide_manager.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

///// AlignmentManager methods /////

AlignmentManager::AlignmentManager(const SequenceSet *sSet, const AlignmentSet *aSet) :
    sequenceViewer(NULL)
{
    NewAlignments(sSet, aSet);
}

AlignmentManager::~AlignmentManager(void)
{
    GlobalMessenger()->RemoveSequenceViewer(sequenceViewer);
    delete sequenceViewer;
}

void AlignmentManager::NewAlignments(const SequenceSet *sSet, const AlignmentSet *aSet)
{
    sequenceSet = sSet;
    alignmentSet = aSet;

    // create a sequence viewer for this alignment
    if (!sequenceViewer) {
        sequenceViewer = new SequenceViewer(this);
        GlobalMessenger()->AddSequenceViewer(sequenceViewer);
    }

    if (!alignmentSet) {
        sequenceViewer->DisplaySequences(&(sequenceSet->sequences));
        return;
    }

    // all slaves start out visible
    slavesVisible.resize(alignmentSet->alignments.size());
    for (int i=0; i<slavesVisible.size(); i++) slavesVisible[i] = true;
    NewMultipleWithRows(slavesVisible);
}

void AlignmentManager::SavePairwiseFromMultiple(const BlockMultipleAlignment *multiple)
{
    // create new AlignmentSet based on this multiple alignment, feed back into StructureSet
    AlignmentSet *newAlignmentSet = AlignmentSet::CreateFromMultiple(alignmentSet->parentSet, multiple);
    if (newAlignmentSet) {
        alignmentSet->parentSet->ReplaceAlignmentSet(newAlignmentSet);
        alignmentSet = newAlignmentSet;
    } else {
        ERR_POST(Warning << "Couldn't create pairwise alignments from the current multiple!\n"
            << "Alignment data in output file will be left unchanged.");
    }
}

const BlockMultipleAlignment * AlignmentManager::GetCurrentMultipleAlignment(void) const
{
    return sequenceViewer->GetCurrentAlignment();
}

static bool AlignedToAllSlaves(int masterResidue,
    const AlignmentManager::AlignmentList& alignments)
{
	AlignmentManager::AlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; a++) {
        if ((*a)->masterToSlave[masterResidue] == -1) return false;
    }
    return true;
}

static bool NoSlaveInsertionsBetween(int masterFrom, int masterTo,
    const AlignmentManager::AlignmentList& alignments)
{
	AlignmentManager::AlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; a++) {
        if (((*a)->masterToSlave[masterTo] - (*a)->masterToSlave[masterFrom]) !=
            (masterTo - masterFrom)) return false;
    }
    return true;
}

static bool NoBlockBoundariesBetween(int masterFrom, int masterTo,
    const AlignmentManager::AlignmentList& alignments)
{
	AlignmentManager::AlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; a++) {
        if ((*a)->blockStructure[masterTo] != (*a)->blockStructure[masterFrom])
            return false;
    }
    return true;
}

BlockMultipleAlignment *
AlignmentManager::CreateMultipleFromPairwiseWithIBM(const AlignmentList& alignments)
{
    AlignmentList::const_iterator a, ae = alignments.end();

    // create sequence list; fill with sequences of master + slaves
    BlockMultipleAlignment::SequenceList
        *sequenceList = new BlockMultipleAlignment::SequenceList(alignments.size() + 1);
    BlockMultipleAlignment::SequenceList::iterator s = sequenceList->begin();
    *(s++) = alignmentSet->master;
    for (a=alignments.begin(); a!=ae; a++, s++) *s = (*a)->slave;
    BlockMultipleAlignment *multipleAlignment = new BlockMultipleAlignment(sequenceList);

    // each block is a continuous region on the master, over which each master
    // residue is aligned to a residue of each slave, and where there are no
    // insertions relative to the master in any of the slaves
    int masterFrom = 0, masterTo, row;
    UngappedAlignedBlock *newBlock;

    while (masterFrom < alignmentSet->master->Length()) {

        // look for first all-aligned residue
        if (!AlignedToAllSlaves(masterFrom, alignments)) {
            masterFrom++;
            continue;
        }

        // find all next continuous all-aligned residues, but checking for
        // block boundaries from the original master-slave pairs, so that
        // blocks don't get merged
        for (masterTo=masterFrom+1;
                masterTo < alignmentSet->master->Length() &&
                AlignedToAllSlaves(masterTo, alignments) &&
                NoSlaveInsertionsBetween(masterFrom, masterTo, alignments) &&
                NoBlockBoundariesBetween(masterFrom, masterTo, alignments);
             masterTo++) ;
        masterTo--; // after loop, masterTo = first residue past block

        // create new block with ranges from master and all slaves
        newBlock = new UngappedAlignedBlock(multipleAlignment);
        newBlock->SetRangeOfRow(0, masterFrom, masterTo);
        newBlock->width = masterTo - masterFrom + 1;

        //TESTMSG("masterFrom " << masterFrom+1 << ", masterTo " << masterTo+1);
        for (a=alignments.begin(), row=1; a!=ae; a++, row++) {
            newBlock->SetRangeOfRow(row,
                (*a)->masterToSlave[masterFrom],
                (*a)->masterToSlave[masterTo]);
            //TESTMSG("slave->from " << b->from+1 << ", slave->to " << b->to+1);
        }

        // copy new block into alignment
        multipleAlignment->AddAlignedBlockAtEnd(newBlock);

        // start looking for next block
        masterFrom = masterTo + 1;
    }

    if (!multipleAlignment->AddUnalignedBlocks() ||
        !multipleAlignment->UpdateBlockMapAndConservationColors()) {
        ERR_POST(Error << "AlignmentManager::CreateMultipleFromPairwiseWithIBM() - "
            "error finalizing alignment");
        return NULL;
    }

    return multipleAlignment;
}

static void GetAlignedResidueIndexes(
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator& b,
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator& be,
    int row, int *seqIndexes)
{
    int i = 0, c;
    const Block::Range *range;
    for (; b!=be; b++) {
        range = (*b)->GetRangeOfRow(row);
        for (c=0; c<(*b)->width; c++) {
            seqIndexes[i++] = range->from + c;
        }
    }
}

void AlignmentManager::RealignAllSlaves(void) const
{
    const BlockMultipleAlignment *multiple = GetCurrentMultipleAlignment();
    if (!multiple) return;
    BlockMultipleAlignment::UngappedAlignedBlockList *blocks = multiple->GetUngappedAlignedBlocks();
    if (!blocks) {
        ERR_POST(Warning << "Can't realign slaves with no aligned residues!");
        return;
    }
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks->end();
    int nResidues = 0;
    for (b=blocks->begin(); b!=be; b++) nResidues += (*b)->width;
    if (nResidues <= 2) {
        ERR_POST(Warning << "Can't realign slaves with < 3 aligned residues!");
        delete blocks;
        return;
    }

    const Sequence *masterSeq = multiple->GetSequenceOfRow(0), *slaveSeq;
    const Molecule *masterMol, *slaveMol;
    if (!masterSeq || !(masterMol = masterSeq->molecule)) {
        ERR_POST(Warning << "Can't realign slaves to non-structured master!");
        delete blocks;
        return;
    }

    int *masterSeqIndexes = new int[nResidues], *slaveSeqIndexes = new int[nResidues];
    GetAlignedResidueIndexes(blocks->begin(), blocks->end(), 0, masterSeqIndexes);

    // for now, just use flat weighting
    double *weights = new double[nResidues];
    int i;
    for (i=0; i<nResidues; i++) weights[i] = 1;

    const StructureObject *slaveObj;

    typedef const Vector * CVP;
    CVP *masterCoords = new CVP[nResidues], *slaveCoords = new CVP[nResidues];
    if (!masterMol->GetAlphaCoords(nResidues, masterSeqIndexes, masterCoords)) {
        ERR_POST(Warning << "Can't get master alpha coords");
    } else {

        for (i=1; i<multiple->NRows(); i++) {
            slaveSeq = multiple->GetSequenceOfRow(i);
            if (!slaveSeq || !(slaveMol = slaveSeq->molecule)) continue;

            GetAlignedResidueIndexes(blocks->begin(), blocks->end(), i, slaveSeqIndexes);
            if (!slaveMol->GetAlphaCoords(nResidues, slaveSeqIndexes, slaveCoords)) continue;

            if (!slaveMol->GetParentOfType(&slaveObj)) continue;

            TESTMSG("realigning slave " << slaveSeq->pdbID << " against master " << masterSeq->pdbID);
            (const_cast<StructureObject*>(slaveObj))->
                RealignStructure(nResidues, masterCoords, slaveCoords, weights);
        }
    }

    delete blocks;
    delete masterSeqIndexes;
    delete slaveSeqIndexes;
    delete masterCoords;
    delete slaveCoords;
    delete weights;
    return;
}

void AlignmentManager::GetAlignmentSetSlaveSequences(std::vector < const Sequence * > *sequences) const
{
    sequences->resize(alignmentSet->alignments.size());

    AlignmentSet::AlignmentList::const_iterator a, ae = alignmentSet->alignments.end();
    int i = 0;
    for (a=alignmentSet->alignments.begin(); a!=ae; a++, i++) {
        sequences->at(i) = (*a)->slave;
    }
}

void AlignmentManager::GetAlignmentSetSlaveVisibilities(std::vector < bool > *visibilities) const
{
    // copy visibility list
    *visibilities = slavesVisible;
}

void AlignmentManager::SelectionCallback(const std::vector < bool >& itemsEnabled)
{
    if (itemsEnabled.size() != slavesVisible.size()) {
        ERR_POST(Error << "AlignmentManager::SelectionCallback() - wrong size list");
        return;
    }

    slavesVisible = itemsEnabled;
    NewMultipleWithRows(slavesVisible);

    // do necessary redraws + show/hides: sequences + chains in the alignment
    sequenceViewer->Refresh();

    AlignmentSet::AlignmentList::const_iterator
        a = alignmentSet->alignments.begin(), ae = alignmentSet->alignments.end();
    const StructureObject *object;

    if ((*a)->master->molecule) {
        // Show() redraws whole StructurObject only if necessary
        if ((*a)->master->molecule->GetParentOfType(&object))
            object->parentSet->showHideManager->ShowObject(object, true);
        // always redraw aligned molecule, in case alignment colors change
        GlobalMessenger()->PostRedrawMolecule((*a)->master->molecule);
    }
    for (int i=0; a!=ae; a++, i++) {
        if ((*a)->slave->molecule) {
            if ((*a)->slave->molecule->GetParentOfType(&object))
                object->parentSet->showHideManager->ShowObject(object, slavesVisible[i]);
            GlobalMessenger()->PostRedrawMolecule((*a)->slave->molecule);
        }
    }
}

void AlignmentManager::NewMultipleWithRows(const std::vector < bool >& visibilities)
{
    if (visibilities.size() != alignmentSet->alignments.size()) {
        ERR_POST(Error << "AlignmentManager::NewMultipleWithRows() - wrong size visibility vector");
        return;
    }

    // make a multiple from all visible rows
    AlignmentList alignments;
    AlignmentSet::AlignmentList::const_iterator a, ae=alignmentSet->alignments.end();
    int i = 0;
    for (a=alignmentSet->alignments.begin(); a!=ae; a++, i++) {
        if (visibilities[i])
            alignments.push_back(*a);
    }

    // sequenceViewer will own the resulting alignment
    sequenceViewer->DisplayAlignment(CreateMultipleFromPairwiseWithIBM(alignments));
}

bool AlignmentManager::IsAligned(const Sequence *sequence, int seqIndex) const
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
        for (int i=0; i<currentAlignment->sequences->size(); i++) {
            if (currentAlignment->sequences->at(i) == sequence)
                return true;
        }
    }
    return false;
}

const Vector * AlignmentManager::GetAlignmentColor(const Sequence *sequence, int seqIndex) const
{
    const BlockMultipleAlignment *currentAlignment = GetCurrentMultipleAlignment();
    if (currentAlignment)
        return currentAlignment->GetAlignmentColor(sequence, seqIndex);
    else
        return NULL;
}

END_SCOPE(Cn3D)

