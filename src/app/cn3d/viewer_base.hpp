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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2001/03/13 01:24:16  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.2  2001/03/06 20:20:44  thiessen
* progress towards >1 alignment in a SequenceDisplay ; misc minor fixes
*
* Revision 1.1  2001/03/01 20:15:30  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* ===========================================================================
*/

#ifndef CN3D_VIEWER_BASE__HPP
#define CN3D_VIEWER_BASE__HPP

#include <corelib/ncbistl.hpp>

#include <list>


BEGIN_SCOPE(Cn3D)

class ViewerWindowBase;
class Sequence;
class Messenger;
class SequenceDisplay;
class BlockMultipleAlignment;

class ViewerBase
{
public:

    // to update/remove the GUI window
    virtual void Refresh(void) = 0; // must be implemented by derived class
    void DestroyGUI(void);

    // to push/pop alignment+display data for undo during editing
    void PushAlignment(void);
    void PopAlignment(void);

    // revert back to original (w/o save)
    void RevertAlignment(void);
    void KeepOnlyStackTop(void);

    // tell viewer to save its data (does nothing unless overridden)
    virtual void SaveDialog(void) { }

    typedef std::list < BlockMultipleAlignment * > AlignmentList;

protected:

    // can't instantiate this base class
    ViewerBase(ViewerWindowBase* *window);
    virtual ~ViewerBase(void);

    // handle to the associated window
    ViewerWindowBase* *const viewerWindow;

    typedef std::list < AlignmentList > AlignmentStack;
    AlignmentStack alignmentStack;

    typedef std::list < SequenceDisplay * > DisplayStack;
    DisplayStack displayStack;

    void InitStacks(const AlignmentList *alignments, SequenceDisplay *display);
    void ClearStacks(void);

public:

    void GUIDestroyed(void) { *viewerWindow = NULL; }

    const AlignmentList * GetCurrentAlignments(void) const
        { return ((alignmentStack.size() > 0) ? &(alignmentStack.back()) : NULL); }

    SequenceDisplay * GetCurrentDisplay(void) const
        { return ((displayStack.size() > 0) ? displayStack.back() : NULL); }
};

END_SCOPE(Cn3D)

#endif // CN3D_VIEWER_BASE__HPP
