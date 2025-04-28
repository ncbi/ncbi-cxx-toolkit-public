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
 *  Blob storage: blob status history record
 *
 *****************************************************************************/

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__STATUS_HISTORY__RECORD_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__STATUS_HISTORY__RECORD_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>

#include <sstream>
#include <string>

#include "../IdCassScope.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

using TBlobStatusFlagsBase = int64_t;
enum class EBlobStatusFlags : TBlobStatusFlagsBase {
    eWithdrawn            = 1 << 0,
    eWithdrawnPermanently = 1 << 1,
    /* Begin - Flags transferred from Sybase storage - internal/ID/Common/idstates.h */
    eSuppressPermanently  = 1 << 2,
    eSuppressEditBlocked  = 1 << 3,
    eSuppressTemporary    = 1 << 4,
    /* End */
    /* Begin - Following flags are reserved and should not be used. */
    /* In case transfer Sybase=>Cassandra required later see - */
    /*     @PubSeqOS_source@/PSloadutils.c - function PubSeqOS_idmaint_set_status() */
    eReservedBlobStatusFlag0 = 1 << 5, // eSuppressWarnNoEdit.
    eReservedBlobStatusFlag1 = 1 << 6, // eNoIncrementalProcessing
    eReservedBlobStatusFlag2 = 1 << 7, // Reserved for Suppress extension
    eReservedBlobStatusFlag3 = 1 << 8, // Reserved for Suppress extension
    /* End */
};

class CBlobStatusHistoryRecord {

public:
    CBlobStatusHistoryRecord() = default;
    CBlobStatusHistoryRecord(CBlobStatusHistoryRecord const &) = default;
    CBlobStatusHistoryRecord(CBlobStatusHistoryRecord &&) = default;

    CBlobStatusHistoryRecord& operator=(CBlobStatusHistoryRecord const &) = default;
    CBlobStatusHistoryRecord& operator=(CBlobStatusHistoryRecord&&) = default;

    CBlobStatusHistoryRecord& SetKey(int32_t value) {
        m_SatKey = value;
        return *this;
    }

    CBlobStatusHistoryRecord& SetReplaces(int32_t value) {
        m_Replaces = value;
        return *this;
    }

    CBlobStatusHistoryRecord& SetReplacesIds(vector<int32_t> value) {
        swap(m_ReplacesIds, value);
        return *this;
    }

    CBlobStatusHistoryRecord& SetReplacedByIds(vector<int32_t> value) {
        swap(m_ReplacedByIds, value);
        return *this;
    }

    CBlobStatusHistoryRecord& SetDoneWhen(int64_t value) {
        m_DoneWhen = value;
        return *this;
    }

    CBlobStatusHistoryRecord& SetComment(string value) {
        m_Comment = std::move(value);
        return *this;
    }

    CBlobStatusHistoryRecord& SetPublicComment(string value) {
        m_PublicComment = std::move(value);
        return *this;
    }

    CBlobStatusHistoryRecord& SetUserName(string value) {
        m_UserName = std::move(value);
        return *this;
    }

    CBlobStatusHistoryRecord& SetFlag(bool set_flag, EBlobStatusFlags flag) {
        if (set_flag) {
            m_Flags |= static_cast<TBlobStatusFlagsBase>(flag);
        } else {
            m_Flags &= ~(static_cast<TBlobStatusFlagsBase>(flag));
        }
        return *this;
    }

    CBlobStatusHistoryRecord& SetFlags(TBlobStatusFlagsBase value) {
        m_Flags = value;
        return *this;
    }

    int32_t GetSatKey() const {
        return m_SatKey;
    }

    // Blob "replaces"/"replaced_by" replation is many-to-many for some blobs
    //   so single sat_key cannot properly represent such relation
    NCBI_STD_DEPRECATED("Use GetReplacesIds() instead (deprecated from 2023-07-11)")
    int32_t GetReplaces() const {
        return m_Replaces;
    }

    vector<int32_t> const& GetReplacesIds() const {
        return m_ReplacesIds;
    }

    vector<int32_t> const& GetReplacedByIds() const {
        return m_ReplacedByIds;
    }

    int64_t GetDoneWhen() const {
        return m_DoneWhen;
    }

    string GetUserName() const {
        return m_UserName;
    }

    string GetComment() const {
        return m_Comment;
    }

    string GetPublicComment() const {
        return m_PublicComment;
    }

    bool GetFlag(EBlobStatusFlags flag) const {
        return m_Flags & static_cast<TBlobStatusFlagsBase>(flag);
    }

    TBlobStatusFlagsBase GetFlags() const {
        return m_Flags;
    }

    string ToString() const {
        stringstream ss;
        CTime done_time(m_DoneWhen / 1000);
        done_time.SetMilliSecond(m_DoneWhen % 1000);
        ss << "{sat_key: " << m_SatKey << ", done_when: '" << done_time.ToLocalTime().AsString(CTimeFormat("Y-M-D h:m:s.r")) << "', flags: " << m_Flags
           << ", user: '" << m_UserName
           << "', comment: '" << m_Comment
           << "', public comment: '" << m_PublicComment
           << "', replaces_ids: [" << NStr::Join(m_ReplacesIds, ",") << "], replaces: " << to_string(m_Replaces)
           << ", replaced_by_ids: [" << NStr::Join(m_ReplacedByIds, ",") << "]}";
        return ss.str();
    }

private:
     int32_t m_SatKey{0};
     int32_t m_Replaces{0};
     int64_t m_DoneWhen{0};
     TBlobStatusFlagsBase m_Flags{0};
     string m_UserName;
     string m_Comment;
     string m_PublicComment;
     vector<int32_t> m_ReplacesIds;
     vector<int32_t> m_ReplacedByIds;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__STATUS_HISTORY__RECORD_HPP
