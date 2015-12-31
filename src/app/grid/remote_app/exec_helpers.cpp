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

#include <sstream>

#include <connect/ncbi_pipe.hpp>

#include <corelib/rwstream.hpp>
#include <corelib/request_ctx.hpp>

#if defined(NCBI_OS_UNIX)
#include <fcntl.h>
#endif

#include "exec_helpers.hpp"

#define PIPE_SIZE 64 * 1024

BEGIN_NCBI_SCOPE

// This class is responsibe for the whole process of reaping child processes
class CRemoteAppReaper
{
    // Context holds all data that is shared between Manager and Collector.
    // Also, it actually implements both these actors
    class CContext
    {
    public:
        CContext(int s, int m)
            : sleep(s > 1 ? s : 1), // Sleep at least one second between reaps
              max_attempts(m),
              stop(false)
        {}

        bool Enabled() const { return max_attempts > 0; }

        CPipe::IProcessWatcher::EAction ManagerImpl(TProcessHandle);
        void CollectorImpl();

        void CollectorImplStop()
        {
            CMutexGuard guard(lock);
            stop = true;
            cond.SignalSome();
        }

    private:
        // A record for a child process
        struct SChild
        {
            CProcess process;
            int attempts;

            SChild(TProcessHandle handle) : process(handle), attempts(0) {}
        };

        typedef list<SChild> TChildren;
        typedef TChildren::iterator TChildren_I;

        bool FillBacklog(TChildren_I&);

        CMutex lock;
        CConditionVariable cond;
        TChildren children;
        TChildren backlog;
        const unsigned sleep;
        const int max_attempts;
        bool stop;
    };

    // Collector (thread) waits/kills child processes
    class CCollector : public CThread
    {
    public:
        CCollector(CContext& context, const string& app_name)
            : m_Context(context),
              m_ThreadName(app_name + "_cl")
        {}

        void Start()
        {
            if (m_Context.Enabled()) {
                Run();
            }
        }

        ~CCollector()
        {
            if (m_Context.Enabled())
                try {
                    m_Context.CollectorImplStop();
                    Join();
                } STD_CATCH_ALL("Exception in ~CCollector()")
        }

    private:
        // This is the only method called in a different thread
        void* Main(void)
        {
            SetCurrentThreadName(m_ThreadName);
            m_Context.CollectorImpl();
            return NULL;
        }

        CContext& m_Context;
        const string m_ThreadName;
    };

public:
    // Manager gives work (a pid/handle of a child process) to Collector
    class CManager
    {
    public:
        CPipe::IProcessWatcher::EAction operator()(TProcessHandle handle)
        {
            return m_Context.ManagerImpl(handle);
        }

    private:
        CManager(CContext& context) : m_Context(context) {}

        CContext& m_Context;

        friend class CRemoteAppReaper;
    };

    CRemoteAppReaper(int sleep, int max_attempts, const string& app_name);

    CManager& GetManager() { return m_Manager; }
    void StartCollector() { m_Collector->Start(); }

private:
    CContext m_Context;
    CManager m_Manager;
    auto_ptr<CCollector> m_Collector;
};

CPipe::IProcessWatcher::EAction CRemoteAppReaper::CContext::ManagerImpl(
        TProcessHandle handle)
{
    if (Enabled()) {
        CMutexGuard guard(lock);
        children.push_back(handle);
        cond.SignalSome();
        return CPipe::IProcessWatcher::eExit;
    }

    return CPipe::IProcessWatcher::eStop;
}

void CRemoteAppReaper::CContext::CollectorImpl()
{
    CProcess::CExitInfo exitinfo;

    for (;;) {
        TChildren_I backlog_end = backlog.end();

        // If stop was requested
        if (!FillBacklog(backlog_end)) {
            return;
        }

        // Wait/kill child processes from the backlog
        TChildren_I it = backlog.begin();
        while (it != backlog_end) {
            bool done = it->process.Wait(0, &exitinfo) != -1 ||
                exitinfo.IsExited() || exitinfo.IsSignaled();

            if (done) {
                // Log a message for those that had failed to be killed
                if (it->attempts) {
                    LOG_POST(Note << "Successfully waited for a process: " <<
                            it->process.GetHandle());
                }
            } else if (it->attempts++) {
                // Give up if there are too many attempts to wait for a process
                if (it->attempts > max_attempts) {
                    done = true;
                    ERR_POST("Give up waiting for a process: " <<
                            it->process.GetHandle());
                }
            } else if (it->process.KillGroup()) {
                done = true;
            } else {
                LOG_POST(Warning << "Failed to kill a process: " <<
                        it->process.GetHandle() << ", will wait for it");
            }

            if (done) {
                backlog.erase(it++);
            } else {
                ++it;
            }
        }
    }
}

