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

#include "srv_rw.hpp"

#include <connect/services/netcache_api_expt.hpp>
#include <connect/services/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_NetCache


BEGIN_NCBI_SCOPE

#define CACHE_XFER_BUFFER_SIZE 4096
#define MAX_PENDING_COUNT (1024 * 1024 * 1024)

CNetServerReader::CNetServerReader(const CNetServer::SExecResult& exec_result) :
    m_Connection(exec_result.conn)
{
    string::size_type pos = exec_result.response.find("SIZE=");

    if (pos == string::npos) {
        NCBI_THROW(CNetCacheException, eInvalidServerResponse,
            "No SIZE field in reply to the blob reading command");
    }

    m_BlobBytesToRead = NStr::StringToUInt8(
        exec_result.response.c_str() + pos + sizeof("SIZE=") - 1,
        NStr::fAllowTrailingSymbols);
}

CNetServerReader::~CNetServerReader()
{
    try {
        Close();
    } NCBI_CATCH_ALL_X(10, "CNetServerReader::~CNetServerReader()");
}

void CNetServerReader::Close()
{
    if (m_BlobBytesToRead != 0)
        m_Connection->Abort();
}

ERW_Result CNetServerReader::Read(void*   buf,
                                 size_t  count,
                                 size_t* bytes_read_ptr)
{
    NETSERVERREADER_PREREAD();

    SocketRead(buf, count, &bytes_read);

    NETSERVERREADER_POSTREAD();
}

ERW_Result CNetServerReader::PendingCount(size_t* count)
{
    return CSocketReaderWriter(&m_Connection->m_Socket).PendingCount(count);
}

void CNetServerReader::SocketRead(void* buf, size_t count, size_t* bytes_read)
{
    EIO_Status status = m_Connection->m_Socket.Read(buf, count, bytes_read);

    switch (status) {
    case eIO_Success:
        break;

    case eIO_Timeout:
        NCBI_THROW(CNetServiceException, eTimeout,
            "Timeout while reading blob contents");

    case eIO_Closed:
        if (count > *bytes_read)
            ReportPrematureEOF();
        break;

    default:
        NCBI_THROW_FMT(CNetServiceException, eCommunicationError,
            "Error while reading blob: " << IO_StatusStr(status));
    }
}

void CNetServerReader::ReportPrematureEOF()
{
    m_BlobBytesToRead = 0;
    NCBI_THROW_FMT(CNetCacheException, eBlobClipped,
        "Amount read is less than the expected blob size "
        "(premature EOF while reading from " <<
        m_Connection->m_Server->m_Address.AsString() << ")");
}

/////////////////////////////////////////////////
CNetServerWriter::CNetServerWriter(const CNetServer::SExecResult& exec_result) :
    m_Connection(exec_result.conn),
    m_ResponseType(eNetCache_Wait)
{
    m_SocketReaderWriter.reset(
        new CSocketReaderWriter(&m_Connection->m_Socket));

    m_TransmissionWriter.reset(
        new CTransmissionWriter(m_SocketReaderWriter.get(),
            eNoOwnership, CTransmissionWriter::eSendEofPacket));
}

CNetServerWriter::CNetServerWriter(ENetCacheResponseType response_type) :
    m_ResponseType(response_type)
{
}

CNetServerWriter::~CNetServerWriter()
{
    try {
        Close();
    } NCBI_CATCH_ALL_X(11, "Exception in ~CNetServerWriter() [IGNORED]");
}

void CNetServerWriter::Close()
{
    if (!IsConnectionOpen())
        return;

    ERW_Result res = m_TransmissionWriter->Close();

    if (res != eRW_Success) {
        AbortConnection();
        if (res == eRW_Timeout) {
            NCBI_THROW(CNetServiceException, eTimeout,
                "Timeout while sending EOF packet");
        } else {
            NCBI_THROW(CNetServiceException, eCommunicationError,
                "IO error while sending EOF packet");
        }
    }

    if (m_ResponseType == eNetCache_Wait) {
        try {
            string dummy;
            m_Connection->ReadCmdOutputLine(dummy);
        }
        catch (...) {
            AbortConnection();
            throw;
        }
    }

    ResetWriters();

    m_Connection = NULL;
}

ERW_Result CNetServerWriter::Write(const void* buf,
                                  size_t      count,
                                  size_t*     bytes_written_ptr)
{
    if (IsConnectionOpen())
        Transmit(buf, count, bytes_written_ptr);
    else
        return eRW_Error;

    return eRW_Success;
}

ERW_Result CNetServerWriter::Flush(void)
{
    if (IsConnectionOpen())
        m_TransmissionWriter->Flush();

    return eRW_Success;
}

void CNetServerWriter::WriteBufferAndClose(const char* buf_ptr, size_t buf_size)
{
    size_t bytes_written;

    while (buf_size > 0) {
        Write(buf_ptr, buf_size, &bytes_written);
        buf_ptr += bytes_written;
        buf_size -= bytes_written;
    }

    Close();
}

void CNetServerWriter::ResetWriters()
{
    try {
        m_TransmissionWriter.reset();
        m_SocketReaderWriter.reset();
    }
    catch (...) {
    }
}

void CNetServerWriter::AbortConnection()
{
    ResetWriters();

    if (m_Connection->m_Socket.GetStatus(eIO_Open) != eIO_Closed)
        m_Connection->Abort();

    m_Connection = NULL;
}

void CNetServerWriter::Transmit(const void* buf,
    size_t count, size_t* bytes_written)
{
    ERW_Result res = m_TransmissionWriter->Write(buf, count, bytes_written);

    try {
        if (res != eRW_Success /*&& res != eRW_NotImplemented*/) {
            NCBI_THROW(CNetServiceException, eCommunicationError,
                g_RW_ResultToString(res));
        }

        STimeout to = {0, 0};
        switch (m_Connection->m_Socket.Wait(eIO_Read, &to)) {
            case eIO_Success:
                {
                    string msg;
                    if (m_Connection->m_Socket.ReadLine(msg) != eIO_Closed) {
                        if (msg.empty())
                            break;
                        if (msg.find("ERR:") == 0) {
                            msg.erase(0, 4);
                            msg = NStr::ParseEscapes(msg);
                        }
                        NCBI_THROW(CNetCacheException, eServerError, msg);
                    } // else FALL THROUGH
                }

            case eIO_Closed:
                NCBI_THROW(CNetServiceException, eCommunicationError,
                    "Server closed communication channel (timeout?)");

            default:
                break;
        }
    }
    catch (...) {
        AbortConnection();
        throw;
    }
}

END_NCBI_SCOPE
