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
#include "ns_precise_time.hpp"


BEGIN_NCBI_SCOPE

class CNSClientId;
class CNSClientsRegistry;
class CNSAffinityRegistry;
class CNSGroupsRegistry;
class CQueueDataBase;



struct SNSNotificationAttributes
{
    ECommandGroup   m_Reason;

    unsigned int    m_Address;
    unsigned short  m_Port;
    CNSPreciseTime  m_Lifetime;

    string          m_ClientNode;   // Non-empty for the new style clients
    bool            m_WnodeAff;     // true if I need to consider the node
                                    // preferred affinities.
    bool            m_AnyJob;       // true if any job is suitable.
    bool            m_ExclusiveNewAff;

    bool            m_NewFormat;    // If true, then a new format of
                                    // notifications will be used. New format
                                    // is for clients used GET2 command.
                                    // Old format is for clients used WGET.
    string          m_Group;

    // Support for two stage (different frequency) notifications
    // First stage is frequent (fast) within configured timeout from the moment
    // when a job is available.
    // second stage is infrequent (slow) till the end of the notifications.

    CNSPreciseTime  m_HifreqNotifyLifetime;
    bool            m_SlowRate;     // true if the client did not come after
                                    // fast notifications period.
    unsigned int    m_SlowRateCount;

    string  Print(const CNSClientsRegistry &   clients_registry,
                  const CNSAffinityRegistry &  aff_registry,
                  bool                         is_active,
                  bool                         verbose) const;
};


// The structure holds information about one time notification
// which is going to be sent at exactly specified time
struct SExactTimeNotification
{
    ECommandGroup           m_Reason;
    unsigned int            m_Address;
    unsigned short          m_Port;
    CNSPreciseTime          m_TimeToSend;
    bool                    m_NewFormat;
};



// The structure holds information about notifications which are
// triggered by worker nodes requests while the queue was paused.
struct SQueueResumeNotification
{
    unsigned int    m_Address;
    unsigned short  m_Port;
    bool            m_NewFormat;
};


const size_t        k_MessageBufferSize = 512;

class CNSNotificationList
{
    public:
        CNSNotificationList(CQueueDataBase &  qdb,
                            const string &    ns_node,
                            const string &    qname);

        void RegisterListener(const CNSClientId &   client,
                              unsigned short        port,
                              unsigned int          timeout,
                              bool                  wnode_aff,
                              bool                  any_job,
                              bool                  exclusive_new_affinity,
                              bool                  new_format,
                              const string &        group,
                              ECommandGroup         cmd_group);
        void UnregisterListener(const CNSClientId &  client,
                                unsigned short       port,
                                ECommandGroup        cmd_group);
        void UnregisterListener(unsigned int         address,
                                unsigned short       port,
                                ECommandGroup        cmd_group);

        void NotifyJobStatus(unsigned int    address,
                             unsigned short  port,
                             const string &  job_key,
                             TJobStatus      job_status,
                             size_t          last_event_index);
        void CheckTimeout(const CNSPreciseTime & current_time,
                          CNSClientsRegistry &   clients_registry,
                          ECommandGroup          cmd_group);
        void NotifyPeriodically(const CNSPreciseTime & current_time,
                                unsigned int           notif_lofreq_mult,
                                CNSClientsRegistry &   clients_registry);
        void CheckOutdatedJobs(const TNSBitVector &    outdated_jobs,
                               CNSClientsRegistry &    clients_registry,
                               const CNSPreciseTime &  notif_highfreq_period,
                               ECommandGroup           cmd_group);
        void Notify(unsigned int           job_id,
                    unsigned int           aff_id,
                    CNSClientsRegistry &   clients_registry,
                    CNSAffinityRegistry &  aff_registry,
                    CNSGroupsRegistry &    group_registry,
                    const CNSPreciseTime & notif_highfreq_period,
                    const CNSPreciseTime & notif_handicap,
                    ECommandGroup          cmd_group);
        void Notify(const TNSBitVector &   jobs,
                    const TNSBitVector &   affinities,
                    bool                   no_aff_jobs,
                    CNSClientsRegistry &   clients_registry,
                    CNSAffinityRegistry &  aff_registry,
                    CNSGroupsRegistry &    group_registry,
                    const CNSPreciseTime & notif_highfreq_period,
                    const CNSPreciseTime & notif_handicap,
                    ECommandGroup          cmd_group);
        void onQueueResumed(bool  any_pending);
        string Print(const CNSClientsRegistry &   clients_registry,
                     const CNSAffinityRegistry &  aff_registry,
                     bool                         verbose) const;
        size_t size(void) const
        {
            CMutexGuard guard(m_ListenersLock);
            return m_PassiveListeners.size() + m_ActiveListeners.size();
        }

