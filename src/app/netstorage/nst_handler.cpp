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

#include <corelib/resource_info.hpp>
#include <connect/services/netstorage.hpp>
#include <corelib/ncbi_message.hpp>

#include "nst_handler.hpp"
#include "nst_server.hpp"
#include "nst_protocol_utils.hpp"
#include "nst_exception.hpp"
#include "nst_version.hpp"
#include "nst_application.hpp"
#include "nst_warning.hpp"
#include "nst_config.hpp"
#include "nst_util.hpp"
#include "error_codes.hpp"


USING_NCBI_SCOPE;

const size_t    kReadBufferSize = 1024 * 1024;
const size_t    kWriteBufferSize = 16 * 1024;


CNetStorageHandler::SProcessorMap   CNetStorageHandler::sm_Processors[] =
{
    { "BYE",              & CNetStorageHandler::x_ProcessBye },
    { "HELLO",            & CNetStorageHandler::x_ProcessHello },
    { "INFO",             & CNetStorageHandler::x_ProcessInfo },
    { "CONFIGURATION",    & CNetStorageHandler::x_ProcessConfiguration },
    { "HEALTH",           & CNetStorageHandler::x_ProcessHealth },
    { "ACKALERT",         & CNetStorageHandler::x_ProcessAckAlert },
    { "RECONFIGURE",      & CNetStorageHandler::x_ProcessReconfigure },
    { "SHUTDOWN",         & CNetStorageHandler::x_ProcessShutdown },
    { "GETCLIENTSINFO",   & CNetStorageHandler::x_ProcessGetClientsInfo },
    { "GETMETADATAINFO",  & CNetStorageHandler::x_ProcessGetMetadataInfo },
    { "GETOBJECTINFO",    & CNetStorageHandler::x_ProcessGetObjectInfo },
    { "GETATTRLIST",      & CNetStorageHandler::x_ProcessGetAttrList },
    { "GETCLIENTOBJECTS", & CNetStorageHandler::x_ProcessGetClientObjects },
    { "GETATTR",          & CNetStorageHandler::x_ProcessGetAttr },
    { "SETATTR",          & CNetStorageHandler::x_ProcessSetAttr },
    { "DELATTR",          & CNetStorageHandler::x_ProcessDelAttr },
    { "READ",             & CNetStorageHandler::x_ProcessRead },
    { "CREATE",           & CNetStorageHandler::x_ProcessCreate },
    { "WRITE",            & CNetStorageHandler::x_ProcessWrite },
    { "DELETE",           & CNetStorageHandler::x_ProcessDelete },
    { "RELOCATE",         & CNetStorageHandler::x_ProcessRelocate },
    { "EXISTS",           & CNetStorageHandler::x_ProcessExists },
    { "GETSIZE",          & CNetStorageHandler::x_ProcessGetSize },
    { "SETEXPTIME",       & CNetStorageHandler::x_ProcessSetExpTime },
    { "LOCKFTPATH",       & CNetStorageHandler::x_ProcessLockFTPath },
    { "",                 NULL }
};



CRequestContextResetter::CRequestContextResetter()
{}

CRequestContextResetter::~CRequestContextResetter()
{
    CDiagContext::SetRequestContext(NULL);
}

CMessageListenerResetter::CMessageListenerResetter()
{}

CMessageListenerResetter::~CMessageListenerResetter()
{
    IMessageListener::PopListener();
}



CNetStorageHandler::CNetStorageHandler(CNetStorageServer *  server)
    : m_Server(server),
      m_ReadBuffer(NULL),
      m_ReadMode(CNetStorageHandler::eReadMessages),
      m_WriteBuffer(NULL),
      m_JSONWriter(m_UTTPWriter),
      m_ObjectBeingWritten(eVoid),
      m_DataMessageSN(-1),
      m_MetadataOption(eMetadataNotSpecified),
      m_DBClientID(-1),
      m_ObjectSize(0),
      m_ByeReceived(false),
      m_FirstMessage(true),
      m_WriteCreateNeedMetaDBUpdate(false)
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

    if (m_Server->IsLog()) {
        CRequestContextResetter     context_resetter;
        x_CreateConnContext();
    }
}


void CNetStorageHandler::OnRead(void)
{
    CRequestContextResetter     context_resetter;
    if (m_ConnContext.NotNull()) {
        if (m_CmdContext.NotNull())
            CDiagContext::SetRequestContext(m_CmdContext);
        else
            CDiagContext::SetRequestContext(m_ConnContext);
    }


    size_t              n_read;
    CNSTPreciseTime     start = CNSTPreciseTime::Current();
    EIO_Status          status = GetSocket().Read(m_ReadBuffer, kReadBufferSize,
                                                  &n_read);
    if (m_Server->IsLogTimingClientSocket())
        m_Timing.Append("Client socket read", start);

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
                x_SetConnRequestStatus(eStatus_BadRequest);
            } else {
                x_SetConnRequestStatus(eStatus_Probe);
            }
            m_Server->CloseConnection(&GetSocket());
        }
        catch (const exception &  ex) {
            ERR_POST("STL exception while processing incoming message: " <<
                     ex.what());
            x_SetConnRequestStatus(eStatus_BadRequest);
            m_Server->CloseConnection(&GetSocket());
        }
        catch (...) {
            ERR_POST("Unknown exception while processing incoming message");
            x_SetConnRequestStatus(eStatus_BadRequest);
            m_Server->CloseConnection(&GetSocket());
        }
    }
}


