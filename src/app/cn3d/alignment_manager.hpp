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

#ifndef CN3D_ALIGNMENT_MANAGER__HPP
#define CN3D_ALIGNMENT_MANAGER__HPP

#include <corelib/ncbistl.hpp>
#include <objects/cdd/Cdd.hpp>
#include <objects/cdd/Update_align.hpp>

#include <list>
#include <vector>
#include <map>

#include "vector_math.hpp"
#include "show_hide_callback.hpp"
#include "style_manager.hpp"


BEGIN_SCOPE(Cn3D)

class Sequence;
class SequenceSet;
class AlignmentSet;
class MasterSlaveAlignment;
class SequenceViewer;
class Messenger;
class BlockMultipleAlignment;
class Threader;
class ThreaderOptions;
class UpdateViewer;
class BLASTer;
class StructureSet;
class MoleculeIdentifier;
class BlockAligner;

class AlignmentManager : public ShowHideCallbackObject
{
public:
    AlignmentManager(const SequenceSet *sSet, const AlignmentSet *aSet);
    typedef std::list< ncbi::CRef< ncbi::objects::CUpdate_align > > UpdateAlignList;
    AlignmentManager(const SequenceSet *sSet, const AlignmentSet *aSet, const UpdateAlignList& updates);
    ~AlignmentManager(void);

    Threader *threader; // made public so viewers have access to it
    BLASTer *blaster;
    BlockAligner *blockAligner;

    void ReplaceUpdatesInASN(ncbi::objects::CCdd::TPending& newUpdates) const;

    // creates the current multiple alignment from the given pairwise alignments (which are
    // assumed to be members of the AlignmentSet).
    typedef std::list < const MasterSlaveAlignment * > PairwiseAlignmentList;
    BlockMultipleAlignment *
        CreateMultipleFromPairwiseWithIBM(const PairwiseAlignmentList& alignments);

    // change the underlying pairwise alignments to match the given multiple and row order
    void SavePairwiseFromMultiple(const BlockMultipleAlignment *multiple,
        const std::vector < int >& rowOrder);

    // recomputes structure alignments for all slave structures in the current
    // sequence alignment; uses only highlighted aligned residues (on master) if highlightedOnly == true
    void RealignAllSlaveStructures(bool highlightedOnly) const;

    // stuff relating to show/hide of alignment rows (slaves)
    void GetAlignmentSetSlaveSequences(std::vector < const Sequence * > *sequences) const;
    void GetAlignmentSetSlaveVisibilities(std::vector < bool > *visibilities) const;
    void ShowHideCallbackFunction(const std::vector < bool >& itemsEnabled);
    void NewMultipleWithRows(const std::vector < bool >& visibilities);

    // find out if a residue is aligned - only works for non-repeated sequences!
    bool IsAligned(const Sequence *sequence, int seqIndex) const;

    // find out if a Sequence is part of the current alignment
    bool IsInAlignment(const Sequence *sequence) const;

    // get a color for an aligned residue that's dependent on the entire alignment
    // (e.g., for coloring by sequence conservation)
    const Vector * GetAlignmentColor(const Sequence *sequence,
        int seqIndex, StyleSettings::eColorScheme colorScheme) const;

    // sequence alignment algorithm functions
    void RealignSlaveSequences(BlockMultipleAlignment *multiple, const std::vector < int >& slavesToRealign);
    void ThreadUpdate(const ThreaderOptions& options, BlockMultipleAlignment *single);
    void ThreadAllUpdates(const ThreaderOptions& options);
    void BlockAlignAllUpdates(void);
    void BlockAlignUpdate(BlockMultipleAlignment *single);
    void ExtendAllUpdates(void);
    void ExtendUpdate(BlockMultipleAlignment *single);

    // merge functions
    typedef std::map < BlockMultipleAlignment *, bool > UpdateMap;
    void MergeUpdates(const UpdateMap& updates, bool mergeToNeighbor);

