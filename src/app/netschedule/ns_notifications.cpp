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


BEGIN_NCBI_SCOPE


// Non-obvious choice of prefixes is made by original NS authors.
// I don't know why they were chosen and why they are not uniform.
const char *    k_JobStateMsgPrefix = "JNTF ";


CNSNotificationList::CNSNotificationList(const string &  qname)
{
    strcpy(m_JobStateMsgBuffer, k_JobStateMsgPrefix);

    snprintf(m_GetMsgBuffer, k_MessageBufferSize, "NCBI_JSQ_%s", qname.c_str());
    m_GetMsgLength = strlen(m_GetMsgBuffer) + 1;
}


void CNSNotificationList::RegisterListener(const CNSClientId &  client,
                                           unsigned short       port,
                                           unsigned int         timeout)
{
    unsigned int                address = client.GetAddress();
    CFastMutexGuard             guard(m_ListenersLock);

    for (list<SNSNotificationAttributes>::iterator k = m_Listeners.begin();
         k != m_Listeners.end(); ++k) {
        if (k->m_Address == address && k->m_Port == port) {
            k->m_Lifetime = time(0) + timeout;
            return;
        }
    }

    // New record should be inserted
    SNSNotificationAttributes   attributes;

    attributes.m_Address = address;
    attributes.m_Port = port;
    attributes.m_Lifetime = time(0) + timeout;

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


void CNSNotificationList::Notify(const string &  aff_token)
{
    time_t                                      current = time(0);

    CFastMutexGuard                             guard(m_ListenersLock);
    list<SNSNotificationAttributes>::iterator   k = m_Listeners.begin();
    while (k != m_Listeners.end()) {
        if (current > k->m_Lifetime)
            // The record is out of date - remove it
            k = m_Listeners.erase(k);
        else {
            // Send a notification
            if (aff_token.empty()) {
                m_GetMsgBuffer[m_GetMsgLength - 1] = '\0';
                m_GetNotificationSocket.Send(m_GetMsgBuffer, m_GetMsgLength,
                                             CSocketAPI::ntoa(k->m_Address),
                                             k->m_Port);
            } else {
                snprintf(m_GetMsgBuffer + m_GetMsgLength - 1,
                         k_MessageBufferSize - m_GetMsgLength + 1,
                         " aff=%s", NStr::URLEncode(aff_token).c_str());
                m_GetNotificationSocket.Send(m_GetMsgBuffer, strlen(m_GetMsgBuffer) + 1);
            }
            ++k;
        }
    }

    return;
}


// Used to notify clients in case of a batch submit when at least
// one job has affinity
void CNSNotificationList::NotifySomeAffinity(void)
{
    time_t                                      current = time(0);

    CFastMutexGuard                             guard(m_ListenersLock);
    list<SNSNotificationAttributes>::iterator   k = m_Listeners.begin();
    while (k != m_Listeners.end()) {
        if (current > k->m_Lifetime)
            // The record is out of date - remove it
            k = m_Listeners.erase(k);
        else {
            // Send a notification about some affinity
            strcpy(m_GetMsgBuffer + m_GetMsgLength - 1, " some_aff");
            // '+ 9' is for the length of " some_aff"
            m_GetNotificationSocket.Send(m_GetMsgBuffer, m_GetMsgLength + 9,
                                         CSocketAPI::ntoa(k->m_Address),
                                         k->m_Port);
            ++k;
        }
    }

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


// Checks if a timeout is over and delete those records
void CNSNotificationList::CheckTimeout(void)
{
    time_t                                      current = time(0);

    CFastMutexGuard                             guard(m_ListenersLock);
    list<SNSNotificationAttributes>::iterator   k = m_Listeners.begin();
    while (k != m_Listeners.end()) {
        if (current > k->m_Lifetime)
            // The record is out of date - remove it
            k = m_Listeners.erase(k);
        else
            ++k;
    }

    return;
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

