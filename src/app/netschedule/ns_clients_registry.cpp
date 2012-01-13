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
 *   NetSchedule clients registry
 *
 */

#include <ncbi_pch.hpp>

#include "ns_clients_registry.hpp"
#include "ns_handler.hpp"


BEGIN_NCBI_SCOPE


CNSClientsRegistry::CNSClientsRegistry() :
    m_LastID(0)
{}


size_t  CNSClientsRegistry::size(void) const
{
    return m_Clients.size();
}


// Called before any command is issued by the client
// Returns waiting port != 0 if the client has reset waiting on WGET
unsigned short  CNSClientsRegistry::Touch(CNSClientId &          client,
                                          CQueue *               queue,
                                          CNSAffinityRegistry &  aff_registry)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return 0;

    CWriteLockGuard                     guard(m_Lock);
    map< string, CNSClient >::iterator  known_client =
                                             m_Clients.find(client.GetNode());

    if (known_client == m_Clients.end()) {
        // The client is not known yet
        CNSClient       new_ns_client(client);
        unsigned int    client_id = x_GetNextID();

        new_ns_client.SetID(client_id);
        client.SetID(client_id);
        m_Clients[ client.GetNode() ] = new_ns_client;
        return 0;
    }

    unsigned short  wait_port = 0;

    // The client has connected before
    if (client.GetSession() != known_client->second.GetSession())
        // The client has changed the session => need to reset waiting if so
        wait_port = x_ResetWaiting(known_client->second, aff_registry);

    known_client->second.Touch(client, queue);
    client.SetID(known_client->second.GetID());
    return wait_port;
}


// Updates the submitter job.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::AddToSubmitted(const CNSClientId &  client,
                                         size_t               count)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CWriteLockGuard                     guard(m_Lock);
    map< string, CNSClient >::iterator  submitter = m_Clients.find(client.GetNode());

    if (submitter == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to set submitted job");

    submitter->second.RegisterSubmittedJobs(count);
    return;
}


// Updates the reader jobs list.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::AddToReading(const CNSClientId &  client,
                                       unsigned int         job_id)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CWriteLockGuard                     guard(m_Lock);
    map< string, CNSClient >::iterator  reader = m_Clients.find(client.GetNode());

    if (reader == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to set reading job");

    reader->second.RegisterReadingJob(job_id);
    return;
}


// Updates the worker node jobs list.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::AddToRunning(const CNSClientId &  client,
                                       unsigned int         job_id)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CWriteLockGuard                     guard(m_Lock);
    map< string, CNSClient >::iterator  worker_node = m_Clients.find(client.GetNode());

    if (worker_node == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to set running job");

    worker_node->second.RegisterRunningJob(job_id);
    return;
}


// Updates the worker node blacklisted jobs list.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::AddToBlacklist(const CNSClientId &  client,
                                         unsigned int         job_id)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CWriteLockGuard                     guard(m_Lock);
    map< string, CNSClient >::iterator  worker_node = m_Clients.find(client.GetNode());

    if (worker_node == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to set blacklisted job");

    worker_node->second.RegisterBlacklistedJob(job_id);
    return;
}


// Updates the reader jobs list.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::ClearReading(const CNSClientId &  client,
                                       unsigned int         job_id)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CWriteLockGuard                     guard(m_Lock);
    map< string, CNSClient >::iterator  reader = m_Clients.find(client.GetNode());

    if (reader != m_Clients.end())
        reader->second.UnregisterReadingJob(job_id);
    return;
}


// Updates the reader jobs list.
// Used when a job is timed out. At this moment there is no information about
// the node identifier so all the clients are checked.
void  CNSClientsRegistry::ClearReading(unsigned int  job_id)
{
    // The container itself is not changed.
    // The changes are in what the element holds.
    CReadLockGuard                      guard(m_Lock);
    map< string, CNSClient >::iterator  k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k)
        k->second.UnregisterReadingJob(job_id);
    return;
}


