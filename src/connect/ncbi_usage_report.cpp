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
 *   Log usage information to NCBI "pinger".
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbi_param.hpp>
#include <connect/ncbi_usage_report.hpp>
#include <atomic>
#include <sstream>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  Defaults
//
//  All parameters can be redefined in the configuration file.

// Default "pinger" CGI url for reporting statistics.
const char* kDefault_URL = "https://www.ncbi.nlm.nih.gov/stat";

// Usage logging is disabled by default.
const bool kDefault_IsEnabled = false;

// Maximum number of threads for asynchronous reporting.
const unsigned kDefault_MaxQueueSize = 100;

// Default connection timeout (negative == use Connect API settings)
const double kDefault_ConnTimeout = -1;

// Default connection max retry count (negative == use Connect API settings)
const int kDefault_ConnMaxTry = -1;

// Default Wait() timeout (negative == infinite timeout)
const double kDefault_WaitTimeout = -1;


/////////////////////////////////////////////////////////////////////////////
//  Global parameters
//

NCBI_PARAM_DECL(string,     USAGE_REPORT, URL);
NCBI_PARAM_DEF_EX(string,   USAGE_REPORT, URL, kDefault_URL, eParam_NoThread, NCBI_USAGE_REPORT_URL);

NCBI_PARAM_DECL(bool,       USAGE_REPORT, Enabled);
NCBI_PARAM_DEF_EX(bool,     USAGE_REPORT, Enabled, kDefault_IsEnabled, eParam_NoThread, NCBI_USAGE_REPORT_ENABLED);

NCBI_PARAM_DECL(string,     USAGE_REPORT, AppName);
NCBI_PARAM_DEF_EX(string,   USAGE_REPORT, AppName, "", eParam_NoThread, NCBI_USAGE_REPORT_APPNAME);

NCBI_PARAM_DECL(string,     USAGE_REPORT, AppVersion);
NCBI_PARAM_DEF_EX(string,   USAGE_REPORT, AppVersion, "", eParam_NoThread, NCBI_USAGE_REPORT_APPVERSION);

NCBI_PARAM_DECL(unsigned,   USAGE_REPORT, MaxQueueSize);
NCBI_PARAM_DEF_EX(unsigned, USAGE_REPORT, MaxQueueSize, kDefault_MaxQueueSize, eParam_NoThread, NCBI_USAGE_REPORT_MAXQUEUESIZE);

NCBI_PARAM_DECL(double,     USAGE_REPORT, ConnTimeout);
NCBI_PARAM_DEF_EX(double,   USAGE_REPORT, ConnTimeout, kDefault_ConnTimeout, eParam_NoThread, NCBI_USAGE_REPORT_CONN_TIMEOUT);

NCBI_PARAM_DECL(int,        USAGE_REPORT, ConnMaxTry);
NCBI_PARAM_DEF_EX(int,      USAGE_REPORT, ConnMaxTry, kDefault_ConnMaxTry, eParam_NoThread, NCBI_USAGE_REPORT_CONN_MAX_TRY);

NCBI_PARAM_DECL(double,     USAGE_REPORT, WaitTimeout);
NCBI_PARAM_DEF_EX(double,   USAGE_REPORT, WaitTimeout, kDefault_WaitTimeout, eParam_NoThread, NCBI_USAGE_REPORT_WAIT_TIMEOUT);



/////////////////////////////////////////////////////////////////////////////
///
/// CUsageReportAPI::
///

// Globals 

// Default parameters to report
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

static bool gs_IsEnabled = NCBI_PARAM_TYPE(USAGE_REPORT, Enabled)::GetDefault();

bool CUsageReportAPI::IsEnabled(void)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    if (!gs_IsEnabled) {
        return false;
    };
    const char* do_not_track = ::getenv("DO_NOT_TRACK");
    if (!do_not_track) {
        return true;
    }
    try {
        if (!NStr::StringToBool(do_not_track)) {
            return true;
        }
    }
    catch (CStringException&) {
    }
#endif
    return false;
}

void CUsageReportAPI::SetEnabled(bool enable)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    gs_IsEnabled = enable;
#endif
}

void CUsageReportAPI::SetURL(const string& url)
{
    NCBI_PARAM_TYPE(USAGE_REPORT, URL)::SetDefault(url);
}

string CUsageReportAPI::GetURL(void)
{
    return NCBI_PARAM_TYPE(USAGE_REPORT, URL)::GetDefault();
}

void CUsageReportAPI::SetAppName(const string& name)
{
    NCBI_PARAM_TYPE(USAGE_REPORT, AppName)::SetDefault(name);
}

