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
*   Unit tests for CGeneralScoreMatrix class.
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <algorithm>
#include <algo/blast/gumbel_params/general_score_matrix.hpp>


// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#undef NCBI_BOOST_NO_AUTO_TEST_MAIN
#define NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers
#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING


USING_NCBI_SCOPE;
USING_SCOPE(blast);

BOOST_AUTO_TEST_CASE(TestCreateMatrixFromVector)
{
    CNcbiIfstream istr("data/blosum62.txt");
    BOOST_REQUIRE(istr);
    const unsigned int kNumResidues = 20;
    vector<Int4> scores(kNumResidues * kNumResidues);
    int k = 0;
    while (!istr.eof()) {
        istr >> scores[k++];
    }
    CGeneralScoreMatrix smat(scores);
    for (unsigned int i=0;i < kNumResidues;i++) {
        for (unsigned int j=0;j < kNumResidues;j++) {
            BOOST_REQUIRE_EQUAL( scores[i * kNumResidues + j],
                                 smat.GetScore((Uint4)i, (Uint4)j) );
        }
    }

    BOOST_REQUIRE_EQUAL(smat.GetNumResidues(), kNumResidues);
}

BOOST_AUTO_TEST_CASE(TestGetScore)
{
    CGeneralScoreMatrix smat(CGeneralScoreMatrix::eBlosum62);
    const char* residues = smat.GetResidueOrder();

    // Score matrix entries accessed by row, column are the same
    // as accessed by residues
    for (Uint4 i=0;i < smat.GetNumResidues();i++) {        
        for (Uint4 j=0;j < smat.GetNumResidues();j++) {
            BOOST_REQUIRE_EQUAL( smat.GetScore(residues[i], residues[j]),
                               smat.GetScore(i, j) );
        }
    }

    // Accessing matrix element out of range or through undefied residue
    // causes exception
    unsigned int num = smat.GetNumResidues();
    BOOST_CHECK_THROW( smat.GetScore(0, num), CGeneralScoreMatrixException );
    BOOST_CHECK_THROW( smat.GetScore('A', '!'), CGeneralScoreMatrixException );
}




#endif /* SKIP_DOXYGEN_PROCESSING */
