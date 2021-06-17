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
class MasterDependentAlignment;
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
class BMARefiner;

class AlignmentManager : public ShowHideCallbackObject
{
public:
    AlignmentManager(const SequenceSet *sSet, const AlignmentSet *aSet);
    typedef std::list< ncbi::CRef< ncbi::objects::CUpdate_align > > UpdateAlignList;
    AlignmentManager(const SequenceSet *sSet, const AlignmentSet *aSet, const UpdateAlignList& updates);
    virtual ~AlignmentManager(void);

    Threader *threader; // made public so viewers have access to it
    BLASTer *blaster;
    BlockAligner *blockAligner;
//    BMARefiner *bmaRefiner;

    void ReplaceUpdatesInASN(ncbi::objects::CCdd::TPending& newUpdates) const;

    // creates the current multiple alignment from the given pairwise alignments (which are
    // assumed to be members of the AlignmentSet).
    typedef std::list < const MasterDependentAlignment * > PairwiseAlignmentList;
    BlockMultipleAlignment *
        CreateMultipleFromPairwiseWithIBM(const PairwiseAlignmentList& alignments);

    // change the underlying pairwise alignments to match the given multiple and row order
    void SavePairwiseFromMultiple(const BlockMultipleAlignment *multiple,
        const std::vector < unsigned int >& rowOrder);

    // recomputes structure alignments for all dependent structures in the current
    // sequence alignment; uses only highlighted aligned residues (on master) if highlightedOnly == true
    void RealignAllDependentStructures(bool highlightedOnly) const;

    // stuff relating to show/hide of alignment rows (dependents)
    void GetAlignmentSetDependentSequences(std::vector < const Sequence * > *sequences) const;
    void GetAlignmentSetDependentVisibilities(std::vector < bool > *visibilities) const;
    void ShowHideCallbackFunction(const std::vector < bool >& itemsEnabled);
    void NewMultipleWithRows(const std::vector < bool >& visibilities);

    // find out if a residue is aligned - only works for non-repeated sequences!
    bool IsAligned(const Sequence *sequence, unsigned int seqIndex) const;

    // find out if a Sequence is part of the current alignment
    bool IsInAlignment(const Sequence *sequence) const;

    // get a color for an aligned residue that's dependent on the entire alignment
    // (e.g., for coloring by sequence conservation)
    const Vector * GetAlignmentColor(const Sequence *sequence,
        unsigned int seqIndex, StyleSettings::eColorScheme colorScheme) const;

    // sequence alignment algorithm functions
    void RealignDependentSequences(BlockMultipleAlignment *multiple, const std::vector < unsigned int >& dependentsToRealign);
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

    // get a list of (dependent) sequences present in the updates
    unsigned int NUpdates(void) const;
    void GetUpdateSequences(std::list < const Sequence * > *updateSequences) const;

    // remove sequence from both multiple alignment and updates
    void PurgeSequence(const MoleculeIdentifier *identifier);

    // run the alignment refiner
    void RefineAlignment(bool setUpOptionsOnly);

    // show sequence/alignment/update viewer
    void ShowSequenceViewer(bool showNow) const;
    void ShowUpdateWindow(void) const;

private:
    void Init(void);
    void NewAlignments(const SequenceSet *sSet, const AlignmentSet *aSet);

    const SequenceSet *sequenceSet;
    const AlignmentSet *alignmentSet;

    mutable std::vector < bool > dependentsVisible;

    // viewer for the current alignment - will own the current alignment (if any)
    SequenceViewer *sequenceViewer;

    // viewer for updates (grab-bag of pairwise alignments)
    UpdateViewer *updateViewer;

    // for change-type comparison
    BlockMultipleAlignment *originalMultiple;
    std::vector < unsigned int > originalRowOrder;
};

END_SCOPE(Cn3D)

#endif // CN3D_ALIGNMENT_MANAGER__HPP