bool CRemoteAppReaper::CContext::FillBacklog(TChildren_I& backlog_end)
{
    CMutexGuard guard(lock);

    while (!stop) {
        // If there are some new child processes to wait for
        if (!children.empty()) {
            // Move them to the backlog, these only will be processed this time
            backlog_end = backlog.begin();
            backlog.splice(backlog_end, children);
            return true;

        // If there is nothing to do, wait for a signal
        } else if (backlog.empty()) {
            while (!cond.WaitForSignal(lock));

        // No backlog processing if there is a signal
        } else if (!cond.WaitForSignal(lock, sleep)) {
            return true;
        }
    }

    return false;
}

CRemoteAppReaper::CRemoteAppReaper(int sleep, int max_attempts,
        const string& app_name)
    : m_Context(sleep, max_attempts),
      m_Manager(m_Context),
      m_Collector(new CCollector(m_Context, app_name))
{
}

// This class is responsible for reporting app/cgi version run by this app
class CRemoteAppVersion
{
public:
    CRemoteAppVersion(const string& app, const vector<string>& args)
        : m_App(app), m_Args(args)
    {}

    string Get(const string& v) const;

private:
    const string m_App;
    const vector<string> m_Args;
};

string CRemoteAppVersion::Get(const string& v) const
{
    struct WaitOneSecond : CPipe::IProcessWatcher
    {
        const CDeadline deadline;
        WaitOneSecond() : deadline(1) {}
        EAction Watch(TProcessHandle)
        {
            return deadline.IsExpired() ? eStop : eContinue;
        }
    } wait_one_second;

    istringstream in;
    ostringstream out;
    ostringstream err;
    int exit_code;

    if (CPipe::ExecWait(m_App, m_Args, in, out, err, exit_code,
                kEmptyStr, 0, &wait_one_second) == CPipe::eDone) {
        // Restrict version string to 1024 chars
        string app_ver(out.str(), 0, 1024);
        return NStr::Sanitize(app_ver) + " / " + v;
    }

    return v;
}

class CTimer
{
public:
    CTimer(const CTimeout& timeout) :
        m_Deadline(timeout),
        m_Timeout(timeout)
    {}

    void Restart() { m_Deadline = m_Timeout; }
    bool IsExpired() const { return m_Deadline.IsExpired(); }
    unsigned PresetSeconds() const { return (unsigned)m_Timeout.GetAsDouble(); }

private:
    CDeadline m_Deadline;
    CTimeout m_Timeout;
};

CTimeout s_ToTimeout(unsigned sec)
{
    // Zero counts as infinite timeout
    return sec ? CTimeout(sec, 0) : CTimeout::eInfinite;
}

struct SSection
{
    const IRegistry& reg;
    const string& name;

    SSection(const IRegistry& r, const string& n) : reg(r), name(n) {}

    int Get(const string& param, int def) const
    {
        return reg.GetInt(name, param, def, 0, IRegistry::eReturn);
    }

    bool Get(const string& param, bool def) const
    {
        return reg.GetBool(name, param, def, 0, IRegistry::eReturn);
    }
};

