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
*      Classes to hold sets of master/slave pairwise alignments
*
* ===========================================================================
*/

#ifndef CAV_ALIGNMENT_SET__HPP
#define CAV_ALIGNMENT_SET__HPP

#include <corelib/ncbistl.hpp>

#include <list>
#include <vector>

#include <objects/seq/Seq_annot.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
class Seq_align;
END_SCOPE(objects)

typedef list < CRef < objects::CSeq_annot > > SeqAnnotList;

class MasterSlaveAlignment;
class Sequence;
class SequenceSet;

class AlignmentSet
{
private:
    int status;

public:
    AlignmentSet(SequenceSet *seqSet, const SeqAnnotList& seqAnnots, bool ignoreBadPairwiseAlignments = false);
    ~AlignmentSet(void);

    typedef vector < const MasterSlaveAlignment * > AlignmentList;
    AlignmentList alignments;

    int Status(void) const { return status; }

    // pointer to the master sequence for each pairwise master/slave alignment in this set
    const Sequence *master;
};

class MasterSlaveAlignment
{
private:
    int status;

public:
    MasterSlaveAlignment(const SequenceSet *sequenceSet, const Sequence *masterSequence,
        const objects::CSeq_align& seqAlign);

    // pointers to the sequences in this pairwise alignment
    const Sequence *master, *slave;

    // this vector maps slave residues onto the master - e.g., masterToSlave[10] = 5
    // means that residue #10 in the master is aligned to residue #5 of the slave.
    // Residues are numbered from zero. masterToSlave[n] = -1 means that master
    // residue n is unaligned.
    typedef vector < int > ResidueVector;
    ResidueVector masterToSlave;

    int Status(void) const { return status; }
};

END_NCBI_SCOPE


#endif // CAV_ALIGNMENT_SET__HPP
