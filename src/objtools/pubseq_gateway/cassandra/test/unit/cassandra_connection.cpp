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
*   Unit test suite to check CCassConnection class
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbireg.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CCassConnectionTest
    : public testing::Test
{
 public:
    CCassConnectionTest()
    {}

 protected:
    static void SetUpTestCase() {
        const string config_section = "TEST";
        CNcbiRegistry r;
        r.Set(config_section, "service", string(m_TestClusterName), IRegistry::fPersistent);
        m_Factory = CCassConnectionFactory::s_Create();
        m_Factory->LoadConfig(r, config_section);
    }

    static void TearDownTestCase() {
        m_Factory = nullptr;
    }

    static const char* m_TestClusterName;
    static shared_ptr<CCassConnectionFactory> m_Factory;
};

const char* CCassConnectionTest::m_TestClusterName = "ID_CASS_TEST";
shared_ptr<CCassConnectionFactory> CCassConnectionTest::m_Factory(nullptr);

TEST_F(CCassConnectionTest, ReconnectShouldNotFail) {
    auto connection = m_Factory->CreateInstance();
    ASSERT_FALSE(connection->IsConnected()) << "Connection should be DOWN before running test";

    // Set impossible hostname to fail connection
    connection->SetConnProp("192.168.111.111", "", "");
    try {
        connection->Connect();
        FAIL() << "Connect should fail for impossible hostname";
    } catch (CCassandraException& exception) {
        ASSERT_EQ(exception.GetErrCode(), 2002) << "Exception code should be CCassandraException::eFailedToConn";
    }

    try {
        connection->Connect();
        FAIL() << "Subsequent connection attempt should fail";
    } catch (CCassandraException& exception) {
        ASSERT_EQ(exception.GetErrCode(), 2002) << "Exception code should be CCassandraException::eFailedToConn";
    }

    ASSERT_FALSE(connection->IsConnected()) << "Connection should be DOWN after 2 failed attempts";

    string host_list;
    int16_t port;
    m_Factory->GetHostPort(host_list, port);
    connection->SetConnProp(host_list, m_Factory->GetUserName(), m_Factory->GetPassword(), port);
    connection->Connect();

    ASSERT_TRUE(connection->IsConnected()) << "Connection should be UP after the reason of failure is fixed";
}

}  // namespace
