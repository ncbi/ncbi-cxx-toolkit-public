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
#include "cn3d/sequence_viewer.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

AlignmentManager::AlignmentManager(const SequenceSet *sSet, const AlignmentSet *aSet,
    SequenceViewer * const *seqViewer) :
    sequenceSet(sSet), alignmentSet(aSet), currentMultipleAlignment(NULL),
    sequenceViewer(seqViewer)
{
    if (!alignmentSet) {
        (*sequenceViewer)->DisplaySequences(&(sequenceSet->sequences));
        return;
    }

    // for testing, make a multiple with all rows
    AlignmentList alignments;
    AlignmentSet::AlignmentList::const_iterator a, ae=alignmentSet->alignments.end();
    for (a=alignmentSet->alignments.begin(); a!=ae; a++) alignments.push_back(*a);
    CreateMultipleFromPairwiseWithIBM(alignments);

    // crudely print out blocks
    MultipleAlignment::BlockList::const_iterator block, be = currentMultipleAlignment->blocks.end();
    MultipleAlignment::Block::const_iterator range, re;
    MultipleAlignment::SequenceList::const_iterator seq;
    TESTMSG("MultipleAlignment:");
    for (block=currentMultipleAlignment->blocks.begin(); block!=be; block++) {
        TESTMSG("block:");
        re = block->end();
        for (range=block->begin(), seq=currentMultipleAlignment->sequences->begin();
             range!=re;
             range++, seq++)
            TESTMSG((*seq)->sequenceString.substr(range->from, range->to - range->from + 1)); 
    }

    (*sequenceViewer)->DisplayAlignment(currentMultipleAlignment);
}

AlignmentManager::~AlignmentManager(void)
{
    if (currentMultipleAlignment) delete currentMultipleAlignment;
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

const MultipleAlignment *
AlignmentManager::CreateMultipleFromPairwiseWithIBM(const AlignmentList& alignments)
{
    if (currentMultipleAlignment) delete currentMultipleAlignment;

    AlignmentList::const_iterator a, ae = alignments.end();

    // create sequence list; fill with sequences of master + slaves
    MultipleAlignment::SequenceList
        *sequenceList = new MultipleAlignment::SequenceList(alignments.size() + 1);
    MultipleAlignment::SequenceList::iterator s = sequenceList->begin();
    *(s++) = alignmentSet->master;
    for (a=alignments.begin(); a!=ae; a++, s++) *s = (*a)->slave;
    currentMultipleAlignment = new MultipleAlignment(sequenceList);

    // each block is a continuous region on the master, over which each master
    // residue is aligned to a residue of each slave, and where there are no
    // insertions relative to the master in any of the slaves
    int masterFrom = 0, masterTo;
    MultipleAlignment::Block::iterator b, be;

    while (masterFrom < alignmentSet->master->Length()) {

        // look for first all-aligned residue
        if (!AlignedToAllSlaves(masterFrom, alignments)) {
            masterFrom++;
            continue;
        }

        // find all next continuous all-aligned residues
        for (masterTo=masterFrom+1;
                masterTo < alignmentSet->master->Length() &&
                AlignedToAllSlaves(masterTo, alignments) &&
                NoSlaveInsertionsBetween(masterFrom, masterTo, alignments);
             masterTo++) ;
        masterTo--; // after loop, masterTo = first non-all-aligned residue

        // create new block with ranges from master and all slaves
		MultipleAlignment::Block newBlock(alignments.size() + 1);
        newBlock.front().from = masterFrom;
        newBlock.front().to = masterTo;
        //TESTMSG("masterFrom " << masterFrom+1 << ", masterTo " << masterTo+1);
        b = newBlock.begin(), be = newBlock.end();
        for (b++, a=alignments.begin(); b!=be; b++, a++) {
            b->from = (*a)->masterToSlave[masterFrom];
            b->to = (*a)->masterToSlave[masterTo];
            //TESTMSG("slave->from " << b->from+1 << ", slave->to " << b->to+1);
        }

        // copy new block into alignment
        currentMultipleAlignment->AddBlockAtEnd(newBlock);

        // start looking for next block
        masterFrom = masterTo + 1;
    }

    return currentMultipleAlignment;
}

MultipleAlignment::MultipleAlignment(const SequenceList *sequenceList) :
    sequences(sequenceList)
{
}

bool MultipleAlignment::AddBlockAtEnd(const Block& newBlock)
{
    if (newBlock.size() != sequences->size()) {
        ERR_POST("MultipleAlignment::AddBlockAtEnd() - block size mismatch");
        return false;
    }

    // make sure ranges are reasonable
    Block::const_iterator range, re = newBlock.end(), prevRange;
    if (blocks.size() > 0) prevRange = blocks.back().begin();
    SequenceList::const_iterator sequence = sequences->begin();
    int blockWidthM1 = newBlock.front().to - newBlock.front().from;
    for (range=newBlock.begin(); range!=re; range++, sequence++) {
        if (range->to - range->from != blockWidthM1 ||              // check block width
            (blocks.size() > 0 && range->from <= prevRange->to) ||  // check for range overlap
            range->from > range->to ||                              // check range values
            range->to >= (*sequence)->Length()) {                   // check bounds of end
            ERR_POST(Error << "MultipleAlignment::AddBlockAtEnd() - range error");
            return false;
        }
        if (blocks.size() > 0) prevRange++;
    }

    blocks.resize(blocks.size() + 1, newBlock);
    return true;
}

END_SCOPE(Cn3D)

