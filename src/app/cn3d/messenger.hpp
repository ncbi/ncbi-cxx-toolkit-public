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
* ===========================================================================
*/

#ifndef CN3D_MESSENGER__HPP
#define CN3D_MESSENGER__HPP

#include <corelib/ncbistl.hpp>
#include <objects/mmdb1/Biostruc_annot_set.hpp>

#include <string>
#include <list>
#include <map>
#include <vector>


BEGIN_SCOPE(Cn3D)

// for now, there is only a single global messenger, which for convenience
// can be accessed anywhere via this function
class Messenger;
Messenger * GlobalMessenger(void);


class ViewerBase;
class BlockMultipleAlignment;
class Sequence;
class StructureWindow;
class StructureObject;
class Molecule;
class MoleculeIdentifier;
class StructureSet;

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
    void SequenceWindowsSave(bool prompt);

    // called to notify all sequence viewers that their font should be changed
    void NewSequenceViewerFont(void);

    // these next few are related to highlighting:

    // typedef for highlight storage
    typedef std::map < const MoleculeIdentifier *, std::vector < bool > > MoleculeHighlightMap;

    // check for highlight
    bool IsHighlighted(const Molecule *molecule, int residueID) const;
    bool IsHighlighted(const Sequence *sequence, unsigned int seqIndex) const;
    bool IsHighlightedAnywhere(const MoleculeIdentifier *identifier) const;
    bool IsHighlightedAnywhere(const Molecule *molecule) const;

    // clear all highlight stores - and optionally post redraws. Returns 'true'
    // if there were actually any highlights to remove
    bool RemoveAllHighlights(bool postRedraws);

    // add/remove highlights based on sequence
    void AddHighlights(const Sequence *sequence, unsigned int seqIndexFrom, unsigned int seqIndexTo);
    void RemoveHighlights(const Sequence *sequence, unsigned int seqIndexFrom, unsigned int seqIndexTo);
    // toggle highlights on each individual residue in given region
    void ToggleHighlights(const Sequence *sequence, unsigned int seqIndexFrom, unsigned int seqIndexTo);

    // highlight any 'ole residue, regardless of molecule type
    void AddHighlights(const Molecule *molecule, int residueIDFrom, int residueIDTo, bool scrollViewersTo = false);
    void ToggleHighlight(const Molecule *molecule, int residueID, bool scrollViewersTo = false);

    // get/set a bunch of highlights all at once - copies highlight list from given set
    void GetHighlights(MoleculeHighlightMap *copyHighlights);
    void SetHighlights(const MoleculeHighlightMap& newHighlights);

    // highlights a sequence and moves viewer to that row
    void HighlightAndShowSequence(const Sequence *sequence);

    // remove all highlights except those on the given sequence
    void KeepHighlightsOnlyOnSequence(const Sequence *sequence);

    // temporarily turns off highlighting (suspend==true) - but doesn't erase highlight stores,
    // so when called with suspend==false, highlights will come back on
    void SuspendHighlighting(bool suspend);

    // store/restore highlights in/from a cache
    void CacheHighlights(void);
    void RestoreCachedHighlights(void);

private:

    // lists of registered viewers
    typedef std::list < ViewerBase * > SequenceViewerList;
    SequenceViewerList sequenceViewers;
    // currently can only have one structure viewer
    StructureWindow * structureWindow;

    // to keep track of messages posted
    typedef std::map < const Molecule *, bool > RedrawMoleculeList; // use map to preclude redundant redraws
    RedrawMoleculeList redrawMolecules;
    typedef std::map < ViewerBase *, bool > RedrawSequenceViewerList;
    RedrawSequenceViewerList redrawSequenceViewers;
    bool redrawAllStructures;
    bool redrawAllSequenceViewers;
    bool highlightingSuspended;

    // To store lists of highlighted entities
    MoleculeHighlightMap highlights, highlightCache;

    bool IsHighlighted(const MoleculeIdentifier *identifier, int index) const;
    void ToggleHighlights(const MoleculeIdentifier *identifier, unsigned int indexFrom, unsigned int indexTo,
        const StructureSet *set);

    void RedrawMoleculesWithIdentifier(const MoleculeIdentifier *identifier, const StructureSet *set);

public:
    Messenger(void) : redrawAllStructures(false), redrawAllSequenceViewers(false), highlightingSuspended(false) { }

    bool IsAnythingHighlighted(void) const { return (highlights.size() > 0); }

    // to get lists of highlighted molecules with structure only (for user annotations)
    bool GetHighlightedResiduesWithStructure(MoleculeHighlightMap *residues) const;

    // to get lists of highlighted residues that come from a single structure object
    // (for cdd annotations) - returns NULL if highlights aren't from one structure
    ncbi::objects::CBiostruc_annot_set * CreateBiostrucAnnotSetForHighlightsOnSingleObject(void) const;

    // formats list of highlighted residues for selection messaging; returns true if
    // there are any highlights
    bool GetHighlightsForSelectionMessage(std::string *data) const;

    // to register sequence and structure viewers for redraw postings
    void AddSequenceViewer(ViewerBase *sequenceViewer)
        { sequenceViewers.push_back(sequenceViewer); }

    void AddStructureWindow(StructureWindow *window)
        { structureWindow = window; }

    // to unregister viewers
    void RemoveStructureWindow(const StructureWindow *structureWindow);
    void RemoveSequenceViewer(const ViewerBase *sequenceViewer);

    // set window titles
    void SetAllWindowTitles(void) const;

    // for sending messages through file messenger
    bool IsFileMessengerActive(void) const;
    void FileMessengerSend(const std::string& toApp, const std::string& command, const std::string& data);
};

END_SCOPE(Cn3D)

#endif // CN3D_MESSENGER__HPP
