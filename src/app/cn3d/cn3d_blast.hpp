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
*      module for aligning with BLAST and related algorithms
*
* ===========================================================================
*/

#ifndef CN3D_BLAST__HPP
#define CN3D_BLAST__HPP

#include <corelib/ncbistl.hpp>

#include <list>


BEGIN_SCOPE(Cn3D)

class Sequence;
class BlockMultipleAlignment;

class BLASTer
{
public:
    BLASTer(void) { }
    ~BLASTer(void) { }

    typedef std::list < BlockMultipleAlignment * > AlignmentList;

    // creates new pairwise alignments (as two-row BlockMultipleAlignments), each of which has
    // the given master and one of the given sequences. If the alignment algorithm fails to
    // align the new sequence, it will include a null-alignment for that sequence. If usePSSM is
    // true, will do BLAST/PSSM, otherwise, BlastTwoSequences. In both cases, alignment is restricted
    // to locations based on the multiple and alignTo/From values.
    void CreateNewPairwiseAlignmentsByBlast(const BlockMultipleAlignment *multiple,
        const AlignmentList& toRealign, AlignmentList *newAlignments, bool usePSSM);

    // calculate the self-hits for the given alignment - e.g., the score of each row when
    // aligned with the multiple using BLAST/PSSM. The scores will be placed in each row, -1.0
    // if no significant alignment is found.
    void CalculateSelfHitScores(const BlockMultipleAlignment *multiple);
};

END_SCOPE(Cn3D)

#endif // CN3D_BLAST__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2005/03/08 17:22:31  thiessen
* apparently working C++ PSSM generation
*
* Revision 1.9  2003/02/03 19:20:02  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.8  2003/01/22 14:47:30  thiessen
* cache PSSM in BlockMultipleAlignment
*
* Revision 1.7  2002/08/01 12:51:36  thiessen
* add E-value display to block aligner
*
* Revision 1.6  2002/06/13 13:32:39  thiessen
* add self-hit calculation
*
* Revision 1.5  2002/05/22 17:17:09  thiessen
* progress on BLAST interface ; change custom spin ctrl implementation
*
* Revision 1.4  2002/05/17 19:10:27  thiessen
* preliminary range restriction for BLAST/PSSM
*
* Revision 1.3  2002/03/28 14:06:02  thiessen
* preliminary BLAST/PSSM ; new CD startup style
*
* Revision 1.2  2001/09/27 15:36:47  thiessen
* decouple sequence import and BLAST
*
* Revision 1.1  2001/09/18 03:09:38  thiessen
* add preliminary sequence import pipeline
*
*/
