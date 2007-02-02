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
 * Authors:  Maxim Didenko
 *
 * File Description:  NetSchedule worker node sample
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiexec.hpp>

#include <connect/services/grid_worker_app.hpp>

#include <cgi/ncbicgi.hpp>

#include <vector>
#include <algorithm>

#include "exec_helpers.hpp"

USING_NCBI_SCOPE;

    
///////////////////////////////////////////////////////////////////////

/// NetSchedule sample job

class CRemoteCgiJob : public IWorkerNodeJob
{
public:
    CRemoteCgiJob(const IWorkerNodeInitContext& context);

    virtual ~CRemoteCgiJob() {} 

    int Do(CWorkerNodeJobContext& context) 
    {
        if (context.IsLogRequested()) {
            LOG_POST( CTime(CTime::eCurrent).AsString() 
                      << ": " << context.GetJobKey() + " " + context.GetJobInput());
        }

        string tmp_path = m_Params.GetTempDir();
        if (!tmp_path.empty())
            tmp_path += CDirEntry::GetPathSeparator() + context.GetJobKey();

        CCgiRequest request;
        request.Deserialize(context.GetIStream(), CCgiRequest::fIgnoreQueryString |
                                                  CCgiRequest::fDoNotParseContent);

        const CNcbiEnvironment& env = request.GetEnvironment();
        list<string> names;
        env.Enumerate(names);
        NON_CONST_ITERATE(list<string>, it, names) {
            *it += "=" + env.Get(*it);
        }
        vector<const char*> senv;
        NON_CONST_ITERATE(list<string>, it, names) {
            senv.push_back(it->c_str());
        }
        senv.push_back(NULL);
        
        vector<string> args;
        
        CNcbiStrstream err;
        CNcbiStrstream str_in;
        CNcbiIstream* in = request.GetInputStream();
        if ( !in )
            in = &str_in;
        
        int ret = -1;
        bool finished_ok = ExecRemoteApp(m_Params.GetAppPath(), 
                                        args, 
                                        *in, 
                                        context.GetOStream(), 
                                        err,
                                        ret,
                                        context,
                                        m_Params.GetMaxAppRunningTime(),
                                        0,
                                        m_Params.GetKeepAlivePeriod(),
                                        tmp_path,
                                        &senv[0],
                                        m_Params.GetMonitorAppPath(),
                                        m_Params.GetMaxMonitorRunningTime(),
                                        m_Params.GetMonitorPeriod());

        string stat;
        if( !finished_ok ) {
            if (context.GetShutdownLevel() == 
                CNetScheduleAdmin::eShutdownImmidiate) 
                stat = " is canceled.";
        } else {
            if (ret != 0 && m_Params.GetNonZeroExitAction() != 
                           CRemoteAppParams::eDoneOnNonZeroExit) {
                if (m_Params.GetNonZeroExitAction() == CRemoteAppParams::eFailOnNonZeroExit) {
                    context.CommitJobWithFailure("Exited with " 
                                                 + NStr::IntToString(ret) +
                                                 " return code.");
                    stat = " is failed.";
                } else if (m_Params.GetNonZeroExitAction() == 
                           CRemoteAppParams::eReturnOnNonZeroExit)
                    stat = " is returned.";
            } else {
                context.CommitJob();
                stat = " is done.";
            }
        }
        if (context.IsLogRequested()) {
            if (err.pcount() > 0 )
                LOG_POST( CTime(CTime::eCurrent).AsString() << ": " << err.rdbuf());
            LOG_POST( CTime(CTime::eCurrent).AsString() 
                      << ": Job " << context.GetJobKey() + " " + context.GetJobOutput()
                      << stat << " Retcode: " << ret);
        }
        return ret;
    }
private:
    CRemoteAppParams m_Params;
};

CRemoteCgiJob::CRemoteCgiJob(const IWorkerNodeInitContext& context)
{
    const IRegistry& reg = context.GetConfig();
    m_Params.Load("remote_cgi", reg);

    CFile file(m_Params.GetAppPath());
    if (!file.Exists())
        NCBI_THROW(CException, eInvalid, 
                   "File : " + m_Params.GetAppPath() + " doesn't exists.");
    if (!CanExecRemoteApp(file))
        NCBI_THROW(CException, eInvalid, 
                   "Could not execute " + m_Params.GetAppPath() + " file.");
    
}

class CRemoteAppIdleTask : public IWorkerNodeIdleTask
{
public:
    CRemoteAppIdleTask(const IWorkerNodeInitContext& context) 
    {
        const IRegistry& reg = context.GetConfig();
        m_AppCmd = reg.GetString("remote_cgi", "idle_app_cmd", "" );
        if (m_AppCmd.empty())
            throw runtime_error("Idle application is not set.");
    }
    virtual ~CRemoteAppIdleTask() {}

    virtual void Run(CWorkerNodeIdleTaskContext& context)
    {
        if (!m_AppCmd.empty())
            CExec::System(m_AppCmd.c_str());
    }
private:
    string m_AppCmd;
};



/////////////////////////////////////////////////////////////////////////////
//  Routine magic spells

NCBI_WORKERNODE_MAIN_EX(CRemoteCgiJob, CRemoteAppIdleTask, 1.5.0);

//NCBI_WORKERNODE_MAIN(CRemoteAppJob, 1.0.1);
