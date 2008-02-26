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
 * Author: Maxim Didenko
 *
 * File Description:
 *   Implementation of net cache client.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/services/netcache_rw.hpp>
#include <connect/services/netcache_api_expt.hpp>
#include <connect/services/netservice_api.hpp>
#include <connect/services/srv_connections.hpp>
#include <connect/services/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_NetCache


BEGIN_NCBI_SCOPE

CNetCacheReader::CNetCacheReader(
    CNetServerConnection connection, size_t blob_size)
    : m_Connection(connection), m_BlobBytesToRead(blob_size)
{
    m_Reader.reset(new CSocketReaderWriter(m_Connection.GetSocket()));
}

CNetCacheReader::~CNetCacheReader()
{
    try {
        Close();
    } NCBI_CATCH_ALL_X(10, "CNetCacheReader::~CNetCacheReader()");
}

void CNetCacheReader::Close()
{
    m_Reader.reset();
    if (m_BlobBytesToRead != 0)
        m_Connection.Abort();
}

ERW_Result CNetCacheReader::Read(void*   buf,
                                 size_t  count,
                                 size_t* bytes_read)
{
    if (!m_Reader.get())
        return eRW_Error;

    ERW_Result res = eRW_Eof;
    if ( m_BlobBytesToRead == 0) {
        if ( bytes_read ) {
            *bytes_read = 0;
        }
        return res;
    }
    if ( m_BlobBytesToRead < count ) {
        count = m_BlobBytesToRead;
    }
    size_t nn_read = 0;
    if ( count ) {
        res = m_Reader->Read(buf, count, &nn_read);
    }

    if ( bytes_read ) {
        *bytes_read = nn_read;
    }
    m_BlobBytesToRead -= nn_read;
    //if (m_BlobBytesToRead == 0) {
    //    FinishTransmission();
    //}
    return res;
}

ERW_Result CNetCacheReader::PendingCount(size_t* count)
{
    if (!m_Reader.get())
        return eRW_Error;

    if ( m_BlobBytesToRead == 0) {
        *count = 0;
        return eRW_Success;
    }
    return m_Reader->PendingCount(count);
}


/////////////////////////////////////////////////
CNetCacheWriter::CNetCacheWriter(const CNetCacheAPI& api,
    CNetServerConnection connection,
    CTransmissionWriter::ESendEofPacket send_eof)
    : m_API(api), m_Connection(connection)
{
    m_Writer.reset(new CTransmissionWriter(
        new CSocketReaderWriter(m_Connection.GetSocket()),
        eTakeOwnership, send_eof));
}

CNetCacheWriter::~CNetCacheWriter()
{
    try {
        Close();
    } NCBI_CATCH_ALL_X(11, "CNetCacheWriter::~CNetCacheWriter()");
}

void CNetCacheWriter::Close()
{
    if (!m_LastError.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError, m_LastError);
    }

    if (!m_Writer.get())
        return;

    try {
        m_Writer.reset();
        m_API.WaitResponse(m_Connection);
    } catch (...) {
        m_Connection.Abort();
        x_Shutdown();
        throw;
    }

    x_Shutdown();
}

ERW_Result CNetCacheWriter::Write(const void* buf,
                                  size_t      count,
                                  size_t*     bytes_written)
{
    if (!m_Writer.get())
        return eRW_Error;

    ERW_Result res = m_Writer->Write(buf, count, bytes_written);
    if (res == eRW_Success) {
        if ( !x_IsStreamOk() )
            return eRW_Error;
    }
    return res;
}

ERW_Result CNetCacheWriter::Flush(void)
{
    if (!m_Writer.get())
        return eRW_Error;

    ERW_Result res = m_Writer->Flush();
    if (res == eRW_Success) {
        if ( !x_IsStreamOk() )
            return eRW_Error;
    }
    return res;
}

bool CNetCacheWriter::x_IsStreamOk()
{
    STimeout to = {0, 0};
    EIO_Status io_st = m_Connection.GetSocket()->Wait(eIO_Read, &to);
    string msg;
    switch (io_st) {
    case eIO_Success:
        {
            io_st = m_Connection.GetSocket()->ReadLine(msg);
            if (io_st == eIO_Closed) {
                m_LastError = "Server closed communication channel (timeout?)";
            } else  if (!msg.empty()) {
                CNetServiceAPI_Base::TrimErr(msg);
                m_LastError = msg;
            } else {
            }
        }
        break;
    case eIO_Closed:
        m_LastError = "Server closed communication channel (timeout?)";
    default:
        break;
    }
    if (!m_LastError.empty()) {
        m_Connection.Abort();
        x_Shutdown();
        return false;
    }
    return true;
}

void CNetCacheWriter::x_Shutdown()
{
    m_Writer.reset();
}

END_NCBI_SCOPE
