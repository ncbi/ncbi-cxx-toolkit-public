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
#include <corelib/request_ctx.hpp>

#include "ns_clients_registry.hpp"
#include "ns_affinity.hpp"
#include "ns_handler.hpp"


BEGIN_NCBI_SCOPE


CNSClientsRegistry::CNSClientsRegistry() :
    m_LastID(0), m_BlacklistTimeout(), m_ReadBlacklistTimeout()
{}


void CNSClientsRegistry::SetRegistries(CNSAffinityRegistry *  aff_registry,
                                       CNSNotificationList *  notif_registry)
{
    m_AffRegistry = aff_registry;
    m_NotifRegistry = notif_registry;
}


// Called before any command is issued by the client
void
CNSClientsRegistry::Touch(CNSClientId &          client,
                          TNSBitVector &         running_jobs,
                          TNSBitVector &         reading_jobs,
                          bool &                 client_was_found,
                          bool &                 session_was_reset,
                          string &               old_session,
                          bool &                 had_wn_pref_affs,
                          bool &                 had_reader_pref_affs)
{
    client_was_found = false;
    session_was_reset = false;
    old_session.clear();
    had_wn_pref_affs = false;
    had_reader_pref_affs = false;

    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  known_client =
                                             m_Clients.find(client.GetNode());

    if (known_client == m_Clients.end()) {
        // The client is not known yet
        CNSClient       new_ns_client(client, &m_BlacklistTimeout,
                                              &m_ReadBlacklistTimeout);
        unsigned int    client_id = x_GetNextID();

        new_ns_client.SetID(client_id);
        client.SetID(client_id);
        m_Clients[ client.GetNode() ] = new_ns_client;
        m_RegisteredClients.set_bit(client_id);
        return;
    }

    client_was_found = true;

    // The client has connected before
    client.SetID(known_client->second.GetID());
    old_session = known_client->second.GetSession();
    if (client.GetSession() != old_session) {
        session_was_reset = true;
        ClearClient(client, running_jobs, reading_jobs,
                    client_was_found, old_session, had_wn_pref_affs,
                    had_reader_pref_affs);
    }

    known_client->second.Touch(client);

    // The 'reset' client type must not reset the collected types when the next
    // command is issued
    if (client.GetType() == eClaimedReset)
        client.SetClientType(eClaimedAutodetect);
}


void CNSClientsRegistry::RegisterSocketWriteError(const CNSClientId &  client)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  cl = m_Clients.find(client.GetNode());
    if (cl != m_Clients.end())
        cl->second.RegisterSocketWriteError();
}


void CNSClientsRegistry::SetLastScope(const CNSClientId &  client)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  cl = m_Clients.find(client.GetNode());
    if (cl != m_Clients.end())
        cl->second.SetLastScope(client.GetScope());
}


void
CNSClientsRegistry::AppendType(const CNSClientId &  client,
                               unsigned int         type_to_append)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  cl = m_Clients.find(client.GetNode());
    if (cl != m_Clients.end())
        cl->second.AppendType(type_to_append);
}


void
CNSClientsRegistry::GCBlacklistedJobs(const CJobStatusTracker &  tracker,
                                      ECommandGroup              cmd_group)
{
    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k)
        k->second.GCBlacklistedJobs(tracker, cmd_group);
}


int
CNSClientsRegistry::SetClientData(const CNSClientId &  client,
                                  const string &  data, int  data_version)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   "only non-anonymous clients may set their data");

    CMutexGuard                     guard(m_Lock);
    map< string,
         CNSClient >::iterator      found = m_Clients.find(client.GetNode());

    if (found == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to set client data");

    return found->second.SetClientData(data, data_version);
}


void  CNSClientsRegistry::MarkAsAdmin(const CNSClientId &  client)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  admin = m_Clients.find(client.GetNode());

    if (admin != m_Clients.end())
        admin->second.MarkAsAdmin();
}


// Updates the submitter job.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::AddToSubmitted(const CNSClientId &  client,
                                         size_t               count)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  submitter = m_Clients.find(client.GetNode());

    if (submitter != m_Clients.end())
        submitter->second.RegisterSubmittedJobs(count);
}


// Updates the client jobs list.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::RegisterJob(const CNSClientId &  client,
                                      unsigned int         job_id,
                                      ECommandGroup        cmd_group)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  cl = m_Clients.find(client.GetNode());

    if (cl == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to register a job");

    cl->second.RegisterJob(job_id, cmd_group);
}


// Updates the client blacklisted jobs list.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::RegisterBlacklistedJob(const CNSClientId &  client,
                                                 unsigned int         job_id,
                                                 ECommandGroup        cmd_group)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  cl = m_Clients.find(client.GetNode());

    if (cl == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to set blacklisted job");

    cl->second.RegisterBlacklistedJob(job_id, cmd_group);
}


