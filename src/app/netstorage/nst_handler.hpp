#ifndef NETSTORAGE_HANDLER__HPP
#define NETSTORAGE_HANDLER__HPP

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

#include <string>
#include <connect/services/json_over_uttp.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/request_ctx.hpp>
#include <connect/server.hpp>
#include <connect/services/netstorage.hpp>

#include "nst_clients.hpp"


BEGIN_NCBI_SCOPE

// Forward declarations
class CNetStorageServer;
struct SCommonRequestArguments;
struct SStorageFlags;
struct SICacheSettings;
struct SUserKey;


class CNetStorageHandler : public IServer_ConnectionHandler
{
public:

    CNetStorageHandler(CNetStorageServer *  server);
    ~CNetStorageHandler();

    // IServer_ConnectionHandler protocol
    virtual EIO_Event GetEventsToPollFor(const CTime** alarm_time) const;
    virtual void      OnOpen(void);
    virtual void      OnRead(void);
    virtual void      OnWrite(void);
    virtual void      OnClose(IServer_ConnectionHandler::EClosePeer peer);
    virtual void      OnTimeout(void);
    virtual void      OnOverflow(EOverflowReason reason);

    // Statuses of commands to be set in diagnostics' request context
    // Additional statuses can be taken from
    // http://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html
    enum EHTTPStatus {
        eStatus_OK                  = 200, // Command is ok and execution is good

        eStatus_BadRequest          = 400, // Command is incorrect
        eStatus_NotFound            = 404, // Object is not found
        eStatus_Inactive            = 408, // Connection was closed due to inactivity
                                           // timeout
        eStatus_SocketIOError       = 499, // Error writing to socket

        eStatus_ServerError         = 500, // Internal server error
        eStatus_NotImplemented      = 501, // Command is not implemented
        eStatus_ShuttingDown        = 503  // Server is shutting down
    };

private:
    // Application specific part
    bool  x_ReadRawData();
    void  x_OnMessage(const CJsonNode &  message);
    void  x_OnData(const void *  data, size_t  data_size);
    void  x_SendWriteConfirmation();

    // It closes the connection if there were socket writing errors
    EIO_Status  x_SendSyncMessage(const CJsonNode &  message);
    EIO_Status  x_SendOutputBuffer(void);
    void  x_PrintMessageRequestStart(const CJsonNode &  message);
    void  x_PrintMessageRequestStop(void);

    // Future versions may send the messages asynchronously
    void  x_SendAsyncMessage(const CJsonNode &  message);

private:
    void x_SetQuickAcknowledge(void);
    void x_SetCmdRequestStatus(unsigned int  status)
    { if (m_CmdContext.NotNull())
        m_CmdContext->SetRequestStatus(status); }
    void x_SetConnRequestStatus(unsigned int  status)
    { if (m_ConnContext.NotNull())
        m_ConnContext->SetRequestStatus(status); }

private:
    void x_CreateConnContext(void);
    unsigned int  x_GetPeerAddress(void);

    CNetStorageServer *         m_Server;

    // Diagnostics context for the current connection
    CRef<CRequestContext>       m_ConnContext;
    // Diagnostics context for the currently executed command
    CRef<CRequestContext>       m_CmdContext;

    // The client identification. It appears after HELLO.
    string                      m_Client;
    CNSTClientRegistry          m_ClientRegistry;

private:
    enum EReadMode {
        eReadMessages,
        eReadRawData
    };

    char *                  m_ReadBuffer;
    EReadMode               m_ReadMode;
    CUTTPReader             m_UTTPReader;
    CJsonOverUTTPReader     m_JSONReader;

    char *                  m_WriteBuffer;
    CUTTPWriter             m_UTTPWriter;
    CJsonOverUTTPWriter     m_JSONWriter;

    CFastMutex              m_OutputQueueMutex;
    vector<CJsonNode>       m_OutputQueue;

private:
    // Asynchronous write support
    CNetStorageObject       m_ObjectStream;
    Int8                    m_DataMessageSN;
    bool                    m_NeedMetaInfo;
    bool                    m_CreateRequest;
    Int8                    m_DBClientID;
    Int8                    m_DBObjectID;
    Int8                    m_ObjectSize;

private:
    bool                    m_ByeReceived;
    bool                    m_FirstMessage;

    typedef void (CNetStorageHandler::*FProcessor)(
                                const CJsonNode &,
                                const SCommonRequestArguments &);
    struct SProcessorMap
    {
        string      m_MessageType;
        FProcessor  m_Processor;
    };
    static SProcessorMap    sm_Processors[];
    FProcessor  x_FindProcessor(const SCommonRequestArguments &  common_args);

    // Individual message processors
    // The return value is how many bytes are expected as raw data
    void x_ProcessBye(const CJsonNode &                message,
                      const SCommonRequestArguments &  common_args);
    void x_ProcessHello(const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args);
    void x_ProcessInfo(const CJsonNode &                message,
                       const SCommonRequestArguments &  common_args);
    void x_ProcessConfiguration(const CJsonNode &                message,
                                const SCommonRequestArguments &  common_args);
    void x_ProcessHealth(const CJsonNode &                message,
                         const SCommonRequestArguments &  common_args);
    void x_ProcessAckAlert(const CJsonNode &                message,
                           const SCommonRequestArguments &  common_args);
    void x_ProcessReconfigure(const CJsonNode &                message,
                              const SCommonRequestArguments &  common_args);
    void x_ProcessShutdown(const CJsonNode &                message,
                           const SCommonRequestArguments &  common_args);
    void x_ProcessGetClientsInfo(const CJsonNode &                message,
                                 const SCommonRequestArguments &  common_args);
    void x_ProcessGetObjectInfo(const CJsonNode &                message,
                                const SCommonRequestArguments &  common_args);
    void x_ProcessGetAttr(const CJsonNode &                message,
                          const SCommonRequestArguments &  common_args);
    void x_ProcessSetAttr(const CJsonNode &                message,
                          const SCommonRequestArguments &  common_args);
    void x_ProcessRead(const CJsonNode &                message,
                       const SCommonRequestArguments &  common_args);
    void x_ProcessCreate(const CJsonNode &                message,
                         const SCommonRequestArguments &  common_args);
    void x_ProcessWrite(const CJsonNode &                message,
                        const SCommonRequestArguments &  common_args);
    void x_ProcessDelete(const CJsonNode &                message,
                         const SCommonRequestArguments &  common_args);
    void x_ProcessRelocate(const CJsonNode &                message,
                           const SCommonRequestArguments &  common_args);
    void x_ProcessExists(const CJsonNode &                message,
                         const SCommonRequestArguments &  common_args);
    void x_ProcessGetSize(const CJsonNode &                 message,
                          const SCommonRequestArguments &  common_args);

private:
    string
    x_GetObjectLoc(const CJsonNode &  message);
    void x_CheckNonAnonymousClient(void);
    void x_CheckObjectLoc(const string &  object_loc);
    void x_CheckICacheSettings(const SICacheSettings &  icache_settings);
    void x_CheckUserKey(const SUserKey &  user_key);
    void x_GetStorageParams(const CJsonNode &   message,
                            SICacheSettings *   icache_settings,
                            SUserKey *          user_key,
                            TNetStorageFlags *  flags);
    CNetStorageObject x_CreateObjectStream(
                    const SICacheSettings &  icache_settings,
                    TNetStorageFlags         flags);

    EIO_Status x_SendOverUTTP();
}; // CNetStorageHandler


END_NCBI_SCOPE

#endif

