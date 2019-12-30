/* $Id$
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
 * Author:  Vladislav Evgeniev, Vladimir Ivanov
 *
 * File Description:
 *   Log usage information to NCBI “pinger”.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbi_param.hpp>
#include <connect/ncbi_usage_report.hpp>
#include <sstream>
#include <atomic>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  Defaults
//
//  All parameters can be redefined in the configuration file.

// Default "pinger" CGI url for reporting statistics.
const char* kDefault_URL = "https://www.ncbi.nlm.nih.gov/stat";

// Usage logging is disabled by default.
const bool  kDefault_IsEnabled = false;

// Maximum number of threads for asynchronous reporting.
const unsigned kDefault_MaxThreads = 3;


/////////////////////////////////////////////////////////////////////////////
//  Global parameters
//

NCBI_PARAM_DECL(string,     NCBI, UsageReportURL);
NCBI_PARAM_DEF_EX(string,   NCBI, UsageReportURL, kDefault_URL, eParam_NoThread, NCBI_CONFIG__USAGE_REPORT_URL);

NCBI_PARAM_DECL(bool,       NCBI, UsageReportEnabled);
NCBI_PARAM_DEF_EX(bool,     NCBI, UsageReportEnabled, kDefault_IsEnabled, eParam_NoThread, NCBI_CONFIG__USAGE_REPORT_ENABLED);

NCBI_PARAM_DECL(string,     NCBI, UsageReportAppName);
NCBI_PARAM_DEF_EX(string,   NCBI, UsageReportAppName, "", eParam_NoThread, NCBI_CONFIG__USAGE_REPORT_APPNAME);

NCBI_PARAM_DECL(string,     NCBI, UsageReportAppVersion);
NCBI_PARAM_DEF_EX(string,   NCBI, UsageReportAppVersion, "", eParam_NoThread, NCBI_CONFIG__USAGE_REPORT_APPVERSION);

NCBI_PARAM_DECL(unsigned,   NCBI, UsageReportMaxThreads);
NCBI_PARAM_DEF_EX(unsigned, NCBI, UsageReportMaxThreads, kDefault_MaxThreads, eParam_NoThread, NCBI_CONFIG__USAGE_REPORT_MAXTHREADS);



/////////////////////////////////////////////////////////////////////////////
///
/// CUsageReportAPI::
///

// Global default parameters to report
static atomic<CUsageReport::TWhat> gs_DefaultParams(CUsageReport::fDefault);

void CUsageReportAPI::SetDefaultParameters(TWhat what)
{
    gs_DefaultParams = what;
}

CUsageReportAPI::TWhat CUsageReportAPI::GetDefaultParameters(void)
{
    return gs_DefaultParams;
}


// For speed up, we don't use MT-protection for gs_IsEnabled, even atomic<>. 
// It is a POD type, and should be save to use as is.
// All other global parameters can be used via GetDefault()/SetDefault() methods, 
// that are MT-protected. Usually they used only once per application,
// so this should not affect performance.

static bool gs_IsEnabled = NCBI_PARAM_TYPE(NCBI, UsageReportEnabled)::GetDefault();

bool CUsageReportAPI::IsEnabled()
{
    return gs_IsEnabled;
}

void CUsageReportAPI::SetEnabled(bool enable)
{
    gs_IsEnabled = enable;
}

void CUsageReportAPI::SetURL(const string& url)
{
    NCBI_PARAM_TYPE(NCBI, UsageReportURL)::SetDefault(url);
}

void CUsageReportAPI::SetAppName(const string& name)
{
    NCBI_PARAM_TYPE(NCBI, UsageReportAppName)::SetDefault(name);
}

void CUsageReportAPI::SetAppVersion(const string& version)
{
    NCBI_PARAM_TYPE(NCBI, UsageReportAppVersion)::SetDefault(version);
}

void CUsageReportAPI::SetAppVersion(const CVersionInfo& version)
{
    SetAppVersion(version.Print());
}

void CUsageReportAPI::SetMaxAsyncThreads(unsigned n)
{
    NCBI_PARAM_TYPE(NCBI, UsageReportMaxThreads)::SetDefault(n ? n : kDefault_MaxThreads);
}

string CUsageReportAPI::GetURL()
{
    return NCBI_PARAM_TYPE(NCBI, UsageReportURL)::GetDefault();
}

string CUsageReportAPI::GetAppName()
{
    string name;
    CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
    if (app) {
        name = app->GetProgramDisplayName();
    }
    if (name.empty()) {
        name = NCBI_PARAM_TYPE(NCBI, UsageReportAppName)::GetDefault();
    }
    return name;
}

string CUsageReportAPI::GetAppVersion()
{
    string version;
    CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
    if (app) {
        version = app->GetVersion().Print();
    }
    if (version.empty()) {
        version = NCBI_PARAM_TYPE(NCBI, UsageReportAppVersion)::GetDefault();
    }
    return version;
}

string CUsageReportAPI::GetOS()
{
    // Check NCBI_OS first, configure can define OS name already
#if defined(NCBI_OS)
    return NCBI_OS;
#endif
    // Fallback, try to guess
#if defined(_WIN32)
    return "MSWIN";         // Windows
#elif defined(__CYGWIN__)
    return "CYGWIN";        // Windows (Cygwin POSIX under Microsoft Window)
#elif defined(__linux__)
    return "LINUX";
#elif defined(__FreeBSD__)
    return "FREEBSD";
#elif defined(__NetBSD__)
    return "NETBSD";
#elif defined(__OpenBSD__)
    return "OPENBSD";
#elif defined(__WXOSX__)
    return "DARWIN";
#elif defined(__APPLE__)
    return "MACOS";
#elif defined(__hpux)
    return "HPUX";
#elif defined(__osf__)
    return "OSF1";
#elif defined(__sgi)
    return "IRIX";
#elif defined(_AIX)
    return "AIX";
#elif defined(__sun)
    return "SOLARIS";
#elif defined(unix) || defined(__unix__) || defined(__unix)
    return "UNIX";
#endif
    return string();        // Unknown environment
}

string CUsageReportAPI::GetHost()
{
    CDiagContext& ctx = GetDiagContext();
    return ctx.GetHost();
}

unsigned CUsageReportAPI::GetMaxAsyncThreads()
{
    return NCBI_PARAM_TYPE(NCBI, UsageReportMaxThreads)::GetDefault();
}


/////////////////////////////////////////////////////////////////////////////
///
/// CUsageReportParameters::
///

#ifdef _DEBUG
    inline void s_CheckName(const string& name)
    {
        /// Name must contain only alphanumeric chars or '_'
        for (char c : name) {
            _ASSERT(isalnum((unsigned char)c)  ||  c == '_');
        }
    }
    #define CHECK_USAGE_REPORT_PARAM_NAME s_CheckName
#else 
    #define CHECK_USAGE_REPORT_PARAM_NAME(name) 
#endif

CUsageReportParameters& CUsageReportParameters::Add(const string& name, const string& value)
{
    CHECK_USAGE_REPORT_PARAM_NAME(name);
    (*m_Params.get())[NStr::URLEncode(name, NStr::eUrlEnc_URIQueryName)] = NStr::URLEncode(value, NStr::eUrlEnc_URIQueryValue);
    return *this;
}

CUsageReportParameters& CUsageReportParameters::Add(const string& name, const char* value)
{
    return Add(name, string(value));
}

CUsageReportParameters& CUsageReportParameters::Add(const string& name, char value)
{
    return Add(name, string(1, value));
}

CUsageReportParameters& CUsageReportParameters::Add(const string& name, int value)
{
    return Add(name, NStr::IntToString(value));
}

CUsageReportParameters& CUsageReportParameters::Add(const string& name, unsigned int value)
{
    return Add(name, NStr::UIntToString(value));
}

CUsageReportParameters& CUsageReportParameters::Add(const string& name, long value)
{
    return Add(name, NStr::LongToString(value));
}

CUsageReportParameters& CUsageReportParameters::Add(const string& name, unsigned long value)
{
    return Add(name, NStr::ULongToString(value));
}

CUsageReportParameters& CUsageReportParameters::Add(const string& name, double value)

{
    return Add(name, NStr::DoubleToString(value));
}

CUsageReportParameters& CUsageReportParameters::Add(const string& name, bool value)
{
    return Add(name, NStr::BoolToString(value));
}

#if (SIZEOF_LONG != 8)
CUsageReportParameters& CUsageReportParameters::Add(const string& name, size_t value)
{
    return Add(name, NStr::SizetToString(value));
}
#endif

string CUsageReportParameters::ToString() const
{
    std::stringstream result;
    bool first = true;
    for (auto const &param : *m_Params) {
        if (first) {
            first = false;
        } else {
            result << '&';
        }
        result << param.first << '=' << param.second;
    }
    return result.str();
}

void CUsageReportParameters::x_CopyFrom(const CUsageReportParameters& other)
{
    m_Params.reset(new TParams(*other.m_Params.get()));
}

void CUsageReportParameters::x_MoveFrom(CUsageReportParameters& other)
{
    m_Params.reset(other.m_Params.release());
}


/////////////////////////////////////////////////////////////////////////////
///
/// CUsageReportJob::
///

void CUsageReportJob::x_SetState(EState state)
{
    m_State = state;
    OnStateChange(state);
}

void CUsageReportJob::x_CopyFrom(const CUsageReportJob& other)
{
    // Parent
    CUsageReportParameters::x_CopyFrom(other);
    // Members
    m_State = other.m_State;
}

void CUsageReportJob::x_MoveFrom(CUsageReportJob& other)
{
    // Parent
    CUsageReportParameters::x_MoveFrom(other);
    // Members
    m_State = other.m_State;
    other.m_State = eInvalid;
}



/////////////////////////////////////////////////////////////////////////////
///
/// CUsageReport::
///

// Usage MT-protection for CUsageReport
#define MT_GUARD std::lock_guard<std::mutex> mt_usage_guard(m_Usage_Mutex)

CUsageReport& CUsageReport::Instance(void)
{
    static CUsageReport* usage_report = new CUsageReport(gs_DefaultParams);
    return *usage_report;
}

inline
void s_AddDefaultParam(CUsageReportParameters& params, const string& name, const string& value)
{
    if (value.empty()) {
        return;
    }
    params.Add(name, value);
}
   
CUsageReport::CUsageReport(TWhat what, const string& url)
{
    // Set parameters reporting by default

    if (what == fDefault) {
        what = CUsageReportAPI::GetDefaultParameters();
    }
    // Create string with default parameters 
    CUsageReportParameters params;
    if (what & fAppName) {
        s_AddDefaultParam(params, "appname", CUsageReportAPI::GetAppName());
    }
    if (what & fAppVersion) {
        s_AddDefaultParam(params, "version", CUsageReportAPI::GetAppVersion());
    }
    if (what & fOS) {
        s_AddDefaultParam(params, "os", CUsageReportAPI::GetOS());
    }
    if (what & fAppName) {
        s_AddDefaultParam(params, "host", CUsageReportAPI::GetHost());
    }
    m_DefaultParams = params.ToString();

    // Save URL
    m_URL = url.empty() ? CUsageReportAPI::GetURL() : url;

    // Create thread pool for async reporting
    unsigned thread_pool_size = CUsageReportAPI::GetMaxAsyncThreads();
    m_ThreadPool.resize(thread_pool_size);
    
    // Enable reporter
    m_IsEnabled = true;
}

CUsageReport::~CUsageReport(void)
{
    // Wait all running async jobs (if any) to finish
    Wait();
}

bool CUsageReport::IsEnabled()
{
    return CUsageReportAPI::IsEnabled()  &&  m_IsEnabled;
}

bool CUsageReport::x_Send(const string& extra_params, int* http_status)
{
    string url = m_URL + '?' + m_DefaultParams;
    if (!extra_params.empty()) {
        url += '&' + extra_params;
    }
    CHttpSession session;
    CHttpResponse response = session.Get(url);
    if (http_status) {
        *http_status = response.GetStatusCode();
    }
    return response.GetStatusCode() == 200;
}

bool CUsageReport::Send(int* http_status)
{
    if ( !IsEnabled() ) {
        if (http_status) {
            *http_status = 0;
        }
        return false;
    }
    return x_Send(string(), http_status);
}

bool CUsageReport::Send(const CUsageReportParameters& params, int* http_status)
{
    if ( !IsEnabled() ) {
        if (http_status) {
            *http_status = 0;
        }
        return false;
    }
    return x_Send(params.ToString(), http_status);
}

// MT-safe
//
void CUsageReport::x_SendAsync(TJobPtr job)
{
    _ASSERT(job);
    MT_GUARD;

    // Thread pool: clear all finished jobs and find first available slot
    int slot = -1;
    for (int i = 0; i < (int)m_ThreadPool.size(); i++) {
        auto& t = m_ThreadPool[i];
        if (t.m_state == eFinished) {
            if (t.m_thread.joinable()) {
                t.m_thread.join();
            }
            t.m_state = eReady;
        }
        if (slot < 0  &&  t.m_state == eReady) {
            // Remember first available slot
            slot = i;
        }
    }
    // Report or reject
    if (slot >= 0) {
        // Empty slot found, run a new reporting thread
        m_ThreadPool[slot].m_state = eRunning;
        m_ThreadPool[slot].m_thread = std::thread(&CUsageReport::x_AsyncHandler, std::ref(Instance()), job, slot);
    }
    else {
        // Too much requests, rejecting
        job->x_SetState(CUsageReportJob::eRejected);
        if (job->GetOwnership() == eTakeOwnership) {
            delete job;
        }
    }
}

void CUsageReport::SendAsync(void)
{
    if ( IsEnabled() ) {
        // Create new default job
        TJobPtr job = new CUsageReportJob();
        job->x_SetOwnership(eTakeOwnership);
        job->x_SetState(CUsageReportJob::eCreated);
        // Report
        x_SendAsync(job);
    }
}

void CUsageReport::SendAsync(CUsageReportParameters& params, EOwnership own)
{
    if ( IsEnabled() ) {
        // Create new async job
        CUsageReportJob* job = new CUsageReportJob();
        job->x_SetOwnership(eTakeOwnership);
        job->x_SetState(CUsageReportJob::eCreated);
        // Copy/move parameters to new job
        if (own == eTakeOwnership) {
            dynamic_cast<CUsageReportParameters*>(job)->x_MoveFrom(params);
        } else {
            dynamic_cast<CUsageReportParameters*>(job)->x_CopyFrom(params);
        }
        // Report
        x_SendAsync(job);
    }
}

void CUsageReport::SendAsync(TJobPtr job, EOwnership own)
{
    _ASSERT(job);
    if ( IsEnabled() ) {
        job->x_SetOwnership(own);
        // Report
        x_SendAsync(job);
    }
    else {
        if (own == eTakeOwnership) {
            delete job;
        }
    }
}

void CUsageReport::Wait(void)
{
    MT_GUARD;
    // Awaiting for unfinished reporting threads
    for (auto& t : m_ThreadPool) {
        if (t.m_thread.joinable()) {
            t.m_thread.join();
            t.m_state = eReady;
        }
    }
}

// Should be MT-safe and no locking -- or deadlock can occur.
//
void CUsageReport::x_AsyncHandler(TJobPtr job, int thread_pool_slot)
{
    // Send
    job->x_SetState(CUsageReportJob::eRunning);
    int http_status;
    x_Send(job->ToString(), &http_status);

    // Set result state
    job->x_SetState(http_status == 200 ? CUsageReportJob::eCompleted : CUsageReportJob::eFailed);

    // Destroy job if necessary
    if (job->GetOwnership() == eTakeOwnership) {
        delete job;
    }
    // Report finishing state to main thread.
    // It should be MT-safe to access thread pool slot associated with 
    // the current thread, it doesn't changes while thread is executes.
    m_ThreadPool[thread_pool_slot].m_state = eFinished;
}


END_NCBI_SCOPE
