#ifndef BLOB_STORAGE__HPP
#define BLOB_STORAGE__HPP

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
 * Authors: Sergey Satskiy, Dmitrii Saprykin
 *
 * File Description:
 *
 * The functionality not directly related to blob operations
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbireg.hpp>

#include <chrono>
#include <optional>

#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/messages.hpp>
#include "IdCassScope.hpp"

BEGIN_IDBLOB_SCOPE

USING_NCBI_SCOPE;

// Note: must be in sync with the Cassandra SAT_INFO.SAT2KEYSPACE schema type values
enum ECassSchemaType {
    eUnknownSchema = 0,
    eResolverSchema = 1,
    eBlobVer1Schema = 2,
    eBlobVer2Schema = 3,
    eNamedAnnotationsSchema = 4,
    eIPGSchema = 5,
    eMaxSchema = eIPGSchema
};

enum class ECassSatInfoFlags : Int4
{
    // Requires WebCookie verification befor blob access
    eSecureSat = 1,
};

struct SBlobStorageConstants
{
    SBlobStorageConstants() = delete;
    static const char* const kChunkTableDefault;
    static const char* const kChunkTableBig;
};

class CSatInfoSchema;
struct SSatInfoEntry final
{
    friend CSatInfoSchema;
 public:
    static constexpr int32_t kInvalidSat{-100};

    SSatInfoEntry() = default;
    SSatInfoEntry(SSatInfoEntry &&) = default;
    SSatInfoEntry(SSatInfoEntry const&) = default;
    SSatInfoEntry& operator=(SSatInfoEntry &&) = default;
    SSatInfoEntry& operator=(SSatInfoEntry const&) = default;

    // Satellite id
    int32_t sat{kInvalidSat};

    // Satellite schema type
    ECassSchemaType schema_type{eUnknownSchema};

    // Keyspace name to read data
    string keyspace;

    // Service name to connect to satellite
    string service;

    // Connection to access secure satellite
    shared_ptr<CCassConnection> connection;

    // Bitmask of satellite properties
    // @see ECassSatInfoFlags
    int64_t flags{0};

    /// Is satellite requires secure access. If True connection should be acquired through
    /// GetSecureConnection() method.
    ///
    /// @return
    ///   True if satellite requires secure access
    ///   False - otherwise (data is public)
    bool IsSecureSat() const
    {
        return flags & static_cast<Int4>(ECassSatInfoFlags::eSecureSat);
    }

    /// Get string representation for debug
    ///
    /// @return
    ///   String representation for debug
    string ToString(string const& prefix) const;

    /// Get secure satellite connection
    ///
    /// @param username
    ///   User name
    ///
    /// @throws When IsSecureSat() == false
    ///
    /// @return
    ///   Connection if username is in allowed users list
    ///   nullptr - otherwise
    shared_ptr<CCassConnection> GetSecureConnection(string const& username) const;

    /// Get public satellite connection
    ///
    /// @throws When IsSecureSat() == true
    ///
    /// @return
    ///   Connection for public satellites
    shared_ptr<CCassConnection> GetConnection() const;

 private:
    // Connection to access secure satellite
    shared_ptr<CCassConnection> m_SecureConnection;

    // List of usernames for eSecureSat satellite.
    set<string> m_SecureUsers;
};

enum class ESatInfoRefreshSchemaResult : char
{
    eUndefined = 0,
    eSatInfoUpdated = 1,
    eSatInfoUnchanged = 2,
    eSatInfoKeyspaceUndefined = 3,
    eSatInfoSat2KeyspaceEmpty = 4,
    eResolverKeyspaceDuplicated = 6,
    eResolverKeyspaceUndefined = 7,
    eBlobKeyspacesEmpty = 8,
    eLbsmServiceNotResolved = 9,
    eUnsupportedSecureKeyspace = 10,
    eServiceFieldParseError = 11,
};

enum class ESatInfoRefreshMessagesResult : char
{
    eUndefined = 0,
    eMessagesUpdated = 1,
    eMessagesUnchanged = 2,
    eSatInfoKeyspaceUndefined = 3,
    eSatInfoMessagesEmpty = 4,
};

class CSatInfoSchemaProvider;

class CSatInfoSchema final
{
 public:
    friend class CSatInfoSchemaProvider;

    CSatInfoSchema() = default;

