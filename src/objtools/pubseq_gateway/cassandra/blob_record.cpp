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
 * File Description:
 *
 * Blob storage: blob record
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_util.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_exception.hpp>

#include <corelib/ncbistr.hpp>
#include <sstream>
#include <utility>
#include <string>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CBlobRecord::CBlobRecord()
    : CBlobRecord(0)
{
}

CBlobRecord::CBlobRecord(int32_t key)
    : m_Modified(0)
    , m_Flags(0)
    , m_Size(0)
    , m_SizeUnpacked(0)
    , m_DateAsn1(0)
    , m_HupDate(0)
    , m_SatKey(key)
    , m_NChunks(0)
    , m_Owner(0)
    , m_Class(0)
{
}

CBlobRecord& CBlobRecord::SetKey(TSatKey value)
{
    m_SatKey = value;
    return *this;
}

CBlobRecord& CBlobRecord::SetModified(TTimestamp value)
{
    m_Modified = value;
    return *this;
}

CBlobRecord& CBlobRecord::SetFlags(TBlobFlagBase value)
{
    m_Flags = value;
    return *this;
}

CBlobRecord& CBlobRecord::SetSize(TSize value)
{
    m_Size = value;
    return *this;
}

CBlobRecord& CBlobRecord::SetSizeUnpacked(TSize value)
{
    m_SizeUnpacked = value;
    return *this;
}

CBlobRecord& CBlobRecord::SetDateAsn1(TTimestamp value)
{
    m_DateAsn1 = value;
    return *this;
}

CBlobRecord& CBlobRecord::SetHupDate(TTimestamp value)
{
    m_HupDate = value;
    return *this;
}


CBlobRecord& CBlobRecord::SetDiv(string value)
{
    m_Div = value;
    return *this;
}

CBlobRecord& CBlobRecord::SetId2Info(string const & value)
{
    m_Id2Info = value;
    return *this;
}

CBlobRecord& CBlobRecord::SetId2Info(int16_t sat, int32_t info, int32_t chunks, int32_t version)
{
    if (sat > 0) {
        m_Id2Info = to_string(sat);
        string info_str = to_string(info);
        string chunks_str = to_string(chunks);
        string version_str;
        size_t reserve = m_Id2Info.size() + info_str.size() + chunks_str.size() + 2;
        if (version > 0) {
            version_str = to_string(version);
            reserve += version_str.size() + 1;
        }
        m_Id2Info.reserve(reserve);
        m_Id2Info.append(1, '.').append(info_str).append(1, '.').append(chunks_str);
        if (version > 0) {
            m_Id2Info.append(1, '.').append(version_str);
        }
    }
    return *this;
}

CBlobRecord& CBlobRecord::SetUserName(string value)
{
    m_UserName = value;
    return *this;
}

CBlobRecord& CBlobRecord::SetNChunks(int32_t value)
{
    m_NChunks = value;
    return *this;
}

CBlobRecord& CBlobRecord::ResetNChunks()
{
    m_NChunks = m_BlobChunks.size();
    return *this;
}

CBlobRecord& CBlobRecord::SetOwner(int32_t value)
{
    m_Owner = value;
    return *this;
}

CBlobRecord& CBlobRecord::SetClass(int16_t value)
{
    m_Class = value;
    return *this;
}

CBlobRecord& CBlobRecord::SetGzip(bool value)
{
    SetFlag(value, EBlobFlags::eGzip);
    return *this;
}

CBlobRecord& CBlobRecord::SetNot4Gbu(bool value)
{
    SetFlag(value, EBlobFlags::eNot4Gbu);
    return *this;
}

CBlobRecord& CBlobRecord::SetSuppress(bool value)
{
    SetFlag(value, EBlobFlags::eSuppress);
    return *this;
}

CBlobRecord& CBlobRecord::SetWithdrawn(bool value)
{
    SetFlag(value, EBlobFlags::eWithdrawn);
    return *this;
}

CBlobRecord& CBlobRecord::SetDead(bool value)
{
    SetFlag(value, EBlobFlags::eDead);
    return *this;
}

CBlobRecord& CBlobRecord::AppendBlobChunk(TBlobChunk&& chunk)
{
    if (!chunk.empty()) {
        m_BlobChunks.emplace_back(move(chunk));
    }
    return *this;
}

CBlobRecord& CBlobRecord::InsertBlobChunk(size_t index, TBlobChunk&& chunk)
{
    if (!chunk.empty()) {
        if (index >= m_BlobChunks.size()) {
            m_BlobChunks.resize(index + 1);
        }
        m_BlobChunks[index] = move(chunk);
    }
    return *this;
}

CBlobRecord& CBlobRecord::ClearBlobChunks()
{
    m_BlobChunks.clear();
    return *this;
}

bool CBlobRecord::NoData() const
{
    return m_Size == 0 || m_BlobChunks.empty();
}

void CBlobRecord::VerifyBlobSize() const
{
    if (static_cast<size_t>(m_NChunks) != m_BlobChunks.size()) {
        RAISE_DB_ERROR(eInconsistentData, string("BlobChunks count inconsistent with NChunks property. key: ") +
            NStr::NumericToString(m_SatKey) +
            ". BlobChunks count: " + NStr::NumericToString(m_BlobChunks.size()) +
            ". m_NChunks: " + NStr::NumericToString(m_NChunks)
        );
    }
    TSize s = 0;
    for (auto c : m_BlobChunks) {
        s += c.size();
    }
    if (s != m_Size) {
        RAISE_DB_ERROR(eInconsistentData, string("BlobChunks size verification failed. key: ") +
            NStr::NumericToString(m_SatKey) +
            ". Prop size: " + NStr::NumericToString(m_Size) +
            ". Chunks size: " + NStr::NumericToString(s)
        );
    }
}

