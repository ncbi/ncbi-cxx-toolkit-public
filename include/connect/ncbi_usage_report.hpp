#ifndef CONNECT___NCBI_USAGE_REPORT__HPP
#define CONNECT___NCBI_USAGE_REPORT__HPP

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
 * Authors:  Vladislav Evgeniev, Vladimir Ivanov
 *
 * File Description:
 *   Log usage information to NCBI “pinger”.
 *
 */

#include <corelib/ncbistl.hpp>
#include <corelib/ncbimisc.hpp>
#include <connect/ncbi_http_session.hpp>
#include <mutex>
#include <thread>


 /** @addtogroup ServiceSupport
 *
 * @{
 */

BEGIN_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
///
/// CUsageReportBase:: Base class to define common types for CUsageReport API.
///

class CUsageReportBase
{
public:
    /// Defines what information should be reported by default by all reporters,
    /// in addition to passed parameters in Send*() methods.
    /// @sa CUsageReportAPI::SetDefaultParameters(), CUsageReport
    enum EWhat {
        fEmpty      = 0,         ///< No defaults, all parameters should be specified
                                 ///< via CUsageReportParameters directly
        fAppName    = 1 << 1,    ///< Application name ("appname")
        fAppVersion = 1 << 2,    ///< Application version ("version")
        fOS         = 1 << 3,    ///< OS name ("os")
        fHost       = 1 << 4,    ///< Host name ("host")
        //
        fDefault    = fAppName | fAppVersion | fOS | fHost
    };
    typedef int TWhat;  ///< Binary OR of "EWhat"
};



/////////////////////////////////////////////////////////////////////////////
///
/// CUsageReportAPI:: Global settings related to the usage reporting.
///
/// All methods are static and can be called from any thread.
/// Global settings affects all newly created usage statistics reporters.
/// If you need to call some Set*() methods, this should be called before
/// creating CUsageReport objects or any CUsageReport::Instance() usage.
/// Of course you can adjust any settings later for each reporter separately.
///
/// @note
///    All methods are MT safe.
/// @note
///   The usage reporting use default connection timeouts and maximum number
///   of tries, specified by $CONN_TIMEOUT and $CONN_MAX_TRY environment
///   variables, or [CONN]TIMEOUT and [CONN]MAX_TRY registry values accordingly.

class NCBI_XCONNECT_EXPORT CUsageReportAPI : public CUsageReportBase
{
public:
    /// Enable or disable usage statistics reporting globally for all reporters.
    ///
    /// This is an only an API method that affects all reporters, 
    /// even already created. Also, you can enable/disable reporting for
    /// each reporter separately, but only if global API is enabled. 
    /// Global settings have a priority over local status of any reporter.
    ///
    /// @note
    ///   The usage reporting is off by default.
    /// @note
    ///   Can be specified thought the global parameter:
    ///   Registry file:
    ///      [NCBI]
    ///      UsageReportEnabled = true/false
    ///   Environment variable:
    ///      NCBI_CONFIG__USAGE_REPORT_ENABLED=1/0
    ///
    /// @sa Enable, Disable, CUsageReport
    ///
    static void SetEnabled(bool enable);
    static void Enable(void)  { SetEnabled(true);  };
    static void Disable(void) { SetEnabled(false); };

    /// Indicates whether global application usage statistics collection is enabled.
    /// @sa Enable, Disable
    static bool IsEnabled(void);

    /// Set default reporting parameters.
    /// 
    /// Allow to change default reporting parameters, that will be
    /// automatically added to each usage report. Should be used before
    /// creating CUsageReport object or any CUsageReport::Instance() usage.
    /// Doesn't affect already created reporters.
    ///
    /// @param what
    ///   New set of parameters, that should be reported by default
    ///   by all newly created reporters.
    /// @sa 
    ///   TWhat, CUsageReport, CUsageReportParameters 
    ///
    static void SetDefaultParameters(TWhat what = fDefault);
    static TWhat GetDefaultParameters(void);

