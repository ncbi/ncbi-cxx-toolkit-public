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

#include "ns_clients.hpp"
#include "ns_queue.hpp"
#include "queue_vc.hpp"
#include "ns_affinity.hpp"


BEGIN_NCBI_SCOPE


// The CClientId serves two types of clients:
// - old style clients; they have peer address only
// - new style clients; they have all three pieces,
//                      address, node id and session id
CNSClientId::CNSClientId() :
    m_Addr(0), m_Capabilities(0), m_ID(0)
{}


CNSClientId::CNSClientId(unsigned int            peer_addr,
                         const TNSProtoParams &  params) :
    m_Capabilities(0), m_Unreported(~0L),
    m_VersionControl(true),
    m_ID(0)
{
    Update(peer_addr, params);
    return;
}


void CNSClientId::Update(unsigned int            peer_addr,
                         const TNSProtoParams &  params)
{
    TNSProtoParams::const_iterator      found;


    m_Addr           = peer_addr;
    m_ProgName       = "";
    m_ClientName     = "";
    m_ClientNode     = "";
    m_ClientSession  = "";
    m_ClientHost     = "";
    m_ControlPort    = 0;
    m_Capabilities   = 0;
    m_Unreported     = ~0L;
    m_VersionControl = true;
    m_ID             = 0;

    found = params.find("client_node");
    if (found != params.end())
        m_ClientNode = found->second;

    found = params.find("client_session");
    if (found != params.end())
        m_ClientSession = found->second;

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

    return;
}


// true if it is a new client identification
bool CNSClientId::IsComplete(void) const
{
    // It is gauranteed in the constructor that both
    // m_ClientNode and m_ClientSession are empty or not empty together.
    return !m_ClientNode.empty();
}


unsigned int CNSClientId::GetAddress(void) const
{
    return m_Addr;
}


const string &  CNSClientId::GetNode(void) const
{
    return m_ClientNode;
}


const string &  CNSClientId::GetSession(void) const
{
    return m_ClientSession;
}


unsigned short  CNSClientId::GetControlPort(void) const
{
    return m_ControlPort;
}


const string &  CNSClientId::GetClientHost(void) const
{
    return m_ClientHost;
}


const string &  CNSClientId::GetProgramName(void) const
{
    return m_ProgName;
}


const string &  CNSClientId::GetClientName(void) const
{
    return m_ClientName;
}


unsigned int  CNSClientId::GetCapabilities(void) const
{
    return m_Capabilities;
}


// This is for rude clients which provide the first (i.e. authorization)
// in the format that cannot be parsed. In this case the whole line
// is considered to be the client name.
void CNSClientId::SetClientName(const string &  client_name)
{
    m_ClientName = client_name;
    return;
}


void CNSClientId::SetControlPort(unsigned short  port)
{
    m_ControlPort = port;
    return;
}


void CNSClientId::SetClientHost(const string &  host)
{
    m_ClientHost = host;
    return;
}


void CNSClientId::AddCapability(unsigned int  capabilities)
{
    m_Capabilities |= capabilities;
    return;
}


// The code of this function has been moved from the
// CNetScheduleHandler::x_CheckAccess().
// The strange manipulations with eNSAC_DynQueueAdmin and
// eNSAC_DynClassAdmin are left as they were as well as the logic with
// reporting access violation and throwing exceptions.
void CNSClientId::CheckAccess(TNSClientRole   role,
                              const CQueue *  queue)
{
    unsigned int    deficit = role & (~m_Capabilities);

    if (!deficit)
        return;

    if (deficit & eNSAC_DynQueueAdmin) {
        // check that we have admin rights on queue m_JobReq.param1
        deficit &= ~eNSAC_DynQueueAdmin;
    }

    if (deficit & eNSAC_DynClassAdmin) {
        // check that we have admin rights on class m_JobReq.param2
        deficit &= ~eNSAC_DynClassAdmin;
    }

    if (!deficit)
        return;

    bool report =
        !((~m_Capabilities) & eNSAC_Queue)  &&  // only if there is a queue
        (m_Unreported & deficit);               // and we did not report it yet


    if (report) {
        m_Unreported &= ~deficit;
        ERR_POST("Unauthorized access from: " +
                 CSocketAPI::gethostbyaddr(m_Addr) + " " +
                 m_ClientName + x_AccessViolationMessage(deficit));
    } else
        m_Unreported = ~0L;

    bool deny =
        (deficit & eNSAC_AnyAdminMask)    ||  // for any admin access
        ((~m_Capabilities) & eNSAC_Queue);    // or if no queue
    if (queue != NULL)
        deny |= queue->GetDenyAccessViolations();

    if (deny)
        NCBI_THROW(CNetScheduleException, eAccessDenied,
                   "Access denied:" + x_AccessViolationMessage(deficit));
        // No space required above: the violation message will have a leading
        // space.

    return;
}


