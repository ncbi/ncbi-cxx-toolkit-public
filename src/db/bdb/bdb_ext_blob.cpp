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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  BDB library external archival BLOB store.
 *
 */

#include <ncbi_pch.hpp>
#include <db/bdb/bdb_ext_blob.hpp>
#include <db/bdb/bdb_expt.hpp>


BEGIN_NCBI_SCOPE


/// @name Serialization masks
/// @{

static const unsigned s_ExtBlob_Mask_16bit = 1;
static const unsigned s_ExtBlob_Mask_32bit = (1 << 1);
static const unsigned s_ExtBlob_Mask_SingleChunk = (1 << 2);

///@}



CBlobMetaDB::CBlobMetaDB()
: CBDB_BLobFile()
{
    BindKey("id_from", &id_from);
    BindKey("id_to",   &id_to);
}

EBDB_ErrCode
CBlobMetaDB::UpdateInsert(const CBDB_BlobMetaContainer& meta_container)
{
    EBDB_ErrCode ret;

    const CBDB_ExtBlobMap& blob_map = meta_container.GetBlobMap();

    Uint4 min_id, max_id;
    blob_map.GetBlobIdRange(&min_id, &max_id);

    CBDB_RawFile::TBuffer buf;
    meta_container.Serialize(&buf);

    this->id_from = min_id;
    this->id_to = max_id;
    ret = TParent::UpdateInsert(buf);

    return ret;
}


EBDB_ErrCode
CBlobMetaDB::FetchMeta(Uint4                   blob_id,
                       CBDB_BlobMetaContainer* meta_container,
                       Uint4*                  id_from,
                       Uint4*                  id_to)
{
    _ASSERT(meta_container);

    CBDB_RawFile::TBuffer buf;
    buf.reserve(2048);

    // Scan backward
    {{
        CBDB_FileCursor cur(*this);
        cur.SetCondition(CBDB_FileCursor::eLE);
        cur.From << blob_id;

        while (cur.Fetch(&buf) == eBDB_Ok) {
            if (buf.size() == 0) {
                continue;
            }
            unsigned from = this->id_from;
            unsigned to   = this->id_to;

            if (from == 0 && to == 0)
                break;

            if (blob_id > to || blob_id < from)
                continue;

            meta_container->Deserialize(buf);
            const CBDB_ExtBlobMap& bmap = meta_container->GetBlobMap();
            if (bmap.HasBlob(blob_id)) {
                if (id_from)
                    *id_from = from;
                if (id_to)
                    *id_to = to;

                return eBDB_Ok;
            }
        }
    }} // cur

    return eBDB_NotFound;
}




CBDB_BlobMetaContainer::CBDB_BlobMetaContainer()
{}

void CBDB_BlobMetaContainer::SetLoc(Uint8 offset, Uint8 size)
{
    m_Loc.resize(1);
    m_Loc[0].offset = offset;
    m_Loc[0].size = size;
}

void CBDB_BlobMetaContainer::GetLoc(Uint8* offset, Uint8* size)
{
    if (m_Loc.size() == 0) {
        _ASSERT(0);
    }
    if (m_Loc.size() != 1) {
        string msg = "Not a single chunk BLOB";
        BDB_THROW(eTooManyChunks, msg);
    }
    if (offset) {
        *offset = m_Loc[0].offset;
    }
    if (size) {
        *size = m_Loc[0].size;
    }
}

size_t CBDB_BlobMetaContainer::ComputeSerializationSize() const
{
    size_t ssize = 4; // Magic header

    if (m_Loc.size() == 1) {
        ssize += sizeof(CBDB_ExtBlobMap::TBlobChunkVec::value_type);
    } else {
        ssize += 4; // chunk map size
        ssize += m_Loc.size() * sizeof(CBDB_ExtBlobMap::TBlobChunkVec::value_type);
    }
    ssize += m_BlobMap.ComputeSerializationSize();
    return ssize;
}