    /// Get blob keyspace connection by sat id
    ///
    /// @param sat
    ///   Blob sat id
    ///
    /// @return
    ///   Configuration entry or empty optional
    ///   if sat does not exists or has non blob schema type
    optional<SSatInfoEntry> GetBlobKeyspace(int32_t sat) const;

    /// Get list of BioseqNA keyspaces connections
    vector<SSatInfoEntry> GetNAKeyspaces() const;

    /// Get connection to resolver keyspace
    SSatInfoEntry GetResolverKeyspace() const;

    /// Get connection to IPG keyspace
    optional<SSatInfoEntry> GetIPGKeyspace() const;

    /// Get max id value for existing blob sat
    int32_t GetMaxBlobKeyspaceSat() const;

    /// Print internal state of CSatInfoSchema
    ///
    /// @for_tests, @for_debug
    string ToString() const;

 private:
    shared_ptr<CCassConnection> x_GetConnectionByService(string const& service, string const& registry_section) const;
    shared_ptr<CCassConnection> x_GetConnectionByConnectionPoint(string const& connection_point) const;

    optional<ESatInfoRefreshSchemaResult> x_AddConnection(
        shared_ptr<CCassConnection> const& connection,
        string const& registry_section,
        string const& service,
        bool is_default
    );
    optional<ESatInfoRefreshSchemaResult> x_AddSatInfoEntry(
        SSatInfoEntry entry,
        shared_ptr<CSatInfoSchema> const& old_schema,
        shared_ptr<IRegistry const> const& registry,
        string const& registry_section,
        set<string> const& secure_users
    );
    optional<ESatInfoRefreshSchemaResult> x_ResolveConnectionByServiceName(
        string service,
        shared_ptr<CSatInfoSchema> const& old_schema,
        shared_ptr<IRegistry const> const& registry,
        string const& registry_section,
        shared_ptr<CCassConnection>& connection
    );
    optional<ESatInfoRefreshSchemaResult> x_ResolveServiceName(
        string const& service, string const& registry_section, vector<string>& connection_points);

    // Mapping {sat => Connection to blob keyspace}
    map<int32_t, SSatInfoEntry> m_BlobKeyspaces;

    // Mapping {sat => set<string>}. List of usernames for eSecureSat satellites.
    map<int32_t, set<string>> m_SecureSatUsers;

    // List of BioseqNA connections
    vector<SSatInfoEntry> m_BioseqNaKeyspaces;

    // Connection to resolver keyspace
    SSatInfoEntry m_ResolverKeyspace;

    // Connection to IPG keyspace
    optional<SSatInfoEntry> m_IPGKeyspace;

    // Host:port => ClusterConnection
    map<string, shared_ptr<CCassConnection>> m_Point2Cluster;

    // ServiceName from service column => ClusterConnection
    map<string, shared_ptr<CCassConnection>> m_Service2Cluster;

    // Default cluster connection
    shared_ptr<CCassConnection> m_DefaultConnection;

    // Default cluster registry section name
    string m_DefaultRegistrySection;

    optional<chrono::milliseconds> m_ResolveTimeout{nullopt};
};

class CSatInfoSchemaProvider final
{
 public:

    /// @param sat_info_keyspace
    ///   Name of Cassandra keyspace to use as a source of configuration
    /// @param domain
    ///   Name of configuration domain for that provider
    /// @param sat_info_connection
    ///   Connection to Cassandra cluster hosting {sat_info_keyspace}
    /// @param registry
    ///   Configuration registry used to create new cluster connections
    /// @param registry_section
    ///   Registry section name used as a template creating new cluster connection
    ///   {service} entry of registry section gets replaced by {service} from {sat_info3.sat2keyspace} table
    CSatInfoSchemaProvider(
        string const& sat_info_keyspace,
        string const& domain,
        shared_ptr<CCassConnection> sat_info_connection,
        shared_ptr<IRegistry const> registry,
        string const& registry_section
    );

    /// Overrides data retrieval timeout
    ///
    /// @param timeout
    ///   Timeout in milliseconds
    void SetTimeout(chrono::milliseconds timeout)
    {
        m_Timeout = timeout;
    }

    /// Changes configuration domain for existing provider
    ///
    /// @param registry_section
    ///   Registry section name used as a template creating new cluster connection for secure satellite access
    ///   and secure satellite user list storage
    ///   {service} entry of registry section gets replaced by {service} from {sat_info3.sat2keyspace} table
    ///
    /// @not_thread_safe
    void SetSecureSatRegistrySection(string const& registry_section)
    {
        m_SecureSatRegistrySection = registry_section;
    }

