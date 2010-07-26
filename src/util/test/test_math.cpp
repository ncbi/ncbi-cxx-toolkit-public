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
* Author:  Pavel Ivanov, Aleksey Grichenko, NCBI
*
* File Description:
*   Unit test for CFormatGuess class
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <util/math/matrix.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;


BOOST_AUTO_TEST_CASE(TestMatrixTranspose)
{
    CNcbiMatrix<float> mat1(3, 3);
    mat1(0, 0) = 1;
    mat1(0, 1) = 2;
    mat1(0, 2) = 3;

    mat1(1, 0) = 1;
    mat1(1, 1) = 2;
    mat1(1, 2) = 3;

    mat1(2, 0) = 1;
    mat1(2, 1) = 2;
    mat1(2, 2) = 3;

    mat1.Transpose();

    CNcbiMatrix<float> answer(3, 3);
    answer(0, 0) = 1;
    answer(0, 1) = 1;
    answer(0, 2) = 1;

    answer(1, 0) = 2;
    answer(1, 1) = 2;
    answer(1, 2) = 2;

    answer(2, 0) = 3;
    answer(2, 1) = 3;
    answer(2, 2) = 3;

    BOOST_CHECK_EQUAL(mat1, answer);
}


BOOST_AUTO_TEST_CASE(TestMatrixMultiply)
{
    {{
         CNcbiMatrix<float> mat1(3, 3);
         CNcbiMatrix<float> mat2(3, 3);

         mat1.Identity();
         mat2(0, 0) = 1;
         mat2(0, 1) = 2;
         mat2(0, 2) = 3;
         mat2(1, 0) = 1;
         mat2(1, 1) = 2;
         mat2(1, 2) = 3;
         mat2(2, 0) = 1;
         mat2(2, 1) = 2;
         mat2(2, 2) = 3;

         CNcbiMatrix<float> mat3(mat2);
         mat3 *= mat1;
         BOOST_CHECK_EQUAL(mat2, mat3);
     }}

    {{
         CNcbiMatrix<float> mat1(3, 4);
         CNcbiMatrix<float> mat2(4, 3);

         mat1(0, 0) = 1;
         mat1(0, 1) = 2;
         mat1(0, 2) = 3;
         mat1(0, 3) = 4;

         mat1(1, 0) = 1;
         mat1(1, 1) = 2;
         mat1(1, 2) = 3;
         mat1(1, 3) = 4;

         mat1(2, 0) = 1;
         mat1(2, 1) = 2;
         mat1(2, 2) = 3;
         mat1(2, 3) = 4;

         mat2(0, 0) = 1;
         mat2(0, 1) = 2;
         mat2(0, 2) = 3;

         mat2(1, 0) = 1;
         mat2(1, 1) = 2;
         mat2(1, 2) = 3;

         mat2(2, 0) = 1;
         mat2(2, 1) = 2;
         mat2(2, 2) = 3;

         mat2(3, 0) = 1;
         mat2(3, 1) = 2;
         mat2(3, 2) = 3;

         CNcbiMatrix<float> mat3(mat2);
         mat3 *= mat1;

         CNcbiMatrix<float> answer(4, 4);
         answer(0, 0) =  6;
         answer(0, 1) = 12;
         answer(0, 2) = 18;
         answer(0, 3) = 24;

         answer(1, 0) =  6;
         answer(1, 1) = 12;
         answer(1, 2) = 18;
         answer(1, 3) = 24;

         answer(2, 0) =  6;
         answer(2, 1) = 12;
         answer(2, 2) = 18;
         answer(2, 3) = 24;

         answer(3, 0) =  6;
         answer(3, 1) = 12;
         answer(3, 2) = 18;
         answer(3, 3) = 24;

         BOOST_CHECK_EQUAL(mat3, answer);
     }}
}




