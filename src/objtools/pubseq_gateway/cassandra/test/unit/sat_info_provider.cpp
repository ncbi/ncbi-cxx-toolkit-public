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

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include <corelib/ncbireg.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

BEGIN_IDBLOB_SCOPE

bool operator==(const SSatInfoEntry& a, const SSatInfoEntry& b) {
    return a.keyspace == b.keyspace
        && a.connection == b.connection
        && a.schema_type == b.schema_type
        && a.sat == b.sat;
}

END_IDBLOB_SCOPE


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

        m_Registry.Set(m_ConfigSectionSecure, "service", string(m_TestClusterName), IRegistry::fPersistent);
        m_Registry.Set(m_ConfigSectionSecure, "namespace", string(m_SecureKeyspace), IRegistry::fPersistent);
        m_Registry.Set(m_ConfigSectionSecure, "qtimeout", "11111", IRegistry::fPersistent);

        m_Registry.Set(m_ConfigSectionTimeout, "service", string(m_TestClusterName), IRegistry::fPersistent);
        m_Registry.Set(m_ConfigSectionTimeout, "qtimeout", "1", IRegistry::fPersistent);

        m_RegistryPtr = make_shared<CCompoundRegistry>();
        m_RegistryPtr->Add(m_Registry);

        m_Factory = CCassConnectionFactory::s_Create();
        m_Factory->LoadConfig(m_RegistryPtr.get(), m_ConfigSection);
        m_Connection = m_Factory->CreateInstance();
        m_Connection->Connect();

        m_FactoryTimeout = CCassConnectionFactory::s_Create();
        m_FactoryTimeout->LoadConfig(m_RegistryPtr.get(), m_ConfigSectionTimeout);
        m_ConnectionTimeout = m_FactoryTimeout->CreateInstance();
        m_ConnectionTimeout->Connect();
    }

    static void TearDownTestCase() {
        m_Connection->Close();
        m_Connection = nullptr;
        m_Factory = nullptr;
    }

    static const char* m_TestClusterName;
    static const char * m_ConfigSection;
    static const char * m_ConfigSectionTimeout;
    static const char * m_ConfigSectionSecure;
    static const char * m_SecureKeyspace;
    static shared_ptr<CCassConnectionFactory> m_Factory;
    static shared_ptr<CCassConnectionFactory> m_FactoryTimeout;
    static shared_ptr<CCassConnection> m_Connection;
    static shared_ptr<CCassConnection> m_ConnectionTimeout;
    static CNcbiRegistry m_Registry;
    static shared_ptr<CCompoundRegistry> m_RegistryPtr;

    string m_KeyspaceName{"sat_info3"};
};

const char* CSatInfoProviderTest::m_TestClusterName = "ID_CASS_TEST";
const char* CSatInfoProviderTest::m_ConfigSection = "TEST";
const char* CSatInfoProviderTest::m_ConfigSectionSecure = "TEST_SECURE";
const char* CSatInfoProviderTest::m_SecureKeyspace = "sat_info_secure";
const char* CSatInfoProviderTest::m_ConfigSectionTimeout = "TEST_TIMEOUT";
shared_ptr<CCassConnectionFactory> CSatInfoProviderTest::m_Factory(nullptr);
shared_ptr<CCassConnectionFactory> CSatInfoProviderTest::m_FactoryTimeout(nullptr);
shared_ptr<CCassConnection> CSatInfoProviderTest::m_Connection(nullptr);
shared_ptr<CCassConnection> CSatInfoProviderTest::m_ConnectionTimeout(nullptr);
CNcbiRegistry CSatInfoProviderTest::m_Registry;
shared_ptr<CCompoundRegistry> CSatInfoProviderTest::m_RegistryPtr(nullptr);

TEST_F(CSatInfoProviderTest, Smoke) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT1", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUpdated, provider.RefreshSchema(false));
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUpdated, provider.RefreshSchema(false));
    EXPECT_EQ("", provider.GetRefreshErrorMessage());
}

TEST_F(CSatInfoProviderTest, DomainDoesNotExist) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "DomainDoesNotExist", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoSat2KeyspaceEmpty, provider.RefreshSchema(false));
    EXPECT_FALSE(provider.GetRefreshErrorMessage().empty());
}

TEST_F(CSatInfoProviderTest, EmptySatInfoName) {
    CSatInfoSchemaProvider provider(
        "", "PSG_CASS_UNIT1", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoKeyspaceUndefined, provider.RefreshSchema(false));
    EXPECT_EQ("mapping_keyspace is not specified", provider.GetRefreshErrorMessage());
}

