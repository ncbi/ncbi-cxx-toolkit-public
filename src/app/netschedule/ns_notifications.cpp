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
 * Authors:  Anatoliy Kuznetsov, Sergey Satskiy
 *
 * File Description: Support for notifying clients which wait for a job
 *                   after issuing the WGET command. The support includes
 *                   a list of active notifications and a thread which sends
 *                   notifications.
 */

#include <ncbi_pch.hpp>
#include <corelib/request_ctx.hpp>

#include "ns_notifications.hpp"
#include "queue_database.hpp"
#include "ns_handler.hpp"

#include "ns_clients.hpp"
#include "ns_clients_registry.hpp"
#include "ns_affinity.hpp"


BEGIN_NCBI_SCOPE


string  SNSNotificationAttributes::Print(
                       const CNSClientsRegistry &   clients_registry,
                       const CNSAffinityRegistry &  aff_registry,
                       bool                         is_active,
                       bool                         verbose) const
{
    static string   s_True  = "TRUE\n";
    static string   s_False = "FALSE\n";
    string          buffer;

    buffer += "OK:CLIENT: '" + m_ClientNode + "'\n"
              "OK:  RECEPIENT ADDRESS: " +
                    CSocketAPI::gethostbyaddr(m_Address) + ":" +
                    NStr::IntToString(m_Port) + "\n";

    CTime   lifetime(m_Lifetime);
    lifetime.ToLocalTime();
    buffer += "OK:  LIFE TIME: " + lifetime.AsString() + "\n";

    buffer += "OK:  ANY JOB: ";
    if (m_AnyJob)
        buffer += s_True;
    else
        buffer += s_False;

    if (verbose == false) {
        buffer += "OK:  EXPLICIT AFFINITIES: n/a (available in VERBOSE mode)\n";
    }
    else {
        if (m_ClientNode.empty())
            buffer += "OK:  EXPLICIT AFFINITIES: CLIENT NOT FOUND\n";
        else {
            TNSBitVector    wait_aff = clients_registry.GetWaitAffinities(m_ClientNode);

            if (wait_aff.any()) {
                buffer += "OK:  EXPLICIT AFFINITIES:\n";

                TNSBitVector::enumerator    en(wait_aff.first());
                for ( ; en.valid(); ++en)
                    buffer += "OK:    '" + aff_registry.GetTokenByID(*en) + "'\n";
            }
            else
                buffer += "OK:  EXPLICIT AFFINITIES: NONE\n";
        }
    }

    if (verbose == false) {
        buffer += "OK:  USE PREFERRED AFFINITIES: ";
        if (m_WnodeAff)
            buffer += s_True;
        else
            buffer += s_False;
    }
    else {
        if (m_WnodeAff) {

            // Need to print resolved preferred affinities
            TNSBitVector    pref_aff = clients_registry.GetPreferredAffinities(m_ClientNode);

            if (pref_aff.any()) {
                buffer += "OK:  USE PREFERRED AFFINITIES:\n";

                TNSBitVector::enumerator    en(pref_aff.first());
                for ( ; en.valid(); ++en)
                    buffer += "OK:    '" + aff_registry.GetTokenByID(*en) + "'\n";
            }
            else
                buffer += "OK:  USE PREFERRED AFFINITIES: NONE\n";
        }
        else
            buffer += "OK:  USE PREFERRED AFFINITIES: FALSE\n";
    }

    buffer += "OK:  EXCLUSIVE NEW AFFINITY: ";
    if (m_ExclusiveNewAff)
        buffer += s_True;
    else
        buffer += s_False;


    buffer += "OK:  ACTIVE: ";
    if (is_active)
        buffer += s_True;
    else
        buffer += s_False;

    if (m_HifreqNotifyLifetime != 0) {
        CTime   highfreq_lifetime(m_HifreqNotifyLifetime);
        highfreq_lifetime.ToLocalTime();
        buffer += "OK:  HIGH FREQUENCY LIFE TIME: " +
                  highfreq_lifetime.AsString() + "\n";
    }
    else
        buffer += "OK:  HIGH FREQUENCY LIFE TIME: n/a\n";

    buffer += "OK:  SLOW RATE ACTIVE: ";
    if (m_SlowRate)
        buffer += s_True;
    else
        buffer += s_False;

    return buffer;
}