    // calculates row scores using the threader
    void CalculateRowScoresWithThreader(double weightPSSM);

    // get the working alignment
    const BlockMultipleAlignment * GetCurrentMultipleAlignment(void) const;

    // get a list of chain sequences when we're viewing a single structure
    bool GetStructureProteins(std::vector < const Sequence * > *chains) const;

    // get a list of (slave) sequences present in the updates
    int NUpdates(void) const;
    void GetUpdateSequences(std::list < const Sequence * > *updateSequences) const;

    // remove sequence from both multiple alignment and updates
    void PurgeSequence(const MoleculeIdentifier *identifier);

    // show sequence/alignment/update viewer
    void ShowSequenceViewer(void) const;
    void ShowUpdateWindow(void) const;

private:
    void Init(void);
    void NewAlignments(const SequenceSet *sSet, const AlignmentSet *aSet);

    const SequenceSet *sequenceSet;
    const AlignmentSet *alignmentSet;

    mutable std::vector < bool > slavesVisible;

    // viewer for the current alignment - will own the current alignment (if any)
    SequenceViewer *sequenceViewer;

    // viewer for updates (grab-bag of pairwise alignments)
    UpdateViewer *updateViewer;

    // for change-type comparison
    BlockMultipleAlignment *originalMultiple;
    std::vector < int > originalRowOrder;
};

END_SCOPE(Cn3D)

#endif // CN3D_ALIGNMENT_MANAGER__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.51  2005/04/22 13:43:01  thiessen
* add block highlighting and structure alignment based on highlighted positions only
*
* Revision 1.50  2004/09/23 10:31:14  thiessen
* add block extension algorithm
*
* Revision 1.49  2004/02/19 17:04:40  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.48  2003/10/13 13:23:31  thiessen
* add -i option to show import window
*
* Revision 1.47  2003/07/17 16:52:34  thiessen
* add FileSaved message with edit typing
*
* Revision 1.46  2003/02/03 19:20:00  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.45  2003/01/29 01:41:05  thiessen
* add merge neighbor instead of merge near highlight
*
* Revision 1.44  2003/01/28 21:07:56  thiessen
* add block fit coloring algorithm; tweak row dragging; fix style bug
*
* Revision 1.43  2002/11/06 00:18:10  thiessen
* fixes for new CRef/const rules in objects
*
* Revision 1.42  2002/09/16 21:24:58  thiessen
* add block freezing to block aligner
*
* Revision 1.41  2002/07/26 15:28:44  thiessen
* add Alejandro's block alignment algorithm
*
* Revision 1.40  2002/03/07 19:16:04  thiessen
* don't auto-show sequence windows
*
* Revision 1.39  2002/02/19 14:59:38  thiessen
* add CDD reject and purge sequence
*
* Revision 1.38  2001/11/30 14:02:05  thiessen
* progress on sequence imports to single structures
*
* Revision 1.37  2001/11/27 16:26:06  thiessen
* major update to data management system
*
* Revision 1.36  2001/09/27 15:36:47  thiessen
* decouple sequence import and BLAST
*
* Revision 1.35  2001/05/31 18:46:25  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.34  2001/05/02 13:46:14  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.33  2001/04/19 12:58:25  thiessen
* allow merge and delete of individual updates
*
* Revision 1.32  2001/04/17 20:15:23  thiessen
* load 'pending' Cdd alignments into update window
*
* Revision 1.31  2001/04/05 22:54:50  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.30  2001/04/04 00:27:21  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.29  2001/03/30 03:07:08  thiessen
* add threader score calculation & sorting
*
* Revision 1.28  2001/03/09 15:48:42  thiessen
* major changes to add initial update viewer
*
* Revision 1.27  2001/03/02 15:33:43  thiessen
* minor fixes to save & show/hide dialogs, wx string headers
*
* Revision 1.26  2001/02/08 23:01:13  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* Revision 1.25  2000/12/29 19:23:49  thiessen
* save row order
*
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
*/