bool CBlobRecord::IsDataEqual(CBlobRecord const & blob) const
{
    size_t this_chunk = 0, blob_chunk = 0;
    size_t this_offset = 0, blob_offset = 0;
    while(this_chunk < this->m_BlobChunks.size()) {
        if (blob_chunk >= blob.m_BlobChunks.size()) {
            return false;
        }
        if (
            this_offset < this->m_BlobChunks[this_chunk].size()
            && blob_offset < blob.m_BlobChunks[blob_chunk].size()
        ) {
            size_t compare_length = min(
                this->m_BlobChunks[this_chunk].size() - this_offset,
                blob.m_BlobChunks[blob_chunk].size() - blob_offset
            );
            if (memcmp(
                    this->m_BlobChunks[this_chunk].data() + this_offset,
                    blob.m_BlobChunks[blob_chunk].data() + blob_offset,
                    compare_length
                ) != 0
            ) {
                return false;
            }
            this_offset += compare_length;
            blob_offset += compare_length;
        } else {
            if (this_offset >= this->m_BlobChunks[this_chunk].size()) {
                this_offset = 0;
                ++this_chunk;
            }
            if (blob_offset >= blob.m_BlobChunks[blob_chunk].size()) {
                blob_offset = 0;
                ++blob_chunk;
            }
        }
    }
    return blob_chunk == blob.m_BlobChunks.size();
}


CBlobRecord& CBlobRecord::SetFlag(bool set_flag, EBlobFlags flag_value) {
    if (set_flag) {
        m_Flags |= static_cast<TBlobFlagBase>(flag_value);
    } else {
        m_Flags &= ~(static_cast<TBlobFlagBase>(flag_value));
    }
    return *this;
}

bool CBlobRecord::GetFlag(EBlobFlags flag_value) const {
    return m_Flags & static_cast<TBlobFlagBase>(flag_value);
}

TBlobFlagBase CBlobRecord::GetFlags() const
{
    return m_Flags;
}

CBlobRecord::TSatKey CBlobRecord::GetKey() const
{
    return m_SatKey;
}

CBlobRecord::TTimestamp CBlobRecord::GetModified() const
{
    return m_Modified;
}

CBlobRecord::TSize CBlobRecord::GetSize() const
{
    return m_Size;
}

CBlobRecord::TSize CBlobRecord::GetSizeUnpacked() const
{
    return m_SizeUnpacked;
}

int16_t CBlobRecord::GetClass() const
{
    return m_Class;
}

CBlobRecord::TTimestamp CBlobRecord::GetDateAsn1() const
{
    return m_DateAsn1;
}

CBlobRecord::TTimestamp CBlobRecord::GetHupDate() const
{
    return m_HupDate;
}

string CBlobRecord::GetDiv() const
{
    return m_Div;
}

string CBlobRecord::GetId2Info() const
{
    return m_Id2Info;
}

int32_t CBlobRecord::GetOwner() const
{
    return m_Owner;
}

string CBlobRecord::GetUserName() const
{
    return m_UserName;
}


bool CBlobRecord::IsConfidential() const
{
   if (m_HupDate == 0)
      return false;

   return (m_HupDate > CCurrentTime().GetTimeT());
}


const CBlobRecord::TBlobChunk& CBlobRecord::GetChunk(size_t index) const
{
    if (index >= m_BlobChunks.size()) {
        RAISE_DB_ERROR(eInconsistentData, string("BlobChunk index requested is too large. key: ") +
            NStr::NumericToString(m_SatKey) +
            ". NChunks: " + NStr::NumericToString(m_NChunks) +
            ". index requested: " + NStr::NumericToString(index)
        );
    }
    return m_BlobChunks[index];
}

string CBlobRecord::ToString() const
{
    stringstream s;
    s << "SatKey: " << m_SatKey << endl
      << "\tm_Modified: " << TimeTmsToString(m_Modified) << " (" << m_Modified << ")" << endl
      << "\tm_Size: " << m_Size << endl
      << "\tm_SizeUnpacked: " << m_SizeUnpacked << endl
      << "\tm_DateAsn1: " << TimeTmsToString(m_DateAsn1) << " (" << m_DateAsn1 << ")" << endl
      << "\tm_HupDate: " << TimeTmsToString(m_HupDate) << " (" << m_HupDate << ")" << endl
      << "\tm_NChunks: " << m_NChunks << endl
      << "\tm_Owner: " << m_Owner << endl
      << "\tm_Class: " << m_Class << endl
      << "\tm_UserName: " << m_UserName << endl
      << "\tm_Div: " << m_Div << endl
      << "\tm_Id2Info: " << m_Id2Info << endl
      << "\tm_Flags: " << m_Flags << endl
      << "\t\tGzip - " << GetFlag(EBlobFlags::eGzip) << endl
      << "\t\tSuppress - " << GetFlag(EBlobFlags::eSuppress) << endl
      << "\t\tWithdrawn - " << GetFlag(EBlobFlags::eWithdrawn) << endl
      << "\t\tNot4Gbu - " << GetFlag(EBlobFlags::eNot4Gbu) << endl
      << "\t\tDead - " << GetFlag(EBlobFlags::eDead) << endl;
    return s.str();
}

END_IDBLOB_SCOPE
