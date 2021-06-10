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
 *  Blob storage: blob record
 *
 *****************************************************************************/

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__BLOB_RECORD_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__BLOB_RECORD_HPP

#include <corelib/ncbistd.hpp>
#include "IdCassScope.hpp"

#include <string>
#include <vector>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

using TBlobFlagBase = int64_t;
enum class EBlobFlags : TBlobFlagBase {
    eCheckFailed = 1,
    eGzip        = 2,
    eNot4Gbu     = 4,
    eWithdrawn   = 8,
    eSuppress    = 16,
    eDead        = 32,
};

class CBlobRecord {
 public:
    using TSatKey = int32_t;
    using TSize = int64_t;
    using TTimestamp = int64_t;
    using TBlobChunk = vector<unsigned char>;

 public:
    CBlobRecord();
    explicit CBlobRecord(TSatKey key);
    CBlobRecord(CBlobRecord const &) = default;
    CBlobRecord(CBlobRecord &&) = default;

    CBlobRecord& operator=(CBlobRecord const &) = default;
    CBlobRecord& operator=(CBlobRecord&&) = default;

    CBlobRecord& SetKey(TSatKey value);

    CBlobRecord& SetModified(TTimestamp value);
    CBlobRecord& SetFlags(TBlobFlagBase value);
    CBlobRecord& SetSize(TSize value);
    CBlobRecord& SetSizeUnpacked(TSize value);

    CBlobRecord& SetDateAsn1(TTimestamp value);
    CBlobRecord& SetHupDate(TTimestamp value);

    CBlobRecord& SetDiv(string value);
    CBlobRecord& SetId2Info(string const & value);
    CBlobRecord& SetId2Info(int16_t sat, int32_t info, int32_t chunks, int32_t version = 0);
    CBlobRecord& SetUserName(string value);

    CBlobRecord& SetNChunks(int32_t value);
    CBlobRecord& ResetNChunks();
    CBlobRecord& SetOwner(int32_t value);
    CBlobRecord& SetClass(int16_t value);

    //  @warning Flags for extended schema are not compatible with old one.
    //  DO NOT use these methods to work with old schema data
    CBlobRecord& SetGzip(bool value);
    CBlobRecord& SetNot4Gbu(bool value);
    CBlobRecord& SetSuppress(bool value);
    CBlobRecord& SetWithdrawn(bool value);
    CBlobRecord& SetDead(bool value);

    CBlobRecord& AppendBlobChunk(TBlobChunk&& chunk);
    CBlobRecord& InsertBlobChunk(size_t index, TBlobChunk&& chunk);
    CBlobRecord& ClearBlobChunks();

    //  ---------------Getters--------------------------

    TSatKey GetKey() const;
    TTimestamp GetModified() const;

    TBlobFlagBase GetFlags() const;
    bool GetFlag(EBlobFlags flag_value) const;
    TSize GetSize() const;
    TSize GetSizeUnpacked() const;

    int16_t GetClass() const;
    TTimestamp GetDateAsn1() const;
    TTimestamp GetHupDate() const;
    string GetDiv() const;
    string GetId2Info() const;
    int32_t GetOwner() const;
    string GetUserName() const;

    int32_t GetNChunks() const
    {
        return m_NChunks;
    }
    const TBlobChunk& GetChunk(size_t index) const;

    bool NoData() const;

    //  ---------------Utils--------------------------

    void VerifyBlobSize() const;
    bool IsDataEqual(CBlobRecord const & blob) const;

    string ToString() const;
    // Records are confidential if HUP date is in the future
    bool IsConfidential() const;

 private:
    CBlobRecord& SetFlag(bool set_flag, EBlobFlags flag_value);

    TTimestamp  m_Modified;
    int64_t     m_Flags;
    TSize       m_Size;
    TSize       m_SizeUnpacked;
    TTimestamp  m_DateAsn1;
    TTimestamp  m_HupDate;

    string      m_Div;
    string      m_Id2Info;
    string      m_UserName;

    TSatKey     m_SatKey;
    int32_t     m_NChunks;
    int32_t     m_Owner;

    int16_t     m_Class;

    vector<TBlobChunk> m_BlobChunks;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__BLOB_RECORD_HPP
