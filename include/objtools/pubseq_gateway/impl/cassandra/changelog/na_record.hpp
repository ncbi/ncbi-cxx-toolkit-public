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
 *  Blob storage: Named annotation changelog record
 *
 *****************************************************************************/

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__CHANGELOG__NA_RECORD_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__CHANGELOG__NA_RECORD_HPP

#include <corelib/ncbistd.hpp>

#include <string>

#include <corelib/ncbitime.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/nannot/record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/changelog/record.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CNAnnotChangelogRecord {
 public:
    static bool IsOperationValid(TChangelogOperation op)
    {
        return op > TChangelogOperation::eUndefined && op <= TChangelogOperation::eChangeLogPartitionUpdated;
    }

    CNAnnotChangelogRecord()
        : CNAnnotChangelogRecord("", "", 0, 0, 0, TChangelogOperation::eUndefined)
    {}

    CNAnnotChangelogRecord(
        string accession,
        string annot_name,
        int16_t version,
        int16_t seq_id_type,
        CBlobRecord::TTimestamp modified,
        TChangelogOperation operation
    )
        : m_Accession(accession)
        , m_AnnotName(annot_name)
        , m_Version(version)
        , m_SeqIdType(seq_id_type)
        , m_Modified(modified)
        , m_Operation(operation)
        , m_UpdatedTime(CTime::eCurrent, CTime::eUTC)
    {

    }
    CNAnnotChangelogRecord(CNAnnotChangelogRecord const &) = default;
    CNAnnotChangelogRecord(CNAnnotChangelogRecord &&) = default;

    CNAnnotChangelogRecord& operator=(CNAnnotChangelogRecord const &) = default;
    CNAnnotChangelogRecord& operator=(CNAnnotChangelogRecord&&) = default;

    string const & GetAccession() const
    {
        return m_Accession;
    }

    int16_t GetVersion() const
    {
        return m_Version;
    }

    int16_t GetSeqIdType() const
    {
        return m_SeqIdType;
    }

    string const & GetAnnotName() const
    {
        return m_AnnotName;
    }

    CBlobRecord::TTimestamp GetModified() const
    {
        return m_Modified;
    }

    TChangelogOperation GetOperation() const
    {
        return m_Operation;
    }

    TChangelogOperationBase GetOperationBase() const
    {
        return static_cast<TChangelogOperationBase>(m_Operation);
    }

    CTime GetUpdatedTime() const
    {
        return m_UpdatedTime;
    }

    CTime GetUpdatedTimePartition(uint64_t partition_seconds) const
    {
        return CBlobChangelogRecord::GetUpdatedTimePartition(m_UpdatedTime, partition_seconds);
    }

    string ToString() const
    {
        stringstream s;
        s << "CNAnnotChangelogRecord" << endl
          << "\tm_Accession: " << m_Accession << endl
          << "\tm_Version: " << m_Version << endl
          << "\tm_SeqIdType: " << m_SeqIdType << endl
          << "\tm_AnnotName: " << m_AnnotName << endl
          << "\tm_Modified: " << m_Modified << endl
          << "\tm_Operation: " << static_cast<TChangelogOperationBase>(m_Operation) << endl
          << "\tm_UpdatedTime: " << m_UpdatedTime << endl;
        return s.str();
    }

 private:
    string m_Accession;
    string m_AnnotName;
    int16_t m_Version;
    int16_t m_SeqIdType;
    CBlobRecord::TTimestamp m_Modified;

    TChangelogOperation m_Operation;
    CTime m_UpdatedTime;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__CHANGELOG__NA_RECORD_HPP
