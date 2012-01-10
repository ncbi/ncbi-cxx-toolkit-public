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


// Non-obvious choice of prefixes is made by original NS authors.
// I don't know why they were chosen and why they are not uniform.
const char *    k_JobStateMsgPrefix = "JNTF ";


CNSNotificationList::CNSNotificationList(const string &  qname)
{
    strcpy(m_JobStateMsgBuffer, k_JobStateMsgPrefix);

    m_GetMsgLength = snprintf(m_GetMsgBuffer, k_MessageBufferSize,
                              "NCBI_JSQ_%s", qname.c_str()) + 1;
}


void CNSNotificationList::RegisterListener(const CNSClientId &   client,
                                           unsigned short        port,
                                           unsigned int          timeout,
                                           bool                  wnode_aff,
                                           bool                  any_job)
{
    unsigned int                address = client.GetAddress();
    CFastMutexGuard             guard(m_ListenersLock);

    for (list<SNSNotificationAttributes>::iterator k = m_Listeners.begin();
         k != m_Listeners.end(); ++k) {
        if (k->m_Address == address && k->m_Port == port) {
            k->m_Lifetime = time(0) + timeout;
            k->m_ClientNode = client.GetNode();
            k->m_WnodeAff = wnode_aff;
            k->m_AnyJob = any_job;
            k->m_ShouldNotify = false;
            k->m_HifreqNotifyLifetime = 0;
            k->m_SlowRate = false;
            k->m_SlowRateCount = 0;

            // If it is an old client => there is no record in the clients
            // registry, so there is no information stored of what explicit
            // affinities were provided even so.  Therefore, the old clients
            // should be notified when any job is available.
            if (k->m_ClientNode.empty())
                k->m_AnyJob = true;
            return;
        }
    }

    // New record should be inserted
    SNSNotificationAttributes   attributes;

    attributes.m_Address = address;
    attributes.m_Port = port;
    attributes.m_Lifetime = time(0) + timeout;
    attributes.m_ClientNode = client.GetNode();
    attributes.m_WnodeAff = wnode_aff;
    attributes.m_AnyJob = any_job;
    attributes.m_ShouldNotify = false;
    attributes.m_HifreqNotifyLifetime = 0;
    attributes.m_SlowRate = false;
    attributes.m_SlowRateCount = 0;

    // See the remark above about old clients.
    if (attributes.m_ClientNode.empty())
        attributes.m_AnyJob = true;

    m_Listeners.push_back(attributes);
    return;
}


void CNSNotificationList::UnregisterListener(const CNSClientId &  client,
                                             unsigned short       port)
{
    unsigned int                address = client.GetAddress();
    CFastMutexGuard             guard(m_ListenersLock);

    for (list<SNSNotificationAttributes>::iterator k = m_Listeners.begin();
         k != m_Listeners.end(); ++k) {
        if (k->m_Address == address && k->m_Port == port) {
            m_Listeners.erase(k);
            return;
        }
    }

    // Nothing has been found
    return;
}


