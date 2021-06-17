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
* Author:  Greg Boratyn
*
* File Description:
*   Unit test module for function related to BLAST statistics.
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <algo/blast/core/blast_stat.h>
#include <vector>

using namespace std;

BOOST_AUTO_TEST_SUITE(stats)

// Ensure that E-value computed with finite size correction is not negative
BOOST_AUTO_TEST_CASE(EvalueForProteinFSC)
{
    Blast_KarlinBlk* kbp = NULL;
    Blast_GumbelBlk* gbp = NULL;
    Blast_Message* error = NULL;

    const char* kMatrix = "BLOSUM62";
    const int kGapOpen = 11;
    const int kGapExtend = 1;    

    // Try a few alignment scores and sequences lengths that used to cause
    // problems
    vector<int> score = {1201, 1204, 1179, 2332};
    vector<int> len1 = {294, 294, 294, 1801};
    vector<int> len2 = {422, 416, 418, 1671};

    kbp = Blast_KarlinBlkNew();
    gbp = (Blast_GumbelBlk*)calloc(1, sizeof(Blast_GumbelBlk));

    Blast_KarlinBlkGappedCalc(kbp, kGapOpen, kGapExtend, kMatrix, &error);
    Blast_GumbelBlkCalc(gbp, kGapOpen, kGapExtend, kMatrix, &error);

    for (size_t i=0;i < score.size();i++) {
        double evalue = BLAST_SpougeStoE(score[i], kbp, gbp, len1[i], len2[i]);
        BOOST_REQUIRE(evalue >= 0.0);
    }

    kbp = Blast_KarlinBlkFree(kbp);
    if (gbp) {
        free(gbp);
    }
}


BOOST_AUTO_TEST_SUITE_END()
