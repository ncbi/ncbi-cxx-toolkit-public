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
 * Authors:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:  NetSchedule worker node sample
 *
 */

#include <ncbi_pch.hpp>

#include "exec_helpers.hpp"

#include <connect/services/grid_worker.hpp>

#include <connect/ncbi_pipe.hpp>

#include <corelib/rwstream.hpp>
#include <corelib/request_ctx.hpp>

#if defined(NCBI_OS_UNIX)
#include <fcntl.h>
#endif

BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////////////////
///
CRemoteAppLauncher::CRemoteAppLauncher(const string& sec_name,
        const IRegistry& reg)
    : m_MaxAppRunningTime(0), m_KeepAlivePeriod(0), 
      m_NonZeroExitAction(eDoneOnNonZeroExit),
      m_RemoveTempDir(true),
      m_CacheStdOutErr(true)
{
    m_MaxAppRunningTime = 
        reg.GetInt(sec_name,"max_app_run_time",0,0,IRegistry::eReturn);
    
    m_KeepAlivePeriod = 
        reg.GetInt(sec_name,"keep_alive_period",0,0,IRegistry::eReturn);

    if (reg.HasEntry(sec_name, "non_zero_exit_action") ) {
        string val = reg.GetString(sec_name, "non_zero_exit_action", "");
        if (NStr::CompareNocase(val, "fail") == 0 )
            m_NonZeroExitAction = eFailOnNonZeroExit;
        else if (NStr::CompareNocase(val, "return") == 0 )
            m_NonZeroExitAction = eReturnOnNonZeroExit;
        else if (NStr::CompareNocase(val, "done") == 0 )
            m_NonZeroExitAction = eDoneOnNonZeroExit;
        else {
           ERR_POST(Warning << "Unknown parameter value : Section [" 
                     << sec_name << "], param : \"non_zero_exit_action\", value : \""
                     << val << "\". Allowed values: fail, return, done"); 
        }

    } else {
        if (reg.HasEntry(sec_name, "fail_on_non_zero_exit") ) {
            if (reg.GetBool(sec_name, "fail_on_non_zero_exit", false, 0, 
                            CNcbiRegistry::eReturn) )
                m_NonZeroExitAction = eFailOnNonZeroExit;
        }
    }

    if (reg.GetBool(sec_name, "run_in_separate_dir", false, 0,
            CNcbiRegistry::eReturn)) {
        if (reg.HasEntry(sec_name, "tmp_dir")) 
            m_TempDir = reg.GetString(sec_name, "tmp_dir", "." );
        else 
            m_TempDir = reg.GetString(sec_name, "tmp_path", "." );
        
        if (!CDirEntry::IsAbsolutePath(m_TempDir)) {
            string tmp = CDir::GetCwd() 
                + CDirEntry::GetPathSeparator() 
                + m_TempDir;
            m_TempDir = CDirEntry::NormalizePath(tmp);
        }
        if (reg.HasEntry(sec_name, "remove_tmp_dir")) 
            m_RemoveTempDir = reg.GetBool(sec_name, "remove_tmp_dir", true, 0, 
                                    CNcbiRegistry::eReturn);
        else
            m_RemoveTempDir = reg.GetBool(sec_name, "remove_tmp_path", true, 0, 
                                    CNcbiRegistry::eReturn);
        m_CacheStdOutErr = reg.GetBool(sec_name, "cache_std_out_err", true, 0, 
                                    CNcbiRegistry::eReturn);
    }

    m_AppPath = reg.GetString(sec_name, "app_path", kEmptyStr);
    if (m_AppPath.empty()) {
        NCBI_THROW_FMT(CConfigException, eParameterMissing,
                "Missing configuration parameter [" << sec_name <<
                "].app_path");
    }
    if (!CDirEntry::IsAbsolutePath(m_AppPath)) {
        string tmp = CDir::GetCwd() 
            + CDirEntry::GetPathSeparator() 
            + m_AppPath;
        m_AppPath = CDirEntry::NormalizePath(tmp);
    }

    m_MonitorAppPath = reg.GetString(sec_name, "monitor_app_path", kEmptyStr);
    if (!m_MonitorAppPath.empty()) {
        if (!CDirEntry::IsAbsolutePath(m_MonitorAppPath)) {
            string tmp = CDir::GetCwd() 
                + CDirEntry::GetPathSeparator() 
                + m_MonitorAppPath;
            m_MonitorAppPath = CDirEntry::NormalizePath(tmp);
        }
        CFile f(m_MonitorAppPath);
        if (!f.Exists() || !CanExecRemoteApp(f)) {
            ERR_POST("Can not execute \"" << m_MonitorAppPath 
                     << "\". The Monitor application will not run!");
            m_MonitorAppPath = kEmptyStr;
        }
    }

    m_MaxMonitorRunningTime = reg.GetInt(sec_name,
            "max_monitor_running_time", 5, 0, IRegistry::eReturn);

    m_MonitorPeriod = reg.GetInt(sec_name,
            "monitor_period", 5, 0, IRegistry::eReturn);

    m_KillTimeout = reg.GetInt(sec_name,
            "kill_timeout", 1, 0, IRegistry::eReturn);

    m_ExcludeEnv.clear();
    m_IncludeEnv.clear();
    m_AddedEnv.clear();

    NStr::Split(reg.GetString("env_inherit", "exclude", "")," ;,", m_ExcludeEnv);
    NStr::Split(reg.GetString("env_inherit", "include", "")," ;,", m_IncludeEnv);

    list<string> added_env;
    reg.EnumerateEntries("env_set", &added_env);

    ITERATE(list<string>, it, added_env) {
        const string& s = *it;
         m_AddedEnv[s] = reg.GetString("env_set", s, "");
    }
}

