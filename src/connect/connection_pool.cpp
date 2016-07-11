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

std::string g_ServerConnTypeToString(enum EServerConnType  conn_type)
{
    switch (conn_type) {
        case eInactiveSocket:       return "eInactiveSocket";
        case eActiveSocket:         return "eActiveSocket";
        case eListener:             return "eListener";
        case ePreDeferredSocket:    return "ePreDeferredSocket";
        case eDeferredSocket:       return "eDeferredSocket";
        case ePreClosedSocket:      return "ePreClosedSocket";
        case eClosedSocket:         return "eClosedSocket";
    }
    return "UnknownServerConnType";
}


CServer_ConnectionPool::CServer_ConnectionPool(unsigned max_connections) :
    m_MaxConnections(max_connections), m_ListeningStarted(false)
{}

CServer_ConnectionPool::~CServer_ConnectionPool()
{
    Erase();
}

void CServer_ConnectionPool::Erase(void)
{
    CMutexGuard guard(m_Mutex);
    NON_CONST_ITERATE(TData, it, m_Data) {
        CServer_Connection* conn = dynamic_cast<CServer_Connection*>(*it);
        if (conn)
            conn->OnSocketEvent(eServIO_OurClose);
        else
            (*it)->OnTimeout();

        delete *it;
    }
    m_Data.clear();
}

void CServer_ConnectionPool::x_UpdateExpiration(TConnBase* conn)
{
    const STimeout* timeout = kDefaultTimeout;
    const CSocket*  socket  = dynamic_cast<const CSocket*>(conn);

    if (socket) {
        timeout = socket->GetTimeout(eIO_ReadWrite);
    }
    if (timeout != kDefaultTimeout  &&  timeout != kInfiniteTimeout) {
        conn->expiration = GetFastLocalTime();
        conn->expiration.AddSecond(timeout->sec, CTime::eIgnoreDaylight);
        conn->expiration.AddNanoSecond(timeout->usec * 1000);
    } else {
        conn->expiration.Clear();
    }
}

bool CServer_ConnectionPool::Add(TConnBase* conn, EServerConnType type)
{
    conn->type_lock.Lock();
    x_UpdateExpiration(conn);
    conn->type = type;
    conn->type_lock.Unlock();

    {{
        CMutexGuard guard(m_Mutex);
        if (m_Data.size() >= m_MaxConnections)
            return false;

        if (m_Data.find(conn) != m_Data.end())
            abort();
        m_Data.insert(conn);
    }}

    if (type == eListener)
        if (m_ListeningStarted)
            // That's a new listener which should be activated right away
            // because the StartListening() had already been called earlier
            // (e.g. in CServer::Run())
            conn->Activate();

    PingControlConnection();
    return true;
}

void CServer_ConnectionPool::Remove(TConnBase* conn)
{
    CMutexGuard guard(m_Mutex);
    m_Data.erase(conn);
}


bool CServer_ConnectionPool::RemoveListener(unsigned short  port)
{
    bool        found = false;

    {{
        CMutexGuard     guard(m_Mutex);

        if (std::find(m_ListenerPortsToStop.begin(),
                      m_ListenerPortsToStop.end(), port) !=
                                                m_ListenerPortsToStop.end()) {
            ERR_POST(Warning << "Removing listener on port " << port <<
                     " which has already been requested for removal");
            return false;
        }

        ITERATE (TData, it, m_Data) {
            TConnBase *         conn_base = *it;

            conn_base->type_lock.Lock();    // To be on the safe side
            if (conn_base->type == eListener) {
                CServer_Listener *  listener = dynamic_cast<CServer_Listener *>(
                                                                    conn_base);
                if (listener) {
                    if (listener->GetPort() == port) {
                        m_ListenerPortsToStop.push_back(port);
                        conn_base->type_lock.Unlock();

                        found = true;
                        break;
                    }
                }
            }
            conn_base->type_lock.Unlock();
        }
    }}

    if (found)
        PingControlConnection();
    else
        ERR_POST(Warning << "No listener on port " << port << " found");
    return found;
}


void CServer_ConnectionPool::SetConnType(TConnBase* conn, EServerConnType type)
{
    conn->type_lock.Lock();
    if (conn->type != eClosedSocket) {
        EServerConnType new_type = type;
        if (type == eInactiveSocket) {
            if (conn->type == ePreDeferredSocket)
                new_type = eDeferredSocket;
            else if (conn->type == ePreClosedSocket)
                new_type = eClosedSocket;
            else
                x_UpdateExpiration(conn);
        }
        conn->type = new_type;
    }
    conn->type_lock.Unlock();

    // Signal poll cycle to re-read poll vector by sending
    // byte to control socket
    if (type == eInactiveSocket)
        PingControlConnection();
}

void CServer_ConnectionPool::PingControlConnection(void)
{
    EIO_Status status = m_ControlTrigger.Set();
    if (status != eIO_Success) {
        ERR_POST_X(4, Warning
                   << "PingControlConnection: failed to set control trigger: "
                   << IO_StatusStr(status));
    }
}


void CServer_ConnectionPool::CloseConnection(TConnBase* conn)
{
    conn->type_lock.Lock();
    if (conn->type != eActiveSocket  &&  conn->type != ePreDeferredSocket
        &&  conn->type != ePreClosedSocket)
    {
        ERR_POST(Critical << "Unexpected connection type (" <<
                 g_ServerConnTypeToString(conn->type) <<
                 ") when closing the connection. Ignore and continue.");
        conn->type_lock.Unlock();
        return;
    }
    conn->type = ePreClosedSocket;
    conn->type_lock.Unlock();

    CServer_Connection* srv_conn = static_cast<CServer_Connection*>(conn);
    srv_conn->Abort();
    srv_conn->OnSocketEvent(eServIO_OurClose);
}

