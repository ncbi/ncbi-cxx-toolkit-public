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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2002/03/28 14:06:02  thiessen
* preliminary BLAST/PSSM ; new CD startup style
*
* Revision 1.12  2002/02/13 14:53:30  thiessen
* add update sort
*
* Revision 1.11  2002/02/12 17:19:23  thiessen
* first working structure import
*
* Revision 1.10  2001/10/01 16:03:58  thiessen
* make CDD annotation window non-modal; add SetWindowTitle to viewers
*
* Revision 1.9  2001/09/27 15:36:48  thiessen
* decouple sequence import and BLAST
*
* Revision 1.8  2001/09/18 03:09:38  thiessen
* add preliminary sequence import pipeline
*
* Revision 1.7  2001/05/02 13:46:15  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.6  2001/04/19 12:58:25  thiessen
* allow merge and delete of individual updates
*
* Revision 1.5  2001/04/05 22:54:51  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.4  2001/04/04 00:27:22  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.3  2001/03/22 00:32:36  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.2  2001/03/13 01:24:16  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.1  2001/03/09 15:48:44  thiessen
* major changes to add initial update viewer
*
* ===========================================================================
*/

#ifndef CN3D_UPDATE_VIEWER__HPP
#define CN3D_UPDATE_VIEWER__HPP

#include <corelib/ncbistl.hpp>

#include <objects/mmdb1/Biostruc.hpp>

#include <list>

#include "cn3d/viewer_base.hpp"


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

    void Refresh(void);             // refreshes the window only if present
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
    void SaveDialog(void);
    void SaveAlignments(void);

    // run BLAST on given pairwise alignment - if BLAST is successful, then alignment will be
    // replaced with the result, otherwise the alignment is left unchanged
    void BlastUpdate(BlockMultipleAlignment *alignment, bool usePSSMFromMultiple);

    // set window title
    void SetWindowTitle(const std::string& title) const;

    // save pending structures
    void SavePendingStructures(void);

    // sort updates
    void SortByIdentifier(void);

private:
    UpdateViewerWindow *updateWindow;

    const Sequence * GetMasterSequence(void) const;
    typedef std::list < const Sequence * > SequenceList;
    void FetchSequenceViaHTTP(int gi, SequenceList *newSequences, StructureSet *sSet) const;
    void ReadSequencesFromFile(SequenceList *newSequences, StructureSet *sSet, int specificGI = -1) const;
    void FetchSequences(StructureSet *sSet, SequenceList *newSequences, int specificGI = -1) const;
    void MakeNewAlignments(const SequenceList& newSequences,
        const Sequence *master, AlignmentList *newAlignments) const;

    typedef std::list < ncbi::CRef < ncbi::objects::CBiostruc > > BiostrucList;
    BiostrucList pendingStructures;

    void SortUpdates(void);
};

END_SCOPE(Cn3D)

#endif // CN3D_UPDATE_VIEWER__HPP
