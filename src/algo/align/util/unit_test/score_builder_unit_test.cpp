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
* Author:  Christiam Camacho, NCBI
*
* File Description:
*   Unit tests for the CScoreBuilder class.
*
* NOTE:
*   Boost.Test reports some memory leaks when compiled in MSVC even for this
*   simple code. Maybe it's related to some toolkit static variables.
*   To avoid annoying messages about memory leaks run this program with
*   parameter --detect_memory_leak=0
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

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


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <algo/align/util/score_builder.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BOOST_AUTO_TEST_CASE(TestDnaAlignment)
{
    const string kFileName("data/seqalign.asn");
    ifstream in(kFileName.c_str());
    CSeq_align alignment;
    in >> MSerial_AsnText >> alignment;

    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    CScoreBuilder score_builder(blast::eBlastn);

    const int kExpectedNumIdent = 37325;
    int num_ident = score_builder.GetIdentityCount(*scope, alignment);
    BOOST_REQUIRE_EQUAL(kExpectedNumIdent, num_ident);
    bool rc = true;
    rc = alignment.GetNamedScore(CSeq_align::eScore_IdentityCount, num_ident);
    BOOST_REQUIRE_EQUAL(false, rc);

    const int kExpectedScore = 74180;
    int score = score_builder.GetBlastScore(*scope, alignment);
    BOOST_REQUIRE_EQUAL(kExpectedScore, score);
    rc = alignment.GetNamedScore(CSeq_align::eScore_Score, score);
    BOOST_REQUIRE_EQUAL(rc, false);

}
