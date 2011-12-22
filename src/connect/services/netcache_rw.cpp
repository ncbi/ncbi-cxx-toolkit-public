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
 * Author: Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of net cache client.
 *
 */

#include <ncbi_pch.hpp>

#include "netcache_api_impl.hpp"

#include <connect/services/netcache_api_expt.hpp>
#include <connect/services/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_NetCache


BEGIN_NCBI_SCOPE

#define CACHE_XFER_BUFFER_SIZE 4096
#define MAX_PENDING_COUNT (1024 * 1024 * 1024)

static const string s_InputBlobCachePrefix = ".nc_cache_input.";
static const string s_OutputBlobCachePrefix = ".nc_cache_output.";

CNetCacheReader::CNetCacheReader(SNetCacheAPIImpl* impl,
        const CNetServer::SExecResult& exec_result,
        size_t* blob_size_ptr,
        CNetCacheAPI::ECachingMode caching_mode) :
    CNetServerReader(exec_result),
    m_CachingEnabled(caching_mode == CNetCacheAPI::eCaching_AppDefault ?
        impl->m_CacheInput : caching_mode == CNetCacheAPI::eCaching_Enable)
{
    if (blob_size_ptr != NULL) {
        *blob_size_ptr = CheckBlobSize(m_BlobBytesToRead);
    }

    if (m_CachingEnabled) {
        m_CacheFile.CreateTemporary(impl->m_TempDir, s_InputBlobCachePrefix);

        char buf[CACHE_XFER_BUFFER_SIZE];
        Uint8 bytes_to_read = m_BlobBytesToRead;

        while (bytes_to_read > 0) {
            size_t bytes_read = 0;
            SocketRead(buf, sizeof(buf) <= bytes_to_read ?
                sizeof(buf) : (size_t) bytes_to_read, &bytes_read);
            m_CacheFile.Write(buf, bytes_read);
            bytes_to_read -= bytes_read;
        }

        m_Connection = NULL;

        if (m_CacheFile.GetFilePos() != m_BlobBytesToRead) {
            NCBI_THROW(CNetCacheException, eBlobClipped,
                "Blob size is greater than the amount of data cached for it");
        }

        m_CacheFile.Flush();
        m_CacheFile.SetFilePos(0);
    }
}

CNetCacheReader::~CNetCacheReader()
{
    try {
        Close();
    } NCBI_CATCH_ALL_X(10, "CNetCacheReader::~CNetCacheReader()");
}

void CNetCacheReader::Close()
{
    if (m_CachingEnabled)
        m_CacheFile.Close();
    else
        CNetServerReader::Close();
}

ERW_Result CNetCacheReader::Read(void*   buf,
                                 size_t  count,
                                 size_t* bytes_read_ptr)
{
    if (!m_CachingEnabled)
        return CNetServerReader::Read(buf, count, bytes_read_ptr);

    NETSERVERREADER_PREREAD();

    if ((bytes_read = m_CacheFile.Read(buf, count)) == 0)
        ReportPrematureEOF();

    NETSERVERREADER_POSTREAD();
}

ERW_Result CNetCacheReader::PendingCount(size_t* count)
{
    if (m_CachingEnabled || m_BlobBytesToRead == 0) {
        *count = m_BlobBytesToRead < MAX_PENDING_COUNT ?
            (size_t) m_BlobBytesToRead : MAX_PENDING_COUNT;
        return eRW_Success;
    } else
        return CNetServerReader::PendingCount(count);
}

/////////////////////////////////////////////////
CNetCacheWriter::CNetCacheWriter(SNetCacheAPIImpl* impl,
        string* blob_id,
        unsigned time_to_live,
        ENetCacheResponseType response_type,
        CNetCacheAPI::ECachingMode caching_mode) :
    CNetServerWriter(response_type),
    m_NetCacheAPI(impl),
    m_BlobID(*blob_id),
    m_TimeToLive(time_to_live),
    m_CachingEnabled(caching_mode == CNetCacheAPI::eCaching_AppDefault ?
        impl->m_CacheOutput : caching_mode == CNetCacheAPI::eCaching_Enable)
{
    if (m_CachingEnabled)
        m_CacheFile.CreateTemporary(impl->m_TempDir, s_OutputBlobCachePrefix);

    if (!m_CachingEnabled || blob_id->empty()) {
        EstablishConnection();
        *blob_id = m_BlobID;
    }
}

CNetCacheWriter::~CNetCacheWriter()
{
    try {
        Close();
    } NCBI_CATCH_ALL_X(11, "Exception in ~CNetCacheWriter() [IGNORED]");
}

void CNetCacheWriter::Close()
{
    if (m_CachingEnabled) {
        m_CacheFile.Flush();

        bool blob_written = false;

        if (IsConnectionOpen()) {
            try {
                UploadCacheFile();
                blob_written = true;
            }
            catch (CNetServiceException&) {
            }
        }

        if (!blob_written) {
            EstablishConnection();

            UploadCacheFile();
        }
    }

    CNetServerWriter::Close();
}

ERW_Result CNetCacheWriter::Write(const void* buf,
                                  size_t      count,
                                  size_t*     bytes_written_ptr)
{
    if (m_CachingEnabled) {
        size_t bytes_written = m_CacheFile.Write(buf, count);
        if (bytes_written_ptr != NULL)
            *bytes_written_ptr = bytes_written;

        return eRW_Success;
    } else
        return CNetServerWriter::Write(buf, count, bytes_written_ptr);
}

ERW_Result CNetCacheWriter::Flush(void)
{
    if (!m_CachingEnabled)
        CNetServerWriter::Flush();

    return eRW_Success;
}

void CNetCacheWriter::EstablishConnection()
{
    ResetWriters();

    m_Connection = m_NetCacheAPI->InitiateWriteCmd(this);

    m_SocketReaderWriter.reset(
        new CSocketReaderWriter(&m_Connection->m_Socket));

    m_TransmissionWriter.reset(
        new CTransmissionWriter(m_SocketReaderWriter.get(),
            eNoOwnership, CTransmissionWriter::eSendEofPacket));
}

void CNetCacheWriter::UploadCacheFile()
{
    char buf[CACHE_XFER_BUFFER_SIZE];
    size_t bytes_read;
    size_t bytes_written;

    m_CacheFile.SetFilePos(0);
    while ((bytes_read = m_CacheFile.Read(buf, sizeof(buf))) > 0)
        Transmit(buf, bytes_read, &bytes_written);
}

END_NCBI_SCOPE
