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

#include <string>


BEGIN_NCBI_SCOPE

// Forward declaration
class CQueue;
class CNetScheduleHandler;


// The CClientId serves two types of clients:
// - old style clients; they have peer address only
// - new style clients; they have all three pieces,
//                      address, node id and session id
class CNSClientId
{
    public:
        CNSClientId();
        CNSClientId(unsigned int            peer_addr,
                    const TNSProtoParams &  params);

        void Update(unsigned int            peer_addr,
                    const TNSProtoParams &  params);

        // true if it is a new client identification
        bool IsComplete(void) const;

        // true if the client name is one of the predefined
        // admin names
        bool IsAdminClientName(void) const;

        // Getters/setters
        unsigned int GetAddress(void) const;
        const string &  GetNode(void) const;
        const string &  GetSession(void) const;
        const string &  GetProgramName(void) const;
        const string &  GetClientName(void) const;
        unsigned int GetCapabilities(void) const;
        void SetClientName(const string &  client_name);
        void AddCapability(unsigned int  capabilities);
        void CheckAccess(TNSClientRole  role, const CQueue *  queue);
        bool CheckVersion(const CQueue *  queue);

    private:
        static string x_AccessViolationMessage(unsigned int  deficit);

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

        // Capabilities - that is combination of ENSAccess
        // rights, which can be performed by this connection
        unsigned int        m_Capabilities;

        unsigned int        m_Unreported;
        bool                m_VersionControl;
};



// The CNSClient stores information about new style clients only;
// The information includes the given jobs,
// type of the client (worker node, submitter) etc.
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
        CNSClient(const CNSClientId &  client_id);
        void Clear(const CNSClientId &  client_id,
                   CQueue *             queue);
        TNSBitVector GetRunningJobs(void) const;
        TNSBitVector GetReadingJobs(void) const;
        TNSBitVector GetBlacklistedJobs(void) const;
        bool IsJobBlacklisted(unsigned int  job_id) const;
        void SetWaitPort(unsigned short  port);
        unsigned short GetAndResetWaitPort(void);

        void RegisterRunningJob(unsigned int  job_id);
        void RegisterReadingJob(unsigned int  job_id);
        void RegisterSubmittedJob(unsigned int  job_id);
        void RegisterSubmittedJobs(unsigned int  start_job_id,
                                   unsigned int  number_of_jobs);
        void RegisterBlacklistedJob(unsigned int  job_id);
        void UnregisterReadingJob(unsigned int  job_id);
        bool MoveReadingJobToBlacklist(unsigned int  job_id);
        void UnregisterRunningJob(unsigned int  job_id);
        bool MoveRunningJobToBlacklist(unsigned int  job_id);

        void Touch(const CNSClientId &  client_id,
                   CQueue *             queue);
        void Print(const string &         node_name,
                   const CQueue *         queue,
                   CNetScheduleHandler &  handler,
                   bool                   verbose) const;

    private:
        bool            m_Cleared;        // Set to true when CLRN is received
                                          // If true => m_Session == "n/a"
        unsigned int    m_Type;           // bit mask of ENSClientType
        unsigned int    m_Addr;           // Client peer address
        time_t          m_LastAccess;     // The last time the client accessed
                                          // netschedule
        string          m_Session;        // Client session id

        TNSBitVector    m_SubmittedJobs;    // The jobs the client submitted
        TNSBitVector    m_RunningJobs;      // The jobs the client is currently
                                            // executing
        TNSBitVector    m_ReadingJobs;      // The jobs the client is currently
                                            // reading
        TNSBitVector    m_BlacklistedJobs;  // The jobs that should not be given
                                            // to the node neither for
                                            // executing nor for reading
        unsigned short  m_WaitPort;         // Port, provided in WGET command or
                                            // 0 otherwise

        string  x_TypeAsString(void) const;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_CLIENTS__HPP */

