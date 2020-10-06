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

#include <cgi/ncbicgi.hpp>

#include <functional>
#include <iterator>

USING_NCBI_SCOPE;

struct IsStandard
{
    typedef string argument_type;
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
struct HasValue
{
    typedef string argument_type;
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
    CCgiEnvHolder(const CRemoteAppLauncher& remote_app_launcher,
        const CNcbiEnvironment& client_env,
        const CNetScheduleJob& job,
        const string& service_name,
        const string& queue_name);

    const char* const* GetEnv() const { return &m_Env[0]; }

private:
    list<string> m_EnvValues;
    vector<const char*> m_Env;
};

CCgiEnvHolder::CCgiEnvHolder(const CRemoteAppLauncher& remote_app_launcher,
    const CNcbiEnvironment& client_env,
    const CNetScheduleJob& job,
    const string& service_name,
    const string& queue_name)
{
    list<string> cln_names;
    client_env.Enumerate(cln_names);
    list<string> names(cln_names.begin(), cln_names.end());

    names.erase(remove_if(names.begin(), names.end(),
            not1(IsStandard())), names.end());

    names.erase(remove_if(names.begin(), names.end(),
            HasValue<list<string> >(remote_app_launcher.GetExcludedEnv())),
            names.end());

    list<string> inc_names(cln_names.begin(), cln_names.end());

    inc_names.erase(remove_if(inc_names.begin(),inc_names.end(),
        not1(HasValue<list<string> >(remote_app_launcher.GetIncludedEnv()))),
            inc_names.end());

    names.insert(names.begin(),inc_names.begin(), inc_names.end());
    const CRemoteAppLauncher::TEnvMap& added_env =
            remote_app_launcher.GetAddedEnv();
    ITERATE(CRemoteAppLauncher::TEnvMap, it, added_env) {
        m_EnvValues.push_back(it->first + "=" +it->second);
    }

    ITERATE(list<string>, it, names) {
        if (added_env.find(*it) == added_env.end())
            m_EnvValues.push_back(*it + "=" + client_env.Get(*it));
    }

    list<string> local_names;
    const CNcbiEnvironment& local_env = remote_app_launcher.GetLocalEnv();
    local_env.Enumerate(local_names);
    ITERATE(list<string>, it, local_names) {
        const string& s = *it;
        if (added_env.find(s) == added_env.end()
            && find(names.begin(), names.end(), s) == names.end())
            m_EnvValues.push_back(s + "=" + local_env.Get(s));
    }

    m_EnvValues.push_back("NCBI_NS_SERVICE=" + service_name);
    m_EnvValues.push_back("NCBI_NS_QUEUE=" + queue_name);
    m_EnvValues.push_back("NCBI_NS_JID=" + job.job_id);
    m_EnvValues.push_back("NCBI_JOB_AFFINITY=" + job.affinity);

    if (!job.client_ip.empty())
        m_EnvValues.push_back("NCBI_LOG_CLIENT_IP=" + job.client_ip);

    if (!job.session_id.empty())
        m_EnvValues.push_back("NCBI_LOG_SESSION_ID=" + job.session_id);

    if (!job.page_hit_id.empty())
        m_EnvValues.push_back("NCBI_LOG_HIT_ID=" + job.page_hit_id);

    ITERATE(list<string>, it, m_EnvValues) {
        m_Env.push_back(it->c_str());
    }
    m_Env.push_back(NULL);
}


///////////////////////////////////////////////////////////////////////

class CRemoteCgiJob : public IWorkerNodeJob
{
public:
    CRemoteCgiJob(const IWorkerNodeInitContext& context,
            const CRemoteAppLauncher& remote_app_launcher);

    virtual ~CRemoteCgiJob() {}

    int Do(CWorkerNodeJobContext& context);

private:
    const CRemoteAppLauncher& m_RemoteAppLauncher;
};

int CRemoteCgiJob::Do(CWorkerNodeJobContext& context)
{
    if (context.IsLogRequested()) {
        LOG_POST(Note << "Job " << context.GetJobKey() + " input: " +
            context.GetJobInput());
    }

    unique_ptr<CCgiRequest> request;

    try {
        request.reset(new CCgiRequest(context.GetIStream(),
            CCgiRequest::fIgnoreQueryString |
            CCgiRequest::fDoNotParseContent));
    }
    catch (exception&) {
        ERR_POST("Cannot deserialize remote_cgi job");
        context.CommitJobWithFailure(
            "Error while parsing CGI request stream");
        return -1;
    }

    CCgiEnvHolder env(m_RemoteAppLauncher,
            request->GetEnvironment(),
            context.GetJob(),
            context.GetWorkerNode().GetServiceName(),
            context.GetQueueName());
    vector<string> args;

    CNcbiOstrstream err;
    CNcbiStrstream str_in;
    CNcbiIstream* in = request->GetInputStream();
    if (!in)
        in = &str_in;

    int ret = -1;
    bool finished_ok = m_RemoteAppLauncher.ExecRemoteApp(args,
                                        *in,
                                        context.GetOStream(),
                                        err,
                                        ret,
                                        context,
                                        0,
                                        env.GetEnv());

    m_RemoteAppLauncher.FinishJob(finished_ok, ret, context);

    if (context.IsLogRequested()) {
        if ( !IsOssEmpty(err) )
            LOG_POST(Note << "STDERR: " << (string)CNcbiOstrstreamToString(err));

        LOG_POST(Note << "Job " << context.GetJobKey() <<
            " is " << context.GetCommitStatusDescription(
                    context.GetCommitStatus()) <<
            ". Exit code: " << ret <<
            "; output: " << context.GetJobOutput());
    }

    return ret;
}

CRemoteCgiJob::CRemoteCgiJob(const IWorkerNodeInitContext&,
        const CRemoteAppLauncher& remote_app_launcher) :
    m_RemoteAppLauncher(remote_app_launcher)
{
    CGridGlobals::GetInstance().SetReuseJobObject(true);
}

#define GRID_APP_NAME "remote_cgi"
extern const char kGridAppName[] = GRID_APP_NAME;

using TRemoteAppJobFactory = CRemoteAppJobFactory<CRemoteCgiJob, CRemoteAppBaseListener, kGridAppName>;

int main(int argc, const char* argv[])
{
    GRID_APP_CHECK_VERSION_ARGS();
    return Main<TRemoteAppJobFactory, CRemoteAppBaseListener>(argc, argv);
}
