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
#include <connect/services/netstorage_impl.hpp>
#include <misc/netstorage/netstorage.hpp>

#include "nst_handler.hpp"
#include "nst_server.hpp"
#include "nst_protocol_utils.hpp"
#include "nst_exception.hpp"
#include "nst_version.hpp"
#include "nst_application.hpp"
#include "nst_warning.hpp"
#include "nst_config.hpp"


USING_NCBI_SCOPE;

const size_t    kReadBufferSize = 1024 * 1024;
const size_t    kWriteBufferSize = 1024;


CNetStorageHandler::SProcessorMap   CNetStorageHandler::sm_Processors[] =
{
    { "BYE",            & CNetStorageHandler::x_ProcessBye },
    { "HELLO",          & CNetStorageHandler::x_ProcessHello },
    { "INFO",           & CNetStorageHandler::x_ProcessInfo },
    { "CONFIGURATION",  & CNetStorageHandler::x_ProcessConfiguration },
    { "HEALTH",         & CNetStorageHandler::x_ProcessHealth },
    { "ACKALERT",       & CNetStorageHandler::x_ProcessAckAlert },
    { "RECONFIGURE",    & CNetStorageHandler::x_ProcessReconfigure },
    { "SHUTDOWN",       & CNetStorageHandler::x_ProcessShutdown },
    { "GETCLIENTSINFO", & CNetStorageHandler::x_ProcessGetClientsInfo },
    { "GETMETADATAINFO",& CNetStorageHandler::x_ProcessGetMetadataInfo },
    { "GETOBJECTINFO",  & CNetStorageHandler::x_ProcessGetObjectInfo },
    { "GETATTR",        & CNetStorageHandler::x_ProcessGetAttr },
    { "SETATTR",        & CNetStorageHandler::x_ProcessSetAttr },
    { "DELATTR",        & CNetStorageHandler::x_ProcessDelAttr },
    { "READ",           & CNetStorageHandler::x_ProcessRead },
    { "CREATE",         & CNetStorageHandler::x_ProcessCreate },
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
      m_NeedMetaInfo(false),
      m_DBClientID(-1),
      m_ObjectSize(0),
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

    while ((uttp_event = m_UTTPReader.GetNextEvent()) == CUTTPReader::eChunk ||
            uttp_event == CUTTPReader::eChunkPart)
        x_OnData(m_UTTPReader.GetChunkPart(), m_UTTPReader.GetChunkPartSize());

    switch (uttp_event) {
    case CUTTPReader::eEndOfBuffer:
        return false;

    case CUTTPReader::eControlSymbol:
        if (m_UTTPReader.GetControlSymbol() == '\n') {
            x_SendWriteConfirmation();
            m_ReadMode = eReadMessages;
            return true;
        }
        /* FALL THROUGH */

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
                            CNetStorageServerException::eInvalidMessageHeader,
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
                            CNetStorageServerException::eShuttingDown,
                            "Server is shutting down");

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
                            CNetStorageServerException::eMessageAfterBye,
                            "No messages are allowed after BYE");

        x_SetCmdRequestStatus(eStatus_BadRequest);
        if (x_SendSyncMessage(response) == eIO_Success)
            m_Server->CloseConnection(&GetSocket());
        return;
    }


    bool          error = true;
    string        error_client_message;
    unsigned int  error_code;
    unsigned int  http_error_code;

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
        http_error_code = ex.ErrCodeToHTTPStatusCode();
        error_code = ex.GetErrCode();
        error_client_message = ex.what();
    }
    catch (const CNetStorageException &  ex) {
        ERR_POST(ex);
        http_error_code = eStatus_ServerError;
        if (ex.GetErrCode() == CNetStorageException::eNotExists)
            error_code = CNetStorageServerException::eObjectNotFound;
        else
            error_code = CNetStorageServerException::eStorageError;
        error_client_message = ex.what();
    }
    catch (const std::exception &  ex) {
        ERR_POST("STL exception: " << ex.what());
        http_error_code = eStatus_ServerError;
        error_code = CNetStorageServerException::eInternalError;
        error_client_message = ex.what();
    }

    if (error) {
        x_SetCmdRequestStatus(http_error_code);

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
void CNetStorageHandler::x_OnData(const void* data, size_t data_size)
{
    if (!m_ObjectBeingWritten) {
        if (data_size > 0) {
            ERR_POST("Received " << data_size << " bytes after "
                     "an error has been reported to the client");
        }
        return;
    }

    try {
        m_ObjectBeingWritten.Write(data, data_size);
        m_ClientRegistry.AddBytesWritten(m_Client, data_size);
        m_ObjectSize += data_size;
    }
    catch (const std::exception &  ex) {
        string  message = "Error writing into " + m_ObjectBeingWritten.GetLoc() +
                          ": " + ex.what();
        ERR_POST(message);

        // Send response message with an error
        CJsonNode   response = CreateErrorResponseMessage(
                                    m_DataMessageSN,
                                    CNetStorageServerException::eWriteError,
                                    message);
        x_SendSyncMessage(response);
        m_ObjectBeingWritten = NULL;
        m_DataMessageSN = -1;
        return;
    }
}


void CNetStorageHandler::x_SendWriteConfirmation()
{
    if (!m_ObjectBeingWritten) {
        x_SetCmdRequestStatus(eStatus_ServerError);
        x_PrintMessageRequestStop();
        return;
    }

    if (m_NeedMetaInfo) {
        // Update of the meta DB is required. It differs depending it was an
        // object creation or writing into an existing one.
        string object_loc = m_ObjectBeingWritten.GetLoc();
        CNetStorageObjectLoc      object_loc_struct(
                m_Server->GetCompoundIDPool(),
                object_loc);

        if (m_CreateRequest) {
            // It was creating an object.
            // In case of problems the response must be ERROR which will be
            // automatically sent in case of an exception.
            try {
                m_Server->GetDb().ExecSP_CreateObjectWithClientID(
                        m_DBObjectID, object_loc_struct.GetUniqueKey(),
                        object_loc, m_ObjectSize, m_DBClientID);
            } catch (...) {
                m_ObjectBeingWritten = NULL;
                throw;
            }
        } else {
            // It was writing into existing object
            try {
                m_Server->GetDb().ExecSP_UpdateObjectOnWrite(
                        object_loc_struct.GetUniqueKey(),
                        object_loc, m_ObjectSize, m_DBClientID);
            } catch (const CException &  ex) {
                CJsonNode   response = CreateResponseMessage(m_DataMessageSN);
                AppendWarning(response, eDatabaseWarning, ex.what());
                x_SendSyncMessage(response);
                x_PrintMessageRequestStop();

                m_ObjectBeingWritten = NULL;
                m_DataMessageSN = -1;
                return;
            } catch (...) {
                CJsonNode   response = CreateResponseMessage(m_DataMessageSN);
                AppendWarning(response, eDatabaseWarning,
                              "Unknown updating object meta info error");
                x_SendSyncMessage(response);
                x_PrintMessageRequestStop();

                m_ObjectBeingWritten = NULL;
                m_DataMessageSN = -1;
                return;
            }
        }
    }

    x_SendSyncMessage(CreateResponseMessage(m_DataMessageSN));
    x_PrintMessageRequestStop();
    m_ObjectBeingWritten = NULL;
    m_DataMessageSN = -1;
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
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "Mandatory field 'Client' is missed");
    }

    string  client = NStr::TruncateSpaces(message.GetString("Client"));
    if (client.empty()) {
        NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                   "Mandatory field 'Client' is empty");
    }

    m_Client = client;
    if (message.HasKey("Application"))
        application = message.GetString("Application");
    if (message.HasKey("Ticket"))
        ticket = message.GetString("Ticket");
    if (message.HasKey("Service")) {
        m_Service = NStr::TruncateSpaces(message.GetString("Service"));
        m_NeedMetaInfo = m_Server->NeedMetadata(m_Service);
    }
    else {
        m_Service = "";
        m_NeedMetaInfo = false;
    }

    // Memorize the client in the registry
    m_ClientRegistry.Touch(m_Client, application,
                           ticket, m_Service, x_GetPeerAddress());

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
    reply.SetString("StartDate",
                    NST_FormatPreciseTime(m_Server->GetStartTime()));
    reply.SetString("ServerSession", m_Server->GetSessionID());
    reply.SetString("ServerBinaryPath",
                    CNcbiApplication::Instance()->GetProgramExecutablePath());
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
    reply.SetString("ConfigurationFilePath",
                    CNcbiApplication::Instance()->GetConfigPath());

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessHealth(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    if (m_Client.empty()) {
        NCBI_THROW(CNetStorageServerException, eHelloRequired,
                   "Anonymous client cannot request server health");
    }

    if (!m_Server->IsAdminClientName(m_Client)) {
        NCBI_THROW(CNetStorageServerException, ePrivileges,
                   "Only administrators can request server health");
    }

    m_ClientRegistry.AppendType(m_Client, CNSTClient::eAdministrator);

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);
    reply.SetByKey("Alerts", m_Server->SerializeAlerts());
    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessAckAlert(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    if (m_Client.empty()) {
        NCBI_THROW(CNetStorageServerException, eHelloRequired,
                   "Anonymous client cannot acknowledge alerts");
    }

    if (!m_Server->IsAdminClientName(m_Client)) {
        NCBI_THROW(CNetStorageServerException, ePrivileges,
                   "Only administrators can acknowledge alerts");
    }

    if (!message.HasKey("Name"))
        NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                   "Mandatory argument Name is not supplied "
                   "in ACKALERT command");

    m_ClientRegistry.AppendType(m_Client, CNSTClient::eAdministrator);

    EAlertAckResult     ack_result = m_Server->AcknowledgeAlert(message.GetString("Name"));
    CJsonNode           reply = CreateResponseMessage(common_args.m_SerialNumber);
    switch (ack_result) {
        case eAcknowledged:
            // No warning needed, everything is fine
            break;
        case eNotFound:
            AppendWarning(reply, eAlertNotFoundWarning,
                          "Alert has not been found");
            break;
        case eAlreadyAcknowledged:
            AppendWarning(reply, eAlertAlreadyAcknowledgedWarning,
                          "Alert has already been acknowledged");
            break;
        default:
            AppendWarning(reply, eAlertUnknownAcknowledgeResultWarning,
                          "Unknown acknowledge result");
    }
    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessReconfigure(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    if (m_Client.empty()) {
        NCBI_THROW(CNetStorageServerException, eHelloRequired,
                   "Anonymous client cannot reconfigure server");
    }

    if (!m_Server->IsAdminClientName(m_Client)) {
        NCBI_THROW(CNetStorageServerException, ePrivileges,
                   "Only administrators can reconfigure server");
    }

    m_ClientRegistry.AppendType(m_Client, CNSTClient::eAdministrator);

    CJsonNode               reply = CreateResponseMessage(common_args.m_SerialNumber);
    CNcbiApplication *      app = CNcbiApplication::Instance();
    bool                    reloaded = app->ReloadConfig(
                                            CMetaRegistry::fReloadIfChanged);
    if (!reloaded) {
        AppendWarning(reply, eConfigNotChangedWarning,
                      "Configuration file has not been changed, RECONFIGURE ignored");
        x_SendSyncMessage(reply);
        x_PrintMessageRequestStop();
        return;
    }

    const CNcbiRegistry &   reg = app->GetConfig();
    bool                    well_formed = NSTValidateConfigFile(reg);

    if (!well_formed) {
        m_Server->RegisterAlert(eReconfigure);
        NCBI_THROW(CNetStorageServerException, eInvalidConfig,
                   "Configuration file is not well formed");
    }


    // Reconfigurable at runtime:
    // [server]: logging and admin name list
    // [metadata_conf]
    SNetStorageServerParameters     params;
    params.Read(reg, "server");

    CJsonNode   server_diff = m_Server->SetParameters(params, true);
    CJsonNode   metadata_diff = m_Server->ReadMetadataConfiguration(reg);

    m_Server->AcknowledgeAlert(eConfig);
    m_Server->AcknowledgeAlert(eReconfigure);

    if (server_diff.IsNull() && metadata_diff.IsNull()) {
        if (m_ConnContext.NotNull())
             GetDiagContext().Extra().Print("accepted_changes", "none");
        AppendWarning(reply, eConfigNotChangedWarning,
                      "No changeable parameters were identified "
                      "in the new configuration file");
        reply.SetByKey("What", CJsonNode::NewObjectNode());
        x_SendSyncMessage(reply);
        x_PrintMessageRequestStop();
        return;
    }

    CJsonNode   total_changes = CJsonNode::NewObjectNode();
    if (!server_diff.IsNull())
        total_changes.SetByKey("server", server_diff);
    if (!metadata_diff.IsNull())
        total_changes.SetByKey("metadata_conf", metadata_diff);

    reply.SetByKey("What", total_changes);
    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessShutdown(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    if (m_Client.empty()) {
        NCBI_THROW(CNetStorageServerException, eHelloRequired,
                   "Anonymous client cannot shutdown server");
    }

    if (!m_Server->IsAdminClientName(m_Client)) {
        NCBI_THROW(CNetStorageServerException, ePrivileges,
                   "Only administrators can shutdown server");
    }

    if (!message.HasKey("Mode")) {
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "Mandatory field 'Mode' is missed");
    }

    string  mode;
    mode = message.GetString("Mode");
    if (mode != "hard" && mode != "soft") {
        NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                   "Allowed 'Mode' values are 'soft' and 'hard'");
    }

    if (mode == "hard")
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
CNetStorageHandler::x_ProcessGetMetadataInfo(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eAdministrator);

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);

    reply.SetByKey("Services", m_Server->serializeMetadataInfo());
    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessGetObjectInfo(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eReader);

    SObjectID         object_id = x_GetObjectKey(message);
    CNetStorage       net_storage(g_CreateNetStorage(
                          CNetICacheClient(CNetICacheClient::eAppRegistry), 0));
    CNetStorageObject net_storage_object = net_storage.Open(
                                                object_id.object_loc, 0);

    CJsonNode         object_info = net_storage_object.GetInfo().ToJSON();
    CJsonNode         reply = CreateResponseMessage(common_args.m_SerialNumber);

    for (CJsonIterator it = object_info.Iterate(); it; ++it)
        reply.SetByKey(it.GetKey(), it.GetNode());

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessGetAttr(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eReader);
    x_CheckNonAnonymousClient();

    if (!message.HasKey("AttrName"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "Mandatory field 'AttrName' is missed");

    string      attr_name = message.GetString("AttrName");
    if (attr_name.empty())
        NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                   "Attribute name must not be empty");

    if (!message.HasKey("ObjectLoc") && !message.HasKey("UserKey"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "GETATTR message must have ObjectLoc or UserKey. "
                   "None of them was found.");

    if (!m_NeedMetaInfo)
        NCBI_THROW(CNetStorageServerException, eInvalidMetaInfoRequest,
                   "Client service is not configured for meta info");

    m_Server->GetDb().ExecSP_CreateClient(m_Client, m_DBClientID);

    SObjectID   object_id = x_GetObjectKey(message);
    string      value;
    int         status = m_Server->GetDb().ExecSP_GetAttribute(
                                            object_id.object_key,
                                            attr_name, value);
    CJsonNode   reply = CreateResponseMessage(common_args.m_SerialNumber);

    if (status == 0) {
        // Everything is fine, the attribute is found
        reply.SetString("AttrValue", value);
    } else if (status == -1) {
        // Object is not found
        NCBI_THROW(CNetStorageServerException, eObjectNotFound,
                   "Object not found");
    } else if (status == -2) {
        // Attribute is not found
        NCBI_THROW(CNetStorageServerException, eAttributeNotFound,
                   "Attribute not found");
    } else if (status == -3) {
        // Attribute value is not found
        NCBI_THROW(CNetStorageServerException, eAttributeValueNotFound,
                   "Attribute value not found");
    } else {
        // Unknown status
        NCBI_THROW(CNetStorageServerException, eInternalError,
                   "Unknown GetAttributeValue status");
    }

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessSetAttr(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eWriter);
    x_CheckNonAnonymousClient();

    if (!message.HasKey("AttrName"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "Mandatory field 'AttrName' is missed");

    string      attr_name = message.GetString("AttrName");
    if (attr_name.empty())
        NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                    "Attribute name must not be empty");

    if (!message.HasKey("AttrValue"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "Mandatory field 'AttrValue' is missed");

    if (!message.HasKey("ObjectLoc") && !message.HasKey("UserKey"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "SETATTR message must have ObjectLoc or UserKey. "
                   "None of them was found.");

    if (!m_NeedMetaInfo)
        NCBI_THROW(CNetStorageServerException, eInvalidMetaInfoRequest,
                   "Client service is not configured for meta info");

    m_Server->GetDb().ExecSP_CreateClient(m_Client, m_DBClientID);

    SObjectID               object_id = x_GetObjectKey(message);
    string                  value = message.GetString("AttrValue");

    // The SP will throw an exception if an error occured
    m_Server->GetDb().ExecSP_AddAttribute(object_id.object_key,
                                          object_id.object_loc,
                                          attr_name, value,
                                          m_DBClientID);
    x_SendSyncMessage(CreateResponseMessage(common_args.m_SerialNumber));
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessDelAttr(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eWriter);
    x_CheckNonAnonymousClient();

    if (!message.HasKey("AttrName"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "Mandatory field 'AttrName' is missed");

    string      attr_name = message.GetString("AttrName");
    if (attr_name.empty())
        NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                    "Attribute name must not be empty");

    if (!message.HasKey("ObjectLoc") && !message.HasKey("UserKey"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "DELATTR message must have ObjectLoc or UserKey. "
                   "None of them was found.");

    if (!m_NeedMetaInfo)
        NCBI_THROW(CNetStorageServerException, eInvalidMetaInfoRequest,
                   "Client service is not configured for meta info");

    m_Server->GetDb().ExecSP_CreateClient(m_Client, m_DBClientID);

    SObjectID   object_id = x_GetObjectKey(message);
    int         status = m_Server->GetDb().ExecSP_DelAttribute(
                                            object_id.object_key, attr_name);
    CJsonNode   reply = CreateResponseMessage(common_args.m_SerialNumber);

    if (status == -1) {
        // Object is not found
        AppendWarning(reply, eObjectNotFoundWarning,
                      "Object not found");
    } else if (status == -2) {
        // Attribute is not found
        AppendWarning(reply, eAttributeNotFoundWarning,
                      "Attribute not found in meta info DB");
    } else if (status == -3) {
        // Attribute value is not found
        AppendWarning(reply, eAttributeValueNotFoundWarning,
                      "Attribute value not found in meta info DB");
    } else if (status != 0) {
        // Unknown status
        NCBI_THROW(CNetStorageServerException, eInternalError,
                   "Unknown DelAttributeValue status");
    }

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessCreate(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eWriter);
    x_CheckNonAnonymousClient();

    TNetStorageFlags    flags = ExtractStorageFlags(message);
    SICacheSettings     icache_settings = ExtractICacheSettings(message);

    x_CheckICacheSettings(icache_settings);

    if (m_NeedMetaInfo) {
        // Meta information is required so check the DB
        m_Server->GetDb().ExecSP_CreateClient(m_Client, m_DBClientID);
    }

    // Create the object stream depending on settings
    m_ObjectBeingWritten = x_CreateObjectStream(icache_settings, flags);

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);
    string          locator = m_ObjectBeingWritten.GetLoc();

    reply.SetString("ObjectLoc", locator);
    x_SendSyncMessage(reply);

    if (m_ConnContext.NotNull() && !message.HasKey("ObjectLoc")) {
        CNetStorageObjectLoc    object_loc_struct(m_Server->GetCompoundIDPool(),
                                                  locator);

        GetDiagContext().Extra()
            .Print("ObjectLoc", locator)
            .Print("ObjectKey", object_loc_struct.GetUniqueKey());
    }

    // Inform the message receiving loop that raw data are to follow
    m_ReadMode = eReadRawData;
    m_DataMessageSN = common_args.m_SerialNumber;
    m_CreateRequest = true;
    m_ObjectSize = 0;
    m_ClientRegistry.AddObjectsWritten(m_Client, 1);
}


void
CNetStorageHandler::x_ProcessWrite(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eWriter);
    x_CheckNonAnonymousClient();

    if (!message.HasKey("ObjectLoc") && !message.HasKey("UserKey"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "WRITE message must have ObjectLoc or UserKey. "
                   "None of them was found.");

    if (m_NeedMetaInfo) {
        // Meta information is required so check the DB
        m_Server->GetDb().ExecSP_CreateClient(m_Client, m_DBClientID);
    }

    SObjectID       object_id = x_GetObjectKey(message);
    CNetStorage     net_storage(g_CreateNetStorage(
                          CNetICacheClient(CNetICacheClient::eAppRegistry), 0));

    m_ObjectBeingWritten = net_storage.Open(object_id.object_loc, 0);

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);
    string          locator = m_ObjectBeingWritten.GetLoc();

    reply.SetString("ObjectLoc", locator);
    x_SendSyncMessage(reply);

    if (m_ConnContext.NotNull() && !message.HasKey("ObjectLoc")) {
        GetDiagContext().Extra()
            .Print("ObjectLoc", locator)
            .Print("ObjectKey", object_id.object_key);
    }

    // Inform the message receiving loop that raw data are to follow
    m_ReadMode = eReadRawData;
    m_DataMessageSN = common_args.m_SerialNumber;
    m_CreateRequest = false;
    m_ObjectSize = 0;
    m_ClientRegistry.AddObjectsWritten(m_Client, 1);
}


void
CNetStorageHandler::x_ProcessRead(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eReader);
    x_CheckNonAnonymousClient();

    if (!message.HasKey("ObjectLoc") && !message.HasKey("UserKey"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "READ message must have ObjectLoc or UserKey. "
                   "None of them was found.");

    if (m_NeedMetaInfo) {
        // Meta information is required so check the DB
        m_Server->GetDb().ExecSP_CreateClient(m_Client, m_DBClientID);
    }

    SObjectID           object_id = x_GetObjectKey(message);
    CNetStorage         net_storage(g_CreateNetStorage(
                          CNetICacheClient(CNetICacheClient::eAppRegistry), 0));
    CNetStorageObject   net_storage_object = net_storage.Open(
                                                    object_id.object_loc, 0);

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);
    x_SendSyncMessage(reply);

    m_ClientRegistry.AddObjectsRead(m_Client, 1);

    char            buffer[kReadBufferSize];
    size_t          bytes_read;
    Int8            total_bytes = 0;

    try {
        while (!net_storage_object.Eof()) {
            bytes_read = net_storage_object.Read(buffer, sizeof(buffer));

            m_UTTPWriter.SendChunk(buffer, bytes_read, false);

            if (x_SendOverUTTP() != eIO_Success)
                return;

            m_ClientRegistry.AddBytesRead(m_Client, bytes_read);
            total_bytes += bytes_read;
        }

        net_storage_object.Close();

        reply = CreateResponseMessage(common_args.m_SerialNumber);
    }
    catch (const CException &  ex) {
        reply = CreateErrorResponseMessage(common_args.m_SerialNumber,
                       CNetStorageServerException::eReadError,
                       string("Object read error: ") + ex.what());
    }

    m_UTTPWriter.SendControlSymbol('\n');

    if (x_SendOverUTTP() != eIO_Success)
        return;

    if (m_NeedMetaInfo) {
        m_Server->GetDb().ExecSP_UpdateObjectOnRead(
                object_id.object_key,
                object_id.object_loc, total_bytes, m_DBClientID);
    }

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessDelete(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eWriter);
    x_CheckNonAnonymousClient();

    if (!message.HasKey("ObjectLoc") && !message.HasKey("UserKey"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "DELETE message must have ObjectLoc or UserKey. "
                   "None of them was found.");

    SObjectID       object_id = x_GetObjectKey(message);
    int             status = 0;

    if (m_NeedMetaInfo) {
        // Meta information is required so delete from the DB first
        status = m_Server->GetDb().ExecSP_RemoveObject(object_id.object_key);
    }

    CNetStorage     net_storage(g_CreateNetStorage(
                          CNetICacheClient(CNetICacheClient::eAppRegistry), 0));

    net_storage.Remove(object_id.object_loc);
    m_ClientRegistry.AddObjectsDeleted(m_Client, 1);

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);

    if (m_NeedMetaInfo && status == -1) {
        // Stored procedure return -1 if the object is not found in the meta DB
        AppendWarning(reply, eObjectNotFoundWarning,
                      "Object not found in meta info DB");
    }

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessRelocate(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eWriter);
    x_CheckNonAnonymousClient();

    // Take the arguments
    if (!message.HasKey("NewLocation")) {
        NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                   "NewLocation argument is not found");
    }

    CJsonNode           new_location = message.GetByKey("NewLocation");
    TNetStorageFlags    new_location_flags = ExtractStorageFlags(new_location);
    SICacheSettings     new_location_icache_settings =
                                ExtractICacheSettings(new_location);

    x_CheckICacheSettings(new_location_icache_settings);

    if (m_NeedMetaInfo) {
        // Meta information is required so check the DB
        m_Server->GetDb().ExecSP_CreateClient(m_Client, m_DBClientID);
    }

    SObjectID       object_id = x_GetObjectKey(message);
    CNetStorage     net_storage(g_CreateNetStorage(
                          CNetICacheClient(CNetICacheClient::eAppRegistry), 0));
    string          new_object_loc = net_storage.Relocate(object_id.object_loc,
                                                          new_location_flags);

    if (m_NeedMetaInfo) {
        m_Server->GetDb().ExecSP_UpdateObjectOnRelocate(
                                    object_id.object_key,
                                    object_id.object_loc,
                                    m_DBClientID);
    }

    m_ClientRegistry.AddObjectsRelocated(m_Client, 1);

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);

    reply.SetString("ObjectLoc", new_object_loc);
    x_SendSyncMessage(reply);

    if (m_ConnContext.NotNull()) {
        GetDiagContext().Extra()
            .Print("NewObjectLoc", new_object_loc);
    }

    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessExists(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    // It was decided not to touch the DB even if meta info is required
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eReader);

    SObjectID       object_id = x_GetObjectKey(message);
    CNetStorage     net_storage(g_CreateNetStorage(
                          CNetICacheClient(CNetICacheClient::eAppRegistry), 0));
    bool            exists = net_storage.Exists(object_id.object_loc);
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
    // It was decided not to touch the DB even if meta info is required
    m_ClientRegistry.AppendType(m_Client, CNSTClient::eReader);

    SObjectID         object_id = x_GetObjectKey(message);
    CNetStorage       net_storage(g_CreateNetStorage(
                        CNetICacheClient(CNetICacheClient::eAppRegistry), 0));
    CNetStorageObject net_storage_object = net_storage.Open(
                        object_id.object_loc, 0);
    Uint8             object_size = net_storage_object.GetSize();
    CJsonNode         reply = CreateResponseMessage(common_args.m_SerialNumber);

    reply.SetInteger("Size", object_size);
    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


