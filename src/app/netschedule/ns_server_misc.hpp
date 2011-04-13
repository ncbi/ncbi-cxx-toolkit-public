#ifndef NETSCHEDULE_SERVER_MISC__HPP
#define NETSCHEDULE_SERVER_MISC__HPP

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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: factories, hosts etc for the NS server
 *
 */

#include <string>
#include <connect/server.hpp>
#include <corelib/ncbiobj.hpp>
#include <util/thread_pool_old.hpp>

#include "background_host.hpp"
#include "ns_util.hpp"


BEGIN_NCBI_SCOPE

class CNetScheduleServer;
class CRequestContext;


//////////////////////////////////////////////////////////////////////////
/// CNetScheduleConnectionFactory::
class CNetScheduleConnectionFactory : public IServer_ConnectionFactory
{
public:
    CNetScheduleConnectionFactory(CNetScheduleServer* server) :
        m_Server(server)
    {}

    IServer_ConnectionHandler* Create(void);

private:
    CNetScheduleServer*     m_Server;
};


//////////////////////////////////////////////////////////////////////////
/// Host for background threads
class CNetScheduleBackgroundHost : public CBackgroundHost
{
public:
    CNetScheduleBackgroundHost(CNetScheduleServer* server)
        : m_Server(server)
    {}

    virtual void ReportError(ESeverity          severity,
                             const std::string& what);
    virtual bool ShouldRun();

private:
    CNetScheduleServer*     m_Server;
};


//////////////////////////////////////////////////////////////////////////
/// Executor for long-standing requests
class CNetScheduleRequestExecutor : public CRequestExecutor
{
public:
    CNetScheduleRequestExecutor(CNetScheduleServer* server)
        : m_Server(server)
    {}

    virtual void SubmitRequest(const CRef<CStdRequest>& request);

private:
    CNetScheduleServer* m_Server;
};


struct CRequestContextPoolFactory
{
    static CRequestContext* Create();
    static void Delete(CRequestContext* rc);
};


//////////////////////////////////////////////////////////////////////////
// CNSRequestContextFactory
class CNSRequestContextFactory : public CRequestContextFactory
{
public:
    CNSRequestContextFactory(CNetScheduleServer* server)
        : m_Server(server)
    {}

    virtual CRequestContext* Get();
    virtual void Return(CRequestContext*);

private:
    CNetScheduleServer*     m_Server;
};


class CRequestContextGuard
{
public:
    CRequestContextGuard();
    ~CRequestContextGuard();

private:
    CRequestContext*        m_Ctx;
};


class CRequestContextSubmitGuard
{
public:
    CRequestContextSubmitGuard(CNetScheduleServer*  server,
                               const string&        client_ip,
                               const string&        session_id);
    ~CRequestContextSubmitGuard();

private:
    CNetScheduleServer*     m_Server;
    CRequestContext*        m_Ctx;
};


END_NCBI_SCOPE

#endif