string CUsageReportAPI::GetAppName(void)
{
    string name;
    CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
    if (app) {
        name = app->GetProgramDisplayName();
    }
    if (name.empty()) {
        name = NCBI_PARAM_TYPE(USAGE_REPORT, AppName)::GetDefault();
    }
    return name;
}

string CUsageReportAPI::GetAppVersion(void)
{
    string version;
    CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
    if (app) {
        version = app->GetVersion().Print();
    }
    if (version.empty()) {
        version = NCBI_PARAM_TYPE(USAGE_REPORT, AppVersion)::GetDefault();
    }
    return version;
}

void CUsageReportAPI::SetAppVersion(const string& version)
{
    NCBI_PARAM_TYPE(USAGE_REPORT, AppVersion)::SetDefault(version);
}

void CUsageReportAPI::SetAppVersion(const CVersionInfo& version)
{
    SetAppVersion(version.Print());
}

unsigned CUsageReportAPI::GetMaxQueueSize(void)
{
    return NCBI_PARAM_TYPE(USAGE_REPORT, MaxQueueSize)::GetDefault();
}

void CUsageReportAPI::SetMaxQueueSize(unsigned int n)
{
    NCBI_PARAM_TYPE(USAGE_REPORT, MaxQueueSize)::SetDefault(n ? n : kDefault_MaxQueueSize);
}

CTimeout CUsageReportAPI::GetTimeout()
{
    double v = NCBI_PARAM_TYPE(USAGE_REPORT, ConnTimeout)::GetDefault();
    if (v < 0) {
        return CTimeout(CTimeout::eDefault);
    }
    return CTimeout(v);
}

void CUsageReportAPI::SetTimeout(const CTimeout& timeout)
{
    double v;
    if (timeout.IsFinite()) {
        v = timeout.GetAsDouble();
    } 
    else if (timeout.IsDefault()) {
        v = kDefault_ConnTimeout;
    } 
    else if (timeout.IsZero()) {
        v = 0;
    }
    else {
        NCBI_THROW(CCoreException, eInvalidArg, "Only default or finite timeouts are supported");
    }
    NCBI_PARAM_TYPE(USAGE_REPORT, ConnTimeout)::SetDefault(v);
}

int CUsageReportAPI::GetRetries()
{
    return NCBI_PARAM_TYPE(USAGE_REPORT, ConnMaxTry)::GetDefault();
}

void CUsageReportAPI::SetRetries(int retries)
{
    NCBI_PARAM_TYPE(USAGE_REPORT, ConnMaxTry)::SetDefault(retries);
}

bool s_CheckConnection(const string& url)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    // Silent mode -- discard all diagnostic messages from CHttpSession during this call.
    // Affects current thread/function only.
    CDiagCollectGuard diag_guard;
    CTimeout timeout = CUsageReportAPI::GetTimeout();
    int tries = CUsageReportAPI::GetRetries();
    CHttpSession session;
    CHttpResponse response = session.Get(url, timeout, tries < 0 ? THttpRetries(null) : THttpRetries(static_cast<unsigned short>(tries)));
    return response.GetStatusCode() == 200;
#else
    return false;
#endif
}

bool CUsageReportAPI::CheckConnection()
{
    return s_CheckConnection(GetURL());
}



/////////////////////////////////////////////////////////////////////////////
///
/// CUsageReportParameters::
///

#if defined(NCBI_USAGE_REPORT_SUPPORTED)
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
#endif

CUsageReportParameters& CUsageReportParameters::Add(const string& name, const string& value)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    CHECK_USAGE_REPORT_PARAM_NAME(name);
    m_Params[NStr::URLEncode(name, NStr::eUrlEnc_URIQueryName)] = NStr::URLEncode(value, NStr::eUrlEnc_URIQueryValue);
#endif
    return *this;
}

CUsageReportParameters& CUsageReportParameters::Add(const string& name, const char* value)
{
    return Add(name, string(value));
}

string CUsageReportParameters::ToString(void) const
{
    string result;

#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    bool first = true;
    for (auto const &param : m_Params) {
        if (first) {
            first = false;
        } else {
            result += '&';
        }
        result += (param.first + '=' += param.second);
    }
#endif
    return result;
}

