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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 6.4  2002/01/25 15:39:29  ucko
* Completely reorganized threaded servers.
*
* Revision 6.3  2002/01/24 20:19:18  ucko
* Add magic TemporarilyStopListening overflow processor
* More cleanups
*
* Revision 6.2  2002/01/24 18:35:56  ucko
* Allow custom queue-overflow handling.
* Clean up SOCKs and CONNs when done with them.
*
* Revision 6.1  2001/12/11 19:55:22  ucko
* Introduce thread-pool-based servers.
*
* ===========================================================================
*/

#include <connect/threaded_server.hpp>
#include <util/thread_pool.hpp>

BEGIN_NCBI_SCOPE


class CSocketRequest : public CStdRequest
{
public:
    CSocketRequest(CThreadedServer& server, SOCK sock)
        : m_Server(server), m_Sock(sock) {}
    virtual void Process(void)
        { m_Server.Process(m_Sock); }

private:
    CThreadedServer& m_Server;
    SOCK             m_Sock;
};


void CThreadedServer::Run(void)
{
    SetParams();

    if (m_InitThreads <= 0  ||  m_MaxThreads < m_InitThreads
        ||  m_MaxThreads > 1000  ||  m_Port > 65535) {
        ERR_POST("CThreadedServer::Run: Bad parameters!");
        return;
    }

    LSOCK             lsock;
    CStdPoolOfThreads pool(m_MaxThreads, m_QueueSize, m_SpawnThreshold);

    pool.Spawn(m_InitThreads);
    LSOCK_Create(m_Port, 5, &lsock);

    for (;;) {
        SOCK sock;
        EIO_Status status = LSOCK_Accept(lsock, NULL, &sock);
        if (status == eIO_Success) {
            try {
                pool.AcceptRequest(new CSocketRequest(*this, sock));
                if (pool.IsFull()  &&  m_TemporarilyStopListening) {
                    LSOCK_Close(lsock);
                    pool.WaitForRoom();
                    LSOCK_Create(m_Port, 5, &lsock);
                }
            } catch (CBlockingQueue<CRef<CStdRequest> >::CException) {
                _ASSERT(!m_TemporarilyStopListening);
                ProcessOverflow(sock);
            }            
        } else {
            ERR_POST("accept failed: " << IO_StatusStr(status));
        }
    }

    LSOCK_Close(lsock);
}


END_NCBI_SCOPE
