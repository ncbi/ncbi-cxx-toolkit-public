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

#include <list>

#include <corelib/ncbistl.hpp>


class ViewableAlignment; // base class for alignments to attach to SequenceViewerWidget


BEGIN_SCOPE(Cn3D)

class Sequence;
class BlockMultipleAlignment;
class SequenceViewerWindow;
class SequenceDisplay;
class Messenger;

class SequenceViewer
{
    friend class SequenceViewerWindow;

public:
    SequenceViewer(Messenger *messenger);
    ~SequenceViewer(void);

    void NewAlignment(const ViewableAlignment *display);

    // to create displays from unaligned sequence(s), or multiple alignment
    typedef std::list < const Sequence * > SequenceList;
    void DisplaySequences(const SequenceList *sequenceList);
    void DisplayAlignment(const BlockMultipleAlignment *multipleAlignment);

    void Refresh(void);

    void ClearGUI(void);
    void DestroyGUI(void);

private:

    Messenger *messenger;
    SequenceViewerWindow *viewerWindow;
    SequenceDisplay *display;
};

END_SCOPE(Cn3D)

#endif // CN3D_SEQUENCE_VIEWER__HPP