//////////////////////////////////////////////////////////////////////////////
///

bool CanExecRemoteApp(const CFile& file)
{
    CDirEntry::TMode user_mode  = 0;
    if (!file.GetMode(&user_mode))
        return false;
    if (user_mode & CDirEntry::fExecute)
        return true;
    return false;
}

//////////////////////////////////////////////////////////////////////////////
///
struct STmpDirGuard
{
    STmpDirGuard(const string& path, bool remove_path) 
        : m_Path(path), m_RemovePath(remove_path)
    { 
        if (!m_Path.empty()) {
            CDir dir(m_Path); 
            if (!dir.Exists()) 
                dir.CreatePath(); 
        }
    }
    ~STmpDirGuard() 
    { 
        if (m_RemovePath && !m_Path.empty()) {
            try {
                if (!CDir(m_Path).Remove(CDirEntry::eRecursiveIgnoreMissing)) {
                   ERR_POST("Could not delete temp directory \"" << m_Path <<"\"");
                }
                //cerr << "Deleted " << m_Path << endl;
            } catch (exception& ex) {
                ERR_POST("Error during tmp directory deletion\"" << m_Path <<"\": " << ex.what());
            }  catch (...) {
                ERR_POST("Error during tmp directory deletion\"" << m_Path <<"\": Unknown error");
            }
        }
    }
    string m_Path;
    bool m_RemovePath;
};
//////////////////////////////////////////////////////////////////////////////
///
class CPipeProcessWatcher_Base : public CPipe::IProcessWatcher
{
public:
    CPipeProcessWatcher_Base(int max_app_running_time)
        : m_MaxAppRunningTime(max_app_running_time)
    {
        if (max_app_running_time > 0)
            m_RunningTime.reset(new CStopWatch(CStopWatch::eStart));
    }
    
    virtual CPipe::IProcessWatcher::EAction Watch(TProcessHandle /*pid*/) 
    {
        if (m_MaxAppRunningTime > 0 && m_RunningTime->Elapsed() >
                (double) m_MaxAppRunningTime) {
            ERR_POST("Job run time exceeded "
                     << m_MaxAppRunningTime
                     <<" seconds, stopping the child");
            return CPipe::IProcessWatcher::eStop;
        }

        return CPipe::IProcessWatcher::eContinue;
    }
private:
    auto_ptr<CStopWatch> m_RunningTime;
    int m_MaxAppRunningTime;
};