void CNetStorageHandler::OnWrite(void)
{
    // This implementation is left to simplify the transition to
    // asynchronous replies. Currently this method will never be called
    // because the GetEventsToPollFor(...) never returns anything but
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
            if (m_CmdContext.NotNull())
                m_CmdContext->SetRequestStatus(eStatus_SocketIOError);
            m_ConnContext->SetRequestStatus(eStatus_SocketIOError);
            break;
    }

    // If a command has not finished its logging by some reasons - do it
    // here as the last chance.
    if (m_CmdContext.NotNull()) {
        CDiagContext::SetRequestContext(m_CmdContext);
        if (!m_Timing.Empty() && m_Server->IsLogTiming()) {
            string      timing =  m_Timing.Serialize(GetDiagContext().Extra());
            if (!timing.empty())
                GetDiagContext().Extra().Print("timing", timing);
        }
        GetDiagContext().PrintRequestStop();
        m_CmdContext.Reset();
        CDiagContext::SetRequestContext(NULL);
    }

    CSocket&        socket = GetSocket();
    m_ConnContext->SetBytesRd(socket.GetCount(eIO_Read));
    m_ConnContext->SetBytesWr(socket.GetCount(eIO_Write));

    CDiagContext::SetRequestContext(m_ConnContext);
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
    CRequestContextResetter     context_resetter;
    if (m_ConnContext.NotNull()) {
        if (m_CmdContext.NotNull())
           CDiagContext::SetRequestContext(m_CmdContext);
        else
            CDiagContext::SetRequestContext(m_ConnContext);
    }

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
                            NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                            "Error extracting mandatory fields",
                            ex.GetClass(),
                            CNetStorageServerException::eInvalidMessageHeader);

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
                            NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                            "Server is shutting down",
                            kScopeLogic,
                            CNetStorageServerException::eShuttingDown);

        x_SetCmdRequestStatus(eStatus_ShuttingDown);
        if (x_SendSyncMessage(response) == eIO_Success)
            m_Server->CloseConnection(&GetSocket());
        return;
    }



    if (m_ByeReceived) {
        // BYE message has been received before. Send error response
        // and close the connection.
        ERR_POST(Warning << "Received a message after BYE. "
                 "The connection will be closed.");

        // Send response message with an error
        CJsonNode   response = CreateErrorResponseMessage(
                            common_args.m_SerialNumber,
                            NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                            "No messages are allowed after BYE",
                            kScopeLogic,
                            CNetStorageServerException::eMessageAfterBye);

        x_SetCmdRequestStatus(eStatus_BadRequest);
        if (x_SendSyncMessage(response) == eIO_Success)
            m_Server->CloseConnection(&GetSocket());
        return;
    }


    bool          error = true;
    string        error_client_message;
    unsigned int  error_code;
    unsigned int  http_error_code;
    string        error_scope;

    try {
        m_Timing.Clear();
        m_Server->GetClientRegistry().Touch(m_Client);

        // Find the processor
        FProcessor  processor = x_FindProcessor(common_args);

        // Call the processor. It returns the number of bytes to expect
        // after this message. If 0 then another message is expected.
        (this->*processor)(message, common_args);
        error = false;
    }
    catch (const CNetStorageServerException &  ex) {
        if (ex.GetErrCode() !=
                CNetStorageServerException::eNetStorageAttributeNotFound &&
            ex.GetErrCode() !=
                CNetStorageServerException::eNetStorageAttributeValueNotFound) {
            // Choke the error printout for this specific case, see CXX-5818
            ERR_POST(ex);
            if (!x_PrintTimingIsOn() && !m_Timing.Empty())
                ERR_POST("Timing information: " + m_Timing.Serialize());
        }
        http_error_code = ex.ErrCodeToHTTPStatusCode();
        error_code = ex.GetErrCode();
        error_client_message = ex.what();
        error_scope = ex.GetClass();

        // Note: eAccess alert is set at the point before an exception is
        // thrown. This is done for a clean alert message.
    }
    catch (const CNetStorageException &  ex) {
        ERR_POST(ex);
        if (!x_PrintTimingIsOn() && !m_Timing.Empty())
            ERR_POST("Timing information: " + m_Timing.Serialize());
        http_error_code = eStatus_ServerError;
        if (ex.GetErrCode() == CNetStorageException::eNotExists)
            error_code = CNetStorageServerException::eRemoteObjectNotFound;
        else
            error_code = CNetStorageServerException::eStorageError;
        error_client_message = ex.what();
        error_scope = ex.GetClass();
    }
    catch (const CException &  ex) {
        ERR_POST(ex);
        if (!x_PrintTimingIsOn() && !m_Timing.Empty())
            ERR_POST("Timing information: " + m_Timing.Serialize());
        http_error_code = eStatus_ServerError;
        error_code = CNetStorageServerException::eInternalError;
        error_client_message = ex.what();
        error_scope = ex.GetClass();
    }
    catch (const std::exception &  ex) {
        ERR_POST("STL exception: " << ex.what());
        if (!x_PrintTimingIsOn() && !m_Timing.Empty())
            ERR_POST("Timing information: " + m_Timing.Serialize());
        http_error_code = eStatus_ServerError;
        error_code = CNetStorageServerException::eInternalError;
        error_client_message = ex.what();
        error_scope = kScopeStdException;
    }
    catch (...) {
        ERR_POST("Unknown exception");
        if (!x_PrintTimingIsOn() && !m_Timing.Empty())
            ERR_POST("Timing information: " + m_Timing.Serialize());
        http_error_code = eStatus_ServerError;
        error_code = CNetStorageServerException::eInternalError;
        error_client_message = "Unknown exception";
        error_scope = kScopeUnknownException;
    }

    if (error) {
        x_SetCmdRequestStatus(http_error_code);

        // Send response message with an error
        CJsonNode   response = CreateErrorResponseMessage(
                                    common_args.m_SerialNumber,
                                    NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                                    error_client_message,
                                    error_scope, error_code);
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

    CNSTPreciseTime     start = CNSTPreciseTime::Current();
    try {
        m_ObjectBeingWritten.Write(data, data_size);
        if (m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI write (" +
                            NStr::NumericToString(data_size) + ")", start);
        start = 0.0;
        m_Server->GetClientRegistry().AddBytesWritten(m_Client, data_size);
        m_ObjectSize += data_size;

        if (m_CmdContext.NotNull())
            m_CmdContext->SetBytesWr(m_ObjectSize);
    }
    catch (const std::exception &  ex) {
        string  message = "Error writing into " +
                          m_ObjectBeingWritten.GetLoc() + ": " + ex.what();
        if (double(start) != 0.0 && m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI writing error (" +
                            NStr::NumericToString(data_size) + ")", start);
        ERR_POST(message);
        if (!x_PrintTimingIsOn() && !m_Timing.Empty())
            ERR_POST("Timing information: " + m_Timing.Serialize());

        // Send response message with an error
        CJsonNode   response = CreateErrorResponseMessage(
                                    m_DataMessageSN,
                                    NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                                    message, kScopeStdException,
                                    CNetStorageServerException::eWriteError);
        x_SendSyncMessage(response);
        m_ObjectBeingWritten = eVoid;
        m_DataMessageSN = -1;
    }
    catch (...) {
        string  message = "Unknown exception while writing into " +
                          m_ObjectBeingWritten.GetLoc();
        if (double(start) != 0.0 && m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI writing error (" +
                            NStr::NumericToString(data_size) + ")", start);
        ERR_POST(message);
        if (!x_PrintTimingIsOn() && !m_Timing.Empty())
            ERR_POST("Timing information: " + m_Timing.Serialize());

        // Send response message with an error
        CJsonNode   response = CreateErrorResponseMessage(
                                    m_DataMessageSN,
                                    NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                                    message, kScopeUnknownException,
                                    CNetStorageServerException::eWriteError);
        x_SendSyncMessage(response);
        m_ObjectBeingWritten = eVoid;
        m_DataMessageSN = -1;
    }
}


void CNetStorageHandler::x_SendWriteConfirmation()
{
    if (!m_ObjectBeingWritten) {
        x_SetCmdRequestStatus(eStatus_ServerError);
        x_PrintMessageRequestStop();
        return;
    }

    CNSTPreciseTime     start = CNSTPreciseTime::Current();
    try {
        if (m_ObjectSize == 0) {
            // This is the object of zero size i.e. there were no write()
            // calls. Thus the remote object was not really created.
            m_ObjectBeingWritten.Write("", 0);
            if (m_Server->IsLogTimingNSTAPI())
                m_Timing.Append("NetStorageAPI writing (0)", start);
        }
        start = CNSTPreciseTime::Current();
        m_ObjectBeingWritten.Close();
        if (m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI closing", start);
    }
    catch (const CException &  ex) {
        x_SetCmdRequestStatus(eStatus_ServerError);
        if (m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI finilizing error", start);

        string  message = "Error while finalizing " +
                          m_ObjectBeingWritten.GetLoc() + ": " + ex.GetMsg();
        ERR_POST(message);
        if (!x_PrintTimingIsOn())
            ERR_POST("Timing information: " + m_Timing.Serialize());
        x_PrintMessageRequestStop();

        CJsonNode   response = CreateErrorResponseMessage(
                                    m_DataMessageSN,
                                    NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                                    message, ex.GetClass(),
                                    CNetStorageServerException::eWriteError);
        x_SendSyncMessage(response);
        m_ObjectBeingWritten = eVoid;
        m_DataMessageSN = -1;
        return;
    }

    if (m_WriteCreateNeedMetaDBUpdate) {
        // Update of the meta DB is required. It differs depending it was an
        // object creation or writing into an existing one.
        const CNetStorageObjectLoc &   object_loc_struct =
                                                m_ObjectBeingWritten.Locator();
        string                         locator = object_loc_struct.GetLocator();

        if (m_CreateRequest) {
            // It was creating an object.
            // In case of problems the response must be ERROR which will be
            // automatically sent in case of an exception.
            try {
                m_Server->GetDb().ExecSP_CreateObjectWithClientID(
                        m_DBObjectID, object_loc_struct.GetUniqueKey(),
                        locator, m_ObjectSize, m_DBClientID, m_CreateTTL,
                        m_Timing);
            } catch (const CException &  ex) {
                x_SetCmdRequestStatus(eStatus_ServerError);
                CJsonNode   response = CreateErrorResponseMessage(
                                    m_DataMessageSN,
                                    NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                                    ex.what(), ex.GetClass(), ex.GetErrCode());
                x_SendSyncMessage(response);
                ERR_POST(ex);
                if (!x_PrintTimingIsOn() && !m_Timing.Empty())
                    ERR_POST("Timing information: " + m_Timing.Serialize());
                x_PrintMessageRequestStop();

                m_ObjectBeingWritten = eVoid;
                m_DataMessageSN = -1;
                return;
            } catch (...) {
                x_SetCmdRequestStatus(eStatus_ServerError);
                CJsonNode   response = CreateErrorResponseMessage(
                                    m_DataMessageSN,
                                    NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                                    "Unknown metadata information DB error",
                                    kScopeUnknownException,
                                    CNetStorageServerException::eUnknownError);
                x_SendSyncMessage(response);
                ERR_POST("Unknown metadata information DB error");
                if (!x_PrintTimingIsOn() && !m_Timing.Empty())
                    ERR_POST("Timing information: " + m_Timing.Serialize());
                x_PrintMessageRequestStop();

                m_ObjectBeingWritten = eVoid;
                m_DataMessageSN = -1;
                return;
            }
        } else {
            // It was writing into existing object
            try {
                if (object_loc_struct.HasUserKey()) {
                    m_Server->GetDb().ExecSP_UpdateUserKeyObjectOnWrite(
                                        object_loc_struct.GetUniqueKey(),
                                        locator, m_ObjectSize, m_DBClientID,
                                        m_WriteServiceProps.GetTTL(),
                                        m_WriteServiceProps.GetProlongOnWrite(),
                                        m_WriteObjectExpiration,
                                        m_Timing);
                } else {
                    m_Server->GetDb().ExecSP_UpdateObjectOnWrite(
                                        object_loc_struct.GetUniqueKey(),
                                        locator, m_ObjectSize, m_DBClientID,
                                        m_WriteServiceProps.GetTTL(),
                                        m_WriteServiceProps.GetProlongOnWrite(),
                                        m_WriteObjectExpiration,
                                        m_Timing);
                }
            } catch (const CException &  ex) {
                CJsonNode   response = CreateResponseMessage(m_DataMessageSN);
                AppendWarning(response,
                              NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                              ex.what(), ex.GetClass(), eDatabaseWarning);
                x_SendSyncMessage(response);
                ERR_POST(ex);
                if (!x_PrintTimingIsOn() && !m_Timing.Empty())
                    ERR_POST("Timing information: " + m_Timing.Serialize());
                x_PrintMessageRequestStop();

                m_ObjectBeingWritten = eVoid;
                m_DataMessageSN = -1;
                return;
            } catch (...) {
                CJsonNode   response = CreateResponseMessage(m_DataMessageSN);
                AppendWarning(response,
                              NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                              "Unknown updating object meta info error",
                              kScopeUnknownException, eDatabaseWarning);
                x_SendSyncMessage(response);
                ERR_POST("Unknown updating object meta info error");
                if (!x_PrintTimingIsOn() && !m_Timing.Empty())
                    ERR_POST("Timing information: " + m_Timing.Serialize());
                x_PrintMessageRequestStop();

                m_ObjectBeingWritten = eVoid;
                m_DataMessageSN = -1;
                return;
            }
        }
    }

    x_SendSyncMessage(CreateResponseMessage(m_DataMessageSN));
    x_PrintMessageRequestStop();
    m_ObjectBeingWritten = eVoid;
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


// Log what we can, set the status code and close the connection
void  CNetStorageHandler::x_OnSocketWriteError(
        EIO_Status  status, size_t  bytes_written,
        const char * output_buffer, size_t  output_buffer_size)
{
    string  report =
        "Error writing message to the client. "
        "Peer: " +  GetSocket().GetPeerAddress() + ". "
        "Socket write error status: " + IO_StatusStr(status) + ". "
        "Written bytes: " + NStr::NumericToString(bytes_written) +
        ". Message begins with: ";

    if (output_buffer_size > 32) {
        CTempString     buffer_head(output_buffer, 32);
        report += NStr::PrintableString(buffer_head) + " (TRUNCATED)";
    }
    else {
        CTempString     buffer_head(output_buffer, output_buffer_size);
        report += NStr::PrintableString(buffer_head);
    }

    if (!x_PrintTimingIsOn() && !m_Timing.Empty())
        report += " Timing info: " + m_Timing.Serialize();

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
    m_Server->GetClientRegistry().RegisterSocketWriteError(m_Client);
    m_Server->CloseConnection(&GetSocket());
}


EIO_Status CNetStorageHandler::x_SendOutputBuffer(void)
{
    const char *    output_buffer;
    size_t          output_buffer_size;
    size_t          bytes_written;

    do {
        m_JSONWriter.GetOutputBuffer(&output_buffer, &output_buffer_size);
        while (output_buffer_size > 0) {
            CNSTPreciseTime     start = CNSTPreciseTime::Current();
            EIO_Status  status = GetSocket().Write(output_buffer,
                                                   output_buffer_size,
                                                   &bytes_written);
            if (m_Server->IsLogTimingClientSocket())
                m_Timing.Append("Client socket write (" +
                                NStr::NumericToString(output_buffer_size) + ")",
                                start);
            if (status != eIO_Success) {
                // Error writing to the socket. Log what we can and close the
                // connection.
                x_OnSocketWriteError(status, bytes_written,
                                     output_buffer, output_buffer_size);
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
    m_ConnContext.Reset(new CRequestContext());
    m_ConnContext->SetRequestID();
    m_ConnContext->SetClientIP(GetSocket().GetPeerAddress(eSAF_IP));

    // Set the connection request as the current one and print request start
    CDiagContext::SetRequestContext(m_ConnContext);
    GetDiagContext().PrintRequestStart()
                    .Print("_type", "conn");
    m_ConnContext->SetRequestStatus(eStatus_OK);
    CDiagContext::SetRequestContext(NULL);
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
    if (m_ConnContext.NotNull()) {
        if (m_CmdContext.IsNull()) {
            m_CmdContext.Reset(new CRequestContext());
            m_CmdContext->SetRequestStatus(eStatus_OK);
            m_CmdContext->SetRequestID();
        }
        CDiagContext::SetRequestContext(m_CmdContext);

        // The setting of the client session and IP
        // must be done before logging.
        SetSessionAndIPAndPHID(message, GetSocket());

        CDiagContext_Extra    ctxt_extra =
            GetDiagContext().PrintRequestStart()
                            .Print("_type", "message")
                            .Print("conn", m_ConnContext->GetRequestID());

        for (CJsonIterator it = message.Iterate(); it; ++it) {
            string      key = it.GetKey();

            // ncbi_phid has been printed by PrintRequestStart() anyway
            if (key == "ncbi_phid")
                continue;
            if (key == "SessionID" || key == "ClientIP")
                continue;

            ctxt_extra.Print(key,
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
        if (!m_Timing.Empty() && m_Server->IsLogTiming()) {
            string      timing =  m_Timing.Serialize(GetDiagContext().Extra());
            if (!timing.empty())
                GetDiagContext().Extra().Print("timing", timing);
        }
        GetDiagContext().PrintRequestStop();
        m_CmdContext.Reset();
        CDiagContext::SetRequestContext(NULL);
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
    string      application;        // Optional field
    string      ticket;             // Optional field
    string      protocol_version;   // Optional field

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
    if (message.HasKey("ProtocolVersion")) {
        // Some clients may send the protocol version as an integer
        // See CXX-6157
        CJsonNode               ver = message.GetByKey("ProtocolVersion");
        CJsonNode::ENodeType    node_type = ver.GetNodeType();
        if (node_type == CJsonNode::eString)
            protocol_version = ver.AsString();
        else if (node_type == CJsonNode::eInteger)
            protocol_version = NStr::NumericToString(ver.AsInteger()) +
                               ".0.0";
        else
            NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                       "Invalid type of the 'ProtocolVersion'. "
                       "String is expected.");
    }
    if (message.HasKey("Service"))
        m_Service = NStr::TruncateSpaces(message.GetString("Service"));
    else
        m_Service = "";
    m_MetadataOption = x_ConvertMetadataArgument(message);

    // Memorize the client in the registry
    m_Server->GetClientRegistry().Touch(m_Client, application,
                           ticket, m_Service, protocol_version,
                           m_MetadataOption, x_GetPeerAddress());

    // Send success response
    x_SendSyncMessage(CreateResponseMessage(common_args.m_SerialNumber));
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessInfo(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_Server->GetClientRegistry().AppendType(m_Client,
                                             CNSTClient::eAdministrator);

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
    m_Server->GetClientRegistry().AppendType(m_Client,
                                             CNSTClient::eAdministrator);

    CNcbiOstrstream             conf;
    CNcbiOstrstreamToString     converter(conf);

    CNcbiApplication::Instance()->GetConfig().Write(conf);

    CJsonNode   reply = CreateResponseMessage(common_args.m_SerialNumber);
    reply.SetString("Configuration", string(converter));
    reply.SetString("ConfigurationFilePath",
                    CNcbiApplication::Instance()->GetConfigPath());
    reply.SetByKey("BackendConfiguration", m_Server->GetBackendConfiguration());
    reply.SetDouble("DBExecuteSPTimeout",
                    m_Server->GetDb().GetExecuteSPTimeout().GetAsDouble());

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

    // GRID dashboard needs this command to be executed regardless of the
    // configuration settings, so the requirement of being an admin is
    // removed
    // if (!m_Server->IsAdminClientName(m_Client)) {
    //     string      msg = "Only administrators can request server health";
    //     m_Server->RegisterAlert(eAccess, msg);
    //     NCBI_THROW(CNetStorageServerException, ePrivileges, msg);
    // }

    m_Server->GetClientRegistry().AppendType(m_Client,
                                             CNSTClient::eAdministrator);

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);
    reply.SetByKey("Alerts", m_Server->SerializeAlerts());

    // Generic database info
    CJsonNode       db_stat_node(CJsonNode::NewObjectNode());
    bool            connected = m_Server->GetDb().IsConnected();

    db_stat_node.SetBoolean("connected", connected);
    if (connected) {
        try {
            map<string, string>   db_stat = m_Server->GetDb().
                                            ExecSP_GetGeneralDBInfo(m_Timing);
            for (map<string, string>::const_iterator  k = db_stat.begin();
                 k != db_stat.end(); ++k)
                db_stat_node.SetString(k->first, k->second);

            db_stat = m_Server->GetDb().ExecSP_GetStatDBInfo(m_Timing);
            for (map<string, string>::const_iterator  k = db_stat.begin();
                 k != db_stat.end(); ++k)
                db_stat_node.SetString(k->first, k->second);
        } catch (const std::exception &  ex) {
            // Health must not fail if there is no meta DB connection
            AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                          ex.what(), kScopeStdException, eDatabaseWarning);
        } catch (...) {
            // Health must not fail if there is no meta DB connection
            AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                          "Unknown metainfo DB access error",
                          kScopeUnknownException, eDatabaseWarning);
        }
    }
    reply.SetByKey("MetainfoDB", db_stat_node);

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
        string      msg = "Only administrators can acknowledge alerts";
        m_Server->RegisterAlert(eAccess, msg);
        NCBI_THROW(CNetStorageServerException, ePrivileges, msg);
    }

    if (!message.HasKey("Name"))
        NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                   "Mandatory argument Name is not supplied "
                   "in ACKALERT command");
    if (!message.HasKey("User"))
        NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                   "Mandatory argument User is not supplied "
                   "in ACKALERT command");

    m_Server->GetClientRegistry().AppendType(m_Client,
                                             CNSTClient::eAdministrator);

    EAlertAckResult ack_result = m_Server->AcknowledgeAlert(
                                                    message.GetString("Name"),
                                                    message.GetString("User"));
    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);
    switch (ack_result) {
        case eAcknowledged:
            // No warning needed, everything is fine
            break;
        case eNotFound:
            AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                          "Alert has not been found", kScopeLogic,
                          eAlertNotFoundWarning);
            break;
        case eAlreadyAcknowledged:
            AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                          "Alert has already been acknowledged", kScopeLogic,
                          eAlertAlreadyAcknowledgedWarning);
            break;
        default:
            AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                          "Unknown acknowledge result", kScopeLogic,
                          eAlertUnknownAcknowledgeResultWarning);
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

    if (!m_Server->AnybodyCanReconfigure()) {
        if (!m_Server->IsAdminClientName(m_Client)) {
            string      msg = "Only administrators can reconfigure server "
                              "(client: " + m_Client + ")";
            m_Server->RegisterAlert(eAccess, msg);
            NCBI_THROW(CNetStorageServerException, ePrivileges, msg);
        }
    }

    m_Server->GetClientRegistry().AppendType(m_Client,
                                             CNSTClient::eAdministrator);

    // Unconditionally reload the decryptor in case if the key files are
    // changed
    CNcbiEncrypt::Reload();

    CJsonNode           reply = CreateResponseMessage(
                                            common_args.m_SerialNumber);
    CNcbiApplication *  app = CNcbiApplication::Instance();

    // Validate the configuration file first without reloading the application
    // config.
    string              config_path = app->GetConfigPath();
    CNcbiIfstream       config_stream(config_path.c_str());
    CNcbiRegistry       candidate_reg(config_stream);
    vector<string>      config_warnings;
    CJsonNode           backend_conf;

    NSTValidateConfigFile(candidate_reg, config_warnings,
                          false);       // false -> no exc on bad port
    backend_conf = NSTGetBackendConfiguration(candidate_reg, m_Server,
                                              config_warnings);

    if (!config_warnings.empty()) {
        string      msg;
        string      alert_msg;
        for (vector<string>::const_iterator k = config_warnings.begin();
             k != config_warnings.end(); ++k) {
            ERR_POST(*k);
            if (!msg.empty()) {
                msg += "; ";
                alert_msg += "\n";
            }
            msg += *k;
            alert_msg += *k;
        }
        m_Server->RegisterAlert(eReconfigure, alert_msg);
        NCBI_THROW(CNetStorageServerException, eInvalidConfig,
                   "Configuration file is not well formed; " + msg);
    }

    // Check that the decryption of the sensitive items works
    SNetStorageServerParameters     params;
    string                          decrypt_warning;
    params.Read(candidate_reg, "server", decrypt_warning);

    if (!decrypt_warning.empty()) {
        ERR_POST(decrypt_warning);
        m_Server->RegisterAlert(eDecryptAdminNames, decrypt_warning);
        NCBI_THROW(CNetStorageServerException, eInvalidConfig, decrypt_warning);
    }


    // Here: the configuration has been validated and could be loaded

    bool                reloaded = app->ReloadConfig(
                                            CMetaRegistry::fReloadIfChanged);

    // The !m_Server->AnybodyCanReconfigure() part is needed because the
    // file might not be changed but the encryption keys could.
    if (!reloaded && !m_Server->AnybodyCanReconfigure()) {
        AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                      "Configuration file has not been changed, "
                      "RECONFIGURE ignored", kScopeLogic,
                      eConfigNotChangedWarning);
        x_SendSyncMessage(reply);
        x_PrintMessageRequestStop();
        return;
    }

    // Update the config file checksum in memory
    vector<string>  config_checksum_warnings;
    string          config_checksum = NST_GetConfigFileChecksum(
                            app->GetConfigPath(), config_checksum_warnings);
    if (config_checksum_warnings.empty()) {
        m_Server->SetRAMConfigFileChecksum(config_checksum);
        m_Server->SetDiskConfigFileChecksum(config_checksum);
    } else {
        for (vector<string>::const_iterator
                k = config_checksum_warnings.begin();
                k != config_checksum_warnings.end(); ++k)
            ERR_POST(*k);
    }


    // Reconfigurable at runtime:
    // [server]: logging, admin name list, max connections, network timeout
    // [metadata_conf]
    const CNcbiRegistry &           reg = app->GetConfig();

    CJsonNode   server_diff = m_Server->SetParameters(params, true);
    CJsonNode   metadata_diff = m_Server->ReadMetadataConfiguration(reg);
    CJsonNode   backend_diff = m_Server->GetBackendConfDiff(backend_conf);
    CJsonNode   db_diff = m_Server->GetDb().SetParameters(reg);

    m_Server->SetBackendConfiguration(backend_conf);

    m_Server->AcknowledgeAlert(eReconfigure, "NSTAcknowledge");
    m_Server->AcknowledgeAlert(eStartupConfig, "NSTAcknowledge");
    m_Server->AcknowledgeAlert(eConfigOutOfSync, "NSTAcknowledge");
    m_Server->SetAnybodyCanReconfigure(false);


    if (server_diff.IsNull() &&
        metadata_diff.IsNull() &&
        backend_diff.IsNull() &&
        db_diff.IsNull()) {
        if (m_ConnContext.NotNull())
             GetDiagContext().Extra().Print("accepted_changes", "none");
        AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                      "No changeable parameters were identified "
                      "in the new configuration file",
                      kScopeLogic, eConfigNotChangedWarning);
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
    if (!backend_diff.IsNull())
        total_changes.SetByKey("backend_conf", backend_diff);
    if (!db_diff.IsNull())
        total_changes.SetByKey("db_conf", db_diff);

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
        string      msg = "Only administrators can shutdown server";
        m_Server->RegisterAlert(eAccess, msg);
        NCBI_THROW(CNetStorageServerException, ePrivileges, msg);
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
    m_Server->GetClientRegistry().AppendType(m_Client,
                                             CNSTClient::eAdministrator);

    // First part is always there: in-memory clients
    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);
    reply.SetByKey("Clients", m_Server->GetClientRegistry().Serialize());

    // Second part might be here
    bool    db_access = (m_MetadataOption == eMetadataMonitoring ||
                         m_Server->InMetadataServices(m_Service));
    if (db_access) {
        try {
            vector<string>      names;
            int     status = m_Server->GetDb().ExecSP_GetClients(names,
                                                                 m_Timing);
            if (status != 0) {
                reply.SetString("DBClients", "MetadataAccessWarning");
                AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                              "Stored procedure return code is non zero",
                              kScopeLogic, eDatabaseWarning);
            } else {
                CJsonNode   db_clients_node(CJsonNode::NewArrayNode());
                for (vector<string>::const_iterator  k = names.begin();
                     k != names.end(); ++k)
                    db_clients_node.AppendString(*k);
                reply.SetByKey("DBClients", db_clients_node);
            }
        } catch (const exception &  ex) {
            reply.SetString("DBClients", "MetadataAccessWarning");
            AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                          "Error while getting a list of clients "
                          "in the metainfo DB: " + string(ex.what()),
                          kScopeStdException, eDatabaseWarning);
        } catch (...) {
            reply.SetString("DBClients", "MetadataAccessWarning");
            AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                          "Unknown error while getting a list of clients "
                          "in the metainfo DB",
                          kScopeUnknownException, eDatabaseWarning);
        }
    } else {
        reply.SetString("DBClients", "NoMetadataAccess");
    }

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessGetMetadataInfo(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_Server->GetClientRegistry().AppendType(m_Client,
                                             CNSTClient::eAdministrator);

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);

    reply.SetByKey("Services", m_Server->SerializeMetadataInfo());
    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessGetObjectInfo(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_Server->GetClientRegistry().AppendType(m_Client, CNSTClient::eReader);

    CJsonNode   reply = CreateResponseMessage(common_args.m_SerialNumber);
    bool        need_db_access = true;
    if (m_MetadataOption != eMetadataMonitoring)
        need_db_access = x_DetectMetaDBNeedOnGetObjectInfo(message);
    CDirectNetStorageObject     direct_object = x_GetObject(message);

    // First source of data - MS SQL database at hand
    if (need_db_access) {
        try {
            TNSTDBValue<CTime>  expiration;
            TNSTDBValue<CTime>  creation;
            TNSTDBValue<CTime>  obj_read;
            TNSTDBValue<CTime>  obj_write;
            TNSTDBValue<CTime>  attr_read;
            TNSTDBValue<CTime>  attr_write;
            TNSTDBValue<Int8>   read_count;
            TNSTDBValue<Int8>   write_count;
            TNSTDBValue<string> client_name;

            // The parameters are output only (from the SP POV) however the
            // integer values should be initialized to avoid valgrind complains
            read_count.m_Value = 0;
            write_count.m_Value = 0;
            int                 status = m_Server->GetDb().
                                    ExecSP_GetObjectFixedAttributes(
                                        direct_object.Locator().GetUniqueKey(),
                                        expiration, creation,
                                        obj_read, obj_write,
                                        attr_read, attr_write,
                                        read_count, write_count,
                                        client_name, m_Timing);

            if (status != 0) {
                // The record in the meta DB for the object is not found
                const string        no_metadata = "NoMetadataFound";
                reply.SetString("ExpirationTime", no_metadata);
                reply.SetString("CreationTime", no_metadata);
                reply.SetString("ObjectReadTime", no_metadata);
                reply.SetString("ObjectWriteTime", no_metadata);
                reply.SetString("AttrReadTime", no_metadata);
                reply.SetString("AttrWriteTime", no_metadata);
                reply.SetString("ObjectReadCount", no_metadata);
                reply.SetString("ObjectWriteCount", no_metadata);
                reply.SetString("ClientName", no_metadata);
            } else {
                // The record in the meta DB for the object is found
                if (expiration.m_IsNull)
                    reply.SetString("ExpirationTime", "NotSet");
                else {
                    if (expiration.m_Value < CurrentTime())
                        NCBI_THROW(CNetStorageServerException,
                                   eNetStorageObjectExpired, "Object expired");

                    reply.SetString("ExpirationTime",
                                    expiration.m_Value.AsString());
                }

                if (creation.m_IsNull)
                    reply.SetString("CreationTime", "NotSet");
                else
                    reply.SetString("CreationTime",
                                    creation.m_Value.AsString());
                if (obj_read.m_IsNull)
                    reply.SetString("ObjectReadTime", "NotSet");
                else
                    reply.SetString("ObjectReadTime",
                                    obj_read.m_Value.AsString());
                if (obj_write.m_IsNull)
                    reply.SetString("ObjectWriteTime", "NotSet");
                else
                    reply.SetString("ObjectWriteTime",
                                    obj_write.m_Value.AsString());
                if (attr_read.m_IsNull)
                    reply.SetString("AttrReadTime", "NotSet");
                else
                    reply.SetString("AttrReadTime",
                                    attr_read.m_Value.AsString());
                if (attr_write.m_IsNull)
                    reply.SetString("AttrWriteTime", "NotSet");
                else
                    reply.SetString("AttrWriteTime",
                                    attr_write.m_Value.AsString());
                if (read_count.m_IsNull)
                    reply.SetString("ObjectReadCount", "NotSet");
                else
                    reply.SetString("ObjectReadCount",
                                    NStr::NumericToString(read_count.m_Value));
                if (write_count.m_IsNull)
                    reply.SetString("ObjectWriteCount", "NotSet");
                else
                    reply.SetString("ObjectWriteCount",
                                    NStr::NumericToString(write_count.m_Value));
                if (client_name.m_IsNull)
                    reply.SetString("ClientName", "NotSet");
                else
                    reply.SetString("ClientName", client_name.m_Value);
            }
        } catch (const CNetStorageServerException &  ex) {
            if (ex.GetErrCode() == CNetStorageServerException::
                                                    eNetStorageObjectExpired)
                throw;

            // eDatabaseError => no connection or MS SQL error
            const string    warning = "MetadataAccessWarning";
            reply.SetString("ExpirationTime", warning);
            reply.SetString("CreationTime", warning);
            reply.SetString("ObjectReadTime", warning);
            reply.SetString("ObjectWriteTime", warning);
            reply.SetString("AttrReadTime", warning);
            reply.SetString("AttrWriteTime", warning);
            reply.SetString("ObjectReadCount", warning);
            reply.SetString("ObjectWriteCount", warning);
            reply.SetString("ClientName", warning);
            AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                          ex.what(), ex.GetClass(), eDatabaseWarning);
        } catch (const exception &  ex) {
            const string    warning = "MetadataAccessWarning";
            reply.SetString("ExpirationTime", warning);
            reply.SetString("CreationTime", warning);
            reply.SetString("ObjectReadTime", warning);
            reply.SetString("ObjectWriteTime", warning);
            reply.SetString("AttrReadTime", warning);
            reply.SetString("AttrWriteTime", warning);
            reply.SetString("ObjectReadCount", warning);
            reply.SetString("ObjectWriteCount", warning);
            reply.SetString("ClientName", warning);
            AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                          "Error while gettingobject fixed attributes: " +
                          string(ex.what()), kScopeStdException,
                          eDatabaseWarning);
        } catch (...) {
            const string    warning = "MetadataAccessWarning";
            reply.SetString("ExpirationTime", warning);
            reply.SetString("CreationTime", warning);
            reply.SetString("ObjectReadTime", warning);
            reply.SetString("ObjectWriteTime", warning);
            reply.SetString("AttrReadTime", warning);
            reply.SetString("AttrWriteTime", warning);
            reply.SetString("ObjectReadCount", warning);
            reply.SetString("ObjectWriteCount", warning);
            reply.SetString("ClientName", warning);
            AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                          "Unknown error while getting object fixed attributes",
                          kScopeUnknownException, eDatabaseWarning);
        }
    } else {
        const string    no_access = "NoMetadataAccess";
        reply.SetString("ExpirationTime", no_access);
        reply.SetString("CreationTime", no_access);
        reply.SetString("ObjectReadTime", no_access);
        reply.SetString("ObjectWriteTime", no_access);
        reply.SetString("AttrReadTime", no_access);
        reply.SetString("AttrWriteTime", no_access);
        reply.SetString("ObjectReadCount", no_access);
        reply.SetString("ObjectWriteCount", no_access);
        reply.SetString("ClientName", no_access);
    }


    // Second source of data - remote object info
    CNSTPreciseTime     start = CNSTPreciseTime::Current();
    try {
        CJsonNode         object_info = direct_object.GetInfo().ToJSON();
        if (m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI GetInfo", start);
        start = 0.0;

        for (CJsonIterator it = object_info.Iterate(); it; ++it) {
            string      key = it.GetKey();
            if (key != "CreationTime")
                reply.SetByKey(it.GetKey(), it.GetNode());
        }
    } catch (const CNetStorageException &  ex) {
        if (ex.GetErrCode() == CNetStorageException::eNotExists)
            AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                          "Error while getting remote object info: " +
                          string(ex.what()), ex.GetClass(),
                          eRemoteObjectInfoWarning);
        if (double(start) != 0.0 && m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI GetInfo exception", start);
    } catch (const exception &  ex) {
        AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                      "Error while getting remote object info: " +
                      string(ex.what()), kScopeStdException,
                      eRemoteObjectInfoWarning);
        if (double(start) != 0.0 && m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI GetInfo exception", start);
    } catch (...) {
        AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                      "Unknown error while getting remote object info",
                      kScopeUnknownException, eRemoteObjectInfoWarning);
        if (double(start) != 0.0 && m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI GetInfo exception", start);
    }

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessGetAttrList(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_Server->GetClientRegistry().AppendType(m_Client, CNSTClient::eReader);
    x_CheckNonAnonymousClient();

    if (!message.HasKey("ObjectLoc") && !message.HasKey("UserKey"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "GETATTRLIST message must have ObjectLoc or UserKey. "
                   "None of them was found.");

    if (m_MetadataOption == eMetadataDisabled)
        NCBI_THROW(CNetStorageServerException, eInvalidMetaInfoRequest,
                   "DB access is restricted in HELLO");

    if (m_MetadataOption == eMetadataNotSpecified ||
        m_MetadataOption == eMetadataRequired)
        x_ValidateWriteMetaDBAccess(message);

    if (m_MetadataOption != eMetadataMonitoring)
        x_CreateClient();

    CJsonNode   reply = CreateResponseMessage(common_args.m_SerialNumber);
    CDirectNetStorageObject     direct_object = x_GetObject(message);
    string                      object_key =
                                        direct_object.Locator().GetUniqueKey();
    x_CheckObjectExpiration(object_key, true, reply);

    vector<string>  attr_names;
    int             status = m_Server->GetDb().ExecSP_GetAttributeNames(
                                        object_key, attr_names, m_Timing);

    if (status == 0) {
        // Everything is fine, the attribute is found
        CJsonNode       names(CJsonNode::NewArrayNode());
        for (vector<string>::const_iterator  k = attr_names.begin();
             k != attr_names.end(); ++k)
            names.AppendString(*k);
        reply.SetByKey("AttributeNames", names);
    } else if (status == -1) {
        // Object is not found
        NCBI_THROW(CNetStorageServerException, eNetStorageObjectNotFound,
                   "NetStorage object is not found");
    } else {
        // Unknown status
        NCBI_THROW(CNetStorageServerException, eInternalError,
                   "Unknown GetAttributeNames status");
    }

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessGetClientObjects(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_Server->GetClientRegistry().AppendType(m_Client, CNSTClient::eReader);
    x_CheckNonAnonymousClient();

    if (!message.HasKey("ClientName"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "GETCLIENTOBJECTS message must have ClientName.");

    TNSTDBValue<Int8>  limit;
    limit.m_IsNull = true;
    limit.m_Value = 0;
    if (message.HasKey("Limit")) {
        try {
            limit.m_Value = message.GetInteger("Limit");
            limit.m_IsNull = false;
        } catch (...) {
            NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                       "GETCLIENTOBJECTS message Limit argument "
                       "must be an integer.");
        }
        if (limit.m_Value <= 0)
            NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                       "GETCLIENTOBJECTS message Limit argument "
                       "must be > 0.");
    }

    if (m_MetadataOption == eMetadataDisabled)
        NCBI_THROW(CNetStorageServerException, eInvalidMetaInfoRequest,
                   "DB access is restricted in HELLO");

    if (m_MetadataOption == eMetadataNotSpecified ||
        m_MetadataOption == eMetadataRequired)
        x_ValidateWriteMetaDBAccess(message);

    if (m_MetadataOption != eMetadataMonitoring)
        x_CreateClient();

    CJsonNode   reply = CreateResponseMessage(common_args.m_SerialNumber);

    vector<string>  locators;
    Int8            total_client_objects;
    int             status = m_Server->GetDb().ExecSP_GetClientObjects(
                                        message.GetString("ClientName"), limit,
                                        total_client_objects, locators,
                                        m_Timing);

    if (status == 0) {
        // Everything is fine, the attribute is found
        CJsonNode       locators_node(CJsonNode::NewArrayNode());
        for (vector<string>::const_iterator  k = locators.begin();
             k != locators.end(); ++k)
            locators_node.AppendString(*k);
        reply.SetByKey("ObjectLocators", locators_node);
        reply.SetInteger("TotalClientObjects", total_client_objects);
    } else if (status == -1) {
        // Client is not found
        NCBI_THROW(CNetStorageServerException, eNetStorageClientNotFound,
                   "NetStorage client is not found");
    } else {
        // Unknown status
        NCBI_THROW(CNetStorageServerException, eInternalError,
                   "Unknown GetClientObjects status");
    }

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessGetAttr(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_Server->GetClientRegistry().AppendType(m_Client, CNSTClient::eReader);
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

    if (m_MetadataOption == eMetadataDisabled)
        NCBI_THROW(CNetStorageServerException, eInvalidMetaInfoRequest,
                   "DB access is restricted in HELLO");

    if (m_MetadataOption == eMetadataNotSpecified ||
        m_MetadataOption == eMetadataRequired)
        x_ValidateWriteMetaDBAccess(message);


    if (m_MetadataOption != eMetadataMonitoring)
        x_CreateClient();

    CJsonNode   reply = CreateResponseMessage(common_args.m_SerialNumber);
    CDirectNetStorageObject     direct_object = x_GetObject(message);
    string                      object_key =
                                        direct_object.Locator().GetUniqueKey();
    x_CheckObjectExpiration(object_key, true, reply);

    string      value;
    int         status = m_Server->GetDb().ExecSP_GetAttribute(
                                        object_key, attr_name,
                                        m_MetadataOption != eMetadataMonitoring,
                                        value, m_Timing);

    if (status == 0) {
        // Everything is fine, the attribute is found
        reply.SetString("AttrValue", value);
    } else if (status == -1) {
        // Object is not found
        NCBI_THROW(CNetStorageServerException, eNetStorageObjectNotFound,
                   "NetStorage object is not found");
    } else if (status == -2) {
        // Attribute is not found
        NCBI_THROW(CNetStorageServerException, eNetStorageAttributeNotFound,
                   "NetStorage attribute is not found");
    } else if (status == -3) {
        // Attribute value is not found
        NCBI_THROW(CNetStorageServerException,
                   eNetStorageAttributeValueNotFound,
                   "NetStorage attribute value is not found");
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
    m_Server->GetClientRegistry().AppendType(m_Client, CNSTClient::eWriter);
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

    if (m_MetadataOption == eMetadataDisabled ||
        m_MetadataOption == eMetadataMonitoring)
        NCBI_THROW(CNetStorageServerException, eInvalidMetaInfoRequest,
                   "State changing operations are restricted in HELLO");

    // The only options left are NotSpecified and Required
    x_ValidateWriteMetaDBAccess(message);

    x_CreateClient();

    CJsonNode   reply = CreateResponseMessage(common_args.m_SerialNumber);
    CDirectNetStorageObject     direct_object = x_GetObject(message);
    string                      object_key =
                                        direct_object.Locator().GetUniqueKey();
    x_CheckObjectExpiration(object_key, true, reply);

    string      value = message.GetString("AttrValue");

    // The SP will throw an exception if an error occured
    int         status = m_Server->GetDb().ExecSP_AddAttribute(
                                          object_key,
                                          attr_name, value,
                                          m_DBClientID, m_Timing);
    if (status == -1) {
        // Object is not found
        NCBI_THROW(CNetStorageServerException, eNetStorageObjectNotFound,
                   "NetStorage object is not found");
    } else if (status != 0) {
        // Unknown status
        NCBI_THROW(CNetStorageServerException, eInternalError,
                   "Unknown AddAttribute status");
    }

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessDelAttr(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_Server->GetClientRegistry().AppendType(m_Client, CNSTClient::eWriter);
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

    if (m_MetadataOption == eMetadataDisabled ||
        m_MetadataOption == eMetadataMonitoring)
        NCBI_THROW(CNetStorageServerException, eInvalidMetaInfoRequest,
                   "State changing operations are restricted in HELLO");

    // The only options left are NotSpecified and Required
    x_ValidateWriteMetaDBAccess(message);

    x_CreateClient();

    CJsonNode   reply = CreateResponseMessage(common_args.m_SerialNumber);
    CDirectNetStorageObject     direct_object = x_GetObject(message);
    string                      object_key =
                                        direct_object.Locator().GetUniqueKey();
    x_CheckObjectExpiration(object_key, true, reply);

    int         status = m_Server->GetDb().ExecSP_DelAttribute(
                                            object_key, attr_name, m_Timing);

    if (status == -1) {
        // Object is not found
        AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                      "NetStorage object is not found",
                      kScopeLogic, eObjectNotFoundWarning);
    } else if (status == -2) {
        // Attribute is not found
        AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                      "NetStorage attribute is not found",
                      kScopeLogic, eAttributeNotFoundWarning);
    } else if (status == -3) {
        // Attribute value is not found
        AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                      "NetStorage attribute value is not found",
                      kScopeLogic, eAttributeValueNotFoundWarning);
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
    m_Server->GetClientRegistry().AppendType(m_Client, CNSTClient::eWriter);
    x_CheckNonAnonymousClient();

    if (m_MetadataOption == eMetadataMonitoring)
        NCBI_THROW(CNetStorageServerException, eInvalidMetaInfoRequest,
                   "State changing operations are restricted in HELLO");

    TNetStorageFlags    flags = ExtractStorageFlags(message);
    SICacheSettings     icache_settings = ExtractICacheSettings(message);

    x_CheckICacheSettings(icache_settings);

    // The DB update depends on the command flags, on the HELLO
    // metadata option and may depend on the HELLO service
    m_WriteCreateNeedMetaDBUpdate = x_DetectMetaDBNeedOnCreate(flags);

    if (m_WriteCreateNeedMetaDBUpdate) {
        // Meta information is required so check the DB
        x_CreateClient();
        m_Server->GetDb().ExecSP_GetNextObjectID(m_DBObjectID, m_Timing);
    } else {
        m_DBObjectID = 0;
    }

    // Create the object stream depending on settings
    m_ObjectBeingWritten = x_CreateObjectStream(icache_settings, flags);

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);
    string          locator = m_ObjectBeingWritten.GetLoc();

    reply.SetString("ObjectLoc", locator);
    x_SendSyncMessage(reply);

    if (m_ConnContext.NotNull() && !message.HasKey("ObjectLoc")) {
        GetDiagContext().Extra()
            .Print("ObjectLoc", locator)
            .Print("ObjectKey", m_ObjectBeingWritten.Locator().GetUniqueKey());
    }

    // Inform the message receiving loop that raw data are to follow
    m_ReadMode = eReadRawData;
    m_DataMessageSN = common_args.m_SerialNumber;
    m_CreateRequest = true;
    m_ObjectSize = 0;
    m_Server->GetClientRegistry().AddObjectsWritten(m_Client, 1);
}


void
CNetStorageHandler::x_ProcessWrite(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_Server->GetClientRegistry().AppendType(m_Client, CNSTClient::eWriter);
    x_CheckNonAnonymousClient();

    if (!message.HasKey("ObjectLoc") && !message.HasKey("UserKey"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "WRITE message must have ObjectLoc or UserKey. "
                   "None of them was found.");

    if (m_MetadataOption == eMetadataMonitoring)
        NCBI_THROW(CNetStorageServerException, eInvalidMetaInfoRequest,
                   "State changing operations are restricted in HELLO");

    // If it was eMetadataDisabled then updates are not required
    m_WriteCreateNeedMetaDBUpdate = false;
    if (m_MetadataOption == eMetadataRequired ||
        m_MetadataOption == eMetadataNotSpecified)
        m_WriteCreateNeedMetaDBUpdate = x_DetectMetaDBNeedUpdate(
                                                message, m_WriteServiceProps);

    if (m_WriteCreateNeedMetaDBUpdate) {
        // Meta information is required so check the DB
        x_CreateClient();
    }

    CJsonNode           reply = CreateResponseMessage(
                                                common_args.m_SerialNumber);
    m_ObjectBeingWritten = x_GetObject(message);
    const CNetStorageObjectLoc &    object_locator =
                                                m_ObjectBeingWritten.Locator();
    string              object_key = object_locator.GetUniqueKey();
    string              locator = object_locator.GetLocator();
    TNSTDBValue<CTime>  object_expiration;
    if (m_WriteCreateNeedMetaDBUpdate)
        m_WriteObjectExpiration = x_CheckObjectExpiration(object_key,
                                                          true, reply);

    reply.SetString("ObjectLoc", locator);
    x_SendSyncMessage(reply);

    if (m_ConnContext.NotNull() && !message.HasKey("ObjectLoc")) {
        GetDiagContext().Extra()
            .Print("ObjectLoc", locator)
            .Print("ObjectKey", object_key);
    }

    // Inform the message receiving loop that raw data are to follow
    m_ReadMode = eReadRawData;
    m_DataMessageSN = common_args.m_SerialNumber;
    m_CreateRequest = false;
    m_ObjectSize = 0;
    m_Server->GetClientRegistry().AddObjectsWritten(m_Client, 1);
}



void
CNetStorageHandler::x_ProcessRead(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_Server->GetClientRegistry().AppendType(m_Client, CNSTClient::eReader);
    x_CheckNonAnonymousClient();

    if (!message.HasKey("ObjectLoc") && !message.HasKey("UserKey"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "READ message must have ObjectLoc or UserKey. "
                   "None of them was found.");

    // For eMetadataMonitoring and eMetadataDisabled there must be no updates
    // in the meta info DB
    bool                    need_meta_db_update = false;
    CNSTServiceProperties   service_properties;
    if (m_MetadataOption == eMetadataRequired ||
        m_MetadataOption == eMetadataNotSpecified)
        need_meta_db_update = x_DetectMetaDBNeedUpdate(message,
                                                       service_properties);


    if (need_meta_db_update) {
        // Meta information is required so check the DB
        x_CreateClient();
    }

    CJsonNode           reply = CreateResponseMessage(
                                                common_args.m_SerialNumber);
    CDirectNetStorageObject         direct_object = x_GetObject(message);
    const CNetStorageObjectLoc &    object_locator = direct_object.Locator();
    string                          object_key = object_locator.GetUniqueKey();
    string                          locator = object_locator.GetLocator();

    TNSTDBValue<CTime>  object_expiration;
    if (need_meta_db_update)
        object_expiration = x_CheckObjectExpiration(object_key, true, reply);
    else if (m_MetadataOption == eMetadataMonitoring)
        x_CheckObjectExpiration(object_key, false, reply);

    x_SendSyncMessage(reply);

    m_Server->GetClientRegistry().AddObjectsRead(m_Client, 1);

    char                buffer[kReadBufferSize];
    size_t              bytes_read;
    Int8                total_bytes = 0;
    CNSTPreciseTime     start = 0.0;

    try {
        while (!direct_object.Eof()) {
            start = CNSTPreciseTime::Current();
            bytes_read = direct_object.Read(buffer, sizeof(buffer));
            if (m_Server->IsLogTimingNSTAPI())
                m_Timing.Append("NetStorageAPI read", start);
            start = 0.0;

            m_UTTPWriter.SendChunk(buffer, bytes_read, false);

            if (x_SendOverUTTP() != eIO_Success)
                return;

            m_Server->GetClientRegistry().AddBytesRead(m_Client, bytes_read);
            total_bytes += bytes_read;

            if (m_CmdContext.NotNull())
                m_CmdContext->SetBytesRd(total_bytes);
        }

        start = CNSTPreciseTime::Current();
        direct_object.Close();
        if (m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI Close", start);
        start = 0.0;

        reply = CreateResponseMessage(common_args.m_SerialNumber);
    }
    catch (const CException &  ex) {
        // Prevent creation of an object in the database if there was none.
        // NB: the object TTL will not be updated if there was an object
        need_meta_db_update = false;
        reply = CreateErrorResponseMessage(common_args.m_SerialNumber,
                        NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                        string("Object read error: ") + ex.what(),
                        ex.GetClass(),
                        CNetStorageServerException::eReadError);
        ERR_POST(ex);
        if (double(start) != 0.0 && m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI read (exception)", start);
        if (!x_PrintTimingIsOn() && !m_Timing.Empty())
            ERR_POST("Object read error timing information: " +
                     m_Timing.Serialize());
    }
    catch (...) {
        if (double(start) != 0.0 && m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI read (unknown exception)", start);
        throw;
    }

    m_UTTPWriter.SendControlSymbol('\n');

    if (x_SendOverUTTP() != eIO_Success)
        return;

    if (need_meta_db_update) {
        m_Server->GetDb().ExecSP_UpdateObjectOnRead(
                object_key,
                locator, total_bytes, m_DBClientID,
                service_properties.GetTTL(),
                service_properties.GetProlongOnRead(),
                object_expiration, m_Timing
                );
    }

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessDelete(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_Server->GetClientRegistry().AppendType(m_Client, CNSTClient::eWriter);
    x_CheckNonAnonymousClient();

    if (!message.HasKey("ObjectLoc") && !message.HasKey("UserKey"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "DELETE message must have ObjectLoc or UserKey. "
                   "None of them was found.");

    if (m_MetadataOption == eMetadataMonitoring)
        NCBI_THROW(CNetStorageServerException, eInvalidMetaInfoRequest,
                   "State changing operations are restricted in HELLO");

    CNSTServiceProperties       service_properties;

    // If it was eMetadataDisabled then updates are not required
    bool        need_meta_db_update = false;
    if (m_MetadataOption == eMetadataRequired ||
        m_MetadataOption == eMetadataNotSpecified)
        need_meta_db_update = x_DetectMetaDBNeedUpdate(message,
                                                       service_properties);

    CDirectNetStorageObject     direct_object = x_GetObject(message);
    int                         status = 0;

    if (need_meta_db_update) {
        // Meta information is required so delete from the DB first
        status = m_Server->GetDb().ExecSP_RemoveObject(
                    direct_object.Locator().GetUniqueKey(), m_Timing);
    }

    ENetStorageRemoveResult     result = eNSTRR_NotFound;
    CNSTPreciseTime             start = CNSTPreciseTime::Current();
    try {
        result = direct_object.Remove();
        if (m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI Remove", start);
    } catch (...) {
        if (m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI Remove exception", start);
        throw;
    }

    m_Server->GetClientRegistry().AddObjectsDeleted(m_Client, 1);

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);

    // Explicitly tell if the object:
    // - was not found in the backend storage
    // - was found and deleted in the backend storage
    reply.SetBoolean("NotFound", result == eNSTRR_NotFound);

    if (need_meta_db_update && status == -1) {
        // Stored procedure return -1 if the object is not found in the meta DB
        AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                      "Object not found in meta info DB",
                      kScopeLogic, eObjectNotFoundWarning);
    }

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessRelocate(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_Server->GetClientRegistry().AppendType(m_Client, CNSTClient::eWriter);
    x_CheckNonAnonymousClient();

    // Take the arguments
    if (!message.HasKey("NewLocation")) {
        NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                   "NewLocation argument is not found");
    }

    if (m_MetadataOption == eMetadataMonitoring)
        NCBI_THROW(CNetStorageServerException, eInvalidMetaInfoRequest,
                   "State changing operations are restricted in HELLO");

    CNSTServiceProperties       service_properties;

    // If it was eMetadataDisabled then updates are not required
    bool        need_meta_db_update = false;
    if (m_MetadataOption == eMetadataRequired ||
        m_MetadataOption == eMetadataNotSpecified)
        need_meta_db_update = x_DetectMetaDBNeedUpdate(message,
                                                       service_properties);


    CJsonNode           new_location = message.GetByKey("NewLocation");
    TNetStorageFlags    new_location_flags = ExtractStorageFlags(new_location);
    SICacheSettings     new_location_icache_settings =
                                ExtractICacheSettings(new_location);

    x_CheckICacheSettings(new_location_icache_settings);

    if (need_meta_db_update) {
        // Meta information is required so check the DB
        x_CreateClient();
    }

    CJsonNode       reply = CreateResponseMessage(common_args.m_SerialNumber);
    CDirectNetStorageObject     direct_object = x_GetObject(message);
    string                      object_key =
                                        direct_object.Locator().GetUniqueKey();
    if (need_meta_db_update)
        x_CheckObjectExpiration(object_key, true, reply);

    string              new_object_loc;
    CNSTPreciseTime     start = CNSTPreciseTime::Current();
    try {
        new_object_loc = direct_object.Relocate(new_location_flags);
        if (m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI Relocate", start);
    } catch (...) {
        if (m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI Relocate exception", start);
        throw;
    }

    if (need_meta_db_update) {
        m_Server->GetDb().ExecSP_UpdateObjectOnRelocate(
                                    object_key,
                                    new_object_loc,
                                    m_DBClientID, m_Timing);
    }

    m_Server->GetClientRegistry().AddObjectsRelocated(m_Client, 1);

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
    // It was decided to check the object expiration
    m_Server->GetClientRegistry().AppendType(m_Client, CNSTClient::eReader);

    CJsonNode                   reply = CreateResponseMessage(
                                                    common_args.m_SerialNumber);
    CDirectNetStorageObject     direct_object = x_GetObject(message);

    if (m_MetadataOption == eMetadataMonitoring)
        x_CheckObjectExpiration(direct_object.Locator().GetUniqueKey(),
                                false, reply);
    else if (x_DetectMetaDBNeedOnGetObjectInfo(message))
        x_CheckObjectExpiration(direct_object.Locator().GetUniqueKey(),
                                true, reply);

    bool                exists = false;
    CNSTPreciseTime     start = CNSTPreciseTime::Current();
    try {
        exists = direct_object.Exists();
        if (m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI Exists", start);
    } catch (...) {
        if (m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI Exists exception", start);
        throw;
    }

    reply.SetBoolean("Exists", exists);
    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessGetSize(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    // It was decided to check the object expiration
    m_Server->GetClientRegistry().AppendType(m_Client, CNSTClient::eReader);

    CJsonNode         reply = CreateResponseMessage(common_args.m_SerialNumber);
    CDirectNetStorageObject     direct_object = x_GetObject(message);
    string                      object_key =
                                        direct_object.Locator().GetUniqueKey();

    if (m_MetadataOption == eMetadataMonitoring)
        x_CheckObjectExpiration(object_key, false, reply);
    else if (x_DetectMetaDBNeedOnGetObjectInfo(message))
        x_CheckObjectExpiration(object_key, true, reply);

    Uint8               object_size = 0;
    CNSTPreciseTime     start = CNSTPreciseTime::Current();
    try {
        object_size = direct_object.GetSize();
        if (m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI GetSize", start);
    } catch (...) {
        if (m_Server->IsLogTimingNSTAPI())
            m_Timing.Append("NetStorageAPI GetSize exception", start);
        throw;
    }

    reply.SetInteger("Size", object_size);
    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessSetExpTime(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_Server->GetClientRegistry().AppendType(m_Client, CNSTClient::eWriter);
    x_CheckNonAnonymousClient();

    if (!message.HasKey("TTL"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "Mandatory field 'TTL' is missed");

    string                  arg_val = message.GetString("TTL");
    TNSTDBValue<CTimeSpan>  ttl;

    ttl.m_IsNull = NStr::EqualNocase(arg_val, "infinity");

    if (!ttl.m_IsNull) {
        try {
            CTimeFormat     format("dTh:m:s");
            ttl.m_Value = CTimeSpan(arg_val, format);
        } catch (const exception &  ex) {
            NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                       "TTL format is not recognized: " + string(ex.what()));
        } catch (...) {
            NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                       "TTL format is not recognized");
        }

        if (ttl.m_Value.GetSign() == eNegative || ttl.m_Value.IsEmpty())
            NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                       "TTL must be > 0");
    }

    if (!message.HasKey("ObjectLoc") && !message.HasKey("UserKey"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "SETEXPTIME message must have ObjectLoc or UserKey. "
                   "None of them was found.");


    // The complicated logic of what and how should be done is described in the
    // ticket CXX-7361 (see the attached pdf)

    if (m_MetadataOption == eMetadataMonitoring)
        NCBI_THROW(CNetStorageServerException, eInvalidMetaInfoRequest,
                   "SETEXPTIME could not be used when the HELLO metadata "
                   "option is set to Monitoring");

    // Detect if the MS SQL update is required
    bool        db_update = true;
    if (m_MetadataOption == eMetadataDisabled)
        db_update = false;
    else {
        CNSTServiceProperties       props;  // not used here
        db_update = x_DetectMetaDBNeedUpdate(message, props);
    }

    CJsonNode   reply = CreateResponseMessage(common_args.m_SerialNumber);
    CDirectNetStorageObject     direct_object = x_GetObject(message);
    // Part I. Update MS SQL if needed
    bool        db_error = false;
    if (db_update) {
        string  object_key = direct_object.Locator().GetUniqueKey();

        // true => there will be an exception, no filling of the reply message
        x_CheckObjectExpiration(object_key, true, reply);
        int         status = 0;
        try {
            x_CreateClient();

            // The SP will throw an exception if an error occured
            status = m_Server->GetDb().ExecSP_SetExpiration(object_key, ttl,
                                                            m_Timing);
        } catch (const exception &  ex) {
            AppendError(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                        ex.what(), kScopeStdException,
                        CNetStorageServerException::eDatabaseError);
            db_error = true;
        } catch (...) {
            AppendError(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                        "Unknown metainfo DB update error",
                        kScopeUnknownException,
                        CNetStorageServerException::eDatabaseError);
            db_error = true;
        }

        // It was decided that we do not continue in case of logical errors in
        // the meta info db
        if (db_error == false ) {
            if (status == -1) {
                // Object is not found
                NCBI_THROW(CNetStorageServerException,
                           eNetStorageObjectNotFound,
                           "NetStorage object is not found");
            } else if (status != 0) {
                // Unknown status
                NCBI_THROW(CNetStorageServerException, eInternalError,
                           "Unknown SetExpiration status");
            }
        }

    }

    // Part II. Call the remote object SetExpiration(...)
    try {
        CTimeout    timeout;
        if (ttl.m_IsNull)
            timeout.Set(CTimeout::eInfinite);
        else
            timeout.Set(ttl.m_Value);

        direct_object.SetExpiration(timeout);
    } catch (const CNetStorageException &  ex) {
        if (ex.GetErrCode() == CNetStorageException::eNotSupported) {
            // Thanks to the CNetStorageException interface: there is no way
            // to test if the SetExpiration() is supported. Here it is not an
            // error, it is just that it makes no sense to call SetExpiration()
            // for the storage.
            if (db_error)
                AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                              ex.what(), ex.GetClass(),
                              eRemoteObjectSetExpirationWarning);
        } else {
            // That's a real problem of setting the object expiration
            if (db_update && db_error)
                AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                              ex.what(), ex.GetClass(),
                              eRemoteObjectSetExpirationWarning);
            else
                AppendError(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                            ex.what(), ex.GetClass(),
                            CNetStorageServerException::eStorageError);
        }
    } catch (const exception &  ex) {
        if (db_update && db_error)
            AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                          ex.what(), kScopeStdException,
                          eRemoteObjectSetExpirationWarning);
        else
            AppendError(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                        ex.what(), kScopeStdException,
                        CNetStorageServerException::eStorageError);
    } catch (...) {
        const string    msg = "Unknown remote storage SetExpiration error";
        if (db_update && db_error)
            AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                          msg, kScopeUnknownException,
                          eRemoteObjectSetExpirationWarning);
        else
            AppendError(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                        msg, kScopeUnknownException,
                        CNetStorageServerException::eStorageError);
    }

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


void
CNetStorageHandler::x_ProcessLockFTPath(
                        const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args)
{
    m_Server->GetClientRegistry().AppendType(m_Client, CNSTClient::eWriter);
    x_CheckNonAnonymousClient();

    if (!message.HasKey("ObjectLoc") && !message.HasKey("UserKey"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "LOCKFTPATH message must have ObjectLoc or UserKey. "
                   "None of them was found.");

    if (!message.HasKey("On"))
        NCBI_THROW(CNetStorageServerException, eMandatoryFieldsMissed,
                   "LOCKFTPATH message must have the On field.");

    CJsonNode               on = message.GetByKey("On");
    CJsonNode::ENodeType    node_type = on.GetNodeType();
    if (node_type != CJsonNode::eBoolean)
        NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                   "Invalid type of the On field. Boolean is expected.");

    if (m_MetadataOption == eMetadataMonitoring)
        NCBI_THROW(CNetStorageServerException, eInvalidMetaInfoRequest,
                   "LOCKFTPATH could not be used when the HELLO metadata "
                   "option is set to Monitoring");

    CJsonNode         reply = CreateResponseMessage(common_args.m_SerialNumber);
    CDirectNetStorageObject     direct_object = x_GetObject(message);
    string                      object_key =
                                        direct_object.Locator().GetUniqueKey();

    if (x_DetectMetaDBNeedOnGetObjectInfo(message))
        x_CheckObjectExpiration(object_key, true, reply);


    CMessageListener_Basic      warnings_listener;  // for collecting warnings
    IMessageListener::PushListener(warnings_listener);

    CMessageListenerResetter    listener_resetter;  // for reliable popping the
                                                    // listener


    CNSTPreciseTime     start = CNSTPreciseTime::Current();
    try {
        if (on.AsBoolean()) {
            string  lock_path = direct_object.FileTrack_Path_Lock();
            reply.SetString("Path", lock_path);
            if (m_Server->IsLogTimingNSTAPI())
                m_Timing.Append("NetStorageAPI FileTrack_Path_Lock", start);
        } else {
            direct_object.FileTrack_Path_Unlock();
            if (m_Server->IsLogTimingNSTAPI())
                m_Timing.Append("NetStorageAPI FileTrack_Path_Unlock", start);
        }
    } catch (...) {
        if (m_Server->IsLogTimingNSTAPI()) {
            if (on.AsBoolean())
                m_Timing.Append("NetStorageAPI FileTrack_Path_Lock exception",
                                start);
            else
                m_Timing.Append("NetStorageAPI FileTrack_Path_Unlock exception",
                                start);
        }
        throw;
    }

    // Append warnings if so
    size_t      warn_count = warnings_listener.Count();
    for (size_t  k = 0; k < warn_count; ++k) {
        const IMessage &    warn = warnings_listener.GetMessage(k);

        // As agreed when CXX-7622 is discussed, all the collected messages are
        // warnings, regardless of their severity. The errors are supposed to
        // be reported via the C++ exceptions mechanism.

        AppendWarning(reply, warn.GetCode(), warn.GetText(),
                      kScopeIMessage, warn.GetSubCode());
    }

    x_SendSyncMessage(reply);
    x_PrintMessageRequestStop();
}


CDirectNetStorageObject
CNetStorageHandler::x_GetObject(const CJsonNode &  message)
{
    CNcbiApplication *  app = CNcbiApplication::Instance();

    if (message.HasKey("ObjectLoc")) {
        string      object_loc = message.GetString("ObjectLoc");
        x_CheckObjectLoc(object_loc);

        try {
            // There could be a decryption exception so there is this try {}
            m_Server->ResetDecryptCacheIfNeed();
            CDirectNetStorage   storage(
                              app->GetConfig(),
                              m_Service,
                              m_Server->GetCompoundIDPool(), kEmptyStr);
            m_Server->ReportNetStorageAPIDecryptSuccess();

            CDirectNetStorageObject object(storage.Open(object_loc));

            if (m_ConnContext.NotNull())
                GetDiagContext().Extra()
                    .Print("ObjectKey", object.Locator().GetUniqueKey());

            return object;
        } catch (const CRegistryException &  ex) {
            if (ex.GetErrCode() == CRegistryException::eDecryptionFailed)
                m_Server->RegisterNetStorageAPIDecryptError(ex.what());
            throw;
        }
    }

    // Take the arguments
    SICacheSettings     icache_settings;
    SUserKey            user_key;
    TNetStorageFlags    flags;

    x_GetStorageParams(message, &icache_settings, &user_key, &flags);

    try {
        // There could be a decryption exception so there is this try {}
        m_Server->ResetDecryptCacheIfNeed();
        CDirectNetStorageByKey    storage(app->GetConfig(), m_Service,
                                          m_Server->GetCompoundIDPool(),
                                          user_key.m_AppDomain);
        m_Server->ReportNetStorageAPIDecryptSuccess();

        CDirectNetStorageObject   object(storage.Open(user_key.m_UniqueID,
                                                      flags));

        // Log if needed
        if (m_ConnContext.NotNull()) {
            GetDiagContext().Extra()
                .Print("ObjectKey", object.Locator().GetUniqueKey());
        }

        return object;
    } catch (const CRegistryException &  ex) {
        if (ex.GetErrCode() == CRegistryException::eDecryptionFailed)
            m_Server->RegisterNetStorageAPIDecryptError(ex.what());
        throw;
    }
}


void
CNetStorageHandler::x_CheckNonAnonymousClient(void) const
{
    if (m_Client.empty()) {
        NCBI_THROW(CNetStorageServerException, eHelloRequired,
                   "Anonymous client cannot perform "
                   "I/O operations with objects");
    }
}


void
CNetStorageHandler::x_CheckObjectLoc(const string &  object_loc) const
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


CDirectNetStorageObject
CNetStorageHandler::x_CreateObjectStream(
                    const SICacheSettings &  icache_settings,
                    TNetStorageFlags         flags)
{
    try {
        // There could be a decryption exception so there is this try {}
        CNcbiApplication *  app = CNcbiApplication::Instance();

        m_Server->ResetDecryptCacheIfNeed();
        CDirectNetStorage   net_storage(app->GetConfig(), m_Service,
                                        m_Server->GetCompoundIDPool(),
                                        icache_settings.m_CacheName);
        m_Server->ReportNetStorageAPIDecryptSuccess();

        return net_storage.Create(m_Service, m_DBObjectID, flags);
    } catch (const CRegistryException &  ex) {
        if (ex.GetErrCode() == CRegistryException::eDecryptionFailed)
            m_Server->RegisterNetStorageAPIDecryptError(ex.what());
        throw;
    }
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
            CNSTPreciseTime     start = CNSTPreciseTime::Current();
            EIO_Status result =
                GetSocket().Write(output_buffer, output_buffer_size, &written);
            if (m_Server->IsLogTimingClientSocket())
                m_Timing.Append("Client socket write (" +
                                NStr::NumericToString(output_buffer_size) + ")",
                                start);
            if (result != eIO_Success) {
                // Error writing to the socket. Log what we can and close the
                // connection.
                x_OnSocketWriteError(result, written,
                                     output_buffer, output_buffer_size);
                return result;
            }
        }
    } while (m_UTTPWriter.NextOutputBuffer());

    return eIO_Success;
}



EMetadataOption
CNetStorageHandler::x_ConvertMetadataArgument(const CJsonNode &  message) const
{
    if (!message.HasKey("Metadata"))
        return eMetadataNotSpecified;

    string  value = message.GetString("Metadata");
    value = NStr::TruncateSpaces(value);
    value = NStr::ToLower(value);
    EMetadataOption result = g_IdToMetadataOption(value);

    if (result == eMetadataUnknown) {
        vector<string>      valid = g_GetSupportedMetadataOptions();
        string              message = "Optional field 'Metadata' value is not "
                                      "recognized. Supported values are: ";
        for (vector<string>::const_iterator  k = valid.begin();
             k != valid.end(); ++k) {
            if (k != valid.begin())
                message += ", ";
            message += *k;
        }
        NCBI_THROW(CNetStorageServerException, eInvalidArgument, message);
    }

    if (result == eMetadataRequired) {
        if (!m_Server->InMetadataServices(m_Service))
            NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                       "Invalid metadata option. It cannot be required for not "
                       "configured service");
    }
    return result;
}


void
CNetStorageHandler::x_ValidateWriteMetaDBAccess(
                                            const CJsonNode &  message) const
{
    if (message.HasKey("ObjectLoc")) {
        // Object locator has a metadata flag and a service name
        string  object_loc = message.GetString("ObjectLoc");
        x_CheckObjectLoc(object_loc);

        CNetStorageObjectLoc  object_loc_struct(m_Server->GetCompoundIDPool(),
                                                object_loc);

        bool    no_metadata = object_loc_struct.IsMetaDataDisabled();

        if (no_metadata)
            NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                       "DB access requested for an object which was created "
                       "with no metadata flag");

        string      service = object_loc_struct.GetServiceName();
        if (service.empty())
            service = m_Service;
        if (!m_Server->InMetadataServices(service))
            NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                       "Effective object service (" + service +
                       ") is not in the list of "
                       "configured services. Metainfo DB access declined.");
        return;
    }

    // This is user key identification
    TNetStorageFlags    flags = ExtractStorageFlags(message);
    if (flags & fNST_NoMetaData) {
        NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                   "Storage flags forbid access to the metainfo DB. "
                   "Metainfo DB access declined.");
    }

    // There is no knowledge of the service and metadata request from the
    // object identifier, so the HELLO service is used
    if (!m_Server->InMetadataServices(m_Service))
        NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                   "Service provided in HELLO ("+ m_Service +
                   ") is not in the list of "
                   "configured services. Metainfo DB access declined.");
}


// Detects if the meta information DB should be updated
bool
CNetStorageHandler::x_DetectMetaDBNeedUpdate(
                                    const CJsonNode &  message,
                                    CNSTServiceProperties &  props) const
{
    if (message.HasKey("ObjectLoc")) {
        // Object locator has a metadata flag and a service name
        string  object_loc = message.GetString("ObjectLoc");
        x_CheckObjectLoc(object_loc);

        CNetStorageObjectLoc  object_loc_struct(m_Server->GetCompoundIDPool(),
                                                object_loc);

        bool    no_metadata = object_loc_struct.IsMetaDataDisabled();

        if (no_metadata)
            return false;

        string  service = object_loc_struct.GetServiceName();
        if (service.empty())
            service = m_Service;

        bool    service_configured = m_Server->GetServiceProperties(service,
                                                                    props);
        if (m_MetadataOption == eMetadataNotSpecified)
            return service_configured;

        // This is the eMetadataRequired option: the check below is needed
        // because the list of services could be reconfigured on the fly
        if (!service_configured)
            NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                       "Effective object service (" + service +
                       ") is not in the list of "
                       "configured services. Metainfo DB access declined.");
        return true;
    }

    // This is user key identification
    TNetStorageFlags    flags = ExtractStorageFlags(message);
    if (flags & fNST_NoMetaData)
        return false;

    // There is no knowledge of the service and metadata request from the
    // object identifier
    bool    service_configured = m_Server->GetServiceProperties(m_Service,
                                                                props);
    if (m_MetadataOption == eMetadataRequired) {
        // The service list could be reconfigured on the fly
        if (!service_configured)
            NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                       "Service provided in HELLO (" + m_Service +
                       ") is not in the list of "
                       "configured services. Metainfo DB access declined.");
    }
    return service_configured;
}


bool
CNetStorageHandler::x_DetectMetaDBNeedOnCreate(TNetStorageFlags  flags)
{
    if ((flags & fNST_NoMetaData) != 0)
        return false;
    if (m_MetadataOption == eMetadataDisabled)
        return false;

    bool    service_configured = m_Server->GetServiceTTL(m_Service,
                                                         m_CreateTTL);
    if (m_MetadataOption == eMetadataRequired) {
        // The services could be reconfigured on the fly so the presence of the
        // service in the list of configured must be done here again
        if (!service_configured)
            NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                       "Service provided in HELLO (" + m_Service +
                       ") is not in the list of "
                       "configured services. Metainfo DB access declined.");
    }
    return service_configured;
}


// Returns true if the metainfo DB should be accessed
bool
CNetStorageHandler::x_DetectMetaDBNeedOnGetObjectInfo(
                                    const CJsonNode & message) const
{
    if (m_MetadataOption == eMetadataDisabled)
        return false;

    if (message.HasKey("ObjectLoc")) {
        // Object locator has a metadata flag and a service name
        string  object_loc = message.GetString("ObjectLoc");
        x_CheckObjectLoc(object_loc);

        CNetStorageObjectLoc  object_loc_struct(m_Server->GetCompoundIDPool(),
                                                object_loc);

        bool    no_metadata = object_loc_struct.IsMetaDataDisabled();

        if (no_metadata)
            return false;

        string  service = object_loc_struct.GetServiceName();
        if (!service.empty())
            return m_Server->InMetadataServices(service);
    } else {
        // This is user key identification
        TNetStorageFlags    flags = ExtractStorageFlags(message);
        if (flags & fNST_NoMetaData)
            return false;
    }

    // In all the other cases the access depends on if the HELLO service is
    // configured for the metadata option
    return m_Server->InMetadataServices(m_Service);
}


// The function is called when it was decided that the DB should be accessed.
// The function throws an exception if the object is expired.
TNSTDBValue<CTime>
CNetStorageHandler::x_CheckObjectExpiration(const string &  object_key,
                                            bool            db_exception,
                                            CJsonNode &     reply)
{
    TNSTDBValue<CTime>  expiration;
    int                 status;

    try {
        status = m_Server->GetDb().ExecSP_GetObjectExpiration(object_key,
                                                              expiration,
                                                              m_Timing);
    } catch (const CNetStorageServerException &  ex) {
        if (db_exception)
            throw;
        AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                      ex.what(), ex.GetClass(), eDatabaseWarning);
        return expiration;
    } catch (const exception &  ex) {
        if (db_exception)
            throw;
        AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                      "Error while getting an object expiration time: " +
                      string(ex.what()),
                      kScopeStdException, eDatabaseWarning);
        return expiration;
    } catch (...) {
        if (db_exception)
            throw;
        AppendWarning(reply, NCBI_ERRCODE_X_NAME(NetStorage_ErrorCode),
                      "Unknown error while getting an object expiration time",
                      kScopeUnknownException, eDatabaseWarning);
        return expiration;
    }

    if (status != 0)
        return expiration;      // The only option here is that the
                                // object is not found

    if (expiration.m_IsNull)
        return expiration;      // Expiration is not available

    // The object TTL is found
    if (expiration.m_Value < CurrentTime())
        NCBI_THROW(CNetStorageServerException,
                   eNetStorageObjectExpired, "Object expired");
    return expiration;
}


bool
CNetStorageHandler::x_PrintTimingIsOn(void) const
{
    return m_Server->IsLogTiming() && m_CmdContext.NotNull();
}


// Checks if the client DB ID is set in the client registry.
// If it is then sets the m_DBClientID. This case means the DB ID was cached
// before.
// If the DB ID is not set then executes the stored procedure which retrieves
// the ID and saves it in the in-memory registry.
void
CNetStorageHandler::x_CreateClient(void)
{
    m_DBClientID = m_Server->GetClientRegistry().GetDBClientID(m_Client);
    if (m_DBClientID != k_UndefinedClientID)
        return;

    // Here: the ID is not set thus a DB request is required
    m_Server->GetDb().ExecSP_CreateClient(m_Client, m_DBClientID, m_Timing);
    if (m_DBClientID != k_UndefinedClientID)
        m_Server->GetClientRegistry().SetDBClientID(m_Client, m_DBClientID);
}