// Updates the client reading job list and its blacklist.
// Used when  the job is timed out and rescheduled for reading as well as when
// RDRB or FRED are received
void  CNSClientsRegistry::ClearReadingSetBlacklist(unsigned int  job_id)
{
    // The container itself is not changed.
    // The changes are in what the element holds.
    CReadLockGuard                      guard(m_Lock);
    map< string, CNSClient >::iterator  k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k)
        if (k->second.MoveReadingJobToBlacklist(job_id))
            return;
    return;
}


// Updates the worker node jobs list.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::ClearExecuting(const CNSClientId &  client,
                                         unsigned int         job_id)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CWriteLockGuard                     guard(m_Lock);
    map< string, CNSClient >::iterator  worker_node = m_Clients.find(client.GetNode());

    if (worker_node != m_Clients.end())
        worker_node->second.UnregisterRunningJob(job_id);
    return;
}


// Updates the worker node jobs list.
// Used when a job is timed out. At this moment there is no information about
// the node identifier so all the clients are checked.
void  CNSClientsRegistry::ClearExecuting(unsigned int  job_id)
{
    // The container itself is not changed.
    // The changes are in what the element holds.
    CReadLockGuard                      guard(m_Lock);
    map< string, CNSClient >::iterator  k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k)
        k->second.UnregisterRunningJob(job_id);
    return;
}


// Updates the worker node executing job list and its blacklist.
// Used when  the job is timed out and rescheduled. At this moment there is no
// information about the node identifier so all the clients are checked.
void  CNSClientsRegistry::ClearExecutingSetBlacklist(unsigned int  job_id)
{
    // The container itself is not changed.
    // The changes are in what the element holds.
    CReadLockGuard                      guard(m_Lock);
    map< string, CNSClient >::iterator  k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k)
        if (k->second.MoveRunningJobToBlacklist(job_id))
            return;
    return;
}


// Used when CLRN command is received
// provides WGET port != 0 if there was waiting reset
unsigned short
CNSClientsRegistry::ClearWorkerNode(const CNSClientId &  client,
                                    CQueue *             queue,
                                    CNSAffinityRegistry &  aff_registry)
{
    // The container itself is not changed.
    // The changes are in what the element holds.
    unsigned short                      wait_port = 0;
    CReadLockGuard                      guard(m_Lock);
    map< string, CNSClient >::iterator  worker_node = m_Clients.find(client.GetNode());

    if (worker_node != m_Clients.end()) {
        wait_port = x_ResetWaiting(worker_node->second, aff_registry);
        worker_node->second.Clear(client, queue);
    }

    return wait_port;
}


void CNSClientsRegistry::PrintClientsList(const CQueue *               queue,
                                          CNetScheduleHandler &        handler,
                                          const CNSAffinityRegistry &  aff_registry,
                                          bool                         verbose) const
{
    CReadLockGuard                              guard(m_Lock);
    map< string, CNSClient >::const_iterator    k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k)
        k->second.Print(k->first, queue, handler, aff_registry, verbose);

    return;
}


void  CNSClientsRegistry::SetWaiting(const CNSClientId &          client,
                                     unsigned short               port,
                                     const TNSBitVector &         aff_ids,
                                     CNSAffinityRegistry &        aff_registry)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CWriteLockGuard                     guard(m_Lock);
    map< string, CNSClient >::iterator  worker_node = m_Clients.find(client.GetNode());

    if (worker_node == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to set waiting attributes");

    worker_node->second.SetWaitPort(port);
    worker_node->second.RegisterWaitAffinities(aff_ids);
    aff_registry.SetWaitClientForAffinities(worker_node->second.GetID(),
                                            worker_node->second.GetWaitAffinities());
    return;
}


unsigned short
CNSClientsRegistry::ResetWaiting(const CNSClientId &    client,
                                 CNSAffinityRegistry &  aff_registry)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return 0;

    return ResetWaiting(client.GetNode(), aff_registry);
}


unsigned short
CNSClientsRegistry::ResetWaiting(const string &         node_name,
                                 CNSAffinityRegistry &  aff_registry)
{
    if (node_name.empty())
        return 0;

    CReadLockGuard                      guard(m_Lock);
    map< string, CNSClient >::iterator  worker_node = m_Clients.find(node_name);

    if (worker_node == m_Clients.end())
        return 0;

    return x_ResetWaiting(worker_node->second, aff_registry);
}


