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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 * The functionality not directly related to blob operations
 *
 */

#include <ncbi_pch.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_socket.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/lbsm_resolver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

#define KEYSPACE_MAPPING_CONSISTENCY    CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM
#define KEYSPACE_MAPPING_RETRY          5

BEGIN_IDBLOB_SCOPE

const char* const SBlobStorageConstants::kChunkTableDefault = "blob_chunk";
const char* const SBlobStorageConstants::kChunkTableBig = "big_blob_chunk";

bool FetchSatToKeyspaceMapping(const string &  mapping_keyspace,
                               shared_ptr<CCassConnection>  conn,
                               vector<tuple<string, ECassSchemaType>> &  mapping,
                               string &  resolver_keyspace,
                               ECassSchemaType  resolver_schema,
                               string &  err_msg)
{
    resolver_keyspace.clear();
    mapping.clear();

    if (mapping_keyspace.empty()) {
        err_msg = "mapping_keyspace is not specified";
        return false;
    }

    bool rv = false;
    err_msg = "sat2keyspace info is empty";
    for (int i = KEYSPACE_MAPPING_RETRY; i >= 0; --i) {
        try {
            auto query = conn->NewQuery();
            query->SetSQL("SELECT sat, keyspace_name, schema_type FROM "
                + mapping_keyspace + ".sat2keyspace", 0);
            query->Query(KEYSPACE_MAPPING_CONSISTENCY, false, false);
            rv = true;
            while (query->NextRow() == ar_dataready) {
                int32_t sat = query->FieldGetInt32Value(0);
                string name = query->FieldGetStrValue(1);
                ECassSchemaType schema_type = static_cast<ECassSchemaType>(query->FieldGetInt32Value(2));

                if (schema_type <= eUnknownSchema || schema_type > eMaxSchema) {
                    // ignoring
                }
                else if (schema_type == resolver_schema) {
                    if (resolver_keyspace.empty()) {
                        resolver_keyspace = name;
                    }
                    else {
                        // More than one resolver keyspace
                        err_msg = "More than one resolver keyspace in the " +
                            mapping_keyspace + ".sat2keyspace table";
                        rv = false;
                        break;
                    }
                }
                else if (sat >= 0) {
                    while (static_cast<int32_t>(mapping.size()) <= sat)
                        mapping.push_back(make_tuple("", eUnknownSchema));
                    mapping[sat] = make_tuple(name, schema_type);
                }
            }
        }
        catch (const CCassandraException& e) {
            if ((e.GetErrCode() == CCassandraException::eQueryTimeout || e.GetErrCode() == CCassandraException::eQueryFailedRestartable) && i > 0) {
                continue;
            }
            throw;

        }
        break;
    }

    if (rv && mapping.empty()) {
        err_msg = "sat2keyspace is incomplete";
        rv = false;
    }
    if (rv && resolver_keyspace.empty() && resolver_schema != eUnknownSchema) {
        err_msg = "resolver schema is not found in sat2keyspace";
        rv = false;
    }

    return rv;
}


bool FetchSatToKeyspaceMapping(const string &  mapping_keyspace,
                               shared_ptr<CCassConnection>  conn,
                               vector<string> &  mapping,
                               ECassSchemaType  mapping_schema,
                               string &  resolver_keyspace,
                               ECassSchemaType  resolver_schema,
                               vector<pair<string, int32_t>> &  bioseq_na_keyspaces,
                               ECassSchemaType  bioseq_na_schema,
                               string &  err_msg)
{
    vector<tuple<string, ECassSchemaType>> lmapping;
    if (FetchSatToKeyspaceMapping(mapping_keyspace, conn, lmapping, resolver_keyspace, resolver_schema, err_msg)) {
        for (size_t sat_id = 0; sat_id < lmapping.size(); ++sat_id) {
            ECassSchemaType  schema = get<1>(lmapping[sat_id]);
            mapping.push_back((schema == mapping_schema ||
                               schema == bioseq_na_schema)? get<0>(lmapping[sat_id]) : "");

            if (schema == bioseq_na_schema)
                bioseq_na_keyspaces.push_back(
                        pair<string, int32_t>(get<0>(lmapping[sat_id]), sat_id));
        }
        return true;
    }
    return false;
}