//////////////////////////////////////////////////////////////////////////////
///
CRemoteAppLauncher::CRemoteAppLauncher(const string& sec_name,
        const IRegistry& reg) :
      m_NonZeroExitAction(eDoneOnNonZeroExit),
      m_RemoveTempDir(true),
      m_CacheStdOutErr(true)
{
    const SSection sec(reg, sec_name);

    m_AppRunTimeout = s_ToTimeout(sec.Get("max_app_run_time", 0));
    m_KeepAlivePeriod = s_ToTimeout(sec.Get("keep_alive_period", 0));

    if (reg.HasEntry(sec_name, "non_zero_exit_action") ) {
        string val = reg.GetString(sec_name, "non_zero_exit_action", "");
        if (NStr::CompareNocase(val, "fail") == 0 )
            m_NonZeroExitAction = eFailOnNonZeroExit;
        else if (NStr::CompareNocase(val, "return") == 0 )
            m_NonZeroExitAction = eReturnOnNonZeroExit;
        else if (NStr::CompareNocase(val, "done") == 0 )
            m_NonZeroExitAction = eDoneOnNonZeroExit;
        else {
           ERR_POST("Unknown parameter value: "
                   "section [" << sec_name << "], "
                   "parameter \"non_zero_exit_action\", "
                   "value: \"" << val << "\". "
                   "Allowed values: fail, return, done");
        }
    } else if (sec.Get("fail_on_non_zero_exit", false))
        m_NonZeroExitAction = eFailOnNonZeroExit;

    if (reg.HasEntry(sec_name, "fail_no_retries_if_exit_code")) {
        m_RangeList.Parse(reg.GetString(sec_name,
                    "fail_no_retries_if_exit_code",
                    kEmptyStr).c_str(), "fail_no_retries_if_exit_code");
    }

    if (sec.Get("run_in_separate_dir", false)) {
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
            m_RemoveTempDir = sec.Get("remove_tmp_dir", true);
        else
            m_RemoveTempDir = sec.Get("remove_tmp_path", true);
        m_CacheStdOutErr = sec.Get("cache_std_out_err", true);
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
        if (!f.Exists() || !CanExec(f)) {
            ERR_POST("Can not execute \"" << m_MonitorAppPath
                     << "\". The Monitor application will not run!");
            m_MonitorAppPath = kEmptyStr;
        }
    }

    m_MonitorRunTimeout = s_ToTimeout(sec.Get("max_monitor_running_time", 5));
    m_MonitorPeriod = s_ToTimeout(sec.Get("monitor_period", 5));
    m_KillTimeout.sec = sec.Get("kill_timeout", 1);
    m_KillTimeout.usec = 0;

    m_ExcludeEnv.clear();
    m_IncludeEnv.clear();
    m_AddedEnv.clear();

    NStr::Split(reg.GetString("env_inherit", "exclude", "")," ;,", m_ExcludeEnv,
            NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    NStr::Split(reg.GetString("env_inherit", "include", "")," ;,", m_IncludeEnv,
            NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    list<string> added_env;
    reg.EnumerateEntries("env_set", &added_env);

    ITERATE(list<string>, it, added_env) {
        const string& s = *it;
         m_AddedEnv[s] = reg.GetString("env_set", s, "");
    }

    const string name = CNcbiApplication::Instance()->GetProgramDisplayName();
    int sleep = sec.Get("sleep_between_reap_attempts", 60);
    int max_attempts = sec.Get("max_reap_attempts_after_kill", 60);
    m_Reaper.reset(new CRemoteAppReaper(sleep, max_attempts, name));

    const string cmd = reg.GetString(sec_name, "version_cmd", m_AppPath);
    const string args = reg.GetString(sec_name, "version_args", "-version");
    vector<string> v;
    m_Version.reset(new CRemoteAppVersion(cmd,
                NStr::Split(args, " ", v, NStr::fSplit_NoMergeDelims)));
}

// We need this explicit empty destructor,
// so it could destruct CRemoteAppReaper and CRemoteAppVersion instances.
// Otherwise, there would be implicit inline destructor
// that could be placed where these classes are incomplete.
CRemoteAppLauncher::~CRemoteAppLauncher()
{
}

//////////////////////////////////////////////////////////////////////////////
///
bool CRemoteAppLauncher::CanExec(const CFile& file)
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
class CTimedProcessWatcher : public CPipe::IProcessWatcher
{
public:
    CTimedProcessWatcher(const CTimeout& run_timeout,
            CRemoteAppReaper::CManager &process_manager)
        : m_ProcessManager(process_manager),
          m_Deadline(run_timeout)
    {
    }

    virtual EAction Watch(TProcessHandle pid)
    {
        if (m_Deadline.IsExpired()) {
            ERR_POST("Job run time exceeded "
                     << m_Deadline.PresetSeconds()
                     <<" seconds, stopping the child: " << pid);
            return m_ProcessManager(pid);
        }

        return eContinue;
    }

protected:
    CRemoteAppReaper::CManager& m_ProcessManager;

private:
    const CTimer m_Deadline;
};

//////////////////////////////////////////////////////////////////////////////
///
class CRAMonitor
{
public:
    // The exit code of the monitor program is interpreted as follows
    // (any exit code not listed below is treated as eInternalError)
    enum EResult {
        // The job is running as expected.
        // The monitor's stdout is interpreted as a job progress message.
        // The stderr goes to the log file if logging is enabled.
        eJobRunning = 0,
        // The monitor detected an inconsistency with the job run;
        // the job must be returned back to the queue.
        // The monitor's stderr goes to the log file
        // regardless of whether logging is enabled or not.
        eJobToReturn = 1,
        // The job must be failed.
        // The monitor's stdout is interpreted as the error message;
        // stderr goes to the log file regardless of whether
        // logging is enabled or not.
        eJobFailed = 2,
        // There's a problem with the monitor application itself.
        // The job continues to run and the monitor's stderr goes
        // to the log file unconditionally.
        eInternalError = 3,
    };

    CRAMonitor(const string& path, const char* const* env,
        CTimeout run_timeout) :
        m_Path(path),
        m_Env(env),
        m_RunTimeout(run_timeout)
    {
    }

    int Run(vector<string>& args, CNcbiOstream& out, CNcbiOstream& err,
            CRemoteAppReaper::CManager &process_manager)
    {
        CTimedProcessWatcher callback(m_RunTimeout, process_manager);
        CNcbiStrstream in;
        int exit_value;
        CPipe::EFinish ret = CPipe::eCanceled;
        try {
            ret = CPipe::ExecWait(m_Path, args, in,
                                  out, err, exit_value,
                                  kEmptyStr, m_Env,
                                  &callback,
                                  NULL,
                                  PIPE_SIZE);
        }
        catch (exception& ex) {
            err << ex.what();
        }
        catch (...) {
            err << "Unknown error";
        }

        return ret != CPipe::eDone ? eInternalError : exit_value;
    }

private:
    string m_Path;
    const char* const* m_Env;
    CTimeout m_RunTimeout;
};

//////////////////////////////////////////////////////////////////////////////
///
class CJobContextProcessWatcher : public CTimedProcessWatcher
{
public:
    CJobContextProcessWatcher(CWorkerNodeJobContext& job_context,
                   CTimeout run_timeout,
                   CTimeout keep_alive_period,
                   CRemoteAppReaper::CManager &process_manager)
        : CTimedProcessWatcher(run_timeout, process_manager),
          m_JobContext(job_context), m_KeepAlive(keep_alive_period)
    {
    }

    virtual EAction OnStart(TProcessHandle pid)
    {
        if (m_JobContext.GetShutdownLevel() ==
            CNetScheduleAdmin::eShutdownImmediate) {
            return eStop;
        }

        LOG_POST(Note << "Child PID: " << NStr::UInt8ToString((Uint8) pid));

        return CTimedProcessWatcher::OnStart(pid);
    }

    virtual EAction Watch(TProcessHandle pid)
    {
        if (m_JobContext.GetShutdownLevel() ==
                CNetScheduleAdmin::eShutdownImmediate) {
            m_JobContext.ReturnJob();
            return eStop;
        }

        EAction action = CTimedProcessWatcher::Watch(pid);

        if (action != eContinue)
            return action;

        if (m_KeepAlive.IsExpired()) {
            m_JobContext.JobDelayExpiration(m_KeepAlive.PresetSeconds() + 10);
            m_KeepAlive.Restart();
        }

        return eContinue;
    }

protected:
    CWorkerNodeJobContext& m_JobContext;

private:
    CTimer m_KeepAlive;
};

//////////////////////////////////////////////////////////////////////////////
///
class CMonitoredProcessWatcher : public CJobContextProcessWatcher
{
public:
    CMonitoredProcessWatcher(CWorkerNodeJobContext& job_context,
                   CTimeout run_timeout,
                   CTimeout keep_alive_period,
                   const string& job_wdir,
                   CRemoteAppReaper::CManager &process_manager)
        : CJobContextProcessWatcher(job_context, run_timeout, keep_alive_period,
                process_manager),
          m_Monitor(NULL), m_MonitorWatch(CTimeout::eInfinite),
          m_JobWDir(job_wdir)
    {
    }

    void SetMonitor(CRAMonitor* monitor, const CTimeout& monitor_period)
    {
        m_Monitor.reset(monitor);
        m_MonitorWatch = monitor_period;
    }

    virtual EAction Watch(TProcessHandle pid)
    {
        EAction action = CJobContextProcessWatcher::Watch(pid);

        if (action != eContinue)
            return action;

        if (m_Monitor.get() && m_MonitorWatch.IsExpired()) {
            CNcbiOstrstream out;
            CNcbiOstrstream err;
            vector<string> args;
            args.push_back("-pid");
            args.push_back(NStr::UInt8ToString((Uint8) pid));
            args.push_back("-jid");
            args.push_back(m_JobContext.GetJobKey());
            args.push_back("-jwdir");
            args.push_back(m_JobWDir);

            switch (m_Monitor->Run(args, out, err, m_ProcessManager)) {
            case CRAMonitor::eJobRunning:
                {
                    bool non_empty_output = !IsOssEmpty(out);
                    if (non_empty_output) {
                        m_JobContext.PutProgressMessage
                            (CNcbiOstrstreamToString(out), true);
                    }
                    if (m_JobContext.IsLogRequested() &&
                        ( !non_empty_output || !IsOssEmpty(err) ))
                        x_Log("exited with zero return code", err);
                }
                break;
            case CRAMonitor::eJobToReturn:
                m_JobContext.ReturnJob();
                x_Log("job is returned", err);
                return eStop;
            case CRAMonitor::eJobFailed:
                {
                    x_Log("job failed", err);
                    string errmsg;
                    if ( !IsOssEmpty(out) ) {
                        errmsg = CNcbiOstrstreamToString(out);
                    } else
                        errmsg = "Monitor requested job termination";
                    throw runtime_error(errmsg);
                }
                break;
            default:
                x_Log("internal error", err);
                break;
            }
            m_MonitorWatch.Restart();
        }

        return eContinue;
    }

private:
    inline void x_Log(const string& what, CNcbiOstrstream& sstream)
    {
        if ( !IsOssEmpty(sstream) ) {
            ERR_POST(m_JobContext.GetJobKey() << " (monitor) " << what <<
                     ": " << (string)CNcbiOstrstreamToString(sstream));
        } else {
            ERR_POST(m_JobContext.GetJobKey() << " (monitor) " << what << ".");
        }
    }

    auto_ptr<CRAMonitor> m_Monitor;
    CTimer m_MonitorWatch;
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
        }
        catch (exception& ex) {
            ERR_POST("CTmpStreamGuard::~CTmpStreamGuard(): " <<
                m_Name << " --> " << ex.what());
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
    unsigned app_run_timeout,
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

        CTimeout run_timeout = min(s_ToTimeout(app_run_timeout), m_AppRunTimeout);
        string working_dir(tmp_path.empty() ? CDir::GetCwd() : tmp_path);

#ifdef NCBI_OS_MSWIN
        NStr::ReplaceInPlace(working_dir, "\\", "/");
#endif

        CMonitoredProcessWatcher callback(job_context,
            run_timeout,
            m_KeepAlivePeriod,
            working_dir,
            m_Reaper->GetManager());

        if (!m_MonitorAppPath.empty() && m_MonitorPeriod.IsFinite()) {
            callback.SetMonitor(
                    new CRAMonitor(m_MonitorAppPath, env, m_MonitorRunTimeout),
                    m_MonitorPeriod);
        }

        bool result = CPipe::ExecWait(GetAppPath(), args, in,
                               std_out_guard.GetOStream(),
                               std_err_guard.GetOStream(),
                               exit_value,
                               tmp_path, env, &callback,
                               &m_KillTimeout,
                               PIPE_SIZE) == CPipe::eDone;

        std_err_guard.Close();
        std_out_guard.Close();

        return result;
    }
}

void CRemoteAppLauncher::FinishJob(bool finished_ok, int ret,
        CWorkerNodeJobContext& context) const
{
    if (!finished_ok) {
        if (!context.IsJobCommitted())
            context.CommitJobWithFailure("Job has been canceled");
    } else
        if (MustFailNoRetries(ret))
            context.CommitJobWithFailure(
                    "Exited with return code " + NStr::IntToString(ret) +
                    " - will not be rerun",
                    true /* no retries */);
        else if (ret == 0 || m_NonZeroExitAction == eDoneOnNonZeroExit)
            context.CommitJob();
        else if (m_NonZeroExitAction == eReturnOnNonZeroExit)
            context.ReturnJob();
        else
            context.CommitJobWithFailure(
                    "Exited with return code " + NStr::IntToString(ret));
}

// Check whether retries are disabled for the specified exit code.
bool CRemoteAppLauncher::MustFailNoRetries(int exit_code) const
{
    const CRangeList::TRangeVector& range = m_RangeList.GetRangeList();

    if (range.empty())
        return false;

    ITERATE(CRangeList::TRangeVector, it, range) {
        if (it->first <= exit_code && exit_code <= it->second)
            return true;
    }

    return false;
}

string CRemoteAppLauncher::GetAppVersion(const string& v) const
{
    return m_Version->Get(v);
}

void CRemoteAppLauncher::OnGridWorkerStart()
{
    m_Reaper->StartCollector();
}

END_NCBI_SCOPE