void CNSNotificationList::NotifyJobStatus(unsigned int    address,
                                          unsigned short  port,
                                          const string &  job_key)
{
    CFastMutexGuard     guard(m_JobStatusLock);

    snprintf(m_JobStateMsgBuffer + sizeof(k_JobStateMsgPrefix),
             k_MessageBufferSize - sizeof(k_JobStateMsgPrefix),
             "key=%s", job_key.c_str());
    m_StatusNotificationSocket.Send(m_JobStateMsgBuffer,
                                    // '+4' is for 'key='
                                    sizeof(k_JobStateMsgPrefix) + 4 + job_key.size() + 1,
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
    CFastMutexGuard                             guard(m_ListenersLock);
    list<SNSNotificationAttributes>::iterator   k = m_Listeners.begin();

    while (k != m_Listeners.end()) {
        if (x_TestTimeout(current_time, clients_registry, aff_registry, k))
            continue;

        k->m_ShouldNotify = false;
        ++k;
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
    CFastMutexGuard                             guard(m_ListenersLock);
    list<SNSNotificationAttributes>::iterator   k = m_Listeners.begin();

    while (k != m_Listeners.end()) {
        if (x_TestTimeout(current_time, clients_registry, aff_registry, k))
            continue;

        // The record should not be deleted, check if it the notification
        // has to be sent
        if (k->m_ShouldNotify) {
            if (k->m_SlowRate || current_time > k->m_HifreqNotifyLifetime) {
                k->m_SlowRate = true;

                // We are at slow rate; might need to skip some
                k->m_SlowRateCount += 1;
                if (k->m_SlowRateCount == 1 || k->m_SlowRateCount > notif_lofreq_mult) {
                    k->m_SlowRateCount = 0;
                    // Send the same packet twice: Denis wanted to increase
                    // the UDP delivery probability
                    x_SendNotificationPacket(k->m_Address, k->m_Port);
                    x_SendNotificationPacket(k->m_Address, k->m_Port);
                }
            } else {
                // We are at fast rate
                x_SendNotificationPacket(k->m_Address, k->m_Port);
            }
        }
        ++k;
    }
    return;
}


// Called when a job in pending state has appeared:
// submit, batch submit, return, timeout, fail etc.
void CNSNotificationList::Notify(unsigned int           aff_id,
                                 CNSClientsRegistry &   clients_registry,
                                 CNSAffinityRegistry &  aff_registry,
                                 unsigned int           notif_highfreq_period)
{
    TNSBitVector    aff_ids;

    if (aff_id != 0)
        aff_ids.set_bit(aff_id, true);

    Notify(aff_ids, clients_registry, aff_registry, notif_highfreq_period);
    return;
}


// Called when a job in pending state has appeared:
// submit, batch submit, return, timeout, fail etc.
void
CNSNotificationList::Notify(const TNSBitVector &   affinities,
                            CNSClientsRegistry &   clients_registry,
                            CNSAffinityRegistry &  aff_registry,
                            unsigned int           notif_highfreq_period)
{
    time_t                                      current_time = time(0);
    CFastMutexGuard                             guard(m_ListenersLock);
    list<SNSNotificationAttributes>::iterator   k = m_Listeners.begin();

    while (k != m_Listeners.end()) {
        if (x_TestTimeout(current_time, clients_registry, aff_registry, k))
            continue;

        // The notification timeout is not over
        bool        should_send = false;
        if (!k->m_ShouldNotify) {
            // The client has not been notified before
            if (k->m_AnyJob)
                should_send = true;
            else
                if (affinities.any())
                    should_send = clients_registry.IsRequestedAffinity(
                                                        k->m_ClientNode,
                                                        affinities,
                                                        k->m_WnodeAff);
        }

        if (should_send) {
            k->m_ShouldNotify = true;
            k->m_HifreqNotifyLifetime = current_time + notif_highfreq_period;
            x_SendNotificationPacket(k->m_Address, k->m_Port);
        }

        ++k;
    }

    return;
}


void
CNSNotificationList::Print(CNetScheduleHandler &        handler,
                           const CNSClientsRegistry &   clients_registry,
                           const CNSAffinityRegistry &  aff_registry,
                           bool                         verbose) const
{
    CFastMutexGuard                             guard(m_ListenersLock);

    for (list<SNSNotificationAttributes>::const_iterator
            k = m_Listeners.begin(); k != m_Listeners.end(); ++k) {
        handler.WriteMessage("OK:CLIENT: " +
                             CSocketAPI::gethostbyaddr(k->m_Address) + ":" +
                             NStr::IntToString(k->m_Port));
        handler.WriteMessage("OK:  NODE: '" + k->m_ClientNode + "'");

        CTime   lifetime(k->m_Lifetime);
        lifetime.ToLocalTime();
        handler.WriteMessage("OK:  LIFE TIME: " + lifetime.AsString());

        if (k->m_AnyJob)
            handler.WriteMessage("OK:  ANY JOB: TRUE");
        else
            handler.WriteMessage("OK:  ANY JOB: FALSE");

        if (verbose == false) {
            handler.WriteMessage("OK:  EXPLICIT AFFINITIES: n/a (available in VERBOSE mode)");
        }
        else {
            if (k->m_ClientNode.empty())
                handler.WriteMessage("OK:  EXPLICIT AFFINITIES: CLIENT NOT FOUND");
            else {
                TNSBitVector    wait_aff = clients_registry.GetWaitAffinities(k->m_ClientNode);

                if (wait_aff.any()) {
                    handler.WriteMessage("OK:  EXPLICIT AFFINITIES:");

                    TNSBitVector::enumerator    en(wait_aff.first());
                    for ( ; en.valid(); ++en)
                        handler.WriteMessage("OK:    " + aff_registry.GetTokenByID(*en));
                }
                else
                    handler.WriteMessage("OK:  EXPLICIT AFFINITIES: NONE");
            }
        }

        if (verbose == false) {
            if (k->m_WnodeAff)
                handler.WriteMessage("OK:  USE PREFERRED AFFINITIES: TRUE");
            else
                handler.WriteMessage("OK:  USE PREFERRED AFFINITIES: FALSE");
        }
        else {
            if (k->m_WnodeAff) {

                // Need to print resolved preferred affinities
                TNSBitVector    pref_aff = clients_registry.GetPreferredAffinities(k->m_ClientNode);

                if (pref_aff.any()) {
                    handler.WriteMessage("OK:  USE PREFERRED AFFINITIES:");

                    TNSBitVector::enumerator    en(pref_aff.first());
                    for ( ; en.valid(); ++en)
                        handler.WriteMessage("OK:    " + aff_registry.GetTokenByID(*en));
                }
                else
                    handler.WriteMessage("OK:  USE PREFERRED AFFINITIES: NONE");
            }
            else
                handler.WriteMessage("OK:  USE PREFERRED AFFINITIES: FALSE");
        }


        if (k->m_ShouldNotify)
            handler.WriteMessage("OK:  ACTIVE: TRUE");
        else
            handler.WriteMessage("OK:  ACTIVE: FALSE");

        if (k->m_HifreqNotifyLifetime != 0) {
            CTime   highfreq_lifetime(k->m_HifreqNotifyLifetime);
            highfreq_lifetime.ToLocalTime();
            handler.WriteMessage("OK:  HIGH FREQUENCY LIFE TIME: " +
                                 highfreq_lifetime.AsString());
        }
        else
            handler.WriteMessage("OK:  HIGH FREQUENCY LIFE TIME: n/a");

        if (k->m_SlowRate)
            handler.WriteMessage("OK:  SLOW RATE ACTIVE: TRUE");
        else
            handler.WriteMessage("OK:  SLOW RATE ACTIVE: FALSE");
    }

    return;
}


void
CNSNotificationList::x_SendNotificationPacket(unsigned int    address,
                                              unsigned short  port)
{
    m_GetNotificationSocket.Send(m_GetMsgBuffer, m_GetMsgLength,
                                 CSocketAPI::ntoa(address), port);
    return;
}


// Tests if a single record if its lifetime is over.
// If so, it deletes the record and moves the iterator to the next record.
bool
CNSNotificationList::x_TestTimeout(
                        time_t                       current_time,
                        CNSClientsRegistry &         clients_registry,
                        CNSAffinityRegistry &        aff_registry,
                        list<SNSNotificationAttributes>::iterator &  record)
{
    if (current_time > record->m_Lifetime) {
        // The record is out of date - remove it

        if (!record->m_ClientNode.empty())
            // That was a new client, so we need to clean up the clients
            // and affinity registry
            clients_registry.ResetWaiting(record->m_ClientNode,
                                          aff_registry);
        record = m_Listeners.erase(record);
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
                        CNetScheduleHandler::eStatus_JobNotifierError);
    }
    catch (...) {
        RequestStop();
        ERR_POST("Unknown error during notification. "
                 "Notification thread has been stopped.");
        if (is_logging)
            ctx->SetRequestStatus(
                        CNetScheduleHandler::eStatus_JobNotifierError);
    }

    if (is_logging) {
        GetDiagContext().PrintRequestStop();
        ctx.Reset();
        GetDiagContext().SetRequestContext(NULL);
    }

}


END_NCBI_SCOPE

