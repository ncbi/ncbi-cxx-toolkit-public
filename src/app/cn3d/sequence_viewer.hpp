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
*      Classes to display sequences and alignments
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/08/30 19:49:04  thiessen
* working sequence window
*
* ===========================================================================
*/

#ifndef CN3D_SEQUENCE_VIEWER__HPP
#define CN3D_SEQUENCE_VIEWER__HPP

#include <list>

#include <corelib/ncbistl.hpp>


// These classes will be defined in sequence_viewer_gui.hpp; it will hold all the
// wxWindows stuff, in order to avoid requiring wxWindows includes in this
// common header
class SequenceViewerGUI;
class SequenceDisplay;


BEGIN_SCOPE(Cn3D)

class Sequence;
class MultipleAlignment;

class SequenceViewer
{
    friend class SequenceViewerGUI;

public:
    SequenceViewer(void);
    ~SequenceViewer(void);

    // to create displays from unaligned sequence(s), or multiple alignment
    typedef std::list < const Sequence * > SequenceList;
    void DisplaySequences(const SequenceList *sequenceList);
    void DisplayAlignment(const MultipleAlignment *multipleAlignment);

    void DestroyGUI(void);

private:
    void NewDisplay(const SequenceDisplay *display);

    SequenceDisplay *display;
    SequenceViewerGUI *viewerGUI;
};

END_SCOPE(Cn3D)

#endif // CN3D_SEQUENCE_VIEWER__HPP
