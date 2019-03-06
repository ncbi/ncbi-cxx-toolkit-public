/*****************************************************************************
 *  $Id$
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
 *  Db Cassandra: class generating execution plans for cassandra table fullscans.
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/fullscan/plan.hpp>

#include <vector>
#include <string>
#include <utility>
#include <memory>
#include <algorithm>

#include <corelib/ncbistr.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassandraFullscanPlan::CCassandraFullscanPlan()
    : m_Connection(nullptr)
    , m_FieldList({"*"})
    , m_MinPartitionsForSubrangeScan(kMinPartitionsForSubrangeScanDefault)
{
}

CCassandraFullscanPlan& CCassandraFullscanPlan::SetConnection(shared_ptr<CCassConnection> connection)
{
    swap(m_Connection, connection);
    return *this;
}

CCassandraFullscanPlan& CCassandraFullscanPlan::SetFieldList(vector<string> fields)
{
    m_FieldList = move(fields);
    return *this;
}

CCassandraFullscanPlan& CCassandraFullscanPlan::SetWhereFilter(string const & where_filter)
{
    m_WhereFilter = where_filter;
    return *this;
}

CCassandraFullscanPlan& CCassandraFullscanPlan::SetMinPartitionsForSubrangeScan(size_t value)
{
    m_MinPartitionsForSubrangeScan = value;
    return *this;
}

CCassandraFullscanPlan& CCassandraFullscanPlan::SetKeyspace(string const & keyspace)
{
    m_Keyspace = keyspace;
    return *this;
}

CCassandraFullscanPlan& CCassandraFullscanPlan::SetTable(string const & table)
{
    m_Table = table;
    return *this;
}

size_t CCassandraFullscanPlan::GetMinPartitionsForSubrangeScan()
{
    return m_MinPartitionsForSubrangeScan;
}

size_t CCassandraFullscanPlan::GetPartitionCountEstimate()
{
    string datacenter, schema, schema_bytes;
    int64_t peers_count = 0, partition_count = 0;

    shared_ptr<CCassQuery> query = m_Connection->NewQuery();
    query->SetSQL("SELECT data_center, schema_version, uuidAsBlob(schema_version) FROM system.local", 0);
    query->Query(CassConsistency::CASS_CONSISTENCY_LOCAL_ONE, false, false);
    query->NextRow();
    datacenter = query->FieldGetStrValue(0);
    schema = query->FieldGetStrValue(1);
    schema_bytes = query->FieldGetStrValue(2);
    ERR_POST(Trace << "CCassandraFullscanPlanner::GetTableRowsCountEstimate - Datacenter  '" << datacenter << "'");
    ERR_POST(Trace << "CCassandraFullscanPlanner::GetTableRowsCountEstimate - Schema  '" << schema << "'");
    ERR_POST(Trace << "CCassandraFullscanPlanner::GetTableRowsCountEstimate - Bytes size " << schema_bytes.size());

    query = m_Connection->NewQuery();
    query->SetSQL("SELECT count(*) FROM system.peers WHERE data_center = ? and schema_version = ? ALLOW FILTERING", 2);
    query->BindStr(0, datacenter);
    query->BindBytes(1, reinterpret_cast<const unsigned char*>(schema_bytes.c_str()), schema_bytes.size());
    query->Query(CassConsistency::CASS_CONSISTENCY_LOCAL_ONE, false, false);
    query->NextRow();
    peers_count = query->FieldGetInt64Value(0, 0);
    ERR_POST(Trace << "CCassandraFullscanPlanner::GetTableRowsCountEstimate - Peers count  '" << peers_count << "'");

    query = m_Connection->NewQuery();
    query->SetSQL("SELECT partitions_count FROM system.size_estimates WHERE table_name = ? AND keyspace_name = ?", 2);
    query->BindStr(0, m_Table);
    query->BindStr(1, m_Keyspace);
    query->Query(CassConsistency::CASS_CONSISTENCY_LOCAL_ONE, false, false);
    while (query->NextRow() == ar_dataready) {
        partition_count += query->FieldGetInt64Value(0);
    }

    ERR_POST(Trace << "CCassandraFullscanPlanner::GetTableRowsCountEstimate - "
        "Local rows estimate -  '" << partition_count << "'");
    ERR_POST(Trace << "CCassandraFullscanPlanner::GetTableRowsCountEstimate - "
        "Total rows estimate -  '" << partition_count * (peers_count + 1) << "'");

    return partition_count * (peers_count + 1);
}

CCassandraFullscanPlan::TQueryPtr CCassandraFullscanPlan::GetNextQuery()
{
    shared_ptr<CCassQuery> query;
    if (m_TokenRanges.empty()) {
        return nullptr;
    } else if (m_TokenRanges.size() == 1 && m_TokenRanges[0].first == 0 && m_TokenRanges[0].second == 0) {
        query = m_Connection->NewQuery();
        query->SetSQL(m_SqlTemplate, 0);
    } else {
        query = m_Connection->NewQuery();
        query->SetSQL(m_SqlTemplate, 2);
        query->BindInt64(0, m_TokenRanges.back().first);
        query->BindInt64(1, m_TokenRanges.back().second);
    }
    m_TokenRanges.pop_back();
    return query;
}

size_t CCassandraFullscanPlan::GetQueryCount() const
{
    return m_TokenRanges.size();
}

void CCassandraFullscanPlan::Generate()
{
    if (!m_Connection || m_Keyspace.empty() || m_Table.empty()) {
        NCBI_THROW(CCassandraException, eSeqFailed, "Invalid sequence of operations, connection should be provided");
    }

    m_TokenRanges.clear();
    if (GetPartitionCountEstimate() < m_MinPartitionsForSubrangeScan) {
        m_SqlTemplate = "SELECT " + NStr::Join(m_FieldList, ", ") + " FROM " + m_Keyspace + "." + m_Table;
        if (!m_WhereFilter.empty())
            m_SqlTemplate += " WHERE " + m_WhereFilter + " ALLOW FILTERING";
        m_TokenRanges.push_back(make_pair(0, 0));
    } else {
        vector<string> partition_fields = m_Connection->GetPartitionKeyColumnNames(m_Keyspace, m_Table);
        m_Connection->GetTokenRanges(m_TokenRanges);
        string partition = NStr::Join(partition_fields, ",");
        m_SqlTemplate = "SELECT " + NStr::Join(m_FieldList, ", ") + " FROM "
            + m_Keyspace + "." + m_Table + " WHERE TOKEN(" + partition + ") > ? AND TOKEN(" + partition + ") <= ?";
        if (!m_WhereFilter.empty())
            m_SqlTemplate += " AND " + m_WhereFilter + " ALLOW FILTERING";
        ERR_POST(Trace << "CCassandraFullscanPlanner::Generate - Sql template = '" << m_SqlTemplate << "'");
    }
}

CCassConnection::TTokenRanges& CCassandraFullscanPlan::GetTokenRanges()
{
    return m_TokenRanges;
}

END_IDBLOB_SCOPE