// The check should be done once per the client session or
// even once for the client.
bool  CNSClientId::CheckVersion(const CQueue *  queue)
{
    // There is nothing to check if it is not a queue required ops
    if (queue == NULL || m_VersionControl == false)
        return true;

    if (m_VersionControl)
        m_VersionControl = queue->IsVersionControl();

    if (m_VersionControl == false)
        return true;


    m_VersionControl = false;
    if ((m_Capabilities & eNSAC_Admin) != 0)
        return true;


    if (m_ProgName.empty())
        return false;

    try {
        CQueueClientInfo    auth_prog_info;

        ParseVersionString(m_ProgName,
                           &auth_prog_info.client_name,
                           &auth_prog_info.version_info);
        return queue->IsMatchingClient(auth_prog_info);
    }
    catch (const exception &) {
        // There could be parsing errors
        return false;
    }
}


unsigned int  CNSClientId::GetID(void) const
{
    return m_ID;
}


void  CNSClientId::SetID(unsigned int  id)
{
    m_ID = id;
    return;
}


string  CNSClientId::x_AccessViolationMessage(unsigned int  deficit)
{
    string      message;

    if (deficit & eNSAC_Queue)
        message += " queue required";
    if (deficit & eNSAC_Worker)
        message += " worker node privileges required";
    if (deficit & eNSAC_Submitter)
        message += " submitter privileges required";
    if (deficit & eNSAC_Admin)
        message += " admin privileges required";
    if (deficit & eNSAC_QueueAdmin)
        message += " queue admin privileges required";
    return message;
}




// The CNSClient implementation

// This constructor is for the map<> container
CNSClient::CNSClient() :
    m_Cleared(false),
    m_Type(0),
    m_Addr(0),
    m_ControlPort(0),
    m_LastAccess(),
    m_Session(),
    m_RunningJobs(bm::BM_GAP),
    m_ReadingJobs(bm::BM_GAP),
    m_BlacklistedJobs(bm::BM_GAP),
    m_WaitPort(0),
    m_ID(0),
    m_Affinities(bm::BM_GAP),
    m_WaitAffinities(bm::BM_GAP),
    m_NumberOfSubmitted(0),
    m_NumberOfRead(0),
    m_NumberOfRun(0),
    m_AffReset(false)
{}


CNSClient::CNSClient(const CNSClientId &  client_id) :
    m_Cleared(false),
    m_Type(0),
    m_Addr(client_id.GetAddress()),
    m_ControlPort(client_id.GetControlPort()),
    m_ClientHost(client_id.GetClientHost()),
    m_LastAccess(time(0)),
    m_Session(client_id.GetSession()),
    m_RunningJobs(bm::BM_GAP),
    m_ReadingJobs(bm::BM_GAP),
    m_BlacklistedJobs(bm::BM_GAP),
    m_WaitPort(0),
    m_ID(0),
    m_Affinities(bm::BM_GAP),
    m_WaitAffinities(bm::BM_GAP),
    m_NumberOfSubmitted(0),
    m_NumberOfRead(0),
    m_NumberOfRun(0),
    m_AffReset(false)
{
    if (!client_id.IsComplete())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Creating client information object for old style clients");
    return;
}


// Returns true if there was at least something in the preferred affinites list
bool CNSClient::Clear(void)
{
    m_Cleared = true;
    m_Session = "";

    m_RunningJobs.clear();
    m_ReadingJobs.clear();

    m_WaitAffinities.clear();

    if (m_Affinities.any()) {
        m_Affinities.clear();
        return true;
    }
    return false;
}


