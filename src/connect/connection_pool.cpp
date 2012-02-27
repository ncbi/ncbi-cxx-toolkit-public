/* $Id$
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
 *   Threaded server connection pool
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
    if (timeout != kDefaultTimeout  &&  timeout != kInfiniteTimeout) {
        expiration = GetFastLocalTime();
        expiration.AddSecond(timeout->sec, CTime::eIgnoreDaylight);
        expiration.AddNanoSecond(timeout->usec * 1000);
    } else {
        expiration.Clear();
    }
}


// CServer_ControlConnection
CStdRequest* CServer_ControlConnection::CreateRequest(
                                            EServIO_Event event,
                                            CServer_ConnectionPool& connPool,
                                            const STimeout* timeout)
{
    char buf[64];
    Read(buf, sizeof(buf));
    return NULL;
}

static void CheckIOStatus(EIO_Status io_status, const char* step)
{
    if (io_status != eIO_Success) {
        NCBI_THROW_FMT(CConnException, eConn,
                       "Cannot create signaling socket for internal use: " <<
                       step << ": " << IO_StatusStr(io_status));
    }
}

CServer_ConnectionPool::CServer_ConnectionPool(unsigned max_connections) :
        m_MaxConnections(max_connections)
{
    // Create internal signaling connection from m_ControlSocket to
    // m_ControlSocketForPoll
    CListeningSocket listener;
    static const STimeout kTimeout = { 10, 500000 }; // 500 ms // DEBUG
    CheckIOStatus(listener.Listen(0, 5, fSOCK_BindLocal), "Listen/Bind");
    CheckIOStatus(m_ControlSocket.Connect("127.0.0.1",
        listener.GetPort(eNH_HostByteOrder)), "Connect");
    m_ControlSocket.DisableOSSendDelay();
    // Set a (modest) timeout to prevent SetConnType from blocking forever
    // with m_Mutex held, which could deadlock the whole server.
    CheckIOStatus(m_ControlSocket.SetTimeout(eIO_Write,
        &kTimeout), "SetTimeout");
    CheckIOStatus(listener.Accept(m_ControlSocketForPoll), "Accept");
}

CServer_ConnectionPool::~CServer_ConnectionPool()
{
    Erase();
}

void CServer_ConnectionPool::Erase(void)
{
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);
    NON_CONST_ITERATE(TData, it, data) {
        CServer_Connection* conn = dynamic_cast<CServer_Connection*>(it->first);
        if (conn)
            conn->OnSocketEvent(eServIO_OurClose);
        else
            it->first->OnTimeout();

        delete it->first;
    }
    data.clear();
}

bool CServer_ConnectionPool::Add(TConnBase* conn, EConnType type)
{
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);
    if (data.size() >= m_MaxConnections)
        return false;

    if (data.find(conn) != data.end())
        abort();
    SPerConnInfo& info = data[conn];
    info.type = type;
    info.UpdateExpiration(conn);

    PingControlConnection();
    return true;
}

void CServer_ConnectionPool::Remove(TConnBase* conn)
{
    CMutexGuard guard(m_Mutex);
    m_Data.erase(conn);
}


void CServer_ConnectionPool::SetConnType(TConnBase* conn,
                                         EConnType  type,
                                         bool       must_exist /* = true */)
{
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);
    TData::iterator it = data.find(conn);
    if (it == data.end()) {
        if (must_exist)
            abort();
        return;
    }
    SPerConnInfo& info = it->second;
    if (info.type != eClosedSocket) {
        EConnType new_type = type;
        if (type == eInactiveSocket) {
            if (info.type == ePreDeferredSocket)
                new_type = eDeferredSocket;
            else if (info.type == ePreClosedSocket)
                new_type = eClosedSocket;
        }
        info.type = new_type;
        info.UpdateExpiration(conn);
    }
    // Signal poll cycle to re-read poll vector by sending
    // byte to control socket
    if (type == eInactiveSocket)
        PingControlConnection();
}

void CServer_ConnectionPool::PingControlConnection(void)
{
    CMutexGuard guard(m_Mutex);
    EIO_Status status = m_ControlSocket.Write("", 1, NULL, eIO_WritePlain);
    if (status != eIO_Success) {
        ERR_POST_X(4, Warning
                   << "PingControlConnection: failed to write to control socket: "
                   << IO_StatusStr(status));
    }
}


void CServer_ConnectionPool::CloseConnection(TConnBase* conn)
{
    {{
        CMutexGuard guard(m_Mutex);
        TData& data = const_cast<TData&>(m_Data);
        TData::iterator it = data.find(conn);
        if (it == data.end())
            abort();
        it->second.type = ePreClosedSocket;
    }}
    dynamic_cast<CServer_Connection*>(conn)->OnSocketEvent(eServIO_OurClose);
}

