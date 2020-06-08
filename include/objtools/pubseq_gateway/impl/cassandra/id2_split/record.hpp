/*****************************************************************************
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
 *  Blob storage: ID2 split record
 *
 *****************************************************************************/

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__ID2_SPLIT__RECORD_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__ID2_SPLIT__RECORD_HPP

#include <corelib/ncbistd.hpp>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "../IdCassScope.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CID2SplitRecord {
 public:
    using TTimestamp = int64_t;
    using TSatKey = int32_t;

 public:
    CID2SplitRecord()
        : m_Ent(0)
        , m_Sat(0)
        , m_SatKey(0)
        , m_EntType(0)
        , m_SplitId(0)
        , m_Length(0)
        , m_FullLength(0)
        , m_Modified(0)
    {
    }

    CID2SplitRecord& operator=(CID2SplitRecord const &) = default;
    CID2SplitRecord& operator=(CID2SplitRecord&&) = default;

    CID2SplitRecord& SetUniqueId(TSatKey value)
    {  m_Ent = value;  return *this; }

    CID2SplitRecord& SetSat(int16_t value)
    {  m_Sat = value;  return *this; }

    CID2SplitRecord& SetSatKey(TSatKey value)
    {  m_SatKey = value;  return *this; }

    CID2SplitRecord& SetEntType(int16_t value)
    {  m_EntType = value;  return *this; }

    CID2SplitRecord& SetSplitId(int16_t value)
    {  m_SplitId = value; return *this; }

    CID2SplitRecord& SetUserName(string value)
    {  m_UserName = value; return *this; }

    CID2SplitRecord& SetLength(int64_t value)
    {  m_Length = value; return *this; }

    CID2SplitRecord& SetFullLen(int64_t value)
    {  m_FullLength = value; return *this; }

    CID2SplitRecord& SetModified(TTimestamp value)
    {  m_Modified = value;  return *this; }

    // ---------------- Getters ------------------------
    TSatKey GetUniqueId() const
    {  return m_Ent; }

    int16_t GetSat() const
    {  return m_Sat; }

    TSatKey GetSatKey() const
    {  return m_SatKey; }
  
    int16_t GetEntType() const
    {  return m_EntType; }

    int16_t GetSplitId() const
    {  return m_SplitId; }
  
    string const & GetUserName() const
    {  return m_UserName; }

    int64_t GetLength() const
    {  return m_Length; }

    int64_t GetFullLen() const
    {  return m_FullLength; }

    TTimestamp GetModified() const
    {  return m_Modified; }

 private:
    TSatKey m_Ent;      // split.ent      - serial ID
    int16_t m_Sat;      // split.sat      - of original blob
    TSatKey m_SatKey;   // split.sat_key  - of original blob
    int16_t m_EntType;  // split.ent_type - 9 (split_info) or 10 (chunk) [| 32 (deleted)]
    int16_t m_SplitId;  // split.split_id - -1 (split_info) or serial number of a chunk
    string  m_UserName; // split.user_name - submitter
    int64_t m_Length;   // split.length - blob size in the database (possibly compressed)
    int64_t m_FullLength; // split.full_length - size of uncompressed blob
    TTimestamp m_Modified; // split.modified - time stamp


};

using TID2SplitConsumeCallback = function<bool(CID2SplitRecord &&, bool last)>;

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__ID2_SPLIT__RECORD_HPP
