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
*      Classes to hold sets of alignments
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/08/28 18:52:16  thiessen
* start unpacking alignments
*
* ===========================================================================
*/

#ifndef CN3D_ALIGNMENT_SET__HPP
#define CN3D_ALIGNMENT_SET__HPP

#include <vector>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include "cn3d/structure_base.hpp"


BEGIN_SCOPE(Cn3D)

typedef list< ncbi::CRef< ncbi::objects::CSeq_annot > > SeqAnnotList;

class MasterSlaveAlignment;
class Sequence;

class AlignmentSet : public StructureBase
{
public:
    AlignmentSet(StructureBase *parent, const SeqAnnotList& seqAnnots);

    typedef LIST_TYPE < const MasterSlaveAlignment * > AlignmentList;
    AlignmentList alignments;

    // pointer to the master sequence for each pairwise master/slave alignment in this set
    const Sequence *master;

    bool Draw(const AtomSet *atomSet = NULL) const { return false; } // not drawable
};

class MasterSlaveAlignment : public StructureBase
{
public:
    MasterSlaveAlignment(StructureBase *parent, const Sequence *masterSequence,
        const ncbi::objects::CSeq_align& seqAlign);

    // pointers to the sequences in this pairwise alignment
    const Sequence *master, *slave;

    // this vector maps slave residues onto the master - e.g., masterToSlave[10] = 5
    // means that residue #10 in the master is aligned to residue #5 of the slave.
    // Residues are numbered from zero. masterToSlave[n] = -1 means that master
    // residue n is unaligned.
    typedef std::vector < int > ResidueVector;
    ResidueVector masterToSlave;
};

END_SCOPE(Cn3D)

#endif // CN3D_ALIGNMENT_SET__HPP
