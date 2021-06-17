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
#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>

#define KEYSPACE_MAPPING_CONSISTENCY    CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM
#define KEYSPACE_MAPPING_RETRY          5

BEGIN_IDBLOB_SCOPE


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
            shared_ptr<CCassQuery>  query = conn->NewQuery();

            query->SetSQL("SELECT\n"
                          "    sat,\n"
                          "    keyspace_name,\n"
                          "    schema_type\n"
                          "FROM\n"
                          "    " + mapping_keyspace + ".sat2keyspace", 0);
            query->Query(KEYSPACE_MAPPING_CONSISTENCY, false, false);

            rv = true;
            while (query->NextRow() == ar_dataready) {
                int32_t     sat = query->FieldGetInt32Value(0);
                string      name = query->FieldGetStrValue(1);
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


END_IDBLOB_SCOPE
