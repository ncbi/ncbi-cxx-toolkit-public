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

#include <string>
#include <sstream>

#include <corelib/ncbitime.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/changelog/record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/changelog/writer.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

void CBlobChangelogWriter::WriteChangelogEvent(
    CCassQuery* query, string const &keyspace, CBlobChangelogRecord const& record) const
{
    string sql = "INSERT INTO " + keyspace + ".blob_prop_change_log "
        "(updated_time, sat_key, last_modified, op) VALUES (?,?,?,?) USING TTL "
        + NStr::NumericToString(CBlobChangelogRecord::GetTTL());
    query->SetSQL(sql, 4);
    int64_t updated_time_partition =
        record.GetUpdatedTimePartition(CBlobChangelogRecord::GetPartitionSize()).GetTimeT() * 1000;
    query->BindInt64(0, updated_time_partition);
    query->BindInt32(1, record.GetSatKey());
    query->BindInt64(2, record.GetLastModified());
    query->BindInt8(3, record.GetOperationBase());
    query->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, true);

    // Special record - one per partition sat_key = 0, last_modified = 0, op = eChangeLogPartitionUpdated
    // to track changelog partition changes writetime(op) == last change timestamp for changelog partition
    query->SetSQL(sql, 4);
    query->BindInt64(0, updated_time_partition);
    query->BindInt32(1, 0);
    query->BindInt64(2, 0);
    query->BindInt8(3, static_cast<TChangelogOperationBase>(TChangelogOperation::eChangeLogPartitionUpdated));
    query->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, true);
}

void CNAnnotChangelogWriter::WriteChangelogEvent(CCassQuery* query, string const& keyspace, CNAnnotChangelogRecord const& record) const
{
    string sql = "INSERT INTO " + keyspace + ".bioseq_na_change_log "
        "(updated_time, accession, version, seq_id_type, annot_name, last_modified, op) VALUES (?,?,?,?,?,?,?) USING TTL "
        + NStr::NumericToString(CBlobChangelogRecord::GetTTL());
    query->SetSQL(sql, 7);
    int64_t updated_time_partition =
        record.GetUpdatedTimePartition(CBlobChangelogRecord::GetPartitionSize()).GetTimeT() * 1000;
    query->BindInt64(0, updated_time_partition);
    query->BindStr(1, record.GetAccession());
    query->BindInt16(2, record.GetVersion());
    query->BindInt16(3, record.GetSeqIdType());
    query->BindStr(4, record.GetAnnotName());
    query->BindInt64(5, record.GetModified());
    query->BindInt8(6, record.GetOperationBase());
    query->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, true);

    query->SetSQL(sql, 7);
    query->BindInt64(0, updated_time_partition);
    query->BindStr(1, "");
    query->BindInt16(2, 0);
    query->BindInt16(3, 0);
    query->BindStr(4, "");
    query->BindInt64(5, 0);
    query->BindInt8(6, static_cast<TChangelogOperationBase>(TChangelogOperation::eChangeLogPartitionUpdated));
    query->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, true);
}

END_IDBLOB_SCOPE