unsigned short CNSClient::GetAndResetWaitPort(void)
{
    unsigned short  old_port = m_WaitPort;

    m_WaitPort = 0;
    return old_port;
}


void CNSClient::RegisterRunningJob(unsigned int  job_id)
{
    m_LastAccess = time(0);
    m_Type |= eWorkerNode;

    m_RunningJobs.set(job_id, true);
    ++m_NumberOfRun;
    return;
}


void CNSClient::RegisterReadingJob(unsigned int  job_id)
{
    m_LastAccess = time(0);
    m_Type |= eReader;

    m_ReadingJobs.set(job_id, true);
    ++m_NumberOfRead;
    return;
}


void CNSClient::RegisterSubmittedJobs(size_t  count)
{
    m_LastAccess = time(0);
    m_Type |= eSubmitter;
    m_NumberOfSubmitted += count;
    return;
}


void CNSClient::RegisterBlacklistedJob(unsigned int  job_id)
{
    // The type of the client should not be updated here.
    // This operation is always prepended by GET/READ so the client type is
    // set anyway.
    m_LastAccess = time(0);
    m_BlacklistedJobs.set_bit(job_id, true);
    return;
}


void CNSClient::UnregisterReadingJob(unsigned int  job_id)
{
    m_ReadingJobs.set(job_id, false);
    return;
}


// returns true if the modifications have been really made
bool CNSClient::MoveReadingJobToBlacklist(unsigned int  job_id)
{
    if (m_ReadingJobs[job_id]) {
        m_BlacklistedJobs.set(job_id, true);
        m_ReadingJobs.set(job_id, false);
        return true;
    }
    return false;
}


void CNSClient::UnregisterRunningJob(unsigned int  job_id)
{
    m_RunningJobs.set(job_id, false);
    return;
}


// returns true if the modifications have been really made
bool CNSClient::MoveRunningJobToBlacklist(unsigned int  job_id)
{
    if (m_RunningJobs[job_id]) {
        m_BlacklistedJobs.set(job_id, true);
        m_RunningJobs.set(job_id, false);
        return true;
    }
    return false;
}


// It updates running and reading job vectors only in case if the client
// session has been changed
// Returns true if the preferred affinities had at least one bit and the
// session has been changed
bool CNSClient::Touch(const CNSClientId &  client_id,
                      TNSBitVector &       running_jobs,
                      TNSBitVector &       reading_jobs)
{
    m_LastAccess = time(0);
    m_Cleared = false;
    m_ControlPort = client_id.GetControlPort();
    m_ClientHost = client_id.GetClientHost();

    // Check the session id
    if (m_Session == client_id.GetSession())
        return false;       // It's still the same session, nothing to check

    // Here: new session so check if there are running or reading jobs
    if (m_RunningJobs.any()) {
        running_jobs = m_RunningJobs;
        m_RunningJobs.clear();
    }

    if (m_ReadingJobs.any()) {
        reading_jobs = m_ReadingJobs;
        m_ReadingJobs.clear();
    }

    // Update the session identifier
    m_Session = client_id.GetSession();

    m_WaitAffinities.clear();

    // There is no need to do anything neither with the blacklisted jobs
    // nor submitted jobs

    if (m_Affinities.any()) {
        m_Affinities.clear();
        return true;
    }
    return false;
}


