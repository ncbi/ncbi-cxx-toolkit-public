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
#include <connect/services/remote_app_mb.hpp>
#include <connect/services/remote_app_sb.hpp>

#include <vector>
#include <algorithm>

#include "exec_helpers.hpp"

USING_NCBI_SCOPE;


static void s_SetParam(const CRemoteAppRequestMB_Executer& request,
                       CRemoteAppResultMB_Executer& result)
{
    result.SetStdOutErrFileNames(request.GetStdOutFileName(),
                                 request.GetStdErrFileName(),
                                 request.GetStdOutErrStorageType());
}

///////////////////////////////////////////////////////////////////////

/// NetSchedule sample job
///
/// Job reads an array of doubles, sorts it and returns data back
/// to the client as a BLOB.
///
class CRemoteAppJob : public IWorkerNodeJob
{
public:
    CRemoteAppJob(const IWorkerNodeInitContext& context);

    virtual ~CRemoteAppJob() {} 

    int Do(CWorkerNodeJobContext& context) 
    {
        CFastLocalTime lt;
        if (context.IsLogRequested()) {
            LOG_POST( lt.GetLocalTime().AsString() 
                      << ": " << context.GetJobKey() << " is received.");
        }

        string tmp_path = m_Params.GetTempDir();
        if (!tmp_path.empty())
            tmp_path += CDirEntry::GetPathSeparator() + 
                context.GetQueueName() + "_"  + context.GetJobKey() + "_" +
                NStr::UIntToString((unsigned int)lt.GetLocalTime().GetTimeT());

        IRemoteAppRequest_Executer* request = &m_RequestMB;
        IRemoteAppResult_Executer* result = &m_ResultMB;

        bool is_sb = context.GetJobMask() & CRemoteAppRequestSB::kSingleBlobMask;
        if (is_sb) {
            request =  &m_RequestSB;
            result = &m_ResultSB;
        }

        request->Receive(context.GetIStream());

        if (!is_sb) {
            s_SetParam(m_RequestMB, m_ResultMB);
            size_t output_size = context.GetMaxServerOutputSize();
            if (output_size == 0) {
                // this means that NS internal storage is not supported and 
                // we all input params will be save into NC anyway. So we are 
                // putting all input into one blob.
                output_size = kMax_UInt;
            } else {
                // here we need some empiric to calculate this size
                // for now just reduce it by 10%
                output_size = output_size - output_size / 10;
            }
            m_ResultMB.SetMaxOutputSize(output_size);
        }

        if (context.IsLogRequested()) {
            request->Log(context.GetJobKey());
        }

        vector<string> args;
        TokenizeCmdLine(request->GetCmdLine(), args);
        
          
        int ret = -1;
        bool finished_ok = false;
        try {
            finished_ok = ExecRemoteApp(m_Params.GetAppPath(), 
                                        args, 
                                        request->GetStdIn(), 
                                        result->GetStdOut(), 
                                        result->GetStdErr(),
                                        ret,
                                        context,
                                        m_Params.GetMaxAppRunningTime(),
                                        request->GetAppRunTimeout(),
                                        m_Params.GetKeepAlivePeriod(),
                                        tmp_path,
                                        x_GetEnv(),
                                        m_Params.GetMonitorAppPath(),
                                        m_Params.GetMaxMonitorRunningTime(),
                                        m_Params.GetMonitorPeriod(),
                                        m_Params.GetKillTimeout());
        } catch (...) {
            request->Reset();
            result->Reset();
            throw;
        }

        result->SetRetCode(ret); 
        result->Send(context.GetOStream());

        string stat;
        if( !finished_ok ) {
            if (context.GetShutdownLevel() == 
                CNetScheduleAdmin::eShutdownImmediate) 
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
            LOG_POST( lt.GetLocalTime().AsString() 
                      << ": Job " << context.GetJobKey() << stat << " ExitCode: " << ret);
            result->Log(context.GetJobKey());
        }
        request->Reset();
        result->Reset();
        return ret;
    }
private:

    static void x_PrepareArgs(const string& cmdline, vector<string>& args);

    const char* const* x_GetEnv();

    CBlobStorageFactory m_Factory;
    CRemoteAppRequestMB_Executer m_RequestMB;
    CRemoteAppResultMB_Executer  m_ResultMB;
    CRemoteAppRequestSB_Executer m_RequestSB;
    CRemoteAppResultSB_Executer  m_ResultSB;
    CRemoteAppParams m_Params;

    list<string> m_EnvValues;
    vector<const char*> m_Env;
};

const char* const* CRemoteAppJob:: x_GetEnv()
{
    if( !m_Env.empty() )
        return &m_Env[0];

    typedef map<string,string> TMapStr;
    const TMapStr& added_env = m_Params.GetAddedEnv();
    ITERATE(TMapStr, it, added_env) {
        m_EnvValues.push_back(it->first + "=" +it->second);
    }
    list<string> names;
    const CNcbiEnvironment& env = m_Params.GetLocalEnv();
    env.Enumerate(names);
    ITERATE(list<string>, it, names) {
        if (added_env.find(*it) == added_env.end())
            m_EnvValues.push_back(*it + "=" + env.Get(*it));
    }
    ITERATE(list<string>, it, m_EnvValues) {
        m_Env.push_back(it->c_str());
    }
    m_Env.push_back(NULL);
    return &m_Env[0];
}

CRemoteAppJob::CRemoteAppJob(const IWorkerNodeInitContext& context)
    : m_Factory(context.GetConfig()), 
      m_RequestMB(m_Factory), m_ResultMB(m_Factory),
      m_RequestSB(m_Factory), m_ResultSB(m_Factory)
{
    const IRegistry& reg = context.GetConfig();
    m_Params.Load("remote_app", reg);

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
        m_AppCmd = reg.GetString("remote_app", "idle_app_cmd", "" );
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

NCBI_WORKERNODE_MAIN_EX(CRemoteAppJob, CRemoteAppIdleTask, 1.5.0);

//NCBI_WORKERNODE_MAIN(CRemoteAppJob, 1.0.1);
