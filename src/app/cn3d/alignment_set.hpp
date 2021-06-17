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

#ifndef CN3D_ALIGNMENT_SET__HPP
#define CN3D_ALIGNMENT_SET__HPP

#include <corelib/ncbistl.hpp>

#include <list>
#include <vector>
#include <map>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include "structure_base.hpp"


BEGIN_SCOPE(Cn3D)

class MasterDependentAlignment;
class Sequence;
class BlockMultipleAlignment;

class AlignmentSet : public StructureBase
{
public:
    typedef std::list< ncbi::CRef< ncbi::objects::CSeq_annot > > SeqAnnotList;
    AlignmentSet(StructureBase *parent, const Sequence *masterSequence, const SeqAnnotList& seqAnnots);
    ~AlignmentSet(void);

    // constructs a new AlignmentSet from a multiple alignment
    static AlignmentSet * CreateFromMultiple(StructureBase *parent,
        const BlockMultipleAlignment *multiple, const std::vector < unsigned int >& rowOrder);

    typedef std::list < const MasterDependentAlignment * > AlignmentList;
    AlignmentList alignments;

    // pointer to the master sequence for each pairwise master/dependent alignment in this set
    const Sequence *master;

    bool Draw(const AtomSet *atomSet = NULL) const { return false; } // not drawable

    // new alignment data in asn1 format, created from (edited) BlockMultipleAlignment
    SeqAnnotList *newAsnAlignmentData;
};

class MasterDependentAlignment : public StructureBase
{
public:
    MasterDependentAlignment(StructureBase *parent, const Sequence *masterSequence,
        const ncbi::objects::CSeq_align& seqAlign);

    // pointers to the sequences in this pairwise alignment
    const Sequence *master, *dependent;

    // this vector maps dependent residues onto the master - e.g., masterToDependent[10] = 5
    // means that residue #10 in the master is aligned to residue #5 of the dependent.
    // Residues are numbered from zero. masterToDependent[n] = -1 means that master
    // residue n is unaligned.
    typedef std::vector < int > ResidueVector;
    ResidueVector masterToDependent;

    // this vector contains the original block structure of the Seq-align, so that
    // the IBM algorithm can avoid merging blocks (primarily for CDD's).
    // blockStructure[i] = n means residue i (of the master) is from block n (0..nblocks-1),
    // or n = -1 if residue i is unaligned
    ResidueVector blockStructure;
};

END_SCOPE(Cn3D)

#endif // CN3D_ALIGNMENT_SET__HPP