void CUsageReportParameters::x_CopyFrom(const CUsageReportParameters& other)
{
    m_Params = other.m_Params;
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


/////////////////////////////////////////////////////////////////////////////
///
/// Helpers
///

#if defined(NCBI_USAGE_REPORT_SUPPORTED)
static string s_GetOS(void)
{
    // Check NCBI_OS first, configure can define OS name already
#if   defined(NCBI_OS)
    return NCBI_OS;
    // Fallback, try to guess
#elif defined(_WIN32)
    return "MSWIN";         // Windows
#elif defined(__CYGWIN__)
    return "CYGWIN";        // Windows (Cygwin POSIX under Microsoft Windows)
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
#else
    return kEmptyStr;       // Unknown environment
#endif
}

static string s_GetHost(void)
{
    CDiagContext& ctx = GetDiagContext();
    return ctx.GetHost();
}
#endif


/////////////////////////////////////////////////////////////////////////////
///
/// CUsageReport::
///

// MT-protection guard for CUsageReport
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
   
CUsageReport::CUsageReport(TWhat what, const string& url, unsigned max_queue_size)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)

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
        s_AddDefaultParam(params, "os", s_GetOS());
    }
    if (what & fHost) {
        s_AddDefaultParam(params, "host", s_GetHost());
    }
    m_DefaultParams = params.ToString();

    // Save arguments
    m_URL          = url.empty() ? CUsageReportAPI::GetURL() : url;
    m_MaxQueueSize = max_queue_size ? max_queue_size : CUsageReportAPI::GetMaxQueueSize();

    // Init statictics counters
    m_CountTotal = 0;
    m_CountSent  = 0;

    // Enable reporter
    m_IsFinishing = false;
    m_IsWaiting   = false;
    m_IsEnabled   = true;

#endif
}

CUsageReport::~CUsageReport()
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    Finish();
#endif
}

bool CUsageReport::IsEnabled(void)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    return !m_IsFinishing  &&  m_IsEnabled  &&  CUsageReportAPI::IsEnabled();
#else
    return false;
#endif
}

// MT-safe
bool CUsageReport::x_Send(const string& extra_params)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    // Silent mode -- discard all diagnostic messages from CHttpSession during this call.
    // Affects current thread/function only.
    CDiagCollectGuard diag_guard;

    string url = m_URL + '?' + m_DefaultParams;
    if (!extra_params.empty()) {
        url += '&' + extra_params;
    }
    CTimeout timeout = CUsageReportAPI::GetTimeout();
    int tries = CUsageReportAPI::GetRetries();

    CHttpSession session;
    CHttpResponse response = session.Get(url, timeout, tries < 0 ? THttpRetries(null) : THttpRetries(static_cast<unsigned short>(tries)));
    return response.GetStatusCode() == 200;
#else
    return false;
#endif
}

// MT-safe
void CUsageReport::x_SendAsync(TJobPtr job_ptr)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)

    _ASSERT(job_ptr);
    MT_GUARD;

    // Check queue size
    if ((unsigned)m_Queue.size() >= m_MaxQueueSize) {
        job_ptr->x_SetState(CUsageReportJob::eRejected);
        delete job_ptr;
        return;
    } 
    // Run reporting thread if not running yet
    if (m_Thread.get_id() == std::thread::id()) {
        m_Thread = std::thread(&CUsageReport::x_ThreadHandler, std::ref(*this));
        if ( !m_Thread.joinable() ) {
            // Cannot start reporting thread, disable reporting.
            SetEnabled(false);
            ERR_POST_ONCE(Warning << "CUsageReport:: Unable to start reporting thread, reporting has disabled");
        }
    }
    // Add job to queue
    m_Queue.push_back(job_ptr);
    job_ptr->x_SetState(CUsageReportJob::eQueued);

    // Notify reporting thread that it have data to process
    m_ThreadSignal.notify_all();

#endif
}

// MT-safe
void CUsageReport::Send(void)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    if ( IsEnabled() ) {
        // Create new empty job and report it.
        // Default parameters will be reported automatically.
        x_SendAsync(new CUsageReportJob());
    }
#endif
}

// MT-safe
void CUsageReport::Send(CUsageReportParameters& params)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    if ( IsEnabled() ) {
        // Create new async job and copy parameters to it
        CUsageReportJob* job_ptr = new CUsageReportJob();
        dynamic_cast<CUsageReportParameters*>(job_ptr)->x_CopyFrom(params);
        // Report
        x_SendAsync(job_ptr);
    }
#endif
}

// MT-safe
unsigned CUsageReport::GetQueueSize(void)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    MT_GUARD;
    return (unsigned)m_Queue.size();
#else
    return 0;
#endif
}
 
// MT-safe
void CUsageReport::ClearQueue(void)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    MT_GUARD;
    x_ClearQueue();
#endif
}