//////////////////////////////////////////////////////////////////////////////
///
class CRAMonitor
{
public:
    CRAMonitor(const string& app, const char* const* env,
        int max_app_running_time) :
        m_App(app),
        m_Env(env),
        m_MaxAppRunningTime(max_app_running_time)
    {
    }

    int Run(vector<string>& args, CNcbiOstream& out, CNcbiOstream& err)
    {
        CPipeProcessWatcher_Base callback(m_MaxAppRunningTime);
        CNcbiStrstream in;
        int exit_value;
        CPipe::EFinish ret = CPipe::eCanceled;
        try {
            ret = CPipe::ExecWait(m_App, args, in, 
                                  out, err, exit_value, 
                                  kEmptyStr, m_Env,
                                  &callback);
        }
        catch (exception& ex) {
            err << ex.what();
        }
        catch (...) {
            err << "Unknown error";
        }
        if (ret != CPipe::eDone || exit_value > 2)
            return 3;
        return exit_value;
    }

private:
    string m_App;
    const char* const* m_Env;
    int m_MaxAppRunningTime;
};

//////////////////////////////////////////////////////////////////////////////
///
class CPipeProcessWatcher : public CPipeProcessWatcher_Base
{
public:
    CPipeProcessWatcher(CWorkerNodeJobContext& job_context,
                   int max_app_running_time,
                   int keep_alive_period,
                   const string& job_wdir)
        : CPipeProcessWatcher_Base(max_app_running_time),
          m_JobContext(job_context), m_KeepAlivePeriod(keep_alive_period),
          m_Monitor(NULL), m_JobWDir(job_wdir)
    {
        if (m_KeepAlivePeriod > 0)
            m_KeepAlive.reset(new CStopWatch(CStopWatch::eStart));
    }

    void SetMonitor(CRAMonitor& monitor, int monitor_perod)
    {
        m_Monitor = &monitor;
        m_MonitorPeriod = monitor_perod;
        if (m_MonitorPeriod)
            m_MonitorWatch.reset(new CStopWatch(CStopWatch::eStart));
    }

    virtual EAction OnStart(TProcessHandle pid)
    {
        if (m_JobContext.GetShutdownLevel() ==
            CNetScheduleAdmin::eShutdownImmediate) {
            return CPipe::IProcessWatcher::eStop;
        }

        LOG_POST("Child PID: " << NStr::UInt8ToString((Uint8) pid));

        return CPipeProcessWatcher_Base::OnStart(pid);
    }

