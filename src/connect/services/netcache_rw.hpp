#ifndef CONN___NETCACHE_RW__HPP
#define CONN___NETCACHE_RW__HPP

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
 * Authors:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *   Net cache client API.
 *
 */

/// @file netcache_rw.hpp
/// NetCache client specs.
///

#include "netcache_api_impl.hpp"

#include <connect/ncbi_conn_reader_writer.hpp>

#include <util/transmissionrw.hpp>


BEGIN_NCBI_SCOPE


/** @addtogroup NetCacheClient
 *
 * @{
 */


class CSocket;

class NCBI_XCONNECT_EXPORT CNetCacheReader : public IReader
{
public:
    CNetCacheReader(SNetCacheAPIImpl* impl,
        const CNetServer::SExecResult& exec_result,
        size_t* blob_size_ptr,
        CNetCacheAPI::ECachingMode caching_mode);

    virtual ~CNetCacheReader();

    virtual ERW_Result Read(void* buf, size_t count,
        size_t* bytes_read_ptr = 0);

    virtual ERW_Result PendingCount(size_t* count);

    void Close();

private:
    void SocketRead(void* buf, size_t count, size_t* bytes_read);
    void ReportPrematureEOF();

    CNetServerConnection m_Connection;
    CFileIO m_CacheFile;
    size_t m_BlobBytesToRead; // Remaining number of bytes to be read
    bool m_CachingEnabled;
};

///////////////////////////////////////////////////////////////////////////////
//

class NCBI_XCONNECT_EXPORT CNetCacheWriter : public IEmbeddedStreamWriter
{
public:
    CNetCacheWriter(SNetCacheAPIImpl* impl,
        string* blob_id,
        unsigned time_to_live,
        ENetCacheResponseType response_type,
        CNetCacheAPI::ECachingMode caching_mode);

    virtual ~CNetCacheWriter();

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);

    /// Flush pending data (if any) down to the output device.
    virtual ERW_Result Flush(void);

    virtual void Close();

    void WriteBufferAndClose(const void* buf_ptr, size_t buf_size);

private:
    void ResetWriters();
    void EstablishConnection();
    void AbortConnection();
    void Transmit(const void* buf, size_t count, size_t* bytes_written);

    CNetCacheAPI m_NetCacheAPI;
    string m_BlobID;
    unsigned m_TimeToLive;
    CNetServerConnection m_Connection;
    auto_ptr<CSocketReaderWriter> m_SocketReaderWriter;
    auto_ptr<CTransmissionWriter> m_TransmissionWriter;
    ENetCacheResponseType m_ResponseType;
    CFileIO m_CacheFile;
    bool m_CachingEnabled;
    bool m_ConnectionIsOpen;
};

/* @} */


END_NCBI_SCOPE

#endif  /* CONN___NETCACHE_RW__HPP */
