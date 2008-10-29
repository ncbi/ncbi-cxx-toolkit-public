#ifndef NETCACHE_MESSAGE_HANDLER__HPP
#define NETCACHE_MESSAGE_HANDLER__HPP
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
 * Authors:  Victor Joukov, Pavel Ivanov
 *
 * File Description: Network cache daemon
 *
 */

#include <corelib/request_ctx.hpp>
#include <connect/server.hpp>
#include <connect/services/netservice_protocol_parser.hpp>

#include "netcached.hpp"

BEGIN_NCBI_SCOPE

// All of IServer_MessageHandler functionality is copied with read timing
// added. Unfortunately, CServer standard abstract class, implementing
// message handling functionality does not expose socket Read
// method. We need to surround this method by timing accounting,
// so we duplicated this code here with timing added.
class CNetCache_MessageHandler : public IServer_ConnectionHandler
{
public:
    CNetCache_MessageHandler(CNetCacheServer* server);
    virtual ~CNetCache_MessageHandler(void) { BUF_Destroy(m_Buffer); }
    // MessageHandler protocol
    virtual EIO_Event GetEventsToPollFor(const CTime** alarm_time) const;
    virtual void OnOpen(void);
    virtual void OnRead(void);
    virtual void OnWrite(void);
    virtual void OnCloseExt(EClosePeer peer);
    virtual void OnTimeout(void);
    virtual void OnOverflow(void);
    virtual int CheckMessage(BUF* buffer, const void *data, size_t size);
    virtual void OnMessage(BUF buffer);

    // Processing stages of session
    void ProcessMsgAuth(BUF buffer);
    void ProcessMsgRequest(BUF buffer);
    void ProcessMsgTransmission(BUF buffer);

    // Called when long running request is finished
    void OnRequestEnd();

    // CNetCache_RequestHandlerHost protocol
    virtual void BeginReadTransmission();
    virtual void BeginDelayedWrite();

protected:
    void WriteMsg(const string& prefix, const string& msg)
    {
        CSocket& socket = GetSocket();
        CNetCacheServer::WriteMsg(socket, prefix, msg);
    }
    bool IsMonitoring()
    {
        return m_Server->IsMonitoring();
    }
    void MonitorPost(const string& msg)
    {
        m_Server->MonitorPost(msg);
    }
    void MonitorException(const std::exception& ex,
                          const string& comment,
                          const string& request);
    void ReportException(const std::exception& ex,
                         const string& comment,
                         const string& request);
    void ReportException(const CException& ex,
                         const string& comment,
                         const string& request);

public:
    typedef void (CNetCache_MessageHandler::*TProcessor)(void);
    struct SCommandExtra {
        TProcessor    processor;
    };
    typedef SNSProtoCmdDef<SCommandExtra>      SCommandDef;
    typedef SNSProtoParsedCmd<SCommandExtra>   SParsedCmd;
    typedef CNetServProtoParser<SCommandExtra> TProtoParser;

    // Command processors
    void ProcessAlive(void);
    void ProcessOK(void);
    void ProcessMoni(void);
    void ProcessSessionReg(void);
    void ProcessSessionUnreg(void);
    void ProcessPut(void);
    void ProcessPut2(void);
    void ProcessPut3(void);
    void ProcessGet(void);
    void ProcessVersion(void);
    void ProcessLog(void);
    void ProcessStat(void);
    void ProcessRemove(void);
    void ProcessRemove2(void);
    void ProcessShutdown(void);
    void ProcessGetConfig(void);
    void ProcessDropStat(void);
    void ProcessHasBlob(void);
    void ProcessGetBlobOwner(void);
    void ProcessGetStat(void);
    void ProcessIsLock(void);
    void ProcessGetSize(void);
    void Process_IC_SetTimeStampPolicy(void);
    void Process_IC_GetTimeStampPolicy(void);
    void Process_IC_SetVersionRetention(void);
    void Process_IC_GetVersionRetention(void);
    void Process_IC_GetTimeout(void);
    void Process_IC_IsOpen(void);
    void Process_IC_Store(void);
    void Process_IC_StoreBlob(void);
    void Process_IC_GetSize(void);
    void Process_IC_GetBlobOwner(void);
    void Process_IC_Read(void);
    void Process_IC_Remove(void);
    void Process_IC_RemoveKey(void);
    void Process_IC_GetAccessTime(void);
    void Process_IC_HasBlobs(void);
    void Process_IC_Purge1(void);

private:
    // Phase of connection - login, queue, command, batch submit etc.
    void (CNetCache_MessageHandler::*m_ProcessMessage)(BUF buffer);

    void x_AssignParams(const map<string, string>& params);
    bool x_ProcessWriteAndReport(unsigned int  blob_size,
                                 const string* req_id = 0);
    void x_ProcessSM(bool reg);

    /// Init diagnostics Client IP and Session ID for proper logging
    void x_InitDiagnostics(void);
    /// Reset diagnostics Client IP and Session ID to avoid logging
    /// not related to the request
    void x_DeinitDiagnostics(void);

private:
    CNetCacheServer* m_Server;

    bool                  m_InRequest;
    // Statistics
    SNetCache_RequestStat m_Stat;
    // IServer_MessageHandling implementation
    BUF m_Buffer;

    // Line message handling
    bool             m_SeenCR;

    // Read Transmission handling
    bool             m_ByteSwap;
    int              m_SigRead;
    unsigned         m_Signature;
    int              m_LenRead;
    unsigned         m_Length;

    //
    string m_Auth;

    /// Continuous read flag
    bool m_InTransmission;
    /// Delayed write flag
    bool m_DelayedWrite;

    CRef<CRequestContext> m_Context;
    bool                  m_StartPrinted;

    string m_Host;
    unsigned int m_Port;
    string m_ReqId;
    string m_ValueParam;
    ICache* m_ICache;
    unsigned int m_Policy;
    unsigned int m_MaxTimeout;
    unsigned int m_Timeout;
    string m_Key;
    unsigned int m_Version;
    string m_SubKey;

    TProtoParser      m_Parser;

    auto_ptr<IWriter> m_Writer;
    auto_ptr<IReader> m_Reader;
    bool              m_PutOK;
    bool              m_SizeKnown;
    size_t            m_BlobSize;
};


END_NCBI_SCOPE

#endif /* NETCACHE_MESSAGE_HANDLER__HPP */