    /// Change CGI URL for reporting usage statistics.
    ///
    /// Doesn't affect already created reporters.
    /// @note
    ///   Can be specified thought the global parameter:
    ///   Registry file:
    ///       [NCBI]
    ///       UsageReportURL = https://...
    ///   Environment variable:
    ///       NCBI_CONFIG__USAGE_REPORT_URL=https://...
    ///
    static void SetURL(const string& url);
    static string GetURL(void);

    /// Set application name for the usage reporters.
    /// 
    /// If CNcbiApplication is used, all usage reporters automatically
    /// use program name specified there, if it is not empty, and this call 
    /// doesn't have any effect. Otherwise, specified application name 
    /// will be used by all reporters, created afterwards.
    ///
    /// @note
    ///   Can be specified thought the global parameter:
    ///   Registry file:
    ///       [NCBI]
    ///       UsageReportAppName = ...
    ///   Environment variable:
    ///       NCBI_CONFIG__USAGE_REPORT_APPNAME=...
    ///
    /// @sa CNcbiApplication::GetProgramDisplayName(), GetAppName
    ///
    static void SetAppName(const string& name);
    static string GetAppName(void);

    /// Set application version for the usage reporter(s).
    /// 
    /// If CNcbiApplication is used, all usage reporters automatically
    /// use application's version specified there, if it is not empty, 
    /// and this call doesn't have any effect. Otherwise, specified value
    /// will be used by all reporters, created afterwards.
    ///
    /// @note
    ///   Can be specified thought the global parameter:
    ///   Registry file:
    ///       [NCBI]
    ///       UsageReportAppVersion = ...
    ///   Environment variable:
    ///       NCBI_CONFIG__USAGE_REPORT_APPVERSION=...
    ///
    /// @sa CNcbiApplicationAPI::GetVersion(), GetAppVersion
    ///
    static void SetAppVersion(const string& version);
    static void SetAppVersion(const CVersionInfo& version);
    static string GetAppVersion(void);

    /// Declare the maximum number of asynchronous threads per reporter.
    ///
    /// Affects all reporters created afterwards only.
    /// Used by CUsageReport::SendAsync() methods.
    ///
    /// @note
    ///   Can be specified thought the global parameter:
    ///   Registry file:
    ///       [NCBI]
    ///       UsageReportMaxThreads = ...
    ///   Environment variable:
    ///       NCBI_CONFIG__USAGE_REPORT_MAXTHREADS=...
    /// @param
    ///   Maximum number of asynchronous threads per reporter.
    ///   0 - (re)set to default value.
    /// @sa GetMaxAsyncThreads, CUsageReport::SendAsync()
    ///
    static void SetMaxAsyncThreads(unsigned n);
    static unsigned GetMaxAsyncThreads();

    // Auxiliary getters, don't change.

    static string GetOS(void);
    static string GetHost(void);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CUsageReportParameters::
///
/// Class for holding additional reporting arguments.
/// @note
///    It is not necessary to add application name, version, host or OS name
///    manually. They can be added automatically on each report.
///    See CUsageReportAPI class.
/// @note
///    This class is not MT safe, concurrent access to the same object
///    may cause data races.
///
class NCBI_XCONNECT_EXPORT CUsageReportParameters
{
public:
    // Default constructor
    CUsageReportParameters(void) : m_Params(new TParams) {};

    /// Add argument
    /// Name must contain only alphanumeric chars or '_'.
    /// Value will be automatically URL-encoded before printing.
    ///
    CUsageReportParameters& Add(const string& name, const string& value);
    CUsageReportParameters& Add(const string& name, const char*   value);
    CUsageReportParameters& Add(const string& name, char          value);
    CUsageReportParameters& Add(const string& name, int           value);
    CUsageReportParameters& Add(const string& name, unsigned int  value);
    CUsageReportParameters& Add(const string& name, long          value);
    CUsageReportParameters& Add(const string& name, unsigned long value);
    CUsageReportParameters& Add(const string& name, double        value);
    CUsageReportParameters& Add(const string& name, bool          value);
#if (SIZEOF_LONG != 8)
    CUsageReportParameters& Add(const string& name, size_t        value);
#endif