SObjectID
CNetStorageHandler::x_GetObjectKey(const CJsonNode &  message)
{
    SObjectID       ret_val;

    if (message.HasKey("ObjectLoc")) {
        string  object_loc = message.GetString("ObjectLoc");
        x_CheckObjectLoc(object_loc);

        CNetStorageObjectLoc  object_loc_struct(m_Server->GetCompoundIDPool(),
                              object_loc);
        if (m_ConnContext.NotNull())
            GetDiagContext().Extra()
                .Print("ObjectKey", object_loc_struct.GetUniqueKey());

        ret_val.object_key = object_loc_struct.GetUniqueKey();
        ret_val.object_loc = object_loc;
        return ret_val;
    }

    // Take the arguments
    SICacheSettings     icache_settings;
    SUserKey            user_key;
    TNetStorageFlags    flags;

    x_GetStorageParams(message, &icache_settings, &user_key, &flags);


    string      client_name(m_Client);
    if (client_name.empty())
        client_name = "anonymous";

    CNetICacheClient    icache_client(icache_settings.m_ServiceName,
                                      icache_settings.m_CacheName, client_name);
    CNetStorageObjectLoc object_loc_struct(m_Server->GetCompoundIDPool(),
                                       flags, user_key.m_AppDomain,
                                       user_key.m_UniqueID,
                                       TFileTrack_Site::GetDefault().c_str());
    g_SetNetICacheParams(object_loc_struct, icache_client);

    ret_val.object_key = object_loc_struct.GetUniqueKey();
    ret_val.object_loc = object_loc_struct.GetLoc();

    // Log if needed
    if (m_ConnContext.NotNull()) {
        GetDiagContext().Extra()
            .Print("ObjectLoc", ret_val.object_loc)
            .Print("ObjectKey", ret_val.object_key);
    }

    return ret_val;
}


