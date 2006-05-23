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
#include <corelib/ncbi_system.hpp>
#include <corelib/blob_storage.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbiexec.hpp>
#include <corelib/stream_utils.hpp>

#include <connect/ncbi_pipe.hpp>
#include <connect/services/grid_worker_app.hpp>
#include <connect/services/remote_job.hpp>

#include <vector>
#include <algorithm>

USING_NCBI_SCOPE;

    
///////////////////////////////////////////////////////////////////////


static bool s_CanExecute(const CFile& file)
{
    CDirEntry::TMode user_mode  = 0;
    if (!file.GetMode(&user_mode))
        return false;
    if (user_mode & CDirEntry::fExecute)
        return true;
    return false;
}

static bool s_Exec(const string& cmd, 
                   const vector<string>& args,
                   CNcbiIstream& in, CNcbiOstream& out, CNcbiOstream& err,
                   int& exit_value,
                   CWorkerNodeJobContext& context,
                   int max_app_running_time,
                   int app_running_time,
                   int keep_alive_period,
                   const string& tmp_path)
{
    CPipe pipe;
    EIO_Status st = pipe.Open(cmd.c_str(), args, CPipe::fStdErr_Open,tmp_path);
    if (st != eIO_Success)
        NCBI_THROW(CException, eInvalid, 
                   "Could not execute " + cmd + " file.");

    
    auto_ptr<CStopWatch> keep_alive;
    if (keep_alive_period > 0)
        keep_alive.reset(new CStopWatch(CStopWatch::eStart));

    auto_ptr<CStopWatch> running_time;
    if (max_app_running_time > 0 || app_running_time >> 0)
        running_time.reset(new CStopWatch(CStopWatch::eStart));

    bool canceled = false;
    bool out_done = false;
    bool err_done = false;
    bool in_done = false;
    
    const size_t buf_size = 4096;
    char buf[buf_size];
    size_t bytes_in_inbuf = 0;
    size_t total_bytes_written = 0;
    char inbuf[buf_size];

    CPipe::TChildPollMask mask = CPipe::fStdIn | CPipe::fStdOut | CPipe::fStdErr;

    STimeout wait_time = {1, 0};
    while (!out_done || !err_done) {
        EIO_Status rstatus;
        size_t bytes_read;

        CPipe::TChildPollMask rmask = pipe.Poll(mask, &wait_time);
        if (rmask & CPipe::fStdIn && !in_done) {
            if ( in.good() && bytes_in_inbuf == 0) {
                bytes_in_inbuf = CStreamUtils::Readsome(in, inbuf, buf_size);
                total_bytes_written = 0;
            }
        
            size_t bytes_written;
            if (bytes_in_inbuf > 0) {
                rstatus = pipe.Write(inbuf + total_bytes_written, bytes_in_inbuf,
                                     &bytes_written);
                if (rstatus != eIO_Success) {
                    ERR_POST("Not all the data is written to stdin of a child process.");
                    in_done = true;
                }
                total_bytes_written += bytes_written;
                bytes_in_inbuf -= bytes_written;
            }

            if ((!in.good() && bytes_in_inbuf == 0) || in_done) {
                pipe.CloseHandle(CPipe::eStdIn);
                mask &= ~CPipe::fStdIn;
            }

        } if (rmask & CPipe::fStdOut) {
            // read stdout
            if (!out_done) {
                rstatus = pipe.Read(buf, buf_size, &bytes_read);
                out.write(buf, bytes_read);
                if (rstatus != eIO_Success) {
                    out_done = true;
                    mask &= ~CPipe::fStdOut;
            }
        }


        } if (rmask & CPipe::fStdErr) {
            if (!err_done) {
                rstatus = pipe.Read(buf, buf_size, &bytes_read, CPipe::eStdErr);
                err.write(buf, bytes_read);
                if (rstatus != eIO_Success) {
                    err_done = true;
                    mask &= ~CPipe::fStdErr;
                }
            }
        }
        if (!CProcess(pipe.GetProcessHandle()).IsAlive())
            break;

        if (context.GetShutdownLevel() == 
            CNetScheduleClient::eShutdownImmidiate) {
            CProcess(pipe.GetProcessHandle()).Kill();
            canceled = true;
            break;
        }
        if (running_time.get()) {
            double elapsed = running_time->Elapsed();
            if (app_running_time > 0 &&  elapsed > (double)app_running_time) {
                CProcess(pipe.GetProcessHandle()).Kill();
                NCBI_THROW(CException, eUnknown, 
                      "The application's runtime has exceeded a time(" +
                       NStr::UIntToString(app_running_time) +
                      " sec) set by the client");
            }
            if ( max_app_running_time > 0 && elapsed > (double)max_app_running_time) {
                CProcess(pipe.GetProcessHandle()).Kill();
                NCBI_THROW(CException, eUnknown, 
                      "The application's runtime has exceeded a time(" +
                      NStr::UIntToString(max_app_running_time) +
                      " sec) set in the config file");
            }
        }

        if (keep_alive.get() 
            && keep_alive->Elapsed() > (double) keep_alive_period ) {
            context.JobDelayExpiration(keep_alive_period + 10);
            keep_alive->Restart();
        }

    }

    pipe.Close(&exit_value);
    return canceled;
}

