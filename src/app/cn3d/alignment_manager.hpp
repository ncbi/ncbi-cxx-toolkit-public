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
* Revision 1.24  2000/12/15 15:52:07  thiessen
* show/hide system installed
*
* Revision 1.23  2000/11/30 15:49:06  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.22  2000/11/17 19:47:37  thiessen
* working show/hide alignment row
*
* Revision 1.21  2000/11/13 18:05:57  thiessen
* working structure re-superpositioning
*
* Revision 1.20  2000/11/11 21:12:06  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.19  2000/11/03 01:12:17  thiessen
* fix memory problem with alignment cloning
*
* Revision 1.18  2000/11/02 16:48:22  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.17  2000/10/17 14:34:18  thiessen
* added row shift - editor basically complete
*
* Revision 1.16  2000/10/16 20:02:17  thiessen
* working block creation
*
* Revision 1.15  2000/10/12 19:20:37  thiessen
* working block deletion
*
* Revision 1.14  2000/10/12 16:22:37  thiessen
* working block split
*
* Revision 1.13  2000/10/12 02:14:32  thiessen
* working block boundary editing
*
* Revision 1.12  2000/10/05 18:34:35  thiessen
* first working editing operation
*
* Revision 1.11  2000/10/04 17:40:44  thiessen
* rearrange STL #includes
*
* Revision 1.10  2000/10/02 23:25:06  thiessen
* working sequence identifier window in sequence viewer
*
* Revision 1.9  2000/09/20 22:22:02  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.8  2000/09/15 19:24:33  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.7  2000/09/11 22:57:55  thiessen
* working highlighting
*
* Revision 1.6  2000/09/11 14:06:02  thiessen
* working alignment coloring
*
* Revision 1.5  2000/09/11 01:45:52  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.4  2000/09/08 20:16:10  thiessen
* working dynamic alignment views
*
* Revision 1.3  2000/08/30 23:45:36  thiessen
* working alignment display
*
* Revision 1.2  2000/08/30 19:49:02  thiessen
* working sequence window
*
* Revision 1.1  2000/08/29 04:34:14  thiessen
* working alignment manager, IBM
*
* ===========================================================================
*/

#ifndef CN3D_ALIGNMENT_MANAGER__HPP
#define CN3D_ALIGNMENT_MANAGER__HPP

#include <corelib/ncbistl.hpp>

#include <list>
#include <vector>

#include "cn3d/vector_math.hpp"
#include "cn3d/show_hide_callback.hpp"


BEGIN_SCOPE(Cn3D)

class Sequence;
class SequenceSet;
class AlignmentSet;
class MasterSlaveAlignment;
class SequenceViewer;
class Messenger;
class BlockMultipleAlignment;

class AlignmentManager : public ShowHideCallback
{
public:
    AlignmentManager(const SequenceSet *sSet, const AlignmentSet *aSet);
    ~AlignmentManager(void);

    void NewAlignments(const SequenceSet *sSet, const AlignmentSet *aSet);

    // creates the current multiple alignment from the given pairwise alignments (which are
    // assumed to be members of the AlignmentSet).
    typedef std::list < const MasterSlaveAlignment * > AlignmentList;
    BlockMultipleAlignment *
		CreateMultipleFromPairwiseWithIBM(const AlignmentList& alignments);

    // get the working alignment
    const BlockMultipleAlignment * GetCurrentMultipleAlignment(void) const;

    // change the underlying pairwise alignments to match the given multiple
    void SavePairwiseFromMultiple(const BlockMultipleAlignment *multiple);

    // recomputes structure alignments for all slave structures in the current
    // sequence alignment
    void RealignAllSlaves(void) const;

    // stuff relating to show/hide of alignment rows (slaves)
    void GetAlignmentSetSlaveSequences(std::vector < const Sequence * > *sequences) const;
    void GetAlignmentSetSlaveVisibilities(std::vector < bool > *visibilities) const;
    void SelectionCallback(const std::vector < bool >& itemsEnabled);
    void NewMultipleWithRows(const std::vector < bool >& visibilities);

    // find out if a residue is aligned - only works for non-repeated sequences!
    bool IsAligned(const Sequence *sequence, int seqIndex) const;

    // find out if a Sequence is part of the current alignment
    bool IsInAlignment(const Sequence *sequence) const;

    // get a color for an aligned residue that's dependent on the entire alignment
    // (e.g., for coloring by sequence conservation)
    const Vector * GetAlignmentColor(const Sequence *sequence, int seqIndex) const;

private:
    const SequenceSet *sequenceSet;
    const AlignmentSet *alignmentSet;

    std::vector < bool > slavesVisible;

    // viewer for the current alignment - will own the current alignment (if any)
    SequenceViewer *sequenceViewer;
};

END_SCOPE(Cn3D)

#endif // CN3D_ALIGNMENT_MANAGER__HPP
