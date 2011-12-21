#ifndef _GRID_THREAD_CONTEXT_HPP_
#define _GRID_THREAD_CONTEXT_HPP_


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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *    NetSchedule Worker Node implementation
 */

#include <connect/services/grid_worker.hpp>

#include <corelib/request_control.hpp>


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
///@internal
class CGridThreadContext
{
public:
    explicit CGridThreadContext(CGridWorkerNode& node);

    void RunJobs(CWorkerNodeJobContext& job_context);
    void CloseStreams();

    CNcbiIstream& GetIStream();
    CNcbiOstream& GetOStream();
    void PutProgressMessage(const string& msg, bool send_immediately);

    void JobDelayExpiration(unsigned runtime_inc);

    bool PutResult(CNetScheduleJob& new_job);
    void ReturnJob();
    void PutFailure();
    void PutFailureAndIgnoreErrors(const char* error_message);
    bool IsJobCanceled();

    IWorkerNodeJob* GetJob();

private:
    CGridWorkerNode&              m_Worker;
    CWorkerNodeJobContext*        m_JobContext;
    CRef<IWorkerNodeJob>          m_Job;
    CNetScheduleExecuter          m_NetScheduleExecuter;

    CNetCacheAPI                  m_NetCacheAPI;
    auto_ptr<CNcbiIstream>        m_RStream;
    auto_ptr<IEmbeddedStreamWriter> m_Writer;
    auto_ptr<CNcbiOstream>        m_WStream;

    CRequestRateControl           m_MsgThrottler;
    long                          m_CheckStatusPeriod;
    CRequestRateControl           m_StatusThrottler;
};

END_NCBI_SCOPE

#endif // _GRID_THREAD_CONTEXT_HPP_
