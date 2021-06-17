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
 * Author:  Dmitry Kazimirov
 *
 * File Description:
 *   Direct access to NetCache servers through NetStorage API (implementation).
 *
 */

#include <ncbi_pch.hpp>

#include "netstorage_direct_nc.hpp"
#include "netcache_api_impl.hpp"

#include <connect/services/error_codes.hpp>

#include <util/ncbi_url.hpp>

#include <corelib/request_ctx.hpp>

#define NCBI_USE_ERRCODE_X  NetStorage_RPC

BEGIN_NCBI_SCOPE

ERW_Result SNetStorage_NetCacheBlob::Read(void* buffer, size_t buf_size, size_t* bytes_read)
{
    try {
        m_IState.reader.reset(m_NetCacheAPI->GetPartReader(m_BlobKey, 0, 0, nullptr, nullptr));
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on reading " + m_BlobKey)

    EnterState(&m_IState);
    return m_IState.Read(buffer, buf_size, bytes_read);
}

ERW_Result SNetStorage_NetCacheBlob::SIState::Read(void* buffer, size_t buf_size, size_t* bytes_read)
{
    ERW_Result rw_res = eRW_Success;

    try {
        rw_res = reader->Read(buffer, buf_size, bytes_read);
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on reading " + reader->GetBlobID())

    if ((rw_res != eRW_Success) && (rw_res != eRW_Eof)) {
        NCBI_THROW_FMT(CNetStorageException, eIOError,
                "I/O error while reading NetCache BLOB " <<
                reader->GetBlobID() << ": " <<
                g_RW_ResultToString(rw_res));
    }

    return rw_res;
}

ERW_Result SNetStorage_NetCacheBlob::PendingCount(size_t* count)
{
    *count = 0;
    return eRW_Success;
}

ERW_Result SNetStorage_NetCacheBlob::SIState::PendingCount(size_t* count)
{
    return reader->PendingCount(count);
}

bool SNetStorage_NetCacheBlob::Eof()
{
    return false;
}

bool SNetStorage_NetCacheBlob::SIState::Eof()
{
    return reader->Eof();
}

void SNetStorage_NetCacheBlob::StartWriting()
{
    try {
        m_OState.writer.reset(m_NetCacheAPI.PutData(&m_BlobKey));
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on writing " + m_BlobKey)

    EnterState(&m_OState);
}

ERW_Result SNetStorage_NetCacheBlob::Write(const void* buf_pos, size_t buf_size,
        size_t* bytes_written)
{
    StartWriting();
    return m_OState.Write(buf_pos, buf_size, bytes_written);
}

ERW_Result SNetStorage_NetCacheBlob::SOState::Write(const void* buf_pos, size_t buf_size,
        size_t* bytes_written)
{
    try {
        return writer->Write(buf_pos, buf_size, bytes_written);
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on writing " + m_BlobKey)
    return eRW_Error; // Not reached
}

ERW_Result SNetStorage_NetCacheBlob::Flush()
{
    return eRW_Success;
}

ERW_Result SNetStorage_NetCacheBlob::SOState::Flush()
{
    return writer->Flush();
}

Uint8 SNetStorage_NetCacheBlob::GetSize()
{
    try {
        return m_NetCacheAPI.GetBlobSize(m_BlobKey);
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on accessing " + m_BlobKey)
    return 0; // Not reached
}

list<string> SNetStorage_NetCacheBlob::GetAttributeList() const
{
    NCBI_THROW_FMT(CNetStorageException, eNotSupported, m_BlobKey <<
            ": attribute retrieval is not implemented for NetCache blobs");
}

string SNetStorage_NetCacheBlob::GetAttribute(const string& /*attr_name*/) const
{
    NCBI_THROW_FMT(CNetStorageException, eNotSupported, m_BlobKey <<
            ": attribute retrieval is not implemented for NetCache blobs");
}

void SNetStorage_NetCacheBlob::SetAttribute(const string& /*attr_name*/,
        const string& /*attr_value*/)
{
    NCBI_THROW_FMT(CNetStorageException, eNotSupported, m_BlobKey <<
            ": attribute setting for NetCache blobs is not implemented");
}

CNetStorageObjectInfo SNetStorage_NetCacheBlob::GetInfo()
{
    try {
        try {
            CNetServerMultilineCmdOutput output(
                    m_NetCacheAPI.GetBlobInfo(m_BlobKey));

            CJsonNode blob_info = CJsonNode::NewObjectNode();
            string line, key, val;

            while (output.ReadLine(line))
                if (NStr::SplitInTwo(line, ": ",
                        key, val, NStr::fSplit_ByPattern))
                    blob_info.SetByKey(key, CJsonNode::GuessType(val));

            CJsonNode size_node(blob_info.GetByKeyOrNull("Size"));

            Uint8 blob_size = size_node && size_node.IsInteger() ?
                    (Uint8) size_node.AsInteger() :
                    m_NetCacheAPI.GetBlobSize(m_BlobKey);

            if (m_NetCacheAPI.HasBlob(m_BlobKey)) {
                return g_CreateNetStorageObjectInfo(m_BlobKey,
                        eNFL_NetCache, NULL, blob_size, blob_info);
            }
        }
        NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on accessing " + m_BlobKey)
    }
    catch (CNetStorageException& e) {
        if (e.GetErrCode() != CNetStorageException::eNotExists)
            throw;
    }

    return g_CreateNetStorageObjectInfo(m_BlobKey,
            eNFL_NotFound, NULL, 0, NULL);
}

void SNetStorage_NetCacheBlob::SetExpiration(const CTimeout& ttl)
{
    if (!ttl.IsFinite()) {
        NCBI_THROW_FMT(CNetStorageException, eNotSupported, m_BlobKey <<
            ": infinite ttl for NetCache blobs is not implemented");
    }

    try {
        m_NetCacheAPI.ProlongBlobLifetime(m_BlobKey, (unsigned)ttl.GetAsDouble());
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on setting ttl " + m_BlobKey)
}

string SNetStorage_NetCacheBlob::FileTrack_Path()
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, m_BlobKey <<
            ": not a FileTrack object");
}

string SNetStorage_NetCacheBlob::Relocate(TNetStorageFlags, TNetStorageProgressCb)
{
    NCBI_THROW_FMT(CNetStorageException, eNotSupported, m_BlobKey <<
            ": Relocate for NetCache blobs is not implemented");
}

bool SNetStorage_NetCacheBlob::Exists()
{
    try {
        return m_NetCacheAPI.HasBlob(m_BlobKey);
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on accessing " + m_BlobKey)
    return false; // Not reached
}

ENetStorageRemoveResult SNetStorage_NetCacheBlob::Remove()
{
    try {
        if (m_NetCacheAPI.HasBlob(m_BlobKey)) {
            m_NetCacheAPI.Remove(m_BlobKey);
            return eNSTRR_Removed;
        }
    }
    NETSTORAGE_CONVERT_NETCACHEEXCEPTION("on removing " + m_BlobKey)

    return eNSTRR_NotFound;
}

void SNetStorage_NetCacheBlob::SIState::Close()
{
    ExitState();
    reader.reset();
}

void SNetStorage_NetCacheBlob::SOState::Close()
{
    ExitState();
    writer->Close();
    writer.reset();
}

void SNetStorage_NetCacheBlob::Close()
{
}

void SNetStorage_NetCacheBlob::SIState::Abort()
{
    ExitState();
    reader.reset();
}

void SNetStorage_NetCacheBlob::SOState::Abort()
{
    ExitState();
    writer->Abort();
    writer.reset();
}

void SNetStorage_NetCacheBlob::Abort()
{
}

END_NCBI_SCOPE
