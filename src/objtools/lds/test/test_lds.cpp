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
* Authors:  Yuri Kapustin, Victor Joukov
*
* File Description:
*   Test memory leaks in C++ object manager
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>

#include <objtools/lds/lds_admin.hpp>
#include <objtools/data_loaders/lds/lds_dataloader.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objects/seqloc/Seq_id.hpp>

// Boost unit test stuff, copied over from example
// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>
#ifndef BOOST_PARAM_TEST_CASE
#  include <boost/test/parameterized_test.hpp>
#endif
#ifndef BOOST_AUTO_TEST_CASE
#  define BOOST_AUTO_TEST_CASE BOOST_AUTO_UNIT_TEST
#endif


USING_NCBI_SCOPE;

// Boost stuff again
using boost::unit_test::test_suite;

// Use macros rather than inline functions to get accurate line number reports

// #define CHECK_NO_THROW(statement) BOOST_CHECK_NO_THROW(statement)
#define CHECK_NO_THROW(statement)                                       \
    try {                                                               \
        statement;                                                      \
        BOOST_CHECK_MESSAGE(true, "no exceptions were thrown by "#statement); \
    } catch (std::exception& e) {                                       \
        BOOST_ERROR("an exception was thrown by "#statement": " << e.what()); \
    } catch (...) {                                                     \
        BOOST_ERROR("a nonstandard exception was thrown by "#statement); \
    }

#define CHECK(expr)       CHECK_NO_THROW(BOOST_CHECK(expr))
#define CHECK_EQUAL(x, y) CHECK_NO_THROW(BOOST_CHECK_EQUAL(x, y))
#define CHECK_THROW(s, x) BOOST_CHECK_THROW(s, x)

#define CHECK_THROW_STD_(statement, xtype)                              \
    try {                                                               \
        statement;                                                      \
        BOOST_ERROR(#statement" failed to throw "#xtype);               \
    } catch (xtype& e) {                                                \
        BOOST_MESSAGE(#statement" properly threw "#xtype": " << e.what()); \
    }
#define CHECK_THROW_STD(s, x) CHECK_NO_THROW(CHECK_THROW_STD_(s, x))

static const char* kLdsDbDirName = "_LDS_";
static const char* kLdsDbName    = "test.ldsdb";
static const char* kDataDir      = "test.data";

static const unsigned kTestLengths[] = {
    1041,
    194,
    194,
    803,
    803
};

static const char* kTestIds[] = {
    "lcl|AT1G51370.2",
    "gi|2",
    "pat|EP|0238993|1",
    "ref|YP_854532",
    "gnl|REF_Tigr|AHA_0004"
};
static const size_t kNumTestIds
= sizeof(kTestIds)/sizeof(*kTestIds);


BOOST_AUTO_TEST_CASE(build_lds_index)
{
    CLDS_Database ldsdb (kLdsDbDirName, kLdsDbName);
    CLDS_Management ldsmgt (ldsdb);
    ldsmgt.Create();
    ldsmgt.SyncWithDir(kDataDir,
                        CLDS_Management::eRecurseSubDirs,
                        CLDS_Management::eNoControlSum);
}


BOOST_AUTO_TEST_CASE(use_lds_index)
{
    CRef<CObjectManager> objmgr (CObjectManager::GetInstance());

    CLDS_Database* ldsdb (new CLDS_Database(kLdsDbDirName, kLdsDbName));
    ldsdb->Open();
    CLDS_DataLoader::RegisterInObjectManager(*objmgr, *ldsdb,
                                             CObjectManager::eDefault);
    CRef<CScope> scope(new CScope(*objmgr));
    scope->AddDefaults();

    for (size_t n = 0; n < kNumTestIds; ++n) {
        CSeq_id seqid (kTestIds[n]);
        CHECK_EQUAL(kTestLengths[n],
                    sequence::GetLength(seqid, scope.GetNonNullPointer()));
    }
}