// Updates the client jobs list.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::UnregisterJob(const CNSClientId &  client,
                                        unsigned int         job_id,
                                        ECommandGroup        cmd_group)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  cl = m_Clients.find(client.GetNode());

    if (cl != m_Clients.end())
        cl->second.UnregisterJob(job_id, cmd_group);
}


// Updates the client jobs list.
// Used when a job is timed out. At this moment there is no information about
// the node identifier so all the clients are checked.
void  CNSClientsRegistry::UnregisterJob(unsigned int   job_id,
                                        ECommandGroup  cmd_group)
{
    // The container itself is not changed.
    // The changes are in what the element holds.
    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k)
        k->second.UnregisterJob(job_id, cmd_group);
}


// Moves the client job into a blacklist.
// No need to check session id, it's done in Touch()
void  CNSClientsRegistry::MoveJobToBlacklist(const CNSClientId &  client,
                                             unsigned int         job_id,
                                             ECommandGroup        cmd_group)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  cl = m_Clients.find(client.GetNode());

    if (cl != m_Clients.end())
        cl->second.MoveJobToBlacklist(job_id, cmd_group);
}


// Updates the client job list and its blacklist.
// Used when  the job is timed out and rescheduled as well as when
// RDRB/RETURN or FRED/FPUT are received
void  CNSClientsRegistry::MoveJobToBlacklist(unsigned int   job_id,
                                             ECommandGroup  cmd_group)
{
    // The container itself is not changed.
    // The changes are in what the element holds.
    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k)
        if (k->second.MoveJobToBlacklist(job_id, cmd_group))
            break;
}



// Used in the following situations:
// - CLRN
// - new session
// - GC for a client (it is not strictly required because it may happened only
//   for inactive clients, i.e. timeouted or after CLRN
void
CNSClientsRegistry::ClearClient(const CNSClientId &    client,
                                TNSBitVector &         running_jobs,
                                TNSBitVector &         reading_jobs,
                                bool &                 client_was_found,
                                string &               old_session,
                                bool &                 had_wn_pref_affs,
                                bool &                 had_reader_pref_affs)
{
    client_was_found = false;

    // Check if it is an old-style client
    if (client.IsComplete())
        ClearClient(client.GetNode(), running_jobs, reading_jobs,
                    client_was_found, old_session, had_wn_pref_affs,
                    had_reader_pref_affs);
}


void
CNSClientsRegistry::ClearClient(const string &         node_name,
                                TNSBitVector &         running_jobs,
                                TNSBitVector &         reading_jobs,
                                bool &                 client_was_found,
                                string &               old_session,
                                bool &                 had_wn_pref_affs,
                                bool &                 had_reader_pref_affs)
{
    client_was_found = false;

    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  cl = m_Clients.find(node_name);

    if (cl != m_Clients.end()) {
        client_was_found = true;
        old_session = cl->second.GetSession();
        cl->second.SetSession("");
        cl->second.SetState(CNSClient::eQuit);
        cl->second.SetClaimedType(eClaimedNotProvided);
        x_ClearClient(node_name, cl->second, running_jobs,
                      had_wn_pref_affs, eGet);
        x_ClearClient(node_name, cl->second, reading_jobs,
                      had_reader_pref_affs, eRead);
    }
}


// Must be called under a lock
// When a client inactivity timeout is detected there is no need to touch the
// running or reading jobs. The only things to be done are:
// - cancel waiting
// - reset pref affinities
// - set reset affinity flag
// - log the event
void
CNSClientsRegistry::ClearOnTimeout(CNSClient &      client,
                                   const string &   client_node,
                                   bool             is_log,
                                   ECommandGroup    cmd_group)
{
    // Deregister preferred affinities
    bool    had_pref_affs = client.HasPreferredAffinities(cmd_group);
    if (had_pref_affs) {
        m_AffRegistry->RemoveClientFromAffinities(
                                    client.GetID(),
                                    client.GetPreferredAffinities(cmd_group),
                                    cmd_group);
        client.ClearPreferredAffinities(cmd_group);

        if (is_log) {
            string      aff_part = "get";
            if (cmd_group == eRead)
                aff_part = "read";

            CRef<CRequestContext>   ctx;
            ctx.Reset(new CRequestContext());
            ctx->SetRequestID();
            GetDiagContext().SetRequestContext(ctx);
            GetDiagContext().PrintRequestStart()
                            .Print("_type", "client_watch")
                            .Print("client_node", client_node)
                            .Print("client_session", client.GetSession())
                            .Print(aff_part + "_preferred_affinities_reset",
                                   "yes");
            ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
            GetDiagContext().PrintRequestStop();
        }

        client.SetAffinityReset(true, cmd_group);
    }

    CancelWaiting(client, cmd_group);
    if (had_pref_affs)
        x_BuildAffinities(cmd_group);
}


