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
*      implementation of non-GUI part of main sequence/alignment viewer
*
* ===========================================================================
*/

#ifndef CN3D_SEQUENCE_VIEWER__HPP
#define CN3D_SEQUENCE_VIEWER__HPP

#include <corelib/ncbistl.hpp>

#include <list>

#include "cn3d/viewer_base.hpp"


BEGIN_SCOPE(Cn3D)

class Sequence;
class AlignmentManager;
class BlockMultipleAlignment;
class SequenceViewerWindow;
class SequenceDisplay;

class SequenceViewer : public ViewerBase
{
    friend class SequenceViewerWindow;
    friend class SequenceDisplay;
    friend class AlignmentManager;

public:

    SequenceViewer(AlignmentManager *alnMgr);
    ~SequenceViewer(void);

    // to create displays from unaligned sequence(s), or multiple alignment
    typedef std::list < const Sequence * > SequenceList;
    void DisplaySequences(const SequenceList *sequenceList);
    void DisplayAlignment(BlockMultipleAlignment *multipleAlignment);

    // turn on the editor (if not already on)
    void TurnOnEditor(void);

    // functions to save edited data
    void SaveDialog(void);
    void SaveAlignment(void);

    // export current alignment
    enum eExportType {
        asFASTA,    // plain FASTA with gaps and all uppercase
        asFASTAa2m, // a2m variant of FASTA, lowercase unaligned and '.' as unaligned gap
        asText,     // plain text with id's and locations
        asHTML      // HTML, like text but with color
    };
    void ExportAlignment(eExportType type);

private:

    SequenceViewerWindow *sequenceWindow;

    void CreateSequenceWindow(bool showNow);
};

END_SCOPE(Cn3D)

#endif // CN3D_SEQUENCE_VIEWER__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.32  2003/10/13 14:16:31  thiessen
* add -n option to not show alignment window
*
* Revision 1.31  2003/02/03 19:20:06  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.30  2002/12/06 17:07:15  thiessen
* remove seqrow export format; add choice of repeat handling for FASTA export; export rows in display order
*
* Revision 1.29  2002/12/02 13:37:08  thiessen
* add seqrow format export
*
* Revision 1.28  2002/09/09 13:38:23  thiessen
* separate save and save-as
*
* Revision 1.27  2002/09/03 13:15:58  thiessen
* add A2M export
*
* Revision 1.26  2002/06/05 14:28:40  thiessen
* reorganize handling of window titles
*
* Revision 1.25  2002/04/22 14:27:28  thiessen
* add alignment export
*
* Revision 1.24  2002/03/07 19:16:04  thiessen
* don't auto-show sequence windows
*
* Revision 1.23  2001/12/06 23:13:45  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.22  2001/10/01 16:03:58  thiessen
* make CDD annotation window non-modal; add SetWindowTitle to viewers
*
* Revision 1.21  2001/04/05 22:54:51  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.20  2001/04/04 00:27:22  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.19  2001/03/19 15:47:37  thiessen
* add row sorting by identifier
*
* Revision 1.18  2001/03/13 01:24:16  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.17  2001/03/09 15:48:43  thiessen
* major changes to add initial update viewer
*
* Revision 1.16  2001/03/01 20:15:29  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
*/
