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
 * Authors:  Maxim Didenko, Dmitry Kazimirov, Rafael Sadyrov
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
#include "async_task.hpp"

#define PIPE_SIZE 64 * 1024

BEGIN_NCBI_SCOPE

// A record for a child process
struct CRemoteAppReaperTask
{
    const CProcess process;

    CRemoteAppReaperTask(TProcessHandle handle) : process(handle) {}

    bool operator()(int current, int max_attempts);
};

bool CRemoteAppReaperTask::operator()(int current, int max_attempts)
{
    CProcess::CExitInfo exitinfo;
    const bool first_attempt = current == 1;

    if (process.Wait(0, &exitinfo) != -1 || exitinfo.IsExited() || exitinfo.IsSignaled()) {
        // Log a message for those that had failed to be killed before
        if (!first_attempt) {
            LOG_POST(Note << "Successfully waited for a process: " << process.GetHandle());
        }

        return true;
    }

    if (first_attempt) {
        if (process.KillGroup()) return true;

        ERR_POST(Warning << "Failed to kill a process: " << process.GetHandle() << ", will wait for it");
        return false;
    }

    if (current > max_attempts) {
        // Give up if there are too many attempts to wait for a process
        ERR_POST("Gave up waiting for a process: " << process.GetHandle());
        return true;
    }

    return false;
}

// This class is responsibe for the whole process of reaping child processes
class CRemoteAppReaper : public CAsyncTaskProcessor<CRemoteAppReaperTask>
{
    using CAsyncTaskProcessor<CRemoteAppReaperTask>::CAsyncTaskProcessor;
};

struct CRemoteAppRemoverTask
{
    const string path;

    CRemoteAppRemoverTask(string p);

    bool operator()(int current, int max_attempts) const;
};

CRemoteAppRemoverTask::CRemoteAppRemoverTask(string p) :
    path(move(p))
{
    if (path.empty()) return;

    CDir dir(path);

    if (dir.Exists()) return;

    dir.CreatePath();
}

bool CRemoteAppRemoverTask::operator()(int current, int max_attempts) const
{
    if (path.empty()) return true;

    const bool first_attempt = current == 1;

    try {
        if (CDir(path).Remove(CDirEntry::eRecursiveIgnoreMissing)) {
            // Log a message for those that had failed to be removed before
            if (!first_attempt) {
                LOG_POST(Note << "Successfully removed a path: " << path);
            }

            return true;
        }
    }
    catch (...) {
    }

    if (current > max_attempts) {
        // Give up if there are too many attempts to remove a path
        ERR_POST("Gave up removing a path: " << path);
        return true;
    }

    if (first_attempt) {
        ERR_POST(Warning << "Failed to remove a path: " << path << ", will try later");
        return false;
    }

    return false;
}

// This class is responsibe for removing tmp directories
class CRemoteAppRemover : public CAsyncTaskProcessor<CRemoteAppRemoverTask>
{
public:
    struct SGuard;

    using CAsyncTaskProcessor<CRemoteAppRemoverTask>::CAsyncTaskProcessor;
};

struct CRemoteAppRemover::SGuard
{
    SGuard(CRemoteAppRemover* remover, CRemoteAppRemoverTask task, bool remove_tmp_dir) :
        m_Scheduler(remover ? &remover->GetScheduler() : nullptr),
        m_Task(move(task)),
        m_RemoveTmpDir(remove_tmp_dir)
    {}

    ~SGuard()
    {
        // Do not remove
        if (!m_RemoveTmpDir) return;

        // Remove asynchronously
        if (m_Scheduler && (*m_Scheduler)(m_Task)) return;

        // Remove synchronously
        m_Task(1, 0);
    }

private:
    CRemoteAppRemover::CScheduler* m_Scheduler;
    CRemoteAppRemoverTask m_Task;
    bool m_RemoveTmpDir;
};

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

class CTimedProcessWatcher : public CPipe::IProcessWatcher
{
public:
    struct SParams
    {
        string process_type;
        const CTimeout& run_timeout;
        CRemoteAppReaper::CScheduler& process_manager;

        SParams(string pt, const CTimeout& rt, CRemoteAppReaper::CScheduler& pm) :
            process_type(move(pt)),
            run_timeout(rt),
            process_manager(pm)
        {}
    };