    /// Convert parameters to string. URL-encode all values.
    string ToString() const;

    /// Copy constructor.
    CUsageReportParameters(const CUsageReportParameters& other) { x_CopyFrom(other); }
    /// Move assignment operator.
    CUsageReportParameters& operator=(const CUsageReportParameters& other) { x_CopyFrom(other); return *this; }
    /// Move constructor.
    CUsageReportParameters(CUsageReportParameters&& other) { x_MoveFrom(other); }
    /// Move assignment operator.
    CUsageReportParameters& operator=(CUsageReportParameters&& other) { x_MoveFrom(other); return *this; }

protected:
    /// Copy parameters to another objects.
    void x_CopyFrom(const CUsageReportParameters& other);
    /// Move parameters to another objects.
    void x_MoveFrom(CUsageReportParameters& other);
    friend class CUsageReport;

private:
    // Pointer to stored parameters (to avoid copying on move semantics)
    using TParams = std::map<string, string>;
    std::unique_ptr<TParams> m_Params;  
};


/////////////////////////////////////////////////////////////////////////////
///
/// CUsageReportJob::
///
/// Extended version of CUsageReportParameters. Can be used the same way 
/// in regards of adding parameters, but also allow to be overloaded to
/// control reporting status for asynchronous calls.
///
/// This is a base class, that DO NOTHING on failed or rejected reports.
/// If you want some additional functionality you need to derive yours own
/// class from it and override the OnStateChange() method.
///
/// @note
///    This class is not MT safe, concurrent access to the same object
///    may cause data races.
///
/// @sa CUsageReportParameters, CUsageReport
///
class NCBI_XCONNECT_EXPORT CUsageReportJob : public CUsageReportParameters
{
public:
    /// Job state
    enum EState {
        eInvalid,    ///< Invalid (internal)
        eCreated,    ///< Created, preparing to be send
        eRunning,    ///< Ready to send
        eCompleted,  ///< Result: successfully sent
        eFailed,     ///< Result: sending failed
        eRejected    ///< Result: too many requests, all threads are busy
    };

    /// Default constructor.
    CUsageReportJob(void) : m_State(eInvalid), m_Ownership(eNoOwnership) {};
    /// Destructor
    virtual ~CUsageReportJob(void) {};
    
    /// Return current job state.
    /// @sa EState
    EState GetState() { return m_State; };

    /// Return ownership status for this object.
    /// if set to eTakeOwnership, the allocated memory will be freed automatically
    /// at the end of lifetime of this job.
    /// @sa EOwnership
    EOwnership GetOwnership() { return m_Ownership; };

    /// Callback for async reporting. Calls on each state change.
    ///
    /// CUsageReport::SendAsync() change job state during asynchronous reporting
    /// and call this method on each change. This method can be overloaded
    /// to allow checking reporting progress or failures, see EState for a list of states.
    /// @sa 
    ///   EState, CUsageReport::SendAsync()
    virtual void OnStateChange(EState state) {};

    /// Copy constructor.
    CUsageReportJob(const CUsageReportJob& other) { x_CopyFrom(other); };
    /// Copy assignment operator.
    CUsageReportJob& operator=(const CUsageReportJob& other) { x_CopyFrom(other); return *this; };
    /// Move constructor.
    CUsageReportJob(CUsageReportJob&& other) { x_MoveFrom(other); };
    /// Move assignment operator.
    CUsageReportJob& operator=(CUsageReportJob&& other) { x_MoveFrom(other); return *this; };

protected:
    /// Change current job state.
    /// Used by CUsageReport::SendAsync() only. User cannot change state directly.
    /// @sa EState, CUsageReport::SendAsync()
    void x_SetState(EState state);