    virtual CPipe::IProcessWatcher::EAction Watch(TProcessHandle pid)
    {
        if (m_JobContext.GetShutdownLevel() ==
                CNetScheduleAdmin::eShutdownImmediate) {
            m_JobContext.ReturnJob();
            return CPipe::IProcessWatcher::eStop;
        }

        CPipe::IProcessWatcher::EAction action =
                CPipeProcessWatcher_Base::Watch(pid);

        if (action != CPipe::IProcessWatcher::eContinue)
            return action;

        if (m_KeepAlive.get() &&
                m_KeepAlive->Elapsed() > (double) m_KeepAlivePeriod) {
            m_JobContext.JobDelayExpiration(m_KeepAlivePeriod + 10);
            m_KeepAlive->Restart();
        }
        if (m_Monitor && m_MonitorWatch.get() &&
                m_MonitorWatch->Elapsed() > (double) m_MonitorPeriod) {
            CNcbiStrstream out;
            CNcbiStrstream err;
            vector<string> args;
            args.push_back("-pid");
            args.push_back(NStr::UInt8ToString((Uint8) pid));
            args.push_back("-jid");
            args.push_back(m_JobContext.GetJobKey());
            args.push_back("-jwdir");
            args.push_back(m_JobWDir);

            switch (m_Monitor->Run(args, out, err)) {
            case 0:
                {
                    bool non_empty_output = out.pcount() > 0;
                    if (non_empty_output) {
                        out << NcbiEnds;
                        m_JobContext.PutProgressMessage(out.str(), true);
                    }
                    if (m_JobContext.IsLogRequested() &&
                            (!non_empty_output || err.pcount() > 0))
                        x_Log("exited with zero return code", err);
                }
                break;
            case 1:
                m_JobContext.ReturnJob();
                x_Log("job is returned", err);
                return CPipe::IProcessWatcher::eStop;
            case 2:
                {
                    x_Log("job failed", err);
                    string errmsg;
                    if (out.pcount() > 0) {
                        out << '\0';
                        errmsg = out.str();
                    } else
                        errmsg = "Monitor requested job termination";
                    throw runtime_error(errmsg);
                }
                break;
            default:
                x_Log("internal error", err);
                break;
            }
            m_MonitorWatch->Restart();
        }

        return CPipe::IProcessWatcher::eContinue;
    }

private:
    inline void x_Log(const string& what, CNcbiStrstream& sstream)
    {
        if (sstream.pcount() > 0) {
            LOG_POST(m_JobContext.GetJobKey() << " (monitor) " << what <<
                    ": " << sstream.rdbuf());
        } else {
            LOG_POST(m_JobContext.GetJobKey() << " (monitor) " << what << ".");
        }
    }

    CWorkerNodeJobContext& m_JobContext;
    int m_KeepAlivePeriod;
    auto_ptr<CStopWatch> m_KeepAlive;
    CRAMonitor* m_Monitor;
    auto_ptr<CStopWatch> m_MonitorWatch;
    int m_MonitorPeriod;
    string m_JobWDir;
};


//////////////////////////////////////////////////////////////////////////////
///

class CTmpStreamGuard
{
public:
    CTmpStreamGuard(const string& tmp_dir,
        const string& name,
        CNcbiOstream& orig_stream,
        bool cache_std_out_err) : m_OrigStream(orig_stream), m_Stream(NULL)
    {
        if (!tmp_dir.empty() && cache_std_out_err) {
            m_Name = tmp_dir + CDirEntry::GetPathSeparator();
            m_Name += name;
        }
        if (!m_Name.empty()) {
            try {
               m_ReaderWriter.reset(new CFileReaderWriter(m_Name,
                   CFileIO_Base::eCreate));
            } catch (CFileException& ex) {
                ERR_POST("Could not create a temporary file " <<
                    m_Name << " :" << ex.what() << " the data will be "
                    "written directly to the original stream");
                m_Name.erase();
                m_Stream = &m_OrigStream;
                return;
            }
#if defined(NCBI_OS_UNIX)
            // If the file is created on an NFS file system, the CLOEXEC
            // flag needs to be set, otherwise deleting the temporary
            // directory will not succeed.
            TFileHandle fd = m_ReaderWriter->GetFileIO().GetFileHandle();
            fcntl(fd, F_SETFD, fcntl(fd, F_GETFD, 0) | FD_CLOEXEC);
#endif            
            m_StreamGuard.reset(new CWStream(m_ReaderWriter.get()));
            m_Stream = m_StreamGuard.get();
        } else {
            m_Stream = &m_OrigStream;
        }
    }
    ~CTmpStreamGuard()
    {
        try {
            Close();
        } catch(exception& ex) {
            ERR_POST( "CTmpStreamGuard::~CTmpStreamGuard(): " <<
                m_Name << " --> " << ex.what());
        } catch(...) {
            ERR_POST( "CTmpStreamGuard::~CTmpStreamGuard(): " <<
                m_Name << " --> Unknown error.");
        }
    }

    CNcbiOstream& GetOStream() { return *m_Stream; }

