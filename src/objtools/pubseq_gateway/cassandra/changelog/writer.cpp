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
 * File Description:
 *
 * Blob storage: blob changelog writer
 */

#include <ncbi_pch.hpp>

#include "writer.hpp"

#include <string>
#include <sstream>

#include <corelib/ncbitime.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/changelog/record.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

void CBlobChangelogWriter::WriteChangelogEvent(
    CCassQuery* query, string const &keyspace, CBlobChangelogRecord const& record) const
{
    string sql = "INSERT INTO " + keyspace + ".blob_prop_change_log "
        "(updated_time, sat_key, last_modified, op) VALUES (?,?,?,?) USING TTL "
        + NStr::NumericToString(CBlobChangelogRecord::GetTTL());
    query->SetSQL(sql, 4);
    query->BindInt64(0,
                     record.GetUpdatedTimePartition(CBlobChangelogRecord::GetPartitionSize()).GetTimeT() * 1000);
    query->BindInt32(1, record.GetSatKey());
    query->BindInt64(2, record.GetLastModified());
    query->BindInt8(3, record.GetOperationBase());
    query->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, true);
}

END_IDBLOB_SCOPE

