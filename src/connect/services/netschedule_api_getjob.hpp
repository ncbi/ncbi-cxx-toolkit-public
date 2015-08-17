#ifndef CONN_SERVICES___NETSCHEDULE_API_GETJOB__HPP
#define CONN_SERVICES___NETSCHEDULE_API_GETJOB__HPP

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
 * Authors: Rafael Sadyrov
 *
 * File Description:
 *   NetSchedule API get/read job implementation.
 *
 */

#include <connect/services/netschedule_api.hpp>

#include <vector>
#include <list>


BEGIN_NCBI_SCOPE


// A namespace-like class
struct CNetScheduleGetJob
{
    // Vector of priority-ordered pairs of affinities and
    // corresponding priority-ordered comma-separated affinity lists.
    // E.g., for "a, b, c" it would be:
    // { "a", "a"       },
    // { "b", "a, b"    },
    // { "c", "a, b, c" }
    typedef vector<pair<string, string> > TAffinityLadder;

    enum EState {
        eWorking,
        eRestarted,
        eStopped
    };

    enum EResult {
        eJob,
        eAgain,
        eInterrupt,
        eNoJobs
    };

    struct SEntry
    {
        SServerAddress server_address;
        CDeadline deadline;
        bool all_affinities;
        bool more_jobs;

        SEntry(const SServerAddress& a, bool j = true) :
            server_address(a),
            deadline(0, 0),
            all_affinities(true),
            more_jobs(j)
        {
        }

        bool operator==(const SEntry& rhs) const
        {
            return server_address == rhs.server_address;
        }
    };
};

template <class TImpl>
class CNetScheduleGetJobImpl : public CNetScheduleGetJob
{
public:
    CNetScheduleGetJobImpl(TImpl& impl) :
        m_Impl(impl),
        m_DiscoveryAction(SServerAddress(0, 0), false)
    {
        m_ImmediateActions.push_back(m_DiscoveryAction);
    }

    CNetScheduleGetJob::EResult GetJob(
            const CDeadline& deadline,
            CNetScheduleJob& job,
            CNetScheduleAPI::EJobStatus* job_status);

private:
    template <class TJobHolder>
    EResult GetJobImmediately(TJobHolder& holder);

    template <class TJobHolder>
    EResult GetJobImpl(const CDeadline& deadline, TJobHolder& holder);

    void Restart();
    void MoveToImmediateActions(SNetServerImpl* server_impl);
    void NextDiscoveryIteration();
    void ReturnNotFullyCheckedServers();

    TImpl& m_Impl;
    list<SEntry> m_ImmediateActions, m_ScheduledActions;
    SEntry m_DiscoveryAction;
};


END_NCBI_SCOPE


#endif  /* CONN_SERVICES___NETSCHEDULE_API_GETJOB__HPP */
