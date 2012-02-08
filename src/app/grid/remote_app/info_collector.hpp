#ifndef __NETSCHEDULE_INFO_COLLECTOR__HPP
#define __NETSCHEDULE_INFO_COLLECTOR__HPP

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
 * Authors: Maxim Didenko
 *
 * File Description:
 *
 */

#include <connect/services/netschedule_api.hpp>
#include <connect/services/remote_app.hpp>

#include <corelib/ncbimisc.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////
///
class CNSInfoCollector;

class CNSJobInfo
{
public:
    CNSJobInfo(const string& id, CNSInfoCollector& collector);

    ~CNSJobInfo();

    CNetScheduleAPI::EJobStatus GetStatus() const;
    const string& GetId() const { return m_Job.job_id; }
    const string& GetProgressMsg() const { return m_Job.progress_msg; }
    const string& GetRawInput() const;
    const string& GetRawOutput() const;
    const string& GetErrMsg() const;
    int GetRetCode() const;

    CNcbiIstream& GetStdIn() const;
    CNcbiIstream& GetStdOut() const;
    CNcbiIstream& GetStdErr() const;
    const string& GetCmdLine() const;
   
private:

    void x_Load();
    void x_Load() const;

    CNcbiIstream*               x_CreateStream(const string& blob_or_data) const;
    CRemoteAppRequest&          x_GetRequest() const;
    CRemoteAppResult&           x_GetResult() const;

    CNetScheduleJob m_Job;
    CNetScheduleAPI::EJobStatus m_Status;

    CNSInfoCollector& m_Collector;
    mutable CRemoteAppRequest* m_Request;
    mutable CRemoteAppResult* m_Result;

    CNSJobInfo(const CNSJobInfo&);
    CNSJobInfo& operator=(const CNSJobInfo&);
};

//////////////////////////////////////////////////////////////////////
///
class CNSInfoCollector
{
public:
    CNSInfoCollector(const string& queue, const string& service_name,
                     CNetCacheAPI::TInstance nc_api);
    explicit CNSInfoCollector(CNetCacheAPI::TInstance nc_api);

    template<typename TInfo>
    class IAction {
    public:
        virtual ~IAction() {}
        virtual void operator()(const TInfo& info) = 0;
    };

    void TraverseJobs(CNetScheduleAPI::EJobStatus, IAction<CNSJobInfo>&);
    void TraverseNodes(IAction<CNetScheduleAdmin::SWorkerNodeInfo>&);
    void CancelJob(const std::string& jid);

    void GetQueues(CNetScheduleAdmin::TQueueList& queues);

    CNSJobInfo* CreateJobInfo(const string& job_id);

    CNcbiIstream* GetBlobContent(const string& blob_id, size_t* blob_size);

private:

    friend class CNSJobInfo;

    CNetScheduleAPI m_NetScheduleAPI;
    CNetScheduleAPI x_GetNetScheduleAPI() {
        if (!m_NetScheduleAPI)
            throw runtime_error("NetScheduele service is not set");
        return m_NetScheduleAPI; 
    }

    CRemoteAppRequest& x_GetRequest();
    CRemoteAppResult& x_GetResult();

    CNetCacheAPI m_NetCacheAPI;

    CNetCacheAPI x_GetNetCacheAPI() {
        if (!m_NetCacheAPI)
            throw runtime_error("NetCache service is not set");
        return m_NetCacheAPI;
    }

    auto_ptr<CRemoteAppRequest> m_Request;
    auto_ptr<CRemoteAppResult> m_Result;
};

END_NCBI_SCOPE

#endif 

