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

class CNetCache_MessageHandler : public IServer_MessageHandler,
    public CNetCache_RequestHandlerHost
{
public:
    CNetCache_MessageHandler(CNetCacheServer* server);
    // MessageHandler protocol
    virtual EIO_Event GetEventsToPollFor(const CTime** alarm_time) const;
    virtual void OnOpen(void);
    virtual void OnWrite(void);
    virtual void OnClose(void);
    virtual void OnTimeout(void);
    virtual void OnOverflow(void);
    virtual int CheckMessage(BUF* buffer, const void *data, size_t size);
    virtual void OnMessage(BUF buffer);

    void ProcessMsgAuth(BUF buffer);
    void ProcessMsgRequest(BUF buffer);
    void ProcessMsgTransmission(BUF buffer);

    // CNetCache_RequestHandlerHost protocol
    virtual void BeginReadTransmission();
    virtual void BeginDelayedWrite();
    virtual CNetCacheServer* GetServer();

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
    bool IsLogging()
    {
        return m_Server->IsLog();
    }
    void MonitorPost(const string& msg)
    {
        m_Server->MonitorPost(msg);
    }

protected:
    NetCache_RequestStat m_Stat;
    CStopWatch           m_StopWatch;

private:
    // Phase of connection - login, queue, command, batch submit etc.
    void (CNetCache_MessageHandler::*m_ProcessMessage)(BUF buffer);

    void ProcessSM(CSocket& socket, string& req);

private:
    CNetCacheServer* m_Server;
    bool m_SeenCR;

    bool m_InTransmission;
    bool m_ByteSwap;
    int  m_SigRead;
    unsigned m_Signature;
    int  m_LenRead;
    unsigned m_Length;
    //
    string m_Auth;
    auto_ptr<CNetCache_RequestHandler> m_ICHandler;
    auto_ptr<CNetCache_RequestHandler> m_NCHandler;
    CNetCache_RequestHandler*          m_LastHandler;

    // Delayed write flag
    bool m_DelayedWrite;
};


END_NCBI_SCOPE

#endif /* NETCACHE_MESSAGE_HANDLER__HPP */
