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
 * Authors:  Denis Vakatov
 *
 * File Description: NetStorage commands handler
 *
 */

#include <ncbi_pch.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "nst_handler.hpp"
#include "nst_server.hpp"


USING_NCBI_SCOPE;

const size_t    kReadBufferSize = 1024 * 1024 * 1024;
const size_t    kWriteBufferSize = 1024 * 1024 * 1024;



CNetStorageHandler::CNetStorageHandler(CNetStorageServer *  server)
    : m_Server(server),
      m_ReadBuffer(NULL),
      m_ReadMode(CNetStorageHandler::eReadMessages),
      m_WriteBuffer(NULL),
      m_JSONWriter(m_UTTPWriter)
{
    m_ReadBuffer = new char[ kReadBufferSize ];
    try {
        m_WriteBuffer = new char[ kWriteBufferSize ];
    } catch (...) {
        delete [] m_ReadBuffer;
        throw;
    }

    m_UTTPWriter.Reset(m_WriteBuffer, kWriteBufferSize);
}


CNetStorageHandler::~CNetStorageHandler()
{
    delete [] m_WriteBuffer;
    delete [] m_ReadBuffer;
}


EIO_Event
CNetStorageHandler::GetEventsToPollFor(const CTime** /*alarm_time*/) const
{
    if (m_OutputQueue.empty())
       return eIO_Read;
    return eIO_ReadWrite;
}


void CNetStorageHandler::OnOpen(void)
{
    CSocket &       socket = GetSocket();
    STimeout        to = {m_Server->GetNetworkTimeout(), 0};

    socket.DisableOSSendDelay();
    socket.SetTimeout(eIO_ReadWrite, &to);
    x_SetQuickAcknowledge();

    if (m_Server->IsLog())
        x_CreateConnContext();
}


void CNetStorageHandler::OnRead(void)
{
    size_t      n_read;
    EIO_Status  status = GetSocket().Read(m_ReadBuffer,
                                          kReadBufferSize, &n_read);

    switch (status) {
    case eIO_Success:
        break;
    case eIO_Timeout:
        this->OnTimeout();
        return;
    case eIO_Closed:
        this->OnClose(IServer_ConnectionHandler::eClientClose);
        return;
    default:
        // eIO_Interrupt, eIO_InvalidArg, eIO_NotSupported, eIO_Unknown
        // TODO: ??? OnError -- this TODO is a verbatim copy from server.cpp
        return;
    }

    m_UTTPReader.SetNewBuffer(m_ReadBuffer, n_read);

    if (m_ReadMode == eReadMessages ||
            x_ReadRawData(m_UTTPReader.GetNextEvent())) {
        CJsonOverUTTPReader::EParsingEvent ret_code;

        while ((ret_code = m_JSONReader.ProcessParsingEvents(m_UTTPReader)) ==
                CJsonOverUTTPReader::eEndOfMessage) {
            size_t  raw_data_size = 0;
            x_OnMessage(m_JSONReader.GetMessage(), &raw_data_size);
            m_JSONReader.Reset();
            if (raw_data_size > 0 &&
                    !x_ReadRawData(m_UTTPReader.ReadRawData(raw_data_size)))
                break;
        }

        if (ret_code != CJsonOverUTTPReader::eNextBuffer) {
            // Parsing error
            ERR_POST("Incoming message parsing error (error code " <<
                     int(ret_code) << "). The connection will be closed.");
            // TODO close the reading end
        }
    }
}


void CNetStorageHandler::OnWrite(void)
{
    for (;;) {
        CJsonNode message;

        {
            CFastMutexGuard guard(m_OutputQueueMutex);

            if (m_OutputQueue.empty())
                break;

            message = m_OutputQueue.front();
            m_OutputQueue.erase(m_OutputQueue.begin());
        }

        m_JSONWriter.SetOutputMessage(message);

        while (m_JSONWriter.ContinueWithReply())
            x_SendOutputBuffer();

        x_SendOutputBuffer();
    }
}


void CNetStorageHandler::OnClose(IServer_ConnectionHandler::EClosePeer peer)
{
    // m_ConnContext != NULL also tells that the logging is required
    if (m_ConnContext.IsNull())
        return;

    switch (peer)
    {
        case IServer_ConnectionHandler::eOurClose:
            if (m_CmdContext.NotNull()) {
                m_ConnContext->SetRequestStatus(
                            m_CmdContext->GetRequestStatus());
            } else {
                int status = m_ConnContext->GetRequestStatus();
                if (status != eStatus_HTTPProbe &&
                    status != eStatus_BadRequest &&
                    status != eStatus_SocketIOError)
                    m_ConnContext->SetRequestStatus(eStatus_Inactive);
            }
            break;
        case IServer_ConnectionHandler::eClientClose:
            if (m_CmdContext.NotNull()) {
                m_CmdContext->SetRequestStatus(eStatus_SocketIOError);
                m_ConnContext->SetRequestStatus(eStatus_SocketIOError);
            }
            break;
    }

    // If a command has not finished its logging by some reasons - do it
    // here as the last chance.
    if (m_CmdContext.NotNull()) {
        CDiagContext::SetRequestContext(m_CmdContext);
        GetDiagContext().PrintRequestStop();
        m_CmdContext.Reset();
    }

    CSocket&        socket = GetSocket();
    CDiagContext::SetRequestContext(m_ConnContext);
    m_ConnContext->SetBytesRd(socket.GetCount(eIO_Read));
    m_ConnContext->SetBytesWr(socket.GetCount(eIO_Write));
    GetDiagContext().PrintRequestStop();

    m_ConnContext.Reset();
    CDiagContext::SetRequestContext(NULL);
}


