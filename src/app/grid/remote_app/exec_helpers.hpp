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
 * Authors: Maxim Didenko
 *
 * File Description:
 *
 */

#include <corelib/ncbistre.hpp>
#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE

class CFile;
bool CanExecRemoteApp(const CFile& file);


class IRegistry;
class CWorkerNodeJobContext;

class CRemoteAppLauncher
{
public:
    enum ENonZeroExitAction {
        eDoneOnNonZeroExit,
        eFailOnNonZeroExit,
        eReturnOnNonZeroExit
    };

    CRemoteAppLauncher();

    void LoadParams(const string& sec_name, const IRegistry&);

    bool ExecRemoteApp(const vector<string>& args,
        CNcbiIstream& in, CNcbiOstream& out, CNcbiOstream& err,
        int& exit_value,
        CWorkerNodeJobContext& context,
        int app_run_timeout,
        const char* const env[]);

    const string& GetAppPath() const { return m_AppPath; }
    ENonZeroExitAction GetNonZeroExitAction() const
        {return m_NonZeroExitAction;}

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

private:
    string m_AppPath;
    int m_MaxAppRunningTime;
    int m_KeepAlivePeriod;
    ENonZeroExitAction m_NonZeroExitAction;
    string m_TempDir;
    bool m_RemoveTempDir;
    bool m_CacheStdOutErr;

    string m_MonitorAppPath;
    int m_MaxMonitorRunningTime;
    int m_MonitorPeriod;
    int m_KillTimeout;

    CNcbiEnvironment m_LocalEnv;
    TEnvMap m_AddedEnv;
    list<string> m_ExcludeEnv;
    list<string> m_IncludeEnv;
};

END_NCBI_SCOPE

#endif 

