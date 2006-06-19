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
#include <connect/services/remote_job.hpp>

#include <vector>
#include <algorithm>

#include "exec_helpers.hpp"

USING_NCBI_SCOPE;

    
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

        m_Request.Receive(context.GetIStream());
        if (m_Request.IsExclusiveModeRequested())
            context.RequestExclusiveMode();

        if (context.IsLogRequested()) {
            if (!m_Request.GetInBlobIdOrData().empty())
                LOG_POST( context.GetJobKey()
                          << " Input data: " << m_Request.GetInBlobIdOrData());
            LOG_POST( context.GetJobKey()
                  << " Running : " << m_Params.GetAppPath() << " " << m_Request.GetCmdLine());
        }

        vector<string> args;
        //args.push_back(m_Request.GetCmdLine());
        //NStr::Tokenize(m_Request.GetCmdLine(), " ", args);
        const string& cmdline = m_Request.GetCmdLine();
        if (!cmdline.empty()) {
            string arg;
            for(SIZE_TYPE i = 0; i < cmdline.size(); ) {
                if (cmdline[i] == ' ') {
                    if( !arg.empty() ) {
                        args.push_back(arg);
                        arg.erase();
                    }
                    i++;
                    continue;
                }
                if (cmdline[i] == '\'' || cmdline[i] == '"') {
                    /*                    if( !arg.empty() ) {
                        args.push_back(arg);
                        arg.erase();
                        }*/
                    char quote = cmdline[i];
                    while( ++i < cmdline.size() && cmdline[i] != quote )
                        arg += cmdline[i];

                    args.push_back(arg);
                    arg.erase();
                    ++i;
                    continue;
                }
                arg += cmdline[i++];                
            }
            if( !arg.empty() ) 
                args.push_back(arg);
        }
        
        
        m_Result.SetStdOutErrFileNames(m_Request.GetStdOutFileName(),
                                       m_Request.GetStdErrFileName(),
                                       m_Request.GetStdOutErrStorageType());
        unsigned int app_running_time = m_Request.GetAppRunTimeout();

            
        int ret = -1;
        bool canceled = ExecRemoteApp(m_Params.GetAppPath(), 
                                      args, 
                                      m_Request.GetStdIn(), 
                                      m_Result.GetStdOut(), 
                                      m_Result.GetStdErr(),
                                      ret,
                                      context,
                                      m_Params.GetMaxAppRunningTime(),
                                      app_running_time,
                                      m_Params.GetKeepAlivePeriod(),
                                      tmp_path);

        m_Result.SetRetCode(ret); 
        m_Result.Send(context.GetOStream());

        string stat = " is canceled.";
        if (!canceled) {
            if (ret != 0 && m_Params.FailOnNonZeroExit() ) {
                context.CommitJobWithFailure("Exited with " 
                                             + NStr::IntToString(ret) +
                                             " return code.");
                stat = " is failed.";
            } else {
                context.CommitJob();
                stat = " is done.";
            }
        }
        if (context.IsLogRequested()) {
            LOG_POST( CTime(CTime::eCurrent).AsString() 
                      << ": Job " << context.GetJobKey() << stat << " ExitCode: " << ret);
            if (!m_Request.GetStdOutFileName().empty())
                LOG_POST( context.GetJobKey()
                          << " StdOutFile: " << m_Request.GetStdOutFileName());
            if (!m_Request.GetStdErrFileName().empty())
                LOG_POST( context.GetJobKey()
                          << " StdErrFile: " << m_Request.GetStdErrFileName());
            if (!m_Result.GetOutBlobIdOrData().empty())
                LOG_POST( context.GetJobKey()
                          << " Out data: " << m_Result.GetOutBlobIdOrData());
            if (!m_Result.GetErrBlobIdOrData().empty())
                LOG_POST( context.GetJobKey()
                          << " Err data: " << m_Result.GetErrBlobIdOrData());
        }
        m_Request.Reset();
        m_Result.Reset();
        return ret;
    }
private:

    CBlobStorageFactory m_Factory;
    CRemoteAppRequest_Executer m_Request;
    CRemoteAppResult_Executer  m_Result;
    CRemoteAppParams m_Params;
};

CRemoteAppJob::CRemoteAppJob(const IWorkerNodeInitContext& context)
    : m_Factory(context.GetConfig()), m_Request(m_Factory), m_Result(m_Factory)
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

NCBI_WORKERNODE_MAIN_EX(CRemoteAppJob, CRemoteAppIdleTask, 1.0.0);

//NCBI_WORKERNODE_MAIN(CRemoteAppJob, 1.0.1);



/*
 * ===========================================================================
 * $Log$
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