bool CServer_ConnectionPool::GetPollAndTimerVec(
                             vector<CSocketAPI::SPoll>& polls,
                             vector<IServer_ConnectionBase*>& timer_requests,
                             STimeout* timer_timeout,
                             vector<IServer_ConnectionBase*>& revived_conns,
                             vector<IServer_ConnectionBase*>& to_close_conns,
                             vector<IServer_ConnectionBase*>& to_delete_conns) const
{
    CTime now = GetFastLocalTime();
    polls.clear();
    revived_conns.clear();
    to_close_conns.clear();
    to_delete_conns.clear();

    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);
    // Control socket goes here as well
    polls.reserve(data.size()+1);
    polls.push_back(CSocketAPI::SPoll(
                    dynamic_cast<CPollable*>(&m_ControlSocketForPoll), eIO_Read));
    CTime current_time(CTime::eEmpty);
    const CTime* alarm_time = NULL;
    const CTime* min_alarm_time = NULL;
    bool alarm_time_defined = false;
    NON_CONST_ITERATE(TData, it, data) {
        // Check that socket is not processing packet - safeguards against
        // out-of-order packet processing by effectively pulling socket from
        // poll vector until it is done with previous packet. See comments in
        // server.cpp: CServer_Connection::CreateRequest() and
        // CServerConnectionRequest::Process()
        TConnBase* conn_base = it->first;
        SPerConnInfo& info = it->second;
        if (info.type == eClosedSocket
            ||  (info.type == eInactiveSocket  &&  !conn_base->IsOpen()))
        {
            // If it's not eClosedSocket then This connection was closed
            // by the client earlier in CServer::Run after Poll returned
            // eIO_Close which was converted into eServIO_ClientClose.
            // Then during OnSocketEvent(eServIO_ClientClose) it was marked
            // as closed. Here we just clean it up from the connection pool.
            to_delete_conns.push_back(conn_base);
        }
        else if (info.type == eInactiveSocket  &&  info.expiration <= now)
        {
            to_close_conns.push_back(conn_base);
        }
        else if ((info.type == eInactiveSocket  ||  info.type == eListener)
                 &&  conn_base->IsOpen())
        {
            CPollable* pollable = dynamic_cast<CPollable*>(conn_base);
            _ASSERT(pollable);
            polls.push_back(CSocketAPI::SPoll(pollable,
                            conn_base->GetEventsToPollFor(&alarm_time)));
            if (alarm_time != NULL) {
                if (!alarm_time_defined) {
                    alarm_time_defined = true;
                    current_time = GetFastLocalTime();
                    min_alarm_time = *alarm_time > current_time? alarm_time: NULL;
                    timer_requests.clear();
                    timer_requests.push_back(conn_base);
                } else if (min_alarm_time == NULL) {
                    if (*alarm_time <= current_time)
                        timer_requests.push_back(conn_base);
                } else if (*alarm_time <= *min_alarm_time) {
                    if (*alarm_time != *min_alarm_time) {
                        min_alarm_time = *alarm_time > current_time? alarm_time
                                                                   : NULL;
                        timer_requests.clear();
                    }
                    timer_requests.push_back(conn_base);
                }
                alarm_time = NULL;
            }
        }
        else if (info.type == eDeferredSocket
                 &&  conn_base->IsReadyToProcess())
        {
            info.type = eActiveSocket;
            revived_conns.push_back(conn_base);
        }
    }
    for (size_t i = 0; i < to_delete_conns.size(); ++i) {
        data.erase(to_delete_conns[i]);
    }
    for (size_t i = 0; i < to_close_conns.size(); ++i) {
        data.erase(to_close_conns[i]);
    }
    guard.Release();

    if (alarm_time_defined) {
        if (min_alarm_time == NULL)
            timer_timeout->usec = timer_timeout->sec = 0;
        else {
            CTimeSpan span(min_alarm_time->DiffTimeSpan(current_time));
            if (span.GetCompleteSeconds() < 0 ||
                span.GetNanoSecondsAfterSecond() < 0)
            {
                timer_timeout->usec = timer_timeout->sec = 0;
            }
            else {
                timer_timeout->sec = (unsigned) span.GetCompleteSeconds();
                timer_timeout->usec = span.GetNanoSecondsAfterSecond() / 1000;
            }
        }
        return true;
    }
    return false;
}

void CServer_ConnectionPool::SetAllActive(const vector<CSocketAPI::SPoll>& polls)
{
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);
    ITERATE(vector<CSocketAPI::SPoll>, it, polls) {
        if (!it->m_REvent) continue;
        IServer_ConnectionBase* conn_base =
                        dynamic_cast<IServer_ConnectionBase*>(it->m_Pollable);
        if (conn_base == &m_ControlSocketForPoll)
            continue;

        TData::iterator it_data = data.find(conn_base);
        if (it_data == data.end())
            abort();
        SPerConnInfo& info = it_data->second;
        if (info.type != eListener) {
            info.type = eActiveSocket;
            info.UpdateExpiration(conn_base);
        }
    }
}

void CServer_ConnectionPool::SetAllActive(const vector<IServer_ConnectionBase*>& conns)
{
    CMutexGuard guard(m_Mutex);
    TData& data = const_cast<TData&>(m_Data);
    ITERATE(vector<IServer_ConnectionBase*>, it, conns) {
        TData::iterator it_data = data.find(*it);
        if (it_data == data.end())
            abort();
        SPerConnInfo& info = it_data->second;
        info.type = eActiveSocket;
        info.UpdateExpiration(*it);
    }
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
