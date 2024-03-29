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
#include <misc/netstorage/netstorage.hpp>

#include "nst_clients.hpp"
#include "nst_users.hpp"
#include "nst_metadata_options.hpp"
#include "nst_database.hpp"
#include "nst_service_parameters.hpp"
#include "nst_timing.hpp"


BEGIN_NCBI_SCOPE

const size_t    kReadBufferSize = 1024 * 1024;
const size_t    kWriteBufferSize = 16 * 1024;


// Forward declarations
class CNetStorageServer;
struct SCommonRequestArguments;
struct SStorageFlags;
struct SICacheSettings;
struct SUserKey;


// Helper class to make sure the current request context is reset at the end of
// the scope
class CRequestContextResetter
{
    public:
        CRequestContextResetter();
        ~CRequestContextResetter();
};


// Helper class to make sure the pushed IMessageListener is popped
// approprietely at the end of the scope
class CMessageListenerResetter
{
    public:
        CMessageListenerResetter();
        ~CMessageListenerResetter();
};


// The CRelocateCallback is used to provide a callback during the RELOCATE
// message processing. The NetStorageAPI Relocate call is synchronous so
// it may take a long time within which the client socket could time out.
// The callback is called after each portion of data is relocated so it is
// a good suitable point to send a heartbeat message to the client. The
// heartbeat - in addition to the keeping the connection active - sends some
// progress information, notably the number of the relocated bytes.
class CNetStorageHandler;
class CRelocateCallback
{
    public:
        CRelocateCallback(CNetStorageHandler &  handler,
                          const SCommonRequestArguments &  common_args,
                          CDirectNetStorageObject &  object,
                          bool  need_progress_report);

    public:
        // Should match TNetStorageProgressCb prototype
        void  Callback(CJsonNode  info);

    private:
        CNetStorageHandler &                m_Handler;
        const SCommonRequestArguments &     m_CommonArgs;
        CDirectNetStorageObject &           m_Object;
        bool                                m_NeedProgressReport;
};


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
        eStatus_OK                  = 200, // Command is ok and execution
                                           // is good

        eStatus_BadRequest          = 400, // Command is incorrect
        eStatus_NotFound            = 404, // Object is not found
        eStatus_Inactive            = 408, // Connection was closed due to
                                           // inactivity timeout
        eStatus_Probe               = 444, // Routine test from systems
        eStatus_SocketIOError       = 499, // Error writing to socket

        eStatus_ServerError         = 500, // Internal server error
        eStatus_NotImplemented      = 501, // Command is not implemented
        eStatus_ShuttingDown        = 503  // Server is shutting down
    };

    // In most of the case a socket timeout is considered as an error however
    // there could be a case -- e.g. candidate is the relocate progress message
    // -- when a timeout needs to be ignored. So there is an enumeration below.
    enum ESocketTimeoutTreat {
        eTimeoutIsError,
        eTimeoutIsOK
    };

private:
    // Application specific part
    bool  x_ReadRawData();
    void  x_OnMessage(const CJsonNode &  message);
    void  x_OnData(const void *  data, size_t  data_size);
    void  x_SendWriteConfirmation();

    // It closes the connection if there were socket writing errors
    EIO_Status  x_SendSyncMessage(
                        const CJsonNode &  message,
                        ESocketTimeoutTreat  timeout_treat = eTimeoutIsError);
    EIO_Status  x_SendOutputBuffer(ESocketTimeoutTreat  timeout_treat);
    void  x_OnSocketWriteError(EIO_Status  status, size_t  bytes_written,
                               const char *  output_buffer,
                               size_t  output_buffer_size);
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
    string                      m_Service;
    string                      m_Client;

private:
    enum EReadMode {
        eReadMessages,
        eReadRawData
    };

    char                    m_ReadBuffer[kReadBufferSize];
    EReadMode               m_ReadMode;
    CUTTPReader             m_UTTPReader;
    CJsonOverUTTPReader     m_JSONReader;

    char                    m_WriteBuffer[kWriteBufferSize];
    CUTTPWriter             m_UTTPWriter;
    CJsonOverUTTPWriter     m_JSONWriter;

    CFastMutex              m_OutputQueueMutex;
    vector<CJsonNode>       m_OutputQueue;

private:
    // Asynchronous write support
    CDirectNetStorageObject m_ObjectBeingWritten;
    Int8                    m_DataMessageSN;
    EMetadataOption         m_MetadataOption;
    bool                    m_CreateRequest;
    TNSTDBValue<CTimeSpan>  m_CreateTTL;
    Int8                    m_DBClientID;
    Int8                    m_DBUserID;
    Int8                    m_ObjectSize;
    CNSTServiceProperties   m_WriteServiceProps;

