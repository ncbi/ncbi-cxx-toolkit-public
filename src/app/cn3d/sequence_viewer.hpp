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
*      Classes to interface with sequence viewer GUI
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.15  2000/12/21 23:42:24  thiessen
* load structures from cdd's
*
* Revision 1.14  2000/11/30 15:49:09  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.13  2000/11/11 21:12:07  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.12  2000/11/03 01:12:17  thiessen
* fix memory problem with alignment cloning
*
* Revision 1.11  2000/11/02 16:48:23  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.10  2000/10/12 02:14:32  thiessen
* working block boundary editing
*
* Revision 1.9  2000/10/05 18:34:35  thiessen
* first working editing operation
*
* Revision 1.8  2000/10/04 17:40:47  thiessen
* rearrange STL #includes
*
* Revision 1.7  2000/09/20 22:22:03  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.6  2000/09/14 14:55:26  thiessen
* add row reordering; misc fixes
*
* Revision 1.5  2000/09/11 14:06:02  thiessen
* working alignment coloring
*
* Revision 1.4  2000/09/11 01:45:54  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.3  2000/09/08 20:16:10  thiessen
* working dynamic alignment views
*
* Revision 1.2  2000/09/03 18:45:57  thiessen
* working generalized sequence viewer
*
* Revision 1.1  2000/08/30 19:49:04  thiessen
* working sequence window
*
* ===========================================================================
*/

#ifndef CN3D_SEQUENCE_VIEWER__HPP
#define CN3D_SEQUENCE_VIEWER__HPP

#include <corelib/ncbistl.hpp>

#include <list>

#include "cn3d/block_multiple_alignment.hpp"


BEGIN_SCOPE(Cn3D)

class Sequence;
class Messenger;
class SequenceDisplay;
class DisplayRowFromString;
class AlignmentManager;

class SequenceViewer
{
    friend class SequenceViewerWindow;

public:
    SequenceViewer(AlignmentManager *alnMgr);
    ~SequenceViewer(void);

    // to create displays from unaligned sequence(s), or multiple alignment
    typedef std::list < const Sequence * > SequenceList;
    void DisplaySequences(const SequenceList *sequenceList);
    void DisplayAlignment(BlockMultipleAlignment *multipleAlignment);

    // adds a string row to the alignment, that contains block boundary indicators
    void AddBlockBoundaryRow(void);
    void RemoveBlockBoundaryRow(void);
    void UpdateBlockBoundaryRow(void);

    // redraw all molecules associated with the SequenceDisplay
    void RedrawAlignedMolecules(void) const;

    // to push/pop alignment+display data for undo during editing
    void PushAlignment(void);
    void PopAlignment(void);

    // save this (edited) alignment
    void SaveDialog(void);
    void SaveAlignment(void);

    // revert back to original (w/o save)
    void RevertAlignment(void);
    void KeepOnlyStackTop(void);

    // to update/remove the GUI window
    void Refresh(void);
    void DestroyGUI(void);

private:

    bool isEditableAlignment;
    void NewDisplay(SequenceDisplay *display);

    AlignmentManager *alignmentManager;
    SequenceViewerWindow *viewerWindow;

    typedef std::list < BlockMultipleAlignment * > AlignmentStack;
    AlignmentStack alignmentStack;

    typedef std::list < SequenceDisplay * > DisplayStack;
    DisplayStack displayStack;
    SequenceDisplay * GetCurrentDisplay(void) const { return displayStack.back(); }

    void InitStacks(BlockMultipleAlignment *alignment, SequenceDisplay *display);
    void ClearStacks(void);

    DisplayRowFromString * FindBlockBoundaryRow(void);

public:

    BlockMultipleAlignment * GetCurrentAlignment(void) const
    {
        BlockMultipleAlignment *alignment = NULL;
        if (alignmentStack.size() > 0) alignment = alignmentStack.back();
        return alignment;
    }

    bool IsEditableAlignment(void) const { return isEditableAlignment; }
    void SetUnalignedJustification(BlockMultipleAlignment::eUnalignedJustification justification);
};

END_SCOPE(Cn3D)

#endif // CN3D_SEQUENCE_VIEWER__HPP
