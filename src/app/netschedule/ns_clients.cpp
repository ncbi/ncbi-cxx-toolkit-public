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
 *   NetSchedule clients registry and supporting facilities
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/netschedule_api.hpp>
#include <connect/ncbi_socket.hpp>
#include <corelib/ncbi_limits.hpp>

#include "ns_clients.hpp"
#include "ns_queue.hpp"
#include "queue_vc.hpp"
#include "ns_affinity.hpp"
#include "job_status.hpp"
#include "ns_server.hpp"


BEGIN_NCBI_SCOPE


// The CClientId serves two types of clients:
// - old style clients; they have peer address only
// - new style clients; they have all three pieces,
//                      address, node id and session id
CNSClientId::CNSClientId() :
    m_Addr(0), m_ClientType(eClaimedNotProvided),
    m_PassedChecks(0), m_ID(0)
{}



// Used instead of a constructor - when the queue is given
// during the handshake phase.
void CNSClientId::Update(unsigned int            peer_addr,
                         const TNSProtoParams &  params)
{
    TNSProtoParams::const_iterator      found;

    m_Addr           = peer_addr;
    m_ProgName       = "";
    m_ClientName     = "";
    m_ClientNode     = "";
    m_ClientSession  = "";
    m_ClientType     = eClaimedNotProvided;
    m_ClientHost     = "";
    m_ControlPort    = 0;
    m_PassedChecks   = 0;
    m_ID             = 0;

    found = params.find("client_node");
    if (found != params.end())
        m_ClientNode = found->second;

    found = params.find("client_session");
    if (found != params.end())
        m_ClientSession = found->second;

    found = params.find("client_type");
    if (found != params.end())
        m_ClientType = x_ConvertToClaimedType(found->second);

    // It must be that either both client_node and client_session
    // parameters are provided or none of them
    if (m_ClientNode.empty() && !m_ClientSession.empty())
        NCBI_THROW(CNetScheduleException, eAuthenticationError,
                   "client_session is provided but client_node is not");
    if (!m_ClientNode.empty() && m_ClientSession.empty())
        NCBI_THROW(CNetScheduleException, eAuthenticationError,
                   "client_node is provided but client_session is not");

    found = params.find("prog");
    if (found != params.end())
        m_ProgName = found->second;

    found = params.find("client");
    if (found != params.end())
        m_ClientName = found->second;

    found = params.find("control_port");
    if (found != params.end()) {
        try {
            m_ControlPort =
                NStr::StringToNumeric<unsigned short>(found->second);
        } catch (...) {
            ERR_POST("Error converting client control port. "
                     "Expected control_port=<unsigned short>, "
                     "received control_port=" << found->second);
            m_ControlPort = 0;
        }
    }

    found = params.find("client_host");
    if (found != params.end())
        m_ClientHost = found->second;
}


// true if it is a new client identification
bool CNSClientId::IsComplete(void) const
{
    // It is gauranteed in the constructor that both
    // m_ClientNode and m_ClientSession are empty or not empty together.
    return !m_ClientNode.empty();
}


// This is for rude clients which provide the first (i.e. authorization)
// in the format that cannot be parsed. In this case the whole line
// is considered to be the client name.
void CNSClientId::SetClientName(const string &  client_name)
{
    m_ClientName = client_name;
}


