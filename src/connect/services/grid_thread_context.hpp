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

#include <util/request_control.hpp>

#include <connect/services/grid_worker.hpp>
#include <connect/services/ns_client_wrappers.hpp>


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// 
///@internal
class CGridThreadContext
{
public:
    explicit CGridThreadContext(CGridWorkerNode& node, long check_status_period = 2);

    CWorkerNodeJobContext& GetJobContext();

    void SetJobContext(CWorkerNodeJobContext& job_context);
    void SetJobContext(CWorkerNodeJobContext& job_context, const CNetScheduleJob& job);
    void CloseStreams();
    void Reset();

    CNcbiIstream& GetIStream(IBlobStorage::ELockMode);
    CNcbiOstream& GetOStream();
    void PutProgressMessage(const string& msg, bool send_immediately);

    void SetJobRunTimeout(unsigned time_to_run);
    void JobDelayExpiration(unsigned runtime_inc);

    bool IsJobCommitted() const;
    bool PutResult(int ret_code, CNetScheduleJob& new_job);
    void ReturnJob();
    void PutFailure(const string& msg);
    bool IsJobCanceled();
   
    IWorkerNodeJob* GetJob();
private:

    CWorkerNodeJobContext*        m_JobContext;
    CRef<IWorkerNodeJob>          m_Job;
    auto_ptr<INSCWrapper>         m_Reporter;

    auto_ptr<IBlobStorage>        m_Reader;
    auto_ptr<CNcbiIstream>        m_RStream;
    auto_ptr<IBlobStorage>        m_Writer;
    auto_ptr<CNcbiOstream>        m_WStream;

    auto_ptr<IBlobStorage>        m_ProgressWriter;
    CRequestRateControl           m_MsgThrottler; 
    long                          m_CheckStatusPeriod;
    CRequestRateControl           m_StatusThrottler; 

    CGridThreadContext(const CGridThreadContext&);
    CGridThreadContext& operator=(const CGridThreadContext&);
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.13  2006/06/28 16:01:56  didenko
 * Redone job's exlusivity processing
 *
 * Revision 6.12  2006/06/22 13:52:36  didenko
 * Returned back a temporary fix for CStdPoolOfThreads
 * Added check_status_period configuration paramter
 * Fixed exclusive mode checking
 *
 * Revision 6.11  2006/05/22 18:11:43  didenko
 * Added an option to fail a job if a remote app returns non zore code
 *
 * Revision 6.10  2006/05/10 19:54:21  didenko
 * Added JobDelayExpiration method to CWorkerNodeContext class
 * Added keep_alive_period and max_job_run_time parmerter to the config
 * file of remote_app
 *
 * Revision 6.9  2006/03/15 17:30:12  didenko
 * Added ability to use embedded NetSchedule job's storage as a job's input/output data instead of using it as a NetCache blob key. This reduces network traffic and increases job submittion speed.
 *
 * Revision 6.8  2006/02/15 19:48:34  didenko
 * Added new optional config parameter "reuse_job_object" which allows reusing
 * IWorkerNodeJob objects in the jobs' threads instead of creating
 * a new object for each job.
 *
 * Revision 6.7  2006/02/15 15:19:03  didenko
 * Implemented an optional possibility for a worker node to have a permanent connection
 * to a NetSchedule server.
 * To expedite the job exchange between a worker node and a NetSchedule server,
 * a call to CNetScheduleClient::PutResult method is replaced to a
 * call to CNetScheduleClient::PutResultGetJob method.
 *
 * Revision 6.6  2005/12/20 17:26:22  didenko
 * Reorganized netschedule storage facility.
 * renamed INetScheduleStorage to IBlobStorage and moved it to corelib
 * renamed INetScheduleStorageFactory to IBlobStorageFactory and moved it to corelib
 * renamed CNetScheduleNSStorage_NetCache to CBlobStorage_NetCache and moved it
 * to separate files
 * Moved CNetScheduleClientFactory to separate files
 *
 * Revision 6.5  2005/10/26 16:37:44  didenko
 * Added for non-blocking read for netschedule storage
 *
 * Revision 6.4  2005/09/30 14:58:56  didenko
 * Added optional parameter to PutProgressMessage methods which allows
 * sending progress messages regardless of the rate control.
 *
 * Revision 6.3  2005/05/10 14:13:10  didenko
 * Added CloseStreams method
 *
 * Revision 6.2  2005/05/06 13:08:06  didenko
 * Added check for a job cancelation in the GetShoutdownLevel method
 *
 * Revision 6.1  2005/05/05 15:18:51  didenko
 * Added debugging facility to worker nodes
 *
 * ===========================================================================
 */
 
#endif // _GRID_THREAD_CONTEXT_HPP_
