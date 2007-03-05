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
    static const unsigned int MAX_UNDO_STACK_SIZE;

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
