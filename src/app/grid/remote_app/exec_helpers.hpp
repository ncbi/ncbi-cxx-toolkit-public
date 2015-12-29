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

#include <util/rangelist.hpp>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbienv.hpp>

#include <connect/services/grid_worker_app.hpp>

BEGIN_NCBI_SCOPE

class CRemoteAppReaper;
class CRemoteAppVersion;

class CRemoteAppLauncher
{
public:
    enum ENonZeroExitAction {
        eDoneOnNonZeroExit,
        eFailOnNonZeroExit,
        eReturnOnNonZeroExit
    };

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
    // Check whether retries are disabled for the specified exit code.
    bool MustFailNoRetries(int exit_code) const;

    string m_AppPath;
    CTimeout m_AppRunTimeout;
    int m_KeepAlivePeriod;
    ENonZeroExitAction m_NonZeroExitAction;
    CRangeList m_RangeList;
    string m_TempDir;
    bool m_RemoveTempDir;
    bool m_CacheStdOutErr;

    string m_MonitorAppPath;
    CTimeout m_MonitorRunTimeout;
    int m_MonitorPeriod;
    unsigned int m_KillTimeout;

    CNcbiEnvironment m_LocalEnv;
    TEnvMap m_AddedEnv;
    list<string> m_ExcludeEnv;
    list<string> m_IncludeEnv;

    auto_ptr<CRemoteAppReaper> m_Reaper;
    auto_ptr<CRemoteAppVersion> m_Version;
};

// This class is for starting CCollector (a separate thread) after Daemonize()
class CRemoteAppBaseListener : public CGridWorkerNodeApp_Listener
{
public:
    typedef auto_ptr<CRemoteAppLauncher> TLauncherPtr;

    CRemoteAppBaseListener(const TLauncherPtr& launcher) : m_Launcher(launcher) {}

    void OnGridWorkerStart()
    {
        _ASSERT(m_Launcher.get());
        m_Launcher->OnGridWorkerStart();
    }

private:
    const TLauncherPtr& m_Launcher;
};

template<class TFactory, class TListener>
int Main(int argc, const char* argv[])
{
    GetDiagContext().SetOldPostFormat(false);
    auto_ptr<TFactory> factory(new TFactory);
    auto_ptr<TListener> listener(factory->CreateListener());
    const string app_name(factory->GetAppName());
    CGridWorkerApp app(factory.release());
    app.SetListener(listener.release());
    return app.AppMain(argc, argv, NULL, eDS_ToStdlog, NcbiEmptyCStr, app_name);
}

END_NCBI_SCOPE

#endif