// Internal version without locks
void CUsageReport::x_ClearQueue(void)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    for (auto& job : m_Queue) {
        job->x_SetState(CUsageReportJob::eCanceled);
        delete job;
    }
    m_Queue.clear();
#endif
}

// MT-safe
void CUsageReport::Wait(EWait how, CTimeout timeout)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)

    {{
        MT_GUARD;
        if (m_IsFinishing) {
            // Finishing, nothing to wait, queue is empty
            return;
        }
        if (m_IsWaiting) {
            // Wait() already happening in the another thread, do nothing
            return;
        }
        // Resolve default timeout
        if (timeout.IsDefault()) {
            double v = NCBI_PARAM_TYPE(USAGE_REPORT, WaitTimeout)::GetDefault();
            if (v < 0) {
                timeout.Set(CTimeout::eInfinite);
            } else{
                timeout.Set(v);
            }
        }
        // Set Wait mode
        m_WaitMode = how;
        m_WaitDeadline = CDeadline(timeout);
        m_IsWaiting = true;
    }}    

    while (true) {
        if (m_IsFinishing) {
            // Finishing, nothing to wait, queue is empty
            return;
        }
        // BIG FAT NOTE 1:
        //
        // Scope {{...}} below is necessary for guards to work correctly,
        // otherwise deadlock can occur (Linux, GCC).
        {{
            // BIG FAT NOTE 2:
            //
            // Signal reporting thread to unlock its wait() and process the
            // remaining queue. Ideally 'mt_signal_guard' below should
            // do this, and unlock conditional variable wait() on locking
            // signal mutex there, but sometimes that doesn't happen (???),
            // which leads to deadlock. Noticed on Linux with GCC 5.4 and 7.3.
            // Most reproducible on slow machines, where Wait() probably starts
            // earlier than the reporting thread is up and ready to process the queue.
            //
            // So, we use both, notify*() reporting thread and lock signal mutex
            // at the same time. Locking signal mutex is not really necessary
            // after that, but allows to minimize spinning in this loop,
            // and allows to check the queue between sending jobs only.
            //
            m_ThreadSignal.notify_all();
            std::lock_guard<std::mutex> mt_signal_guard(m_ThreadSignal_Mutex);

            // Check queue size
            MT_GUARD;
            if (!m_IsWaiting) {
                // Wait has been canceled in the thread handler
                return;
            }
            if (m_Queue.empty()) {
                m_IsWaiting = false;
                return;
            }
        }}
    }
#endif
}

// MT-safe
void CUsageReport::Finish(void)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    {{
        MT_GUARD;
        // Clear queue
        x_ClearQueue();
        // Signal reporting thread to terminate
        m_IsFinishing = true;
        m_ThreadSignal.notify_all();
    }}
    // Awaiting reporting thread to terminate
    if (m_Thread.joinable()) {
        m_Thread.join();
    }
#endif
}

// Should be MT-safe and no locking -- or deadlock can occur.
//
void CUsageReport::x_ThreadHandler(void)
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    std::unique_lock<std::mutex> signal_lock(m_ThreadSignal_Mutex);

    while (true) {
        // Awaiting for a signal from the main thread to proceed
        m_ThreadSignal.wait(signal_lock);

        // Process jobs in the queue

        while (true) {
            // Check on finishing flag between processing jobs
            if (m_IsFinishing) {
                return;
            }
            TJobPtr job = nullptr;
            {{
                MT_GUARD;
                // Check Wait() conditions, if any, and cancel it if required
                if (m_IsWaiting) {
                    if ((m_WaitMode == eSkipIfNoConnection  &&  m_CountTotal > 0  &&  m_CountSent == 0) ||
                        m_WaitDeadline.IsExpired() ) {
                        m_IsWaiting = false;
                        break;
                    }
                }
                // Get next job from the queue
                if (!m_Queue.empty()) {
                    if (IsEnabled()) {
                        job = m_Queue.front();
                        m_Queue.pop_front();
                    }
                    else {
                        x_ClearQueue();
                    }
                }
            }}
            if (job) {
                // Send report
                job->x_SetState(CUsageReportJob::eRunning);
                bool res = x_Send(job->ToString());
                // Increase counters
                ++m_CountTotal;
                if (res) {
                    ++m_CountSent;
                }
                // Set result state
                job->x_SetState(res ? CUsageReportJob::eCompleted : CUsageReportJob::eFailed);
            } 
            else {
                // No more jobs to send, go back to wait() a signal
                break;
            }
        }
    }
#endif
}

// MT-safe
bool CUsageReport::CheckConnection()
{
#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    return s_CheckConnection(m_URL);
#else
    return false;
#endif
}


END_NCBI_SCOPE
