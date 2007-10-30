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


class CWorkerNodeJobContext;
bool ExecRemoteApp(const string& cmd, 
                   const vector<string>& args,
                   CNcbiIstream& in, CNcbiOstream& out, CNcbiOstream& err,
                   int& exit_value,
                   CWorkerNodeJobContext& context,
                   int max_app_running_time,
                   int app_running_time,
                   int keep_alive_period,
                   const string& tmp_path,
                   bool remove_tmp_path,
                   const string& job_wdir,
                   const char* const env[] = 0,
                   const string& monitor_app = kEmptyStr,
                   int max_monitor_running_time = 5,
                   int monitor_period = 5,
                   int kill_timeout = 1);



class IRegistry;
class CRemoteAppParams 
{
public:
    enum ENonZeroExitAction {
        eDoneOnNonZeroExit,
        eFailOnNonZeroExit,
        eReturnOnNonZeroExit
    };

    CRemoteAppParams();
    ~CRemoteAppParams();

    void Load(const string& sec_name, const IRegistry&);

    const string& GetAppPath() const { return m_AppPath; }
    int GetMaxAppRunningTime() const { return m_MaxAppRunningTime; }
    int GetKeepAlivePeriod() const { return m_KeepAlivePeriod; }
    ENonZeroExitAction GetNonZeroExitAction() const { return m_NonZeroExitAction; }
    bool RunInSeparateDir() const { return m_RunInSeparateDir; }
    const string& GetTempDir() const { return m_TempDir; }
    bool RemoveTempDir() const { return m_RemoveTempDir; }

    const string& GetMonitorAppPath() const { return m_MonitorAppPath; }
    int GetMaxMonitorRunningTime() const { return m_MaxMonitorRunningTime; }
    int GetMonitorPeriod() const { return m_MonitorPeriod; }

    int GetKillTimeout() const { return m_KillTimeout; }

    const CNcbiEnvironment& GetLocalEnv() const { return m_LocalEnv; }
    const map<string,string>& GetAddedEnv() const { return m_AddedEnv; }
    const list<string>& GetExcludedEnv() const { return m_ExcludeEnv; }
    const list<string>& GetIncludedEnv() const { return m_IncludeEnv; }

private:
    string m_AppPath;
    int m_MaxAppRunningTime;
    int m_KeepAlivePeriod;
    ENonZeroExitAction m_NonZeroExitAction;
    bool m_RunInSeparateDir;
    string m_TempDir;
    bool m_RemoveTempDir;

    string m_MonitorAppPath;
    int m_MaxMonitorRunningTime;
    int m_MonitorPeriod;
    int m_KillTimeout;

    CNcbiEnvironment m_LocalEnv;
    map<string,string> m_AddedEnv;
    list<string> m_ExcludeEnv;
    list<string> m_IncludeEnv;
    
};


END_NCBI_SCOPE

#endif 

