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


// Called before any command is issued by the client
void CNSClientsRegistry::Touch(const CNSClientId &  client,
                               CQueue *             queue)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CWriteLockGuard                     guard(m_Lock);
    map< string, CNSClient >::iterator  known_client = m_Clients.find(client.GetNode());

    if (known_client == m_Clients.end()) {
        // The client is not known yet
        CNSClient       new_ns_client(client);

        m_Clients[ client.GetNode() ] = new_ns_client;
    } else {
        // The client has connected before
        known_client->second.Touch(client, queue);
    }
    return;
}


// Updates the submitter job.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::AddToSubmitted(const CNSClientId &  client,
                                         unsigned int         job_id)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CWriteLockGuard                     guard(m_Lock);
    map< string, CNSClient >::iterator  submitter = m_Clients.find(client.GetNode());

    if (submitter == m_Clients.end()) {
        // This is going to be an extremely rare case when the record has been
        // deleted between Touch() and this calls. However it is better to be
        // on the safe side.
        CNSClient       new_ns_client(client);
        submitter = m_Clients.insert(pair<string, CNSClient>(client.GetNode(), new_ns_client)).first;
    }

    submitter->second.RegisterSubmittedJob(job_id);
    return;
}


// Updates the submitter jobs for batch submit.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::AddToSubmitted(const CNSClientId &  client,
                                         unsigned int         start_job_id,
                                         unsigned int         number_of_jobs)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CWriteLockGuard                     guard(m_Lock);
    map< string, CNSClient >::iterator  submitter = m_Clients.find(client.GetNode());

    if (submitter == m_Clients.end()) {
        // This is going to be an extremely rare case when the record has been
        // deleted between Touch() and this calls. However it is better to be
        // on the safe side.
        CNSClient       new_ns_client(client);
        submitter = m_Clients.insert(pair<string, CNSClient>(client.GetNode(), new_ns_client)).first;
    }

    submitter->second.RegisterSubmittedJobs(start_job_id, number_of_jobs);
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

    if (reader == m_Clients.end()) {
        // This is going to be an extremely rare case when the record has been
        // deleted between Touch() and this calls. However it is better to be
        // on the safe side.
        CNSClient       new_ns_client(client);
        reader = m_Clients.insert(pair<string, CNSClient>(client.GetNode(), new_ns_client)).first;
    }

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

    if (worker_node == m_Clients.end()) {
        // This is going to be an extremely rare case when the record has been
        // deleted between Touch() and this calls. However it is better to be
        // on the safe side.
        CNSClient       new_ns_client(client);
        worker_node = m_Clients.insert(pair<string, CNSClient>(client.GetNode(), new_ns_client)).first;
    }

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

    if (worker_node == m_Clients.end()) {
        // This is going to be an extremely rare case when the record has been
        // deleted between Touch() and this calls. However it is better to be
        // on the safe side.
        CNSClient       new_ns_client(client);
        worker_node = m_Clients.insert(pair<string, CNSClient>(client.GetNode(), new_ns_client)).first;
    }

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

    if (reader == m_Clients.end()) {
        // This is going to be an extremely rare case when the record has been
        // deleted between Touch() and this calls. However it is better to be
        // on the safe side.
        CNSClient       new_ns_client(client);
        reader = m_Clients.insert(pair<string, CNSClient>(client.GetNode(), new_ns_client)).first;
    }

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

    if (worker_node == m_Clients.end()) {
        // This is going to be an extremely rare case when the record has been
        // deleted between Touch() and this calls. However it is better to be
        // on the safe side.
        CNSClient       new_ns_client(client);
        worker_node = m_Clients.insert(pair<string, CNSClient>(client.GetNode(), new_ns_client)).first;
    }

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
void  CNSClientsRegistry::ClearWorkerNode(const CNSClientId &  client,
                                          CQueue *             queue)
{
    // The container itself is not changed.
    // The changes are in what the element holds.
    CReadLockGuard                      guard(m_Lock);
    map< string, CNSClient >::iterator  worker_node = m_Clients.find(client.GetNode());

    if (worker_node != m_Clients.end())
        worker_node->second.Clear(client, queue);
    return;
}


void CNSClientsRegistry::PrintClientsList(const CQueue *         queue,
                                          CNetScheduleHandler &  handler,
                                          bool                   verbose) const
{
    CReadLockGuard                              guard(m_Lock);
    map< string, CNSClient >::const_iterator    k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k)
        k->second.Print(k->first, queue, handler, verbose);

    return;
}


void  CNSClientsRegistry::SetWaitPort(const CNSClientId &  client,
                                      unsigned short       port)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CWriteLockGuard                     guard(m_Lock);
    map< string, CNSClient >::iterator  worker_node = m_Clients.find(client.GetNode());

    if (worker_node == m_Clients.end()) {
        // This is going to be an extremely rare case when the record has been
        // deleted between a command from the workernode and this call. 
        // However it is better to be on the safe side.
        CNSClient       new_ns_client(client);
        worker_node = m_Clients.insert(pair<string, CNSClient>(client.GetNode(), new_ns_client)).first;
    }

    worker_node->second.SetWaitPort(port);
    return;
}


unsigned short
CNSClientsRegistry::GetAndResetWaitPort(const CNSClientId &  client)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return 0;

    CReadLockGuard                      guard(m_Lock);
    map< string, CNSClient >::iterator  worker_node = m_Clients.find(client.GetNode());

    if (worker_node == m_Clients.end())
        return 0;

    return worker_node->second.GetAndResetWaitPort();
}



static TNSBitVector     s_no_jobs = TNSBitVector();

TNSBitVector
CNSClientsRegistry::GetBlacklistedJobs(const CNSClientId &  client)
{
    if (!client.IsComplete())
        return s_no_jobs;

    CWriteLockGuard                             guard(m_Lock);
    map< string, CNSClient >::const_iterator    found = m_Clients.find(client.GetNode());

    if (found == m_Clients.end()) {
        // This is going to be an extremely rare case when the record has been
        // deleted between Touch() and this calls. However it is better to be
        // on the safe side.
        CNSClient       new_ns_client(client);
        found = m_Clients.insert(pair<string, CNSClient>(client.GetNode(), new_ns_client)).first;
    }

    return found->second.GetBlacklistedJobs();
}


END_NCBI_SCOPE