unsigned short
CNSClientsRegistry::x_ResetWaiting(CNSClient &            client,
                                   CNSAffinityRegistry &  aff_registry)
{
    // We need to reset affinities the client might had been waiting for
    // and reset the waiting port.
    aff_registry.RemoveWaitClientFromAffinities(client.GetID(),
                                                client.GetWaitAffinities());
    client.ClearWaitAffinities();
    return client.GetAndResetWaitPort();
}


static TNSBitVector     s_empty_vector = TNSBitVector();

TNSBitVector
CNSClientsRegistry::GetBlacklistedJobs(const CNSClientId &  client)
{
    if (!client.IsComplete())
        return s_empty_vector;

    CWriteLockGuard                             guard(m_Lock);
    map< string, CNSClient >::const_iterator    found = m_Clients.find(client.GetNode());

    if (found == m_Clients.end())
        return s_empty_vector;

    return found->second.GetBlacklistedJobs();
}


TNSBitVector  CNSClientsRegistry::GetPreferredAffinities(
                                const CNSClientId &  client) const
{
    if (!client.IsComplete())
        return s_empty_vector;

    return GetPreferredAffinities(client.GetNode());
}


TNSBitVector  CNSClientsRegistry::GetPreferredAffinities(
                                const string &  node) const
{
    if (node.empty())
        return s_empty_vector;

    CReadLockGuard                              guard(m_Lock);
    map< string, CNSClient >::const_iterator    found = m_Clients.find(node);

    if (found == m_Clients.end())
        return s_empty_vector;

    return found->second.GetPreferredAffinities();
}


TNSBitVector  CNSClientsRegistry::GetWaitAffinities(
                                const CNSClientId &  client) const
{
    if (!client.IsComplete())
        return s_empty_vector;

    return GetWaitAffinities(client.GetNode());
}


TNSBitVector  CNSClientsRegistry::GetWaitAffinities(
                                const string &  node) const
{
    if (node.empty())
        return s_empty_vector;

    CReadLockGuard                              guard(m_Lock);
    map< string, CNSClient >::const_iterator    found = m_Clients.find(node);

    if (found == m_Clients.end())
        return s_empty_vector;

    return found->second.GetWaitAffinities();
}


void  CNSClientsRegistry::UpdatePreferredAffinities(
                                const CNSClientId &   client,
                                const TNSBitVector &  aff_to_add,
                                const TNSBitVector &  aff_to_del)
{
    if (!client.IsComplete())
        return;

    CWriteLockGuard                     guard(m_Lock);
    map< string, CNSClient >::iterator  found = m_Clients.find(client.GetNode());

    if (found == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to update preferred affinities");

    found->second.AddPreferredAffinities(aff_to_add);
    found->second.RemovePreferredAffinities(aff_to_del);
    return;
}


bool
CNSClientsRegistry::IsRequestedAffinity(const string &         name,
                                        const TNSBitVector &   aff,
                                        bool                   use_preferred) const
{
    if (name.empty())
        return false;

    CReadLockGuard                              guard(m_Lock);
    map< string, CNSClient >::const_iterator    worker_node = m_Clients.find(name);

    if (worker_node == m_Clients.end())
        return false;

    return worker_node->second.IsRequestedAffinity(aff, use_preferred);
}


string  CNSClientsRegistry::GetNodeName(unsigned int  id) const
{
    CReadLockGuard                              guard(m_Lock);
    map< string, CNSClient >::const_iterator    k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k )
        if (k->second.GetID() == id)
            return k->first;
    return "";
}


unsigned int  CNSClientsRegistry::x_GetNextID(void)
{
    CFastMutexGuard     guard(m_LastIDLock);

    // 0 is an invalid value, so make sure the ID != 0
    ++m_LastID;
    if (m_LastID == 0)
        m_LastID = 1;
    return m_LastID;
}

END_NCBI_SCOPE

