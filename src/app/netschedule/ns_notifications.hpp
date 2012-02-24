#ifndef NS_GET_NOTIFICATIONS__HPP
#define NS_GET_NOTIFICATIONS__HPP

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


#include <corelib/ncbimtx.hpp>
#include <corelib/ncbithr.hpp>

#include <list>

#include "ns_types.hpp"


BEGIN_NCBI_SCOPE

class CNSClientId;
class CNSClientsRegistry;
class CNSAffinityRegistry;
class CNetScheduleHandler;


struct SNSNotificationAttributes
{
    unsigned int    m_Address;
    unsigned short  m_Port;
    time_t          m_Lifetime;

    string          m_ClientNode;   // Non-empty for the new style lients
    bool            m_WnodeAff;     // true if I need to consider the node
                                    // preferred affinities.
    bool            m_AnyJob;       // true if any job is suitable.

    // Support for two stage (different frequency) notifications
    // First stage is frequent (fast) within configured timeout from the moment
    // when a job is available.
    // second stage is infrequent (slow) till the end of the notifications.

    bool            m_ShouldNotify; // If true then the notification thread
                                    // will notify the client.
    time_t          m_HifreqNotifyLifetime;
    bool            m_SlowRate;     // true if the client did not come after
                                    // fast notifications period.
    unsigned int    m_SlowRateCount;

    string  Print(const CNSClientsRegistry &   clients_registry,
                  const CNSAffinityRegistry &  aff_registry,
                  bool                         verbose) const;
};


const size_t        k_MessageBufferSize = 512;

class CNSNotificationList
{
    public:
        CNSNotificationList(const string &  ns_node,
                            const string &  qname);

        void RegisterListener(const CNSClientId &   client,
                              unsigned short        port,
                              unsigned int          timeout,
                              bool                  wnode_aff,
                              bool                  any_job);
        void UnregisterListener(const CNSClientId &  client,
                                unsigned short       port);

        void NotifyJobStatus(unsigned int    address,
                             unsigned short  port,
                             const string &  job_key);
        void CheckTimeout(time_t                 current_time,
                          CNSClientsRegistry &   clients_registry,
                          CNSAffinityRegistry &  aff_registry);
        void NotifyPeriodically(time_t                 current_time,
                                unsigned int           notif_lofreq_mult,
                                CNSClientsRegistry &   clients_registry,
                                CNSAffinityRegistry &  aff_registry);
        void Notify(unsigned int           aff_id,
                    CNSClientsRegistry &   clients_registry,
                    CNSAffinityRegistry &  aff_registry,
                    unsigned int           notif_highfreq_period);
        void Notify(const TNSBitVector &   affinities,
                    CNSClientsRegistry &   clients_registry,
                    CNSAffinityRegistry &  aff_registry,
                    unsigned int           notif_highfreq_period);
        void Print(CNetScheduleHandler &        handler,
                   const CNSClientsRegistry &   clients_registry,
                   const CNSAffinityRegistry &  aff_registry,
                   bool                         verbose) const;

    private:
        void x_SendNotificationPacket(unsigned int    address,
                                      unsigned short  port);
        bool x_TestTimeout(time_t                       current_time,
                           CNSClientsRegistry &         clients_registry,
                           CNSAffinityRegistry &        aff_registry,
                           list<SNSNotificationAttributes>::iterator &  record);

    private:
        list<SNSNotificationAttributes>     m_Listeners;
        mutable CFastMutex                  m_ListenersLock;

        list<SNSNotificationAttributes>::const_iterator
                                    x_SkipRecords(size_t count) const;

        CDatagramSocket         m_GetNotificationSocket;
        char                    m_GetMsgBuffer[k_MessageBufferSize];
        size_t                  m_GetMsgLength;

        CDatagramSocket         m_StatusNotificationSocket;
        char                    m_JobStateConstPart[k_MessageBufferSize];
        size_t                  m_JobStateConstPartLength;

    private:
        CNSNotificationList(const CNSNotificationList &);
        CNSNotificationList &  operator=(const CNSNotificationList &);
};



// Forward declaration for CGetJobNotificationThread
class CQueueDataBase;

// The thread for notifying listeners which wait for a job
// after issuing the WGET command
class CGetJobNotificationThread : public CThread
{
    public:
        CGetJobNotificationThread(CQueueDataBase &  qdb,
                                  unsigned int      sec_delay,
                                  unsigned int      nanosec_delay,
                                  const bool &      logging);
        ~CGetJobNotificationThread();

        void RequestStop(void);

    private:
        void x_DoJob(void);

    protected:
        virtual void *  Main(void);

    private:
        CQueueDataBase &                        m_QueueDB;
        const bool &                            m_NotifLogging;
        unsigned int                            m_SecDelay;
        unsigned int                            m_NanosecDelay;

    private:
        mutable CSemaphore                      m_StopSignal;
        mutable CAtomicCounter_WithAutoInit     m_StopFlag;

    private:
        CGetJobNotificationThread(const CGetJobNotificationThread &);
        CGetJobNotificationThread &  operator=(const CGetJobNotificationThread &);
};


END_NCBI_SCOPE

#endif