void CNSClientId::CheckAccess(TNSCommandChecks  cmd_reqs,
                              CNetScheduleServer *  server,
                              const string &  cmd)
{
    if (cmd_reqs & eNS_Queue) {
        if ((m_PassedChecks & eNS_Queue) == 0)
            NCBI_THROW(CNetScheduleException, eInvalidParameter,
                       "Invalid parameter: queue required");
    }

    if (IsAdmin())
        return;     // Admin can do everything

    if (cmd_reqs & eNS_Admin) {
        if ((m_PassedChecks & eNS_Admin) == 0) {
            server->RegisterAlert(eAccess, "admin privileges required "
                                  "to execute " + cmd);
            NCBI_THROW(CNetScheduleException, eAccessDenied,
                       "Access denied: admin privileges required");
        }
    }

    if (cmd_reqs & eNS_Submitter) {
        if ((m_PassedChecks & eNS_Submitter) == 0) {
            server->RegisterAlert(eAccess, "submitter privileges required "
                                  "to execute " + cmd);
            NCBI_THROW(CNetScheduleException, eAccessDenied,
                       "Access denied: submitter privileges required");
        }
    }

    if (cmd_reqs & eNS_Worker) {
        if ((m_PassedChecks & eNS_Worker) == 0) {
            server->RegisterAlert(eAccess, "worker node privileges required "
                                  "to execute " + cmd);
            NCBI_THROW(CNetScheduleException, eAccessDenied,
                       "Access denied: worker node privileges required");
        }
    }

    if (cmd_reqs & eNS_Reader) {
        if ((m_PassedChecks & eNS_Reader) == 0) {
            server->RegisterAlert(eAccess, "reader privileges required "
                                  "to execute " + cmd);
            NCBI_THROW(CNetScheduleException, eAccessDenied,
                       "Access denied: reader privileges required");
        }
    }

    if (cmd_reqs & eNS_Program) {
        if ((m_PassedChecks & eNS_Program) == 0) {
            server->RegisterAlert(eAccess, "program privileges required "
                                  "to execute " + cmd);
            NCBI_THROW(CNetScheduleException, eAccessDenied,
                       "Access denied: program privileges required");
        }
    }
}


EClaimedClientType
CNSClientId::x_ConvertToClaimedType(const string &  claimed_type) const
{
    string      ci_claimed_type = claimed_type;
    NStr::ToLower(ci_claimed_type);
    if (ci_claimed_type == "submitter")
        return eClaimedSubmitter;
    if (ci_claimed_type == "worker node")
        return eClaimedWorkerNode;
    if (ci_claimed_type == "reader")
        return eClaimedReader;
    if (ci_claimed_type == "admin")
        return eClaimedAdmin;
    if (ci_claimed_type == "auto")
        return eClaimedAutodetect;
    if (ci_claimed_type == "reset")
        return eClaimedReset;

    ERR_POST(Warning <<
             "Unsupported client_type value at the handshake phase. Supported "
             "values are: submitter, worker node, reader, admin and auto. "
             "Received: " << m_ClientType);
    return eClaimedNotProvided;
}


SRemoteNodeData::SRemoteNodeData() :
    m_NumberOfGiven(0),
    m_Jobs(bm::BM_GAP),
    m_BlacklistedJobs(bm::BM_GAP),
    m_BlacklistTimeout(NULL),
    m_WaitPort(0),
    m_AffReset(false),
    m_PrefAffinities(bm::BM_GAP),
    m_WaitAffinities(bm::BM_GAP),
    m_JobsOpCount(0),
    m_BlacklistedJobsOpCount(0),
    m_PrefAffinitiesOpCount(0),
    m_WaitAffinitiesOpCount(0)
{}


SRemoteNodeData::SRemoteNodeData(CNSPreciseTime *  timeout) :
    m_NumberOfGiven(0),
    m_Jobs(bm::BM_GAP),
    m_BlacklistedJobs(bm::BM_GAP),
    m_BlacklistTimeout(timeout),
    m_WaitPort(0),
    m_AffReset(false),
    m_PrefAffinities(bm::BM_GAP),
    m_WaitAffinities(bm::BM_GAP),
    m_JobsOpCount(0),
    m_BlacklistedJobsOpCount(0),
    m_PrefAffinitiesOpCount(0),
    m_WaitAffinitiesOpCount(0)
{}


// Checks if jobs should be removed from the node blacklist
void SRemoteNodeData::x_UpdateBlacklist(void) const
{
    if (m_BlacklistLimits.empty())
        return;

    CNSPreciseTime          current_time = CNSPreciseTime::Current();
    vector<unsigned int>    to_be_removed;
    map<unsigned int,
        CNSPreciseTime>::const_iterator
                            end_iterator = m_BlacklistLimits.end();

    for (map<unsigned int,
             CNSPreciseTime>::const_iterator k = m_BlacklistLimits.begin();
         k != end_iterator; ++k) {
        if (k->second < current_time)
            to_be_removed.push_back(k->first);
    }

    for (vector<unsigned int>::const_iterator j = to_be_removed.begin();
         j != to_be_removed.end(); ++j) {
        m_BlacklistedJobs.set_bit(*j, false);
        x_BlacklistedOp();
        m_BlacklistLimits.erase(*j);
    }
}


