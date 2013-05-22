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
#include <signal.h>

#include <connect/services/netstorage.hpp>
#include <misc/netstorage/netstorage.hpp>

#include "nst_handler.hpp"
#include "nst_server.hpp"
#include "nst_protocol_utils.hpp"
#include "nst_exception.hpp"
#include "nst_error_warning.hpp"
#include "nst_version.hpp"


USING_NCBI_SCOPE;

const size_t    kReadBufferSize = 1024 * 1024;
const size_t    kWriteBufferSize = 1024;


CNetStorageHandler::SProcessorMap   CNetStorageHandler::sm_Processors[] =
{
    { "BYE",            & CNetStorageHandler::x_ProcessBye },
    { "HELLO",          & CNetStorageHandler::x_ProcessHello },
    { "INFO",           & CNetStorageHandler::x_ProcessInfo },
    { "CONFIGURATION",  & CNetStorageHandler::x_ProcessConfiguration },
    { "SHUTDOWN",       & CNetStorageHandler::x_ProcessShutdown },
    { "GETCLIENTSINFO", & CNetStorageHandler::x_ProcessGetClientsInfo },
    { "GETOBJECTINFO",  & CNetStorageHandler::x_ProcessGetObjectInfo },
    { "GETATTR",        & CNetStorageHandler::x_ProcessGetAttr },
    { "SETATTR",        & CNetStorageHandler::x_ProcessSetAttr },
    { "READ",           & CNetStorageHandler::x_ProcessRead },
    { "WRITE",          & CNetStorageHandler::x_ProcessWrite },
    { "DELETE",         & CNetStorageHandler::x_ProcessDelete },
    { "RELOCATE",       & CNetStorageHandler::x_ProcessRelocate },
    { "EXISTS",         & CNetStorageHandler::x_ProcessExists },
    { "GETSIZE",        & CNetStorageHandler::x_ProcessGetSize },
    { "",               NULL }
};



CNetStorageHandler::CNetStorageHandler(CNetStorageServer *  server)
    : m_Server(server),
      m_ReadBuffer(NULL),
      m_ReadMode(CNetStorageHandler::eReadMessages),
      m_WriteBuffer(NULL),
      m_JSONWriter(m_UTTPWriter),
      m_DataMessageSN(-1),
      m_ByeReceived(false),
      m_FirstMessage(true)
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
    // This implementation is left to simplify the transition to
    // asynchronous replies. Currently the m_OutputQueue is always
    // empty so the only eIO_Read will be returned.
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
    case eIO_Interrupt:
        // Will be repeated again later?
        return;
    default:
        // eIO_InvalidArg, eIO_NotSupported, eIO_Unknown
        x_SetConnRequestStatus(eStatus_SocketIOError);
        m_Server->CloseConnection(&GetSocket());
        return;
    }

    m_UTTPReader.SetNewBuffer(m_ReadBuffer, n_read);

    if (m_ReadMode == eReadMessages || x_ReadRawData()) {
        try {
            while (m_JSONReader.ReadMessage(m_UTTPReader)) {

                if (!m_JSONReader.GetMessage().IsObject()) {
                    // All the incoming objects must be dictionaries
                    x_SetConnRequestStatus(eStatus_BadRequest);
                    m_Server->CloseConnection(&GetSocket());
                    return;
                }

                x_OnMessage(m_JSONReader.GetMessage());
                m_FirstMessage = false;
                m_JSONReader.Reset();
                if (m_ReadMode == eReadRawData && !x_ReadRawData())
                    break;
            }
        }
        catch (CJsonOverUTTPException& e) {
            // Parsing error
            if (!m_FirstMessage) {
                // The systems try to attack all the servers and they send
                // provocative messages. If the very first unparsable message
                // received then it does not make sense even to log it to avoid
                // excessive messages in AppLog.
                ERR_POST("Incoming message parsing error. " << e <<
                        " The connection will be closed.");
            }
            x_SetConnRequestStatus(eStatus_BadRequest);
            m_Server->CloseConnection(&GetSocket());
            return;
        }
    }
}


