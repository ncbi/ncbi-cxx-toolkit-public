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
 * Author:  Aaron Ucko
 *
 * File Description:
 *   Framework for a multithreaded network server
 */

#include <ncbi_pch.hpp>
#include <connect/threaded_server.hpp>
#include <connect/ncbi_socket.hpp>
#include <util/thread_pool.hpp>


BEGIN_NCBI_SCOPE


class CSocketRequest : public CStdRequest
{
public:
    CSocketRequest(CThreadedServer& server, SOCK sock)
        : m_Server(server), m_Sock(sock) {}
    virtual void Process(void);

private:
    CThreadedServer& m_Server;
    SOCK             m_Sock;
};


void CSocketRequest::Process(void)
{
    try {
        m_Server.Process(m_Sock);
    } STD_CATCH_ALL("CThreadedServer")
}


void CThreadedServer::Run(void)
{
    SetParams();

    if (m_InitThreads <= 0  ||
        m_MaxThreads  < m_InitThreads  ||  m_MaxThreads > 1000) {
        NCBI_THROW(CThreadedServerException, eBadParameters,
                   "CThreadedServer::Run: Bad parameters");
    }

    CListeningSocket lsock(m_Port);
    if (lsock.GetStatus() != eIO_Success) {
        NCBI_THROW(CThreadedServerException, eCouldntListen,
                   "CThreadedServer::Run: Unable to create listening socket: "
                   + string(strerror(errno)));
    }

    CStdPoolOfThreads pool(m_MaxThreads, m_QueueSize, m_SpawnThreshold);
    pool.Spawn(m_InitThreads);


    while ( !ShutdownRequested() ) {
        CSocket    sock;
        EIO_Status status = lsock.GetStatus();
        if (status != eIO_Success) {
            if (m_AcceptTimeout != kDefaultTimeout
                &&  m_AcceptTimeout != kInfiniteTimeout) {
                pool.WaitForRoom(m_AcceptTimeout->sec,
                                 m_AcceptTimeout->usec * 1000);
            } else {
                pool.WaitForRoom();
            }
            lsock.Listen(m_Port);
            continue;
        }
        status = lsock.Accept(sock, m_AcceptTimeout);
        if (status == eIO_Success) {
            sock.SetOwnership(eNoOwnership); // Process[Overflow] will close it
            try {
                pool.AcceptRequest
                    (CRef<ncbi::CStdRequest>
                     (new CSocketRequest(*this, sock.GetSOCK())));
                if (pool.IsFull()  &&  m_TemporarilyStopListening) {
                    lsock.Close();
                }
            } catch (CBlockingQueueException&) {
                _ASSERT( !m_TemporarilyStopListening );
                ProcessOverflow(sock.GetSOCK());
            }
        } else if (status == eIO_Timeout) {
            ProcessTimeout();
        } else {
            ERR_POST("accept failed: " << IO_StatusStr(status));
        }
    }

    lsock.Close();
    pool.KillAllThreads(true);
}


const char* CThreadedServerException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eBadParameters: return "eBadParameters";
    case eCouldntListen: return "eCouldntListen";
    default:             return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