// Checks if a single job is in a blacklist and
// whether it should be removed from there
void SRemoteNodeData::UpdateBlacklist(unsigned int  job_id) const
{
    if (m_BlacklistLimits.empty())
        return;

    if (!m_BlacklistedJobs.get_bit(job_id))
        return;

    CNSPreciseTime                  current_time = CNSPreciseTime::Current();
    map<unsigned int,
        CNSPreciseTime>::iterator   found = m_BlacklistLimits.find(job_id);
    if (found != m_BlacklistLimits.end()) {
        if (found->second < current_time) {
            m_BlacklistedJobs.set_bit(job_id, false);
            x_BlacklistedOp();
            m_BlacklistLimits.erase(found);
        }
    }
}


string SRemoteNodeData::GetBlacklistLimit(unsigned int  job_id) const
{
    map<unsigned int,
        CNSPreciseTime>::iterator   found = m_BlacklistLimits.find(job_id);
    if (found != m_BlacklistLimits.end())
        return NS_FormatPreciseTime(found->second);
    return "0.0";
}


void SRemoteNodeData::AddToBlacklist(unsigned int            job_id,
                                     const CNSPreciseTime &  last_access)
{
    if (m_BlacklistTimeout == NULL) {
        ERR_POST("Design error in NetSchedule. "
                 "Blacklist timeout pointer must not be NULL. "
                 "Ignore blacklisting request and continue.");
        return;
    }

    if (*m_BlacklistTimeout == kTimeZero)
        return;     // No need to blacklist the job (per configuration)

    // Here: the job must be blacklisted. So be attentive to overflow.
    CNSPreciseTime  last_time_in_list = last_access + *m_BlacklistTimeout;
    if (last_time_in_list < last_access)
        last_time_in_list = kTimeNever;  // overflow

    m_BlacklistLimits[job_id] = last_time_in_list;
    m_BlacklistedJobs.set_bit(job_id, true);
    x_BlacklistedOp();
}


bool SRemoteNodeData::ClearPreferredAffinities(void)
{
    if (m_PrefAffinities.any()) {
        m_PrefAffinities.clear();
        x_PrefAffinitiesOp();
        return true;
    }
    return false;
}


void SRemoteNodeData::RegisterJob(unsigned int  job_id)
{
    m_Jobs.set_bit(job_id);
    x_JobsOp();
    ++m_NumberOfGiven;
    return;
}


void SRemoteNodeData::UnregisterGivenJob(unsigned int  job_id)
{
    m_Jobs.set_bit(job_id, false);
    x_JobsOp();
}


// returns true if the modifications have been really made
bool SRemoteNodeData::MoveJobToBlacklist(unsigned int  job_id)
{
    if (m_Jobs.get_bit(job_id)) {
        m_Jobs.set_bit(job_id, false);
        AddToBlacklist(job_id, CNSPreciseTime::Current());
        x_JobsOp();
        return true;
    }
    return false;
}


bool SRemoteNodeData::AddPreferredAffinity(unsigned int  aff)
{
    if (aff != 0) {
        if (!m_PrefAffinities.get_bit(aff)) {
            m_PrefAffinities.set_bit(aff);
            x_PrefAffinitiesOp();
            return true;
        }
    }
    return false;
}


// Used in the notifications code to test if a notifications should be sent to
// the client
bool
SRemoteNodeData::IsRequestedAffinity(const TNSBitVector &  aff,
                                     bool                  use_preferred) const
{
    if ((m_WaitAffinities & aff).any())
        return true;

    if (use_preferred)
        if ((m_PrefAffinities & aff).any())
            return true;

    return false;
}


void
SRemoteNodeData::GCBlacklist(const CJobStatusTracker &   tracker,
                             const vector<TJobStatus> &  match_states)
{
    // Checks if jobs should be removed from a reader blacklist
    if (m_BlacklistLimits.empty())
        return;

    unsigned int            min_existed_id = tracker.GetMinJobID();
    vector<unsigned int>    to_be_removed;

    for (map<unsigned int,
             CNSPreciseTime>::const_iterator k = m_BlacklistLimits.begin();
         k != m_BlacklistLimits.end(); ++k) {
        if (k->first < min_existed_id)
            to_be_removed.push_back(k->first);
        else {
            TJobStatus      status = tracker.GetStatus(k->first);
            for (vector<TJobStatus>::const_iterator j = match_states.begin();
                 j != match_states.end(); ++j) {
                if (status == *j) {
                    to_be_removed.push_back(k->first);
                    break;
                }
            }
        }
    }

    for (vector<unsigned int>::const_iterator m = to_be_removed.begin();
         m != to_be_removed.end(); ++m) {
        m_BlacklistedJobs.set_bit(*m, false);
        x_BlacklistedOp();
        m_BlacklistLimits.erase(*m);
    }
}


