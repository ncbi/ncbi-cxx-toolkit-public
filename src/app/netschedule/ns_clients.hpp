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


// The client can claim that it belongs to a certain type
enum EClaimedClientType {
    eClaimedSubmitter,
    eClaimedWorkerNode,
    eClaimedReader,
    eClaimedAdmin,
    eClaimedAutodetect,     // The detection of the type must be done basing
                            // on the issued commands
    eClaimedReset,          // Reset the collected type bits (based on commands)
                            // and switch to auto
    eClaimedNotProvided
};



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
        EClaimedClientType  GetType(void) const
        { return m_ClientType; }
        void SetClientType(EClaimedClientType  new_type)
        { m_ClientType = new_type; }
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
                         CNetScheduleServer * server,
                         const string &  cmd);

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
        EClaimedClientType  m_ClientType;     // Client type, e.g. admin
        unsigned short      m_ControlPort;    // Client control port
        string              m_ClientHost;     // Client host name if passed in
                                              // the handshake line.

        TNSCommandChecks    m_PassedChecks;   // What checks the client has
                                              // passed successfully

        // 0 for old style clients
        // non 0 for new style clients.
        // This identifier is set at the moment of touching the clients
        // registry. The id is needed to support affinities. The affinity
        // registry will store IDs of the clients which informed that a certain
        // affinity is preferred.
        unsigned int        m_ID;

    private:
        EClaimedClientType  x_ConvertToClaimedType(
                                        const string &  claimed_type) const;
        string  x_NormalizeNodeOrSession(const string &  val,
                                         const string &  key);
};



// The class stores common data for WNs and readers
struct SRemoteNodeData
{
    public:
        SRemoteNodeData();
        SRemoteNodeData(CNSPreciseTime *  timeout);

        TNSBitVector GetBlacklistedJobs(void) const
        { x_UpdateBlacklist();
          return m_BlacklistedJobs; }

        void ClearJobs(void)
        { m_Jobs.clear();
          x_JobsOp(); }

        void AddPreferredAffinities(const TNSBitVector &  aff)
        { m_PrefAffinities |= aff;
          x_PrefAffinitiesOp(); }

        void RemovePreferredAffinities(const TNSBitVector &  aff)
        { m_PrefAffinities -= aff;
          x_PrefAffinitiesOp(); }

        void RemovePreferredAffinity(unsigned int  aff)
        { m_PrefAffinities.set_bit(aff, false);
          x_PrefAffinitiesOp(); }

        void SetPreferredAffinities(const TNSBitVector &  aff)
        { m_PrefAffinities = aff;
          m_AffReset = false;
          x_PrefAffinitiesOp(); }

        void SetWaitAffinities(const TNSBitVector &  aff)
        { m_WaitAffinities = aff;
          x_WaitAffinitiesOp(); }

        void ClearWaitAffinities(void)
        { m_WaitAffinities.clear();
          x_WaitAffinitiesOp(); }

        void UpdateBlacklist(unsigned int  job_id) const;
        string GetBlacklistLimit(unsigned int  job_id) const;
        void AddToBlacklist(unsigned int            job_id,
                            const CNSPreciseTime &  last_access);
        bool ClearPreferredAffinities(void);
        void RegisterJob(unsigned int  job_id);
        void UnregisterGivenJob(unsigned int  job_id);
        bool MoveJobToBlacklist(unsigned int  job_id);
        bool AddPreferredAffinity(unsigned int  aff);
        bool IsRequestedAffinity(const TNSBitVector &  aff,
                                 bool                  use_preferred) const;
        void GCBlacklist(const CJobStatusTracker &   tracker,
                         const vector<TJobStatus> &  match_states);
        void CancelWaiting(void);

    public:
        size_t                      m_NumberOfGiven;

        // Running or reading jobs
        mutable TNSBitVector        m_Jobs;

        // Black list support
        mutable TNSBitVector        m_BlacklistedJobs;
        CNSPreciseTime *            m_BlacklistTimeout;
        mutable
        map<unsigned int,
            CNSPreciseTime>         m_BlacklistLimits;


        // GET2/WGET or READ wait port
        unsigned short              m_WaitPort;

        // Affinities support
        bool                        m_AffReset;
        mutable TNSBitVector        m_PrefAffinities;
        mutable TNSBitVector        m_WaitAffinities;

    private:
        mutable size_t              m_JobsOpCount;
        mutable size_t              m_BlacklistedJobsOpCount;
        mutable size_t              m_PrefAffinitiesOpCount;
        mutable size_t              m_WaitAffinitiesOpCount;

    private:
        void x_ClearPreferredAffinities(void)
        { m_PrefAffinities.clear();
          x_PrefAffinitiesOp();
        }