void CNetStorageHandler::OnWrite(void)
{
    // This implementation is left to simplify the transition to
    // asynchronous replies. Currently this method will never be called
    // because the GetEventsToPollFor(...) never returns amything but
    // eIO_Read

    for (;;) {
        CJsonNode message;

        {
            CFastMutexGuard guard(m_OutputQueueMutex);

            if (m_OutputQueue.empty())
                break;

            message = m_OutputQueue.front();
            m_OutputQueue.erase(m_OutputQueue.begin());
        }

        if (!m_JSONWriter.WriteMessage(message))
            do
                x_SendOutputBuffer();
            while (!m_JSONWriter.CompleteMessage());

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
                if (status != eStatus_BadRequest &&
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


bool CNetStorageHandler::x_ReadRawData()
{
    CUTTPReader::EStreamParsingEvent uttp_event;

    while ((uttp_event = m_UTTPReader.GetNextEvent()) ==
            CUTTPReader::eChunkPart)
        x_OnData(m_UTTPReader.GetChunkPart(),
                m_UTTPReader.GetChunkPartSize(), false);

    switch (uttp_event) {
    case CUTTPReader::eChunk:
        x_OnData(m_UTTPReader.GetChunkPart(),
                m_UTTPReader.GetChunkPartSize(), true);
        m_ReadMode = eReadMessages;
        return true;

    case CUTTPReader::eEndOfBuffer:
        return false;

    default:
        ERR_POST("Data stream parsing error. The connection will be closed.");
        x_SetConnRequestStatus(eStatus_BadRequest);
        m_Server->CloseConnection(&GetSocket());
        return false;
    }
}


// x_OnMessage gets control when a command message is received.
void CNetStorageHandler::x_OnMessage(const CJsonNode &  message)
{
    if (m_ConnContext.NotNull()) {
        m_CmdContext.Reset(new CRequestContext());
        m_CmdContext->SetRequestStatus(eStatus_OK);
        m_CmdContext->SetRequestID();
        CDiagContext::SetRequestContext(m_CmdContext);

        // The setting of the client session and IP
        // must be done before logging.
        SetSessionAndIP(message, GetSocket());
    }

    // Log all the message parameters
    x_PrintMessageRequestStart(message);

    // Extract message type and its serial number
    SCommonRequestArguments     common_args;
    try {
        common_args = ExtractCommonFields(message);
    }
    catch (const CNetStorageServerException &  ex) {
        ERR_POST("Error extracting mandatory fields: " << ex.what() << ". "
                 "The connection will be closed.");

        CJsonNode   response = CreateErrorResponseMessage(
                                        common_args.m_SerialNumber,
                                        eInvalidMandatoryHeader,
                                        "Error extracting mandatory fields");

        x_SetCmdRequestStatus(eStatus_BadRequest);
        if (x_SendSyncMessage(response) == eIO_Success)
            m_Server->CloseConnection(&GetSocket());
        return;
    }

    // Shutting down analysis is here because it needs the command serial
    // number to return it in the reply message.
    if (m_Server->ShutdownRequested()) {
        ERR_POST(Warning << "Server is shutting down. "
                 "The connection will be closed.");

        // Send response message with an error
        CJsonNode   response = CreateErrorResponseMessage(
                                        common_args.m_SerialNumber,
                                        eShuttingDown,
                                        "No messages are allowed after BYE");

        x_SetCmdRequestStatus(eStatus_ShuttingDown);
        if (x_SendSyncMessage(response) == eIO_Success)
            m_Server->CloseConnection(&GetSocket());
        return;
    }



    if (m_ByeReceived) {
        // BYE message has been received before. Send error responce
        // and close the connection.
        ERR_POST(Warning << "Received a message after BYE. "
                 "The connection will be closed.");

        // Send response message with an error
        CJsonNode   response = CreateErrorResponseMessage(
                                        common_args.m_SerialNumber,
                                        eMessageAfterBye,
                                        "No messages are allowed after BYE");

        x_SetCmdRequestStatus(eStatus_BadRequest);
        if (x_SendSyncMessage(response) == eIO_Success)
            m_Server->CloseConnection(&GetSocket());
        return;
    }


    bool          error = true;
    string        error_client_message;
    unsigned int  error_code;

    try {
        m_ClientRegistry.Touch(m_Client);

        // Find the processor
        FProcessor  processor = x_FindProcessor(common_args);

        // Call the processor. It returns the number of bytes to expect
        // after this message. If 0 then another message is expected.
        (this->*processor)(message, common_args);
        error = false;
    }
    catch (const CNetStorageServerException &  ex) {
        ERR_POST(ex);
        error_code = ex.ErrCodeToHTTPStatusCode();
        error_client_message = ex.what();
    }
    catch (const CNetStorageException &  ex) {
        ERR_POST(ex);
        error_code = eStatus_ServerError;
        error_client_message = ex.what();
    }
    catch (const std::exception &  ex) {
        ERR_POST("STL exception: " << ex.what());
        error_code = eStatus_ServerError;
        error_client_message = ex.what();
    }

    if (error) {
        x_SetCmdRequestStatus(error_code);

        // Send response message with an error
        CJsonNode   response = CreateErrorResponseMessage(
                                        common_args.m_SerialNumber,
                                        error_code,
                                        error_client_message);
        x_SendSyncMessage(response);
        x_PrintMessageRequestStop();
    }
}


// x_OnData gets control when raw data are received.
void CNetStorageHandler::x_OnData(
        const void* data, size_t data_size, bool last_chunk)
{
    if (!m_ObjectStream) {
        if (!last_chunk || data_size > 0) {
            ERR_POST("Received " << data_size << " bytes after "
                     "an error has been reported to the client");
        }

        if (last_chunk) {
            x_SetCmdRequestStatus(eStatus_ServerError);
            x_PrintMessageRequestStop();
        }
        return;
    }

    try {
        m_ObjectStream.Write(data, data_size);
        m_ClientRegistry.AddBytesWritten(m_Client, data_size);
    }
    catch (const std::exception &  ex) {
        string  message = "Error writing into " + m_ObjectStream.GetID() +
                          ": " + ex.what();
        ERR_POST(message);

        // Send response message with an error
        CJsonNode   response = CreateErrorResponseMessage(
                                        m_DataMessageSN,
                                        eWriteError, message);
        x_SendSyncMessage(response);
        m_ObjectStream = NULL;
        m_DataMessageSN = -1;
        return;
    }

    if (last_chunk) {
        x_SendSyncMessage(CreateResponseMessage(m_DataMessageSN));
        x_PrintMessageRequestStop();
        m_ObjectStream = NULL;
        m_DataMessageSN = -1;
    }
}


EIO_Status CNetStorageHandler::x_SendSyncMessage(const CJsonNode & message)
{
    if (!m_JSONWriter.WriteMessage(message))
        do {
            EIO_Status      status = x_SendOutputBuffer();
            if (status != eIO_Success)
                return status;
        } while (!m_JSONWriter.CompleteMessage());

    return x_SendOutputBuffer();
}



void CNetStorageHandler::x_SendAsyncMessage(const CJsonNode & par)
{
    // The current implementation does not use this.
    // The code is left here to simplify transition to the asynchronous
    // way of communicating.

    {
        CFastMutexGuard     guard(m_OutputQueueMutex);

        m_OutputQueue.push_back(par);
    }

    m_Server->WakeUpPollCycle();
}


EIO_Status CNetStorageHandler::x_SendOutputBuffer(void)
{
    const char *    output_buffer;
    size_t          output_buffer_size;
    size_t          bytes_written;

    do {
        m_JSONWriter.GetOutputBuffer(&output_buffer, &output_buffer_size);
        while (output_buffer_size > 0) {
            EIO_Status  status = GetSocket().Write(output_buffer,
                                                   output_buffer_size,
                                                   &bytes_written);
            if (status != eIO_Success) {
                // Error writing to the socket.
                // Log what we can.
                string  report = "Error writing message to the client. "
                                 "Peer: " +  GetSocket().GetPeerAddress() + ". "
                                 "Socket write error status: " + IO_StatusStr(status) + ". "
                                 "Written bytes: " + NStr::NumericToString(bytes_written) + ". "
                                 "Message begins with: ";

                if (output_buffer_size > 32) {
                    CTempString     buffer_head(output_buffer, 32);
                    report += NStr::PrintableString(buffer_head) + " (TRUNCATED)";
                }
                else {
                    CTempString     buffer_head(output_buffer, output_buffer_size);
                    report += NStr::PrintableString(buffer_head);
                }

                ERR_POST(report);

                if (m_ConnContext.NotNull()) {
                    if (m_ConnContext->GetRequestStatus() == eStatus_OK)
                        m_ConnContext->SetRequestStatus(eStatus_SocketIOError);
                    if (m_CmdContext.NotNull()) {
                        if (m_CmdContext->GetRequestStatus() == eStatus_OK)
                            m_CmdContext->SetRequestStatus(eStatus_SocketIOError);
                    }
                }

                // Register the socket error with the client
                m_ClientRegistry.RegisterSocketWriteError(m_Client);

                m_Server->CloseConnection(&GetSocket());
                return status;
            }

            output_buffer += bytes_written;
            output_buffer_size -= bytes_written;
        }
    } while (m_JSONWriter.NextOutputBuffer());

    x_SetQuickAcknowledge();
    return eIO_Success;
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



CNetStorageHandler::FProcessor
CNetStorageHandler::x_FindProcessor(
                        const SCommonRequestArguments &  common_args)
{
    for (size_t  index = 0; sm_Processors[index].m_Processor != NULL; ++index)
    {
        if (sm_Processors[index].m_MessageType == common_args.m_MessageType)
            return sm_Processors[index].m_Processor;
    }
    NCBI_THROW(CNetStorageServerException, eInvalidMessageType,
               "Message type '" + common_args.m_MessageType +
               "' is not supported");
}


void
CNetStorageHandler::x_PrintMessageRequestStart(const CJsonNode &  message)
{
    if (m_CmdContext.NotNull()) {
        CDiagContext::SetRequestContext(m_CmdContext);
        CDiagContext_Extra    ctxt_extra =
            GetDiagContext().PrintRequestStart()
                            .Print("_type", "message");

        for (CJsonIterator it = message.Iterate(); it; ++it) {
            ctxt_extra.Print(it.GetKey(),
                    it.GetNode().Repr(CJsonNode::fVerbatimIfString));
        }

        ctxt_extra.Flush();

        // Workaround:
        // When extra of the GetDiagContext().PrintRequestStart() is destroyed
        // or flushed it also resets the status to 0 so I need to set it here
        // to 200 though it was previously set to 200 when the request context
        // is created.
        m_CmdContext->SetRequestStatus(eStatus_OK);
    }
}


void
CNetStorageHandler::x_PrintMessageRequestStop(void)
{
    if (m_CmdContext.NotNull()) {
        CDiagContext::SetRequestContext(m_CmdContext);
        GetDiagContext().PrintRequestStop();
        CDiagContext::SetRequestContext(m_ConnContext);
        m_CmdContext.Reset();
    }
}


void
CNetStorageHandler::x_ProcessBye(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ByeReceived = true;
    x_SendSyncMessage(CreateResponseMessage(common_args.m_SerialNumber));
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessHello(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    string      application;    // Optional field
    string      ticket;         // Optional field

    if (!message.HasKey("Client")) {
        CJsonNode   response = CreateErrorResponseMessage(
                                        common_args.m_SerialNumber,
                                        eMandatoryFieldsMissed,
                                        "Mandatory field 'Client' is missed");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_SendSyncMessage(response);
        x_PrintMessageRequestStop();
        return;
    }

    string  client = NStr::TruncateSpaces(message.GetString("Client"));
    if (client.empty()) {
        CJsonNode   response = CreateErrorResponseMessage(
                                        common_args.m_SerialNumber,
                                        eInvalidArguments,
                                        "Mandatory field 'Client' is empty");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_SendSyncMessage(response);
        x_PrintMessageRequestStop();
        return;
    }

    m_Client = client;
    if (message.HasKey("Application"))
        application = message.GetString("Application");
    if (message.HasKey("Ticket"))
        ticket = message.GetString("Ticket");

    // Memorize the client in the registry
    m_ClientRegistry.Touch(m_Client, application, ticket, x_GetPeerAddress());

    // Send success response
    x_SendSyncMessage(CreateResponseMessage(common_args.m_SerialNumber));
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessInfo(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eAdministrator);

    CJsonNode   reply = CreateResponseMessage(common_args.m_SerialNumber);
    reply.SetString("ServerVersion", NETSTORAGED_VERSION);
    reply.SetString("ProtocolVersion", NETSTORAGED_PROTOCOL_VERSION);
    reply.SetInteger("PID", CDiagContext::GetPID());
    reply.SetString("BuildDate", NETSTORAGED_BUILD_DATE);
    reply.SetString("StartDate", NST_FormatPreciseTime(m_Server->GetStartTime()));
    reply.SetString("ServerSession", m_Server->GetSessionID());
    reply.SetString("ServerBinaryPath", CNcbiApplication::Instance()->GetProgramExecutablePath());
    reply.SetString("ServerCommandLine", m_Server->GetCommandLine());

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessConfiguration(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eAdministrator);

    CNcbiOstrstream             conf;
    CNcbiOstrstreamToString     converter(conf);

    CNcbiApplication::Instance()->GetConfig().Write(conf);

    CJsonNode   reply = CreateResponseMessage(common_args.m_SerialNumber);
    reply.SetString("Configuration", string(converter));
    reply.SetString("ConfigurationFilePath", CNcbiApplication::Instance()->GetConfigPath());

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessShutdown(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    if (m_Client.empty()) {
        CJsonNode   response = CreateErrorResponseMessage(
                                        common_args.m_SerialNumber,
                                        eHelloRequired,
                                        "Anonymous client cannot shutdown server");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_SendSyncMessage(response);
        x_PrintMessageRequestStop();
        return;
    }

    if (!m_Server->IsAdminClientName(m_Client)) {
        CJsonNode   response = CreateErrorResponseMessage(
                                        common_args.m_SerialNumber,
                                        ePrivileges,
                                        "Only administrators can shutdown server");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_SendSyncMessage(response);
        x_PrintMessageRequestStop();
        return;
    }

    if (!message.HasKey("Mode")) {
        CJsonNode   response = CreateErrorResponseMessage(
                                        common_args.m_SerialNumber,
                                        eMandatoryFieldsMissed,
                                        "Mandatory field 'Mode' is missed");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_SendSyncMessage(response);
        x_PrintMessageRequestStop();
        return;

    }

    string  mode;
    mode = message.GetString("Mode");
    if (mode != "hard" && mode != "soft") {
        CJsonNode   response = CreateErrorResponseMessage(
                                        common_args.m_SerialNumber,
                                        eInvalidArguments,
                                        "Allowed 'Mode' values are 'soft' and 'hard'");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_SendSyncMessage(response);
        x_PrintMessageRequestStop();
        return;
    }

    if (mode == "hard" )
        exit(1);

    m_Server->SetShutdownFlag(SIGTERM);

    // Send success response
    x_SendSyncMessage(CreateResponseMessage(common_args.m_SerialNumber));
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessGetClientsInfo(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eAdministrator);

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);

    reply.SetByKey("Clients", m_ClientRegistry.serialize());
    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessGetObjectInfo(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eReader);
}


void
CNetStorageHandler::x_ProcessGetAttr(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eReader);
}


void
CNetStorageHandler::x_ProcessSetAttr(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eWriter);
}


void
CNetStorageHandler::x_ProcessWrite(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eWriter);

    if (!x_CheckNonAnonymousClient(common_args))
        return;

    // Take the command arguments
    SStorageFlags       storage_flags = ExtractStorageFlags(message);
    SICacheSettings     icache_settings = ExtractICacheSettings(message);
    SUserKey            user_key = ExtractUserKey(message);

    // Check the parameters validity
    if (!x_CheckICacheSettings(icache_settings, common_args))
        return;
    if (!x_CheckUserKey(user_key, common_args))
        return;

    // Convert received flags into TNetStorageFlags
    TNetStorageFlags    flags = x_ConvertStorageFlags(storage_flags);

    // Create the object stream depending on settings
    m_ObjectStream = x_CreateObjectStream(icache_settings, user_key, flags);


    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);

    reply.SetString("FileID", m_ObjectStream.GetID());
    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();

    // Inform the message receiving loop that raw data are to follow
    m_ReadMode = eReadRawData;
    m_DataMessageSN = common_args.m_SerialNumber;
    m_ClientRegistry.AddObjectsWritten( m_Client, 1 );
}


void
CNetStorageHandler::x_ProcessRead(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eReader);

    if (!x_CheckNonAnonymousClient(common_args))
        return;

    // Take the arguments
    CNetFile    net_file;
    if (message.HasKey("FileID")) {
        string  file_id = message.GetString("FileID");
        if (!x_CheckFileID(file_id, common_args))
            return;

        net_file = g_CreateNetStorage(0).Open(file_id, 0);
    } else {
        SStorageFlags       storage_flags = ExtractStorageFlags(message);
        SICacheSettings     icache_settings = ExtractICacheSettings(message);
        SUserKey            user_key = ExtractUserKey(message);

        // Check the parameters validity
        if (!x_CheckICacheSettings(icache_settings, common_args))
            return;
        if (!x_CheckUserKey(user_key, common_args))
            return;

        // Convert received flags into TNetStorageFlags
        TNetStorageFlags    flags = x_ConvertStorageFlags(storage_flags);

        // Create the object stream depending on settings
        net_file = x_CreateObjectStream(icache_settings, user_key, flags);
    }

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);
    x_SendSyncMessage(reply);
 
    m_ClientRegistry.AddObjectsRead( m_Client, 1 );

    char buffer[kReadBufferSize];
    size_t bytes_read;

    try {
        while (!net_file.Eof()) {
            bytes_read = net_file.Read(buffer, sizeof(buffer));
            if (x_SendOverUTTP(buffer, bytes_read, false) != eIO_Success)
                return;
            m_ClientRegistry.AddBytesRead(m_Client, bytes_read);
        }

        net_file.Close();

        reply = CreateResponseMessage(common_args.m_SerialNumber);
    }
    catch (const CException &  ex) {
        reply = CreateErrorResponseMessage(common_args.m_SerialNumber,
                       eReadError, string("Object read error: ") + ex.what());
    }

    if (x_SendOverUTTP("", 0, true) != eIO_Success)
        return;

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessDelete(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eWriter);

    if (!x_CheckNonAnonymousClient(common_args))
        return;

    // Take the arguments
    if (message.HasKey("FileID")) {
        string  file_id = message.GetString("FileID");
        if (!x_CheckFileID(file_id, common_args))
            return;

        CNetStorage net_storage(g_CreateNetStorage(0));
        net_storage.Remove(file_id);
    } else {
        SStorageFlags       storage_flags = ExtractStorageFlags(message);
        SICacheSettings     icache_settings = ExtractICacheSettings(message);
        SUserKey            user_key = ExtractUserKey(message);

        // Check the parameters validity
        if (!x_CheckICacheSettings(icache_settings, common_args))
            return;
        if (!x_CheckUserKey(user_key, common_args))
            return;

        // Convert received flags into TNetStorageFlags
        TNetStorageFlags    flags = x_ConvertStorageFlags(storage_flags);

        // Create the object stream depending on settings
        x_CreateNetStorageByKey(icache_settings, user_key).Remove(
                user_key.m_UniqueID, flags);
    }

    m_ClientRegistry.AddObjectsDeleted(m_Client, 1);

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessRelocate(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eWriter);

    if (!x_CheckNonAnonymousClient(common_args))
        return;

    // Take the arguments
    if (!message.HasKey("NewLocation")) {
        CJsonNode   response = CreateErrorResponseMessage(
                                        common_args.m_SerialNumber,
                                        eInvalidArguments,
                                        "NewLocation argument is not found");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_SendSyncMessage(response);
        x_PrintMessageRequestStop();
        return;
    }

    CJsonNode           new_location = message.GetByKey("NewLocation");
    SStorageFlags       new_location_flags = ExtractStorageFlags(new_location);
    TNetStorageFlags    new_loc_flags = x_ConvertStorageFlags(new_location_flags);
    SICacheSettings new_location_icache_settings = ExtractICacheSettings(new_location);

    if (!x_CheckICacheSettings(new_location_icache_settings, common_args))
        return;


    if (message.HasKey("FileID")) {
        string  file_id = message.GetString("FileID");
        if (!x_CheckFileID(file_id, common_args))
            return;

        CNetStorage net_storage(g_CreateNetStorage(0));
        net_storage.Relocate(file_id, new_loc_flags);
    } else {
        SStorageFlags       storage_flags = ExtractStorageFlags(message);
        SICacheSettings     icache_settings = ExtractICacheSettings(message);
        SUserKey            user_key = ExtractUserKey(message);

        // Check the parameters validity
        if (!x_CheckICacheSettings(icache_settings, common_args))
            return;
        if (!x_CheckUserKey(user_key, common_args))
            return;

        // Convert received flags into TNetStorageFlags
        TNetStorageFlags    flags = x_ConvertStorageFlags(storage_flags);

        // Create the object stream depending on settings
        x_CreateNetStorageByKey(icache_settings, user_key).Relocate(
                user_key.m_UniqueID, new_loc_flags, flags);
    }

    m_ClientRegistry.AddObjectsRelocated(m_Client, 1 );

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessExists(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eReader);

    if (!x_CheckNonAnonymousClient(common_args))
        return;

    bool        exists = false;

    // Take the argument
    if (message.HasKey("FileID")) {
        string  file_id = message.GetString("FileID");
        if (!x_CheckFileID(file_id, common_args))
            return;

        CNetStorage net_storage(g_CreateNetStorage(0));
        exists = net_storage.Exists(file_id);
    } else {
        SStorageFlags       storage_flags = ExtractStorageFlags(message);
        SICacheSettings     icache_settings = ExtractICacheSettings(message);
        SUserKey            user_key = ExtractUserKey(message);

        // Check the parameters validity
        if (!x_CheckICacheSettings(icache_settings, common_args))
            return;
        if (!x_CheckUserKey(user_key, common_args))
            return;

        // Convert received flags into TNetStorageFlags
        TNetStorageFlags    flags = x_ConvertStorageFlags(storage_flags);

        // Create the object stream depending on settings
        exists = x_CreateNetStorageByKey(icache_settings, user_key).Exists(
                                                    user_key.m_UniqueID, flags);
    }

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);

    reply.SetBoolean("Exists", exists);
    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessGetSize(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eReader);

    if (!x_CheckNonAnonymousClient(common_args))
        return;

    // Take the arguments
    CNetFile    net_file;
    if (message.HasKey("FileID")) {
        string  file_id = message.GetString("FileID");
        if (!x_CheckFileID(file_id, common_args))
            return;

        net_file = g_CreateNetStorage(0).Open(file_id, 0);
    } else {
        SStorageFlags       storage_flags = ExtractStorageFlags(message);
        SICacheSettings     icache_settings = ExtractICacheSettings(message);
        SUserKey            user_key = ExtractUserKey(message);

        // Check the parameters validity
        if (!x_CheckICacheSettings(icache_settings, common_args))
            return;
        if (!x_CheckUserKey(user_key, common_args))
            return;

        // Convert received flags into TNetStorageFlags
        TNetStorageFlags    flags = x_ConvertStorageFlags(storage_flags);

        // Create the object stream depending on settings
        net_file = x_CreateObjectStream(icache_settings, user_key, flags);
    }

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);

    reply.SetInteger("Size", net_file.GetSize());
    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