// Handles the following cases:
// - GET2/READ with waiting has been interrupted by another GET2/READ
// - CWGET/CWREAD received
// - GET2/READ wait timeout is over
void
SRemoteNodeData::CancelWaiting(void)
{
    if (m_WaitAffinities.any())
        ClearWaitAffinities();

    m_WaitPort = 0;
}


void SRemoteNodeData::x_JobsOp(void) const
{
    if (++m_JobsOpCount >= k_OpLimitToOptimize) {
        m_JobsOpCount = 0;
        m_Jobs.optimize(0, TNSBitVector::opt_free_0);
    }
}

void SRemoteNodeData::x_BlacklistedOp(void) const
{
    if (++m_BlacklistedJobsOpCount >= k_OpLimitToOptimize) {
        m_BlacklistedJobsOpCount = 0;
        m_BlacklistedJobs.optimize(0, TNSBitVector::opt_free_0);
    }
}

void SRemoteNodeData::x_PrefAffinitiesOp(void) const
{
    if (++m_PrefAffinitiesOpCount >= k_OpLimitToOptimize) {
        m_PrefAffinitiesOpCount = 0;
        m_PrefAffinities.optimize(0, TNSBitVector::opt_free_0);
    }
}

void SRemoteNodeData::x_WaitAffinitiesOp(void) const
{
    if (++m_WaitAffinitiesOpCount >= k_OpLimitToOptimize) {
        m_WaitAffinitiesOpCount = 0;
        m_WaitAffinities.optimize(0, TNSBitVector::opt_free_0);
    }
}


// The CNSClient implementation

// This constructor is for the map<> container
CNSClient::CNSClient() :
    m_State(eActive),
    m_Type(0),
    m_ClaimedType(eClaimedNotProvided),
    m_Addr(0),
    m_ControlPort(0),
    m_RegistrationTime(0, 0),
    m_SessionStartTime(0, 0),
    m_SessionResetTime(0, 0),
    m_LastAccess(0, 0),
    m_Session(),
    m_ID(0),
    m_NumberOfSubmitted(0),
    m_NumberOfSockErrors(0),
    m_ClientDataVersion(0)
{}


CNSClient::CNSClient(const CNSClientId &  client_id,
                     CNSPreciseTime *     blacklist_timeout,
                     CNSPreciseTime *     read_blacklist_timeout) :
    m_State(eActive),
    m_Type(0),
    m_ClaimedType(client_id.GetType()),
    m_Addr(client_id.GetAddress()),
    m_ControlPort(client_id.GetControlPort()),
    m_ClientHost(client_id.GetClientHost()),
    m_RegistrationTime(0, 0),
    m_SessionStartTime(0, 0),
    m_SessionResetTime(0, 0),
    m_LastAccess(0, 0),
    m_Session(client_id.GetSession()),
    m_ID(0),
    m_NumberOfSubmitted(0),
    m_NumberOfSockErrors(0),
    m_ClientDataVersion(0),
    m_WNData(blacklist_timeout),
    m_ReaderData(read_blacklist_timeout)
{
    if (!client_id.IsComplete())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Creating client information object for old style clients");
    m_RegistrationTime = CNSPreciseTime::Current();
    m_SessionStartTime = m_RegistrationTime;
    m_LastAccess = m_RegistrationTime;

    // It does not make any sense at the time of creation to have 'reset'
    // client type
    if (m_ClaimedType == eClaimedReset)
        m_ClaimedType = eClaimedAutodetect;
}


void CNSClient::RegisterJob(unsigned int  job_id, ECommandGroup  cmd_group)
{
    m_LastAccess = CNSPreciseTime::Current();
    if (cmd_group == eGet) {
        m_Type |= eWorkerNode;
        m_WNData.RegisterJob(job_id);
    } else {
        m_Type |= eReader;
        m_ReaderData.RegisterJob(job_id);
    }
}


void CNSClient::RegisterSubmittedJobs(size_t  count)
{
    m_LastAccess = CNSPreciseTime::Current();
    m_Type |= eSubmitter;
    m_NumberOfSubmitted += count;
}


