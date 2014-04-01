#ifndef NETSCHEDULE_CLIENTS__HPP
#define NETSCHEDULE_CLIENTS__HPP

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
 *   NetSchedule clients registry supporting facilities
 *
 */

#include <connect/services/netservice_protocol_parser.hpp>

#include "ns_types.hpp"
#include "ns_access.hpp"
#include "ns_precise_time.hpp"

#include <string>


BEGIN_NCBI_SCOPE

// Forward declaration
class CQueue;
class CNSAffinityRegistry;
class CJobStatusTracker;
class CNetScheduleServer;


// The CClientId serves two types of clients:
// - old style clients; they have peer address only
// - new style clients; they have all three pieces,
//                      address, node id and session id
class CNSClientId
{
    public:
        CNSClientId();
        void Update(unsigned int            peer_addr,
                    const TNSProtoParams &  params);

        // true if it is a new client identification
        bool IsComplete(void) const;

        // Getters/setters
        unsigned int GetAddress(void) const
        { return m_Addr; }
        const string &  GetNode(void) const
        { return m_ClientNode; }
        const string &  GetSession(void) const
        { return m_ClientSession; }
        unsigned short  GetControlPort(void) const
        { return m_ControlPort; }
        const string &  GetClientHost(void) const
        { return m_ClientHost; }
        const string &  GetProgramName(void) const
        { return m_ProgName; }
        const string &  GetClientName(void) const
        { return m_ClientName; }
        void SetClientName(const string &  client_name);
        void SetClientHost(const string &  client_host)
        { m_ClientHost = client_host; }
        void SetControlPort(unsigned short  port)
        { m_ControlPort = port; }
        unsigned int GetID(void) const
        { return m_ID; }
        void SetID(unsigned int  id)
        { m_ID = id; }

        bool IsAdmin(void) const
        { return (m_PassedChecks & eNS_Admin) != 0; }
        void SetPassedChecks(TNSCommandChecks  check)
        { m_PassedChecks |= check; }
        TNSCommandChecks GetPassedChecks(void) const
        { return m_PassedChecks; }

        // The admin check is done per connection so there is no need
        // to reset it when a queue is changed or when a queue is from a job
        // key.
        void ResetPassedCheck(void)
        { if (IsAdmin()) { m_PassedChecks = 0; SetPassedChecks(eNS_Admin); }
          else           { m_PassedChecks = 0; } }

        void CheckAccess(TNSCommandChecks  cmd_reqs,
                         CNetScheduleServer * server);

    private:
        unsigned int        m_Addr;           // Client peer address
        string              m_ProgName;       // Program name - formed by API
                                              // and usually is an exe name
        string              m_ClientName;     // Client name - taken from the
                                              // app config file
        string              m_ClientNode;     // Client node,
                                              // e.g. service10:9300
        string              m_ClientSession;  // Session of working
                                              //  with netschedule.
        unsigned short      m_ControlPort;    // Client control port
        string              m_ClientHost;     // Client host name if passed in
                                              // the handshake line.

        TNSCommandChecks    m_PassedChecks;   // What checks the client has passed
                                              // successfully

        // 0 for old style clients
        // non 0 for new style clients.
        // This identifier is set at the moment of touching the clients
        // registry. The id is needed to support affinities. The affinity
        // registry will store IDs of the clients which informed that a certain
        // affinity is preferred.
        unsigned int        m_ID;
};



// The CNSClient stores information about new style clients only;
// The information includes the given jobs,
// type of the client (worker node, submitter) etc.

// Note: access to the instances of this class is possible only via the client
// registry and the corresponding container access is always done under a lock.
// So it is safe to do any modifications in the members without any locks here.
class CNSClient
{
    public:
        // Used for a bit mask to identify what kind of
        // operations the client tried to perform
        enum ENSClientType {
            eSubmitter  = 1,
            eWorkerNode = 2,
            eReader     = 4
        };

    public:
        CNSClient();
        CNSClient(const CNSClientId &  client_id,
                  CNSPreciseTime *     blacklist_timeout);
        bool Clear(void);
        TNSBitVector GetRunningJobs(void) const
        { return m_RunningJobs; }
        TNSBitVector GetReadingJobs(void) const
        { return m_ReadingJobs; }
        TNSBitVector GetBlacklistedJobs(void) const;
        bool IsJobBlacklisted(unsigned int  job_id) const;
        void SetWaitPort(unsigned short  port)
        { m_WaitPort = port; }
        string GetSession(void) const
        { return m_Session; }
        CNSPreciseTime GetRegistrationTime(void) const
        { return m_RegistrationTime; }
        CNSPreciseTime GetSessionStartTime(void) const
        { return m_SessionStartTime; }
        CNSPreciseTime GetSessionResetTime(void) const
        { return m_SessionResetTime; }
        CNSPreciseTime GetLastAccess(void) const
        { return m_LastAccess; }
        void RegisterSocketWriteError(void)
        { ++m_NumberOfSockErrors; }

        unsigned short GetAndResetWaitPort(void);

