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
* Revision 1.11  2001/03/01 20:15:29  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* Revision 1.10  2000/12/15 15:52:08  thiessen
* show/hide system installed
*
* Revision 1.9  2000/11/30 15:49:08  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.8  2000/11/02 16:48:22  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.7  2000/10/19 12:40:21  thiessen
* avoid multiple sequence redraws with scroll set
*
* Revision 1.6  2000/10/12 02:14:32  thiessen
* working block boundary editing
*
* Revision 1.5  2000/10/04 17:40:45  thiessen
* rearrange STL #includes
*
* Revision 1.4  2000/10/02 23:25:07  thiessen
* working sequence identifier window in sequence viewer
*
* Revision 1.3  2000/09/15 19:24:33  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.2  2000/09/11 22:57:55  thiessen
* working highlighting
*
* Revision 1.1  2000/09/11 01:45:53  thiessen
* working messenger for sequence<->structure window communication
*
* ===========================================================================
*/

#ifndef CN3D_MESSENGER__HPP
#define CN3D_MESSENGER__HPP

#include <corelib/ncbistl.hpp>

#include <list>
#include <map>
#include <vector>


BEGIN_SCOPE(Cn3D)

class ViewerBase;
class BlockMultipleAlignment;
class Sequence;
class Cn3DMainFrame;
class StructureObject;
class Molecule;
class Messenger;


// for now, there is only a single global messenger, which for convenience
// can be accessed anywhere via this function
Messenger * GlobalMessenger(void);


class Messenger
{
public:
    // redraw messages - "Post..." means redraw is *not* immediate, but
    // is cached and actually occurs during system idle time. Thus, during
    // processing of an event, any number of PostRedraws can be called, and
    // only one redraw as appropriate will be done at idle time.
    void PostRedrawSequenceViewer(ViewerBase *viewer);
    void PostRedrawAllSequenceViewers(void);

    void PostRedrawMolecule(const Molecule *molecule);
    void PostRedrawAllStructures(void);

    // un-Post a redraw message - use (carefully!) to avoid redundant redraws
    // (flicker) if some other action is known to cause immediate redraw.
    void UnPostRedrawAllSequenceViewers(void);
    void UnPostRedrawSequenceViewer(ViewerBase *viewer);

    // un-Post structure redraws - again, use carefully to avoid redundant redraws
    // when some non-Messenger method causes structure redraws to occur
    void UnPostStructureRedraws(void);

    // should be called only by Cn3DApp at idle time; processes any redraws
    // that have been posted by prior event(s)
    void ProcessRedraws(void);

    // called if the application is about to exit - tell sequence window(s) to save
    void SequenceWindowsSave(void);


    // these next few are related to highlighting:

    // clear all highlight stores - and optionally post redraws. Returns 'true'
    // if there were actually any highlights to remove
    bool RemoveAllHighlights(bool postRedraws);

    // check for highlight
    bool IsHighlighted(const Molecule *molecule, int residueID) const;
    bool IsHighlighted(const Sequence *sequence, int seqIndex) const;

    // highlight any 'ole residue, regardless of molecule type
    void ToggleHighlightOnAnyResidue(const Molecule *molecule, int residueID);

    // add/remove highlights based on sequence
    void AddHighlights(const Sequence *sequence, int seqIndexFrom, int seqIndexTo);
    void RemoveHighlights(const Sequence *sequence, int seqIndexFrom, int seqIndexTo);

private:

    // lists of registered viewers
    typedef std::list < ViewerBase * > SequenceViewerList;
    SequenceViewerList sequenceViewers;
    typedef std::list < Cn3DMainFrame * > StructureWindowList;
    StructureWindowList structureWindows;

    // to keep track of messages posted
    typedef std::map < const Molecule *, bool > RedrawMoleculeList; // use map to preclude redundant redraws
    RedrawMoleculeList redrawMolecules;
    typedef std::map < ViewerBase *, bool > RedrawSequenceViewerList;
    RedrawSequenceViewerList redrawSequenceViewers;
    bool redrawAllStructures;
    bool redrawAllSequenceViewers;

    void RedrawMoleculesOfSameSequence(const Sequence *sequence);

    // to store lists of highlighted stuff
    typedef std::pair < const Molecule *, int > ResidueIdentifier;
    typedef std::map < ResidueIdentifier, bool > PerResidueHighlightStore;
    PerResidueHighlightStore residueHighlights;

    typedef std::map < const Sequence *, std::vector < bool > > PerSequenceHighlightStore;
    PerSequenceHighlightStore sequenceHighlights;

public:
    Messenger(void) : redrawAllStructures(false), redrawAllSequenceViewers(false) { }

    bool IsAnythingHighlighted(void) const
        { return (residueHighlights.size() > 0 || sequenceHighlights.size() > 0); }

    // to register sequence and structure viewers for redraw postings
    void AddSequenceViewer(ViewerBase *sequenceViewer)
        { sequenceViewers.push_back(sequenceViewer); }

    void AddStructureWindow(Cn3DMainFrame *structureWindow)
        { structureWindows.push_back(structureWindow); }

    // to unregister viewers
    void RemoveStructureWindow(const Cn3DMainFrame *structureWindow);
    void RemoveSequenceViewer(const ViewerBase *sequenceViewer);

};

END_SCOPE(Cn3D)

#endif // CN3D_MESSENGER__HPP