// Prints the client info
string CNSClient::Print(const string &               node_name,
                        const CQueue *               queue,
                        const CNSAffinityRegistry &  aff_registry,
                        bool                         verbose) const
{
    string      buffer;

    buffer += "OK:CLIENT: '" + node_name + "'\n";
    if (m_Cleared)
        buffer += "OK:  STATUS: cleared\n";
    else
        buffer += "OK:  STATUS: active\n";
    buffer += "OK:  PREFERRED AFFINITIES RESET: ";
    if (m_AffReset) buffer += "TRUE\n";
    else            buffer += "FALSE\n";

    CTime       access_time;
    access_time.SetTimeT(m_LastAccess);
    access_time.ToLocalTime();
    buffer += "OK:  LAST ACCESS: " + access_time.AsString() + "\n";
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
              NStr::SizetToString(m_NumberOfSubmitted) + "\n";

    if (m_BlacklistedJobs.any()) {
        if (verbose) {
            buffer += "OK:  BLACKLISTED JOBS:\n";

            TNSBitVector::enumerator    en(m_BlacklistedJobs.first());
            for ( ; en.valid(); ++en)
                buffer += "OK:    " + queue->MakeKey(*en) + "\n";
        } else {
            buffer += "OK:  NUMBER OF BLACKLISTED JOBS: " +
                      NStr::UIntToString(m_BlacklistedJobs.count()) + "\n";
        }
    }

    if (m_RunningJobs.any()) {
        buffer += "OK:  RUNNING JOBS:\n";

        TNSBitVector::enumerator    en(m_RunningJobs.first());
        for ( ; en.valid(); ++en)
            buffer += "OK:    " + queue->MakeKey(*en) + "\n";
    }

    buffer += "OK:  NUMBER OF JOBS GIVEN FOR EXECUTION: " +
              NStr::SizetToString(m_NumberOfRun) + "\n";

    if (m_ReadingJobs.any()) {
        buffer += "OK:  READING JOBS:\n";

        TNSBitVector::enumerator    en(m_ReadingJobs.first());
        for ( ; en.valid(); ++en)
            buffer += "OK:    " + queue->MakeKey(*en) + "\n";
    }

    buffer += "OK:  NUMBER OF JOBS GIVEN FOR READING: " +
              NStr::SizetToString(m_NumberOfRead) + "\n";

    if (m_Affinities.any()) {
        if (verbose) {
            buffer += "OK:  PREFERRED AFFINITIES:\n";

            TNSBitVector::enumerator    en(m_Affinities.first());
            for ( ; en.valid(); ++en)
                buffer += "OK:    '" + aff_registry.GetTokenByID(*en) + "'\n";
        } else {
            buffer += "OK:  NUMBER OF PREFERRED AFFINITIES: " +
                      NStr::UIntToString(m_Affinities.count()) + "\n";
        }
    }

    if (m_WaitAffinities.any()) {
        if (verbose) {
            buffer += "OK:  REQUESTED AFFINITIES:\n";

            TNSBitVector::enumerator    en(m_WaitAffinities.first());
            for ( ; en.valid(); ++en)
                buffer += "OK:    '" + aff_registry.GetTokenByID(*en) + "'\n";
        } else {
            buffer += "OK:  NUMBER OF REQUESTED AFFINITIES: " +
                      NStr::UIntToString(m_WaitAffinities.count()) + "\n";
        }
    }

    return buffer;
}


void  CNSClient::AddPreferredAffinities(const TNSBitVector &  aff)
{
    m_Type |= eWorkerNode;
    m_Affinities |= aff;
    m_AffReset = false;
    return;
}


void  CNSClient::AddPreferredAffinity(unsigned int  aff)
{
    m_Type |= eWorkerNode;
    if (aff != 0)
        m_Affinities.set_bit(aff, true);
    m_AffReset = false;
    return;
}


void  CNSClient::RemovePreferredAffinities(const TNSBitVector &  aff)
{
    m_Type |= eWorkerNode;
    m_Affinities -= aff;
    m_AffReset = false;
    return;
}


void  CNSClient::RemovePreferredAffinity(unsigned int  aff)
{
    m_Type |= eWorkerNode;
    if (aff != 0)
        m_Affinities.set_bit(aff, false);
    m_AffReset = false;
    return;
}


void  CNSClient::RegisterWaitAffinities(const TNSBitVector &  aff)
{
    m_Type |= eWorkerNode;
    m_WaitAffinities = aff;
    return;
}


// Used in the notifications code to test if a notifications should be sent to
// the client
bool  CNSClient::IsRequestedAffinity(const TNSBitVector &  aff,
                                     bool                  use_preferred) const
{
    if ((m_WaitAffinities & aff).any())
        return true;

    if (use_preferred)
        if ((m_Affinities & aff).any())
            return true;

    return false;
}


bool  CNSClient::ClearPreferredAffinities(void)
{
    if (m_Affinities.any()) {
        m_Affinities.clear();
        return true;
    }
    return false;
}


string  CNSClient::x_TypeAsString(void) const
{
    string      result;

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

    if (result.empty())
        return "unknown";
    return result;
}


END_NCBI_SCOPE