TEST_F(CSatInfoProviderTest, Basic) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT1", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUpdated, provider.RefreshSchema(true));
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUnchanged, provider.RefreshSchema(false));
    auto sat4 = provider.GetBlobKeyspace(4);
    auto sat5 = provider.GetBlobKeyspace(5);
    auto sat23 = provider.GetBlobKeyspace(23);
    auto sat24 = provider.GetBlobKeyspace(24);
    auto sat52 = provider.GetBlobKeyspace(52);
    auto sat1000 = provider.GetBlobKeyspace(1000);
    ASSERT_TRUE(sat4.has_value());
    ASSERT_TRUE(sat5.has_value());
    ASSERT_TRUE(sat23.has_value());
    ASSERT_TRUE(sat24.has_value());
    ASSERT_FALSE(sat1000.has_value());

    auto ipg_keyspace = provider.GetIPGKeyspace();
    ASSERT_TRUE(ipg_keyspace.has_value());

    ASSERT_NE(nullptr, ipg_keyspace.value().connection);
    ASSERT_NE(nullptr, provider.GetResolverKeyspace().connection);
    ASSERT_NE(nullptr, sat4.value().connection);
    ASSERT_NE(nullptr, sat5.value().connection);
    ASSERT_NE(nullptr, sat23.value().connection);
    ASSERT_NE(nullptr, sat24.value().connection);
    ASSERT_NE(nullptr, sat52.value().connection);

    ASSERT_EQ(sat52->GetConnection(), sat52->connection);

    ASSERT_EQ("idmain2", provider.GetResolverKeyspace().keyspace);
    ASSERT_EQ("ipg_storage", ipg_keyspace.value().keyspace);
    ASSERT_EQ("satncbi_extended", sat4.value().keyspace);
    ASSERT_EQ("satprot_v2", sat5.value().keyspace);
    ASSERT_EQ("satddbj_wgs", sat23.value().keyspace);
    ASSERT_EQ("satold03", sat24.value().keyspace);
    ASSERT_EQ("nannotg3", sat52.value().keyspace);

    EXPECT_EQ(sat4.value().connection.get(), sat5.value().connection.get());
    EXPECT_EQ(sat4.value().connection.get(), sat24.value().connection.get());
    EXPECT_NE(sat4.value().connection.get(), sat23.value().connection.get());

    EXPECT_EQ("DC1", sat4.value().connection->GetDatacenterName());
    EXPECT_EQ("BETHESDA", sat23.value().connection->GetDatacenterName());

    vector<SSatInfoEntry> na_keyspaces = {sat52.value()};
    EXPECT_EQ(na_keyspaces, provider.GetNAKeyspaces());

    // Reload from other domain
    provider.SetDomain("PSG_CASS_UNIT2");
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUpdated, provider.RefreshSchema(false));
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUpdated, provider.RefreshSchema(true));

    auto sat23_reloaded = provider.GetBlobKeyspace(23);
    ASSERT_TRUE(sat23_reloaded.has_value());
    // Connection should be inherited from previous schema version (no Cassandra reconnect)
    EXPECT_EQ(sat23_reloaded.value().connection.get(), sat23.value().connection.get());
    EXPECT_EQ(230, provider.GetMaxBlobKeyspaceSat());

    // Reload from other domain
    provider.SetDomain("PSG_CASS_UNIT2.1");
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUpdated, provider.RefreshSchema(false));
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUpdated, provider.RefreshSchema(true));

    auto sat230 = provider.GetBlobKeyspace(230);
    ASSERT_TRUE(sat230.has_value());
    // Connection should be inherited from previous schema version (no Cassandra reconnect)
    EXPECT_EQ(sat230.value().connection.get(), sat23.value().connection.get());

    //cout << provider.GetSchema()->ToString() << endl;
}