TNetStorageFlags
CNetStorageHandler::x_ConvertStorageFlags(const SStorageFlags &  storage_flags)
{
    TNetStorageFlags    flags = 0;
    if (storage_flags.m_Fast)
        flags |= fNST_Fast;
    if (storage_flags.m_Persistent)
        flags |= fNST_Persistent;
    if (storage_flags.m_Movable)
        flags |= fNST_Movable;
    if (storage_flags.m_Cacheable)
        flags |= fNST_Cacheable;
    return flags;
}

bool
CNetStorageHandler::x_CheckNonAnonymousClient(
                            const SCommonRequestArguments &  common_args)
{
    if (m_Client.empty()) {
        CJsonNode   response = CreateErrorResponseMessage(
                                        common_args.m_SerialNumber,
                                        eHelloRequired,
                                        "Anonymous client cannot perform "
                                        "I/O operations with objects");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_SendSyncMessage(response);
        x_PrintMessageRequestStop();
        return false;
    }
    return true;
}


bool
CNetStorageHandler::x_CheckFileID(const string &                   file_id,
                                  const SCommonRequestArguments &  common_args)
{
    if (file_id.empty()) {
        CJsonNode   response = CreateErrorResponseMessage(
                                        common_args.m_SerialNumber,
                                        eInvalidArguments,
                                        "FileID must not be an empty string");
        x_SetCmdRequestStatus(eStatus_BadRequest);
        x_SendSyncMessage(response);
        x_PrintMessageRequestStop();
        return false;
    }
    return true;
}