void CBDB_BlobMetaContainer::Serialize(CBDB_RawFile::TBuffer* buf,
                                       Uint8                  buf_offset) const
{
    _ASSERT(buf);

    size_t ser_size = ComputeSerializationSize();
    buf->resize(ser_size + (size_t)buf_offset);

    unsigned char* ptr = &((*buf)[0]);
    ptr += buf_offset;

    unsigned magic = 0;

    if (m_Loc.size() == 1) {
        magic |= s_ExtBlob_Mask_SingleChunk;
    }
    ::memcpy(ptr, &magic, sizeof(magic));
    ptr += sizeof(magic);

    unsigned sz = (unsigned)(m_Loc.size());
    _ASSERT(sz);
    if (sz != 1) {
        ::memcpy(ptr, &sz, sizeof(sz));
        ptr += sizeof(sz);
    }

    for (size_t i = 0; i < m_Loc.size(); ++i) {
        const CBDB_ExtBlobMap::SBlobChunkLoc& loc = m_Loc[i];
        ::memcpy(ptr, &loc.offset, sizeof(loc.offset));
        ptr += sizeof(loc.offset);
        ::memcpy(ptr, &loc.size, sizeof(loc.size));
        ptr += sizeof(loc.size);
    } // for

    size_t buffer_offset = ptr - &((*buf)[0]);
    m_BlobMap.Serialize(buf, buffer_offset);
}


void
CBDB_BlobMetaContainer::Deserialize(const CBDB_RawFile::TBuffer& buf,
                                    Uint8                        buf_offset)
{
    const unsigned char* ptr = &buf[0];
    ptr += buf_offset;

    unsigned magic;
    ::memcpy(&magic, ptr, sizeof(magic));
    ptr += sizeof(magic);

    unsigned sz;

    if (magic & s_ExtBlob_Mask_SingleChunk) {
        sz = 1;
    } else {
        ::memcpy(&sz, ptr, sizeof(sz));
        ptr += sizeof(sz);
    }
    m_Loc.resize(sz);

    for (size_t i = 0; i < m_Loc.size(); ++i) {
        CBDB_ExtBlobMap::SBlobChunkLoc& loc = m_Loc[i];
        ::memcpy(&loc.offset, ptr, sizeof(loc.offset));
        ptr += sizeof(loc.offset);
        ::memcpy(&loc.size, ptr, sizeof(loc.size));
        ptr += sizeof(loc.size);
    } // for


    size_t buffer_offset = ptr - &((buf)[0]);
    m_BlobMap.Deserialize(buf, buffer_offset);
}








CBDB_ExtBlobMap::CBDB_ExtBlobMap()
{
}

void CBDB_ExtBlobMap::Add(Uint4 blob_id, Uint8 offset, Uint8 size)
{
    if (HasBlob(blob_id)) {
        string msg = "BLOB id already exists:" + NStr::UIntToString(blob_id);
        BDB_THROW(eIdConflict, msg);
    }

    m_BlobMap.push_back(SBlobLoc(blob_id, offset, size));
}

bool CBDB_ExtBlobMap::HasBlob(Uint4 blob_id) const
{
    for (size_t i = 0; i < m_BlobMap.size(); ++i) {
        if (m_BlobMap[i].blob_id == blob_id) {
            return true;
        }
    }
    return false;
}

bool
CBDB_ExtBlobMap::GetBlobLoc(Uint4    blob_id,
                            Uint8*   offset,
                            Uint8*   size) const
{
    for (size_t i = 0; i < m_BlobMap.size(); ++i) {
        if (m_BlobMap[i].blob_id == blob_id) {
            const SBlobLoc& bl = m_BlobMap[i];
            if (bl.blob_location_table.size() == 0) {
                _ASSERT(0);
                continue;
            }
            if (bl.blob_location_table.size() != 1) {
                string msg = "Not a single chunk BLOB :" +
                                    NStr::UIntToString(blob_id);
                BDB_THROW(eTooManyChunks, msg);
            }

            if (offset) {
                *offset = bl.blob_location_table[0].offset;
            }
            if (size) {
                *size = bl.blob_location_table[0].size;
            }
            return true;
        }
    } // for
    return false;
}