TEST_F(CSatInfoProviderTest, SecureSat) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT_SECURE", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );

    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUpdated, provider.RefreshSchema(true));
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUnchanged, provider.RefreshSchema(false));
    auto sat4 = provider.GetBlobKeyspace(4);
    auto sat5 = provider.GetBlobKeyspace(5);
    ASSERT_TRUE(sat4.has_value());
    ASSERT_TRUE(sat5.has_value());

    EXPECT_FALSE(sat4->IsSecureSat());
    EXPECT_NE(nullptr, sat4->connection);
    EXPECT_THROW(sat4->GetSecureConnection(""), CCassandraException);

    // Secure configuration section is not loaded
    EXPECT_TRUE(sat5->IsSecureSat());
    EXPECT_EQ(nullptr, sat5->connection);
    EXPECT_EQ(nullptr, sat5->GetSecureConnection("random_user"));
    EXPECT_EQ(nullptr, sat5->GetSecureConnection("secure_user"));
    EXPECT_THROW(sat5->GetConnection(), CCassandraException);

    provider.SetSecureSatRegistrySection(m_ConfigSectionSecure);
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUpdated, provider.RefreshSchema(true));
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUnchanged, provider.RefreshSchema(false));
    sat4 = provider.GetBlobKeyspace(4);
    sat5 = provider.GetBlobKeyspace(5);
    ASSERT_TRUE(sat4.has_value());
    ASSERT_TRUE(sat5.has_value());

    EXPECT_FALSE(sat4->IsSecureSat());
    EXPECT_FALSE(sat4->IsFrozenSat());
    ASSERT_NE(nullptr, sat4->connection);
    EXPECT_EQ("DC1", sat4.value().connection->GetDatacenterName());

    // Secure configuration section is loaded
    EXPECT_TRUE(sat5->IsSecureSat());
    EXPECT_EQ(nullptr, sat5->connection);
    ASSERT_NE(nullptr, sat5->GetSecureConnection("secure_user"));
    EXPECT_EQ(nullptr, sat5->GetSecureConnection("random_user"));
    EXPECT_EQ(nullptr, sat5->GetSecureConnection(""));
    EXPECT_NE(sat4->connection.get(), sat5->GetSecureConnection("secure_user").get());
    EXPECT_EQ("DC1", sat5->GetSecureConnection("secure_user")->GetDatacenterName());

    //cout << provider.GetSchema()->ToString() << endl;
}

TEST_F(CSatInfoProviderTest, NotLoaded) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT1", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );
    ASSERT_EQ(nullopt, provider.GetBlobKeyspace(23));
    ASSERT_EQ(nullptr, provider.GetResolverKeyspace().connection);
    ASSERT_EQ("", provider.GetResolverKeyspace().keyspace);
    ASSERT_TRUE(provider.GetNAKeyspaces().empty());
    EXPECT_EQ(-1, provider.GetMaxBlobKeyspaceSat());
}

TEST_F(CSatInfoProviderTest, ServiceResolutionErrors) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT3", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eLbsmServiceNotResolved, provider.RefreshSchema(true));
    EXPECT_FALSE(provider.GetRefreshErrorMessage().empty());

    provider.SetDomain("PSG_CASS_UNIT4");
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eLbsmServiceNotResolved, provider.RefreshSchema(true));
    EXPECT_FALSE(provider.GetRefreshErrorMessage().empty());

    provider.SetDomain("PSG_CASS_UNIT5");
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eLbsmServiceNotResolved, provider.RefreshSchema(true));
    EXPECT_FALSE(provider.GetRefreshErrorMessage().empty());
}

TEST_F(CSatInfoProviderTest, ResolverKeyspaceDuplicated) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT6", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eResolverKeyspaceDuplicated, provider.RefreshSchema(true));
}

TEST_F(CSatInfoProviderTest, ResolverKeyspaceUndefined) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT7", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eResolverKeyspaceUndefined, provider.RefreshSchema(true));

    provider.SetResolverKeyspaceRequired(false);
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUpdated, provider.RefreshSchema(true));
}

TEST_F(CSatInfoProviderTest, BlobKeyspacesUndefined) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT8", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eBlobKeyspacesEmpty, provider.RefreshSchema(true));
}

TEST_F(CSatInfoProviderTest, EmptyMessages) {
    {
        CSatInfoSchemaProvider provider(
            m_KeyspaceName, "PSG_CASS_UNIT7", m_Connection,
            m_RegistryPtr, m_ConfigSection
        );
        EXPECT_EQ("", provider.GetMessage("ANY"));
        EXPECT_EQ(ESatInfoRefreshMessagesResult::eSatInfoMessagesEmpty, provider.RefreshMessages(false));
    }
    {
        CSatInfoSchemaProvider provider(
            "", "PSG_CASS_UNIT7", m_Connection,
            m_RegistryPtr, m_ConfigSection
        );
        EXPECT_EQ(ESatInfoRefreshMessagesResult::eSatInfoKeyspaceUndefined, provider.RefreshMessages(false));
    }
}

TEST_F(CSatInfoProviderTest, BasicMessages) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT1", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );
    EXPECT_EQ(ESatInfoRefreshMessagesResult::eMessagesUpdated, provider.RefreshMessages(false));
    EXPECT_EQ(ESatInfoRefreshMessagesResult::eMessagesUpdated, provider.RefreshMessages(true));
    EXPECT_EQ(ESatInfoRefreshMessagesResult::eMessagesUnchanged, provider.RefreshMessages(true));
    EXPECT_EQ("MESSAGE1_TEXT", provider.GetMessage("MESSAGE1"));
    EXPECT_EQ("", provider.GetMessage("MESSAGE2"));
}

