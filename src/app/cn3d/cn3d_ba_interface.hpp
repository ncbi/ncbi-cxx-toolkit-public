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
*      Interface to Alejandro's block alignment algorithm
*
* ===========================================================================
*/

#ifndef CN3D_BA_INTERFACE__HPP
#define CN3D_BA_INTERFACE__HPP

#include <corelib/ncbistl.hpp>

#include <list>

class wxWindow;


BEGIN_SCOPE(Cn3D)

class BlockMultipleAlignment;

class BlockAligner
{
public:
    BlockAligner(void);
    ~BlockAligner(void) { }

    typedef std::list < BlockMultipleAlignment * > AlignmentList;

    // creates new pairwise alignments (as two-row BlockMultipleAlignments), each of which has
    // the given master and one of the given sequences. If the alignment algorithm fails to
    // align the new sequence, it will include a null-alignment for that sequence.
    bool CreateNewPairwiseAlignmentsByBlockAlignment(BlockMultipleAlignment *multiple,
        const AlignmentList& toRealign, AlignmentList *newAlignments, int *nRowsAddedToMultiple,
        bool canMerge);

    // brings up a dialog that lets the user set block aligner ; returns false if cancelled
    bool SetOptions(wxWindow* parent);

    typedef struct {
        double loopPercentile;
        int loopExtension;
        int loopCutoff;
        bool globalAlignment;
        bool keepExistingBlocks;
        bool mergeAfterEachSequence;
    } BlockAlignerOptions;

private:
    BlockAlignerOptions currentOptions;
};

END_SCOPE(Cn3D)

#endif // CN3D_BA_INTERFACE__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2003/08/23 22:26:23  thiessen
* switch to new dp block aligner, remove Alejandro's
*
* Revision 1.9  2003/03/27 18:46:00  thiessen
* update blockaligner code
*
* Revision 1.8  2003/02/03 19:20:02  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.7  2002/09/30 17:13:02  thiessen
* change structure import to do sequences as well; change cache to hold mimes; change block aligner vocabulary; fix block aligner dialog bugs
*
* Revision 1.6  2002/09/23 19:12:32  thiessen
* add option to allow long gaps between frozen blocks
*
* Revision 1.5  2002/09/16 21:24:58  thiessen
* add block freezing to block aligner
*
* Revision 1.4  2002/08/13 20:46:36  thiessen
* add global block aligner
*
* Revision 1.3  2002/08/01 01:59:26  thiessen
* fix type syntax error
*
* Revision 1.2  2002/08/01 01:55:16  thiessen
* add block aligner options dialog
*
* Revision 1.1  2002/07/26 15:28:45  thiessen
* add Alejandro's block alignment algorithm
*
*/