void CNetStorageHandler::OnTimeout(void)
{
    if (m_ConnContext.NotNull())
        m_ConnContext->SetRequestStatus(eStatus_Inactive);
}


void CNetStorageHandler::OnOverflow(EOverflowReason reason)
{
    switch (reason) {
        case eOR_ConnectionPoolFull:
            ERR_POST("eCommunicationError:Connection pool full");
            break;
        case eOR_UnpollableSocket:
            ERR_POST("eCommunicationError:Unpollable connection");
            break;
        case eOR_RequestQueueFull:
            ERR_POST("eCommunicationError:Request queue full");
            break;
        default:
            ERR_POST("eCommunicationError:Unknown overflow error");
    }
}


bool CNetStorageHandler::x_ReadRawData(
        CUTTPReader::EStreamParsingEvent  uttp_event)
{
    switch (uttp_event) {
    case CUTTPReader::eChunk:
        x_OnData(m_UTTPReader.GetChunkPart(), m_UTTPReader.GetChunkPartSize());
        m_ReadMode = eReadMessages;
        return true;

    case CUTTPReader::eChunkPart:
        x_OnData(m_UTTPReader.GetChunkPart(), m_UTTPReader.GetChunkPartSize());
        /* FALL THROUGH */

    default: /* case CUTTPReader::eEndOfBuffer: */
        m_ReadMode = eReadRawData;
        return false;
    }
}


void CNetStorageHandler::x_OnMessage(const CJsonNode &  message,
        size_t* raw_data_size)
{
    *raw_data_size = 0;
    x_SendMessage(message);
}


void CNetStorageHandler::x_OnData(
        const void* /*data*/, size_t /*data_size*/)
{
}


void CNetStorageHandler::x_SendMessage(const CJsonNode &  message)
{
    CJsonNode   output_message(message);

    output_message.PushString("Hi! I'll be your server today.");

    {
        CFastMutexGuard     guard(m_OutputQueueMutex);

        m_OutputQueue.push_back(output_message);
    }

    m_Server->WakeUpPollCycle();
}


void CNetStorageHandler::x_SendOutputBuffer()
{
    const char *    output_buffer;
    size_t          output_buffer_size;
    size_t          bytes_written;

    do {
        m_JSONWriter.GetOutputBuffer(&output_buffer, &output_buffer_size);
        for (;;) {
            if (GetSocket().Write(output_buffer, output_buffer_size,
                    &bytes_written) != eIO_Success) {
                NCBI_THROW(CIOException, eWrite,
                    "Error while writing to the socket");
            }
            if (bytes_written == output_buffer_size)
                break;
            output_buffer += bytes_written;
            output_buffer_size -= bytes_written;
        }
    } while (m_JSONWriter.NextOutputBuffer());
}


void CNetStorageHandler::x_SetQuickAcknowledge(void)
{
    int     fd = 0;
    int     val = 1;

    GetSocket().GetOSHandle(&fd, sizeof(fd));
    setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &val, sizeof(val));
}


void CNetStorageHandler::x_CreateConnContext(void)
{
    CSocket &       socket = GetSocket();

    m_ConnContext.Reset(new CRequestContext());
    m_ConnContext->SetRequestID();
    m_ConnContext->SetClientIP(socket.GetPeerAddress(eSAF_IP));

    // Set the connection request as the current one and print request start
    CDiagContext::SetRequestContext(m_ConnContext);
    GetDiagContext().PrintRequestStart()
                    .Print("_type", "conn");
    m_ConnContext->SetRequestStatus(eStatus_OK);
}


unsigned int CNetStorageHandler::x_GetPeerAddress(void)
{
    unsigned int        peer_addr;

    GetSocket().GetPeerAddress(&peer_addr, 0, eNH_NetworkByteOrder);

    // always use localhost(127.0*) address for clients coming from
    // the same net address (sometimes it can be 127.* or full address)
    if (peer_addr == m_Server->GetHostNetAddr())
        return CSocketAPI::GetLoopbackAddress();
    return peer_addr;
}

