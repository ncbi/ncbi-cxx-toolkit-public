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
*      base functionality for non-GUI part of viewers
*
* ===========================================================================
*/

#ifndef CN3D_VIEWER_BASE__HPP
#define CN3D_VIEWER_BASE__HPP

#include <corelib/ncbistl.hpp>

#include <list>
#include <string>


BEGIN_SCOPE(Cn3D)

class ViewerWindowBase;
class Sequence;
class Messenger;
class SequenceDisplay;
class BlockMultipleAlignment;
class Vector;
class AlignmentManager;
class Molecule;
class MoleculeIdentifier;

class ViewerBase
{
    friend class ViewerWindowBase;
    friend class AlignmentManager;
    friend class SequenceDisplay;

public:

    // to update/remove the GUI window
    void Refresh(void);
    void DestroyGUI(void);

    void GUIDestroyed(void) { *viewerWindow = NULL; }

    // set customized window title
    void SetWindowTitle(void) const;

    // ask whether the editor is enabled for this viewer
    bool EditorIsOn(void) const;

    void RemoveBlockBoundaryRows(void);

    // tell viewer to save its data
    virtual void SaveDialog(bool prompt);

    // to be notified of font change
    void NewFont(void);

    // make residue visible, if present
    void MakeResidueVisible(const Molecule *molecule, int seqIndex);
    void MakeSequenceVisible(const MoleculeIdentifier *identifier);

    void CalculateSelfHitScores(const BlockMultipleAlignment *multiple);

    typedef std::list < BlockMultipleAlignment * > AlignmentList;

protected:
    // can't instantiate this base class
    ViewerBase(ViewerWindowBase* *window, AlignmentManager *alnMgr);
    virtual ~ViewerBase(void);

    AlignmentManager *alignmentManager;

    // handle to the associated window
    ViewerWindowBase* *const viewerWindow;

private:
    // alignment and display stack data
    AlignmentList currentAlignments;
    typedef std::list < AlignmentList > AlignmentStack;
    AlignmentStack alignmentStack;

    SequenceDisplay *currentDisplay;
    typedef std::list < SequenceDisplay * > DisplayStack;
    DisplayStack displayStack;

    // limits the size of the stack (set to -1 for unlimited)
    static const int MAX_UNDO_STACK_SIZE;

    int nRedosStored;
    bool stacksEnabled;

    void CopyDataFromStack(void);
    void ClearAllData(void);
    void SetUndoRedoMenuStates(void);

public:

    // initialization
    void InitData(const AlignmentList *alignments, SequenceDisplay *display);

    // to store alignment+display data for undo/redo during editing
    void EnableStacks(void);
    void Save(void);
    void Undo(void);
    void Redo(void);

    // revert back to bottom of stack, or keep current
    void Revert(void);
    void KeepCurrent(void);

protected:
    const AlignmentList& GetCurrentAlignments(void) const { return currentAlignments; }
    AlignmentList& GetCurrentAlignments(void) { return currentAlignments; }
    SequenceDisplay * GetCurrentDisplay(void) { return currentDisplay; }
};

END_SCOPE(Cn3D)

#endif // CN3D_VIEWER_BASE__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  2003/10/20 13:17:15  thiessen
* add float geometry violations sorting
*
* Revision 1.16  2003/02/03 19:20:08  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.15  2002/10/13 22:58:08  thiessen
* add redo ability to editor
*
* Revision 1.14  2002/10/07 13:29:32  thiessen
* add double-click -> show row to taxonomy tree
*
* Revision 1.13  2002/09/09 13:38:23  thiessen
* separate save and save-as
*
* Revision 1.12  2002/06/13 14:54:07  thiessen
* add sort by self-hit
*
* Revision 1.11  2002/06/05 14:28:42  thiessen
* reorganize handling of window titles
*
* Revision 1.10  2002/02/05 18:53:26  thiessen
* scroll to residue in sequence windows when selected in structure window
*
* Revision 1.9  2001/12/06 23:13:47  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.8  2001/10/01 16:03:59  thiessen
* make CDD annotation window non-modal; add SetWindowTitle to viewers
*
* Revision 1.7  2001/08/14 17:17:48  thiessen
* add user font selection, store in registry
*
* Revision 1.6  2001/06/14 18:59:27  thiessen
* left out 'class' in 'friend ...' statments
*
* Revision 1.5  2001/04/05 22:54:51  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.4  2001/03/22 00:32:36  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.3  2001/03/13 01:24:16  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.2  2001/03/06 20:20:44  thiessen
* progress towards >1 alignment in a SequenceDisplay ; misc minor fixes
*
* Revision 1.1  2001/03/01 20:15:30  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
*/