    /// Set ownership status.
    /// Used by CUsageReport::SendAsync() only. User cannot change this.
    /// @sa EOwnership, CUsageReport::SendAsync()
    void x_SetOwnership(EOwnership own) { m_Ownership = own; };

    /// Copy data from 'other' job.
    void x_CopyFrom(const CUsageReportJob& other);
    /// Move data from 'other' job, 'other' became invalid.
    void x_MoveFrom(CUsageReportJob& other);

    friend class CUsageReport;

private:
    EState      m_State;      ///< Job state
    EOwnership  m_Ownership;  ///< Defines if this job objects is owned
                              ///< by CUsageReport API and memory should be freed on destruction.
};



/////////////////////////////////////////////////////////////////////////////
///
/// CUsageReport::
///
/// Usage reporter.
/// Log usage information to some CGI. CGI URL is customizable.
/// By default report to applog through stat Applog URL parser/composer:
/// https://www.ncbi.nlm.nih.gov/stat.
///
/// @note
///   We recommend to use static method CUsageReport::Instance() to get
///   a global instance of the usage reporter. It will be created on first
///   usage and can be used from any thread and place in yours code, sharing 
///   the same resources. Of course you can directly create separate reporter,
///   constructing CUsageReport class object on stack or heap. This scenario
///   can be used for synchronous reporting. Indeed, asynchronous (background)
///   reporting can be a problem, because it is impossible to control number
///   of reporting threads, and can lead to excessive usage of resources
///   if you have too many reporters.
///    
/// @note
///   Maximum number of threads for asynchronous reporting is controlled by
///   CUsageReportAPI::SetMaxAsyncThreads(). It defines the number of threads
///   per reporter! If some reporter reaches the maximum number of concurrently
///   running asynchronous threads, all new async jobs will be rejected until
///   some previously started jobs finish its work.
///
/// @note
///   If any asynchronous reporting is used, you should call Wait() method 
///   for that reporter before application exit, to allow them to finish.
///   Otherwise, running in background threads can lead to unexpected results.
///   Behavior is undefined.
///
class NCBI_XCONNECT_EXPORT CUsageReport : public CUsageReportBase
{
public:
    /// Return global instance of CUsageReport.
    ///
    /// This method allow to use a single copy of usage reporter for
    /// an application. Real initialization will be done on first call
    /// to this method. Newly created reporter inherit current CUsageReportAPI
    /// setting, like reporting URL and default parameters. You should change
    /// it if necessary before any call to Instance().
    ///
    /// @return
    ///   Global instance of the usage reporter.
    /// @sa
    ///   CUsageReportAPI
    static CUsageReport& Instance(void);

    /// Constructor.
    ///
    /// Creates new reporting instance.
    /// For general case we still recommend to use global usage reporter,
    /// one per application, accessible via CUsageReport::Instance().
    /// But, if you need to use different reporting URL or default parameters
    /// for each reporter, you can create new instance yourself with needed parameters.
    ///
    /// @param what
    ///   Set of parameters, that should be reported by default. 
    ///   If fDefault, global default API setting will be used, 
    ///   see CUsageReportAPI::SetDefaultParameters().
    /// @param url
    ///   Reporting URL. If not specified or empty, global API setting will be used, 
    ///   see CUsageReportAPI::SetURL().
    /// @sa 
    ///   TWhat, Instance(), 
    ///   CUsageReportAPI::SetDefaultParameters(), CUsageReportAPI::SetURL() 
    ///
    CUsageReport(TWhat what = fDefault, const string& url = string());

    /// Destructor.
    /// Clean up the reporter, awaiting all current reporting sessions to finish.
    virtual ~CUsageReport(void);

    /// Enable or disable usage reporter (current instance only).
    /// Note, global API settings have a priority, and if disabled, overrides
    /// enable status for the current instance.
    ///
    /// @sa CUsageReportAPI::SetEnabled(), IsEnabled()
    ///
    void SetEnabled(bool enable) { m_IsEnabled = enable; };
    void Enable(void)  { SetEnabled(true);  };
    void Disable(void) { SetEnabled(false); };

