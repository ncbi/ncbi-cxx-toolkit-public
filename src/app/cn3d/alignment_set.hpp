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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  2006/07/13 22:33:50  thiessen
* change all 'slave' -> 'dependent'
*
* Revision 1.17  2005/10/19 17:28:17  thiessen
* migrate to wxWidgets 2.6.2; handle signed/unsigned issue
*
* Revision 1.16  2004/02/19 17:04:40  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.15  2003/02/03 19:20:00  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.14  2002/11/06 00:18:10  thiessen
* fixes for new CRef/const rules in objects
*
* Revision 1.13  2001/11/27 16:26:06  thiessen
* major update to data management system
*
* Revision 1.12  2001/05/02 13:46:14  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.11  2001/04/17 20:15:23  thiessen
* load 'pending' Cdd alignments into update window
*
* Revision 1.10  2000/12/29 19:23:49  thiessen
* save row order
*
* Revision 1.9  2000/12/26 16:47:39  thiessen
* preserve block boundaries
*
* Revision 1.8  2000/11/12 04:02:21  thiessen
* working file save including alignment edits
*
* Revision 1.7  2000/11/11 21:12:07  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.6  2000/11/02 16:48:22  thiessen
* working editor undo; dynamic dependent transforms
*
* Revision 1.5  2000/10/04 17:40:44  thiessen
* rearrange STL #includes
*
* Revision 1.4  2000/09/15 19:24:33  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.3  2000/08/30 19:49:03  thiessen
* working sequence window
*
* Revision 1.2  2000/08/29 04:34:15  thiessen
* working alignment manager, IBM
*
* Revision 1.1  2000/08/28 18:52:16  thiessen
* start unpacking alignments
*
*/