        void x_UpdateBlacklist(void) const;
        void x_JobsOp(void) const;
        void x_BlacklistedOp(void) const;
        void x_PrefAffinitiesOp(void) const;
        void x_WaitAffinitiesOp(void) const;
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
            eReader     = 4,
            eAdmin      = 8
        };

        enum ENSClientState {
            eActive           = 1,
            eWNStale          = 2,
            eReaderStale      = 4,
            eWNAndReaderStale = 8,
            eQuit             = 16
        };

    public:
        CNSClient();
        CNSClient(const CNSClientId &  client_id,
                  CNSPreciseTime *     blacklist_timeout,
                  CNSPreciseTime *     read_blacklist_timeout);

        TNSBitVector GetJobs(ECommandGroup  cmd_group) const
        { if (cmd_group == eGet) return m_WNData.m_Jobs;
          return m_ReaderData.m_Jobs; }

        void ClearJobs(ECommandGroup  cmd_group)
        { if (cmd_group == eGet) m_WNData.ClearJobs();
          else                   m_ReaderData.ClearJobs();
        }

        TNSBitVector GetBlacklistedJobs(ECommandGroup  cmd_group) const
        { if (cmd_group == eGet) return m_WNData.GetBlacklistedJobs();
          return m_ReaderData.GetBlacklistedJobs(); }

        void SetWaitPort(unsigned short  port, ECommandGroup  cmd_group)
        { if (cmd_group == eGet) m_WNData.m_WaitPort = port;
          m_ReaderData.m_WaitPort = port; }

        unsigned short GetWaitPort(ECommandGroup  cmd_group) const
        { if (cmd_group == eGet) return m_WNData.m_WaitPort;
          return m_ReaderData.m_WaitPort; }

        string GetSession(void) const
        { return m_Session; }

        void SetSession(const string &  new_session)
        { m_Session = new_session;
          if (new_session == "")
            m_SessionResetTime = CNSPreciseTime::Current(); }

        CNSPreciseTime GetLastAccess(void) const
        { return m_LastAccess; }

        void RegisterSocketWriteError(void)
        { ++m_NumberOfSockErrors; }

        unsigned short GetAndResetWaitPort(ECommandGroup  cmd_group)
        { SRemoteNodeData & data = m_ReaderData;
          if (cmd_group == eGet) data = m_WNData;
          unsigned short    old_port = data.m_WaitPort;
          data.m_WaitPort = 0;
          return old_port; }

        void MarkAsAdmin(void)
        { m_Type |= eAdmin; }

        unsigned int GetID(void) const
        { return m_ID; }

        void SetID(unsigned int  id)
        { m_ID = id; }

        unsigned int GetType(void) const
        { return m_Type; }

        void AppendType(unsigned int  type_to_append)
        { m_Type |= type_to_append; }

        bool MoveJobToBlacklist(unsigned int  job_id, ECommandGroup  cmd_group)
        { if (cmd_group == eGet) return m_WNData.MoveJobToBlacklist(job_id);
          return m_ReaderData.MoveJobToBlacklist(job_id); }

        void UnregisterJob(unsigned int  job_id, ECommandGroup  cmd_group)
        { if (cmd_group == eGet) m_WNData.UnregisterGivenJob(job_id);
          else                   m_ReaderData.UnregisterGivenJob(job_id); }

        ENSClientState GetState(void) const
        { return m_State; }

        void SetState(ENSClientState  new_state)
        { m_State = new_state; }

        TNSBitVector GetPreferredAffinities(ECommandGroup  cmd_group) const
        { if (cmd_group == eGet) return m_WNData.m_PrefAffinities;
          return m_ReaderData.m_PrefAffinities; }

        bool HasPreferredAffinities(ECommandGroup  cmd_group) const
        { if (cmd_group == eGet) return m_WNData.m_PrefAffinities.any();
          return m_ReaderData.m_PrefAffinities.any(); }

        TNSBitVector GetWaitAffinities(ECommandGroup  cmd_group) const
        { if (cmd_group == eGet) return m_WNData.m_WaitAffinities;
          return m_ReaderData.m_WaitAffinities; }

        bool GetAffinityReset(ECommandGroup  cmd_group) const
        { if (cmd_group == eGet) return m_WNData.m_AffReset;
          return m_ReaderData.m_AffReset; }

        void SetAffinityReset(bool  value, ECommandGroup  cmd_group)
        { if (cmd_group == eGet) m_WNData.m_AffReset = value;
          else                   m_ReaderData.m_AffReset = value; }

        bool HasWaitAffinities(ECommandGroup  cmd_group) const
        { if (cmd_group == eGet) return m_WNData.m_WaitAffinities.any();
          return m_ReaderData.m_WaitAffinities.any(); }

        unsigned int GetPeerAddress(void) const
        { return m_Addr; }

        void ClearWaitAffinities(ECommandGroup  cmd_group)
        { if (cmd_group == eGet) m_WNData.ClearWaitAffinities();
          else                   m_ReaderData.ClearWaitAffinities(); }

        bool ClearPreferredAffinities(ECommandGroup  cmd_group)
        { if (cmd_group == eGet) return m_WNData.ClearPreferredAffinities();
          return m_ReaderData.ClearPreferredAffinities(); }

        void AddPreferredAffinities(const TNSBitVector &  aff,
                                    ECommandGroup         cmd_group)
        { if (cmd_group == eGet) { m_Type |= eWorkerNode;
                                   m_WNData.AddPreferredAffinities(aff); }
          else                   { m_Type |= eReader;
                                   m_ReaderData.AddPreferredAffinities(aff); }
        }

        bool AddPreferredAffinity(unsigned int   aff,
                                  ECommandGroup  cmd_group)
        { if (cmd_group == eGet) { m_Type |= eWorkerNode;
                                   return m_WNData.AddPreferredAffinity(aff); }
          m_Type |= eReader;
          return m_ReaderData.AddPreferredAffinity(aff);
        }

        void RemovePreferredAffinities(const TNSBitVector &  aff,
                                       ECommandGroup         cmd_group)
        { if (cmd_group == eGet) { m_Type |= eWorkerNode;
                                   m_WNData.RemovePreferredAffinities(aff); }
          else                   { m_Type |= eReader;
                                   m_ReaderData.RemovePreferredAffinities(aff); }
        }

        void RemovePreferredAffinity(unsigned int   aff,
                                     ECommandGroup  cmd_group)
        { if (cmd_group == eGet) { m_Type |= eWorkerNode;
                                   m_WNData.RemovePreferredAffinity(aff); }
          else                   { m_Type |= eReader;
                                   m_ReaderData.RemovePreferredAffinity(aff); }
        }

        void SetPreferredAffinities(const TNSBitVector &  aff,
                                    ECommandGroup         cmd_group)
        { if (cmd_group == eGet) { m_Type |= eWorkerNode;
                                   m_WNData.SetPreferredAffinities(aff); }
          else                   { m_Type |= eReader;
                                   m_ReaderData.SetPreferredAffinities(aff); }
        }

        void SetWaitAffinities(const TNSBitVector &  aff,
                               ECommandGroup         cmd_group)
        { if (cmd_group == eGet) { m_Type |= eWorkerNode;
                                   m_WNData.SetWaitAffinities(aff); }
          else                   { m_Type |= eReader;
                                   m_ReaderData.SetWaitAffinities(aff); }
        }

        bool IsRequestedAffinity(const TNSBitVector &  aff,
                                 bool                  use_preferred,
                                 ECommandGroup         cmd_group) const
        { if (cmd_group == eGet)
            return m_WNData.IsRequestedAffinity(aff, use_preferred);
          return m_ReaderData.IsRequestedAffinity(aff, use_preferred);
        }

        void CancelWaiting(ECommandGroup  cmd_group)
        { if (cmd_group == eGet) m_WNData.CancelWaiting();
          else                   m_ReaderData.CancelWaiting();
        }

        void SetClaimedType(EClaimedClientType  new_type)
        { m_ClaimedType = new_type; }

        void RegisterJob(unsigned int   job_id,
                         ECommandGroup  cmd_group);
        void RegisterSubmittedJobs(size_t  count);
        void RegisterBlacklistedJob(unsigned int   job_id,
                                    ECommandGroup  cmd_group);
        int SetClientData(const string &  data, int  data_version);
        void GCBlacklistedJobs(const CJobStatusTracker &  tracker,
                               ECommandGroup              cmd_group);
        void Touch(const CNSClientId &  client_id);
        string Print(const string &               node_name,
                     const CQueue *               queue,
                     const CNSAffinityRegistry &  aff_registry,
                     const set< string > &        gc_clients,
                     const set< string > &        read_gc_clients,
                     bool                         verbose) const;

    private:
        ENSClientState      m_State;          // Client state
                                              // If true => m_Session == "n/a"
        unsigned int        m_Type;           // bit mask of ENSClientType

        /* Note: at the handshake time a client may claim that it is a certain
         * type of client. It has nothing to do with how NS decides if an
         * adminstritive permission required command could be executed. The
         * member below tells what the client claimed and also how it will be
         * shown in the STAT CLIENTS output. And nothing else.
         */
        EClaimedClientType  m_ClaimedType;
        unsigned int        m_Addr;           // Client peer address
        unsigned short      m_ControlPort;    // Worker node control port
        string              m_ClientHost;     // Client host as given in the
                                              // handshake line.
        CNSPreciseTime      m_RegistrationTime;
        CNSPreciseTime      m_SessionStartTime;
        CNSPreciseTime      m_SessionResetTime;
        CNSPreciseTime      m_LastAccess;     // The last time the client
                                              // accessed netschedule
        string              m_Session;
        unsigned int        m_ID;             // Client identifier, see comments
                                              // for CNSClientId::m_ID
        size_t              m_NumberOfSubmitted;
        size_t              m_NumberOfSockErrors;

        string              m_ClientData;
        int                 m_ClientDataVersion;

        SRemoteNodeData     m_WNData;
        SRemoteNodeData     m_ReaderData;

    private:
        string  x_TypeAsString(void) const;
        string  x_StateAsString(void) const;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_CLIENTS__HPP */