/// NetSchedule sample job
///
/// Job reads an array of doubles, sorts it and returns data back
/// to the client as a BLOB.
///

struct STmpDirGuard
{
    STmpDirGuard(const string& path) : m_Path(path) 
    { 
        if (!m_Path.empty()) {
            CDir dir(m_Path); 
            if (!dir.Exists()) 
                dir.CreatePath(); 
        }
    }
    ~STmpDirGuard() { if (!m_Path.empty()) CDir(m_Path).Remove(); }
    const string& m_Path;
};
class CRemoteAppJob : public IWorkerNodeJob
{
public:
    CRemoteAppJob(const IWorkerNodeInitContext& context);

    virtual ~CRemoteAppJob() {} 

    int Do(CWorkerNodeJobContext& context) 
    {
        if (context.IsLogRequested()) {
            LOG_POST( CTime(CTime::eCurrent).AsString() 
                      << ": " << context.GetJobKey() + " " + context.GetJobInput());
        }

        string tmp_path = m_TempDir;
        if (!tmp_path.empty())
            tmp_path += CDirEntry::GetPathSeparator() + context.GetJobKey();

        STmpDirGuard guard(tmp_path);

        m_Request.Receive(context.GetIStream());
        if (m_Request.IsExclusiveModeRequested())
            context.RequestExclusiveMode();

        vector<string> args;
        NStr::Tokenize(m_Request.GetCmdLine(), " ", args);
        
        m_Result.SetStdOutErrFileNames(m_Request.GetStdOutFileName(),
                                       m_Request.GetStdErrFileName(),
                                       m_Request.GetStdOutErrStorageType());
        unsigned int app_running_time = m_Request.GetAppRunTimeout();

            
        int ret = -1;
        bool canceled = s_Exec(m_AppPath, 
                               args, 
                               m_Request.GetStdIn(), 
                               m_Result.GetStdOut(), 
                               m_Result.GetStdErr(),
                               ret,
                               context,
                               m_MaxAppRunningTime,
                               app_running_time,
                               m_KeepAlivePeriod,
                               tmp_path);

        m_Result.SetRetCode(ret); 
        m_Result.Send(context.GetOStream());
        m_Request.Reset();

        string stat = " is canceled.";
        if (!canceled) {
            if (ret != 0 && m_FailOnNonZeroExit ) {
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
                      << ": Job " << context.GetJobKey() + " " + context.GetJobOutput()
                      << stat);
        }
        return ret;
    }
private:

    string m_AppPath;

    CBlobStorageFactory m_Factory;
    CRemoteAppRequest_Executer m_Request;
    CRemoteAppResult_Executer  m_Result;
    int m_MaxAppRunningTime;
    int m_KeepAlivePeriod;
    bool m_FailOnNonZeroExit;
    bool m_RunInSeparateDir;
    string m_TempDir;
};

CRemoteAppJob::CRemoteAppJob(const IWorkerNodeInitContext& context)
    : m_Factory(context.GetConfig()), m_Request(m_Factory), m_Result(m_Factory),
      m_MaxAppRunningTime(0), m_KeepAlivePeriod(0), m_FailOnNonZeroExit(false),
      m_RunInSeparateDir(false)
{
    const IRegistry& reg = context.GetConfig();

    m_MaxAppRunningTime = 
        reg.GetInt("remote_app","max_app_run_time",0,0,IRegistry::eReturn);
    
    m_KeepAlivePeriod = 
        reg.GetInt("remote_app","keep_alive_period",0,0,IRegistry::eReturn);

    m_FailOnNonZeroExit =
        reg.GetBool("remote_app", "fail_on_non_zero_exit", false, 0, 
                    CNcbiRegistry::eReturn);

    m_RunInSeparateDir =
        reg.GetBool("remote_app", "run_in_separate_dir", false, 0, 
                    CNcbiRegistry::eReturn);

    if (m_RunInSeparateDir) {
        m_TempDir = reg.GetString("remote_app", "tmp_path", "." );
        if (!CDirEntry::IsAbsolutePath(m_TempDir)) {
            string tmp = CDir::GetCwd() 
                + CDirEntry::GetPathSeparator() 
                + m_TempDir;
            m_TempDir = CDirEntry::NormalizePath(tmp);
        }
    }

    m_AppPath = reg.GetString("remote_app", "app_path", "" );
    if (!CDirEntry::IsAbsolutePath(m_AppPath)) {
        string tmp = CDir::GetCwd() 
            + CDirEntry::GetPathSeparator() 
            + m_AppPath;
        m_AppPath = CDirEntry::NormalizePath(tmp);
    }

    CFile file(m_AppPath);
    if (!file.Exists())
        NCBI_THROW(CException, eInvalid, 
                   "File : " + m_AppPath + " doesn't exists.");
    if (!s_CanExecute(file))
        NCBI_THROW(CException, eInvalid, 
                   "Could not execute " + m_AppPath + " file.");
    
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

