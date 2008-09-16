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
 * Authors:  Victor Joukov
 *
 * File Description: Network cache daemon
 *
 */
#include <connect/server.hpp>
#include "request_handler.hpp"

BEGIN_NCBI_SCOPE

// All of IServer_MessageHandler functionality is copied with read timing
// added. Unfortunately, CServer standard abstract class, implementing
// message handling functionality does not expose socket Read
// method. We need to surround this method by timing accounting,
// so we duplicated this code here with timing added.
class CNetCache_MessageHandler : public IServer_ConnectionHandler,
    public CNetCache_RequestHandlerHost
{
public:
    CNetCache_MessageHandler(CNetCacheServer* server);
    virtual ~CNetCache_MessageHandler(void) { BUF_Destroy(m_Buffer); }
    // MessageHandler protocol
    virtual EIO_Event GetEventsToPollFor(const CTime** alarm_time) const;
    virtual void OnOpen(void);
    virtual void OnRead(void);
    virtual void OnWrite(void);
    virtual void OnClose(void);
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
    virtual CNetCacheServer* GetServer();
    virtual SNetCache_RequestStat* GetStat();
    virtual const string* GetAuth();
    virtual void SetDiagParameters(const string& client_ip,
                                   const string& session_id);

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

private:
    // Phase of connection - login, queue, command, batch submit etc.
    void (CNetCache_MessageHandler::*m_ProcessMessage)(BUF buffer);

    void ProcessSM(const CNCRequestParser& parser);

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

    auto_ptr<CNetCache_RequestHandler> m_ICHandler;
    auto_ptr<CNetCache_RequestHandler> m_NCHandler;

    /// Continuous read flag
    bool m_InTransmission;
    /// Delayed write flag
    bool m_DelayedWrite;
    /// Client IP for current request. It will be set in diagnostics for
    /// proper logging.
    string m_ClientIP;
    /// Session ID for current request. It will be set in diagnostics for
    /// proper logging.
    string m_SessionID;
    // By original design, connection can process a mix of commands from
    // both NC and IC subsets. Commands that require Read Transmission or
    // DelayedWrite are processed in more than one request, and we need to
    // know which handler new request should be redirected to.
    // For this purpose we introduced m_LastHandler.
    CNetCache_RequestHandler*          m_LastHandler;

};


END_NCBI_SCOPE

#endif /* NETCACHE_MESSAGE_HANDLER__HPP */
