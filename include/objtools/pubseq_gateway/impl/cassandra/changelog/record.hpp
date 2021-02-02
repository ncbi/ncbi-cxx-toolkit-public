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
 *  Blob storage: blob record
 *
 *****************************************************************************/

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__CHANGELOG__RECORD_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__CHANGELOG__RECORD_HPP

#include <corelib/ncbistd.hpp>

#include <string>

#include <corelib/ncbitime.hpp>

#include "../IdCassScope.hpp"
#include "../blob_record.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

using TChangelogOperationBase = int8_t;
enum class TChangelogOperation : TChangelogOperationBase {
    eUndefined = 0,
    eUpdated = 1,
    eDeleted = 2,
    eChangeLogPartitionUpdated = 3,
    eStatusHistoryInserted = 4,
    eStatusHistoryDeleted = 5,
};

class CBlobChangelogRecord {
 public:
    static bool IsOperationValid(TChangelogOperation op)
    {
        return op > TChangelogOperation::eUndefined && op <= TChangelogOperation::eStatusHistoryDeleted;
    }

    static CTime GetUpdatedTimePartition(CTime updated_time, uint64_t partition_seconds);

    static uint64_t GetPartitionSize();
    static void SetPartitionSize(uint64_t value);
    static void SetTTL(uint64_t value);
    static uint64_t GetTTL();

    CBlobChangelogRecord();
    CBlobChangelogRecord(
        CBlobRecord::TSatKey sat_key,
        CBlobRecord::TTimestamp last_modified,
        TChangelogOperation operation
    );
    CBlobChangelogRecord(CBlobChangelogRecord const &) = default;
    CBlobChangelogRecord(CBlobChangelogRecord &&) = default;

    CBlobChangelogRecord& operator=(CBlobChangelogRecord const &) = default;
    CBlobChangelogRecord& operator=(CBlobChangelogRecord&&) = default;

    CBlobRecord::TSatKey GetSatKey() const;
    CBlobRecord::TTimestamp GetLastModified() const;

    TChangelogOperation GetOperation() const;
    TChangelogOperationBase GetOperationBase() const;

    CTime GetUpdatedTime() const;
    CTime GetUpdatedTimePartition(uint64_t partition_seconds) const;

    void SetUpdatedTime(CTime time);

    string ToString() const;

 private:
    static uint64_t sPartitionSize;
    static uint64_t sTTL;

    CBlobRecord::TSatKey m_SatKey;
    CBlobRecord::TTimestamp m_LastModified;
    TChangelogOperation m_Operation;
    CTime m_UpdatedTime;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__CHANGELOG__RECORD_HPP
