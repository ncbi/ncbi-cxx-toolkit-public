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

#include "sat_info_service_parser.hpp"

BEGIN_IDBLOB_SCOPE

const char* const SBlobStorageConstants::kChunkTableDefault = "blob_chunk";
const char* const SBlobStorageConstants::kChunkTableBig = "big_blob_chunk";

BEGIN_SCOPE()

constexpr TCassConsistency kSatInfoReadConsistency{CCassConsistency::kLocalQuorum};
constexpr int kSatInfoReadRetry{5};

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
ReadCassandraSatInfo(
    string const& keyspace, string const& domain,
    shared_ptr<CCassConnection> connection,
    optional<chrono::milliseconds> timeout
) {
    vector<SSatInfoEntry> result;
    for (int i = kSatInfoReadRetry; i >= 0; --i) {
        try {
            auto query = connection->NewQuery();
            if (timeout) {
                query->UsePerRequestTimeout(true);
                query->SetTimeout(timeout.value().count());
            }
            query->SetSQL(
                "SELECT sat, keyspace_name, schema_type, service, flags FROM "
                + keyspace + ".sat2keyspace WHERE domain = ?", 1);
            query->BindStr(0, domain);
            query->Query(kSatInfoReadConsistency, false, false);
            while (query->NextRow() == ar_dataready) {
                SSatInfoEntry row;
                row.sat = query->FieldGetInt32Value(0);
                row.keyspace = query->FieldGetStrValue(1);
                row.schema_type = static_cast<ECassSchemaType>(query->FieldGetInt32Value(2));
                row.service = query->FieldGetStrValueDef(3, "");
                row.flags = query->FieldGetInt64Value(4, 0);
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
ReadCassandraMessages(
    string const& keyspace, string const& domain,
    shared_ptr<CCassConnection> connection,
    optional<chrono::milliseconds> timeout
) {
    auto result = make_shared<CPSGMessages>();
    for (int i = kSatInfoReadRetry; i >= 0; --i) {
        try {
            result->Clear();
            auto query = connection->NewQuery();
            if (timeout) {
                query->UsePerRequestTimeout(true);
                query->SetTimeout(timeout.value().count());
            }
            query->SetSQL("SELECT name, value FROM " + keyspace + ".messages WHERE domain = ?", 1);
            query->BindStr(0, domain);
            query->Query(kSatInfoReadConsistency, false, false);
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

set<string> ReadSecureSatUsers(
    string const& keyspace, int32_t sat,
    shared_ptr<CCassConnection> connection,
    optional<chrono::milliseconds> timeout
) {
    set<string> result;
    for (int i = kSatInfoReadRetry; i >= 0; --i) {
        try {
            result.clear();
            auto query = connection->NewQuery();
            if (timeout) {
                query->UsePerRequestTimeout(true);
                query->SetTimeout(timeout.value().count());
            }
            query->SetSQL("SELECT username FROM " + keyspace + ".web_user WHERE sat = ?", 1);
            query->BindInt32(0, sat);
            query->Query(kSatInfoReadConsistency, false, false);
            while (query->NextRow() == ar_dataready) {
                auto username = query->FieldGetStrValueDef(0, "");
                if (!username.empty()) {
                    result.insert(username);
                }
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

size_t HashSatInfoData(
    vector<SSatInfoEntry> const& rows,
    string const& secure_registry_section,
    map<int32_t, set<string>> secure_users
)
{
    size_t result{0};
    hash_combine(result, secure_registry_section);
    for (auto const& row : rows) {
        hash_combine(result, row.sat);
        hash_combine(result, row.keyspace);
        hash_combine(result, row.schema_type);
        hash_combine(result, row.service);
        hash_combine(result, row.flags);
        if (row.IsSecureSat()) {
            for (auto user: secure_users[row.sat]) {
                hash_combine(result, user);
            }
        }
    }
    return result;
}


shared_ptr<CCassConnection>
MakeCassConnection(
    shared_ptr<IRegistry const> const& registry,
    string const& section,
    string const& service,
    bool reset_namespace
)
{
    auto factory = CCassConnectionFactory::s_Create();
    factory->LoadConfig(registry.get(), section);
    if (!service.empty()) {
        factory->SetServiceName(service);
    }
    if (reset_namespace) {
        factory->SetDataNamespace("");
    }
    auto connection = factory->CreateInstance();
    connection->Connect();
    return connection;
}

inline string GetServiceKey(string const& service, string const& registry_section)
{
    return registry_section + "|" + service;
}

inline string GetConnectionPointKey(string const& peer, int16_t port, string const& registry_section)
{
    return registry_section + "|" + peer + ":" + to_string(port);
}

string GetConnectionDatacenterName(shared_ptr<CCassConnection> connection)
{
    for (int i = kSatInfoReadRetry; i >= 0; --i) {
        try {
           return connection->GetDatacenterName();
        }
        catch (CCassandraException const& e) {
            if (!CanRetry(e, i)) {
                throw;
            }
        }
    }
    return {}; // unreachable
}

string ParseSatInfoServiceField(shared_ptr<CCassConnection>& connection, vector<SSatInfoEntry>& sat_info)
{
    auto datacenter = GetConnectionDatacenterName(connection);
    CSatInfoServiceParser parser;
    for (auto & entry: sat_info) {
        if (!entry.service.empty()) {
            auto lbsm_service = parser.GetConnectionString(entry.service, datacenter);
            if (lbsm_service.empty()) {
                return "Cannot parse service field value: '" + entry.service + "'";
            }
            entry.service = lbsm_service;
        }
    }
    return {};
}

END_SCOPE()

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

optional<SSatInfoEntry> CSatInfoSchema::GetIPGKeyspace() const
{
    return m_IPGKeyspace;
}

shared_ptr<CCassConnection> CSatInfoSchema::x_GetConnectionByService(string const& service, string const& registry_section) const
{
    auto itr = m_Service2Cluster.find(GetServiceKey(service, registry_section));
    return itr == cend(m_Service2Cluster) ? nullptr : itr->second;
}

shared_ptr<CCassConnection> CSatInfoSchema::x_GetConnectionByConnectionPoint(string const& connection_point) const
{
    auto itr = m_Point2Cluster.find(connection_point);
    return itr == cend(m_Point2Cluster) ? nullptr : itr->second;
}

optional<ESatInfoRefreshSchemaResult> CSatInfoSchema::x_AddConnection(
    shared_ptr<CCassConnection> const& connection,
    string const& registry_section,
    string const& service,
    bool is_default
)
{
    unsigned int timeout = m_ResolveTimeout ? m_ResolveTimeout.value().count() : 0;
    for (auto peer : connection->GetLocalPeersAddressList("", timeout)) {
        m_Point2Cluster[GetConnectionPointKey(peer, connection->GetPort(), registry_section)] = connection;
    }
    if (is_default) {
        m_DefaultConnection = connection;
        m_DefaultRegistrySection = registry_section;
    }
    else {
        m_Service2Cluster[GetServiceKey(service, registry_section)] = connection;
    }
    return {};
}

optional<ESatInfoRefreshSchemaResult> CSatInfoSchema::x_ResolveServiceName(
    string const& service, string const& registry_section, vector<string>& connection_points)
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
            connection_points.push_back(GetConnectionPointKey(item_host, item_port, registry_section));
        }
        else {
            item = GetAddressString(item, is_hostlist);
            if (item.empty()) {
                return ESatInfoRefreshSchemaResult::eLbsmServiceNotResolved;
            }
            connection_points.push_back(
                GetConnectionPointKey(item_host, CCassConnection::kCassDefaultPort, registry_section)
            );
        }
    }
    return {};
}

optional<ESatInfoRefreshSchemaResult> CSatInfoSchema::x_ResolveConnectionByServiceName(
    string service,
    shared_ptr<CSatInfoSchema> const& old_schema,
    shared_ptr<IRegistry const> const& registry,
    string const& registry_section,
    shared_ptr<CCassConnection>& connection
)
{
    // Check this schema data
    if (service.empty()) {
        if (registry_section == m_DefaultRegistrySection) {
            connection = m_DefaultConnection;
            return {};
        }
        else {
            service = registry->Get(registry_section, "service");
        }
    }
    connection = x_GetConnectionByService(service, registry_section);
    if (connection) {
        return {};
    }
    vector<string> connection_points;
    auto result = x_ResolveServiceName(service, registry_section, connection_points);
    if (result.has_value()) {
        return result;
    }
    for (auto const& connection_point : connection_points) {
        connection = x_GetConnectionByConnectionPoint(connection_point);
        if (connection) {
            m_Service2Cluster.emplace(GetServiceKey(service, registry_section), connection);
            return {};
        }
    }

    // Check previous schema version
    if (old_schema) {
        connection = old_schema->x_GetConnectionByService(service, registry_section);
        if (connection) {
            x_AddConnection(connection, registry_section, service, false);
            return {};
        }
        for (auto const& connection_point : connection_points) {
            connection = old_schema->x_GetConnectionByConnectionPoint(connection_point);
            if (connection) {
                x_AddConnection(connection, registry_section, service, false);
                return {};
            }
        }
    }

    // Make NEW connection
    {
        connection = MakeCassConnection(registry, registry_section, service, true);
        x_AddConnection(connection, registry_section, service, false);
    }

    return {};
}

optional<ESatInfoRefreshSchemaResult> CSatInfoSchema::x_AddSatInfoEntry(
    SSatInfoEntry entry,
    shared_ptr<CSatInfoSchema> const& old_schema,
    shared_ptr<IRegistry const> const& registry,
    string const& registry_section,
    set<string> const& secure_users
)
{
    shared_ptr<CCassConnection> connection;
    auto result = x_ResolveConnectionByServiceName(entry.service, old_schema, registry, registry_section, connection);
    if (result.has_value()) {
        return result;
    }
    if (!secure_users.empty()) {
        entry.m_SecureUsers = secure_users;
        m_SecureSatUsers[entry.sat] = secure_users;
    }
    if (entry.IsSecureSat()) {
        entry.connection = nullptr;
        entry.m_SecureConnection = std::move(connection);
    }
    else {
        entry.connection = std::move(connection);
        entry.m_SecureConnection = nullptr;
    }

    // Temporary restriction. Until PSG needs/supports those types of secure satellites
    if (entry.IsSecureSat() && entry.schema_type != eBlobVer2Schema)
    {
        return ESatInfoRefreshSchemaResult::eUnsupportedSecureKeyspace;
    }
    switch(entry.schema_type) {
        case eResolverSchema: {
            if (!m_ResolverKeyspace.keyspace.empty()) {
                return ESatInfoRefreshSchemaResult::eResolverKeyspaceDuplicated;
            }
            m_ResolverKeyspace = std::move(entry);
            break;
        }
        case eNamedAnnotationsSchema: {
            m_BlobKeyspaces.emplace(entry.sat, entry);
            m_BioseqNaKeyspaces.push_back(std::move(entry));
            break;
        }
        case eBlobVer1Schema:
        case eBlobVer2Schema:
        {
            m_BlobKeyspaces.emplace(entry.sat, std::move(entry));
            break;
        }
        case eIPGSchema:
        {
            m_IPGKeyspace = make_optional(std::move(entry));
            break;
        }
        case eUnknownSchema: // LCOV_EXCL_LINE
            break; // LCOV_EXCL_LINE
    }
    return {};
}

string SSatInfoEntry::ToString(string const& prefix) const
{
    stringstream s;
    for (auto user : m_SecureUsers) {
        s << "\n" << prefix << " - '" << user << "'";
    }

    string secure_users = s.str();
    secure_users = secure_users.empty() ? "{}" : secure_users;
    return prefix + "sat: " + to_string(sat)
        + ", schema: "+ to_string(static_cast<int>(schema_type))
        + ", keyspace: '" + keyspace + "'"
        + ", service: '" + service + "'"
        + ", flags: " + to_string(flags)
        + ", connection: &" + to_string(reinterpret_cast<intptr_t>(connection.get()))
        + ", secure_connection: &" + to_string(reinterpret_cast<intptr_t>(m_SecureConnection.get()))
        + ", secure_users: " + secure_users;
}

shared_ptr<CCassConnection> SSatInfoEntry::GetSecureConnection(string const& username) const
{
    if (!IsSecureSat()) {
        NCBI_THROW(CCassandraException, eGeneric, "SSatInfoEntry::GetSecureConnection() should not be called for NON secure satellites");
    }
    if (username.empty() || m_SecureUsers.empty()) {
        return nullptr;
    }
    auto user_entry = m_SecureUsers.find(username);
    bool user_allowed_to_read = user_entry != cend(m_SecureUsers);
    if (user_allowed_to_read) {
        return m_SecureConnection;
    }
    else {
        return nullptr;
    }
}

shared_ptr<CCassConnection> SSatInfoEntry::GetConnection() const
{
    if (IsSecureSat()) {
        NCBI_THROW(CCassandraException, eGeneric, "SSatInfoEntry::GetConnection() should not be called for secure satellites");
    }
    return this->connection;
}

string CSatInfoSchema::ToString() const
{
    stringstream s;
    s << "Blob keyspaces: \n";
    for (auto sat : m_BlobKeyspaces) {
        s << sat.second.ToString("  ") << "\n";
    }
    s << "BioseqNA keyspaces: \n";
    for (auto sat : m_BioseqNaKeyspaces) {
        s << sat.ToString("  ") << "\n";
    }
    s << "Resolver keyspace: \n";
    s << m_ResolverKeyspace.ToString("  ") << "\n";
    s << "IPG keyspace: \n";
    s << ((m_IPGKeyspace) ? m_IPGKeyspace->ToString("  ")  :  "  None") << "\n";
    s << "Secure sat users: \n";
    for (auto sat_users : m_SecureSatUsers) {
        s << "  " << sat_users.first << "\n";
        for (auto user : sat_users.second) {
            s << "  - '" << user << "'\n";
        }
    }
    s << "Default connection: &" << to_string(reinterpret_cast<intptr_t>(m_DefaultConnection.get())) << "\n";
    s << "Default registry section: '" << m_DefaultRegistrySection << "'\n";
    s << "m_Point2Cluster: \n";
    for (auto item : m_Point2Cluster) {
        s << "  " << item.first << " : &" << to_string(reinterpret_cast<intptr_t>(item.second.get())) << "\n";
    }
    s << "m_Service2Cluster: \n";
    for (auto item : m_Service2Cluster) {
        s << "  " << item.first << " : &" << to_string(reinterpret_cast<intptr_t>(item.second.get())) << "\n";
    }
    return s.str();
}

CSatInfoSchemaProvider::CSatInfoSchemaProvider(
    string const& sat_info_keyspace,
    string const& domain,
    shared_ptr<CCassConnection> sat_info_connection,
    shared_ptr<IRegistry const> registry,
    string const& registry_section
)
    : m_SatInfoKeyspace(sat_info_keyspace)
    , m_Domain(domain)
    , m_SatInfoConnection(std::move(sat_info_connection))
    , m_Registry(std::move(registry))
    , m_RegistrySection(registry_section)
{
    if (m_SatInfoConnection == nullptr) {
        NCBI_THROW(CCassandraException, eFatal, "CSatInfoSchemaProvider() Cassandra connection should not be nullptr");
    }
}

void CSatInfoSchemaProvider::SetSatInfoConnection(shared_ptr<CCassConnection> sat_info_connection)
{
    atomic_store(&m_SatInfoConnection, std::move(sat_info_connection));
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

optional<SSatInfoEntry> CSatInfoSchemaProvider::GetIPGKeyspace() const
{
    auto p = GetSchema();
    return p ? p->GetIPGKeyspace() : nullopt;
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
    auto sat_info_connection = x_GetSatInfoConnection();
    auto rows = ReadCassandraSatInfo(m_SatInfoKeyspace, m_Domain, sat_info_connection, m_Timeout);
    if (rows.empty()) {
        x_SetRefreshErrorMessage(m_SatInfoKeyspace + ".sat2keyspace info is empty for domain '" + m_Domain + "'");
        return ESatInfoRefreshSchemaResult::eSatInfoSat2KeyspaceEmpty;
    }
    auto service_result = ParseSatInfoServiceField(sat_info_connection, rows);
    if (!service_result.empty()) {
        x_SetRefreshErrorMessage(service_result);
        return ESatInfoRefreshSchemaResult::eServiceFieldParseError;
    }
    map<int32_t, set<string>> secure_users;
    if (!m_SecureSatRegistrySection.empty()) {
        auto secure_connection = MakeCassConnection(m_Registry, m_SecureSatRegistrySection, "", false);
        for (auto sat_info: rows) {
            if (sat_info.IsSecureSat()) {
                secure_users[sat_info.sat] = ReadSecureSatUsers(
                    secure_connection->Keyspace(), sat_info.sat, secure_connection, m_Timeout
                );
            }
        }
    }
    auto rows_hash = HashSatInfoData(rows, m_SecureSatRegistrySection, secure_users);
    if (rows_hash == m_SatInfoHash) {
        return ESatInfoRefreshSchemaResult::eSatInfoUnchanged;
    }
    else if (!apply) {
        return ESatInfoRefreshSchemaResult::eSatInfoUpdated;
    }
    auto schema = make_shared<CSatInfoSchema>();
    schema->m_ResolveTimeout = m_Timeout;
    auto old_schema = GetSchema();
    auto result = x_PopulateNewSchema(schema, old_schema, std::move(rows), std::move(secure_users));
    if (result.has_value()) {
        return result.value();
    }
    atomic_store(&m_SatInfoSchema, std::move(schema));
    m_SatInfoHash = rows_hash;
    return ESatInfoRefreshSchemaResult::eSatInfoUpdated;
}

optional<ESatInfoRefreshSchemaResult> CSatInfoSchemaProvider::x_PopulateNewSchema(
    shared_ptr<CSatInfoSchema>& new_schema,
    shared_ptr<CSatInfoSchema> const& old_schema,
    vector<SSatInfoEntry>&& sat_info,
    map<int32_t, set<string>>&& secure_users
)
{
    auto result = new_schema->x_AddConnection(x_GetSatInfoConnection(), m_RegistrySection, "", true);
    if (result.has_value()) {
        return result.value();
    }
    for (auto& entry : sat_info) {
        string registry_section = m_RegistrySection;
        if (entry.IsSecureSat() && !m_SecureSatRegistrySection.empty()) {
            registry_section = m_SecureSatRegistrySection;
        }
        auto sat_secure_users = secure_users[entry.sat];
        auto result = new_schema->x_AddSatInfoEntry(entry, old_schema, m_Registry, registry_section, sat_secure_users);
        if (result.has_value()) {
            switch(result.value()) {
            case ESatInfoRefreshSchemaResult::eResolverKeyspaceDuplicated:
                x_SetRefreshErrorMessage("More than one resolver keyspace in the " +
                    m_SatInfoKeyspace + ".sat2keyspace table");
            break;
            case ESatInfoRefreshSchemaResult::eLbsmServiceNotResolved:
                x_SetRefreshErrorMessage("Cannot resolve service name: '" + entry.service + "'");
            break;
            case ESatInfoRefreshSchemaResult::eUnsupportedSecureKeyspace:
                x_SetRefreshErrorMessage("Satellite with id (" + to_string(entry.sat) + ") cannot be configured as secure");
            break;
            default:
                x_SetRefreshErrorMessage("Unexpected result for SatInfoEntry processing: "
                    + to_string(static_cast<int64_t>(result.value())));
            }
            return result.value();
        }
    }
    if (
        m_ResolverKeyspaceRequired &&
        (new_schema->m_ResolverKeyspace.keyspace.empty() || !new_schema->m_ResolverKeyspace.connection)
    ) {
        x_SetRefreshErrorMessage("resolver schema is not found in sat2keyspace");
        return ESatInfoRefreshSchemaResult::eResolverKeyspaceUndefined;
    }
    if (new_schema->GetMaxBlobKeyspaceSat() == -1) {
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
    auto messages = ReadCassandraMessages(m_SatInfoKeyspace, m_Domain, x_GetSatInfoConnection(), m_Timeout);
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
    atomic_store(&m_SatInfoMessages, std::move(messages));
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
    atomic_store(&m_RefreshErrorMessage, std::move(msg));
}


END_IDBLOB_SCOPE
