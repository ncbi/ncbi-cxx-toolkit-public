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
*      Classes to handle messaging and communication between sequence
*      and structure windows
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/09/11 01:45:53  thiessen
* working messenger for sequence<->structure window communication
*
* ===========================================================================
*/

#ifndef CN3D_MESSENGER__HPP
#define CN3D_MESSENGER__HPP

#include <list>

#include <corelib/ncbistl.hpp>


BEGIN_SCOPE(Cn3D)

class SequenceViewer;
class BlockMultipleAlignment;
class Sequence;
class Cn3DMainFrame;
class StructureObject;

class Messenger
{
public:
    // redraw messages - "Post..." means redraw is *not* immediate, but
    // is cached and actually occurs during system idle time. Thus, during
    // processing of an event, any number of PostRedraws can be called, and
    // only one redraw as appropriate will be done at idle time.
    void PostRedrawSequenceViewers(void);
    void PostRedrawAllStructures(void);
    void PostRedrawMolecule(const StructureObject *object, int moleculeID);

    // should be called only by Cn3DApp at idle time; processes any redraws
    // that have been posted by prior event(s)
    void ProcessRedraws(void);

    // inform sequence viewers that there's a new entity to display
    typedef std::list < const Sequence * > SequenceList;
    void DisplaySequences(const SequenceList *sequences);
    void DisplayAlignment(const BlockMultipleAlignment *alignment);

    // remove any alignment from sequence viewers
    void ClearSequenceViewers(void);

private:
    typedef std::list < SequenceViewer * > SequenceViewerList;
    SequenceViewerList sequenceViewers;

    typedef std::list < Cn3DMainFrame * > StructureWindowList;
    StructureWindowList structureWindows;

    // to keep track of messages posted
    typedef std::pair < const StructureObject *, int > MoleculeIdentifier;
    typedef std::list < MoleculeIdentifier > RedrawMoleculeList;
    RedrawMoleculeList redrawMolecules;

    bool redrawAllStructures;
    bool redrawSequenceViewers;

public:
    Messenger(void) : redrawAllStructures(false), redrawSequenceViewers(false) { }

    // to register sequence and structure viewers
    void AddSequenceViewer(SequenceViewer *sequenceViewer)
        { sequenceViewers.push_back(sequenceViewer); }

    void AddStructureWindow(Cn3DMainFrame *structureWindow)
        { structureWindows.push_back(structureWindow); }

    // to remove viewers - if no structure viewers left, will kill all sequence
    // windows and exit application
    void RemoveStructureWindow(const Cn3DMainFrame *structureWindow);

};

END_SCOPE(Cn3D)

#endif // CN3D_MESSENGER__HPP
