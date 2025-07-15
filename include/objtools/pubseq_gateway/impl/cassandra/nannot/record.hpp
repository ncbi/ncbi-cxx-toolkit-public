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
 *  Blob storage: named annotation record
 *
 *****************************************************************************/

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__NANNOT__RECORD_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__NANNOT__RECORD_HPP

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

class CNAnnotRecord {
 public:
    using TTimestamp = int64_t;
    using TSatKey = int32_t;
    using TCoord = int32_t;
    using TAnnotInfo = string;
    using TWritetime = int64_t;
    using TState = int8_t;

    enum EState {
        eStateDead = 0,
        eStateAlive = 1,
    };

 public:
    CNAnnotRecord() = default;
    CNAnnotRecord(CNAnnotRecord const &) = default;
    CNAnnotRecord(CNAnnotRecord&&) = default;
    CNAnnotRecord& operator=(CNAnnotRecord const &) = default;
    CNAnnotRecord& operator=(CNAnnotRecord&&) = default;

    CNAnnotRecord& SetAccession(string value)
    {
        m_Accession = value;
        return *this;
    }

    CNAnnotRecord& SetVersion(int16_t value)
    {
        m_Version = value;
        return *this;
    }

    CNAnnotRecord& SetSeqIdType(int16_t value)
    {
        m_SeqIdType = value;
        return *this;
    }

    CNAnnotRecord& SetSatKey(TSatKey value)
    {
        m_SatKey = value;
        return *this;
    }

    CNAnnotRecord& SetModified(TTimestamp value)
    {
        m_Modified = value;
        return *this;
    }

    CNAnnotRecord& SetAnnotInfoModified(TTimestamp value)
    {
        m_AnnotInfoModified = value;
        return *this;
    }

    CNAnnotRecord& SetAnnotName(string value)
    {
        m_AnnotName = value;
        return *this;
    }

    CNAnnotRecord& SetStart(TCoord value)
    {
        m_Start = value;
        return *this;
    }

    CNAnnotRecord& SetStop(TCoord value)
    {
        m_Stop = value;
        return *this;
    }

    CNAnnotRecord& SetPutSat( int value)
    {
        m_PutDBSat = value;
        return *this;
    }

    CNAnnotRecord& SetPutSatKey( int value)
    {
        m_PutDBSatKey = value;
        return *this;
    }

    CNAnnotRecord& SetSeqAnnotInfo(TAnnotInfo const& value)
    {
        m_SeqAnnotInfo = value;
        return *this;
    }

    CNAnnotRecord& SetSeqAnnotInfo(TAnnotInfo&& value)
    {
        m_SeqAnnotInfo = std::move(value);
        return *this;
    }

    CNAnnotRecord& SetWritetime(TWritetime  value)
    {
        m_Writetime = value;
        return *this;
    }

    CNAnnotRecord& SetState(TState  value)
    {
        m_State = value;
        return *this;
    }

    // ---------------- Getters ------------------------
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

    TSatKey GetSatKey() const
    {
        return m_SatKey;
    }

    TCoord GetStart() const
    {
        return m_Start;
    }

    TCoord GetStop() const
    {
        return m_Stop;
    }

    TTimestamp GetModified() const
    {
        return m_Modified;
    }

    TTimestamp GetAnnotInfoModified() const
    {
        return m_AnnotInfoModified;
    }

    string const & GetAnnotName() const
    {
        return m_AnnotName;
    }

    TAnnotInfo const & GetSeqAnnotInfo() const
    {
        return m_SeqAnnotInfo;
    }

    TWritetime GetWritetime() const
    {
        return m_Writetime;
    }

    TState GetState() const
    {
        return m_State;
    }

    int GetPutSat() const
    {
        return m_PutDBSat;
    }

    int GetPutSatKey() const
    {
        return m_PutDBSatKey;
    }
  
    string ToString() const;

 private:
    string m_Accession;
    string m_AnnotName;
    TAnnotInfo m_SeqAnnotInfo;
    TTimestamp m_Modified{0};
    TTimestamp m_AnnotInfoModified{0};
    TWritetime m_Writetime{0};
    TSatKey m_SatKey{0};
    TCoord m_Start{0};
    TCoord m_Stop{0};
    int16_t m_Version{0};
    int16_t m_SeqIdType{0};
    TState m_State{0};
    int    m_PutDBSat{0};    // NAnnotGnn..emap.sat = sat_id of NAnnotP
    int    m_PutDBSatKey{0}; // NAnnotGnn..emap.sat_key = NAnnotP..Entity.ent 
};

using TNAnnotConsumeCallback = function<bool(CNAnnotRecord &&, bool last)>;

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__NANNOT__RECORD_HPP
