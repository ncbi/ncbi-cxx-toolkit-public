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
* Authors:  Aaron Ucko, Victor Joukov
*
* File Description:
*   threaded server connection pool
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <connect/error_codes.hpp>
#include "connection_pool.hpp"


#define NCBI_USE_ERRCODE_X   Connect_ThrServer


BEGIN_NCBI_SCOPE


// SPerConnInfo
void CServer_ConnectionPool::SPerConnInfo::UpdateExpiration(
    const TConnBase* conn)
{
    const STimeout* timeout = kDefaultTimeout;
    const CSocket*  socket  = dynamic_cast<const CSocket*>(conn);

    if (socket) {
        timeout = socket->GetTimeout(eIO_ReadWrite);
    }
    if (type == eInactiveSocket  &&  timeout != kDefaultTimeout
        &&  timeout != kInfiniteTimeout) {
        expiration.SetCurrent();
        expiration.AddSecond(timeout->sec);
        expiration.AddNanoSecond(timeout->usec * 1000);
    } else {
        expiration.Clear();
    }
}


// CServer_ControlConnection
CStdRequest* CServer_ControlConnection::CreateRequest(
    EServIO_Event event,
    CServer_ConnectionPool& connPool,
    const STimeout* timeout, int request_id)
{
    char buf[64];
    Read(buf, sizeof(buf));
    _TRACE("Control socket read");
    return NULL;
}


//
CServer_ConnectionPool::CServer_ConnectionPool(unsigned max_connections) :
        m_MaxConnections(max_connections)
{
    // Create internal signaling connection from m_ControlSocket to
    // m_ControlSocketForPoll
    unsigned short port;
    CListeningSocket listener;
    static const STimeout kTimeout = { 10, 500000 }; // 500 ms // DEBUG
    for (port = 2049; port < 65535; ++port) {
        if (eIO_Success == listener.Listen(port, 5, fLSCE_BindLocal))
            break;
    }
    m_ControlSocket.Connect("127.0.0.1", port);
    m_ControlSocket.DisableOSSendDelay();
    // Set a (modest) timeout to prevent SetConnType from blocking forever
    // with m_Mutex held, which could deadlock the whole server.
    m_ControlSocket.SetTimeout(eIO_Write, &kTimeout);
    listener.Accept(dynamic_cast<CSocket&>(m_ControlSocketForPoll));
}

CServer_ConnectionPool::~CServer_ConnectionPool()
{
    try {
        Erase();
    } catch(...) {
        ERR_POST_X(3, "Exception thrown from ~CServer_ConnectionPool");
    }
}

void CServer_ConnectionPool::Erase(void)
{
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);
    NON_CONST_ITERATE(TData, it, data) {
         it->first->OnTimeout();
         delete it->first;
    }
    data.clear();
}

bool CServer_ConnectionPool::Add(TConnBase* conn, EConnType type)
{
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);

    if (data.size() >= m_MaxConnections) {
        // XXX - try to prune old idle connections before giving up?
        _TRACE("Failed to add connection " << conn << " to pool");
        return false;
    }

    SPerConnInfo& info = data[conn];
    info.type = type;
    info.UpdateExpiration(conn);

    _TRACE("Added connection " << conn << " to pool");
    return true;
}


void CServer_ConnectionPool::SetConnType(TConnBase* conn, EConnType type)
{
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);
    TData::iterator it = data.find(conn);
    if (it == data.end()) {
        _TRACE("SetConnType called on unknown connection.");
    } else {
        it->second.type = type;
        it->second.UpdateExpiration(conn);
    }
    // Signal poll cycle to re-read poll vector by sending
    // byte to control socket
    if (type == eInactiveSocket) {
        // DEBUG if (it != data.end() && !it->first->IsOpen())
        //     printf("Socket just closed\n");
        _TRACE("Connection inactive " << conn);
//        _TRACE("Control socket BEGIN");
        EIO_Status status = m_ControlSocket.Write("", 1, NULL, eIO_WritePlain);
//        _TRACE("Control socket END");
        if (status != eIO_Success) {
            ERR_POST_X(4, Warning
                       << "SetConnType: failed to write to control socket: "
                       << IO_StatusStr(status));
        }
    }
}