bool FetchMessages(const string &  mapping_keyspace,
                   shared_ptr<CCassConnection>  conn,
                   CPSGMessages &  messages,
                   string &  err_msg)
{
    if (mapping_keyspace.empty()) {
        err_msg = "mapping_keyspace is not specified";
        return false;
    }

    bool rv = false;
    err_msg = mapping_keyspace + ".messages info is empty";
    for (int i = KEYSPACE_MAPPING_RETRY; i >= 0; --i) {
        try {
            shared_ptr<CCassQuery>  query = conn->NewQuery();
            query->SetSQL("SELECT name, value FROM " + mapping_keyspace + ".messages", 0);
            query->Query(KEYSPACE_MAPPING_CONSISTENCY, false, false);
            while (query->NextRow() == ar_dataready) {
                messages.Set(
                    query->FieldGetStrValue(0),
                    query->FieldGetStrValueDef(1, "")
                );
                err_msg.clear();
            }
            rv = true;
            break;
        }
        catch (const CCassandraException& e) {
            if (
                (e.GetErrCode() == CCassandraException::eQueryTimeout || e.GetErrCode() == CCassandraException::eQueryFailedRestartable)
                && i > 0
            ) {
                continue;
            }
            throw;
        }
    }

    return rv;
}

//------------------------------------------------------------------------------

BEGIN_SCOPE()

bool CanRetry(CCassandraException const& e, int retries)
{
    return
        (
            e.GetErrCode() == CCassandraException::eQueryTimeout
            || e.GetErrCode() == CCassandraException::eQueryFailedRestartable
        )
        && retries > 0;
}

vector<SSatInfoEntry>
ReadCassandraSatInfo(string const& keyspace, string const& domain, shared_ptr<CCassConnection> connection)
{
    vector<SSatInfoEntry> result;
    for (int i = KEYSPACE_MAPPING_RETRY; i >= 0; --i) {
        try {
            auto query = connection->NewQuery();
            query->SetSQL(
                "SELECT sat, keyspace_name, schema_type, service FROM "
                + keyspace + ".sat2keyspace WHERE domain = ?", 1);
            query->BindStr(0, domain);
            query->Query(KEYSPACE_MAPPING_CONSISTENCY, false, false);
            while (query->NextRow() == ar_dataready) {
                SSatInfoEntry row;
                row.sat = query->FieldGetInt32Value(0);
                row.keyspace = query->FieldGetStrValue(1);
                row.schema_type = static_cast<ECassSchemaType>(query->FieldGetInt32Value(2));
                row.service = query->FieldGetStrValueDef(3, "");
                if (row.schema_type <= eUnknownSchema || row.schema_type > eMaxSchema) {
                    // ignoring
                }
                else {
                    result.push_back(row);
                }
            }
        }
        catch (CCassandraException const& e) {
            if (!CanRetry(e, i)) {
                throw;
            }
        }
        break;
    }

    sort(begin(result), end(result),
        [](SSatInfoEntry const& a, SSatInfoEntry const& b)
        {
            return a.sat < b.sat;
        }
    );

    return result;
}

shared_ptr<CPSGMessages>
ReadCassandraMessages(string const& keyspace, string const& domain, shared_ptr<CCassConnection> connection)
{
    auto result = make_shared<CPSGMessages>();
    for (int i = KEYSPACE_MAPPING_RETRY; i >= 0; --i) {
        try {
            auto query = connection->NewQuery();
            query->SetSQL("SELECT name, value FROM " + keyspace + ".messages WHERE domain = ?", 1);
            query->BindStr(0, domain);
            query->Query(KEYSPACE_MAPPING_CONSISTENCY, false, false);
            while (query->NextRow() == ar_dataready) {
                result->Set(
                    query->FieldGetStrValue(0),
                    query->FieldGetStrValueDef(1, "")
                );
            }
            break;
        }
        catch (CCassandraException const& e) {
            if (!CanRetry(e, i)) {
                throw;
            }
        }
    }
    return result;
}