void CNSClient::RegisterBlacklistedJob(unsigned int   job_id,
                                       ECommandGroup  cmd_group)
{
    // The type of the client should not be updated here.
    // This operation is always prepended by GET/READ so the client type is
    // set anyway.
    m_LastAccess = CNSPreciseTime::Current();
    if (cmd_group == eGet)
        m_WNData.AddToBlacklist(job_id, m_LastAccess);
    else
        m_ReaderData.AddToBlacklist(job_id, m_LastAccess);
}


// It updates running and reading job vectors
// only in case if the client session has been changed
void CNSClient::Touch(const CNSClientId &  client_id)
{
    m_LastAccess = CNSPreciseTime::Current();
    m_State = eActive;
    m_ControlPort = client_id.GetControlPort();
    m_ClientHost = client_id.GetClientHost();
    m_Addr = client_id.GetAddress();

    EClaimedClientType      claimed_client_type = client_id.GetType();
    if (claimed_client_type == eClaimedReset) {
        m_Type = 0;
        m_ClaimedType = eClaimedAutodetect;
    } else if (claimed_client_type != eClaimedNotProvided)
        m_ClaimedType = claimed_client_type;

    // Check the session id
    if (m_Session == client_id.GetSession())
        return;       // It's still the same session, nothing to check

    m_SessionStartTime = m_LastAccess;
    m_SessionResetTime = m_LastAccess;
    m_Session = client_id.GetSession();

    // Affinity members are handled in the upper level of Touch()
    // There is no need to do anything with the blacklisted jobs. If there are
    // some jobs there they should stay in the list.

    m_WNData.m_AffReset = false;
    m_ReaderData.m_AffReset = false;
}