    /// Indicates whether application usage statistics collection is enabled
    /// for a current reporter instance. Takes into account local status and global
    /// API setting as well.
    /// @sa CUsageReportAPI::SetEnabled, SetEnabled
    bool IsEnabled(void);


    /////////////////////////////////////////////////////////////////////////
    // Synchronous reporting

    /// Report usage statistics (pinger, no extra parameters except defaults).
    ///
    /// Synchronous reporting. Run reporting immediately and don't return until
    /// all done. It runs in the main thread, independently of any asynchronous
    /// requests you may have.
    ///
    /// @param http_status
    ///   Optional argument to get HTTP status of the last request.
    ///   Useful for the error reporting.
    ///   Sets to 0 if reporting API is disabled.
    /// @return 
    ///   TRUE on success, FALSE otherwise.
    /// @sa 
    ///   CUsageReportParameters, CUsageReportAPI::Enable(), SendAsync()
    bool Send(int* http_status = nullptr);

    /// Report usage statistics.
    ///
    /// Synchronous reporting. Run reporting immediately and don't return until
    /// all done. It runs in the main thread, independently of any asynchronous
    /// requests you may have.
    ///
    /// @param params
    ///   Reporting parameters.
    /// @param http_status
    ///   Optional argument to get HTTP status of the last request.
    ///   Useful for the error reporting.
    ///   Sets to 0 if reporting API is disabled.
    /// @return 
    ///   TRUE on success, FALSE otherwise.
    /// @sa 
    ///   CUsageReportParameters, CUsageReportAPI::Enable()
    bool Send(const CUsageReportParameters& params, int* http_status = nullptr);


    /////////////////////////////////////////////////////////////////////////
    // Asynchronous reporting

    /// Report usage statistics asynchronously.
    ///
    /// Send usage statistics with default parameters in background,
    /// without blocking current thread execution.
    /// @sa 
    ///   CUsageReportParameters, CUsageReportAPI::Enable()
    void SendAsync(void);

    /// Report usage statistics asynchronously.
    ///
    /// Send usage statistics with specified parameters in background,
    /// without blocking current thread execution.
    /// @param params
    ///   Additional parameters to report. Default parameters can be specified in the constructor.
    /// @param own
    ///   For MT-safety reporting parameters cannot be used "as is", because it can be
    ///   accidentally changed in the middle of reporting process that can lead
    ///   to incorrect result. And it is not worth to add additional MT-protection to 
    ///   CUsageReportParameters class. So, reporting API can "copy" or "move" specified 
    ///   parameters, and 'own' argument specify what to do here:
    ///     - eTakeOwnership -- move parameters, makes 'params' object empty after this call;
    ///     - eNoOwnership   -- copy parameters, leave 'params' objects in unchanged state.
    ///   eNoOwnership is slower if you have many parameters, but it is more
    ///   convenient to use if you report almost the same set of parameters and
    ///   changes only few of them before each reporting.
    ///   The "copy" behavior is set by default.
    /// @sa 
    ///   CUsageReportParameters, TWhat, CUsageReportJob::OnStateChange(), 
    ///   Enable(), Wait()
    ///
    void SendAsync(CUsageReportParameters& params, EOwnership own = eNoOwnership);