bool
CNetStorageHandler::x_CheckICacheSettings(
                            const SICacheSettings &          icache_settings,
                            const SCommonRequestArguments &  common_args)
{
    if (!icache_settings.m_ServiceName.empty()) {
        // CacheName is mandatory in this case
        if (icache_settings.m_CacheName.empty()) {
            CJsonNode   response = CreateErrorResponseMessage(
                                            common_args.m_SerialNumber,
                                            eInvalidArguments,
                                            "CacheName is required if "
                                            "ServiceName is provided");
            x_SetCmdRequestStatus(eStatus_BadRequest);
            x_SendSyncMessage(response);
            x_PrintMessageRequestStop();
            return false;
        }
    }
    return true;
}


bool
CNetStorageHandler::x_CheckUserKey(
                            const SUserKey &                 user_key,
                            const SCommonRequestArguments &  common_args)
{
    if (!user_key.m_UniqueID.empty()) {
        // AppDomain is mandatory in this case
        if (user_key.m_AppDomain.empty()) {
            CJsonNode   response = CreateErrorResponseMessage(
                                            common_args.m_SerialNumber,
                                            eInvalidArguments,
                                            "AppDomain is required if "
                                            "UniqueKey is provided");
            x_SetCmdRequestStatus(eStatus_BadRequest);
            x_SendSyncMessage(response);
            x_PrintMessageRequestStop();
            return false;
        }
    }
    return true;
}