// Prints the client info
string CNSClient::Print(const string &               node_name,
                        const CQueue *               queue,
                        const CNSAffinityRegistry &  aff_registry,
                        const set< string > &        wn_gc_clients,
                        const set< string > &        read_gc_clients,
                        bool                         verbose) const
{
    string      buffer;

    buffer += "OK:CLIENT: '" + node_name + "'\n"
              "OK:  STATUS: " + x_StateAsString() + "\n"
              "OK:  PREFERRED AFFINITIES RESET: ";
    if (m_WNData.m_AffReset) buffer += "TRUE\n";
    else                     buffer += "FALSE\n";

    buffer += "OK:  READER PREFERRED AFFINITIES RESET: ";
    if (m_ReaderData.m_AffReset) buffer += "TRUE\n";
    else                         buffer += "FALSE\n";

    if (m_LastAccess == kTimeZero)
        buffer += "OK:  LAST ACCESS: n/a\n";
    else
        buffer += "OK:  LAST ACCESS: " +
                  NS_FormatPreciseTime(m_LastAccess) + "\n";

    if (m_SessionStartTime == kTimeZero)
        buffer += "OK:  SESSION START TIME: n/a\n";
    else
        buffer += "OK:  SESSION START TIME: " +
                  NS_FormatPreciseTime(m_SessionStartTime) + "\n";

    if (m_RegistrationTime == kTimeZero)
        buffer += "OK:  REGISTRATION TIME: n/a\n";
    else
        buffer += "OK:  REGISTRATION TIME: " +
                  NS_FormatPreciseTime(m_RegistrationTime) + "\n";

    if (m_SessionResetTime == kTimeZero)
        buffer += "OK:  SESSION RESET TIME: n/a\n";
    else
        buffer += "OK:  SESSION RESET TIME: " +
                  NS_FormatPreciseTime(m_SessionResetTime) + "\n";

    buffer += "OK:  PEER ADDRESS: " + CSocketAPI::gethostbyaddr(m_Addr) + "\n";

    buffer += "OK:  CLIENT HOST: ";
    if (m_ClientHost.empty())
        buffer += "n/a\n";
    else
        buffer += m_ClientHost + "\n";

    buffer += "OK:  WORKER NODE CONTROL PORT: ";
    if (m_ControlPort == 0)
       buffer += "n/a\n";
    else
       buffer += NStr::NumericToString(m_ControlPort) + "\n";

    if (m_Session.empty())
        buffer += "OK:  SESSION: n/a\n";
    else
        buffer += "OK:  SESSION: '" + m_Session + "'\n";

    buffer += "OK:  TYPE: " + x_TypeAsString() + "\n";

    buffer += "OK:  NUMBER OF SUBMITTED JOBS: " +
              NStr::NumericToString(m_NumberOfSubmitted) + "\n";

    TNSBitVector    blacklist = m_WNData.GetBlacklistedJobs();
    buffer += "OK:  NUMBER OF BLACKLISTED JOBS: " +
              NStr::NumericToString(blacklist.count()) + "\n";
    if (verbose && blacklist.any()) {
        buffer += "OK:  BLACKLISTED JOBS:\n";

        TNSBitVector::enumerator    en(blacklist.first());
        for ( ; en.valid(); ++en) {
            unsigned int    job_id = *en;
            TJobStatus      status = queue->GetJobStatus(job_id);
            buffer += "OK:    " + queue->MakeJobKey(job_id) + " " +
                      m_WNData.GetBlacklistLimit(job_id) + " " +
                      CNetScheduleAPI::StatusToString(status) + "\n";
        }
    }

    blacklist = m_ReaderData.GetBlacklistedJobs();
    buffer += "OK:  NUMBER OF READ BLACKLISTED JOBS: " +
              NStr::NumericToString(blacklist.count()) + "\n";
    if (verbose && blacklist.any()) {
        buffer += "OK:  READ BLACKLISTED JOBS:\n";

        TNSBitVector::enumerator    en(blacklist.first());
        for ( ; en.valid(); ++en) {
            unsigned int    job_id = *en;
            TJobStatus      status = queue->GetJobStatus(job_id);
            buffer += "OK:    " + queue->MakeJobKey(job_id) + " " +
                      m_ReaderData.GetBlacklistLimit(job_id) + " " +
                      CNetScheduleAPI::StatusToString(status) + "\n";
        }
    }

    buffer += "OK:  NUMBER OF RUNNING JOBS: " +
              NStr::NumericToString(m_WNData.m_Jobs.count()) + "\n";
    if (verbose && m_WNData.m_Jobs.any()) {
        buffer += "OK:  RUNNING JOBS:\n";

        TNSBitVector::enumerator    en(m_WNData.m_Jobs.first());
        for ( ; en.valid(); ++en)
            buffer += "OK:    " + queue->MakeJobKey(*en) + "\n";
    }

    buffer += "OK:  NUMBER OF JOBS GIVEN FOR EXECUTION: " +
              NStr::NumericToString(m_WNData.m_NumberOfGiven) + "\n";

    buffer += "OK:  NUMBER OF READING JOBS: " +
              NStr::NumericToString(m_ReaderData.m_Jobs.count()) + "\n";
    if (verbose && m_ReaderData.m_Jobs.any()) {
        buffer += "OK:  READING JOBS:\n";

        TNSBitVector::enumerator    en(m_ReaderData.m_Jobs.first());
        for ( ; en.valid(); ++en)
            buffer += "OK:    " + queue->MakeJobKey(*en) + "\n";
    }

    buffer += "OK:  NUMBER OF JOBS GIVEN FOR READING: " +
              NStr::NumericToString(m_ReaderData.m_NumberOfGiven) + "\n";

    buffer += "OK:  NUMBER OF PREFERRED AFFINITIES: " +
              NStr::NumericToString(m_WNData.m_PrefAffinities.count()) + "\n";
    if (verbose && m_WNData.m_PrefAffinities.any()) {
        buffer += "OK:  PREFERRED AFFINITIES:\n";

        TNSBitVector::enumerator    en(m_WNData.m_PrefAffinities.first());
        for ( ; en.valid(); ++en)
            buffer += "OK:    '" + aff_registry.GetTokenByID(*en) + "'\n";
    }

    buffer += "OK:  NUMBER OF REQUESTED AFFINITIES: " +
              NStr::NumericToString(m_WNData.m_WaitAffinities.count()) + "\n";
    if (verbose && m_WNData.m_WaitAffinities.any()) {
        buffer += "OK:  REQUESTED AFFINITIES:\n";

        TNSBitVector::enumerator    en(m_WNData.m_WaitAffinities.first());
        for ( ; en.valid(); ++en)
            buffer += "OK:    '" + aff_registry.GetTokenByID(*en) + "'\n";
    }

    buffer += "OK:  NUMBER OF READER PREFERRED AFFINITIES: " +
              NStr::NumericToString(m_ReaderData.m_PrefAffinities.count()) +
              "\n";
    if (verbose && m_ReaderData.m_PrefAffinities.any()) {
        buffer += "OK:  READER PREFERRED AFFINITIES:\n";

        TNSBitVector::enumerator    en(m_ReaderData.m_PrefAffinities.first());
        for ( ; en.valid(); ++en)
            buffer += "OK:    '" + aff_registry.GetTokenByID(*en) + "'\n";
    }

    buffer += "OK:  NUMBER OF READER REQUESTED AFFINITIES: " +
              NStr::NumericToString(m_ReaderData.m_WaitAffinities.count()) +
              "\n";
    if (verbose && m_ReaderData.m_WaitAffinities.any()) {
        buffer += "OK:  READER REQUESTED AFFINITIES:\n";

        TNSBitVector::enumerator    en(m_ReaderData.m_WaitAffinities.first());
        for ( ; en.valid(); ++en)
            buffer += "OK:    '" + aff_registry.GetTokenByID(*en) + "'\n";
    }

    buffer += "OK:  NUMBER OF SOCKET WRITE ERRORS: " +
              NStr::NumericToString(m_NumberOfSockErrors) + "\n"
              "OK:  DATA: '" + NStr::PrintableString(m_ClientData) + "'\n"
              "OK:  DATA VERSION: " +
                    NStr::NumericToString(m_ClientDataVersion) + "\n";

    if (wn_gc_clients.find(node_name) == wn_gc_clients.end())
        buffer += "OK:  WN AFFINITIES GARBAGE COLLECTED: FALSE\n";
    else
        buffer += "OK:  WN AFFINITIES GARBAGE COLLECTED: TRUE\n";

    if (read_gc_clients.find(node_name) == read_gc_clients.end())
        buffer += "OK:  READER AFFINITIES GARBAGE COLLECTED: FALSE\n";
    else
        buffer += "OK:  READER AFFINITIES GARBAGE COLLECTED: TRUE\n";

    return buffer;
}


