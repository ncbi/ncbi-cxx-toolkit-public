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
*   Unit test suite for CSatInfoSchemaProvider
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <gtest/gtest.h>

#include <corelib/ncbireg.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CSatInfoProviderTest
    : public testing::Test
{
 public:
    CSatInfoProviderTest() = default;

 protected:
    static void SetUpTestCase() {
        m_Registry.Set(m_ConfigSection, "service", string(m_TestClusterName), IRegistry::fPersistent);
        m_Factory = CCassConnectionFactory::s_Create();
        m_Factory->LoadConfig(m_Registry, m_ConfigSection);
        m_Connection = m_Factory->CreateInstance();
        m_Connection->Connect();
    }

    static void TearDownTestCase() {
        m_Connection->Close();
        m_Connection = nullptr;
        m_Factory = nullptr;
    }

    static const char* m_TestClusterName;
    static const char * m_ConfigSection;
    static shared_ptr<CCassConnectionFactory> m_Factory;
    static shared_ptr<CCassConnection> m_Connection;
    static CNcbiRegistry m_Registry;

    string m_KeyspaceName{"sat_info3"};
};

const char* CSatInfoProviderTest::m_TestClusterName = "ID_CASS_TEST";
const char* CSatInfoProviderTest::m_ConfigSection = "TEST";
shared_ptr<CCassConnectionFactory> CSatInfoProviderTest::m_Factory(nullptr);
shared_ptr<CCassConnection> CSatInfoProviderTest::m_Connection(nullptr);
CNcbiRegistry CSatInfoProviderTest::m_Registry;

TEST_F(CSatInfoProviderTest, Smoke) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT1", m_Connection,
        m_Registry, m_ConfigSection
    );
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUpdated, provider.RefreshSchema(false));
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUpdated, provider.RefreshSchema(false));
}

TEST_F(CSatInfoProviderTest, DomainDoesNotExist) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "DomainDoesNotExist", m_Connection,
        m_Registry, m_ConfigSection
    );
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoSat2KeyspaceEmpty, provider.RefreshSchema(false));
}

TEST_F(CSatInfoProviderTest, EmptySatInfoName) {
    CSatInfoSchemaProvider provider(
        "", "PSG_CASS_UNIT1", m_Connection,
        m_Registry, m_ConfigSection
    );
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoKeyspaceUndefined, provider.RefreshSchema(false));
}

TEST_F(CSatInfoProviderTest, Basic) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT1", m_Connection,
        m_Registry, m_ConfigSection
    );
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUpdated, provider.RefreshSchema(true));
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUnchanged, provider.RefreshSchema(false));
    auto sat4 = provider.GetBlobKeyspace(4);
    auto sat5 = provider.GetBlobKeyspace(5);
    auto sat23 = provider.GetBlobKeyspace(23);
    ASSERT_TRUE(sat4.has_value());
    ASSERT_TRUE(sat5.has_value());
    ASSERT_TRUE(sat23.has_value());

    ASSERT_NE(nullptr, provider.GetResolverKeyspace().connection);
    ASSERT_NE(nullptr, sat4.value().connection);
    ASSERT_NE(nullptr, sat5.value().connection);
    ASSERT_NE(nullptr, sat23.value().connection);

    ASSERT_EQ("idmain2", provider.GetResolverKeyspace().keyspace);
    ASSERT_EQ("satncbi_extended", sat4.value().keyspace);
    ASSERT_EQ("satprot_v2", sat5.value().keyspace);
    ASSERT_EQ("satddbj_wgs", sat23.value().keyspace);

    EXPECT_EQ(sat4.value().connection.get(), sat5.value().connection.get());
    EXPECT_NE(sat4.value().connection.get(), sat23.value().connection.get());

    EXPECT_EQ("DC1", sat4.value().connection->GetDatacenterName());
    EXPECT_EQ("BETHESDA", sat23.value().connection->GetDatacenterName());
}

}  // namespace
