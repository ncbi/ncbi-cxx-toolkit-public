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

#include <memory>
#include <string>

#include "../IdCassScope.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CNAnnotRecord {
 public:
    using TTimestamp = int64_t;
    using TSatKey = int32_t;
    using TCoord = int32_t;

 public:
    CNAnnotRecord();

    CNAnnotRecord& operator=(CNAnnotRecord const &) = default;
    CNAnnotRecord& operator=(CNAnnotRecord&&) = default;

    CNAnnotRecord& SetAccession(string value);
    CNAnnotRecord& SetVersion(int16_t value);
    CNAnnotRecord& SetSeqIdType(int16_t value);
    CNAnnotRecord& SetSatKey(TSatKey value);
    CNAnnotRecord& SetModified(TTimestamp value);
    CNAnnotRecord& SetAnnotName(string value);
    CNAnnotRecord& SetStart(TCoord value);
    CNAnnotRecord& SetStop(TCoord value);

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

    string const & GetAnnotName() const
    {
        return m_AnnotName;
    }

    string ToString() const;

 private:
    string m_Accession;
    string m_AnnotName;
    string m_AnnotData;
    TTimestamp m_Modified;
    TSatKey m_SatKey;
    TCoord m_Start;
    TCoord m_Stop;
    int16_t m_Version;
    int16_t m_SeqIdType;
};

using TNAnnotConsumeCallback = function<bool(CNAnnotRecord &&, bool last)>;

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__NANNOT__RECORD_HPP
