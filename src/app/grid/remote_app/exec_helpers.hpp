#ifndef __REMOTE_APP_EXEC_HELPERS__HPP
#define __REMOTE_APP_EXEC_HELPERS__HPP

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
 * Authors: Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include <corelib/ncbistre.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiexec.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbiargs.hpp>

#include <connect/services/grid_worker_app.hpp>
#include <connect/services/grid_globals.hpp>

#include <vector>
#include <algorithm>

BEGIN_NCBI_SCOPE

class CRemoteAppReaper;
class CRemoteAppRemover;
class CRemoteAppVersion;
class CRemoteAppTimeoutReporter;
class CRanges;

class CRemoteAppLauncher
{
public:
    CRemoteAppLauncher(const string& sec_name, const IRegistry&);
    ~CRemoteAppLauncher();

    bool ExecRemoteApp(const vector<string>& args,
        CNcbiIstream& in, CNcbiOstream& out, CNcbiOstream& err,
        int& exit_value,
        CWorkerNodeJobContext& job_context,
        unsigned app_run_timeout,
        const char* const env[]) const;

    void FinishJob(bool finished_ok, int ret,
            CWorkerNodeJobContext& context) const;

    const string& GetAppPath() const { return m_AppPath; }
    const CNcbiEnvironment& GetLocalEnv() const { return m_LocalEnv; }

#ifdef NCBI_OS_MSWIN
    // Use case-insensitive comparison on Windows so that
    // duplicate entries do not appear in m_EnvValues.
    typedef map<string, string, PNocase> TEnvMap;
#else
    typedef map<string, string> TEnvMap;
#endif

    const TEnvMap& GetAddedEnv() const { return m_AddedEnv; }
    const list<string>& GetExcludedEnv() const { return m_ExcludeEnv; }
    const list<string>& GetIncludedEnv() const { return m_IncludeEnv; }

    string GetAppVersion(const string&) const;

    void OnGridWorkerStart();

    static bool CanExec(const CFile& file);

private:
    enum ENonZeroExitAction {
        eDoneOnNonZeroExit,
        eFailOnNonZeroExit,
        eReturnOnNonZeroExit
    };

    string m_AppPath;
    CTimeout m_AppRunTimeout;
    CTimeout m_KeepAlivePeriod;
    ENonZeroExitAction m_NonZeroExitAction;
    string m_TempDir;
    bool m_RemoveTempDir;
    bool m_CacheStdOutErr;

    string m_MonitorAppPath;
    CTimeout m_MonitorRunTimeout;
    CTimeout m_MonitorPeriod;
    STimeout m_KillTimeout;

    CNcbiEnvironment m_LocalEnv;
    TEnvMap m_AddedEnv;
    list<string> m_ExcludeEnv;
    list<string> m_IncludeEnv;

    unique_ptr<CRemoteAppReaper> m_Reaper;
    unique_ptr<CRemoteAppRemover> m_Remover;
    unique_ptr<CRemoteAppVersion> m_Version;
    unique_ptr<CRemoteAppTimeoutReporter> m_TimeoutReporter;
    unique_ptr<CRanges> m_MustFailNoRetries;
};

// This class is for starting CCollector (a separate thread) after Daemonize()
class CRemoteAppBaseListener : public CGridWorkerNodeApp_Listener
{
public:
    typedef unique_ptr<CRemoteAppLauncher> TLauncherPtr;

    CRemoteAppBaseListener(const TLauncherPtr& launcher) : m_Launcher(launcher) {}

    void OnGridWorkerStart()
    {
        _ASSERT(m_Launcher.get());
        m_Launcher->OnGridWorkerStart();
    }

private:
    const TLauncherPtr& m_Launcher;
};

class CRemoteAppIdleTask : public IWorkerNodeIdleTask
{
public:
    CRemoteAppIdleTask(const string& idle_app_cmd) : m_AppCmd(idle_app_cmd) {}
    ~CRemoteAppIdleTask() {}

    void Run(CWorkerNodeIdleTaskContext&);

private:
    string m_AppCmd;
};

template <class TJob, class TListener, const char* const kName>
class CRemoteAppJobFactory : public IWorkerNodeJobFactory
{
public:
    virtual void Init(const IWorkerNodeInitContext& context)
    {
        m_RemoteAppLauncher.reset(
                new CRemoteAppLauncher(kName, context.GetConfig()));

        CFile file(m_RemoteAppLauncher->GetAppPath());

        if (!file.Exists()) {
            NCBI_THROW_FMT(CException, eInvalid, "File \"" <<
                    m_RemoteAppLauncher->GetAppPath() <<
                    "\" does not exists.");
        }

        if (!CRemoteAppLauncher::CanExec(file)) {
            NCBI_THROW_FMT(CException, eInvalid, "File \"" <<
                    m_RemoteAppLauncher->GetAppPath() <<
                    "\" is not executable.");
        }

        m_WorkerNodeInitContext = &context;
        string idle_app_cmd = context.GetConfig().GetString(kName,
                "idle_app_cmd", kEmptyStr);
        if (!idle_app_cmd.empty())
            m_IdleTask.reset(new CRemoteAppIdleTask(idle_app_cmd));
    }

    virtual IWorkerNodeJob* CreateInstance(void)
    {
        return new TJob(*m_WorkerNodeInitContext, *m_RemoteAppLauncher);
    }

    virtual IWorkerNodeIdleTask* GetIdleTask() { return m_IdleTask.get(); }

    virtual string GetJobVersion() const
    {
        return m_WorkerNodeInitContext->GetNetScheduleAPI().GetProgramVersion();
    }

    virtual string GetAppName() const
    {
        return kName;
    }

    virtual string GetAppVersion() const
    {
        _ASSERT(m_RemoteAppLauncher.get());
        return m_RemoteAppLauncher->GetAppVersion(GRID_APP_VERSION);
    }

    TListener* CreateListener() const
    {
        return new TListener(m_RemoteAppLauncher);
    }

private:
    unique_ptr<CRemoteAppLauncher> m_RemoteAppLauncher;
    const IWorkerNodeInitContext* m_WorkerNodeInitContext;
    unique_ptr<IWorkerNodeIdleTask> m_IdleTask;
};

template<class TFactory, class TListener>
int Main(int argc, const char* argv[])
{
    GetDiagContext().SetOldPostFormat(false);
    unique_ptr<TFactory> factory(new TFactory);
    unique_ptr<TListener> listener(factory->CreateListener());
    const string app_name(factory->GetAppName());
    CGridWorkerApp app(factory.release());
    app.SetListener(listener.release());
    return app.AppMain(argc, argv, NULL, eDS_ToStdlog, NcbiEmptyCStr, app_name);
}

END_NCBI_SCOPE

#endif