void CBDB_ExtBlobMap::GetBlobIdRange(Uint4* min_id, Uint4* max_id) const
{
    Uint4 min_id_tmp = 0;
    Uint4 max_id_tmp = 0;

    for (size_t i = 0; i < m_BlobMap.size(); ++i) {
        if (i == 0) {
            min_id_tmp = m_BlobMap[i].blob_id;
            max_id_tmp = m_BlobMap[i].blob_id;
        } else {
            if (m_BlobMap[i].blob_id < min_id_tmp) {
                min_id_tmp = m_BlobMap[i].blob_id;
            }
            if (m_BlobMap[i].blob_id > max_id_tmp) {
                max_id_tmp = m_BlobMap[i].blob_id;
            }
        }
    }
    if (min_id)
        *min_id = min_id_tmp;
    if (max_id)
        *max_id = max_id_tmp;
}

size_t CBDB_ExtBlobMap::ComputeSerializationSize() const
{
    unsigned bits;
    bool is_single_chunk;
    return x_ComputeSerializationSize(&bits, &is_single_chunk);
}

size_t
CBDB_ExtBlobMap::x_ComputeSerializationSize(unsigned* bits_used,
                                            bool* is_single_chunk) const
{
    bool b16, b32;
    b16 = b32 = true;

    *is_single_chunk = true;

    size_t ssize = 4; // Magic header
    ssize += 4; // map size

    size_t ssize32 = ssize;
    size_t ssize16 = ssize;

    for (size_t i = 0; i < m_BlobMap.size(); ++i) {
        const SBlobLoc& bl = m_BlobMap[i];
        ssize += sizeof(bl.blob_id);
        ssize += 4; // size of blob_location_table

        ssize32 += sizeof(bl.blob_id) + 4;
        ssize16 += sizeof(bl.blob_id) + 4;

        if (bl.blob_location_table.size() != 1) {
            *is_single_chunk = false;
        }

        for (size_t j = 0; j < bl.blob_location_table.size(); ++j) {
            ssize += sizeof(SBlobChunkLoc);
            ssize16 += sizeof(unsigned short) + sizeof(unsigned short);
            ssize32 += sizeof(unsigned) + sizeof(unsigned);

            if (bl.blob_location_table[j].offset > kMax_UShort) {
                b16 = false;
            }
            if (bl.blob_location_table[j].offset > kMax_UInt) {
                b32 = false;
            }
            if (bl.blob_location_table[j].size > kMax_UShort) {
                b16 = false;
            }
            if (bl.blob_location_table[j].size > kMax_UInt) {
                b32 = false;
            }
        } // for j

    } // for i

    if (*is_single_chunk) {
        ssize   -= m_BlobMap.size() * 4;
        ssize16 -= m_BlobMap.size() * 4;
        ssize32 -= m_BlobMap.size() * 4;
    }

    if (b16) {
        *bits_used = 16;
        return ssize16;
    }
    if (b32) {
        *bits_used = 32;
        return ssize32;
    }
    *bits_used = 64;
    return ssize;
}

