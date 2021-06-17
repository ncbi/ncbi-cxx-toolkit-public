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

#include <corelib/ncbistd.hpp>

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
