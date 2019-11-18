#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__REQUEST_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__REQUEST_HPP

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
 * Authors: Dmitrii Saprykin
 *
 * File Description:
 *
 *  PSG storage requests
 *
 */

#include <string>

#include <objtools/pubseq_gateway/impl/cassandra/psg_scope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>

BEGIN_PSG_SCOPE

class CBioseqInfoFetchRequest
{
    using TFields = uint8_t;
 public:
    enum class EFields : TFields {
        eAccession = 1,
        eVersion = 2,
        eSeqIdType = 4,
        eGI = 8,
    };

    CBioseqInfoFetchRequest() = default;
    CBioseqInfoFetchRequest(CBioseqInfoFetchRequest const&) = default;
    CBioseqInfoFetchRequest(CBioseqInfoFetchRequest &&) = default;
    CBioseqInfoFetchRequest& operator=(CBioseqInfoFetchRequest const&) = default;
    CBioseqInfoFetchRequest& operator=(CBioseqInfoFetchRequest &&) = default;

    CBioseqInfoFetchRequest& SetAccession(string const& value)
    {
        m_Accession = value;
        SetField(EFields::eAccession);
        return *this;
    }

    CBioseqInfoFetchRequest& SetVersion(CBioseqInfoRecord::TVersion value)
    {
        m_Version = value;
        SetField(EFields::eVersion);
        return *this;
    }

    CBioseqInfoFetchRequest& SetSeqIdType(CBioseqInfoRecord::TSeqIdType value)
    {
        m_SeqIdType = value;
        SetField(EFields::eSeqIdType);
        return *this;
    }

    CBioseqInfoFetchRequest& SetGI(CBioseqInfoRecord::TGI value)
    {
        m_GI = value;
        SetField(EFields::eGI);
        return *this;
    }

    string const& GetAccession() const
    {
        if (!HasField(EFields::eAccession)) {
            NCBI_USER_THROW("CBioseqInfoFetchRequest field Accession is missing");
        }
        return m_Accession;
    }

    CBioseqInfoRecord::TGI GetGI() const
    {
        if (!HasField(EFields::eGI)) {
            NCBI_USER_THROW("CBioseqInfoFetchRequest field GI is missing");
        }
        return m_GI;
    }

    CBioseqInfoRecord::TVersion GetVersion() const
    {
        if (!HasField(EFields::eVersion)) {
            NCBI_USER_THROW("CBioseqInfoFetchRequest field Version is missing");
        }
        return m_Version;
    }

    CBioseqInfoRecord::TSeqIdType GetSeqIdType() const
    {
        if (!HasField(EFields::eSeqIdType)) {
            NCBI_USER_THROW("CBioseqInfoFetchRequest field SeqIdType is missing");
        }
        return m_SeqIdType;
    }

    bool HasField(EFields field) const
    {
        return m_State & static_cast<TFields>(field);
    }

    CBioseqInfoFetchRequest& Reset()
    {
        m_Accession.clear();
        m_GI = 0;
        m_Version = 0;
        m_SeqIdType = 0;
        m_State = 0;
        return *this;
    }

 private:
    void SetField(EFields value)
    {
        m_State |= static_cast<TFields>(value);
    }

    string m_Accession;
    CBioseqInfoRecord::TGI m_GI = 0;
    CBioseqInfoRecord::TVersion m_Version = 0;
    CBioseqInfoRecord::TSeqIdType m_SeqIdType = 0;
    TFields m_State = 0;
};

END_PSG_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__REQUEST_HPP