void
CNetStorageHandler::x_CheckNonAnonymousClient(void)
{
    if (m_Client.empty()) {
        NCBI_THROW(CNetStorageServerException, eHelloRequired,
                   "Anonymous client cannot perform "
                   "I/O operations with objects");
    }
}


void
CNetStorageHandler::x_CheckObjectLoc(const string &  object_loc)
{
    if (object_loc.empty()) {
        NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                   "Object locator must not be an empty string");
    }
}


void
CNetStorageHandler::x_CheckICacheSettings(
                            const SICacheSettings &  icache_settings)
{
    if (!icache_settings.m_ServiceName.empty()) {
        // CacheName is mandatory in this case
        if (icache_settings.m_CacheName.empty()) {
            NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                       "CacheName is required if ServiceName is provided");
        }
    }
}


void
CNetStorageHandler::x_CheckUserKey(const SUserKey &  user_key)
{
    if (!user_key.m_UniqueID.empty()) {
        // AppDomain is mandatory in this case
        if (user_key.m_AppDomain.empty()) {
            NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                       "AppDomain is required if UniqueKey is provided");
        }
    }
}


void
CNetStorageHandler::x_GetStorageParams(
                        const CJsonNode &   message,
                        SICacheSettings *   icache_settings,
                        SUserKey *          user_key,
                        TNetStorageFlags *  flags)
{
    *icache_settings = ExtractICacheSettings(message);
    *user_key = ExtractUserKey(message);

    // Check the parameters validity
    x_CheckICacheSettings(*icache_settings);
    x_CheckUserKey(*user_key);

    *flags = ExtractStorageFlags(message);
}