CNSNotificationList::CNSNotificationList(const string &  ns_node,
                                         const string &  qname)
{
    m_JobStateConstPartLength = snprintf(m_JobStateConstPart,
                                         k_MessageBufferSize,
                                         "ns_node=%s&job_key=",
                                         ns_node.c_str());

    m_GetMsgLength = snprintf(m_GetMsgBuffer, k_MessageBufferSize,
                              "ns_node=%s&queue=%s",
                              ns_node.c_str(), qname.c_str()) + 1;
    m_GetMsgLengthObsoleteVersion =
                     snprintf(m_GetMsgBufferObsoleteVersion,
                              k_MessageBufferSize,
                              "NCBI_JSQ_%s", qname.c_str()) + 1;
}


void CNSNotificationList::RegisterListener(const CNSClientId &   client,
                                           unsigned short        port,
                                           unsigned int          timeout,
                                           bool                  wnode_aff,
                                           bool                  any_job,
                                           bool                  exclusive_new_affinity,
                                           bool                  new_format)
{
    unsigned int                                address = client.GetAddress();
    list<SNSNotificationAttributes>::iterator   found;
    CMutexGuard                                 guard(m_ListenersLock);


    found = x_FindListener(m_PassiveListeners, address, port);
    if (found != m_PassiveListeners.end()) {
        // Passive was here
        found->m_Lifetime = time(0) + timeout;
        found->m_ClientNode = client.GetNode();
        found->m_WnodeAff = wnode_aff;
        found->m_AnyJob = any_job;
        found->m_ExclusiveNewAff = exclusive_new_affinity;
        found->m_NewFormat = new_format;
        found->m_HifreqNotifyLifetime = 0;
        found->m_SlowRate = false;
        found->m_SlowRateCount = 0;

        // If it is an old client => there is no record in the clients
        // registry, so there is no information stored of what explicit
        // affinities were provided even so.  Therefore, the old clients
        // should be notified when any job is available.
        if (found->m_ClientNode.empty())
            found->m_AnyJob = true;

        return;
    }

    found = x_FindListener(m_ActiveListeners, address, port);
    if (found != m_ActiveListeners.end()) {
        // Active was here - remove it from the active list and insert a new
        // record into the passive list as it would be absolutely new one.
        m_ActiveListeners.erase(found);
    }


    // New record should be inserted
    SNSNotificationAttributes   attributes;

    attributes.m_Address = address;
    attributes.m_Port = port;
    attributes.m_Lifetime = time(0) + timeout;
    attributes.m_ClientNode = client.GetNode();
    attributes.m_WnodeAff = wnode_aff;
    attributes.m_AnyJob = any_job;
    attributes.m_ExclusiveNewAff = exclusive_new_affinity;
    attributes.m_NewFormat = new_format;
    attributes.m_HifreqNotifyLifetime = 0;
    attributes.m_SlowRate = false;
    attributes.m_SlowRateCount = 0;

    // See the remark above about old clients.
    if (attributes.m_ClientNode.empty())
        attributes.m_AnyJob = true;

    m_PassiveListeners.push_back(attributes);
    return;
}


void CNSNotificationList::UnregisterListener(const CNSClientId &  client,
                                             unsigned short       port)
{
    UnregisterListener(client.GetAddress(), port);
    return;
}


void CNSNotificationList::UnregisterListener(unsigned int         address,
                                             unsigned short       port)
{
    CMutexGuard                                 guard(m_ListenersLock);
    list<SNSNotificationAttributes>::iterator   found;

    found = x_FindListener(m_PassiveListeners, address, port);
    if (found != m_PassiveListeners.end()) {
        m_PassiveListeners.erase(found);
        return;
    }

    found = x_FindListener(m_ActiveListeners, address, port);
    if (found != m_ActiveListeners.end())
        m_ActiveListeners.erase(found);

    return;
}


