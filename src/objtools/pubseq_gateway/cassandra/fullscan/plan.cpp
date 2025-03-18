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
#include <cmath>
#include <memory>
#include <algorithm>

#include <corelib/ncbistr.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

namespace {

// In multidc environment size estimates are incomplete
//    (contain data for local primary ranges only).
// We will fill gaps using neighboring ranges data.
vector<SCassSizeEstimate> NormalizeSizeEstimates(vector<SCassSizeEstimate> const & input)
{
    vector<SCassSizeEstimate> output;
    output.reserve(input.size() * 3);
    int64_t current_range_start = numeric_limits<int64_t>::min();
    for (auto const & estimate : input) {
        if (estimate.range_start > current_range_start) {
            SCassSizeEstimate new_estimate;
            new_estimate.range_start = current_range_start;
            new_estimate.range_end = estimate.range_start;
            new_estimate.mean_partition_size = estimate.mean_partition_size;
            double size_ratio =
                (1.0 * (new_estimate.range_end - new_estimate.range_start)) /
                (estimate.range_end - estimate.range_start);
            new_estimate.partitions_count = static_cast<int64_t>(size_ratio * estimate.partitions_count);
            output.push_back(new_estimate);
        }
        current_range_start = estimate.range_end;
        output.push_back(estimate);
    }
    if (current_range_start < numeric_limits<int64_t>::max() && !input.empty()) {
        SCassSizeEstimate new_estimate;
        new_estimate.range_start = current_range_start;
        new_estimate.range_end = numeric_limits<int64_t>::max();
        auto last = input.rbegin();
        new_estimate.mean_partition_size = last->mean_partition_size;
        double size_ratio =
            (1.0 * (new_estimate.range_end - new_estimate.range_start)) /
            (last->range_end - last->range_start);
        new_estimate.partitions_count = static_cast<int64_t>(size_ratio * last->partitions_count);
        output.push_back(new_estimate);
    }
    return output;
}

// @todo in the future leave for Debug only
void VerifySizeEstimates(vector<SCassSizeEstimate> const & estimates)
{
    if (
        estimates.empty()
        || estimates.cbegin()->range_start != numeric_limits<int64_t>::min()
        || estimates.crbegin()->range_end != numeric_limits<int64_t>::max()
    ) {
        NCBI_THROW(CCassandraException, eFatal, "Token range size estimates empty or have wrong min/max");
    }
    for (auto itr = estimates.cbegin(); itr != estimates.cend(); ++itr) {
        if (itr->range_start >= itr->range_end) {
            NCBI_THROW_FMT(CCassandraException, eFatal, "Token range has wrong borders: "
                << itr->range_start << ":" << itr->range_end);
        }
        if (itr->partitions_count < 0) {
            NCBI_THROW_FMT(CCassandraException, eFatal, "Token range has wrong partitions_count: "
                << itr->range_start << ":" << itr->range_end << " - " << itr->partitions_count);
        }
        auto next = itr + 1;
        if (next != estimates.cend() && itr->range_end != next->range_start) {
            NCBI_THROW_FMT(CCassandraException, eFatal, "Adjacent ranges have gap: "
                << itr->range_start << ":" << itr->range_end
                << " and " << next->range_start << ":" << next->range_end);
        }
    }
}

}

CCassandraFullscanPlan::CCassandraFullscanPlan() = default;

CCassandraFullscanPlan& CCassandraFullscanPlan::SetConnection(shared_ptr<CCassConnection> connection)
{
    swap(m_Connection, connection);
    return *this;
}

CCassandraFullscanPlan& CCassandraFullscanPlan::SetFieldList(vector<string> fields)
{
    m_FieldList = std::move(fields);
    return *this;
}

CCassandraFullscanPlan& CCassandraFullscanPlan::SetWhereFilter(string const & where_filter)
{
    m_WhereFilter = where_filter;
    return *this;
}

CCassandraFullscanPlan& CCassandraFullscanPlan::SetWhereFilter(
    string const & sql, unsigned int params_count, TParamsBinder params_binder
)
{
    m_WhereFilter = sql;
    m_WhereFilterParamsCount = params_count;
    m_WhereFilterParamsBinder = std::move(params_binder);
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

CCassandraFullscanPlan& CCassandraFullscanPlan::SetPartitionCountPerQueryLimit(int64_t value)
{
    if (value < 0) {
        ERR_POST(Warning << "CCassandraFullscanPlanner::SetPartitionCountPerQueryLimit - wrong value ignored '" << value << "'");
    } else {
        m_PartitionCountPerQueryLimit = value;
    }
    return *this;
}

size_t CCassandraFullscanPlan::GetMinPartitionsForSubrangeScan()
{
    return m_MinPartitionsForSubrangeScan;
}

size_t CCassandraFullscanPlan::GetPartitionCountEstimate()
{
    string datacenter, schema, schema_bytes;
    int64_t peers_count{0}, partition_count{0};

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
    }
    unsigned int token_count{0};
    if (m_TokenRanges.size() == 1 && m_TokenRanges[0].first == 0 && m_TokenRanges[0].second == 0) {
        query = m_Connection->NewQuery();
        query->SetSQL(m_SqlTemplate, token_count + m_WhereFilterParamsCount);
    }
    else {
        token_count = 2;
        query = m_Connection->NewQuery();
        query->SetSQL(m_SqlTemplate, token_count + m_WhereFilterParamsCount);
        query->BindInt64(0, m_TokenRanges.back().first);
        query->BindInt64(1, m_TokenRanges.back().second);
    }
    if (m_WhereFilterParamsBinder) {
        m_WhereFilterParamsBinder(*query, token_count);
    }
    m_TokenRanges.pop_back();
    return query;
}