void
CNSClient::GCBlacklistedJobs(const CJobStatusTracker &  tracker,
                             ECommandGroup              cmd_group)
{
    vector<TJobStatus>      match_states;
    match_states.push_back(CNetScheduleAPI::eJobNotFound);
    match_states.push_back(CNetScheduleAPI::eConfirmed);
    match_states.push_back(CNetScheduleAPI::eReadFailed);

    if (cmd_group == eGet) {
        match_states.push_back(CNetScheduleAPI::eDone);
        match_states.push_back(CNetScheduleAPI::eFailed);
        match_states.push_back(CNetScheduleAPI::eCanceled);

        m_WNData.GCBlacklist(tracker, match_states);
    } else
        m_ReaderData.GCBlacklist(tracker, match_states);
}


int  CNSClient::SetClientData(const string &  data, int  data_version)
{
    if (data_version != -1 && data_version != m_ClientDataVersion)
        NCBI_THROW(CNetScheduleException, eClientDataVersionMismatch,
                   "client data version does not match");
    m_ClientData = data;
    ++m_ClientDataVersion;
    if (m_ClientDataVersion < 0)
        m_ClientDataVersion = 0;
    return m_ClientDataVersion;
}


string  CNSClient::x_TypeAsString(void) const
{
    string      result;

    switch (m_ClaimedType) {
        case eClaimedSubmitter:     return "submitter";
        case eClaimedWorkerNode:    return "worker node";
        case eClaimedReader:        return "reader";
        case eClaimedAdmin:         return "admin";

        // autodetect or not provided falls to detection by a command group
        case eClaimedAutodetect:
        case eClaimedNotProvided:
        default:
                break;
    }

    if (m_Type & eSubmitter)
        result = "submitter";

    if (m_Type & eWorkerNode) {
        if (!result.empty())
            result += " | ";
        result += "worker node";
    }

    if (m_Type & eReader) {
        if (!result.empty())
            result += " | ";
        result += "reader";
    }

    if (m_Type & eAdmin) {
        if (!result.empty())
            result += " | ";
        result += "admin";
    }

    if (result.empty())
        return "unknown";
    return result;
}


string  CNSClient::x_StateAsString(void) const
{
    switch (m_State) {
        case eActive:           return "active";
        case eWNStale:          return "worker node stale";
        case eReaderStale:      return "reader stale";
        case eWNAndReaderStale: return "worker node stale | reader stale";
        case eQuit:             return "quit";
        default: ;
    }
    return "unknown";
}

END_NCBI_SCOPE

