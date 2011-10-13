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
#include "ns_handler.hpp"


BEGIN_NCBI_SCOPE


// The CClientId serves two types of clients:
// - old style clients; they have peer address only
// - new style clients; they have all three pieces,
//                      address, node id and session id
CNSClientId::CNSClientId() :
    m_Addr(0), m_Capabilities(0)
{}


CNSClientId::CNSClientId(unsigned int            peer_addr,
                         const TNSProtoParams &  params) :
    m_Capabilities(0), m_Unreported(~0L),
    m_VersionControl(true)
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
    m_Capabilities   = 0;
    m_Unreported     = ~0L;
    m_VersionControl = true;

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

    return;
}


// true if it is a new client identification
bool CNSClientId::IsComplete(void) const
{
    // It is gauranteed in the constructor that both
    // m_ClientNode and m_ClientSession are empty or not empty together.
    return !m_ClientNode.empty();
}


bool CNSClientId::IsAdminClientName(void) const
{
    return m_ClientName == "netschedule_control" ||
           m_ClientName == "netschedule_admin";
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
        LOG_POST(Warning << "Unauthorized access from: " +
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

    return;
}


// The check should be done once per the client session or
// even once for the client.
bool  CNSClientId::CheckVersion(const CQueue *  queue)
{
    // There is nothing to check if it is not a queue required ops
    if (queue == NULL)
        return true;

    if (m_VersionControl)
        m_VersionControl = queue->IsVersionControl();

    if (!m_VersionControl)
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
    m_LastAccess(),
    m_Session(),
    m_SubmittedJobs(bm::BM_GAP),
    m_RunningJobs(bm::BM_GAP),
    m_ReadingJobs(bm::BM_GAP),
    m_BlacklistedJobs(bm::BM_GAP),
    m_WaitPort(0)
{}


CNSClient::CNSClient(const CNSClientId &  client_id) :
    m_Cleared(false),
    m_Type(0),
    m_Addr(client_id.GetAddress()),
    m_LastAccess(time(0)),
    m_Session(client_id.GetSession()),
    m_SubmittedJobs(bm::BM_GAP),
    m_RunningJobs(bm::BM_GAP),
    m_ReadingJobs(bm::BM_GAP),
    m_BlacklistedJobs(bm::BM_GAP),
    m_WaitPort(0)
{
    if (!client_id.IsComplete())
        NCBI_THROW(CNetScheduleException, eInternalError,
                   "Creating client information object for old style clients");
    return;
}


void CNSClient::Clear(const CNSClientId &   client,
                      CQueue *              queue)
{
    m_Cleared = true;
    m_Session = "";

    if (m_RunningJobs.any()) {
        queue->ResetRunningDueToClear(client, m_RunningJobs);
        m_RunningJobs.clear();
    }

    if (m_ReadingJobs.any()) {
        queue->ResetReadingDueToClear(client, m_ReadingJobs);
        m_ReadingJobs.clear();
    }

    return;
}


TNSBitVector CNSClient::GetRunningJobs(void) const
{
    return m_RunningJobs;
}


TNSBitVector CNSClient::GetReadingJobs(void) const
{
    return m_ReadingJobs;
}


TNSBitVector CNSClient::GetBlacklistedJobs(void) const
{
    return m_BlacklistedJobs;
}


bool CNSClient::IsJobBlacklisted(unsigned int  job_id) const
{
    return m_BlacklistedJobs[job_id];
}


void CNSClient::SetWaitPort(unsigned short  port)
{
    m_WaitPort = port;
    return;
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
    return;
}


void CNSClient::RegisterReadingJob(unsigned int  job_id)
{
    m_LastAccess = time(0);
    m_Type |= eReader;

    m_ReadingJobs.set(job_id, true);
    return;
}


void CNSClient::RegisterSubmittedJob(unsigned int  job_id)
{
    m_LastAccess = time(0);
    m_Type |= eSubmitter;
    m_SubmittedJobs.set_bit(job_id, true);
    return;
}


// Used for batch submits
void CNSClient::RegisterSubmittedJobs(unsigned int  start_job_id,
                                      unsigned int  number_of_jobs)
{
    m_LastAccess = time(0);
    m_Type |= eSubmitter;
    m_SubmittedJobs.set_range(start_job_id,
                              start_job_id + number_of_jobs - 1,
                              true);
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


void CNSClient::Touch(const CNSClientId &  client_id,
                      CQueue *             queue)
{
    m_LastAccess = time(0);
    m_Cleared = false;

    // Check the session id
    if (m_Session == client_id.GetSession())
        return;     // It's still the same session, nothing to check

    // Here: new session so check if there are running or reading jobs
    if (m_RunningJobs.any()) {
        queue->ResetRunningDueToNewSession(client_id, m_RunningJobs);
        m_RunningJobs.clear();
    }

    if (m_ReadingJobs.any()) {
        queue->ResetReadingDueToNewSession(client_id, m_ReadingJobs);
        m_ReadingJobs.clear();
    }

    // Update the session identifier
    m_Session = client_id.GetSession();

    // There is no need to do anything neither with the blacklisted jobs
    // nor submitted jobs
    return;
}


// Prints the client info
void CNSClient::Print(const string &         node_name,
                      const CQueue *         queue,
                      CNetScheduleHandler &  handler,
                      bool                   verbose) const
{
    handler.WriteMessage("OK:CLIENT " + node_name);
    if (m_Cleared)
        handler.WriteMessage("OK:  STATUS: cleared");
    else
        handler.WriteMessage("OK:  STATUS: active");

     CTime       access_time;
     access_time.SetTimeT(m_LastAccess);
     access_time.ToLocalTime();
     handler.WriteMessage("OK:  LAST ACCESS: " + access_time.AsString());
     handler.WriteMessage("OK:  ADDRESS: " + CSocketAPI::gethostbyaddr(m_Addr));

     if (m_Session.empty())
         handler.WriteMessage("OK:  SESSION: n/a");
     else
         handler.WriteMessage("OK:  SESSION: " + m_Session);

    handler.WriteMessage("OK:  TYPE: " + x_TypeAsString());

    if (m_SubmittedJobs.any()) {
        if (verbose) {
            handler.WriteMessage("OK:  SUBMITTED JOBS:");

            TNSBitVector::enumerator    en(m_SubmittedJobs.first());
            for ( ; en.valid(); ++en)
                handler.WriteMessage("OK:    " + queue->MakeKey(*en));
        } else {
            handler.WriteMessage("OK:  NUMBER OF SUBMITTED JOBS: " +
                                 NStr::UIntToString(m_SubmittedJobs.count()));
        }
    }

    if (m_BlacklistedJobs.any()) {
        if (verbose) {
            handler.WriteMessage("OK:  BLACKLISTED JOBS:");

            TNSBitVector::enumerator    en(m_BlacklistedJobs.first());
            for ( ; en.valid(); ++en)
                handler.WriteMessage("OK:    " + queue->MakeKey(*en));
        } else {
            handler.WriteMessage("OK:  NUMBER OF BLACKLISTED JOBS:" +
                                 NStr::UIntToString(m_BlacklistedJobs.count()));
        }
    }

    if (m_RunningJobs.any()) {
        handler.WriteMessage("OK:  RUNNING JOBS:");

        TNSBitVector::enumerator    en(m_RunningJobs.first());
        for ( ; en.valid(); ++en)
            handler.WriteMessage("OK:    " + queue->MakeKey(*en));
    }

    if (m_ReadingJobs.any()) {
        handler.WriteMessage("OK:  READING JOBS:");

        TNSBitVector::enumerator    en(m_ReadingJobs.first());
        for ( ; en.valid(); ++en)
            handler.WriteMessage("OK:    " + queue->MakeKey(*en));
    }

    return;
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