void CNSNotificationList::NotifyJobStatus(unsigned int    address,
                                          unsigned short  port,
                                          const string &  job_key,
                                          TJobStatus      job_status,
                                          size_t          last_event_index  // zero based
                                          )
{
    char    buffer[k_MessageBufferSize];

    memcpy(buffer, m_JobStateConstPart, m_JobStateConstPartLength);
    strncpy(buffer + m_JobStateConstPartLength,
            job_key.c_str(), k_MessageBufferSize - m_JobStateConstPartLength);
    snprintf(buffer + m_JobStateConstPartLength + job_key.size(),
             k_MessageBufferSize - m_JobStateConstPartLength - job_key.size(),
             "&job_status=%s&last_event_index=%ld",
             CNetScheduleAPI::StatusToString(job_status).c_str(),
             (long int)last_event_index);


    m_StatusNotificationSocket.Send(
                                buffer,
                                strlen(buffer) + 1,
                                CSocketAPI::ntoa(address), port);
    return;
}


// Checks if a timeout is over and delete those records;
// Called from the notification thread when there are no jobs in pending state,
// so the notification flag should be reset.
void CNSNotificationList::CheckTimeout(time_t                 current_time,
                                       CNSClientsRegistry &   clients_registry,
                                       CNSAffinityRegistry &  aff_registry)
{
    list<SNSNotificationAttributes>::iterator   rec;
    CMutexGuard                                 guard(m_ListenersLock);

    // Passive records could only be deleted
    rec = m_PassiveListeners.begin();
    while (rec != m_PassiveListeners.end()) {
        if (x_TestTimeout(current_time, clients_registry, aff_registry,
                          m_PassiveListeners, rec))
            continue;
        ++rec;
    }

    // Active records could be deleted or moved to the passive list
    rec = m_ActiveListeners.begin();
    while (rec != m_ActiveListeners.end()) {
        if (x_TestTimeout(current_time, clients_registry, aff_registry,
                          m_ActiveListeners, rec))
            continue;

        // Need to move the record to the passive list
        m_PassiveListeners.push_back(SNSNotificationAttributes(*rec));
        rec = m_ActiveListeners.erase(rec);
    }

    return;
}


// Called from a notification thread which notifies worker nodes periodically
void
CNSNotificationList::NotifyPeriodically(time_t                 current_time,
                                        unsigned int           notif_lofreq_mult,
                                        CNSClientsRegistry &   clients_registry,
                                        CNSAffinityRegistry &  aff_registry)
{
    CMutexGuard                                 guard(m_ListenersLock);
    list<SNSNotificationAttributes>::iterator   k = m_ActiveListeners.begin();

    while (k != m_ActiveListeners.end()) {
        if (x_TestTimeout(current_time, clients_registry, aff_registry,
                          m_ActiveListeners, k))
            continue;

        // The record should not be deleted, so send the notification
        if (k->m_SlowRate || current_time > k->m_HifreqNotifyLifetime) {
            k->m_SlowRate = true;

            // We are at slow rate; might need to skip some
            k->m_SlowRateCount += 1;
            if (k->m_SlowRateCount > notif_lofreq_mult) {
                k->m_SlowRateCount = 0;
                // Send the same packet twice: Denis wanted to increase
                // the UDP delivery probability
                x_SendNotificationPacket(k->m_Address, k->m_Port, k->m_NewFormat);
                x_SendNotificationPacket(k->m_Address, k->m_Port, k->m_NewFormat);
            }
        } else {
            // We are at fast rate
            x_SendNotificationPacket(k->m_Address, k->m_Port, k->m_NewFormat);
        }
        ++k;
    }
    return;
}


void
CNSNotificationList::CheckOutdatedJobs(const TNSBitVector &  outdated_jobs,
                                       CNSClientsRegistry &  clients_registry,
                                       unsigned int          notif_highfreq_period)
{
    time_t                                      current_time = time(0);
    CMutexGuard                                 guard(m_ListenersLock);
    list<SNSNotificationAttributes>::iterator   k = m_PassiveListeners.begin();

    while (k != m_PassiveListeners.end()) {
        if (k->m_ExclusiveNewAff) {
            if ((outdated_jobs -
                 clients_registry.GetBlacklistedJobs(k->m_ClientNode)).any()) {
                x_SendNotificationPacket(k->m_Address, k->m_Port,
                                         k->m_NewFormat);

                // Move from passive list to the active one
                k->m_HifreqNotifyLifetime = current_time + notif_highfreq_period;
                m_ActiveListeners.push_back(SNSNotificationAttributes(*k));
                k = m_PassiveListeners.erase(k);
                continue;
            }
        }
        ++k;
    }
    return;
}