bool CServer_ConnectionPool::GetPollAndTimerVec(
                             vector<CSocketAPI::SPoll>& polls,
                             vector<IServer_ConnectionBase*>& timer_requests,
                             STimeout* timer_timeout,
                             vector<IServer_ConnectionBase*>& revived_conns,
                             vector<IServer_ConnectionBase*>& to_close_conns,
                             vector<IServer_ConnectionBase*>& to_delete_conns)
{
    CTime now = GetFastLocalTime();
    polls.clear();
    revived_conns.clear();
    to_close_conns.clear();
    to_delete_conns.clear();

    const CTime *   alarm_time = NULL;
    const CTime *   min_alarm_time = NULL;
    bool            alarm_time_defined = false;
    CTime           current_time(CTime::eEmpty);

    CMutexGuard     guard(m_Mutex);

    // Control trigger goes here as well
    polls.push_back(CSocketAPI::SPoll(&m_ControlTrigger, eIO_Read));

    ERASE_ITERATE(TData, it, m_Data) {
        // Check that socket is not processing packet - safeguards against
        // out-of-order packet processing by effectively pulling socket from
        // poll vector until it is done with previous packet. See comments in
        // server.cpp: CServer_Connection::CreateRequest() and
        // CServerConnectionRequest::Process()
        TConnBase* conn_base = *it;
        conn_base->type_lock.Lock();
        EServerConnType conn_type = conn_base->type;

        // There might be a request to delete a listener
        if (conn_type == eListener) {
            CServer_Listener *  listener = dynamic_cast<CServer_Listener *>(
                                                                    conn_base);
            if (listener) {
                unsigned short  port = listener->GetPort();

                vector<unsigned short>::iterator    port_it = 
                        std::find(m_ListenerPortsToStop.begin(),
                                  m_ListenerPortsToStop.end(), port);
                if (port_it != m_ListenerPortsToStop.end()) {
                    conn_base->type_lock.Unlock();
                    m_ListenerPortsToStop.erase(port_it);
                    delete conn_base;
                    m_Data.erase(it);
                    continue;
                }
            }
        }


        if (conn_type == eClosedSocket
            ||  (conn_type == eInactiveSocket  &&  !conn_base->IsOpen()))
        {
            // If it's not eClosedSocket then This connection was closed
            // by the client earlier in CServer::Run after Poll returned
            // eIO_Close which was converted into eServIO_ClientClose.
            // Then during OnSocketEvent(eServIO_ClientClose) it was marked
            // as closed. Here we just clean it up from the connection pool.
            to_delete_conns.push_back(conn_base);
            m_Data.erase(it);
        }
        else if (conn_type == eInactiveSocket  &&  conn_base->expiration <= now)
        {
            to_close_conns.push_back(conn_base);
            m_Data.erase(it);
        }
        else if ((conn_type == eInactiveSocket  ||  conn_type == eListener)
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
        else if (conn_type == eDeferredSocket  &&  conn_base->IsReadyToProcess())
        {
            conn_base->type = eActiveSocket;
            revived_conns.push_back(conn_base);
        }
        conn_base->type_lock.Unlock();
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
    ITERATE(vector<CSocketAPI::SPoll>, it, polls) {
        if (!it->m_REvent)
            continue;

        CTrigger *      trigger = dynamic_cast<CTrigger *>(it->m_Pollable);
        if (trigger)
            continue;

        IServer_ConnectionBase* conn_base =
                        dynamic_cast<IServer_ConnectionBase*>(it->m_Pollable);

        conn_base->type_lock.Lock();
        if (conn_base->type == eInactiveSocket)
            conn_base->type = eActiveSocket;
        else if (conn_base->type != eListener)
            abort();
        conn_base->type_lock.Unlock();
    }
}

void CServer_ConnectionPool::SetAllActive(const vector<IServer_ConnectionBase*>& conns)
{
    ITERATE(vector<IServer_ConnectionBase*>, it, conns) {
        IServer_ConnectionBase* conn_base = *it;
        conn_base->type_lock.Lock();
        if (conn_base->type != eInactiveSocket)
            abort();
        conn_base->type = eActiveSocket;
        conn_base->type_lock.Unlock();
    }
}


void CServer_ConnectionPool::StartListening(void)
{
    CMutexGuard guard(m_Mutex);
    ITERATE (TData, it, m_Data) {
        (*it)->Activate();
    }
    m_ListeningStarted = true;
}


void CServer_ConnectionPool::StopListening(void)
{
    CMutexGuard guard(m_Mutex);
    ITERATE (TData, it, m_Data) {
        (*it)->Passivate();
    }
}


vector<unsigned short>  CServer_ConnectionPool::GetListenerPorts(void)
{
    vector<unsigned short>      ports;

    CMutexGuard guard(m_Mutex);
    ITERATE (TData, it, m_Data) {
        TConnBase *         conn_base = *it;

        conn_base->type_lock.Lock();    // To be on the safe side
        if (conn_base->type == eListener) {
            CServer_Listener *  listener = dynamic_cast<CServer_Listener *>(
                                                                    conn_base);
            if (listener) {
                unsigned short  port = listener->GetPort();

                if (std::find(m_ListenerPortsToStop.begin(),
                              m_ListenerPortsToStop.end(), port) ==
                                                m_ListenerPortsToStop.end())
                    ports.push_back(listener->GetPort());
            }
        }
        conn_base->type_lock.Unlock();
    }

    return ports;
}

END_NCBI_SCOPE
