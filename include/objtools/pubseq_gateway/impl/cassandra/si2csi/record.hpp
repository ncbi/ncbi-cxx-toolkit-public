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
 *  Blob storage: si2csi record
 *
 *****************************************************************************/

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__SI2CSI__RECORD_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__SI2CSI__RECORD_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/request_status.hpp>

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

class CSI2CSIRecord
{
 public:
    using TSecSeqId = string;
    using TSecSeqIdType = int16_t;

    using TAccession = string;
    using TGI = int64_t;
    using TSecSeqState = int8_t;
    using TSeqIdType = int16_t;
    using TVersion = int16_t;
    using TWritetime = int64_t;

    CSI2CSIRecord() :
        m_SecSeqIdType(-1),
        m_GI(-1),
        m_SecSeqState(-1),
        m_SeqIdType(-1),
        m_Version(-1),
        m_Writetime(-1)
    {}

    CSI2CSIRecord(CSI2CSIRecord const &) = default;
    CSI2CSIRecord(CSI2CSIRecord &&) = default;
    CSI2CSIRecord & operator=(CSI2CSIRecord const &) = default;
    CSI2CSIRecord & operator=(CSI2CSIRecord &&) = default;

    // Setters
    CSI2CSIRecord & SetSecSeqId(const TSecSeqId &  value)
    {
        m_SecSeqId = value;
        return *this;
    }

    CSI2CSIRecord & SetSecSeqIdType(TSecSeqIdType  value)
    {
        m_SecSeqIdType = value;
        return *this;
    }

    CSI2CSIRecord & SetAccession(const TAccession &  value)
    {
        m_Accession = value;
        return *this;
    }

    CSI2CSIRecord & SetGI(TGI  value)
    {
        m_GI = value;
        return *this;
    }

    CSI2CSIRecord & SetSecSeqState(TSecSeqState  value)
    {
        m_SecSeqState = value;
        return *this;
    }

    CSI2CSIRecord & SetSeqIdType(TSeqIdType  value)
    {
        m_SeqIdType = value;
        return *this;
    }

    CSI2CSIRecord & SetVersion(TVersion  value)
    {
        m_Version = value;
        return *this;
    }

    CSI2CSIRecord & SetWritetime(TWritetime  value)
    {
        m_Writetime = value;
        return *this;
    }


    // Getters
    TSecSeqId const & GetSecSeqId(void) const
    {
        return m_SecSeqId;
    }

    TSecSeqIdType GetSecSeqIdType(void) const
    {
        return m_SecSeqIdType;
    }

    TAccession const & GetAccession(void) const
    {
        return m_Accession;
    }

    TGI GetGI(void) const
    {
        return m_GI;
    }

    TSecSeqState GetSecSeqState(void) const
    {
        return m_SecSeqState;
    }

    TSeqIdType GetSeqIdType(void) const
    {
        return m_SeqIdType;
    }

    TVersion  GetVersion(void) const
    {
        return m_Version;
    }

    TWritetime  GetWritetime(void) const
    {
        return m_Writetime;
    }

    string ToString(void) const;

 private:
    TSecSeqId           m_SecSeqId;
    TSecSeqIdType       m_SecSeqIdType;

    TAccession          m_Accession;
    TGI                 m_GI;
    TSecSeqState        m_SecSeqState;
    TSeqIdType          m_SeqIdType;
    TVersion            m_Version;
    TWritetime          m_Writetime;
};

using TSI2CSIConsumeCallback = function<void(vector<CSI2CSIRecord> &&)>;

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__SI2CSI__RECORD_HPP
