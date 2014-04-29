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
* Author:  Colleen Bollin, NCBI
*
* File Description:
*   Unit tests for biosample_chk.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include "unit_test_biosample_chk.hpp"

#include <corelib/ncbi_system.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#define NCBI_BOOST_NO_AUTO_TEST_MAIN

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <common/ncbi_export.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CheckDiffs(const TFieldDiffList& expected, const TFieldDiffList& observed)
{
    TFieldDiffList::const_iterator ex = expected.begin();
    TFieldDiffList::const_iterator ob = observed.begin();
    while (ex != expected.end() && ob != observed.end()) {
        string ex_str = (*ex)->GetFieldName() + ":" 
                        + (*ex)->GetSrcVal() + ":"
                        + (*ex)->GetSampleVal();
        string ob_str = (*ob)->GetFieldName() + ":" 
                        + (*ob)->GetSrcVal() + ":"
                        + (*ob)->GetSampleVal();
        BOOST_CHECK_EQUAL(ex_str, ob_str);
        ex++;
        ob++;
    }
    while (ex != expected.end()) {
        string ex_str = (*ex)->GetFieldName() + ":" 
                        + (*ex)->GetSrcVal() + ":"
                        + (*ex)->GetSampleVal();
        BOOST_CHECK_EQUAL(ex_str, "");
        ex++;
    }
    while (ob != observed.end()) {
        string ob_str = (*ob)->GetFieldName() + ":" 
                        + (*ob)->GetSrcVal() + ":"
                        + (*ob)->GetSampleVal();
        BOOST_CHECK_EQUAL("", ob_str);
        ob++;
    }
}


BOOST_AUTO_TEST_CASE(Test_GetBiosampleDiffs)
{
    CRef<CBioSource> test_src(new CBioSource());
    CRef<CBioSource> test_sample(new CBioSource());

    test_src->SetOrg().SetTaxname("A");
    test_sample->SetOrg().SetTaxname("B");

    TFieldDiffList expected;
    expected.push_back(CRef<CFieldDiff>(new CFieldDiff("Organism Name", "A", "B")));
    TFieldDiffList diff_list = test_src->GetBiosampleDiffs(*test_sample);
    BOOST_CHECK_EQUAL(diff_list.size(), 1);

    CheckDiffs(expected, diff_list);
}


END_SCOPE(objects)
END_NCBI_SCOPE
