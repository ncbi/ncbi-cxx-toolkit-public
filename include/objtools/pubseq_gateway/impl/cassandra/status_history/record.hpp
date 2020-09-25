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

#include <sstream>
#include <string>

#include "../IdCassScope.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

using TBlobStatusFlagsBase = int64_t;
enum class EBlobStatusFlags : TBlobStatusFlagsBase {
    eWithdrawn            = 1,
    eWithdrawnPermanently = 2,
    eSuppressPermanently  = 4,
    eSuppressEditBlocked  = 8,
    eSuppressTemporary    = 16,
};

class CBlobStatusHistoryRecord {

 public:
    CBlobStatusHistoryRecord()
        : m_SatKey(0)
        , m_Replaces(0)
        , m_DoneWhen(0)
        , m_Flags(0)
    {}
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

    CBlobStatusHistoryRecord& SetDoneWhen(int64_t value) {
        m_DoneWhen = value;
        return *this;
    }

    CBlobStatusHistoryRecord& SetComment(string value) {
        m_Comment = move(value);
        return *this;
    }

    CBlobStatusHistoryRecord& SetPublicComment(string value) {
        m_PublicComment = move(value);
        return *this;
    }

    CBlobStatusHistoryRecord& SetUserName(string value) {
        m_UserName = move(value);
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

    int32_t GetSatKey() const {
        return m_SatKey;
    }

    int32_t GetReplaces() const {
        return m_Replaces;
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

    string ToString() const {
        stringstream ss;
        ss << "SatKey: " << m_SatKey << endl
           << "DoneWhen:" << m_DoneWhen << endl
           << "Flags: " << m_Flags << endl
           << "User: " << m_UserName << endl
           << "Comment: " << m_Comment << endl
           << "Public comment: " << m_PublicComment << endl
           << "Replaces: " << m_Replaces << endl;

        return ss.str();
    }

 private:
     int32_t m_SatKey;
     int32_t m_Replaces;
     int64_t m_DoneWhen;
     TBlobStatusFlagsBase m_Flags;
     string m_UserName;
     string m_Comment;
     string m_PublicComment;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__STATUS_HISTORY__RECORD_HPP
