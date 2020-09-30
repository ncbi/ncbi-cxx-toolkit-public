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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 * The functionality not directly related to blob operations
 *
 */

#include <corelib/ncbistd.hpp>
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

END_IDBLOB_SCOPE

#endif
