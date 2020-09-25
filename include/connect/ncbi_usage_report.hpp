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
 *   Log usage information to NCBI "pinger".
 *
 */

#include <corelib/ncbistl.hpp>
#include <connect/ncbi_http_session.hpp>

// API is available for MT builds only, 
// for single thread builds all API is available but disabled.
#if defined(NCBI_THREADS)
#  define NCBI_USAGE_REPORT_SUPPORTED 1
#endif

#if defined(NCBI_USAGE_REPORT_SUPPORTED)
#   include <mutex>
#   include <thread>
#   include <condition_variable>
#endif

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
    /// in addition to passed parameters for Send() method.
    /// @sa CUsageReportAPI::SetDefaultParameters(), CUsageReport
    enum EWhat {
        fNone       = 0,        ///< No defaults, all parameters should be specified
                                ///< via CUsageReportParameters directly
        fAppName    = 1 << 1,   ///< Application name ("appname")
        fAppVersion = 1 << 2,   ///< Application version ("version")
        fOS         = 1 << 3,   ///< OS name ("os")
        fHost       = 1 << 4,   ///< Host name ("host")
        //
        fDefault    = fAppName | fAppVersion | fOS
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
///   of tries specified by $CONN_TIMEOUT and $CONN_MAX_TRY environment
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
    ///      [USAGE_REPORT]
    ///      Enabled = true/false
    ///   Environment variable:
    ///      NCBI_USAGE_REPORT_ENABLED=1/0
    ///
    /// @sa SetEnabled(), CUsageReport
    ///
    static void SetEnabled(bool enable = true);

    /// Indicates whether global application usage statistics collection is enabled.
    /// @sa SetEnabled()
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
    static void  SetDefaultParameters(TWhat what = fDefault);
    static TWhat GetDefaultParameters(void);

    /// Change CGI URL for reporting usage statistics.
    ///
    /// Doesn't affect already created reporters.
    /// @note
    ///   Can be specified thought the global parameter:
    ///   Registry file:
    ///       [USAGE_REPORT]
    ///       URL = https://...
    ///   Environment variable:
    ///       NCBI_USAGE_REPORT_URL=https://...
    ///
    static void   SetURL(const string& url);
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
    ///       USAGE_REPORT]
    ///       AppName = ...
    ///   Environment variable:
    ///       NCBI_USAGE_REPORT_APPNAME=...
    ///
    /// @sa CNcbiApplication::GetProgramDisplayName(), GetAppName
    ///
    static void   SetAppName(const string& name);
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
    ///       [USAGE_REPORT]
    ///       AppVersion = ...
    ///   Environment variable:
    ///       NCBI_USAGE_REPORT_APPVERSION=...
    ///
    /// @sa CNcbiApplicationAPI::GetVersion(), GetAppVersion
    ///
    static void   SetAppVersion(const string& version);
    static void   SetAppVersion(const CVersionInfo& version);
    static string GetAppVersion(void);

    /// Declare the maximum reporting jobs queue size per reporter.
    ///
    /// Minimum value is 1. Value 0 sets queue to predefined size. 
    /// Maximum value is unspecified, but be aware that too big queue size
    /// takes more memory and affects reporting accuracy. All requests 
    /// processes on first-in-first-out base, and if queue is almost full,
    /// some newly added reports can become obsolete before the reporter
    /// can send it to server. 
    ///
    /// Affects all reporters created afterwards only.
    /// Used by CUsageReport::Send() method.
    ///
    /// @note
    ///   Can be specified thought the global parameter:
    ///   Registry file:
    ///       [USAGE_REPORT]
    ///       MaxQueueSize = ...
    ///   Environment variable:
    ///       NCBI_USAGE_REPORT_MAXQUEUESIZE=...
    /// @param
    ///   Maximum number of reporting jobs in the queue per reporter.
    ///   0 - (re)set to default value.
    /// @sa GetMaxQueueSize, CUsageReport::Send()
    ///
    static void     SetMaxQueueSize(unsigned n);
    static unsigned GetMaxQueueSize();
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
    CUsageReportParameters(void) {};

    /// Add argument
    /// Name must contain only alphanumeric chars or '_'.
    /// Value will be automatically URL-encoded before printing.
    ///
    CUsageReportParameters& Add(const string& name, const string& value);
    CUsageReportParameters& Add(const string& name, const char*   value);
    template <typename TValue>
    CUsageReportParameters& Add(const string& name, TValue value) { return Add(name, std::to_string(value)); }

    /// Convert parameters to string. URL-encode all values.
    string ToString() const;

    /// Copy constructor.
    CUsageReportParameters(const CUsageReportParameters& other) { x_CopyFrom(other); }
    /// Copy assignment operator.
    CUsageReportParameters& operator=(const CUsageReportParameters& other) { x_CopyFrom(other); return *this; }

protected:
    /// Copy parameters to another objects.
    void x_CopyFrom(const CUsageReportParameters& other);

private:
    std::map<string, string> m_Params;   ///< Stored parameters

    friend class CUsageReport;
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
/// class from it and override the OnStateChange() method and.
/// For derived class you can also need to define a copy constructor.
/// But in the case of primitive members only default copy constructor
/// should works.
///
/// @note
///    This class is not MT safe by default, concurrent access to the same
///    object may cause data races.
///
/// @sa CUsageReportParameters, CUsageReport
///
class NCBI_XCONNECT_EXPORT CUsageReportJob : public CUsageReportParameters
{
public:
    /// Job state
    enum EState {
        eCreated,    ///< Initial state, not reported to OnStateChange()
        eQueued,     ///< Added to queue (sending temporary postpones)
        eRunning,    ///< Ready to send
        eCompleted,  ///< Result: successfully sent
        eFailed,     ///< Result: send failed
        eCanceled,   ///< Result: canceled / removed from queue
        eRejected    ///< Result: rejected / too many requests
    };

    /// Default constructor.
    CUsageReportJob(void) : m_State(eCreated) {};
    /// Destructor
    virtual ~CUsageReportJob(void) {};
    
    /// Return current job state.
    /// @sa EState
    EState GetState() { return m_State; };

    /// Callback for async reporting. Calls on each state change.
    ///
    /// CUsageReport::Send() change job state during asynchronous reporting
    /// and call this method on each change. This method can be overloaded
    /// to allow checking reporting progress or failures, see EState for a list of states.
    /// @sa 
    ///   EState, CUsageReport::Send()
    virtual void OnStateChange(EState /*state*/) {};

    /// Copy constructor.
    CUsageReportJob(const CUsageReportJob& other) : CUsageReportParameters(other) { m_State = other.m_State; };
    /// Copy assignment operator.
    CUsageReportJob& operator=(const CUsageReportJob& other) { x_CopyFrom(other); return *this; };
    
protected:
    /// Set new job state.
    /// Used by CUsageReport::Send() only. User cannot change state directly.
    /// @sa EState, CUsageReport::Send()
    void x_SetState(EState state);

    /// Copy data from 'other' job.
    void x_CopyFrom(const CUsageReportJob& other);

private:
    EState m_State;  ///< Job state

    friend class CUsageReport;
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
///   constructing CUsageReport class object on stack or heap.
///    
/// @note
///   Maximum queue size for asynchronous reporting is controlled by
///   CUsageReportAPI::SetMaxQueueSize(). If reporter unable to send reports
///   as fast as it gets new request it puts received jobs into a temporary
///   queue and process it in background. If reporter is overwhelmed with
///   requests and queue reaches the maximum allowed size, all new requests
///   will be rejected until it can send some already queued jobs to server 
///  and free some space in the queue.
///
/// @note
///   It is recommended to call Finish() method for each reporter before 
///   an application exit, this allow to correctly clear queue and terminate
///   reporting thread. Also, you can call Wait() before Finish() if you want
///   to be sure that all queued jobs are processed, instead of discarding them. 
///   If you don't call Finish(), terminating running in background threads
///   can lead to unexpected results, depending on platform and used compiler,
///   behavior is undefined.
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
    /// For general case we still recommend to use a single global usage reporter,
    /// one per application, accessible via CUsageReport::Instance().
    /// But, if you need to use different reporting URL or default parameters
    /// for each reporter, you can create new instance yourself with needed arguments.
    ///
    /// @param what
    ///   Set of parameters that should be reported by default. 
    ///   If fDefault, global default API setting will be used, 
    ///   see CUsageReportAPI::SetDefaultParameters().
    /// @param url
    ///   Reporting URL. If not specified or empty, global API setting will be used, 
    ///   see CUsageReportAPI::SetURL().
    /// @param max_queue_size
    ///   Maximum reporting jobs queue size. If not specified or zero,
    ///   global API setting will be used, see CUsageReportAPI::SetMaxQueueSize().
    /// @sa 
    ///   TWhat, Instance(), 
    ///   CUsageReportAPI::SetDefaultParameters(), CUsageReportAPI::SetURL(), 
    ///   CUsageReportAPI::SetMaxQueueSize() 
    ///
    CUsageReport(TWhat what = fDefault, const string& url = string(), unsigned max_queue_size = 0);

    /// Destructor.
    /// Clean up the reporter, call Finish(), all queued and non-sent reporting
    /// jobs will be destroyed.
    /// @sa 
    ///   Wait(), Finish()
    virtual ~CUsageReport(void);

    /// Enable or disable usage reporter (current instance only).
    /// Note, global API settings have a priority, and if disabled, overrides
    /// enable status for the current instance.
    ///
    /// @sa CUsageReportAPI::SetEnabled(), IsEnabled()
    ///
    void SetEnabled(bool enable = true) { m_IsEnabled = enable; }

    /// Indicates whether application usage statistics collection is enabled
    /// for a current reporter instance. Takes into account local status and global
    /// API setting as well.
    /// @sa CUsageReportAPI::SetEnabled, SetEnabled
    bool IsEnabled(void);

    /// Report usage statistics (asynchronously), default parameters.
    ///
    /// Send usage statistics with default parameters in background,
    /// without blocking current thread execution.
    /// @sa 
    ///   CUsageReportParameters, SetEnabled(), Wait()
    void Send(void);

    /// Report usage statistics (asynchronously).
    ///
    /// Send usage statistics with specified parameters in background,
    /// without blocking current thread execution.
    /// @param params
    ///   Specifies extra parameters to report, in addition to default
    ///   parameters specified in the CUsageReport constructor.
    ///   The reporter copy parameters before asynchronously reporting
    ///   them in background, so parameters object can be freely changed or 
    ///   destroyed after this call.
    /// @note
    ///   This version do nothing on errors. If you want to control
    ///   reporting progress and results you can use Send(CUsageReportJob&)
    ///   version with your own class derived from CUsageReportJob.
    /// @sa 
    ///   CUsageReportParameters, CUsageReportJob, TWhat, SetEnabled(), Wait()
    ///
    void Send(CUsageReportParameters& params);

    /// Report usage statistics (asynchronously) (advanced version).
    ///
    /// Send usage statistics in background without blocking current thread execution.
    /// You can use this version if you want to control reporting 
    /// progress and get status of submitted reports. 
    ///
    /// @param job
    ///   Job to report, some object of type CUsageReportJob or derived from it.
    ///   CUsageReportJob do nothing if something bad happens on reporting. 
    ///   All errors are ignored and silenced. You need to derive your own class
    ///   from CUsageReportJob if you want to control reporting status or store
    ///   additional information. See CUsageReportJob description for details.
    /// @sa 
    ///   CUsageReportJob, CUsageReportJob::OnStateChange(), SetEnabled(), Wait()
    /// @code
    ///   // Usage example
    ///   CUsageReportJob_or_derived_class job;
    ///   job.Add(...);
    ///   CUsageReport::Instance().Send(job);
    /// @endcode
    ///
    template <typename TJob> void Send(TJob& job)
    { 
        if ( IsEnabled() ) {
            CUsageReportJob* job_ptr = new TJob(job);
            x_SendAsync(job_ptr);
        }
    }

    /// Get number of jobs in the queue -- number of unprocessed yet jobs.
    unsigned GetQueueSize(void);

    /// Remove all unprocessed reporting jobs from queue.
    /// @sa 
    ///   Finish(), WaitAndFinish()
    void ClearQueue(void);

    /// Wait until all queued jobs starts to process and queue is empty.
    /// Can be called before Finish() to be sure that all queued requests
    /// are processed and nothing is discarded.
    /// @note
    ///   It doesn't wait for already started job that is sending at the current moment.
    ///   You still need to call Finish().
    /// @sa 
    ///   Finish(), ClearQueue()
    void Wait(void);

    /// Finish reporting for the current reporting object.
    /// All reporting jobs in the queue will be discarded and reporting
    /// thread destroyed. If you want to wait all queued requests to finish,
    /// call Wait() just before Finish().
    /// @note
    ///   Only queued requests will be discarded. It doesn't affect already
    ///   started job, that is sending at the current moment (if any).
    ///   Reporting thread will be destroyed immediately after finishing
    ///   sending last started job.
    /// @note
    ///   The reporter become invalid after this call and shouldn't be used anymore.
    /// @sa 
    ///   ClearQueue(), Wait()
    void Finish(void);

private:
    /// Prevent copying.
    CUsageReport(const CUsageReport&) = delete;
    CUsageReport& operator=(const CUsageReport&) = delete;
    friend class CUsageReportAPI;

private:
    using TJobPtr = CUsageReportJob*;

    /// Send parameters string synchronously
    bool x_Send(const string& extra_params);
    /// Send job asynchronously
    void x_SendAsync(TJobPtr job_ptr);
    /// Handler for asynchronous job reporting 
    void x_ThreadHandler(void);
    /// Remove all unprocessed reporting jobs from queue - internal version
    void x_ClearQueue(void);

private:
    mutable bool  m_IsEnabled;      ///< Enable/disable status

#if defined(NCBI_USAGE_REPORT_SUPPORTED)
    mutable bool  m_IsFinishing;    ///< TRUE if Finish() has called and reporting thread should terminate
    string        m_DefaultParams;  ///< Default parameters to report, concatenated and URL-encoded
    string        m_URL;            ///< Reporting URL
    std::thread   m_Thread;         ///< Reporting thread
    list<TJobPtr> m_Queue;          ///< Job queue
    unsigned      m_MaxQueueSize;   ///< Maximum allowed queue size
    std::mutex    m_Usage_Mutex;    ///< MT-protection to access members

    /// Signal conditional variable for reporting thread synchronization
    std::condition_variable m_ThreadSignal;
    std::mutex m_ThreadSignal_Mutex;
#endif
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

#define __NCBI_REPORT_USAGE(event, args)                    \
    {                                                       \
        CUsageReport& reporter = CUsageReport::Instance();  \
        if (reporter.IsEnabled()) {                         \
            CUsageReportParameters params;                  \
            params.Add("jsevent", (event)) args;            \
            reporter.Send(params);                          \
        }                                                   \
    }

/// Enable usage statistics reporting (globally for all reporters).
///
#define NCBI_REPORT_USAGE_START   CUsageReportAPI::SetEnabled()

/// Wait until all reports via NCBI_REPORT_USAGE will be processed.
///
#define NCBI_REPORT_USAGE_WAIT    CUsageReport::Instance().Wait()

/// Finishing reporting via NCBI_REPORT_USAGE and global usage reporter,
///
#define NCBI_REPORT_USAGE_FINISH  CUsageReport::Instance().Finish()


/* @} */

END_NCBI_SCOPE


#endif /* CONNECT___NCBI_USAGE_REPORT__HPP */