    CTimedProcessWatcher(SParams& p)
        : m_ProcessManager(p.process_manager),
          m_ProcessType(p.process_type),
          m_Deadline(p.run_timeout)
    {
    }

    virtual EAction Watch(TProcessHandle pid)
    {
        if (m_Deadline.IsExpired()) {
            ERR_POST(m_ProcessType << " run time exceeded "
                     << m_Deadline.PresetSeconds()
                     <<" second(s), stopping the child: " << pid);
            return m_ProcessManager(pid) ? eExit : eStop;
        }

        return eContinue;
    }

protected:
    CRemoteAppReaper::CScheduler& m_ProcessManager;
    const string m_ProcessType;
    const CTimer m_Deadline;
};

// This class is responsible for reporting app/cgi version run by this app
class CRemoteAppVersion
{
public:
    CRemoteAppVersion(const string& app, const vector<string>& args)
        : m_App(app), m_Args(args)
    {}

    string Get(CTimedProcessWatcher::SParams& p, const string& v) const;

private:
    const string m_App;
    const vector<string> m_Args;
};

string CRemoteAppVersion::Get(CTimedProcessWatcher::SParams& p, const string& v) const
{
    CTimedProcessWatcher  wait_one_second(p);
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

// This class is responsible for reporting clients about apps timing out
class CRemoteAppTimeoutReporter
{
public:
    CRemoteAppTimeoutReporter(const string& mode) : m_Mode(Get(mode)) {}

    void Report(CWorkerNodeJobContext& job_context, unsigned seconds)
    {
        if (m_Mode != eNever) {
            job_context.PutProgressMessage("Job run time exceeded " +
                    NStr::UIntToString(seconds) + " seconds.", true,
                    m_Mode == eAlways);
        }
    }

private:
    enum EMode { eSmart, eAlways, eNever };

    static EMode Get(const string& mode)
    {
        if (!NStr::CompareNocase(mode, "smart"))
            return eSmart;

        else if (!NStr::CompareNocase(mode, "always"))
            return eAlways;

        else if (!NStr::CompareNocase(mode, "never"))
            return eNever;
        else
            ERR_POST("Unknown parameter value: "
                    "parameter \"progress_message_on_timeout\", "
                    "value: \"" << mode << "\". "
                    "Allowed values: smart, always, never");

        return eSmart;
    }

    EMode m_Mode;
};

/// Class representing (non overlapping) integer ranges.
///
class CRanges
{
public:
    /// Reads integer ranges from an input stream.
    ///
    /// The data must be in the following format (in ascending order):
    ///     [!] R1, N2, ..., Rn
    ///
    /// Where:
    ///     !         - negation, makes all provided ranges be excluded
    ///                 (not included).
    ///     R1 ... Rn - integer closed ranges specified either as 
    ///                 FROM - TO (corresponds to [FROM, TO] range) or as
    ///                 NUMBER (corresponds to [NUMBER, NUMBER] range).
    /// Example:
    ///     4, 6 - 9, 16 - 40, 64
    /// 
    CRanges(istream& is);

    /// Checks whether provided number belongs to one of the ranges.
    ///
    bool Contain(int n) const;

private:
    vector<int> m_Ranges;
};

CRanges::CRanges(istream& is)
{
    char ch;

    if (!(is >> skipws >> ch)) return; // EoF

    // Char '!' makes provided ranges to be excluded (not included)
    if (ch == '!') 
        m_Ranges.push_back(numeric_limits<int>::min());
    else
        is.putback(ch);

    bool interval = false;

    do {
        int n;

        if (!(is >> n)) break;
        if (!(is >> ch)) ch = ',';

        if (m_Ranges.size()) {
            int previous =  m_Ranges.back();

            if (n < previous) {
                ostringstream err;
                err << n << " is less or equal than previous number, "
                    "intervals must be sorted and not overlapping";
                throw invalid_argument(err.str());
            }
        }

        if (ch == ',') {
            // If it's only one number in interval, add both start and end of it
            if (!interval) m_Ranges.push_back(n);

            m_Ranges.push_back(n + 1);
            interval = false;

        } else if (ch == '-' && !interval) {
            interval = true;
            m_Ranges.push_back(n);

        } else {
            ostringstream err;
            err << "Unexpected char '" << ch << "'";
            throw invalid_argument(err.str());
        }
    } while (is);

    if (interval) {
        ostringstream err;
        err << "Missing interval end";
        throw invalid_argument(err.str());
    }

    if (!is.eof()) {
        ostringstream err;
        err << "Not a number near: " << is.rdbuf();
        throw invalid_argument(err.str());
    }
}

bool CRanges::Contain(int n) const
{
    auto range = upper_bound(m_Ranges.begin(), m_Ranges.end(), n);

    // Every number starts a range, every second range is excluded
    return (range - m_Ranges.begin()) % 2;
}

CRanges* s_ReadRanges(const IRegistry& reg, const string& sec, string param)
{
    if (reg.HasEntry(sec, param)) {
        istringstream iss(reg.GetString(sec, param, kEmptyStr));

        try {
            return new CRanges(iss);
        }
        catch (invalid_argument& ex) {
            NCBI_THROW_FMT(CInvalidParamException, eInvalidCharacter,
                "Parameter '" << param << "' parsing error: " << ex.what());
        }
    }

    return nullptr;
}

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

    m_MustFailNoRetries.reset(
            s_ReadRanges(reg, sec_name, "fail_no_retries_if_exit_code"));

    const string name = CNcbiApplication::Instance()->GetProgramDisplayName();

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

        int sleep = sec.Get("sleep_between_remove_tmp_attempts", 60);
        int max_attempts = sec.Get("max_remove_tmp_attempts", 60);
        m_Remover.reset(new CRemoteAppRemover(sleep, max_attempts, name + "_rm"));

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

    int sleep = sec.Get("sleep_between_reap_attempts", 60);
    int max_attempts = sec.Get("max_reap_attempts_after_kill", 60);
    m_Reaper.reset(new CRemoteAppReaper(sleep, max_attempts, name + "_cl"));

    const string cmd = reg.GetString(sec_name, "version_cmd", m_AppPath);
    const string args = reg.GetString(sec_name, "version_args", "-version");
    vector<string> v;
    m_Version.reset(new CRemoteAppVersion(cmd,
                NStr::Split(args, " ", v)));

    const string mode = reg.GetString(sec_name, "progress_message_on_timeout",
            "smart");
    m_TimeoutReporter.reset(new CRemoteAppTimeoutReporter(mode));
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
class CJobContextProcessWatcher : public CTimedProcessWatcher
{
public:
    struct SParams : CTimedProcessWatcher::SParams
    {
        CWorkerNodeJobContext& job_context;
        const CTimeout& keep_alive_period;
        CRemoteAppTimeoutReporter& timeout_reporter;

        SParams(CWorkerNodeJobContext& jc,
                string pt,
                const CTimeout& rt,
                const CTimeout& kap,
                CRemoteAppTimeoutReporter& tr,
                CRemoteAppReaper::CScheduler& pm)
            : CTimedProcessWatcher::SParams(move(pt), rt, pm),
                job_context(jc),
                keep_alive_period(kap),
                timeout_reporter(tr)
        {}
    };

    CJobContextProcessWatcher(SParams& p)
        : CTimedProcessWatcher(p),
          m_JobContext(p.job_context), m_KeepAlive(p.keep_alive_period),
          m_TimeoutReporter(p.timeout_reporter)
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

        if (action != eContinue) {
            m_TimeoutReporter.Report(m_JobContext, m_Deadline.PresetSeconds());
            return action;
        }

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
    CRemoteAppTimeoutReporter& m_TimeoutReporter;
};

//////////////////////////////////////////////////////////////////////////////
///
class CMonitoredProcessWatcher : public CJobContextProcessWatcher
{
public:
    CMonitoredProcessWatcher(SParams& p, const string& job_wdir,
            const string& path, const char* const* env,
            CTimeout run_period, CTimeout run_timeout)
        : CJobContextProcessWatcher(p),
          m_MonitorWatch(run_period),
          m_JobWDir(job_wdir),
          m_Path(path),
          m_Env(env),
          m_RunTimeout(run_timeout)
    {
    }

    virtual EAction Watch(TProcessHandle pid)
    {
        EAction action = CJobContextProcessWatcher::Watch(pid);

        if (action != eContinue)
            return action;

        if (m_MonitorWatch.IsExpired()) {
            action = MonitorRun(pid);
            m_MonitorWatch.Restart();
        }

        return action;
    }

private:
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

    EAction MonitorRun(TProcessHandle pid)
    {
        CNcbiStrstream in;
        CNcbiOstrstream out;
        CNcbiOstrstream err;
        vector<string> args;
        args.push_back("-pid");
        args.push_back(NStr::UInt8ToString((Uint8) pid));
        args.push_back("-jid");
        args.push_back(m_JobContext.GetJobKey());
        args.push_back("-jwdir");
        args.push_back(m_JobWDir);

        CTimedProcessWatcher::SParams params("Monitor", m_RunTimeout, m_ProcessManager);
        CTimedProcessWatcher callback(params);
        int exit_value = eInternalError;
        try {
            if (CPipe::eDone != CPipe::ExecWait(m_Path, args, in,
                                  out, err, exit_value,
                                  kEmptyStr, m_Env,
                                  &callback,
                                  NULL,
                                  PIPE_SIZE)) {
                exit_value = eInternalError;
            }
        }
        catch (exception& ex) {
            err << ex.what();
        }
        catch (...) {
            err << "Unknown error";
        }

        switch (exit_value) {
        case eJobRunning:
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
            return eContinue;

        case eJobToReturn:
            m_JobContext.ReturnJob();
            x_Log("job is returned", err);
            return eStop;

        case eJobFailed:
            {
                x_Log("job failed", err);
                string errmsg;
                if ( !IsOssEmpty(out) ) {
                    errmsg = CNcbiOstrstreamToString(out);
                } else
                    errmsg = "Monitor requested job termination";
                throw runtime_error(errmsg);
            }
            return eContinue;
        }

        x_Log("internal error", err);
        return eContinue;
    }

    inline void x_Log(const string& what, CNcbiOstrstream& sstream)
    {
        if ( !IsOssEmpty(sstream) ) {
            ERR_POST(m_JobContext.GetJobKey() << " (monitor) " << what <<
                     ": " << (string)CNcbiOstrstreamToString(sstream));
        } else {
            ERR_POST(m_JobContext.GetJobKey() << " (monitor) " << what << ".");
        }
    }

    CTimer m_MonitorWatch;
    string m_JobWDir;
    string m_Path;
    const char* const* m_Env;
    CTimeout m_RunTimeout;
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
    unique_ptr<CFileReaderWriter> m_ReaderWriter;
    unique_ptr<CNcbiOstream> m_StreamGuard;
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

    CRemoteAppRemover::SGuard guard(m_Remover.get(), tmp_path, m_RemoveTempDir);
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

        CJobContextProcessWatcher::SParams params(job_context,
                "Job",
                run_timeout,
                m_KeepAlivePeriod,
                *m_TimeoutReporter,
                m_Reaper->GetScheduler());

        bool monitor = !m_MonitorAppPath.empty() && m_MonitorPeriod.IsFinite();

        unique_ptr<CPipe::IProcessWatcher> watcher(monitor ?
                new CMonitoredProcessWatcher(params, working_dir,
                    m_MonitorAppPath, env, m_MonitorPeriod, m_MonitorRunTimeout) :
                new CJobContextProcessWatcher(params));

        bool result = CPipe::ExecWait(GetAppPath(), args, in,
                               std_out_guard.GetOStream(),
                               std_err_guard.GetOStream(),
                               exit_value,
                               tmp_path, env, watcher.get(),
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
        // Check whether retries are disabled for the specified exit code.
        if (m_MustFailNoRetries && m_MustFailNoRetries->Contain(ret))
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

string CRemoteAppLauncher::GetAppVersion(const string& v) const
{
    CTimedProcessWatcher::SParams params("Version", CTimeout(1.0), m_Reaper->GetScheduler());
    return m_Version->Get(params, v);
}

void CRemoteAppLauncher::OnGridWorkerStart()
{
    m_Reaper->StartExecutor();
    if (m_Remover) m_Remover->StartExecutor();
}

void CRemoteAppIdleTask::Run(CWorkerNodeIdleTaskContext&)
{
    if (!m_AppCmd.empty())
        CExec::System(m_AppCmd.c_str());
}

END_NCBI_SCOPE
