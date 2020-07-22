/*****************************************************************************
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
 *  BiSi: changelog record
 *
 *****************************************************************************/

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__CHANGELOG__BISI_RECORD_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__CHANGELOG__BISI_RECORD_HPP

#include <corelib/ncbistd.hpp>

#include <chrono>
#include <string>

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_util.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

using TBiSiChangelogOperationBase = int8_t;
enum class TBiSiChangelogOperation : TBiSiChangelogOperationBase
{
    eUndefined = 0,
    eChangeLogOpInsert = 'I',
    eChangeLogOpErase = 'E',
    eChangeLogOpBeacon = 'U',
};

class CBiSiPartitionMaker
{
    static constexpr int64_t kChangelogGranularitySec = 120;
    static constexpr int64_t kChangelogTtlSec = 14L * 24 * 3600;  // 14 days

 public:
    int64_t GetCurrentPartition() const
    {
        CTime time;
        time.SetCurrent();
        int64_t update_time = CTimeToTimeTms(time);
        int64_t granularity_ms = kChangelogGranularitySec * 1000;
        return (update_time / granularity_ms) * granularity_ms;
    }

    constexpr int64_t GetGranularitySec() const
    {
        return kChangelogGranularitySec;
    }

    constexpr int64_t GetTTLSec() const
    {
        return kChangelogTtlSec;
    }
};

class CSiChangelogRecord
{

 public:
    CSiChangelogRecord();
    CSiChangelogRecord(int64_t partition, string seq_id, int16_t seq_id_type, TBiSiChangelogOperation operation)
        : m_Partition(partition)
        , m_SeqId(seq_id)
        , m_SeqIdType(seq_id_type)
        , m_Operation(operation)
    {
    }
    CSiChangelogRecord(CSiChangelogRecord const &) = default;
    CSiChangelogRecord(CSiChangelogRecord &&) = default;

    CSiChangelogRecord& operator=(CSiChangelogRecord const &) = default;
    CSiChangelogRecord& operator=(CSiChangelogRecord&&) = default;

    int64_t GetPartition() const {
        return m_Partition;
    }
    string GetSeqId() const {
        return m_SeqId;
    }
    int16_t GetSeqIdType() const {
        return m_SeqIdType;
    }

    TBiSiChangelogOperation GetOperation() const {
        return m_Operation;
    }
    TBiSiChangelogOperationBase GetOperationBase() const {
        return static_cast<TBiSiChangelogOperationBase>(m_Operation);
    }

 private:
    int64_t m_Partition;
    string m_SeqId;
    int16_t m_SeqIdType;
    TBiSiChangelogOperation m_Operation;
};

class CBiChangelogRecord
{
 public:
    CBiChangelogRecord();
    CBiChangelogRecord(
        int64_t partition, string accession, int16_t version, int16_t seq_id_type, int64_t gi, TBiSiChangelogOperation operation
    )
        : m_Partition(partition)
        , m_Accession(accession)
        , m_Gi(gi)
        , m_Version(version)
        , m_SeqIdType(seq_id_type)
        , m_Operation(operation)
    {
    }
    CBiChangelogRecord(CBiChangelogRecord const &) = default;
    CBiChangelogRecord(CBiChangelogRecord &&) = default;

    CBiChangelogRecord& operator=(CBiChangelogRecord const &) = default;
    CBiChangelogRecord& operator=(CBiChangelogRecord&&) = default;

    int64_t GetPartition() const {
        return m_Partition;
    }
    string GetAccession() const {
        return m_Accession;
    }
    int64_t GetGi() const {
        return m_Gi;
    }
    int16_t GetVersion() const {
        return m_Version;
    }
    int16_t GetSeqIdType() const {
        return m_SeqIdType;
    }

    TBiSiChangelogOperation GetOperation() const {
        return m_Operation;
    }
    TBiSiChangelogOperationBase GetOperationBase() const {
        return static_cast<TBiSiChangelogOperationBase>(m_Operation);
    }

 private:
    int64_t m_Partition;
    string m_Accession;
    int64_t m_Gi;
    int16_t m_Version;
    int16_t m_SeqIdType;
    TBiSiChangelogOperation m_Operation;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__CHANGELOG__BISI_RECORD_HPP
