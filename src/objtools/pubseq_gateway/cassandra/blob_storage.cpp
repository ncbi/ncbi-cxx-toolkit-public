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

BEGIN_IDBLOB_SCOPE


void FetchSatToKeyspaceMapping(const string &  mapping_keyspace,
                               shared_ptr<CCassConnection>  conn,
                               vector<string> &  mapping,
                               int  mapping_schema,
                               string &  resolver_keyspace,
                               int  resolver_schema,
                               string &  err_msg)
{
    shared_ptr<CCassQuery>  query = conn->NewQuery();

    query->SetSQL("SELECT sat, keyspace_name, schema_type FROM " +
                  mapping_keyspace + ".sat2keyspace", 0);
    query->Query(KEYSPACE_MAPPING_CONSISTENCY, false, false);

    while (query->NextRow() == ar_dataready) {
        int32_t     sat = query->FieldGetInt32Value(0);
        string      name = query->FieldGetStrValue(1);
        int32_t     schema_type = query->FieldGetInt32Value(2);

        if (schema_type == mapping_schema) {
            while (static_cast<int32_t>(mapping.size()) <= sat)
                mapping.push_back("");
            mapping[sat] = name;
            continue;
        }

        if (schema_type == resolver_schema) {
            if (resolver_keyspace.empty()) {
                resolver_keyspace = name;
                continue;
            }

            // More than one resolver keyspace
            err_msg = "More than one resolver keyspace in the " +
                      mapping_keyspace + ".sat2keyspace table";
            break;
        }
    }
}


END_IDBLOB_SCOPE