size_t CCassandraFullscanPlan::GetQueryCount() const
{
    return m_TokenRanges.size();
}

void CCassandraFullscanPlan::SplitTokenRangesForLimits()
{
    auto local_dc = m_Connection->GetDatacenterName();
    ERR_POST(Trace << "CCassandraFullscanPlan::SplitTokenRangesForLimits - "
        "Local dc -  '" << local_dc << "'");
    auto local_estimates = m_Connection->GetSizeEstimates(local_dc, m_Keyspace, m_Table);
    ERR_POST(Trace << "CCassandraFullscanPlan::SplitTokenRangesForLimits - "
            "Local estimates size -  '" << local_estimates.size() << "'");
    auto estimates = NormalizeSizeEstimates(local_estimates);

    // Additional verification of estimates data
    VerifySizeEstimates(estimates);

    auto search_start = begin(estimates);
    CCassConnection::TTokenRanges result_ranges;
    for (auto const & range : m_TokenRanges) {
        auto range_start = range.first;
        auto range_end = range.second;

        // Search for first intersecting range
        auto itr = search_start;
        while (itr != end(estimates) && itr->range_end <= range_start) {
            ++itr;
        }
        search_start = itr;

        // Counting total partitions from size estimates
        int64_t partitions_count{0};
        while (itr != end(estimates) && itr->range_start < range_end) {
            auto intersect_start = max(itr->range_start, range_start);
            auto intersect_end = min(itr->range_end, range_end);
            double size_ratio =
                (1.0 * (intersect_end - intersect_start)) /
                (itr->range_end - itr->range_start);
            partitions_count += size_ratio * itr->partitions_count;
            ++itr;
        }

        // Split token range if required
        if (partitions_count > m_PartitionCountPerQueryLimit) {
            int64_t parts = static_cast<int64_t>(ceil(1.0 * partitions_count / m_PartitionCountPerQueryLimit));
            if (parts > 1) {
                int64_t step = (range_end - range_start) / parts;
                assert(step > 0);
                auto start = range_start;
                while (start < range_end) {
                    auto end = (range_end - start) < step ? range_end : (start + step);
                    result_ranges.push_back(make_pair(start, end));
                    start = end;
                }
            } else {
                result_ranges.push_back(range);
            }
        } else {
            result_ranges.push_back(range);
        }
    }
    swap(result_ranges, m_TokenRanges);
}

void CCassandraFullscanPlan::Generate()
{
    if (!m_Connection || m_Keyspace.empty() || m_Table.empty()) {
        NCBI_THROW(CCassandraException, eSeqFailed, "Invalid sequence of operations, connection should be provided");
    }

    m_TokenRanges.clear();
    if (GetPartitionCountEstimate() < m_MinPartitionsForSubrangeScan) {
        m_SqlTemplate = "SELECT " + NStr::Join(m_FieldList, ", ") + " FROM " + m_Keyspace + "." + m_Table;
        if (!m_WhereFilter.empty()) {
            m_SqlTemplate += " WHERE " + m_WhereFilter + " ALLOW FILTERING";
        }
        m_TokenRanges.emplace_back(0, 0);
    } else {
        vector<string> partition_fields = m_Connection->GetPartitionKeyColumnNames(m_Keyspace, m_Table);
        m_Connection->GetTokenRanges(m_TokenRanges);
        if (m_PartitionCountPerQueryLimit > 0) {
            SplitTokenRangesForLimits();
        }
        string partition = NStr::Join(partition_fields, ",");
        m_SqlTemplate = "SELECT " + NStr::Join(m_FieldList, ", ") + " FROM "
            + m_Keyspace + "." + m_Table + " WHERE TOKEN(" + partition + ") > ? AND TOKEN(" + partition + ") <= ?";
        if (!m_WhereFilter.empty()) {
            m_SqlTemplate += " AND " + m_WhereFilter + " ALLOW FILTERING";
        }
        ERR_POST(Trace << "CCassandraFullscanPlanner::Generate - Sql template = '" << m_SqlTemplate << "'");
    }
}

CCassConnection::TTokenRanges& CCassandraFullscanPlan::GetTokenRanges()
{
    return m_TokenRanges;
}

END_IDBLOB_SCOPE
