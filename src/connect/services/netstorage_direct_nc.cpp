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
#include "netstorageobjectinfo.hpp"

#include <connect/services/netstorage_admin.hpp>
#include <connect/services/error_codes.hpp>

#include <util/ncbi_url.hpp>

#include <corelib/request_ctx.hpp>

#define NCBI_USE_ERRCODE_X  NetStorage_RPC

BEGIN_NCBI_SCOPE

string SNetStorage_NetCacheBlob::GetLoc()
{
    return m_NetCacheKey.GetKey();
}

#define CONVERT_NETCACHEEXCEPTION(read_write, reading_writing) \
    catch (CNetCacheException& e) { \
        switch (e.GetErrCode()) { \
        case CNetCacheException::eAuthenticationError: \
        case CNetCacheException::eAccessDenied: \
            NCBI_RETHROW_FMT(e, CNetStorageException, eAuthError, \
                    "Authentication or authorization error " \
                    "while accessing NetCache blob " << \
                    m_NetCacheKey.GetKey()); \
        case CNetCacheException::eBlobNotFound: \
            NCBI_RETHROW_FMT(e, CNetStorageException, eNotExists, \
                    "NetCache blob " << m_NetCacheKey.GetKey() << \
                    " does not exist"); \
        case CNetCacheException::eKeyFormatError: \
        case CNetCacheException::eUnknownCommand: \
        case CNetCacheException::eNotImplemented: \
        case CNetCacheException::eInvalidServerResponse: \
            NCBI_RETHROW_FMT(e, CNetStorageException, eInvalidArg, \
                    "Cannot " read_write " NetCache blob " << \
                    m_NetCacheKey.GetKey()); \
        default: \
            NCBI_RETHROW_FMT(e, CNetStorageException, eServerError, \
                    "NetCache server error while " reading_writing " " << \
                    m_NetCacheKey.GetKey()); \
        } \
    }

#define CONVERT_NETSERVICEEXCEPTION(reading_writing) \
    catch (CNetServiceException& e) { \
        switch (e.GetErrCode()) { \
        case CNetServiceException::eTimeout: \
            NCBI_RETHROW_FMT(e, CNetStorageException, eTimeout, \
                    "Timeout while " reading_writing " NetCache blob " << \
                    m_NetCacheKey.GetKey()); \
        case CNetServiceException::eCommunicationError: \
            NCBI_RETHROW_FMT(e, CNetStorageException, eIOError, \
                    "I/O error while " reading_writing " NetCache blob " << \
                    m_NetCacheKey.GetKey()); \
        default: \
            NCBI_RETHROW_FMT(e, CNetStorageException, eServerError, \
                    "NetCache server error while " reading_writing " " << \
                    m_NetCacheKey.GetKey()); \
        } \
    }

size_t SNetStorage_NetCacheBlob::Read(void* buffer, size_t buf_size)
{
    if (m_State == eWriting) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Cannot read a NetCache blob while writing to it");
    }

    try {
        if (m_State == eReady) {
            try {
                m_NetCacheReader.reset(m_NetCacheAPI.GetReader(
                        m_NetCacheKey.GetKey(), &m_BlobSize));
            }
            CONVERT_NETCACHEEXCEPTION("read", "reading");
            m_State = eReading;
            m_BytesRead = 0;
        }

        size_t bytes_read;
        g_ReadFromNetCache(m_NetCacheReader.get(),
                reinterpret_cast<char*>(buffer), buf_size, &bytes_read);
        m_BytesRead += bytes_read;
        return bytes_read;
    }
    CONVERT_NETSERVICEEXCEPTION("reading");
}

void SNetStorage_NetCacheBlob::Read(string* data)
{
    m_NetCacheAPI.ReadData(m_NetCacheKey.GetKey(), *data);
}

bool SNetStorage_NetCacheBlob::Eof()
{
    switch (m_State) {
    case eReady:
        return false;

    case eReading:
        return m_BytesRead >= m_BlobSize;

    default: /* case eWriting: */
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Cannot read a NetCache blob while writing");
    }
}

void SNetStorage_NetCacheBlob::Write(const void* buf_pos, size_t buf_size)
{
    if (m_State == eReading) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Cannot write a NetCache blob while reading");
    }

    try {
        if (m_State == eReady) {
            try {
                string blob_key(m_NetCacheKey.GetKey());
                m_NetCacheWriter.reset(m_NetCacheAPI.PutData(&blob_key));
            }
            CONVERT_NETCACHEEXCEPTION("write", "writing");
            m_State = eWriting;
        }

        m_NetCacheWriter->Write(buf_pos, buf_size);
    }
    CONVERT_NETSERVICEEXCEPTION("writing");
}

Uint8 SNetStorage_NetCacheBlob::GetSize()
{
    return m_NetCacheAPI.GetBlobSize(m_NetCacheKey.GetKey());
}

string SNetStorage_NetCacheBlob::GetAttribute(const string& /*attr_name*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            m_NetCacheKey.GetKey() <<
            ": attribute retrieval is not implemented for NetCache blobs");
}

void SNetStorage_NetCacheBlob::SetAttribute(const string& /*attr_name*/,
        const string& /*attr_value*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            m_NetCacheKey.GetKey() <<
            ": attribute setting for NetCache blobs is not implemented");
}

CNetStorageObjectInfo SNetStorage_NetCacheBlob::GetInfo()
{
    try {
        CNetServerMultilineCmdOutput output(
                m_NetCacheAPI.GetBlobInfo(m_NetCacheKey.GetKey()));

        CJsonNode blob_info = CJsonNode::NewObjectNode();
        string line, key, val;

        while (output.ReadLine(line))
            if (NStr::SplitInTwo(line, ": ",
                    key, val, NStr::fSplit_ByPattern))
                blob_info.SetByKey(key, CJsonNode::GuessType(val));

        CJsonNode size_node(blob_info.GetByKeyOrNull("Size"));

        Uint8 blob_size = size_node && size_node.IsInteger() ?
                (Uint8) size_node.AsInteger() :
                m_NetCacheAPI.GetBlobSize(m_NetCacheKey.GetKey());

        return new SNetStorageObjectInfoImpl(m_NetCacheKey.GetKey(),
                eNFL_NetCache, NULL, blob_size, blob_info);
    }
    catch (CNetCacheException& e) {
        if (e.GetErrCode() != CNetCacheException::eBlobNotFound)
            throw;
        return new SNetStorageObjectInfoImpl(m_NetCacheKey.GetKey(),
                eNFL_NotFound, NULL, 0, NULL);
    }
}

void SNetStorage_NetCacheBlob::Close()
{
    switch (m_State) {
    case eReading:
        m_State = eReady;
        m_NetCacheReader.reset();
        break;

    case eWriting:
        m_State = eReady;
        m_NetCacheWriter->Close();
        m_NetCacheWriter.reset();
        break;

    default:
        break;
    }
}

CNetStorageObject g_CreateNetStorage_NetCacheBlob(
        CNetCacheAPI::TInstance nc_api, const CNetCacheKey& nc_key)
{
    return new SNetStorage_NetCacheBlob(nc_api, nc_key);
}

END_NCBI_SCOPE