// Called when a job in pending state has appeared:
// submit, batch submit, return, timeout, fail etc.
void CNSNotificationList::Notify(unsigned int           job_id,
                                 unsigned int           aff_id,
                                 CNSClientsRegistry &   clients_registry,
                                 CNSAffinityRegistry &  aff_registry,
                                 unsigned int           notif_highfreq_period)
{
    TNSBitVector    aff_ids;
    TNSBitVector    jobs;

    if (aff_id != 0)
        aff_ids.set_bit(aff_id, true);

    jobs.set_bit(job_id, true);

    Notify(jobs, aff_ids, aff_id == 0,
           clients_registry, aff_registry, notif_highfreq_period);
    return;
}


// Called when a job in pending state has appeared:
// submit, batch submit, return, timeout, fail etc.
void
CNSNotificationList::Notify(const TNSBitVector &   jobs,
                            const TNSBitVector &   affinities,
                            bool                   no_aff_jobs,
                            CNSClientsRegistry &   clients_registry,
                            CNSAffinityRegistry &  aff_registry,
                            unsigned int           notif_highfreq_period)
{
    time_t          current_time = time(0);
    TNSBitVector    all_preferred_affs =
                                clients_registry.GetAllPreferredAffinities();

    CMutexGuard                                 guard(m_ListenersLock);
    list<SNSNotificationAttributes>::iterator   k = m_PassiveListeners.begin();

    while (k != m_PassiveListeners.end()) {
        if (x_TestTimeout(current_time, clients_registry, aff_registry,
                          m_PassiveListeners, k))
            continue;

        // The notification timeout is not over
        bool        should_send = false;

        // Check if all the jobs are in in its blacklist
        if ((jobs -
             clients_registry.GetBlacklistedJobs(
                                    k->m_ClientNode)).any() == false) {
            ++k;
            continue;
        }

        if (k->m_AnyJob)
            should_send = true;
        else
            // Here: the client is new because old clients always have
            //       m_AnyJob set to true
            if (affinities.any())
                should_send = clients_registry.IsRequestedAffinity(
                                                    k->m_ClientNode,
                                                    affinities,
                                                    k->m_WnodeAff);
        if (should_send == false) {
            if (k->m_ExclusiveNewAff) {
                if (no_aff_jobs)
                    should_send = true;
                else
                    if (affinities.any())
                        should_send = (affinities -
                                       all_preferred_affs).any();
            }
        }

        if (should_send) {
            k->m_HifreqNotifyLifetime = current_time + notif_highfreq_period;
            x_SendNotificationPacket(k->m_Address, k->m_Port, k->m_NewFormat);
            m_ActiveListeners.push_back(SNSNotificationAttributes(*k));
            k = m_PassiveListeners.erase(k);
            continue;
        }

        ++k;
    }

    return;
}


// If a lock is released during printing, then there is a chance that
// the same entry is printed twice - as active and as inactive.
// Bearing in mind that the list of worker nodes waiting on notifications
// is going to be requested rare and rather manually, and that the printing
// is done into a string under the lock, it seems reasonable to avoid
// releasing the lock.
void
CNSNotificationList::Print(CNetScheduleHandler &        handler,
                           const CNSClientsRegistry &   clients_registry,
                           const CNSAffinityRegistry &  aff_registry,
                           bool                         verbose) const
{
    string                                              buffer;
    list<SNSNotificationAttributes>::const_iterator     current;
    CMutexGuard                                         guard(m_ListenersLock);

    // Approximation
    buffer.reserve((m_ActiveListeners.size() +
                    m_PassiveListeners.size()) * 384);


    for (current = m_ActiveListeners.begin();
         current != m_ActiveListeners.end(); ++current)
        buffer += current->Print(clients_registry, aff_registry,
                                 true, verbose);

    for (current = m_PassiveListeners.begin();
         current != m_PassiveListeners.end(); ++current)
        buffer += current->Print(clients_registry, aff_registry,
                                 false, verbose);

    if (!buffer.empty())
        handler.WriteMessage(buffer.c_str());

    return;
}


