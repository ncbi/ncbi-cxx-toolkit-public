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
* Author:  Tom Madden
*
* File Description:
*   Unit test module to test utilities in gapinfo.c
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <algo/blast/core/gapinfo.h>

BOOST_AUTO_TEST_SUITE(gapinfo)

BOOST_AUTO_TEST_CASE(testGapEditScriptNew)
{
    const int kSize = 3;
    GapEditScript* esp = NULL;

    esp = GapEditScriptNew(kSize);
    BOOST_REQUIRE_EQUAL(kSize, esp->size);
    esp = GapEditScriptDelete(esp);

    esp = GapEditScriptNew(-1);
    BOOST_REQUIRE_EQUAL((void *)NULL, (void*) esp);
}

BOOST_AUTO_TEST_CASE(testGapEditScriptDup)
{
    const int kSize = 3;
    const int kNums[kSize] = {7, 11, 13};
    const EGapAlignOpType kOptype[kSize] = {eGapAlignSub, eGapAlignDel, eGapAlignIns};
    GapEditScript* esp = GapEditScriptNew(kSize);
    for (int i=0; i<kSize; i++)
    {
         esp->num[i] = kNums[i];
         esp->op_type[i] = kOptype[i];
    }

    GapEditScript* esp_dup = GapEditScriptDup(esp);
    BOOST_REQUIRE_EQUAL(kSize, esp_dup->size);
    for (int i=0; i<kSize; i++)
    {
         BOOST_REQUIRE_EQUAL(kNums[i], esp_dup->num[i]);
         BOOST_REQUIRE_EQUAL(kOptype[i], esp_dup->op_type[i]);
    }
    esp = GapEditScriptDelete(esp);
    esp_dup = GapEditScriptDelete(esp_dup);

}
BOOST_AUTO_TEST_SUITE_END()
