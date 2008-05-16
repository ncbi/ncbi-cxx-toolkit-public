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

#include <cgi/ncbicgi.hpp>

#include <vector>
#include <algorithm>
#include <iterator>

#include "exec_helpers.hpp"

USING_NCBI_SCOPE;

struct IsStandard : unary_function <string,bool>
{
    bool operator() (const string& val) const;
    static const char* const sm_StandardCgiEnv[];
};

const char* const IsStandard::sm_StandardCgiEnv[] = {
    "DOCUMENT_ROOT",
    "GATEWAY_INTERFACE",
    "PROXIED_IP",
    "QUERY_STRING",
    "REMOTE_ADDR",
    "REMOTE_PORT",
    "REMOTE_IDENT",
    "REQUEST_METHOD",
    "REQUEST_URI",
    "SCRIPT_FILENAME",
    "SCRIPT_NAME",
    "SCRIPT_URI",
    "SCRIPT_URL",
    "SERVER_ADDR",
    "SERVER_ADMIN",
    "SERVER_NAME",
    "SERVER_PORT",
    "SERVER_PROTOCOL",
    "SERVER_SIGNATURE",
    "SERVER_SOFTWARE",
    "SERVER_METHOD",
    "PATH_INFO",
    "PATH_TRANSLATED",
    "AUTH_TYPE",
    "CONTENT_TYPE",
    "CONTENT_LENGTH",
    "UNIQUE_ID",
    NULL
};

inline bool IsStandard::operator() (const string& val) const
{
    if (NStr::StartsWith(val, "HTTP_")) return true;
    for( int i = 0; sm_StandardCgiEnv[i] != NULL; ++i) {
        if (NStr::Compare(val, sm_StandardCgiEnv[i]) == 0) return true;
    }
    return false;
}

template <typename Cont>
struct HasValue : unary_function <string,bool>
{
    HasValue(const Cont& cont) : m_Cont(cont) {}
    inline bool operator() (const string& val) const
    {
        return find(m_Cont.begin(), m_Cont.end(), val) != m_Cont.end();
    }
    const Cont& m_Cont;
};

class CCgiEnvHolder
{
public:
    CCgiEnvHolder(const CRemoteAppParams& params, const CNcbiEnvironment& client_env);
    ~CCgiEnvHolder() {}

    const char* const* GetEnv() const { return &m_Env[0]; }
    
private:
    list<string> m_EnvValues;
    vector<const char*> m_Env;
};

CCgiEnvHolder::CCgiEnvHolder(const CRemoteAppParams& params, const CNcbiEnvironment& client_env)
{
    list<string> cln_names;
    client_env.Enumerate(cln_names);
    list<string> names(cln_names.begin(), cln_names.end());
    names.erase(remove_if(names.begin(),names.end(), 
                          not1(IsStandard())),
                names.end());
    names.erase(remove_if(names.begin(),names.end(), 
                          HasValue<list<string> >(params.GetExcludedEnv())),
                names.end());
    list<string> inc_names(cln_names.begin(), cln_names.end());
    inc_names.erase(remove_if(inc_names.begin(),inc_names.end(), 
                              not1(HasValue<list<string> >(params.GetIncludedEnv()))),
                    inc_names.end());
    names.insert(names.begin(),inc_names.begin(), inc_names.end());
    typedef map<string,string> TMapStr;
    const TMapStr& added_env = params.GetAddedEnv();
    ITERATE(TMapStr, it, added_env) {
        m_EnvValues.push_back(it->first + "=" +it->second);
    }

    ITERATE(list<string>, it, names) {
        if (added_env.find(*it) == added_env.end())
            m_EnvValues.push_back(*it + "=" + client_env.Get(*it));
    }
    
    list<string> local_names;
    const CNcbiEnvironment& local_env = params.GetLocalEnv();
    local_env.Enumerate(local_names);
    ITERATE(list<string>, it, local_names) {
        const string& s = *it;
        if (added_env.find(s) == added_env.end()
            && find(names.begin(), names.end(), s) == names.end())
            m_EnvValues.push_back(s + "=" + local_env.Get(s));
    }

    ITERATE(list<string>, it, m_EnvValues) {
        m_Env.push_back(it->c_str());
    }
    m_Env.push_back(NULL);
}

    
///////////////////////////////////////////////////////////////////////

/// NetSchedule sample job

class CRemoteCgiJob : public IWorkerNodeJob
{
public:
    CRemoteCgiJob(const IWorkerNodeInitContext& context);

    virtual ~CRemoteCgiJob() {} 

    int Do(CWorkerNodeJobContext& context) 
    {
        if (context.IsLogRequested()) {
            LOG_POST("Job " << context.GetJobKey() + " input: " +
                context.GetJobInput());
        }

        string tmp_path = m_Params.GetTempDir();
        if (!tmp_path.empty()) {
            CFastLocalTime lt;
            tmp_path += CDirEntry::GetPathSeparator() + 
                context.GetQueueName() + "_"  + context.GetJobKey() + "_" +
                NStr::UIntToString((unsigned int)lt.GetLocalTime().GetTimeT());
        }

        string job_wdir = tmp_path.empty() ? CDir::GetCwd() : tmp_path;

        CCgiRequest request;
        request.Deserialize(context.GetIStream(), CCgiRequest::fIgnoreQueryString |
                                                  CCgiRequest::fDoNotParseContent);

        CCgiEnvHolder env(m_Params, request.GetEnvironment());       
        vector<string> args;
        
        CNcbiStrstream err;
        CNcbiStrstream str_in;
        CNcbiIstream* in = request.GetInputStream();
        if ( !in )
            in = &str_in;
        
        int ret = -1;
        bool finished_ok = ExecRemoteApp(m_Params.GetAppPath(), 
                                         args, 
                                         *in, 
                                         context.GetOStream(), 
                                         err,
                                         m_Params.CacheStdOutErr(),
                                         ret,
                                         context,
                                         m_Params.GetMaxAppRunningTime(),
                                         0,
                                         m_Params.GetKeepAlivePeriod(),
                                         tmp_path,
                                         m_Params.RemoveTempDir(),
                                         job_wdir,
                                         env.GetEnv(),
                                         m_Params.GetMonitorAppPath(),
                                         m_Params.GetMaxMonitorRunningTime(),
                                         m_Params.GetMonitorPeriod(),
                                         m_Params.GetKillTimeout());

        string stat;
        if( !finished_ok ) {
            if (context.GetShutdownLevel() == 
                CNetScheduleAdmin::eShutdownImmediate) 
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
            if (err.pcount() > 0 )
                LOG_POST("STDERR: " << err.rdbuf());

            LOG_POST("Job " << context.GetJobKey() + " " + context.GetJobOutput()
                      << stat << " Retcode: " << ret);
        }
        return ret;
    }
private:
    CRemoteAppParams m_Params;
};

CRemoteCgiJob::CRemoteCgiJob(const IWorkerNodeInitContext& context)
{
    const IRegistry& reg = context.GetConfig();
    m_Params.Load("remote_cgi", reg);

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
        m_AppCmd = reg.GetString("remote_cgi", "idle_app_cmd", "" );
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

NCBI_WORKERNODE_MAIN_EX(CRemoteCgiJob, CRemoteAppIdleTask, 1.5.0);

//NCBI_WORKERNODE_MAIN(CRemoteAppJob, 1.0.1);