    /// Report usage statistics asynchronously (advanced version).
    ///
    /// Send usage statistics in background without blocking current thread execution.
    ///
    /// @param job_ptr
    ///   Pointer to CUsageReportJob based object. Note, that allocated object
    ///   should exist until usage reporting will finish its job in background,
    ///   otherwise it can lead to memory corruption and unpredictable results.
    ///   See ownership parameter.
    ///   Also, you can derive your own class from CUsageReportJob if you want
    ///   to control reporting status for asynchronous reporting.
    /// @param own
    ///   Ownership of the job object. If you don't want to control lifetime 
    ///   of the job object yourself, you can allow the API to take care of it.
    ///   Usually you don't know when background reporting task will be finished,
    ///   so you can create job via new(), add parameters, and set ownership
    ///   to eTakeOwnership. The API automatically free allocated memory when
    ///   necessary. But if you want to deallocate memory yourself please use
    ///   eNoOwnership value.
    /// @sa 
    ///   CUsageReportJob, CUsageReportJob::OnStateChange(), Enable(), Wait()
    ///
    /// @code
    ///   // Usage example
    ///   unique_ptr<CUsageReportJob_or_derived_class> job(new CUsageReportJob_or_derived_class());
    ///   job->Add(...);
    ///   CUsageReport::Instance().SendAsync(job.release(), eTakeOwnership);
    /// @endcode
    ///
    void SendAsync(CUsageReportJob* job_ptr, EOwnership own);

    /// Wait all asynchronous reporting jobs to finish (if any).
    void Wait(void);

private:
    /// Send parameters string synchronously, returns HTTP status if specified. MT-safe.
    bool x_Send(const string& extra_params, int* http_status);

    using TJobPtr = CUsageReportJob*;
    /// Send job asynchronously.
    void x_SendAsync(TJobPtr job);
    /// Handler for asynchronous job reporting in the separate thread. 
    void x_AsyncHandler(TJobPtr job, int thread_pool_slot);

private:
    /// Prevent copying.
    CUsageReport(const CUsageReport&) = delete;
    CUsageReport& operator=(const CUsageReport&) = delete;
    friend class CUsageReportAPI;

private:
    mutable bool m_IsEnabled;      ///< Enable/disable status
    string       m_DefaultParams;  ///< Default parameters to report, concatenated and URL-encoded.
    string       m_URL;            ///< Reporting URL

    // Async processing

    enum EThreadState {
        eReady,
        eRunning,
        eFinished
    };
    struct SThread {
        SThread() :  m_state(eReady) {};
        std::thread  m_thread;
        EThreadState m_state;
    };
    vector<SThread> m_ThreadPool;   ///< Async thread pool
    std::mutex      m_Usage_Mutex;  ///< MT-protection
};


/////////////////////////////////////////////////////////////////////////////
///
/// Convenience macro to log "jsevent" usage statistics (asynchronously).
/// 
/// @param event
///   Value for "jsevent" parameter. Automatically set "jsevent=...".
/// @param args
///   A chain of additional parameters to report. Optional.
///   You can list as many .Add() calls here as you want, or drop this
///   parameter at all. See usage example.
/// @par Usage example:
///   This example demonstrates how to log usage, with and without arguments.
/// @code
///   NCBI_REPORT_USAGE("tools");
///   NCBI_REPORT_USAGE("tools", .Add("tool_name", "XYZ") .Add("tool_version", 1));
///   NCBI_REPORT_USAGE("tools",  
///                     .Add("tool_name", "XYZ")
///                     .Add("tool_version", 2));
/// @endcode
///
#define NCBI_REPORT_USAGE(event, ...) __NCBI_REPORT_USAGE(event, __VA_ARGS__)

#define __NCBI_REPORT_USAGE(event, args)                            \
    {                                                               \
        CUsageReport& reporter = CUsageReport::Instance();          \
        if (reporter.IsEnabled()) {                                 \
            unique_ptr<CUsageReportJob> job(new CUsageReportJob()); \
            job->Add("jsevent", (event)) args;                      \
            reporter.SendAsync(job.release(), eTakeOwnership);      \
        }                                                           \
    }

/// Enable usage statistics reporting (globally for all reporters).
///
#define NCBI_REPORT_USAGE_START  CUsageReportAPI::Enable()

/// Finishing reporting via NCBI_REPORT_USAGE and global usage reporter,
/// awaiting all asynchronous jobs to finish. Should be used before application exit.
///
#define NCBI_REPORT_USAGE_FINISH  CUsageReport::Instance().Wait()


/* @} */


END_NCBI_SCOPE

#endif /* CONNECT___NCBI_USAGE_REPORT__HPP */
