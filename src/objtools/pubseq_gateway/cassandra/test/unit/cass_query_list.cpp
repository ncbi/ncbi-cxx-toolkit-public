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
* Author:  Dmitrii Saprykin, NCBI
*
* File Description:
*   Unit test suite to check CCassQueryList class basic API
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <thread>

#include <gtest/gtest.h>

#include <corelib/ncbireg.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_query_list.hpp>

#include "test_environment.hpp"

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CCassQueryListTest
    : public testing::Test
{
public:
    CCassQueryListTest() = default;

protected:
    static void SetUpTestCase() {
        sm_Registry.Get().Set(m_ConfigSection, "service", string(m_TestClusterName), IRegistry::fPersistent);
        sm_CompoundRegistry.Get().Add(sm_Registry.Get());
        sm_Env.Get().SetUp(&sm_CompoundRegistry.Get(), m_ConfigSection);
    }

    static void TearDownTestCase() {
        sm_Env.Get().TearDown();
    }

    static const char* m_TestClusterName;
    static const char * m_ConfigSection;
    static CSafeStatic<STestEnvironment> sm_Env;
    static CSafeStatic<CNcbiRegistry> sm_Registry;
    static CSafeStatic<CCompoundRegistry> sm_CompoundRegistry;

    string m_KeyspaceName{"sat_info3"};
};

const char* CCassQueryListTest::m_TestClusterName = "ID_CASS_TEST";
const char* CCassQueryListTest::m_ConfigSection = "TEST";
CSafeStatic<STestEnvironment> CCassQueryListTest::sm_Env;
CSafeStatic<CNcbiRegistry> CCassQueryListTest::sm_Registry;
CSafeStatic<CCompoundRegistry> CCassQueryListTest::sm_CompoundRegistry;

TEST_F(CCassQueryListTest, CheckThreadOwner)
{
    auto list = CCassQueryList::Create(sm_Env.Get().connection);
    list->Finalize();
    list->Finalize();
    bool exception_observed{false};
    thread t([&list, &exception_observed]() {
        try {
            list->Finalize();
        }
        catch (CCassandraException const &ex) {
            exception_observed = true;
            EXPECT_EQ(CCassandraException::eSeqFailed, ex.GetErrCode());
            EXPECT_EQ("CCassQueryList is a single-thread object", ex.GetMsg());
        }
    });
    t.join();
    EXPECT_TRUE(exception_observed) << "CheckAccess() should throw if called from non owning thread";
}

}  // namespace