CNetFile CNetStorageHandler::x_CreateObjectStream(
                    const SICacheSettings &  icache_settings,
                    const SUserKey &         user_key,
                    TNetStorageFlags         flags)
{
    if (user_key.m_UniqueID.empty()) {
        CNetStorage     net_storage;
        if (icache_settings.m_ServiceName.empty())
            net_storage = g_CreateNetStorage(0);
        else {
            CNetICacheClient icache_client(icache_settings.m_ServiceName,
                    icache_settings.m_CacheName, m_Client);
            net_storage = g_CreateNetStorage(icache_client, 0);
        }
        return net_storage.Create(flags);
    }

    return x_CreateNetStorageByKey(
                icache_settings, user_key).Open(user_key.m_UniqueID, flags);
}


CNetStorageByKey
CNetStorageHandler::x_CreateNetStorageByKey(
                          const SICacheSettings &  icache_settings,
                          const SUserKey &         user_key)
{
    CNetStorageByKey    net_storage_by_key;
    if (icache_settings.m_ServiceName.empty())
        return g_CreateNetStorageByKey(user_key.m_AppDomain, 0);

    CNetICacheClient icache_client(icache_settings.m_ServiceName,
            icache_settings.m_CacheName, m_Client);
    return g_CreateNetStorageByKey(icache_client, user_key.m_AppDomain, 0);
}

EIO_Status
CNetStorageHandler::x_SendOverUTTP(const char* buffer,
        size_t buffer_size, bool last_chunk)
{
    const char* output_buffer;
    size_t      output_buffer_size;
    size_t      written;

    m_UTTPWriter.SendChunk(buffer, buffer_size, !last_chunk);

    do {
        m_UTTPWriter.GetOutputBuffer(&output_buffer, &output_buffer_size);
        if (output_buffer_size > 0) {
            // Write to the socket as a single transaction
            EIO_Status result =
                GetSocket().Write(output_buffer, output_buffer_size, &written);
            if (result != eIO_Success) {
                ERR_POST("Error writing to the client socket. The connection will be closed.");
                x_SetConnRequestStatus(eStatus_SocketIOError);
                m_Server->CloseConnection(&GetSocket());
                return result;
            }
        }
    } while (m_UTTPWriter.NextOutputBuffer());

    return eIO_Success;
}