void CServer_ConnectionPool::Clean(void)
{
    CTime now(CTime::eCurrent, CTime::eGmt);
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);
    if (data.empty())
        return;
    int n_connections = 0;
    list<TConnBase*> to_delete;
    NON_CONST_ITERATE(TData, it, data) {
        SPerConnInfo& info = it->second;
        if (info.type != eListener) ++n_connections;
        if (info.type != eInactiveSocket) {
            continue;
        }
        CServer_Connection* conn = dynamic_cast<CServer_Connection*>(it->first);
        if (!it->first->IsOpen() || info.expiration <= now) {
            if (info.expiration <= now) {
                _TRACE("Timeout on " << dynamic_cast<TConnBase *>(it->first));
                it->first->OnTimeout();
            } else {
                _TRACE("Closed " << dynamic_cast<TConnBase *>(it->first));
                conn->OnSocketEvent(eServIO_OurClose);
            }
            to_delete.push_back(it->first);
        }
    }
    int n_cleaned = 0;
    ITERATE(list<TConnBase*>, it, to_delete) {
        data.erase(*it);
        delete *it;
        ++n_cleaned;
    }
    // DEBUG if (n_cleaned)
    //  printf("Cleaned %d connections out of %d\n", n_cleaned, n_connections);
}


bool CServer_ConnectionPool::GetPollAndTimerVec(
    vector<CSocketAPI::SPoll>& polls,
    vector<IServer_ConnectionBase*>& timer_requests,
    STimeout* timer_timeout) const
{
    polls.clear();
    CMutexGuard guard(m_Mutex);
    const TData& data = const_cast<const TData&>(m_Data);
    // Control socket goes here as well
    polls.reserve(data.size()+1);
    polls.push_back(CSocketAPI::SPoll(
        dynamic_cast<CPollable*>(&m_ControlSocketForPoll), eIO_Read));
    CTime current_time(CTime::eEmpty, CTime::eGmt);
    const CTime* alarm_time = NULL;
    const CTime* min_alarm_time = NULL;
    bool alarm_time_defined = false;
//    _TRACE("Connection map of size " << data.size());
    ITERATE (TData, it, data) {
        // Check that socket is not processing packet - safeguards against
        // out-of-order packet processing by effectively pulling socket from
        // poll vector until it is done with previous packet. See comments in
        // server.cpp: CServer_Connection::CreateRequest() and
        // CServerConnectionRequest::Process()
//        _TRACE("Considering " << dynamic_cast<TConnBase *>(it->first));
        if (it->second.type != eActiveSocket && it->first->IsOpen()) {
            CPollable* pollable = dynamic_cast<CPollable*>(it->first);
            _ASSERT(pollable);
//            _TRACE("ConnBase " << dynamic_cast<TConnBase *>(it->first) << " inserted");
            polls.push_back(CSocketAPI::SPoll(pollable,
                it->first->GetEventsToPollFor(&alarm_time)));
            if (alarm_time != NULL) {
                if (!alarm_time_defined) {
                    alarm_time_defined = true;
                    current_time.SetCurrent();
                    min_alarm_time = *alarm_time > current_time ?
                        alarm_time : NULL;
                    timer_requests.clear();
                    timer_requests.push_back(it->first);
                } else if (min_alarm_time == NULL) {
                    if (*alarm_time <= current_time)
                        timer_requests.push_back(it->first);
                } else if (*alarm_time <= *min_alarm_time) {
                    if (*alarm_time != *min_alarm_time) {
                        min_alarm_time = *alarm_time > current_time ?
                            alarm_time : NULL;
                        timer_requests.clear();
                    }
                    timer_requests.push_back(it->first);
                }
                alarm_time = NULL;
            }
        }
    }
    if (alarm_time_defined) {
        if (min_alarm_time == NULL)
            timer_timeout->usec = timer_timeout->sec = 0;
        else {
            CTimeSpan span(min_alarm_time->DiffTimeSpan(current_time));
            if (span.GetCompleteSeconds() < 0 ||
                span.GetNanoSecondsAfterSecond() < 0)
                timer_timeout->usec = timer_timeout->sec = 0;
            else {
                timer_timeout->sec = (unsigned) span.GetCompleteSeconds();
                timer_timeout->usec = span.GetNanoSecondsAfterSecond() / 1000;
            }
        }
        return true;
    }
    return false;
}


void CServer_ConnectionPool::StartListening(void)
{
    CMutexGuard guard(m_Mutex);
    const TData& data = const_cast<const TData&>(m_Data);
    ITERATE (TData, it, data) {
        it->first->Activate();
    }
}


void CServer_ConnectionPool::StopListening(void)
{
    CMutexGuard guard(m_Mutex);
    const TData& data = const_cast<const TData&>(m_Data);
    ITERATE (TData, it, data) {
        it->first->Passivate();
    }
}

END_NCBI_SCOPE