        void ClearExactGetNotifications(void);
        CNSPreciseTime NotifyExactListeners(void);

        void AddToQueueResumedNotifications(unsigned int  address,
                                            unsigned short  port,
                                            bool  new_format);
        CNSPreciseTime
        GetPassiveNotificationLifetime(unsigned int  address,
                                       unsigned short  port,
                                       ECommandGroup  cmd_group) const;

    private:
        void x_AddToExactNotifications(unsigned int  address,
                                       unsigned short  port,
                                       const CNSPreciseTime &  when,
                                       bool  new_format,
                                       ECommandGroup  reason);
        void x_SendNotificationPacket(unsigned int            address,
                                      unsigned short          port,
                                      bool                    new_format,
                                      ECommandGroup           reason);
        bool x_TestTimeout(
                const CNSPreciseTime &       current_time,
                CNSClientsRegistry &         clients_registry,
                list<SNSNotificationAttributes> &            container,
                list<SNSNotificationAttributes>::iterator &  record);
        bool x_IsInExactList(unsigned int  address, unsigned short  port) const;

    private:
        list<SNSNotificationAttributes>     m_PassiveListeners;
        list<SNSNotificationAttributes>     m_ActiveListeners;
        mutable CMutex                      m_ListenersLock;

        list<SExactTimeNotification>        m_ExactTimeNotifications;
        mutable CMutex                      m_ExactTimeNotifLock;

        list<SQueueResumeNotification>      m_QueueResumeNotifications;
        mutable CMutex                      m_QueueResumeNotifLock;

        list<SNSNotificationAttributes>::iterator
                x_FindListener(list<SNSNotificationAttributes> &  container,
                               unsigned int                       address,
                               unsigned short                     port,
                               ECommandGroup                      cmd_group);

        CDatagramSocket     m_GetAndReadNotificationSocket;
        CFastMutex          m_GetAndReadNotificationSocketLock;
        char                m_GetMsgBuffer[k_MessageBufferSize];
        size_t              m_GetMsgLength;
        char                m_GetMsgBufferObsoleteVersion[k_MessageBufferSize];
        size_t              m_GetMsgLengthObsoleteVersion;
        char                m_ReadMsgBuffer[k_MessageBufferSize];
        size_t              m_ReadMsgLength;

        CDatagramSocket     m_StatusNotificationSocket;
        CFastMutex          m_StatusNotificationSocketLock;
        char                m_JobStateConstPart[k_MessageBufferSize];
        size_t              m_JobStateConstPartLength;

        CQueueDataBase &    m_QueueDB;

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
        void WakeUp(void);

    private:
        void x_DoJob(void);
        CNSPreciseTime x_ProcessExactTimeNotifications(void);

    protected:
        virtual void *  Main(void);

    private:
        CQueueDataBase &        m_QueueDB;
        const bool &            m_NotifLogging;
        CNSPreciseTime          m_Period;
        CNSPreciseTime          m_NextScheduled;

    private:
        mutable CSemaphore      m_StopSignal;
        mutable bool            m_StopFlag;

    private:
        CGetJobNotificationThread(const CGetJobNotificationThread &);
        CGetJobNotificationThread &
        operator=(const CGetJobNotificationThread &);
};


END_NCBI_SCOPE

#endif

