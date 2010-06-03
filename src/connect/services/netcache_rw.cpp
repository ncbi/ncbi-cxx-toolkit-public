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

#include "netservice_api_impl.hpp"
#include "netcache_rw.hpp"

#include <connect/services/netcache_api_expt.hpp>
#include <connect/services/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_NetCache


BEGIN_NCBI_SCOPE

CNetCacheReader::CNetCacheReader(
        CNetServerConnection::TInstance connection, size_t blob_size) :
    m_Connection(connection),
    m_Reader(&m_Connection->m_Socket),
    m_BlobBytesToRead(blob_size)
{
}

CNetCacheReader::~CNetCacheReader()
{
    try {
        Close();
    } NCBI_CATCH_ALL_X(10, "CNetCacheReader::~CNetCacheReader()");
}

void CNetCacheReader::Close()
{
    if (m_BlobBytesToRead != 0)
        m_Connection->Abort();
}

ERW_Result CNetCacheReader::Read(void*   buf,
                                 size_t  count,
                                 size_t* bytes_read)
{
    if (m_BlobBytesToRead == 0) {
        if (bytes_read != NULL)
            *bytes_read = 0;
        return eRW_Eof;
    }

    if (m_BlobBytesToRead < count)
        count = m_BlobBytesToRead;

    size_t nn_read = 0;
    ERW_Result res = eRW_Eof;

    if (count > 0)
        if ((res = m_Reader.Read(buf, count, &nn_read)) == eRW_Eof) {
            NCBI_THROW_FMT(CNetCacheException, eBlobClipped,
                "Amount read is less than the expected blob size "
                "(premature EOF while reading from " <<
                m_Connection->m_Server->m_Address.AsString() << ")");
        }

    if (bytes_read != NULL)
        *bytes_read = nn_read;

    m_BlobBytesToRead -= nn_read;

    return res;
}

ERW_Result CNetCacheReader::PendingCount(size_t* count)
{
    if (m_BlobBytesToRead == 0) {
        *count = 0;
        return eRW_Success;
    }
    return m_Reader.PendingCount(count);
}


/////////////////////////////////////////////////
CNetCacheWriter::CNetCacheWriter(CNetServerConnection::TInstance connection,
    EServerResponseType response_type, string* error_message) :
        m_Connection(connection),
        m_SocketReaderWriter(&m_Connection->m_Socket),
        m_TransmissionWriter(&m_SocketReaderWriter,
            eNoOwnership, CTransmissionWriter::eSendEofPacket),
        m_ResponseType(response_type),
        m_IgnoredError(kEmptyStr)
{
    if (error_message != NULL)
        *(m_ErrorMessage = error_message) = kEmptyStr;
    else
        m_ErrorMessage = &m_IgnoredError;
}

CNetCacheWriter::~CNetCacheWriter()
{
    try {
        Close();
    } NCBI_CATCH_ALL_X(11, "Error while closing output stream");
    if (m_ErrorMessage == &m_IgnoredError) {
        ERR_POST("Error [IGNORED]: " << m_IgnoredError);
    }
}

void CNetCacheWriter::Close()
{
    if (!m_Connection || !m_ErrorMessage->empty())
        return;

    m_TransmissionWriter.Close();

    if (m_ResponseType == eNetCache_Wait) {
        try {
            string dummy;
            m_Connection->ReadCmdOutputLine(dummy);
        }
        catch (CException& e) {
            *m_ErrorMessage = e.GetMsg();
        }
        catch (exception& e) {
            *m_ErrorMessage = e.what();
        }
        if (!m_ErrorMessage->empty() &&
                m_Connection->m_Socket.GetStatus(eIO_Open) != eIO_Closed)
            m_Connection->Abort();
    }

    m_Connection = NULL;
}

ERW_Result CNetCacheWriter::Write(const void* buf,
                                  size_t      count,
                                  size_t*     bytes_written)
{
    ERW_Result res = m_TransmissionWriter.Write(buf, count, bytes_written);

    return res != eRW_Success || x_IsStreamOk() ? res : eRW_Error;
}

ERW_Result CNetCacheWriter::Flush(void)
{
    ERW_Result res = m_TransmissionWriter.Flush();

    return res != eRW_Success || x_IsStreamOk() ? res : eRW_Error;
}

bool CNetCacheWriter::x_IsStreamOk()
{
    STimeout to = {0, 0};
    EIO_Status io_st = m_Connection->m_Socket.Wait(eIO_Read, &to);
    string msg;
    switch (io_st) {
    case eIO_Success:
        {
            io_st = m_Connection->m_Socket.ReadLine(msg);
            if (io_st == eIO_Closed) {
                *m_ErrorMessage =
                    "Server closed communication channel (timeout?)";
            } else  if (!msg.empty()) {
                if (msg.find("ERR:") == 0) {
                    msg.erase(0, 4);
                    msg = NStr::ParseEscapes(msg);
                }
                *m_ErrorMessage = msg;
            }
        }
        break;
    case eIO_Closed:
        *m_ErrorMessage = "Server closed communication channel (timeout?)";
    default:
        break;
    }
    if (!m_ErrorMessage->empty()) {
        m_Connection->Abort();
        return false;
    }
    return true;
}

END_NCBI_SCOPE