    /// Changes configuration domain for existing provider
    ///
    /// @param domain
    ///   Name of configuration domain for that provider
    ///
    /// @for_tests, @not_thread_safe
    void SetDomain(string const& domain)
    {
        m_Domain = domain;
    }

    /// Should existence of {resolver} keyspace be checked?
    ///
    /// @param value
    ///   true - Resolver keyspace is required to be present in configuration domain
    ///   false - Resolver keyspace is optional (e.g. for blob daemon or ASN.1 dumpers)
    ///   {default} - true
    ///
    /// @not_thread_safe
    void SetResolverKeyspaceRequired(bool value)
    {
        m_ResolverKeyspaceRequired = value;
    }

    /// Changes Cassandra connection used to communicate with sat_info3
    ///
    /// @used in genbank-blobdaemon project
    ///
    /// @thread_safe
    void SetSatInfoConnection(shared_ptr<CCassConnection> sat_info_connection);

    /// Get blob keyspace connection by sat id
    ///
    /// @param sat
    ///   Blob sat id
    ///
    /// @return
    ///   Configuration entry or empty optional
    ///   if sat does not exists or has non blob schema type
    ///
    /// @thread_safe
    optional<SSatInfoEntry> GetBlobKeyspace(int32_t sat) const;

    /// Get list of BioseqNA keyspaces connections
    ///
    /// @thread_safe
    vector<SSatInfoEntry> GetNAKeyspaces() const;

    /// Get connection to resolver keyspace
    ///
    /// @thread_safe
    SSatInfoEntry GetResolverKeyspace() const;

    /// Get connection to IPG keyspace
    ///
    /// @thread_safe
    optional<SSatInfoEntry> GetIPGKeyspace() const;

    /// Get max id value for existing blob sat
    ///
    /// @thread_safe
    int32_t GetMaxBlobKeyspaceSat() const;

    /// Get configured message by name
    ///
    /// @param name
    ///   Message name
    ///
    /// @thread_safe
    string GetMessage(string const& name) const;

    /// Refresh information for configuration database {sat_info3.sat2keyspace}
    ///
    /// @param appy
    ///   - true to apply changes
    ///   - false to check if configuration database content changed
    ///
    /// @return
    ///   Operation result
    ///
    /// @non_thread_safe - Should be called from a single control thread
    ESatInfoRefreshSchemaResult RefreshSchema(bool apply);

    /// Refresh information for messages database {sat_info3.messages}
    ///
    /// @param appy
    ///   - true to apply changes
    ///   - false to check if configuration database content changed
    ///
    /// @return
    ///   Operation result
    ///
    /// @non_thread_safe - Should be called from a single control thread
    ESatInfoRefreshMessagesResult RefreshMessages(bool apply);

    /// Get configuration schema snapshot
    ///
    /// @thread_safe
    shared_ptr<CSatInfoSchema> GetSchema() const;

    /// Get messages snapshot
    ///
    /// @thread_safe
    shared_ptr<CPSGMessages> GetMessages() const;

    /// Get detailed message for last refresh operation (common for RefreshSchema and RefreshMessages).
    /// Empty string in case there were no errors
    ///
    /// @thread_safe
    string GetRefreshErrorMessage() const;

 private:
    shared_ptr<CCassConnection> x_GetSatInfoConnection() const;
    void x_SetRefreshErrorMessage(string const& message);
    optional<ESatInfoRefreshSchemaResult> x_PopulateNewSchema(
        shared_ptr<CSatInfoSchema>& new_schema,
        shared_ptr<CSatInfoSchema> const& old_schema,
        vector<SSatInfoEntry>&& sat_info,
        map<int32_t, set<string>>&& secure_users
    );

    string m_SatInfoKeyspace;
    string m_Domain;
    shared_ptr<CCassConnection> m_SatInfoConnection;

    shared_ptr<IRegistry const> m_Registry;
    string m_RegistrySection;
    string m_SecureSatRegistrySection;

    shared_ptr<CSatInfoSchema> m_SatInfoSchema;
    shared_ptr<CPSGMessages> m_SatInfoMessages;
    size_t m_SatInfoHash{0};

    shared_ptr<string> m_RefreshErrorMessage;
    bool m_ResolverKeyspaceRequired{true};

    optional<chrono::milliseconds> m_Timeout{nullopt};
};

END_IDBLOB_SCOPE

#endif
