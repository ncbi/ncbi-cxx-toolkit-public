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
* Revision 1.1  2000/08/29 04:34:14  thiessen
* working alignment manager, IBM
*
* ===========================================================================
*/

#ifndef CN3D_ALIGNMENT_MANAGER__HPP
#define CN3D_ALIGNMENT_MANAGER__HPP

#include <list>

#include <corelib/ncbistl.hpp>


BEGIN_SCOPE(Cn3D)

class Sequence;
class SequenceSet;
class AlignmentSet;
class MasterSlaveAlignment;
class MultipleAlignment;

class AlignmentManager
{
public:
    AlignmentManager(const SequenceSet *sSet, const AlignmentSet *aSet);

    const SequenceSet *sequenceSet;
    const AlignmentSet *alignmentSet;

    // creates a multiple alignment from the given pairwise alignments (which are
    // assumed to be members of the AlignmentSet).
    typedef std::list < const MasterSlaveAlignment * > AlignmentList;
    MultipleAlignment * CreateMultipleFromPairwiseWithIBM(const AlignmentList& alignments) const;

private:
    bool AlignedToAllSlaves(int masterResidue, const AlignmentList& alignments) const;
    bool NoSlaveInsertionsBetween(int masterFrom, int masterTo, const AlignmentList& alignments) const;
};

class MultipleAlignment
{
public:
    typedef std::list < const Sequence * > SequenceList;
    MultipleAlignment(const SequenceList *sequenceList);

    typedef struct {
        int from, to;
    } Range;

    typedef std::list < Range > Block;
    typedef std::list < Block > BlockList;
    BlockList blocks;
    const SequenceList *sequences;

    bool AddBlockAtEnd(const Block& newBlock);

    int NBlocks(void) const { return blocks.size(); }
    int NSequences(void) const { return sequences->size(); }

    ~MultipleAlignment(void) { if (sequences) delete sequences; }
};

END_SCOPE(Cn3D)

#endif // CN3D_ALIGNMENT_MANAGER__HPP
