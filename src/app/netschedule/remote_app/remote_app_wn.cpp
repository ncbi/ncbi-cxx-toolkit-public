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
        if (context.IsLogRequested()) {
            LOG_POST( CTime(CTime::eCurrent).AsString() 
                      << ": " << context.GetJobKey() << " is received.");
        }

        string tmp_path = m_Params.GetTempDir();
        if (!tmp_path.empty())
            tmp_path += CDirEntry::GetPathSeparator() + context.GetJobKey();

        IRemoteAppRequest_Executer* request = &m_RequestMB;
        IRemoteAppResult_Executer* result = &m_ResultMB;

        bool is_sb = context.GetJobMask() & CRemoteAppRequestSB::kSingleBlobMask;
        if (is_sb) {
            request =  &m_RequestSB;
            result = &m_ResultSB;
        }

        request->Receive(context.GetIStream());

        if (!is_sb) 
            s_SetParam(m_RequestMB, m_ResultMB);

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
                                        NULL,
                                        m_Params.GetMonitorAppPath(),
                                        m_Params.GetMaxMonitorRunningTime(),
                                        m_Params.GetMonitorPeriod());
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
                CNetScheduleClient::eShutdownImmidiate) 
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
            LOG_POST( CTime(CTime::eCurrent).AsString() 
                      << ": Job " << context.GetJobKey() << stat << " ExitCode: " << ret);
            result->Log(context.GetJobKey());
        }
        request->Reset();
        result->Reset();
        return ret;
    }
private:

    static void x_PrepareArgs(const string& cmdline, vector<string>& args);

    CBlobStorageFactory m_Factory;
    CRemoteAppRequestMB_Executer m_RequestMB;
    CRemoteAppResultMB_Executer  m_ResultMB;
    CRemoteAppRequestSB_Executer m_RequestSB;
    CRemoteAppResultSB_Executer  m_ResultSB;
    CRemoteAppParams m_Params;
};

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



/*
 * ===========================================================================
 * $Log$
 * Revision 1.25  2006/09/06 16:58:03  didenko
 * Changed the version number
 *
 * Revision 1.24  2006/09/05 14:35:22  didenko
 * Added option to run a job monitor appliction
 *
 * Revision 1.23  2006/07/13 14:38:54  didenko
 * Added support for running an application using remote_app_dispatcher utility
 *
 * Revision 1.22  2006/06/28 16:01:49  didenko
 * Redone job's exlusivity processing
 *
 * Revision 1.21  2006/06/22 19:33:14  didenko
 * Parameter fail_on_non_zero_exit is replaced with non_zero_exit_action
 *
 * Revision 1.20  2006/06/19 13:26:38  didenko
 * fixed cmdline parsing
 * added logging information
 *
 * Revision 1.19  2006/06/16 16:00:11  didenko
 * fixed command line arguments parsing
 *
 * Revision 1.18  2006/06/15 20:08:31  didenko
 * Added logging of a command line.
 *
 * Revision 1.17  2006/05/30 16:43:36  didenko
 * Moved the commonly used code to separate files.
 *
 * Revision 1.16  2006/05/23 16:59:32  didenko
 * Added ability to run a remote application from a separate directory
 * for each job
 *
 * Revision 1.15  2006/05/22 18:11:43  didenko
 * Added an option to fail a job if a remote app returns non zore code
 *
 * Revision 1.14  2006/05/19 13:40:40  didenko
 * Added ns_remote_job_control utility
 *
 * Revision 1.13  2006/05/15 15:26:53  didenko
 * Added support for running exclusive jobs
 *
 * Revision 1.12  2006/05/11 14:32:43  didenko
 * Cosmetics
 *
 * Revision 1.11  2006/05/10 19:54:21  didenko
 * Added JobDelayExpiration method to CWorkerNodeContext class
 * Added keep_alive_period and max_job_run_time parmerter to the config
 * file of remote_app
 *
 * Revision 1.10  2006/05/08 15:16:42  didenko
 * Added support for an optional saving of a remote application's stdout
 * and stderr into files on a local file system
 *
 * Revision 1.9  2006/05/03 14:54:02  didenko
 * <util/stream_util.hpp> is replaced with <corelib/stream_util.hpp>
 *
 * Revision 1.8  2006/03/30 16:13:18  didenko
 * + optional jobs logging
 *
 * Revision 1.7  2006/03/16 15:14:41  didenko
 * Renamed CRemoteJob... to CRemoteApp...
 *
 * Revision 1.6  2006/03/15 17:42:01  didenko
 * Performance improvement
 *
 * Revision 1.5  2006/03/10 15:21:07  ucko
 * Don't (implicitly) inline CRemoteJobApp's constructor, as WorkShop 5.3
 * then refuses to let it call s_CanExecute.
 *
 * Revision 1.4  2006/03/09 14:56:43  didenko
 * Added status check after writing into the pipe
 *
 * Revision 1.3  2006/03/09 14:24:33  didenko
 * Fixed logic of event handling
 *
 * Revision 1.2  2006/03/07 18:00:18  didenko
 * Spelling fix
 *
 * Revision 1.1  2006/03/07 17:39:59  didenko
 * Added a workernode for running external applications
 *
 * ===========================================================================
 */

