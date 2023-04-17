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

#include <optional>

#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/messages.hpp>
#include "IdCassScope.hpp"

BEGIN_IDBLOB_SCOPE

USING_NCBI_SCOPE;

// Note: must be in sync with the Cassandra SAT_INFO.SAT2KEYSPACE schema type
//       values
enum ECassSchemaType {
    eUnknownSchema = 0,
    eResolverSchema = 1,
    eBlobVer1Schema = 2,
    eBlobVer2Schema = 3,
    eNamedAnnotationsSchema = 4,
    eMaxSchema = eNamedAnnotationsSchema
};

struct SBlobStorageConstants
{
    SBlobStorageConstants() = delete;
    static const char* const kChunkTableDefault;
    static const char* const kChunkTableBig;
};

// Reads the sat2keyspace table from the given keyspace and builds the
// mapping between the sat and the keyspace name selecting only suitable
// records. Also picks the resolver (idmain usually) keyspace and checks that
// it appears once.
bool FetchSatToKeyspaceMapping(const string &  mapping_keyspace,
                               shared_ptr<CCassConnection>  conn,
                               vector<tuple<string, ECassSchemaType>> &  mapping,
                               string &  resolver_keyspace,
                               ECassSchemaType  resolver_schema,
                               string &  err_msg);

bool FetchSatToKeyspaceMapping(const string &  mapping_keyspace,
                               shared_ptr<CCassConnection>  conn,
                               vector<string> &  mapping,
                               ECassSchemaType  mapping_schema,
                               string &  resolver_keyspace,
                               ECassSchemaType  resolver_schema,
                               vector<pair<string, int32_t>> &  bioseq_na_keyspaces,
                               ECassSchemaType  bioseq_na_schema,
                               string &  err_msg);

bool FetchMessages(const string &  mapping_keyspace,
                   shared_ptr<CCassConnection>  conn,
                   CPSGMessages &  messages,
                   string &  err_msg);
//------------------------------------------------------------------------------

struct SSatInfoEntry final
{
    static constexpr int32_t kInvalidSat{-100};

    int32_t sat{kInvalidSat};
    ECassSchemaType schema_type{eUnknownSchema};
    string keyspace;
    string service;
    shared_ptr<CCassConnection> connection;
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

    CSatInfoSchema() = delete;

    /// @param registry
    ///   Configuration registry used to create new cluster connections
    /// @param registry_section
    ///   Registry section name used as a template creating new cluster connection
    ///   {service} entry of registry section gets replaced by {service} from {sat_info3.sat2keyspace} table
    CSatInfoSchema(
        CNcbiRegistry const & registry,
        string const & registry_section
    );

    optional<SSatInfoEntry> GetBlobKeyspace(int32_t sat) const;
    vector<SSatInfoEntry> GetNAKeyspaces() const;
    SSatInfoEntry GetResolverKeyspace() const;

    int32_t GetMaxBlobKeyspaceSat() const;

 private:
    shared_ptr<CCassConnection> x_GetConnectionByService(string const& service) const;
    shared_ptr<CCassConnection> x_GetConnectionByConnectionPoint(string const& connection_point) const;

    optional<ESatInfoRefreshSchemaResult> x_AddClusterConnection(shared_ptr<CCassConnection> const& connection, bool is_default);
    optional<ESatInfoRefreshSchemaResult> x_AddSatInfoEntry(SSatInfoEntry&& entry, shared_ptr<CSatInfoSchema> const& old_schema);
    optional<ESatInfoRefreshSchemaResult> x_AddClusterByServiceName(
        string const& service,
        shared_ptr<CSatInfoSchema> const& old_schema,
        shared_ptr<CCassConnection>& cluster
    );
    optional<ESatInfoRefreshSchemaResult> x_ResolveServiceName(string const& service, vector<string>& connection_points);

    const CNcbiRegistry & m_Registry;
    string m_RegistrySection;

    // Mapping {sat => Connection to blob keyspace}
    map<int32_t, SSatInfoEntry> m_BlobKeyspaces;

    // List of BioseqNA connections
    vector<SSatInfoEntry> m_BioseqNaKeyspaces;

    // Connection to resolve keyspace
    SSatInfoEntry m_ResolverKeyspace;

    // Host:port => ClusterConnection
    map<string, shared_ptr<CCassConnection>> m_Point2Cluster;

    // ServiceName from service column => ClusterConnection
    map<string, shared_ptr<CCassConnection>> m_Service2Cluster;

    // Default cluster connection
    shared_ptr<CCassConnection> m_DefaultCluster;
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
        string const & domain,
        shared_ptr<CCassConnection> sat_info_connection,
        const CNcbiRegistry & registry,
        const string & registry_section
    );

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
    ///   Configuration entry of empty optional
    ///   if sat does not exists or has non blob schema type
    ///
    /// @thread_safe
    optional<SSatInfoEntry> GetBlobKeyspace(int32_t sat) const;

    /// Get list of BioseqNA keyspaces connections
    ///
    /// @thread_safe
    vector<SSatInfoEntry> GetNAKeyspaces() const;

    /// Get connection to resolve keyspace
    ///
    /// @thread_safe
    SSatInfoEntry GetResolverKeyspace() const;

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
        shared_ptr<CSatInfoSchema>& schema,
        vector<SSatInfoEntry>&& sat_info,
        shared_ptr<CSatInfoSchema> const& old_schema
    );

    string m_SatInfoKeyspace;
    string m_Domain;
    shared_ptr<CCassConnection> m_SatInfoConnection;

    const CNcbiRegistry & m_Registry;
    string m_RegistrySection;

    shared_ptr<CSatInfoSchema> m_SatInfoSchema;
    shared_ptr<CPSGMessages> m_SatInfoMessages;
    size_t m_SatInfoHash{0};

    shared_ptr<string> m_RefreshErrorMessage;
};

END_IDBLOB_SCOPE

#endif
