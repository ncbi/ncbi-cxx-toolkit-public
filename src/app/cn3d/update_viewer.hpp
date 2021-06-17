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
*      implementation of non-GUI part of update viewer
*
* ===========================================================================
*/

#ifndef CN3D_UPDATE_VIEWER__HPP
#define CN3D_UPDATE_VIEWER__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.hpp>

#include <objects/mmdb1/Biostruc.hpp>
#include <objects/mmdb3/Biostruc_feature.hpp>

#include <list>

#include "viewer_base.hpp"


BEGIN_SCOPE(Cn3D)

class UpdateViewerWindow;
class BlockMultipleAlignment;
class AlignmentManager;
class Sequence;
class StructureSet;

class UpdateViewer : public ViewerBase
{
    friend class UpdateViewerWindow;
    friend class SequenceDisplay;

public:

    UpdateViewer(AlignmentManager *alnMgr);
    ~UpdateViewer(void);

    void CreateUpdateWindow(void);  // (re)creates the window

    // add new pairwise alignments to the update window
    typedef std::list < BlockMultipleAlignment * > AlignmentList;
    void AddAlignments(const AlignmentList& alignmentList);

    // replace contents of update window with given alignments
    void ReplaceAlignments(const AlignmentList& alignmentList);

    // delete a single alignment
    void DeleteAlignment(BlockMultipleAlignment *toDelete);

    // import
    void ImportSequences(void);
    void ImportStructure(void);

    // turns the current alignments+display into the "initial state" (the bottom) of the undo stack
    void SetInitialState(void);

    // functions to save edited data
    void SaveDialog(bool prompt);
    void SaveAlignments(void);

    // run BLAST on given pairwise alignment - if BLAST is successful, then alignment will be
    // replaced with the result, otherwise the alignment is left unchanged
    void BlastUpdate(BlockMultipleAlignment *alignment, bool usePSSMFromMultiple);

    // find alignment of nearest BLAST-2-sequences neighbor in the multiple, and use this as
    // a guide alignment to align this update with the multiple's master
    void BlastNeighbor(BlockMultipleAlignment *update);

    // save pending structures
    void SavePendingStructures(void);

    // sort updates
    void SortByIdentifier(void);
    void SortByPSSM(void);

private:
    UpdateViewerWindow *updateWindow;

    const Sequence * GetMasterSequence(void) const;
    typedef std::list < const Sequence * > SequenceList;
    void FetchSequencesViaHTTP(SequenceList *newSequences, StructureSet *sSet) const;
    void ReadSequencesFromFile(SequenceList *newSequences, StructureSet *sSet) const;
    void FetchSequences(StructureSet *sSet, SequenceList *newSequences) const;

    void SortUpdates(void);
    void MakeEmptyAlignments(const SequenceList& newSequences,
        const Sequence *master, AlignmentList *newAlignments) const;

    typedef std::list < ncbi::CRef < ncbi::objects::CBiostruc > > BiostrucList;
    BiostrucList pendingStructures;
    typedef struct {
        ncbi::CRef < ncbi::objects::CBiostruc_feature > structureAlignment;
        int masterDomainID, dependentDomainID;
    } StructureAlignmentInfo;
    typedef std::list < StructureAlignmentInfo > PendingStructureAlignments;
    PendingStructureAlignments pendingStructureAlignments;

    void GetVASTAlignments(const SequenceList& newSequences,
        const Sequence *master, AlignmentList *newAlignments,
		PendingStructureAlignments *structureAlignments,
        unsigned int masterFrom = kMax_UInt, unsigned int masterTo = kMax_UInt) const;  // kMax_UInt means unrestricted
};

END_SCOPE(Cn3D)

#endif // CN3D_UPDATE_VIEWER__HPP