string
CNSClientsRegistry::PrintClientsList(const CQueue *               queue,
                                     size_t                       batch_size,
                                     bool                         verbose) const
{
    TNSBitVector        batch;
    string              result;

    TNSBitVector                registered_clients = GetRegisteredClients();
    TNSBitVector::enumerator    en(registered_clients.first());

    while (en.valid()) {
        batch.set_bit(*en);
        ++en;

        if (batch.count() >= batch_size) {
            result += x_PrintSelected(batch, queue, verbose) + "\n";
            batch.clear();
        }
    }

    if (batch.count() > 0)
        result += x_PrintSelected(batch, queue, verbose) + "\n";
    return result;
}


string
CNSClientsRegistry::x_PrintSelected(const TNSBitVector &  batch,
                                    const CQueue *        queue,
                                    bool                  verbose) const
{
    string      buffer;
    size_t      printed = 0;

    CMutexGuard                                 guard(m_Lock);
    map< string, CNSClient >::const_iterator    k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k) {
        if (batch.get_bit(k->second.GetID())) {
            buffer += k->second.Print(k->first, queue, *m_AffRegistry,
                                      m_GCWNodeClients, m_GCReaderClients,
                                      verbose);
            ++printed;
            if (printed >= batch.count())
                break;
        }
    }

    return buffer;
}


