/* ===========================================================================
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
 * ===========================================================================
 *
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 *  Class to produce change log write queries
 *
 */

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__CHANGELOG__BISI_WRITER_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__CHANGELOG__BISI_WRITER_HPP

#include <string>

#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/diag/AppLog.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/changelog/bisi_record.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CBiSiChangelogWriter
{
 public:
    CBiSiChangelogWriter() = default;
    void WriteSiEvent(CCassQuery& query, string const & keyspace, CSiChangelogRecord const & record, int64_t ttl) const
    {
        query.SetSQL(
            "INSERT INTO " + keyspace + ".si2csi_change_log (updated_time, seq_id, seq_id_type, op)"
            " VALUES (?,?,?,?) USING TTL " + NStr::NumericToString(ttl), 4);
        query.BindInt64(0, record.GetPartition());
        query.BindStr(1, record.GetSeqId());
        query.BindInt16(2, record.GetSeqIdType());
        query.BindInt8(3, record.GetOperationBase());
        query.Execute(CASS_CONSISTENCY_LOCAL_QUORUM, true);
    }

    void WriteSiPartition(CCassQuery& query, string const & keyspace, int64_t partition, int64_t ttl) const
    {
        query.SetSQL(
            "INSERT INTO " + keyspace + ".si2csi_change_log (updated_time, seq_id, seq_id_type, op)"
            " VALUES (?,'',0,?) USING TTL " + NStr::NumericToString(ttl), 2);
        query.BindInt64(0, partition);
        query.BindInt8 (1, static_cast<TBiSiChangelogOperationBase>(TBiSiChangelogOperation::eChangeLogOpBeacon));
        query.Execute(CASS_CONSISTENCY_LOCAL_QUORUM, true);
    }

    void WriteBiEvent(CCassQuery& query, string const & keyspace, CBiChangelogRecord const & record, int64_t ttl) const
    {
        query.SetSQL(
            "INSERT INTO " + keyspace + ".bioseq_info_change_log"
            " (updated_time, accession, version, seq_id_type, gi, op)"
            " VALUES (?,?,?,?,?,?)"
            " USING TTL " + NStr::NumericToString(ttl), 6);
        query.BindInt64(0, record.GetPartition());
        query.BindStr(1, record.GetAccession());
        query.BindInt16(2, record.GetVersion());
        query.BindInt16(3, record.GetSeqIdType());
        query.BindInt64(4, record.GetGi());
        query.BindInt8(5, static_cast<TBiSiChangelogOperationBase>(TBiSiChangelogOperation::eChangeLogOpInsert));
        query.Execute(CASS_CONSISTENCY_LOCAL_QUORUM, true);
    }

    void WriteBiPartition(CCassQuery& query, string const & keyspace, int64_t partition, int64_t ttl) const
    {
        query.SetSQL(
            "INSERT INTO " + keyspace + ".bioseq_info_change_log"
            " (updated_time, accession, version, seq_id_type, gi, op)"
            " VALUES (?,'',0,0,0,?)"
            " USING TTL " + NStr::NumericToString(ttl), 2);
        query.BindInt64(0, partition);
        query.BindInt8 (1, static_cast<TBiSiChangelogOperationBase>(TBiSiChangelogOperation::eChangeLogOpBeacon));
        query.Execute(CASS_CONSISTENCY_LOCAL_QUORUM, true);
    }
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__CHANGELOG__BISI_WRITER_HPP
