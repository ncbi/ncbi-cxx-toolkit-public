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
* ===========================================================================
*/

#ifndef SU_ALIGNMENT_SET__HPP
#define SU_ALIGNMENT_SET__HPP

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_limits.hpp>

#include <list>
#include <vector>
#include <map>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>


BEGIN_SCOPE(struct_util)

class Sequence;
class SequenceSet;
class BlockMultipleAlignment;

class MasterSlaveAlignment : public ncbi::CObject
{
public:
    MasterSlaveAlignment(const ncbi::objects::CSeq_align& seqAlign,
        const Sequence *masterSequence, const SequenceSet& sequenceSet);

    // pointers to the sequences in this pairwise alignment
    const Sequence *m_master, *m_slave;

    // this vector maps slave residues onto the master - e.g., masterToSlave[10] = 5
    // means that residue #10 in the master is aligned to residue #5 of the slave.
    // Residues are numbered from zero. masterToSlave[n] = UNALIGNED means that master
    // residue n is unaligned.
    enum {
        eUnaligned = kMax_UInt
    };
    typedef std::vector < unsigned int > ResidueVector;
    ResidueVector m_masterToSlave;

    // this vector contains the original block structure of the Seq-align, so that
    // the IBM algorithm can avoid merging blocks (primarily for CDD's).
    // blockStructure[i] = n means residue i (of the master) is from block n (0..nblocks-1),
    // or n = UNALIGNED if residue i is unaligned
    ResidueVector m_blockStructure;
};

class AlignmentSet : public ncbi::CObject
{
public:
    typedef std::list< ncbi::CRef< ncbi::objects::CSeq_annot > > SeqAnnotList;
    AlignmentSet(const SeqAnnotList& seqAnnots, const SequenceSet& sequenceSet);

    typedef std::list < ncbi::CRef < MasterSlaveAlignment > > AlignmentList;
    AlignmentList m_alignments;

    // pointer to the master sequence for each pairwise master/slave alignment in this set
    const Sequence *m_master;

    // constructs a new AlignmentSet (and asn data) from a multiple alignment
    static AlignmentSet * CreateFromMultiple(
        const BlockMultipleAlignment *multiple, SeqAnnotList *newAsnAlignmentData,
        const SequenceSet& sequenceSet, const std::vector < unsigned int > *rowOrder = NULL);
};

END_SCOPE(struct_util)

#endif // SU_ALIGNMENT_SET__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2004/05/26 14:45:11  gorelenk
* UNALIGNED->eUnaligned
*
* Revision 1.4  2004/05/26 01:57:47  ucko
* Move #include <corelib/ncbi_limits.hpp> from su_alignment_set.cpp for
* kMax_UInt.
*
* Revision 1.3  2004/05/25 21:22:28  ucko
* Some compilers don't support static const members, so make
* MasterSlaveAlignment::UNALIGNED a member of an (anonymous) enum instead.
*
* Revision 1.2  2004/05/25 15:52:17  thiessen
* add BlockMultipleAlignment, IBM algorithm
*
* Revision 1.1  2004/05/24 23:04:05  thiessen
* initial checkin
*
*/