void
CNSClientsRegistry::SetNodeWaiting(const CNSClientId &   client,
                                   unsigned short        port,
                                   const TNSBitVector &  aff_ids,
                                   ECommandGroup         cmd_group)
{
    // Check if it is an old-style client
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  node = m_Clients.find(client.GetNode());

    if (node == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to set waiting attributes");

    node->second.SetWaitPort(port, cmd_group);
    node->second.SetWaitAffinities(aff_ids, cmd_group);
    m_AffRegistry->SetWaitClientForAffinities(node->second.GetID(), aff_ids,
                                              cmd_group);
}


// Handles the following cases:
// - GET2/READ with waiting has been interrupted by another GET2/READ
// - CWGET/CWREAD received
// - GET2/READ wait timeout is over
// - notification timeout is over
bool CNSClientsRegistry::CancelWaiting(CNSClient &    client,
                                       ECommandGroup  cmd_group,
                                       bool           touch_notif_registry)
{
    bool        ret_val = false;
    // Deregister wait affinities if so
    if (client.HasWaitAffinities(cmd_group))
        m_AffRegistry->RemoveWaitClientFromAffinities(
                                            client.GetID(),
                                            client.GetWaitAffinities(cmd_group),
                                            cmd_group);

    // Remove from notifications if wait port is non-zero
    unsigned short  port = client.GetWaitPort(cmd_group);
    if (port != 0) {
        ret_val = true;

        // One of the cases is when a notification timeout is over.
        // In this case it is detected in the notification registry and the
        // corresponding record is already deleted.
        if (touch_notif_registry)
            m_NotifRegistry->UnregisterListener(client.GetPeerAddress(), port,
                                                cmd_group);
    }

    // Clear affinities and port
    client.CancelWaiting(cmd_group);
    return ret_val;
}


bool
CNSClientsRegistry::CancelWaiting(const CNSClientId &  client,
                                  ECommandGroup        cmd_group)
{
    // Check if it is an old-style client
    if (client.IsComplete())
        return CancelWaiting(client.GetNode(), cmd_group);
    return false;
}


bool
CNSClientsRegistry::CancelWaiting(const string &  node_name,
                                  ECommandGroup   cmd_group,
                                  bool            touch_notif_registry)
{
    if (node_name.empty())
        return false;

    CMutexGuard                         guard(m_Lock);
    map< string, CNSClient >::iterator  cl = m_Clients.find(node_name);

    if (cl != m_Clients.end())
        return CancelWaiting(cl->second, cmd_group, touch_notif_registry);
    return false;
}


void
CNSClientsRegistry::SubtractBlacklistedJobs(
                                const CNSClientId &  client,
                                ECommandGroup        cmd_group,
                                TNSBitVector &       bv) const
{
    if (client.IsComplete())
        SubtractBlacklistedJobs(client.GetNode(), cmd_group, bv);
}


void
CNSClientsRegistry::SubtractBlacklistedJobs(
                                const string &  client_node,
                                ECommandGroup   cmd_group,
                                TNSBitVector &  bv) const
{
    CMutexGuard                         guard(m_Lock);
    map< string,
         CNSClient >::const_iterator    found = m_Clients.find(client_node);

    if (found != m_Clients.end())
        found->second.SubtractBlacklistedJobs(cmd_group, bv);
}


void
CNSClientsRegistry::AddBlacklistedJobs(
                                const CNSClientId &  client,
                                ECommandGroup        cmd_group,
                                TNSBitVector &       bv) const
{
    if (client.IsComplete())
        AddBlacklistedJobs(client.GetNode(), cmd_group, bv);
}


void
CNSClientsRegistry::AddBlacklistedJobs(
                                const string &  client_node,
                                ECommandGroup   cmd_group,
                                TNSBitVector &  bv) const
{
    CMutexGuard                         guard(m_Lock);
    map< string,
         CNSClient >::const_iterator    found = m_Clients.find(client_node);

    if (found != m_Clients.end())
        found->second.AddBlacklistedJobs(cmd_group, bv);
}


TNSBitVector
CNSClientsRegistry::GetPreferredAffinities(const CNSClientId &  client,
                                           ECommandGroup        cmd_group) const
{
    if (!client.IsComplete())
        return kEmptyBitVector;
    return GetPreferredAffinities(client.GetNode(), cmd_group);
}


TNSBitVector
CNSClientsRegistry::GetPreferredAffinities(const string &  node,
                                           ECommandGroup   cmd_group) const
{
    if (node.empty())
        return kEmptyBitVector;

    CMutexGuard                                 guard(m_Lock);
    map< string, CNSClient >::const_iterator    found = m_Clients.find(node);

    if (found == m_Clients.end())
        return kEmptyBitVector;
    return found->second.GetPreferredAffinities(cmd_group);
}


TNSBitVector
CNSClientsRegistry::GetAllPreferredAffinities(ECommandGroup   cmd_group) const
{
    CMutexGuard     guard(m_Lock);

    if (cmd_group == eGet)
        return m_WNodeAffinities;
    return m_ReaderAffinities;
}


TNSBitVector
CNSClientsRegistry::GetWaitAffinities(const CNSClientId &  client,
                                      ECommandGroup        cmd_group) const
{
    if (!client.IsComplete())
        return kEmptyBitVector;
    return GetWaitAffinities(client.GetNode(), cmd_group);
}


TNSBitVector
CNSClientsRegistry::GetWaitAffinities(const string &  node,
                                      ECommandGroup   cmd_group) const
{
    if (node.empty())
        return kEmptyBitVector;

    CMutexGuard                                 guard(m_Lock);
    map< string, CNSClient >::const_iterator    found = m_Clients.find(node);

    if (found == m_Clients.end())
        return kEmptyBitVector;
    return found->second.GetWaitAffinities(cmd_group);
}


TNSBitVector  CNSClientsRegistry::GetRegisteredClients(void) const
{
    CMutexGuard         guard(m_Lock);
    return m_RegisteredClients;
}


void
CNSClientsRegistry::UpdatePreferredAffinities(const CNSClientId &   client,
                                              const TNSBitVector &  aff_to_add,
                                              const TNSBitVector &  aff_to_del,
                                              ECommandGroup         cmd_group)
{
    if (!client.IsComplete())
        return;

    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  found = m_Clients.find(client.GetNode());

    if (found == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to update preferred affinities");

    m_AffRegistry->AddClientToAffinities(client.GetID(), aff_to_add, cmd_group);
    m_AffRegistry->RemoveClientFromAffinities(client.GetID(), aff_to_del,
                                              cmd_group);

    found->second.AddPreferredAffinities(aff_to_add, cmd_group);
    found->second.RemovePreferredAffinities(aff_to_del, cmd_group);

    // Update the union bit vector with WN/reader affinities
    if (aff_to_del.any())
        x_BuildAffinities(cmd_group);
    else {
        if (aff_to_add.any()) {
            if (cmd_group == eGet)
                m_WNodeAffinities |= aff_to_add;
            else
                m_ReaderAffinities |= aff_to_add;
        }
    }
}


bool
CNSClientsRegistry::UpdatePreferredAffinities(const CNSClientId &   client,
                                              unsigned int          aff_to_add,
                                              unsigned int          aff_to_del,
                                              ECommandGroup         cmd_group)
{
    if (aff_to_add + aff_to_del == 0)
        return false;

    if (!client.IsComplete())
        return false;

    bool                        aff_added = false;
    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  found = m_Clients.find(client.GetNode());

    if (found == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to update preferred affinities");

    if (aff_to_add != 0) {
        m_AffRegistry->AddClientToAffinity(client.GetID(), aff_to_add,
                                           cmd_group);
        aff_added = found->second.AddPreferredAffinity(aff_to_add,
                                                       cmd_group);
    }

    if (aff_to_del != 0) {
        m_AffRegistry->RemoveClientFromAffinities(client.GetID(), aff_to_del,
                                              cmd_group);
        found->second.RemovePreferredAffinity(aff_to_del, cmd_group);
    }

    // Update the union bit vector with WN/reader affinities
    if (aff_to_del != 0)
        x_BuildAffinities(cmd_group);
    else {
        if (aff_added) {
            if (cmd_group == eGet)
                m_WNodeAffinities.set_bit(aff_to_add);
            else
                m_ReaderAffinities.set_bit(aff_to_add);
        }
    }
    return aff_added;
}


void
CNSClientsRegistry::SetPreferredAffinities(const CNSClientId &   client,
                                           const TNSBitVector &  aff_to_set,
                                           ECommandGroup         cmd_group)
{
    if (!client.IsComplete())
        return;

    string                      client_name = client.GetNode();
    CMutexGuard                 guard(m_Lock);
    map< string,
         CNSClient >::iterator  found = m_Clients.find(client_name);

    if (found == m_Clients.end())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Cannot find client '" + client.GetNode() +
                   "' to update preferred affinities");

    TNSBitVector    curr_affs = found->second.GetPreferredAffinities(cmd_group);
    TNSBitVector    aff_to_add = aff_to_set - curr_affs;
    TNSBitVector    aff_to_del = curr_affs - aff_to_set;

    if (aff_to_add.any())
        m_AffRegistry->AddClientToAffinities(client.GetID(), aff_to_add,
                                             cmd_group);

    if (aff_to_del.any())
        m_AffRegistry->RemoveClientFromAffinities(client.GetID(), aff_to_del,
                                              cmd_group);

    found->second.SetPreferredAffinities(aff_to_set, cmd_group);

    // Update the union bit vector with WN/reader affinities
    if (aff_to_del.any())
        x_BuildAffinities(cmd_group);
    else {
        if (aff_to_add.any()) {
            if (cmd_group == eGet)
                m_WNodeAffinities |= aff_to_add;
            else
                m_ReaderAffinities |= aff_to_add;
        }
    }

    // Remove the client from the GC collected, because it affects affinities
    // only which are re-set here anyway
    if (cmd_group == eGet)
        m_GCWNodeClients.erase(client_name);
    else
        m_GCReaderClients.erase(client_name);
}


bool
CNSClientsRegistry::IsRequestedAffinity(
                            const string &         name,
                            const TNSBitVector &   aff,
                            bool                   use_preferred,
                            ECommandGroup          cmd_group) const
{
    if (name.empty())
        return false;

    CMutexGuard                         guard(m_Lock);
    map< string,
         CNSClient >::const_iterator    node = m_Clients.find(name);

    if (node == m_Clients.end())
        return false;

    return node->second.IsRequestedAffinity(aff, use_preferred, cmd_group);
}


bool
CNSClientsRegistry::IsPreferredByAny(unsigned int   aff_id,
                                     ECommandGroup  cmd_group) const
{
    CMutexGuard     guard(m_Lock);
    if (cmd_group == eGet)
        return m_WNodeAffinities.get_bit(aff_id);
    return m_ReaderAffinities.get_bit(aff_id);
}


bool
CNSClientsRegistry::GetAffinityReset(const CNSClientId &   client,
                                     ECommandGroup         cmd_group) const
{
    if (!client.IsComplete())
        return false;

    CMutexGuard                                 guard(m_Lock);
    map< string, CNSClient >::const_iterator    found =
                                             m_Clients.find(client.GetNode());

    if (found == m_Clients.end())
        return false;

    return found->second.GetAffinityReset(cmd_group);
}


string  CNSClientsRegistry::GetNodeName(unsigned int  id) const
{
    CMutexGuard                                 guard(m_Lock);
    map< string, CNSClient >::const_iterator    k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k )
        if (k->second.GetID() == id)
            return k->first;
    return "";
}


// true if the node could be made stale
bool
CNSClientsRegistry::x_CouldBeStale(const CNSPreciseTime &  current_time,
                                   const CNSPreciseTime &  timeout,
                                   const CNSClient &       client,
                                   ECommandGroup           cmd_group)
{
    if (current_time - client.GetLastAccess() <= timeout)
        return false;

    // The client may wait on GET2/READ longer than the configured timeout.
    // Take the longest timeout of two if so.
    unsigned short      wait_port = client.GetWaitPort(cmd_group);
    unsigned int        wait_address = client.GetPeerAddress();
    if (wait_port != 0) {
        CNSPreciseTime      get_lifetime = m_NotifRegistry->
                                GetPassiveNotificationLifetime(wait_address,
                                                               wait_port,
                                                               cmd_group);
        if (current_time <= get_lifetime)
            return false;
    }
    return true;
}


void
CNSClientsRegistry::StaleNodes(const CNSPreciseTime &  current_time,
                               const CNSPreciseTime &  wn_timeout,
                               const CNSPreciseTime &  reader_timeout,
                               bool                    is_log)
{
    // Checks if any of the worker nodes are inactive for too long
    CNSClient::ENSClientState               state;
    unsigned int                            type;
    CMutexGuard                             guard(m_Lock);
    map< string, CNSClient >::iterator      k = m_Clients.begin();

    for ( ; k != m_Clients.end(); ++k ) {
        state = k->second.GetState();
        if (state == CNSClient::eQuit || state == CNSClient::eWNAndReaderStale)
            continue;

        type = k->second.GetType();
        if ((type & (CNSClient::eWorkerNode | CNSClient::eReader)) == 0)
            continue;

        if ((type & CNSClient::eWorkerNode) != 0 &&
            state != CNSClient::eWNStale) {
            // This an active WN which should be checked
            if (x_CouldBeStale(current_time, wn_timeout,
                               k->second, eGet)) {
                ClearOnTimeout(k->second, k->first, is_log, eGet);
                if (state == CNSClient::eActive)
                    k->second.SetState(CNSClient::eWNStale);
                else
                    k->second.SetState(CNSClient::eWNAndReaderStale);
            }
        }

        if ((type & CNSClient::eReader) != 0 &&
            state != CNSClient::eReaderStale) {
            // This an active reader which should be checked
            if (x_CouldBeStale(current_time, reader_timeout,
                               k->second, eRead)) {
                ClearOnTimeout(k->second, k->first, is_log, eRead);
                if (state == CNSClient::eActive)
                    k->second.SetState(CNSClient::eReaderStale);
                else
                    k->second.SetState(CNSClient::eWNAndReaderStale);
            }
        }
    }
}


void  CNSClientsRegistry::Purge(const CNSPreciseTime &  current_time,
                                const CNSPreciseTime &  timeout_worker_node,
                                unsigned int            min_worker_nodes,
                                const CNSPreciseTime &  timeout_admin,
                                unsigned int            min_admins,
                                const CNSPreciseTime &  timeout_submitter,
                                unsigned int            min_submitters,
                                const CNSPreciseTime &  timeout_reader,
                                unsigned int            min_readers,
                                const CNSPreciseTime &  timeout_unknown,
                                unsigned int            min_unknowns,
                                bool                    is_log)
{
    CMutexGuard     guard(m_Lock);

    x_PurgeWNodesAndReaders(current_time, timeout_worker_node, min_worker_nodes,
                            timeout_reader, min_readers, is_log);
    x_PurgeAdmins(current_time, timeout_admin, min_admins, is_log);
    x_PurgeSubmitters(current_time, timeout_submitter, min_submitters, is_log);
    x_PurgeUnknowns(current_time, timeout_unknown, min_unknowns, is_log);
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


// Must be called under the lock
void  CNSClientsRegistry::x_BuildAffinities(ECommandGroup  cmd_group)
{
    if (cmd_group == eRead) {
        m_ReaderAffinities.clear();
        for (map< string, CNSClient >::const_iterator
                k = m_Clients.begin(); k != m_Clients.end(); ++k )
            if (k->second.GetType() & CNSClient::eReader)
                m_ReaderAffinities |= k->second.GetPreferredAffinities(eRead);
    } else {
        m_WNodeAffinities.clear();
        for (map< string, CNSClient >::const_iterator
                k = m_Clients.begin(); k != m_Clients.end(); ++k )
            if (k->second.GetType() & CNSClient::eWorkerNode)
                m_WNodeAffinities |= k->second.GetPreferredAffinities(eGet);
    }
}


bool
CNSClientsRegistry::WasGarbageCollected(const CNSClientId &  client,
                                        ECommandGroup        cmd_group) const
{
    if (!client.IsComplete())
        return false;

    CMutexGuard                                 guard(m_Lock);
    if (cmd_group == eGet)
        return m_GCWNodeClients.find(client.GetNode()) !=
               m_GCWNodeClients.end();
    return m_GCReaderClients.find(client.GetNode()) !=
           m_GCReaderClients.end();
}


// Must be called under the lock
void
CNSClientsRegistry::x_ClearClient(const string &     node_name,
                                  CNSClient &        client,
                                  TNSBitVector &     jobs,
                                  bool &             had_pref_affs,
                                  ECommandGroup      cmd_group)
{
    jobs = client.GetJobs(cmd_group);
    client.ClearJobs(cmd_group);

    // Deregister preferred affinities
    had_pref_affs = client.HasPreferredAffinities(cmd_group);
    if (had_pref_affs) {
        m_AffRegistry->RemoveClientFromAffinities(
                                    client.GetID(),
                                    client.GetPreferredAffinities(cmd_group),
                                    cmd_group);
        client.ClearPreferredAffinities(cmd_group);
    }

    CancelWaiting(client, cmd_group);
    if (had_pref_affs)
        x_BuildAffinities(cmd_group);

    // Client has gone. No need to remember it if it was
    // garbage collected previously.
    if (cmd_group == eGet)
        m_GCWNodeClients.erase(node_name);
    else
        m_GCReaderClients.erase(node_name);
}


// Compares last access time of two clients basing on their identifiers
class AgeFunctor
{
    public:
        AgeFunctor(map< string, CNSClient > &  clients) :
            m_Clients(clients)
        {}

        bool operator () (const string &  lhs, const string &  rhs)
        {
            return m_Clients[ lhs ].GetLastAccess() <
                   m_Clients[ rhs ].GetLastAccess();
        }

    private:
        map< string, CNSClient > &    m_Clients;
};


// Must be called under the lock
void
CNSClientsRegistry::x_PurgeWNodesAndReaders(
                            const CNSPreciseTime &  current_time,
                            const CNSPreciseTime &  timeout_worker_node,
                            unsigned int            min_worker_nodes,
                            const CNSPreciseTime &  timeout_reader,
                            unsigned int            min_readers,
                            bool                    is_log)
{
    map< string, CNSClient >::iterator  k = m_Clients.begin();
    list< string >                      inactive_wns;
    list< string >                      inactive_readers;
    unsigned int                        total_wn_count = 0;
    unsigned int                        total_reader_count = 0;
    unsigned int                        type;
    CNSClient::ENSClientState           state;

    // Count total WNs and readers as well as detect purge candidates
    for ( ; k != m_Clients.end(); ++k ) {
        type = k->second.GetType();

        if ((type & CNSClient::eWorkerNode) != 0)
            ++total_wn_count;
        else if ((type & CNSClient::eReader) != 0)
            ++total_reader_count;
        else
            continue;

        state = k->second.GetState();
        if (state == CNSClient::eActive)
            continue;

        if ((type & (CNSClient::eWorkerNode | CNSClient::eReader)) ==
                CNSClient::eWorkerNode) {
            // The client is exclusively a worker node
            if (state != CNSClient::eWNStale &&
                state != CNSClient::eWNAndReaderStale &&
                state != CNSClient::eQuit)
                continue;

            // Test if the wn timeout is over
            if (current_time - k->second.GetLastAccess() >
                    timeout_worker_node)
                inactive_wns.push_back(k->first);

        } else if ((type & (CNSClient::eWorkerNode | CNSClient::eReader)) ==
                CNSClient::eReader) {
            // The client is exclusively a reader
            if (state != CNSClient::eReaderStale &&
                state != CNSClient::eWNAndReaderStale &&
                state != CNSClient::eQuit)
                continue;

            // Test if the reader timeout is over
            if (current_time - k->second.GetLastAccess() >
                    timeout_reader)
                inactive_readers.push_back(k->first);

        } else {
            // The client is both a worker node and a reader
            if (state != CNSClient::eWNAndReaderStale &&
                state != CNSClient::eQuit)
                continue;

            // Test bothe WN and reader timeouts
            if (current_time - k->second.GetLastAccess() >
                    timeout_worker_node &&
                current_time - k->second.GetLastAccess() >
                    timeout_reader) {
                inactive_wns.push_back(k->first);
                inactive_readers.push_back(k->first);
            }
        }
    }

    // Deal with worker nodes first
    if (total_wn_count > min_worker_nodes  &&  ! inactive_wns.empty()) {

        // Calculate the number of records to be deleted
        unsigned int    active_count = total_wn_count - inactive_wns.size();
        unsigned int    remove_count = 0;

        if (active_count >= min_worker_nodes)
            remove_count = inactive_wns.size();
        else
            remove_count = total_wn_count - min_worker_nodes;

        // Sort the inactive ones to delete the oldest
        inactive_wns.sort(AgeFunctor(m_Clients));

        // Delete the oldest
        for (list<string>::iterator  j = inactive_wns.begin();
             j != inactive_wns.end() && remove_count > 0; ++j, --remove_count) {

            // The affinity reset flag is NOT set if:
            // - the client issued CLEAR
            // - the client did not have preferred affinities
            // It makes sense to memo the client only if it had preferred
            // affinities.
            if (m_Clients[*j].GetAffinityReset(eGet))
                m_GCWNodeClients.insert(*j);
            if (m_Clients[*j].GetAffinityReset(eRead))
                m_GCReaderClients.insert(*j);

            if (m_GCWNodeClients.size() > 100000)
                ERR_POST("Garbage collected worker node list exceeds 100000 "
                         "records. There are currently " <<
                         m_GCWNodeClients.size() << " records.");
            if (m_GCReaderClients.size() > 100000)
                ERR_POST("Garbage collected reader list exceeds 100000 "
                         "records. There are currently " <<
                         m_GCReaderClients.size() << " records.");

            list<string>::iterator  found = find(inactive_readers.begin(),
                                                 inactive_readers.end(), *j);
            if (found != inactive_readers.end()) {
                // That was a reader too, so adjust the readers info
                inactive_readers.erase(found);
                --total_reader_count;
            }

            m_Clients.erase(*j);
        }
    }

    // Here: deal with readers consideraing that if it was a worker node too
    // then the client must not be removed.
    if (total_reader_count > min_readers  &&  ! inactive_readers.empty()) {

        // Calculate the number of records to be deleted
        unsigned int    active_count = total_reader_count -
                                                    inactive_readers.size();
        unsigned int    remove_count = 0;

        if (active_count >= min_readers)
            remove_count = inactive_readers.size();
        else
            remove_count = total_reader_count - min_readers;

        // Sort the inactive ones to delete the oldest
        inactive_readers.sort(AgeFunctor(m_Clients));

        // Delete the oldest
        for (list<string>::iterator  j = inactive_readers.begin();
             j != inactive_readers.end() && remove_count > 0; ++j) {

            if (find(inactive_wns.begin(), inactive_wns.end(), *j) !=
                                                            inactive_wns.end())
                continue;   // keep the reader because of WN limitations

            // The affinity reset flag is NOT set if:
            // - the client issued CLEAR
            // - the client did not have preferred affinities
            // It makes sense to memo the client only if it had preferred
            // affinities.
            if (m_Clients[*j].GetAffinityReset(eRead))
                m_GCReaderClients.insert(*j);


            if (m_GCReaderClients.size() > 100000)
                ERR_POST("Garbage collected reader list exceeds 100000 "
                         "records. There are currently " <<
                         m_GCReaderClients.size() << " records.");

            m_Clients.erase(*j);
            --remove_count;
        }
    }
}


// Must be called under the lock
void
CNSClientsRegistry::x_PurgeAdmins(const CNSPreciseTime &  current_time,
                                  const CNSPreciseTime &  timeout_admin,
                                  unsigned int            min_admins,
                                  bool                    is_log)
{
    x_PurgeInactiveClients(current_time, timeout_admin, min_admins,
                           CNSClient::eAdmin, is_log);
}


// Must be called under the lock
void
CNSClientsRegistry::x_PurgeSubmitters(const CNSPreciseTime &  current_time,
                                      const CNSPreciseTime &  timeout_submitter,
                                      unsigned int            min_submitters,
                                      bool                    is_log)
{
    x_PurgeInactiveClients(current_time, timeout_submitter, min_submitters,
                           CNSClient::eSubmitter, is_log);
}


// Must be called under the lock
void
CNSClientsRegistry::x_PurgeUnknowns(const CNSPreciseTime &  current_time,
                                    const CNSPreciseTime &  timeout_unknown,
                                    unsigned int            min_unknowns,
                                    bool                    is_log)
{
    x_PurgeInactiveClients(current_time, timeout_unknown, min_unknowns,
                           0, is_log);
}


void
CNSClientsRegistry::x_PurgeInactiveClients(const CNSPreciseTime &  current_time,
                                           const CNSPreciseTime &  timeout,
                                           unsigned int            min_clients,
                                           unsigned int            client_type,
                                           bool                    is_log)
{
    map< string, CNSClient >::iterator      k = m_Clients.begin();
    list< string >                          inactive;
    unsigned int                            inactive_count = 0;
    unsigned int                            total_count = 0;    // of given type
    unsigned int                            type;

    // Count active and inactive admins
    for ( ; k != m_Clients.end(); ++k ) {
        type = k->second.GetType();

        if (client_type == 0) {
            if (type != 0)
                continue;
        } else {
            if ((type & client_type) == 0)
                continue;
            // Worker nodes and readers are handled separately
            if (type & CNSClient::eWorkerNode || type & CNSClient::eReader)
                continue;
        }

        ++total_count;
        if (current_time - k->second.GetLastAccess() > timeout) {
            ++inactive_count;
            inactive.push_back(k->first);
        }
    }

    if (total_count <= min_clients || inactive_count == 0)
        return;

    // Calculate the number of records to be deleted
    unsigned int    active_count = total_count - inactive_count;
    unsigned int    remove_count = 0;
    if (active_count >= min_clients)
        remove_count = inactive_count;
    else
        remove_count = total_count - min_clients;

    // Sort the inactive ones to delete the oldest
    inactive.sort(AgeFunctor(m_Clients));

    // Delete the oldest records
    for (list<string>::iterator  j = inactive.begin();
         j != inactive.end() && remove_count > 0; ++j, --remove_count) {
        m_Clients.erase(*j);
    }
}

END_NCBI_SCOPE

