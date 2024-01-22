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
*   Unit test suite to check cassandra cluster meta data operations
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
#include <vector>

namespace {

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

class CassandraClusterMetaTest
    : public testing::Test
{
public:
    CassandraClusterMetaTest()
     : m_TestClusterName("ID_CASS_TEST")
     , m_Factory(nullptr)
     , m_Connection(nullptr)
    {}

    virtual void SetUp()
    {
        const string config_section = "TEST";
        CNcbiRegistry r;
        r.Set(config_section, "service", m_TestClusterName, IRegistry::fPersistent);
        m_Factory = CCassConnectionFactory::s_Create();
        m_Factory->LoadConfig(r, config_section);
        m_Connection = m_Factory->CreateInstance();
        m_Connection->Connect();
    }

    virtual void TearDown()
    {
        m_Connection->Close();
        m_Connection = nullptr;
        m_Factory = nullptr;
    }
protected:
    const string m_TestClusterName;
    shared_ptr<CCassConnectionFactory> m_Factory;
    shared_ptr<CCassConnection> m_Connection;
};

TEST_F(CassandraClusterMetaTest, GetPartitionKeyColumnNames) {
    string keyspace_name = "test_ipg_storage_entrez";
    string table_name = "ipg_report";
    vector<string> expected = {"ipg"};
    vector<string> actual = m_Connection->GetPartitionKeyColumnNames(keyspace_name, table_name);
    EXPECT_EQ(expected, actual)
        << "Partition key column list for " << keyspace_name << "." << table_name << " is not equal to expected";

    keyspace_name = "test_mlst_storage";
    table_name = "allele_data";
    expected = {"taxid", "version"};
    actual = m_Connection->GetPartitionKeyColumnNames(keyspace_name, table_name);
    EXPECT_EQ(expected, actual)
        << "Partition key column list for " << keyspace_name << "." << table_name << " is not equal to expected";

    EXPECT_THROW(
        m_Connection->GetPartitionKeyColumnNames("non_existent", table_name),
        CCassandraException
    ) << "GetPartitionKeyColumnNames should throw for non existent keyspace";

    EXPECT_THROW(
        m_Connection->GetPartitionKeyColumnNames(keyspace_name, "non_existent"),
        CCassandraException
    ) << "GetPartitionKeyColumnNames should throw for non existent table";

    m_Connection->Close();
    EXPECT_THROW(
        m_Connection->GetPartitionKeyColumnNames(keyspace_name, table_name),
        CCassandraException
    ) << "GetPartitionKeyColumnNames should throw on closed connection";
}

TEST_F(CassandraClusterMetaTest, GetClusteringKeyColumnNames) {
    string keyspace_name = "test_ipg_storage_entrez";
    string table_name = "ipg_report";
    vector<string> expected = {"accession", "nuc_accession"};
    vector<string> actual = m_Connection->GetClusteringKeyColumnNames(keyspace_name, table_name);
    EXPECT_EQ(expected, actual)
        << "Clustering key column list for " << keyspace_name << "." << table_name << " is not equal to expected";

    keyspace_name = "test_mlst_storage";
    table_name = "allele_data";
    expected = {"locus_name"};
    actual = m_Connection->GetClusteringKeyColumnNames(keyspace_name, table_name);
    EXPECT_EQ(expected, actual)
        << "Clustering key column list for " << keyspace_name << "." << table_name << " is not equal to expected";

    EXPECT_THROW(
        m_Connection->GetClusteringKeyColumnNames("non_existent", table_name),
        CCassandraException
    ) << "GetClusteringKeyColumnNames should throw for non existent keyspace";

    EXPECT_THROW(
        m_Connection->GetClusteringKeyColumnNames(keyspace_name, "non_existent"),
        CCassandraException
    ) << "GetClusteringKeyColumnNames should throw for non existent table";

    m_Connection->Close();
    EXPECT_THROW(
        m_Connection->GetClusteringKeyColumnNames(keyspace_name, table_name),
        CCassandraException
    ) << "GetClusteringKeyColumnNames should throw on closed connection";
}

TEST_F(CassandraClusterMetaTest, GetColumnNames) {
    string keyspace_name = "test_ipg_storage_entrez";
    string table_name = "ipg_report";
    vector<string> expected = { "ipg", "accession", "nuc_accession", "assembly", "bioproject",
        "cds", "compartment", "created", "def_line", "div", "flags", "gb_state", "length",
        "product_name", "pubmedids", "src_db", "src_refseq", "strain", "taxid", "updated", "weights" };
    vector<string> actual = m_Connection->GetColumnNames(keyspace_name, table_name);
    EXPECT_EQ(expected, actual)
        << "Column list for " << keyspace_name << "." << table_name << " is not equal to expected";

    keyspace_name = "test_mlst_storage";
    table_name = "allele_data";
    expected = { "taxid", "version", "locus_name", "created", "gaid", "hash" };
    actual = m_Connection->GetColumnNames(keyspace_name, table_name);
    EXPECT_EQ(expected, actual)
        << "Column list for " << keyspace_name << "." << table_name << " is not equal to expected";

    EXPECT_THROW(
        m_Connection->GetClusteringKeyColumnNames("non_existent", table_name),
        CCassandraException
    ) << "GetColumnNames should throw for non existent keyspace";

    EXPECT_THROW(
        m_Connection->GetClusteringKeyColumnNames(keyspace_name, "non_existent"),
        CCassandraException
    ) << "GetColumnNames should throw for non existent table";

    m_Connection->Close();
    EXPECT_THROW(
        m_Connection->GetClusteringKeyColumnNames(keyspace_name, table_name),
        CCassandraException
    ) << "GetColumnNames should throw on closed connection";
}

TEST_F(CassandraClusterMetaTest, GetKeyspaces) {
    vector<string> expected = {
        "blob_repartition", "dbsnp_rsid_lookup_blue", "dbsnp_rsid_lookup_green",
        "idmain2", "ipg_storage", "maintenance", "mlst_storage", "nannotg2_2", "nannotg3"};
    vector<string> actual = m_Connection->GetKeyspaces();
    sort(begin(expected), end(expected));
    sort(begin(actual), end(actual));
    EXPECT_TRUE(includes(begin(actual), end(actual), begin(expected), end(expected)));
    EXPECT_LE(63UL, actual.size());
}

TEST_F(CassandraClusterMetaTest, GetTables) {
    vector<string> expected = {
        "blob_chunk", "blob_prop", "blob_prop_change_log", "blob_split_history", "blob_status_history" };
    vector<string> actual = m_Connection->GetTables("satold_v2");
    EXPECT_EQ(expected, actual) << "Tables list for 'satold_v2' is not equal to expected";
    EXPECT_THROW(
        m_Connection->GetTables("non_existent"),
        CCassandraException
    ) << "GetTables should throw for non existent keyspace";
}

}  // namespace
