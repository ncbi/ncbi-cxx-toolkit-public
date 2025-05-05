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
    eCheckFailed   = 1 << 0, // 1
    eGzip          = 1 << 1, // 2 - Blob chunks are compressed
    eNot4Gbu       = 1 << 2, // 4 - Sybase::not2gbu != 0
    eWithdrawn     = 1 << 3, // 8 - Sybase::withdrawn != 0
    eSuppress      = 1 << 4, // 16 - Sybase::SuppressPerm - suppress & 0x01
    eDead          = 1 << 5, // 32 - Sybase::ent_state != 100
    eBigBlobSchema = 1 << 6, // 64 - BlobChunks total size more than a limit configured for keyspace
    /* Begin - Flags transferred from Sybase storage - internal/ID/Common/idstates.h */
    eWithdrawnBase        = 1 << 7,  // 128 - Sybase::withdrawn == 1
    eWithdrawnPermanently = 1 << 8,  // 256 - Sybase::withdrawn == 2
    eReservedBlobFlag0    = 1 << 9,  // 512  - Reserved for Withdrawn extension
    eReservedBlobFlag1    = 1 << 10, // 1024 - Reserved for Withdrawn extension
    eSuppressEditBlocked  = 1 << 11, // 2048 - Sybase::SuppressNoEdit - suppress & 0x02
    eSuppressTemporary    = 1 << 12, // 4096 - Sybase::SuppressTemporary - suppress & 0x04
    eReservedBlobFlag2    = 1 << 13, // 8192 - Sybase::eSuppressWarnNoEdit - suppress & 0x08 (!!!Unused)
    eNoIncrementalProcessing = 1 << 14, // 16384 - Sybase::eSuppressSkipIncrement - suppress & 0x10
    eReservedBlobFlag3    = 1 << 15, // 32768 - Reserved for Suppress extension
    eReservedBlobFlag4    = 1 << 16, // 65536 - Reserved for Suppress extension
    /* End */
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

    CBlobRecord& SetGzip(bool value);
    CBlobRecord& SetNot4Gbu(bool value);
    CBlobRecord& SetSuppress(bool value);
    CBlobRecord& SetWithdrawn(bool value);
    CBlobRecord& SetDead(bool value);
    CBlobRecord& SetBigBlobSchema(bool value);
    CBlobRecord& SetFlag(bool set_flag, EBlobFlags flag_value);

    CBlobRecord& AppendBlobChunk(TBlobChunk&& chunk);
    CBlobRecord& InsertBlobChunk(size_t index, TBlobChunk&& chunk);
    CBlobRecord& ClearBlobChunks();

    //  ---------------Getters--------------------------

    TSatKey GetKey() const;
    TTimestamp GetModified() const;

    TBlobFlagBase GetFlags() const;
    bool GetFlag(EBlobFlags flag_value) const;
    /* Begin - ID based values for suppress/withdrawn fields - internal/ID/Common/ps_common.hpp#0296 */
    Uint1 GetIDSuppress() const;
    Int2 GetIDWithdrawn() const;
    /* End */
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
