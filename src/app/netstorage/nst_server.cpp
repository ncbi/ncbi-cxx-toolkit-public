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
 * Authors:
 *   Dmitry Kazimirov
 *
 * File Description:
 *   The CServer part of netstoraged - implementation.
 *
 */

#include <ncbi_pch.hpp>

#include "nst_server.hpp"

#define READ_BUFFER_SIZE 6
#define WRITE_BUFFER_SIZE 24

USING_NCBI_SCOPE;

CNetStorageServer* CNetStorageServer::GetInstance()
{
    static CNetStorageServer server;

    return &server;
}

bool CNetStorageServer::ShutdownRequested()
{
    return CServer::ShutdownRequested();
}

void CNetStorageServer::SetShutdownFlag(int /*signum*/)
{
}

void CNetStorageServer::SetNSTParameters(SNSTServerParameters& /*params*/,
        bool /*bool_param*/)
{
}

CNetStorageConnectionHandler::CNetStorageConnectionHandler(
        CNetStorageServer* server) :
    m_Server(server),
    m_ReadBuffer(new char[READ_BUFFER_SIZE]),
    m_ReadMode(CNetStorageConnectionHandler::eReadMessages),
    m_WriteBuffer(new char[WRITE_BUFFER_SIZE]),
    m_JSONWriter(m_UTTPWriter)
{
    m_UTTPWriter.Reset(m_WriteBuffer, WRITE_BUFFER_SIZE);
}

CNetStorageConnectionHandler::~CNetStorageConnectionHandler()
{
    delete[] m_WriteBuffer;
    delete[] m_ReadBuffer;
}

EIO_Event CNetStorageConnectionHandler::GetEventsToPollFor(
        const CTime** /*alarm_time*/) const
{
    return m_OutputQueue.empty() ? eIO_Read : eIO_ReadWrite;
}

void CNetStorageConnectionHandler::OnOpen()
{
}

void CNetStorageConnectionHandler::OnRead()
{
    size_t n_read;

    EIO_Status status = GetSocket().Read(m_ReadBuffer,
            READ_BUFFER_SIZE, &n_read);

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
        // TODO: ??? OnError -- this TODO is a verbatim copy from server.cpp
        return;
    }

    m_UTTPReader.SetNewBuffer(m_ReadBuffer, n_read);

    if (m_ReadMode == eReadMessages ||
            ReadRawData(m_UTTPReader.GetNextEvent())) {
        CJsonOverUTTPReader::EParsingEvent ret_code;

        while ((ret_code = m_JSONReader.ProcessParsingEvents(m_UTTPReader)) ==
                CJsonOverUTTPReader::eEndOfMessage) {
            size_t raw_data_size = 0;
            OnMessage(m_JSONReader.GetMessage(), &raw_data_size);
            m_JSONReader.Reset();
            if (raw_data_size > 0 &&
                    !ReadRawData(m_UTTPReader.ReadRawData(raw_data_size)))
                break;
        }

        if (ret_code != CJsonOverUTTPReader::eNextBuffer) {
            // Parsing error
            printf("Parsing error %d\n", int(ret_code));
            // TODO close the reading end
        }
    }
}

void CNetStorageConnectionHandler::OnWrite()
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
            SendOutputBuffer();

        SendOutputBuffer();
    }
}

bool CNetStorageConnectionHandler::ReadRawData(
        CUTTPReader::EStreamParsingEvent uttp_event)
{
    switch (uttp_event) {
    case CUTTPReader::eChunk:
        OnData(m_UTTPReader.GetChunkPart(), m_UTTPReader.GetChunkPartSize());
        m_ReadMode = eReadMessages;
        return true;

    case CUTTPReader::eChunkPart:
        OnData(m_UTTPReader.GetChunkPart(), m_UTTPReader.GetChunkPartSize());
        /* FALL THROUGH */

    default: /* case CUTTPReader::eEndOfBuffer: */
        m_ReadMode = eReadRawData;
        return false;
    }
}

void CNetStorageConnectionHandler::OnMessage(const CJsonNode& message,
        size_t* raw_data_size)
{
    *raw_data_size = 0;
    SendMessage(message);
}

void CNetStorageConnectionHandler::OnData(
        const void* /*data*/, size_t /*data_size*/)
{
}

void CNetStorageConnectionHandler::SendMessage(const CJsonNode& message)
{
    CJsonNode output_message(message);

    output_message.PushString("Hi! I'll be your server today.");

    {
        CFastMutexGuard guard(m_OutputQueueMutex);

        m_OutputQueue.push_back(output_message);
    }

    m_Server->WakeUpPollCycle();
}

void CNetStorageConnectionHandler::SendOutputBuffer()
{
    const char* output_buffer;
    size_t output_buffer_size;
    size_t bytes_written;

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

CNetStorageConnectionFactory::CNetStorageConnectionFactory(
        CNetStorageServer* server) :
    m_Server(server)
{
}

IServer_ConnectionHandler* CNetStorageConnectionFactory::Create()
{
    return new CNetStorageConnectionHandler(m_Server);
}