        void RegisterRunningJob(unsigned int  job_id);
        void RegisterReadingJob(unsigned int  job_id);
        void RegisterSubmittedJobs(size_t  count);
        void RegisterBlacklistedJob(unsigned int  job_id);
        bool MoveReadingJobToBlacklist(unsigned int  job_id);
        bool MoveRunningJobToBlacklist(unsigned int  job_id);
        void UnregisterRunningJob(unsigned int  job_id);
        void UnregisterReadingJob(unsigned int  job_id);

        bool Touch(const CNSClientId &  client_id,
                   TNSBitVector &       running_jobs,
                   TNSBitVector &       reading_jobs,
                   bool &               session_was_reset,
                   string &             old_session);
        string Print(const string &               node_name,
                     const CQueue *               queue,
                     const CNSAffinityRegistry &  aff_registry,
                     bool                         verbose) const;
        unsigned int GetID(void) const
        { return m_ID; }
        void SetID(unsigned int  id)
        { m_ID = id; }
        unsigned int GetType(void) const
        { return m_Type; }
        void AppendType(unsigned int  type_to_append)
        { m_Type |= type_to_append; }
        TNSBitVector  GetPreferredAffinities(void) const
        { return m_Affinities; }
        void  AddPreferredAffinities(const TNSBitVector &  aff);
        bool  AddPreferredAffinity(unsigned int  aff);
        void  RemovePreferredAffinities(const TNSBitVector &  aff);
        void  RemovePreferredAffinity(unsigned int  aff);
        void  SetPreferredAffinities(const TNSBitVector &  aff);

        // WGET support
        void  RegisterWaitAffinities(const TNSBitVector &  aff);
        TNSBitVector  GetWaitAffinities(void) const
        { return m_WaitAffinities; }
        bool  IsRequestedAffinity(const TNSBitVector &  aff,
                                  bool                  use_preferred) const;
        void  ClearWaitAffinities(void)
        { m_WaitAffinities.clear(); }
        bool  ClearPreferredAffinities(void);

        bool  GetAffinityReset(void) const
        { return m_AffReset; }
        void  SetAffinityReset(bool  value)
        { m_AffReset = value; }
        bool  AnyAffinity(void) const
        { return m_Affinities.any(); }
        bool  AnyWaitAffinity(void) const
        { return m_WaitAffinities.any(); }
        unsigned int  GetPeerAddress(void) const
        { return m_Addr; }

        void  CheckBlacklistedJobsExisted(const CJobStatusTracker &  tracker);
        int  SetClientData(const string &  data, int  data_version);

    private:
        bool            m_Cleared;        // Set to true when CLRN is received
                                          // If true => m_Session == "n/a"
        unsigned int    m_Type;           // bit mask of ENSClientType
        unsigned int    m_Addr;           // Client peer address
        unsigned short  m_ControlPort;    // Worker node control port
        string          m_ClientHost;     // Client host as given in the
                                          // handshake line.
        CNSPreciseTime  m_RegistrationTime;
        CNSPreciseTime  m_SessionStartTime;
        CNSPreciseTime  m_SessionResetTime;
        CNSPreciseTime  m_LastAccess;     // The last time the client accessed
                                          // netschedule
        string          m_Session;        // Client session id

        TNSBitVector    m_RunningJobs;      // The jobs the client is currently
                                            // executing
        TNSBitVector    m_ReadingJobs;      // The jobs the client is currently
                                            // reading
        mutable
        TNSBitVector    m_BlacklistedJobs;  // The jobs that should not be given
                                            // to the node neither for
                                            // executing nor for reading
        unsigned short  m_WaitPort;         // Port, provided in WGET command or
                                            // 0 otherwise
        unsigned int    m_ID;               // Client identifier, see comments
                                            // for CNSClientId::m_ID
        TNSBitVector    m_Affinities;       // The client preferred affinities
        TNSBitVector    m_WaitAffinities;   // The list of affinities the client
                                            // waits for on WGET
        size_t          m_NumberOfSubmitted;// Number of submitted jobs
        size_t          m_NumberOfRead;     // Number of jobs given for reading
        size_t          m_NumberOfRun;      // Number of jobs given for executing
        bool            m_AffReset;         // true => affinities were reset due to
                                            // client inactivity timeout
        size_t          m_NumberOfSockErrors;

        CNSPreciseTime *    m_BlacklistTimeout;
        mutable
        map<unsigned int,
            CNSPreciseTime>
                        m_BlacklistLimits;  // job id -> last second the job is in
                                            // the blacklist

        string  x_TypeAsString(void) const;
        void    x_AddToBlacklist(unsigned int  job_id);
        void    x_UpdateBlacklist(void) const;
        void    x_UpdateBlacklist(unsigned int  job_id) const;
        string  x_GetBlacklistLimit(unsigned int  job_id) const;

        size_t  m_RunningJobsOpCount;
        size_t  m_ReadingJobsOpCount;
        mutable size_t  m_BlacklistedJobsOpCount;
        size_t  m_AffinitiesOpCount;
        size_t  m_WaitAffinitiesOpCount;

        string  m_ClientData;
        int     m_ClientDataVersion;

        void x_RunningJobsOp(void);
        void x_ReadingJobsOp(void);
        void x_BlacklistedOp(void) const;
        void x_AffinitiesOp(void);
        void x_WaitAffinitiesOp(void);
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_CLIENTS__HPP */