CNetStorageObject CNetStorageHandler::x_CreateObjectStream(
                    const SICacheSettings &  icache_settings,
                    TNetStorageFlags         flags)
{
    CNetStorage     net_storage;
    if (icache_settings.m_ServiceName.empty())
        net_storage = g_CreateNetStorage(
                CNetICacheClient(CNetICacheClient::eAppRegistry), 0);
    else {
        CNetICacheClient icache_client(icache_settings.m_ServiceName,
                icache_settings.m_CacheName, m_Client);
        net_storage = g_CreateNetStorage(icache_client, 0);
    }

    if (m_NeedMetaInfo) {
        m_Server->GetDb().ExecSP_GetNextObjectID(m_DBObjectID);

        return g_CreateNetStorageObject(net_storage,
                                        m_DBObjectID, flags);
    }

    return net_storage.Create(flags);
}


EIO_Status
CNetStorageHandler::x_SendOverUTTP()
{
    const char *    output_buffer;
    size_t          output_buffer_size;
    size_t          written;

    do {
        m_UTTPWriter.GetOutputBuffer(&output_buffer, &output_buffer_size);
        if (output_buffer_size > 0) {
            // Write to the socket as a single transaction
            EIO_Status result =
                GetSocket().Write(output_buffer, output_buffer_size, &written);
            if (result != eIO_Success) {
                ERR_POST("Error writing to the client socket. "
                         "The connection will be closed.");
                x_SetConnRequestStatus(eStatus_SocketIOError);
                m_Server->CloseConnection(&GetSocket());
                return result;
            }
        }
    } while (m_UTTPWriter.NextOutputBuffer());

    return eIO_Success;
}