void
CNSNotificationList::x_SendNotificationPacket(unsigned int    address,
                                              unsigned short  port,
                                              bool            new_format)
{
    if (new_format)
        m_GetNotificationSocket.Send(m_GetMsgBuffer, m_GetMsgLength,
                                     CSocketAPI::ntoa(address), port);
    else
        m_GetNotificationSocket.Send(m_GetMsgBufferObsoleteVersion,
                                     m_GetMsgLengthObsoleteVersion,
                                     CSocketAPI::ntoa(address), port);
    return;
}


// Tests if a single record if its lifetime is over.
// If so, it deletes the record and moves the iterator to the next record.
bool
CNSNotificationList::x_TestTimeout(
                time_t                                       current_time,
                CNSClientsRegistry &                         clients_registry,
                CNSAffinityRegistry &                        aff_registry,
                list<SNSNotificationAttributes> &            container,
                list<SNSNotificationAttributes>::iterator &  record)
{
    if (current_time > record->m_Lifetime) {
        // The record is out of date - remove it

        if (!record->m_ClientNode.empty())
            // That was a new client, so we need to clean up the clients
            // and affinity registry
            clients_registry.ResetWaiting(record->m_ClientNode,
                                          aff_registry);
        record = container.erase(record);
        return true;
    }

    return false;
}


// Get job notification thread implementation
CGetJobNotificationThread::CGetJobNotificationThread(
                                        CQueueDataBase &  qdb,
                                        unsigned int      sec_delay,
                                        unsigned int      nanosec_delay,
                                        const bool &      logging) :
    m_QueueDB(qdb), m_NotifLogging(logging),
    m_SecDelay(sec_delay), m_NanosecDelay(nanosec_delay),
    m_StopSignal(0, 10000000)
{}


CGetJobNotificationThread::~CGetJobNotificationThread()
{}


void CGetJobNotificationThread::RequestStop(void)
{
    m_StopFlag.Add(1);
    m_StopSignal.Post();
    return;
}


void *  CGetJobNotificationThread::Main(void)
{
    prctl(PR_SET_NAME, "netscheduled_nt", 0, 0, 0);
    while (1) {
        x_DoJob();

        if (m_StopSignal.TryWait(m_SecDelay, m_NanosecDelay))
            if (m_StopFlag.Get() != 0)
                break;
    } // while (1)

    return 0;
}


void CGetJobNotificationThread::x_DoJob(void)
{
    CRef<CRequestContext>       ctx;
    bool                        is_logging = m_NotifLogging;

    if (is_logging) {
        ctx.Reset(new CRequestContext());
        ctx->SetRequestID();
        GetDiagContext().SetRequestContext(ctx);
        GetDiagContext().PrintRequestStart()
                        .Print("_type", "get_job_notification_thread");
        ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
    }


    try {
        m_QueueDB.NotifyListeners();
    }
    catch (exception &  ex) {
        RequestStop();
        ERR_POST("Error during notification: " << ex.what() <<
                 " notification thread has been stopped.");
        if (is_logging)
            ctx->SetRequestStatus(
                        CNetScheduleHandler::eStatus_ServerError);
    }
    catch (...) {
        RequestStop();
        ERR_POST("Unknown error during notification. "
                 "Notification thread has been stopped.");
        if (is_logging)
            ctx->SetRequestStatus(
                        CNetScheduleHandler::eStatus_ServerError);
    }

    if (is_logging) {
        GetDiagContext().PrintRequestStop();
        ctx.Reset();
        GetDiagContext().SetRequestContext(NULL);
    }

}


list<SNSNotificationAttributes>::iterator
CNSNotificationList::x_FindListener(list<SNSNotificationAttributes> &  container,
                                    unsigned int                       address,
                                    unsigned short                     port)
{
    for (list<SNSNotificationAttributes>::iterator k = container.begin();
         k != container.end(); ++k) {
        if (k->m_Address == address && k->m_Port == port)
            return k;
    }
    return container.end();
}

END_NCBI_SCOPE

