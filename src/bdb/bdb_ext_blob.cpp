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
#include <bdb/bdb_ext_blob.hpp>
#include <bdb/bdb_expt.hpp>


BEGIN_NCBI_SCOPE


CExtBlobLocDB::CExtBlobLocDB()
{
    DisableNull();

    BindKey("id_from", &id_from);
    BindKey("id_to",   &id_to);
}

EBDB_ErrCode 
CExtBlobLocDB::Insert(const CBDB_BlobMetaContainer& meta_container)
{
    EBDB_ErrCode ret;
    return ret;
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

    ssize += 4 // chunk map size
          + m_Loc.size() * sizeof(CBDB_ExtBlobMap::TBlobChunkVec::value_type);

    ssize += m_BlobMap.ComputeSerializationSize();
    return ssize;
}

void CBDB_BlobMetaContainer::Serialize(CBDB_RawFile::TBuffer* buf,
                                       Uint8                  buf_offset)
{
    _ASSERT(buf);

    size_t ser_size = ComputeSerializationSize();
    buf->resize(ser_size + (size_t)buf_offset);

    unsigned char* ptr = &((*buf)[0]);
    ptr += buf_offset;

    unsigned magic = 0;
    ::memcpy(ptr, &magic, sizeof(magic));
    ptr += sizeof(magic);

    unsigned sz = m_Loc.size();
    ::memcpy(ptr, &sz, sizeof(sz));
    ptr += sizeof(sz);

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
    ::memcpy(&sz, ptr, sizeof(sz));
    ptr += sizeof(sz);
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
    size_t ssize = 4; // Magic header

    ssize += 4; // map size

    for (size_t i = 0; i < m_BlobMap.size(); ++i) {
        const SBlobLoc& bl = m_BlobMap[i];
        ssize += sizeof(bl.blob_id);

        ssize += 4; // size of blob_location_table
        for (size_t j = 0; j < bl.blob_location_table.size(); ++j) {
            ssize += sizeof(SBlobChunkLoc);
        } // for j

    } // for i
    return ssize;
}

void CBDB_ExtBlobMap::Serialize(CBDB_RawFile::TBuffer* buf,
                                Uint8                  buf_offset)
{
    _ASSERT(buf);

    size_t ser_size = ComputeSerializationSize();
    buf->resize(ser_size + (size_t)buf_offset);

    unsigned char* ptr = &((*buf)[0]);
    ptr += buf_offset;

    unsigned magic = 0;
    ::memcpy(ptr, &magic, sizeof(magic));
    ptr += sizeof(magic);

    unsigned sz = m_BlobMap.size();
    ::memcpy(ptr, &sz, sizeof(sz));
    ptr += sizeof(sz);

    for (size_t i = 0; i < m_BlobMap.size(); ++i) {
        const SBlobLoc& bl = m_BlobMap[i];
        
        ::memcpy(ptr, &bl.blob_id, sizeof(bl.blob_id));
        ptr += sizeof(bl.blob_id);

        sz = bl.blob_location_table.size();
        ::memcpy(ptr, &sz, sizeof(sz));
        ptr += sizeof(sz);

        for (size_t j = 0; j < bl.blob_location_table.size(); ++j) {
            ::memcpy(ptr, &bl.blob_location_table[j].offset, sizeof(Uint8));
            ptr += sizeof(Uint8);
            ::memcpy(ptr, &bl.blob_location_table[j].size, sizeof(Uint8));
            ptr += sizeof(Uint8);            
        } // for j

    } // for i
}

void CBDB_ExtBlobMap::Deserialize(const CBDB_RawFile::TBuffer& buf, 
                                  Uint8                        buf_offset)
{
    const unsigned char* ptr = &buf[0];
    ptr += buf_offset;

    unsigned magic;
    ::memcpy(&magic, ptr, sizeof(magic));
    ptr += sizeof(magic);

    unsigned sz;
    ::memcpy(&sz, ptr, sizeof(sz));
    ptr += sizeof(sz);
    m_BlobMap.resize(sz);

    for (size_t i = 0; i < m_BlobMap.size(); ++i) {
        SBlobLoc& bl = m_BlobMap[i];

        ::memcpy(&bl.blob_id, ptr, sizeof(bl.blob_id));        
        ptr += sizeof(bl.blob_id);

        ::memcpy(&sz, ptr, sizeof(sz));
        ptr += sizeof(sz);
        bl.blob_location_table.resize(sz);

        for (size_t j = 0; j < bl.blob_location_table.size(); ++j) {
            ::memcpy(&bl.blob_location_table[j].offset, ptr, sizeof(Uint8));
            ptr += sizeof(Uint8);
            ::memcpy(&bl.blob_location_table[j].size, ptr, sizeof(Uint8));
            ptr += sizeof(Uint8);            
        } // for j

    } // for i
}


END_NCBI_SCOPE

