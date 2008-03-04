#ifndef _GRID_CONTROL_THREADD_HPP_
#define _GRID_CONTROL_THREADD_HPP_


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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *    NetSchedule Worker Node implementation
 */

#include <corelib/ncbimisc.hpp>
#include <connect/server.hpp>

#include <map>

BEGIN_NCBI_SCOPE

class CGridWorkerNode;
/////////////////////////////////////////////////////////////////////////////
//
/// @internal
class NCBI_XCONNECT_EXPORT CWorkerNodeControlThread : public CServer
{
public:

    class IRequestProcessor
    {
    public:
        virtual ~IRequestProcessor() {}

        virtual bool Authenticate(const string& host,
                                  const string& auth,
                                  const string& queue,
                                  CNcbiOstream& reply,
                                  const CGridWorkerNode& node) { return true; }

        virtual void Process(const string& request,
                             CNcbiOstream& reply,
                             CGridWorkerNode& node) = 0;
    };
    CWorkerNodeControlThread(unsigned int start_port, unsigned int end_port,
                             CGridWorkerNode& worker_node);

    CGridWorkerNode& GetWorkerNode() { return m_WorkerNode; }

    virtual ~CWorkerNodeControlThread();

    virtual bool ShutdownRequested(void);

    void RequestShutdown() { m_ShutdownRequested = true; }

    unsigned int GetControlPort() const { return m_Port; }

    static IRequestProcessor* MakeProcessor(const string& cmd);

protected:
    virtual void ProcessTimeout(void);

private:
    CGridWorkerNode& m_WorkerNode;
    volatile bool    m_ShutdownRequested;
    unsigned int m_Port;

    CWorkerNodeControlThread(const CWorkerNodeControlThread&);
    CWorkerNodeControlThread& operator=(const CWorkerNodeControlThread&);
};


//////////////////////////////////////////////////////////////
class NCBI_XCONNECT_EXPORT CWNCTConnectionHandler
    : public IServer_LineMessageHandler
{
public:
    CWNCTConnectionHandler(CWorkerNodeControlThread& server);
    virtual ~CWNCTConnectionHandler();

    virtual void OnOpen(void);
    virtual void OnMessage(BUF buffer);

    virtual void OnWrite() {}
    virtual void OnClose() {}

private:

    CWorkerNodeControlThread& m_Server;
    string m_Auth;
    string m_Queue;

    void (CWNCTConnectionHandler::*m_ProcessMessage)(BUF buffer);

    void x_ProcessAuth(BUF buffer);
    void x_ProcessQueue(BUF buffer);
    void x_ProcessRequest(BUF buffer);

    CWNCTConnectionHandler(const CWNCTConnectionHandler&);
    CWNCTConnectionHandler& operator=(const CWNCTConnectionHandler&);
};

END_NCBI_SCOPE

#endif // _GRID_CONTROL_THREADD_HPP_
