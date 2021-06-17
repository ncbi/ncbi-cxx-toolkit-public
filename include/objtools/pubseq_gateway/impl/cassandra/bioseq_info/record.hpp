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
 *  Blob storage: bioseq_info record
 *
 *****************************************************************************/

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__BIOSEQ_INFO__RECORD_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__BIOSEQ_INFO__RECORD_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/request_status.hpp>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CBioseqInfoRecord
{
 public:
    using TAccession = string;
    using TName = string;
    using TVersion = int16_t;
    using TSeqIdType = int16_t;
    using TDateChanged = int64_t;
    using THash = int32_t;
    using TGI = int64_t;
    using TLength = int32_t;
    using TMol = int8_t;
    using TSat = int16_t;
    using TSatKey = int32_t;
    using TSeqIds = set<tuple<int16_t, string>>;
    using TSeqState = int8_t;
    using TState = int8_t;
    using TTaxId = int32_t;
    using TWritetime = int64_t;

    static const TState kStateAlive = 10;

 public:
    CBioseqInfoRecord()
        : m_Version(-1)
        , m_SeqIdType(-1)
        , m_GI(-1)
        , m_DateChanged(0)
        , m_Hash(-1)
        , m_Length(-1)
        , m_Mol(-1)
        , m_Sat(-1)
        , m_SatKey(-1)
        , m_SeqState(-1)
        , m_State(-1)
        , m_TaxId(-1)
        , m_Writetime(-1)
    {}

    void Reset(void);

    CBioseqInfoRecord(CBioseqInfoRecord const &) = default;
    CBioseqInfoRecord & operator=(CBioseqInfoRecord const &) = default;
    CBioseqInfoRecord & operator=(CBioseqInfoRecord &&) = default;

    // Setters
    CBioseqInfoRecord & SetAccession(const TAccession &  value)
    {
        m_Accession = value;
        return *this;
    }

    CBioseqInfoRecord & SetVersion(TVersion  value)
    {
        m_Version = value;
        return *this;
    }

    CBioseqInfoRecord & SetSeqIdType(TSeqIdType  value)
    {
        m_SeqIdType = value;
        return *this;
    }

    CBioseqInfoRecord & SetDateChanged(TDateChanged  value)
    {
        m_DateChanged = value;
        return *this;
    }

    CBioseqInfoRecord & SetHash(THash  value)
    {
        m_Hash = value;
        return *this;
    }

    CBioseqInfoRecord & SetGI(TGI value)
    {
        m_GI = value;
        return *this;
    }

    CBioseqInfoRecord & SetLength(TLength  value)
    {
        m_Length = value;
        return *this;
    }

    CBioseqInfoRecord & SetMol(TMol  value)
    {
        m_Mol = value;
        return *this;
    }

    CBioseqInfoRecord & SetSat(TSat  value)
    {
        m_Sat = value;
        return *this;
    }

    CBioseqInfoRecord & SetSatKey(TSatKey  value)
    {
        m_SatKey = value;
        return *this;
    }

    CBioseqInfoRecord & SetSeqIds(TSeqIds const &  value)
    {
        m_SeqIds = value;
        return *this;
    }

    CBioseqInfoRecord & SetSeqIds(TSeqIds&& value)
    {
        m_SeqIds = move(value);
        return *this;
    }

    CBioseqInfoRecord & SetSeqState(TSeqState  value)
    {
        m_SeqState = value;
        return *this;
    }

    CBioseqInfoRecord & SetState(TState  value)
    {
        m_State = value;
        return *this;
    }

    CBioseqInfoRecord & SetTaxId(TTaxId  value)
    {
        m_TaxId = value;
        return *this;
    }

    CBioseqInfoRecord & SetName(TName  value)
    {
        m_Name = value;
        return *this;
    }

    CBioseqInfoRecord & SetWritetime(TWritetime  value)
    {
        m_Writetime = value;
        return *this;
    }


    // Getters
    TAccession const & GetAccession(void) const
    {
        return m_Accession;
    }

    TVersion  GetVersion(void) const
    {
        return m_Version;
    }

    TSeqIdType GetSeqIdType(void) const
    {
        return m_SeqIdType;
    }

    TDateChanged GetDateChanged(void) const
    {
        return m_DateChanged;
    }

    THash GetHash(void) const
    {
        return m_Hash;
    }

    TGI GetGI(void) const
    {
        return m_GI;
    }

    TLength GetLength(void) const
    {
        return m_Length;
    }

    TMol GetMol(void) const
    {
        return m_Mol;
    }

    TSat GetSat(void) const
    {
        return m_Sat;
    }

    TSatKey GetSatKey(void) const
    {
        return m_SatKey;
    }

    TSeqIds & GetSeqIds(void)
    {
        return m_SeqIds;
    }

    TSeqIds const & GetSeqIds(void) const
    {
        return m_SeqIds;
    }

    TSeqState GetSeqState(void) const
    {
        return m_SeqState;
    }

    TState GetState(void) const
    {
        return m_State;
    }

    TTaxId GetTaxId(void) const
    {
        return m_TaxId;
    }

    TName GetName(void) const
    {
        return m_Name;
    }

    TWritetime GetWritetime(void) const
    {
        return m_Writetime;
    }

    string ToString(void) const;

 private:
    TAccession m_Accession;
    TVersion m_Version;
    TSeqIdType m_SeqIdType;
    TGI m_GI;

    TName               m_Name;
    TDateChanged        m_DateChanged;
    THash               m_Hash;
    TLength             m_Length;
    TMol                m_Mol;
    TSat                m_Sat;
    TSatKey             m_SatKey;
    TSeqIds             m_SeqIds;
    TSeqState           m_SeqState;
    TState              m_State;
    TTaxId              m_TaxId;
    TWritetime          m_Writetime;
};

using TBioseqInfoConsumeCallback = function<void(vector<CBioseqInfoRecord> &&)>;

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__BIOSEQ_INFO__RECORD_HPP