    void Close()
    {
        if (!m_Name.empty() && m_StreamGuard.get()) {
            m_StreamGuard.reset();
            m_ReaderWriter->Flush();
            m_ReaderWriter->GetFileIO().SetFilePos(0, CFileIO_Base::eBegin);
            {
            CRStream rstm(m_ReaderWriter.get());
            if (!rstm.good() 
                || !NcbiStreamCopy(m_OrigStream, rstm)) 
                ERR_POST( "Cannot copy \"" << m_Name << "\" file.");
            }
            m_ReaderWriter.reset();
        }
    }

private:
    CNcbiOstream& m_OrigStream;
    auto_ptr<CFileReaderWriter> m_ReaderWriter;
    auto_ptr<CNcbiOstream> m_StreamGuard;
    CNcbiOstream* m_Stream;
    string m_Name;
};


bool CRemoteAppLauncher::ExecRemoteApp(const vector<string>& args,
    CNcbiIstream& in, CNcbiOstream& out, CNcbiOstream& err,
    int& exit_value,
    CWorkerNodeJobContext& job_context,
    int app_run_timeout,
    const char* const env[]) const
{
    string tmp_path = m_TempDir;
    if (!tmp_path.empty()) {
        CFastLocalTime lt;
        bool substitution_found = false;
        size_t subst_pos;
        while ((subst_pos = tmp_path.find('%')) != string::npos) {
            if (subst_pos + 1 >= tmp_path.length())
                break;
            switch (tmp_path[subst_pos + 1]) {
            case '%':
                tmp_path.replace(subst_pos, 2, 1, '%');
                continue;
            case 'q':
                tmp_path.replace(subst_pos, 2, job_context.GetQueueName());
                break;
            case 'j':
                tmp_path.replace(subst_pos, 2, job_context.GetJobKey());
                break;
            case 'r':
                tmp_path.replace(subst_pos, 2, NStr::UInt8ToString(
                    GetDiagContext().GetRequestContext().GetRequestID()));
                break;
            case 't':
                tmp_path.replace(subst_pos, 2, NStr::UIntToString(
                    (unsigned) lt.GetLocalTime().GetTimeT()));
                break;
            default:
                tmp_path.erase(subst_pos, 2);
            }
            substitution_found = true;
        }
        if (!substitution_found)
            tmp_path += CDirEntry::GetPathSeparator() +
                job_context.GetQueueName() + "_"  +
                job_context.GetJobKey() + "_" +
                NStr::UIntToString((unsigned) lt.GetLocalTime().GetTimeT());
    }

    STmpDirGuard guard(tmp_path, m_RemoveTempDir);
    {
        CTmpStreamGuard std_out_guard(tmp_path, "std.out", out,
            m_CacheStdOutErr);
        CTmpStreamGuard std_err_guard(tmp_path, "std.err", err,
            m_CacheStdOutErr);

        int max_app_run_time = m_MaxAppRunningTime;

        if (app_run_timeout > 0 &&
            (max_app_run_time == 0 || max_app_run_time > app_run_timeout))
            max_app_run_time = app_run_timeout;

        string working_dir(tmp_path.empty() ? CDir::GetCwd() : tmp_path);

#ifdef NCBI_OS_MSWIN
        NStr::ReplaceInPlace(working_dir, "\\", "/");
#endif

        CPipeProcessWatcher callback(job_context,
            max_app_run_time,
            m_KeepAlivePeriod,
            working_dir);

        auto_ptr<CRAMonitor> ra_monitor;
        if (!m_MonitorAppPath.empty() && m_MonitorPeriod > 0) {
            ra_monitor.reset(new CRAMonitor(m_MonitorAppPath, env,
                m_MaxMonitorRunningTime));
            callback.SetMonitor(*ra_monitor, m_MonitorPeriod);
        }

        STimeout kill_tm = {m_KillTimeout, 0};

        return CPipe::ExecWait(GetAppPath(), args, in,
                               std_out_guard.GetOStream(),
                               std_err_guard.GetOStream(),
                               exit_value, 
                               tmp_path, env, &callback,
                               &kill_tm) == CPipe::eDone;
    }
}


END_NCBI_SCOPE