void CBDB_ExtBlobMap::Serialize(CBDB_RawFile::TBuffer* buf,
                                Uint8                  buf_offset) const
{
    _ASSERT(buf);

    unsigned bits_used;
    bool is_single_chunk;

    size_t ser_size =
        x_ComputeSerializationSize(&bits_used, &is_single_chunk);
    buf->resize(ser_size + (size_t)buf_offset);

    unsigned char* ptr = &((*buf)[0]);
    unsigned char* ptr0 = ptr;
    ptr += buf_offset;

    unsigned magic = 0;

    if (bits_used == 16) {
        magic |= s_ExtBlob_Mask_16bit;
    } else
    if (bits_used == 32) {
        magic |= s_ExtBlob_Mask_32bit;
    }

    if (is_single_chunk) {
        magic |= s_ExtBlob_Mask_SingleChunk;
    }

    ::memcpy(ptr, &magic, sizeof(magic));
    ptr += sizeof(magic);

    unsigned sz = (unsigned)(m_BlobMap.size());
    ::memcpy(ptr, &sz, sizeof(sz));
    ptr += sizeof(sz);

    for (size_t i = 0; i < m_BlobMap.size(); ++i) {
        const SBlobLoc& bl = m_BlobMap[i];

        ::memcpy(ptr, &bl.blob_id, sizeof(bl.blob_id));
        ptr += sizeof(bl.blob_id);

        if (!is_single_chunk) {
            sz = bl.blob_location_table.size();
            ::memcpy(ptr, &sz, sizeof(sz));
            ptr += sizeof(sz);
        }

        for (size_t j = 0; j < bl.blob_location_table.size(); ++j) {
            if (bits_used == 16) {
                unsigned short s;
                s = (unsigned short)bl.blob_location_table[j].offset;
                ::memcpy(ptr, &s, sizeof(s));
                ptr += sizeof(s);
                s = (unsigned short)bl.blob_location_table[j].size;
                ::memcpy(ptr, &s, sizeof(s));
                ptr += sizeof(s);
            } else
            if (bits_used == 32) {
                unsigned  s;
                s = (unsigned)bl.blob_location_table[j].offset;
                ::memcpy(ptr, &s, sizeof(s));
                ptr += sizeof(s);
                s = (unsigned)bl.blob_location_table[j].size;
                ::memcpy(ptr, &s, sizeof(s));
                ptr += sizeof(s);
            } else {
                ::memcpy(ptr,
                         &bl.blob_location_table[j].offset, sizeof(Uint8));
                ptr += sizeof(Uint8);
                ::memcpy(ptr,
                         &bl.blob_location_table[j].size, sizeof(Uint8));
                ptr += sizeof(Uint8);
            }
        } // for j

    } // for i

    buf->resize(ptr - ptr0);
}

void CBDB_ExtBlobMap::Deserialize(const CBDB_RawFile::TBuffer& buf,
                                  Uint8                        buf_offset)
{
    const unsigned char* ptr = &buf[0];
    ptr += buf_offset;

    unsigned magic;
    ::memcpy(&magic, ptr, sizeof(magic));
    ptr += sizeof(magic);

    unsigned bits_used = 64;
    if (magic & s_ExtBlob_Mask_16bit) {
        bits_used = 16;
    } else
    if (magic & s_ExtBlob_Mask_32bit) {
        bits_used = 32;
    }
    bool is_single_chunk = (magic & s_ExtBlob_Mask_SingleChunk) != 0;

    unsigned sz;
    ::memcpy(&sz, ptr, sizeof(sz));
    ptr += sizeof(sz);
    m_BlobMap.resize(sz);

    for (size_t i = 0; i < m_BlobMap.size(); ++i) {
        SBlobLoc& bl = m_BlobMap[i];

        ::memcpy(&bl.blob_id, ptr, sizeof(bl.blob_id));
        ptr += sizeof(bl.blob_id);

        if (is_single_chunk) {
            sz = 1;
        } else {
            ::memcpy(&sz, ptr, sizeof(sz));
            ptr += sizeof(sz);
        }
        bl.blob_location_table.resize(sz);

        for (size_t j = 0; j < bl.blob_location_table.size(); ++j) {
            if (bits_used == 16) {
                unsigned short s;
                ::memcpy(&s, ptr, sizeof(s));
                ptr += sizeof(s);
                bl.blob_location_table[j].offset = s;
                ::memcpy(&s, ptr, sizeof(s));
                ptr += sizeof(s);
                bl.blob_location_table[j].size = s;
            } else
            if (bits_used == 32) {
                unsigned s;
                ::memcpy(&s, ptr, sizeof(s));
                ptr += sizeof(s);
                bl.blob_location_table[j].offset = s;
                ::memcpy(&s, ptr, sizeof(s));
                ptr += sizeof(s);
                bl.blob_location_table[j].size = s;
            } else {
                ::memcpy(&bl.blob_location_table[j].offset,
                         ptr, sizeof(Uint8));
                ptr += sizeof(Uint8);
                ::memcpy(&bl.blob_location_table[j].size,
                         ptr, sizeof(Uint8));
                ptr += sizeof(Uint8);
            }
        } // for j

    } // for i
}


END_NCBI_SCOPE