string GetAddressString(string const& host, bool is_host)
{
    if (is_host && !CSocketAPI::isip(host, false)) {
        auto addr = CSocketAPI::gethostbyname(host);
        if (addr == 0) {
            return "";
        }
        return CSocketAPI::HostPortToString(addr, 0);
    }
    return host;
}

template <class T>
inline void hash_combine(size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

size_t HashSatInfoData(vector<SSatInfoEntry> const& rows)
{
    size_t result{0};
    for (auto const& row : rows) {
        hash_combine(result, row.sat);
        hash_combine(result, row.keyspace);
        hash_combine(result, row.schema_type);
        hash_combine(result, row.service);
    }
    return result;
}

END_SCOPE()

CSatInfoSchema::CSatInfoSchema(
    CNcbiRegistry const & registry,
    string const & registry_section
)
    : m_Registry(registry)
    , m_RegistrySection(registry_section)
{
}

optional<SSatInfoEntry> CSatInfoSchema::GetBlobKeyspace(int32_t sat) const
{
    auto itr = m_BlobKeyspaces.find(sat);
    if (
        itr != cend(m_BlobKeyspaces)
        && (
            itr->second.schema_type == eBlobVer2Schema
            || itr->second.schema_type == eNamedAnnotationsSchema
        )
    ) {
        return itr->second;
    }
    return {};
}

int32_t CSatInfoSchema::GetMaxBlobKeyspaceSat() const
{
    auto itr = crbegin(m_BlobKeyspaces);
    return itr == crend(m_BlobKeyspaces) ? -1 : itr->first;
}

vector<SSatInfoEntry> CSatInfoSchema::GetNAKeyspaces() const
{
    return m_BioseqNaKeyspaces;
}

SSatInfoEntry CSatInfoSchema::GetResolverKeyspace() const
{
    return m_ResolverKeyspace;
}

shared_ptr<CCassConnection> CSatInfoSchema::x_GetConnectionByService(string const& service) const
{
    auto itr = m_Service2Cluster.find(service);
    return itr == cend(m_Service2Cluster) ? nullptr : itr->second;
}

shared_ptr<CCassConnection> CSatInfoSchema::x_GetConnectionByConnectionPoint(string const& connection_point) const
{
    auto itr = m_Point2Cluster.find(connection_point);
    return itr == cend(m_Point2Cluster) ? nullptr : itr->second;
}

optional<ESatInfoRefreshSchemaResult> CSatInfoSchema::x_AddClusterConnection(shared_ptr<CCassConnection> const& connection, bool is_default)
{
    auto port = ":" + to_string(connection->GetPort());
    auto peer_list = connection->GetLocalPeersAddressList("");
    for (auto & peer : peer_list) {
        auto point = peer + port;
        m_Point2Cluster[point] = connection;
    }
    if (is_default) {
        m_DefaultCluster = connection;
    }
    return {};
}

optional<ESatInfoRefreshSchemaResult> CSatInfoSchema::x_ResolveServiceName(string const& service, vector<string>& connection_points)
{
    connection_points.clear();
    {
        class CInPlaceConnIniter : protected CConnIniter
        {} conn_initer;  /*NCBI_FAKE_WARNING*/
    }

    bool is_hostlist = (service.find(':') != string::npos)
        || (service.find(' ') != string::npos)
        || (service.find(',') != string::npos);

    string hosts;
    if (!is_hostlist) {
        ERR_POST(Info << "CSatInfoSchema::x_AddClusterByServiceName uses service name: '" << service << "'");
        hosts = LbsmLookup::s_Resolve(service, ',');
        if (hosts.empty()) {
            ERR_POST(Info << "CSatInfoSchema::x_AddClusterByServiceName failed to resolve LBSM service name: '" << service << "'");
            return ESatInfoRefreshSchemaResult::eLbsmServiceNotResolved;
        }
        ERR_POST(Info << "CSatInfoSchema::x_AddClusterByServiceName resolved service name: '" << service << "' => '" << hosts << "'");
    }
    else {
        ERR_POST(Info << "CSatInfoSchema::x_AddClusterByServiceName uses host list: '" << service << "'");
        hosts = service;
    }

    vector<string> items;
    NStr::Split(hosts, ", ", items, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    for (auto item : items) {
        string item_host;
        string item_port_token;
        if (NStr::SplitInTwo(item, ":", item_host, item_port_token)) {
            int16_t item_port = NStr::StringToNumeric<short>(item_port_token, NStr::fConvErr_NoThrow);
            item_port = item_port ? item_port : CCassConnection::kCassDefaultPort;
            item_host = GetAddressString(item_host, is_hostlist);
            if (item_host.empty()) {
                return ESatInfoRefreshSchemaResult::eLbsmServiceNotResolved;
            }
            connection_points.push_back(item_host + ":" + to_string(item_port));
        }
        else {
            item = GetAddressString(item, is_hostlist);
            if (item.empty()) {
                return ESatInfoRefreshSchemaResult::eLbsmServiceNotResolved;
            }
            connection_points.push_back(item + ":" + to_string(CCassConnection::kCassDefaultPort));
        }
    }
    return {};
}

optional<ESatInfoRefreshSchemaResult> CSatInfoSchema::x_AddClusterByServiceName(
    string const& service,
    shared_ptr<CSatInfoSchema> const& old_schema,
    shared_ptr<CCassConnection>& cluster
)
{
    // Check this schema data
    if (service.empty()) {
        cluster = m_DefaultCluster;
        return {};
    }
    cluster = x_GetConnectionByService(service);
    if (cluster) {
        return {};
    }
    vector<string> connection_points;
    auto result = x_ResolveServiceName(service, connection_points);
    if (result.has_value()) {
        return result;
    }
    for (auto const& connection_point : connection_points) {
        cluster = x_GetConnectionByConnectionPoint(connection_point);
        if (cluster) {
            m_Service2Cluster.emplace(service, cluster);
            return {};
        }
    }

    // Check previous schema version
    if (old_schema) {
        cluster = old_schema->x_GetConnectionByService(service);
        if (cluster) {
            m_Service2Cluster[service] = cluster;
            x_AddClusterConnection(cluster, false);
            return {};
        }
        for (auto const& connection_point : connection_points) {
            cluster = old_schema->x_GetConnectionByConnectionPoint(connection_point);
            if (cluster) {
                m_Service2Cluster[service] = cluster;
                x_AddClusterConnection(cluster, false);
                return {};
            }
        }
    }

    // Make NEW connection
    {
        auto factory = CCassConnectionFactory::s_Create();
        factory->LoadConfig(m_Registry, m_RegistrySection);
        factory->SetServiceName(service);
        factory->SetDataNamespace("");
        cluster = factory->CreateInstance();
        cluster->Connect();
        m_Service2Cluster[service] = cluster;
        x_AddClusterConnection(cluster, false);
        return {};
    }

    return {};
}

optional<ESatInfoRefreshSchemaResult> CSatInfoSchema::x_AddSatInfoEntry(
    SSatInfoEntry&& entry,
    shared_ptr<CSatInfoSchema> const& old_schema
)
{
    shared_ptr<CCassConnection> connection;
    auto result = x_AddClusterByServiceName(entry.service, old_schema, connection);
    if (result.has_value()) {
        return result;
    }
    switch(entry.schema_type) {
        case eResolverSchema: {
            if (!m_ResolverKeyspace.keyspace.empty()) {
                return ESatInfoRefreshSchemaResult::eResolverKeyspaceDuplicated;
            }
            m_ResolverKeyspace = entry;
            m_ResolverKeyspace.connection = move(connection);
            break;
        }
        case eNamedAnnotationsSchema: {
            entry.connection = move(connection);
            m_BlobKeyspaces.emplace(entry.sat, entry);
            m_BioseqNaKeyspaces.push_back(move(entry));
            break;
        }
        case eBlobVer1Schema:
        case eBlobVer2Schema:
        {
            entry.connection = move(connection);
            m_BlobKeyspaces.emplace(entry.sat, move(entry));
            break;
        }
        case eUnknownSchema: // LCOV_EXCL_LINE
            break; // LCOV_EXCL_LINE
    }
    return {};
}

CSatInfoSchemaProvider::CSatInfoSchemaProvider(
    string const& sat_info_keyspace,
    string const & domain,
    shared_ptr<CCassConnection> sat_info_connection,
    const CNcbiRegistry & registry,
    const string & registry_section
)
    : m_SatInfoKeyspace(sat_info_keyspace)
    , m_Domain(domain)
    , m_SatInfoConnection(move(sat_info_connection))
    , m_Registry(registry)
    , m_RegistrySection(registry_section)
{}

void CSatInfoSchemaProvider::SetSatInfoConnection(shared_ptr<CCassConnection> sat_info_connection)
{
    atomic_store(&m_SatInfoConnection, move(sat_info_connection));
}

shared_ptr<CCassConnection> CSatInfoSchemaProvider::x_GetSatInfoConnection() const
{
    return atomic_load(&m_SatInfoConnection);
}

optional<SSatInfoEntry> CSatInfoSchemaProvider::GetBlobKeyspace(int32_t sat) const
{
    auto p = GetSchema();
    return p ? p->GetBlobKeyspace(sat) : nullopt;
}

vector<SSatInfoEntry> CSatInfoSchemaProvider::GetNAKeyspaces() const
{
    auto p = GetSchema();
    return p ? p->GetNAKeyspaces() : vector<SSatInfoEntry>();
}

SSatInfoEntry CSatInfoSchemaProvider::GetResolverKeyspace() const
{
    auto p = GetSchema();
    return p ? p->GetResolverKeyspace() : SSatInfoEntry();
}

int32_t CSatInfoSchemaProvider::GetMaxBlobKeyspaceSat() const
{
    auto p = GetSchema();
    return p ? p->GetMaxBlobKeyspaceSat() : -1;
}

string CSatInfoSchemaProvider::GetMessage(string const& name) const
{
    auto p = GetMessages();
    return p ? p->Get(name) : "";
}

shared_ptr<CSatInfoSchema> CSatInfoSchemaProvider::GetSchema() const
{
    return atomic_load(&m_SatInfoSchema);
}

shared_ptr<CPSGMessages> CSatInfoSchemaProvider::GetMessages() const
{
    return atomic_load(&m_SatInfoMessages);
}

ESatInfoRefreshSchemaResult CSatInfoSchemaProvider::RefreshSchema(bool apply)
{
    if (m_SatInfoKeyspace.empty()) {
        x_SetRefreshErrorMessage("mapping_keyspace is not specified");
        return ESatInfoRefreshSchemaResult::eSatInfoKeyspaceUndefined;
    }
    auto rows = ReadCassandraSatInfo(m_SatInfoKeyspace, m_Domain, x_GetSatInfoConnection());
    if (rows.empty()) {
        x_SetRefreshErrorMessage(m_SatInfoKeyspace + ".sat2keyspace info is empty");
        return ESatInfoRefreshSchemaResult::eSatInfoSat2KeyspaceEmpty;
    }
    auto rows_hash = HashSatInfoData(rows);
    if (rows_hash == m_SatInfoHash) {
        return ESatInfoRefreshSchemaResult::eSatInfoUnchanged;
    }
    else if (!apply) {
        return ESatInfoRefreshSchemaResult::eSatInfoUpdated;
    }
    auto schema = make_shared<CSatInfoSchema>(m_Registry, m_RegistrySection);
    auto old_schema = GetSchema();
    auto result = x_PopulateNewSchema(schema, move(rows), old_schema);
    if (result.has_value()) {
        return result.value();
    }
    atomic_store(&m_SatInfoSchema, move(schema));
    m_SatInfoHash = rows_hash;
    return ESatInfoRefreshSchemaResult::eSatInfoUpdated;
}

optional<ESatInfoRefreshSchemaResult> CSatInfoSchemaProvider::x_PopulateNewSchema(
    shared_ptr<CSatInfoSchema>& schema,
    vector<SSatInfoEntry>&& sat_info,
    shared_ptr<CSatInfoSchema> const& old_schema
)
{
    auto result = schema->x_AddClusterConnection(x_GetSatInfoConnection(), true);
    if (result.has_value()) {
        return result.value();
    }
    for (auto& entry : sat_info) {
        auto result = schema->x_AddSatInfoEntry(move(entry), old_schema);
        if (result.has_value()) {
            switch(result.value()) {
            case ESatInfoRefreshSchemaResult::eResolverKeyspaceDuplicated:
                x_SetRefreshErrorMessage("More than one resolver keyspace in the " +
                    m_SatInfoKeyspace + ".sat2keyspace table");
            break;
            case ESatInfoRefreshSchemaResult::eLbsmServiceNotResolved:
                x_SetRefreshErrorMessage("Cannot resolve service name: '" + entry.service + "'");
            break;
            default:
                x_SetRefreshErrorMessage("Unexpected result for SatInfoEntry processing: "
                    + to_string(static_cast<int64_t>(result.value())));
            }
            return result.value();
        }
    }
    if (schema->m_ResolverKeyspace.keyspace.empty() || !schema->m_ResolverKeyspace.connection) {
        x_SetRefreshErrorMessage("mapping_keyspace is not specified");
        return ESatInfoRefreshSchemaResult::eResolverKeyspaceUndefined;
    }
    if (schema->GetMaxBlobKeyspaceSat() == -1) {
        x_SetRefreshErrorMessage("sat2keyspace is incomplete");
        return ESatInfoRefreshSchemaResult::eBlobKeyspacesEmpty;
    }
    return {};
}

ESatInfoRefreshMessagesResult CSatInfoSchemaProvider::RefreshMessages(bool apply)
{
    if (m_SatInfoKeyspace.empty()) {
        x_SetRefreshErrorMessage("mapping_keyspace is not specified");
        return ESatInfoRefreshMessagesResult::eSatInfoKeyspaceUndefined;
    }
    auto messages = ReadCassandraMessages(m_SatInfoKeyspace, m_Domain, x_GetSatInfoConnection());
    if (messages->IsEmpty()) {
        x_SetRefreshErrorMessage(m_SatInfoKeyspace + ".messages info is empty");
        return ESatInfoRefreshMessagesResult::eSatInfoMessagesEmpty;
    }

    auto old_messages = GetMessages();
    if (old_messages && *old_messages == *messages) {
        return ESatInfoRefreshMessagesResult::eMessagesUnchanged;
    }
    else if (!apply) {
        return ESatInfoRefreshMessagesResult::eMessagesUpdated;
    }
    atomic_store(&m_SatInfoMessages, move(messages));
    return ESatInfoRefreshMessagesResult::eMessagesUpdated;
}

string CSatInfoSchemaProvider::GetRefreshErrorMessage() const
{
    auto p = atomic_load(&m_RefreshErrorMessage);
    return p ? *p : "";
}

void CSatInfoSchemaProvider::x_SetRefreshErrorMessage(string const& message)
{
    auto msg = make_shared<string>(message);
    atomic_store(&m_RefreshErrorMessage, move(msg));
}


END_IDBLOB_SCOPE
