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
 * Blob storage: blob changelog record
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/changelog/record.hpp>

#include <string>
#include <sstream>

#include <corelib/ncbitime.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

uint64_t CBlobChangelogRecord::sPartitionSize = 120;
uint64_t CBlobChangelogRecord::sTTL = 2592000;

uint64_t CBlobChangelogRecord::GetPartitionSize()
{
    return sPartitionSize;
}

void CBlobChangelogRecord::SetPartitionSize(uint64_t value)
{
    sPartitionSize = value;
}

void CBlobChangelogRecord::SetTTL(uint64_t value)
{
    sTTL = value;
}

uint64_t CBlobChangelogRecord::GetTTL()
{
    return sTTL;
}

CTime CBlobChangelogRecord::GetUpdatedTimePartition(CTime updated_time, uint64_t partition_seconds)
{
    time_t t = updated_time.ToUniversalTime().GetTimeT();
    return CTime(t - (t % partition_seconds));
}

CBlobChangelogRecord::CBlobChangelogRecord()
    : CBlobChangelogRecord(0, 0, TChangelogOperation::eUndefined)
{
}

CBlobChangelogRecord::CBlobChangelogRecord(
    CBlobRecord::TSatKey sat_key,
    CBlobRecord::TTimestamp last_modified,
    TChangelogOperation operation
)
    : m_SatKey(sat_key)
    , m_LastModified(last_modified)
    , m_Operation(operation)
    , m_UpdatedTime(CTime::eCurrent, CTime::eUTC)
{
}

CBlobRecord::TSatKey CBlobChangelogRecord::GetSatKey() const
{
    return m_SatKey;
}

CBlobRecord::TTimestamp CBlobChangelogRecord::GetLastModified() const
{
    return m_LastModified;
}

TChangelogOperation CBlobChangelogRecord::GetOperation() const
{
    return m_Operation;
}

TChangelogOperationBase CBlobChangelogRecord::GetOperationBase() const
{
    return static_cast<TChangelogOperationBase>(m_Operation);
}

CTime CBlobChangelogRecord::GetUpdatedTime() const
{
    return m_UpdatedTime;
}

CTime CBlobChangelogRecord::GetUpdatedTimePartition(uint64_t partition_seconds) const
{
    return GetUpdatedTimePartition(m_UpdatedTime, partition_seconds);
}


void CBlobChangelogRecord::SetUpdatedTime(CTime time)
{
    m_UpdatedTime = time;
}

string CBlobChangelogRecord::ToString() const
{
    stringstream s;
    s << "CBlobChangelogRecord" << endl
      << "\tm_SatKey: " << m_SatKey << endl
      << "\tm_LastModified: " << m_LastModified << endl
      << "\tm_Operation: " << static_cast<TChangelogOperationBase>(m_Operation) << endl
      << "\tm_UpdatedTime: " << m_UpdatedTime << endl;
    return s.str();
}

END_IDBLOB_SCOPE