private:
    bool                    m_ByeReceived;
    bool                    m_FirstMessage;
    bool                    m_WriteCreateNeedMetaDBUpdate;

    CNSTTiming              m_Timing;

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
    void x_ProcessGetUsersInfo(const CJsonNode &                message,
                               const SCommonRequestArguments &  common_args);
    void x_ProcessGetMetadataInfo(const CJsonNode &                message,
                                  const SCommonRequestArguments &  common_args);
    void x_ProcessGetObjectInfo(const CJsonNode &                message,
                                const SCommonRequestArguments &  common_args);
    void x_ProcessGetAttrList(const CJsonNode &                message,
                              const SCommonRequestArguments &  common_args);
    void x_ProcessGetClientObjects(const CJsonNode &                message,
                                   const SCommonRequestArguments &  comm_args);
    void x_ProcessGetUserObjects(const CJsonNode &                message,
                                 const SCommonRequestArguments &  comm_args);
    void x_ProcessGetAttr(const CJsonNode &                message,
                          const SCommonRequestArguments &  common_args);
    void x_ProcessSetAttr(const CJsonNode &                message,
                          const SCommonRequestArguments &  common_args);
    void x_ProcessDelAttr(const CJsonNode &                message,
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
    void x_ProcessGetSize(const CJsonNode &                message,
                          const SCommonRequestArguments &  common_args);
    void x_ProcessSetExpTime(const CJsonNode &                message,
                             const SCommonRequestArguments &  common_args);
    void x_ProcessLockFTPath(const CJsonNode &                message,
                             const SCommonRequestArguments &  common_args);

private:
    CDirectNetStorageObject x_GetObject(const CJsonNode &  message,
                                        bool  need_fake_write = false);
    void x_CheckObjectLoc(const string &  object_loc) const;
    void x_CheckICacheSettings(const SICacheSettings &  icache_settings);
    void x_CheckUserKey(const SUserKey &  user_key);
    void x_GetStorageParams(const CJsonNode &   message,
                            SICacheSettings *   icache_settings,
                            SUserKey *          user_key,
                            TNetStorageFlags *  flags);
    CDirectNetStorageObject x_CreateObjectStream(
                    const SICacheSettings &  icache_settings,
                    TNetStorageFlags         flags);
    EIO_Status x_SendOverUTTP();
    EMetadataOption x_ConvertMetadataArgument(const CJsonNode &  message) const;
    void x_ValidateWriteMetaDBAccess(const CJsonNode &  message,
                                     bool  expect_object = true) const;
    bool x_DetectMetaDBNeedUpdate(const CJsonNode &  message,
                                  CNSTServiceProperties &  props) const;
    bool x_DetectMetaDBNeedOnCreate(TNetStorageFlags  flags);
    bool x_DetectMetaDBNeedOnGetObjectInfo(
                                        const CJsonNode & message,
                                        CNSTServiceProperties &  props) const;
    void x_CreateClient(void);
    Int8 x_GetClientID(const string &  client);
    void x_CreateUser(void);
    Int8 x_GetUserID(const CNSTUserID &  user);
    void x_FillObjectInfo(CJsonNode &  reply, const string &  val);
    void x_SetObjectInfoReply(CJsonNode &  reply, const string &  name,
                              const TNSTDBValue<CTime> &  value);
    void x_SetObjectInfoReply(CJsonNode &  reply, const string &  name,
                              const TNSTDBValue<Int8> &  value);
    void x_SetObjectInfoReply(CJsonNode &  reply, const string &  name,
                              const TNSTDBValue<string> &  value);

    void x_OptionalExpirationUpdate(CDirectNetStorageObject &  object,
                                    CJsonNode &  reply,
                                    const string &  user_message);
    void x_CheckExistanceStatus(int  status);
    void x_CheckExpirationStatus(int  status);
    void x_CheckNonAnonymousClient(const string &  op) const;
    string x_GetConnRef(void);

private:
    enum EOp {
        eReadOp,
        eWriteOp,
        eRelocateOp
    };
    void x_ProlongObjectOnFailure(EOp  operation,
                                  const string &  object_key,
                                  const CNSTServiceProperties &  service_props);

    friend class CRelocateCallback;
}; // CNetStorageHandler


END_NCBI_SCOPE

#endif

