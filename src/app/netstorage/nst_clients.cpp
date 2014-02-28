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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   NetStorage clients registry supporting facilities
 *
 */

#include <ncbi_pch.hpp>
#include <connect/ncbi_socket.hpp>

#include "nst_clients.hpp"


USING_NCBI_SCOPE;



CNSTClient::CNSTClient() :
    m_Type(0),
    m_Addr(0),
    m_RegistrationTime(CNSTPreciseTime::Current()),
    m_LastAccess(m_RegistrationTime),
    m_NumberOfBytesWritten(0),
    m_NumberOfBytesRead(0),
    m_NumberOfBytesRelocated(0),
    m_NumberOfObjectsWritten(0),
    m_NumberOfObjectsRead(0),
    m_NumberOfObjectsRelocated(0),
    m_NumberOfObjectsDeleted(0),
    m_NumberOfSockErrors(0)
{}



void CNSTClient::Touch(void)
{
    // Basically updates the last access time
    m_LastAccess = CNSTPreciseTime::Current();
}


CJsonNode  CNSTClient::serialize(void) const
{
    CJsonNode       client(CJsonNode::NewObjectNode());

    client.SetString("Application", m_Application);
    client.SetString("Service", m_Service);
    client.SetBoolean("TicketProvided", !m_Ticket.empty());
    client.SetString("Type", x_TypeAsString());
    client.SetString("PeerAddress", CSocketAPI::gethostbyaddr(m_Addr));
    client.SetString("RegistrationTime",
                     NST_FormatPreciseTime(m_RegistrationTime));
    client.SetString("LastAccess", NST_FormatPreciseTime(m_LastAccess));
    client.SetInteger("BytesWritten", m_NumberOfBytesWritten);
    client.SetInteger("BytesRead", m_NumberOfBytesRead);
    client.SetInteger("BytesRelocated", m_NumberOfBytesRelocated);
    client.SetInteger("ObjectsWritten", m_NumberOfObjectsWritten);
    client.SetInteger("ObjectsRead", m_NumberOfObjectsRead);
    client.SetInteger("ObjectsRelocated", m_NumberOfObjectsRelocated);
    client.SetInteger("ObjectsDeleted", m_NumberOfObjectsDeleted);
    client.SetInteger("SocketErrors", m_NumberOfSockErrors);

    return client;
}


string  CNSTClient::x_TypeAsString(void) const
{
    string      result;

    if (m_Type & eReader)
        result = "reader";

    if (m_Type & eWriter) {
        if (!result.empty())
            result += " | ";
        result += "writer";
    }

    if (m_Type & eAdministrator) {
        if (!result.empty())
            result += " | ";
        result += "administrator";
    }

    if (result.empty())
        return "unknown";
    return result;
}




CNSTClientRegistry::CNSTClientRegistry()
{}


size_t CNSTClientRegistry::size(void) const
{
    return m_Clients.size();
}


void  CNSTClientRegistry::Touch(const string &  client,
                                const string &  applications,
                                const string &  ticket,
                                const string &  service,
                                unsigned int    peer_address)
{
    if (client.empty())
        return;

    CMutexGuard                         guard(m_Lock);
    map<string, CNSTClient>::iterator   found = m_Clients.find(client);

    if (found != m_Clients.end()) {
        // Client with this name existed before. Update the information.
        found->second.Touch();
        found->second.SetApplication(applications);
        found->second.SetTicket(ticket);
        found->second.SetService(service);
        found->second.SetPeerAddress(peer_address);
        return;
    }

    CNSTClient      new_client;
    new_client.SetApplication(applications);
    new_client.SetTicket(ticket);
    new_client.SetService(service);
    new_client.SetPeerAddress(peer_address);
    m_Clients[client] = new_client;
}


void  CNSTClientRegistry::Touch(const string &  client)
{
    if (client.empty())
        return;

    CMutexGuard                         guard(m_Lock);
    map<string, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.Touch();
}


void  CNSTClientRegistry::RegisterSocketWriteError(const string &  client)
{
    if (client.empty())
        return;

    CMutexGuard                         guard(m_Lock);
    map<string, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.RegisterSocketWriteError();
}


void  CNSTClientRegistry::AppendType(const string &  client,
                                     unsigned int    type_to_append)
{
    if (client.empty())
        return;

    CMutexGuard                         guard(m_Lock);
    map<string, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AppendType(type_to_append);
}


void  CNSTClientRegistry::AddBytesWritten(const string &  client,
                                          size_t          count)
{
    if (client.empty())
        return;

    CMutexGuard                         guard(m_Lock);
    map<string, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AddBytesWritten(count);
}


void  CNSTClientRegistry::AddBytesRead(const string &  client,
                                       size_t          count)
{
    if (client.empty())
        return;

    CMutexGuard                         guard(m_Lock);
    map<string, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AddBytesRead(count);
}


void  CNSTClientRegistry::AddBytesRelocated(const string &  client,
                                            size_t          count)
{
    if (client.empty())
        return;

    CMutexGuard                         guard(m_Lock);
    map<string, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AddBytesRelocated(count);
}


void  CNSTClientRegistry::AddObjectsWritten(const string &  client,
                                            size_t          count)
{
    if (client.empty())
        return;

    CMutexGuard                         guard(m_Lock);
    map<string, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AddObjectsWritten(count);
}


void  CNSTClientRegistry::AddObjectsRead(const string &  client,
                                         size_t          count)
{
    if (client.empty())
        return;

    CMutexGuard                         guard(m_Lock);
    map<string, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AddObjectsRead(count);
}


void  CNSTClientRegistry::AddObjectsRelocated(const string &  client,
                                              size_t          count)
{
    if (client.empty())
        return;

    CMutexGuard                         guard(m_Lock);
    map<string, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AddObjectsRelocated(count);
}


void  CNSTClientRegistry::AddObjectsDeleted(const string &  client,
                                            size_t          count)
{
    if (client.empty())
        return;

    CMutexGuard                         guard(m_Lock);
    map<string, CNSTClient>::iterator   found = m_Clients.find(client);
    if (found != m_Clients.end())
        found->second.AddObjectsDeleted(count);
}


CJsonNode  CNSTClientRegistry::serialize(void) const
{
    CJsonNode       clients(CJsonNode::NewArrayNode());
    CMutexGuard     guard(m_Lock);

    for (map<string, CNSTClient>::const_iterator  k = m_Clients.begin();
         k != m_Clients.end(); ++k) {
        CJsonNode   single_client(k->second.serialize());

        single_client.SetString("Name", k->first);
        clients.Append(single_client);
    }

    return clients;
}