TEST_F(CSatInfoProviderTest, CassException) {
    {
        CSatInfoSchemaProvider provider(
            "NON_EXISTENT_KEYSPACE", "PSG_CASS_UNIT1", m_Connection,
            m_RegistryPtr, m_ConfigSection
        );
        EXPECT_THROW(provider.RefreshMessages(true), CCassandraException);
        EXPECT_THROW(provider.RefreshSchema(true), CCassandraException);
    }
    {
        EXPECT_THROW(
            CSatInfoSchemaProvider(
                m_KeyspaceName, "PSG_CASS_UNIT1", nullptr,
                m_RegistryPtr, m_ConfigSection
            ),
            CCassandraException
        );
    }
    {
        CSatInfoSchemaProvider provider(
            m_KeyspaceName, "PSG_CASS_UNIT1", m_Connection,
            m_RegistryPtr, m_ConfigSection
        );
        provider.RefreshSchema(true);
        provider.SetSecureSatRegistrySection("NON_EXISTENT_SECTION");
        EXPECT_THROW(provider.RefreshSchema(true), CCassandraException);
    }
}

TEST_F(CSatInfoProviderTest, PsgDomain) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_TEST", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );
    provider.SetSecureSatRegistrySection(m_ConfigSectionSecure);
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUpdated, provider.RefreshSchema(true));
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUnchanged, provider.RefreshSchema(false));

    //cout << provider.GetSchema()->ToString() << endl;
}

TEST_F(CSatInfoProviderTest, SchemaProviderTimeoutOverride) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT1", m_ConnectionTimeout,
        m_RegistryPtr, m_ConfigSectionTimeout
    );
    EXPECT_EQ(1UL, m_ConnectionTimeout->QryTimeoutMs());
    //EXPECT_THROW(provider.RefreshSchema(true), CCassandraException);
    provider.SetTimeout(chrono::seconds(1));
    provider.RefreshSchema(true);
}

TEST_F(CSatInfoProviderTest, JsonServiceValueEmptyBrackets) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT9", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );
    testing::internal::CaptureStderr();
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eServiceFieldParseError, provider.RefreshSchema(true));
    auto stderr = testing::internal::GetCapturedStderr();
    EXPECT_EQ("Cannot parse service field value: '{}'", provider.GetRefreshErrorMessage());
    EXPECT_EQ("Warning: Failed to parse JSON value for 'service' field:"
              " 'Value for datacenter (DC1) not found' - '{}'\n", stderr);
}

TEST_F(CSatInfoProviderTest, JsonServiceValueNotAString) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT12", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );
    testing::internal::CaptureStderr();
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eServiceFieldParseError, provider.RefreshSchema(true));
    auto stderr = testing::internal::GetCapturedStderr();
    EXPECT_EQ("Cannot parse service field value: '{\"DC1\":2}'", provider.GetRefreshErrorMessage());
    EXPECT_EQ("Warning: Failed to parse JSON value for 'service' field:"
              " 'Value for datacenter (DC1) key is not a string' - '{\"DC1\":2}'\n", stderr);
}

TEST_F(CSatInfoProviderTest, JsonServiceValue) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT10", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );
        EXPECT_EQ(ESatInfoRefreshSchemaResult::eSatInfoUpdated, provider.RefreshSchema(true));
    EXPECT_EQ("", provider.GetRefreshErrorMessage());
    EXPECT_EQ("ID_CASS_TEST", provider.GetResolverKeyspace().service);
}

TEST_F(CSatInfoProviderTest, JsonServiceValueBrokenJson) {
    CSatInfoSchemaProvider provider(
        m_KeyspaceName, "PSG_CASS_UNIT11", m_Connection,
        m_RegistryPtr, m_ConfigSection
    );
    testing::internal::CaptureStderr();
    EXPECT_EQ(ESatInfoRefreshSchemaResult::eServiceFieldParseError, provider.RefreshSchema(true));
    auto stderr = testing::internal::GetCapturedStderr();
    EXPECT_EQ("Cannot parse service field value: '{\"DC1\":ID_CASS_TEST\"'", provider.GetRefreshErrorMessage());
    EXPECT_EQ("Warning: Failed to parse JSON value for 'service' field:"
              " 'Invalid value.(7)' - '{\"DC1\":ID_CASS_TEST\"'\n", stderr);
}

}  // namespace
