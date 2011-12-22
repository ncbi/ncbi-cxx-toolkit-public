#ifndef CONN___SRV_RW__HPP
#define CONN___SRV_RW__HPP

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

#include "netservice_api_impl.hpp"

#include <connect/ncbi_conn_reader_writer.hpp>

#include <util/transmissionrw.hpp>

#include <limits>

BEGIN_NCBI_SCOPE


/** @addtogroup NetCacheClient
 *
 * @{
 */

class CSocket;

class NCBI_XCONNECT_EXPORT CNetServerReader : public IReader
{
public:
    CNetServerReader(const CNetServer::SExecResult& exec_result);

    virtual ~CNetServerReader();

    virtual ERW_Result Read(void* buf, size_t count,
        size_t* bytes_read_ptr = 0);

    virtual ERW_Result PendingCount(size_t* count);

    virtual void Close();

protected:
    void SocketRead(void* buf, size_t count, size_t* bytes_read);
    void ReportPrematureEOF();

    CNetServerConnection m_Connection;
    Uint8 m_BlobBytesToRead; // Remaining number of bytes to be read
};

///////////////////////////////////////////////////////////////////////////////
//

enum ENetCacheResponseType {
    eNetCache_Wait,
    eICache_NoWait,
};

class NCBI_XCONNECT_EXPORT CNetServerWriter : public IEmbeddedStreamWriter
{
public:
    CNetServerWriter(const CNetServer::SExecResult& exec_result);

    virtual ~CNetServerWriter();

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);

    /// Flush pending data (if any) down to the output device.
    virtual ERW_Result Flush(void);

    virtual void Close();
    virtual void Abort();

    void WriteBufferAndClose(const char* buf_ptr, size_t buf_size);

    ENetCacheResponseType GetResponseType() const {return m_ResponseType;}

protected:
    CNetServerWriter(ENetCacheResponseType response_type);

    bool IsConnectionOpen() { return m_TransmissionWriter.get() != NULL; }
    void ResetWriters();
    void AbortConnection();
    void Transmit(const void* buf, size_t count, size_t* bytes_written);

    CNetServerConnection m_Connection;
    auto_ptr<CSocketReaderWriter> m_SocketReaderWriter;
    auto_ptr<CTransmissionWriter> m_TransmissionWriter;
    ENetCacheResponseType m_ResponseType;
};

/* @} */

#define NETSERVERREADER_PREREAD() \
    if (m_BlobBytesToRead == 0) { \
        if (bytes_read_ptr != NULL) \
            *bytes_read_ptr = 0; \
        return eRW_Eof; \
    } \
    if (m_BlobBytesToRead < count) \
        count = (size_t) m_BlobBytesToRead; \
    size_t bytes_read = 0; \
    if (count > 0) {

#define NETSERVERREADER_POSTREAD() \
        m_BlobBytesToRead -= bytes_read; \
    } \
    if (bytes_read_ptr != NULL) \
        *bytes_read_ptr = bytes_read; \
    return eRW_Success;


END_NCBI_SCOPE

#endif  /* CONN___SRV_RW__HPP */
