#ifndef CORELIB___PERF_LOG__HPP
#define CORELIB___PERF_LOG__HPP

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
 * Author:  Denis Vakatov, Vladimir Ivanov
 *
 *
 */

/// @file perf_log.hpp
///
///   Defines NCBI C++ API for timing-and-logging, classes, and macros.
///

#include <corelib/ncbitime.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/request_status.hpp>


/** @addtogroup Diagnostics
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
/// Forward declaration

class CPerfLogGuard;


/////////////////////////////////////////////////////////////////////////////
///
/// CPerfLogger -- 
///
/// The CPerfLogger measure time spend somewhere, executing some operation 
/// and put result to the performance log. Each measurement will result 
/// in printing a one-line record to the performance log.
/// This class is designed to measure just one operation. Each measurement
/// should be finished with calling Post() or Discard() method. You can call
/// Start() and Suspend() methods in between as many times as you want.
///
/// @attention
///   If the logging is off then neither logging nor timing will be done at all.
///   It will however check for incorrect usage and post errors, if any.
///   But if you use next construction
///     perf_logger.Post(...).Print(...)
///   that extra record will be put into the log if the logging is off.
///   Please use PERF_POST macro or PerfLogGuard class to avoid this.
/// @sa
///   PERF_POST, PERF_POST_DB, PerfLogGuard

class NCBI_XNCBI_EXPORT CPerfLogger
{
public:
    /// If to start the timing immediately
    enum EStart {
        eStart,   ///< Call Start() immediately after creating
        eSuspend  ///< Do not start timer (call Start() later)
    };

    /// Constructor. Starts the timer by default.
    CPerfLogger(EStart state = eStart);

    /// Constructor. Use the provided stopwatch to track time.
    /// Start or stop the stopwatch according to the 'state'.
    /// The same stopwatch object can be used multiple times
    /// to accumulate total time for several operations.
    /// @note
    ///   The stopwatch is not copied, so the original object must not be
    ///   destroyed while the logger is running.
    CPerfLogger(CStopWatch& stopwatch, EStart state = eStart);

    /// Activate and start (or, restart after Suspend()) the timer.
    /// @note
    ///   If the timer is already running, post an error (once).
    /// @sa Suspend
    void Start(void);

    /// Suspend the timer.
    /// Call Start() to continue to count time accured before.
    /// @sa Start
    void Suspend(void);

    /// Log the timing; stop and deactivate the timer.
    /// @param resource
    ///   Name of the resource (must be non-empty, else throws an exception).
    /// @param status
    ///   Status of the timed code.
    /// @param status_msg
    ///   Verbal description of the status of the timed code.
    /// @note
    ///   If the timer is already inactive, then post an error (once).
    /// @note
    ///   If an external stopwatch was provided, it is stopped by this method.
    /// @sa Discard
    CDiagContext_Extra Post(CRequestStatus::ECode status,
                            CTempString           resource,
                            CTempString           status_msg = CTempString());

    CDiagContext_Extra Post(int                   status,
                            CTempString           resource,
                            CTempString           status_msg = CTempString());

    /// Discard the timing results; stop and deactivate the timer.
    void Discard(void);

    /// If the timer is still active, then post an error (once).
    /// Usually each measurement should be finished with calling Post()
    /// or Discard() method.
    ~CPerfLogger();

    /// Is performance logging on, globally?
    /// Controlled by CParam(section="Log", entry="PerfLogging", default=false)
    static bool IsON(void);

    /// Turn performance logging on/off globally.
    static void SetON(bool enable = true);

private:
    bool x_CheckValidity(const CTempString& err_msg) const;
    friend class CPerfLogGuard;

private:
    auto_ptr<CStopWatch> m_StopWatchGuard; // Internal timer if auto-created.
    CStopWatch*          m_StopWatch;      // Timer (internal or provided by user)
    CStopWatch::EStart   m_TimerState;     // Internal timer state to save cycles 
    bool                 m_IsDiscarded;    // TRUE if Post() or Discard() is already called
};


/////////////////////////////////////////////////////////////////////////////

/// Convenience macro that also saves cycles when the performance logging is
/// globally turned off.
///
/// @par Usage example:
/// This example demonstrates logging a variety of performance statistics.
/// @code
/// CPerfLogger perf_logger;
/// PERF_POST(perf_logger, e200_Ok, "ApacheSlotStats",
///           .Print("total_slots",
///                  NStr::NumericToString(total))
///           .Print("free_slots",
///                  NStr::NumericToString(total ? total - used       : 0))
///           .Print("used_slots",
///                  NStr::NumericToString(total ? used               : 0))
///           .Print("used_slots_pct",
///                  NStr::NumericToString(total ? 100 * used / total : 0))
///           .Print("ratio",
///                  NStr::DoubleToString(ratio, 2))
///           .Print("penalty", m_Mode == eShmem ? "N/A" :
///                  NStr::DoubleToString(m_Penalty, 0))
///           .Print("error",
///                  NStr::BoolToString(!okay)));
/// @endcode
/// @note The status must come from enum CRequestStatus::ECode (but without
/// the class scope, which is added by the macro).
#define PERF_POST(perf_logger, status, resource, args)              \
    do { if ( CPerfLogger::IsON() )                                 \
        perf_logger.Post(CRequestStatus::status, resource) args;    \
    } while (false)


/// Adaptation for logging database performance.
///
/// @par Usage example:
/// This example demonstrates logging the results of a stored procedure call.
/// @code
/// CPerfLogger perf_logger;
/// PERF_POST_DB(perf_logger, e200_Ok,
///              "StoredProc123", "MSSQL444")
///              .Print("foo", "bar"));
/// @endcode
/// @note The status must come from enum CRequestStatus::ECode (but without
/// the class scope, which is added by the macro).
///
#define PERF_POST_DB(perf_logger, status, resource, server, args)   \
    do { if ( CPerfLogger::IsON() )                                 \
        perf_logger.Post(CRequestStatus::status, resource)          \
                   .Print("dbserver", server) args;                 \
    } while (false)



/////////////////////////////////////////////////////////////////////////////
///
/// CPerfLogGuard -- 
///
/// @attention
///   If the logging is off then neither logging nor timing will be done at all.
///   It will however check for incorrect usage and post errors, if any.
///
///   If a usage error is encountered, then an error will be posted -- only
///   once per an error type per process.

class NCBI_XNCBI_EXPORT CPerfLogGuard
{
public:
    /// Constructor.
    /// @param resource
    ///   Name of the resource (must be non-empty, else throws an exception).
    /// @param state
    ///   Whether to start the timer by default.
    CPerfLogGuard(CTempString resource,
                  CPerfLogger::EStart state = CPerfLogger::eStart);

    /// Constructor.
    /// @param resource
    ///   Name of the resource (must be non-empty, else throws an exception).
    /// Constructor. Use the provided stopwatch to track time.
    /// @param stopwatch
    ///   User-provided stopwatch to use for tracking time.
    ///   The same stopwatch object can be used multiple times
    ///   to accumulate total time for several operations.
    /// @param state
    ///   Whether to start the timer by default.
    /// @note
    ///   The stopwatch is not copied, so the original object must not be
    ///   destroyed while the logger is running.
    CPerfLogGuard(CTempString resource,
                  CStopWatch& stopwatch,
                  CPerfLogger::EStart state = CPerfLogger::eStart);

    /// Activate and start (or, restart after Suspend()) the timer.
    /// @note
    ///   If the timer is already running, post an error (once).
    /// @sa Suspend
    void Start(void);

    /// Suspend the timer.
    /// Call Start() to continue to count time accured before.
    /// @sa Start
    void Suspend(void);

    /// Add info to the resource's description
    CPerfLogGuard& AddParameter(CTempString name, CTempString value);

    /// Write the collected resource info and timing to the log.
    /// @param status
    ///   Status of the timed code.
    /// @param status_msg
    ///   Verbal description of the status of the timed code.
    /// @note
    ///   After this any action on this guard will be an error (and no-op).
    void Post(CRequestStatus::ECode status,
              CTempString           status_msg = CTempString());

    void Post(int                   status,
              CTempString           status_msg = CTempString());

    /// Discard the results.
    /// @note
    ///   After this any action on this guard will be an error (and no-op).
    void Discard(void);

    /// If Post() or Discard() have not been called, then log the collected
    /// info with status 500.
    ~CPerfLogGuard();

private:
    CPerfLogger              m_Logger;
    string                   m_Resource;
    SDiagMessage::TExtraArgs m_Parameters;
};


/* @} */


//=============================================================================
//
//  Inline class methods
//
//=============================================================================

//
//  CPerfLogger
//

inline
CPerfLogger::CPerfLogger(EStart state)
{
    m_StopWatchGuard.reset(new CStopWatch);
    m_StopWatch = m_StopWatchGuard.get();
    m_IsDiscarded = false;
    m_TimerState  = CStopWatch::eStop;
    if ( state == eStart ) {
        Start();
    }
}


inline
CPerfLogger::CPerfLogger(CStopWatch& stopwatch, EStart state)
{
    m_StopWatch = &stopwatch;
    m_IsDiscarded = false;
    m_TimerState  = CStopWatch::eStop;
    if ( state == eStart ) {
        Start();
    }
}


inline
void CPerfLogger::Start()
{
    if ( !x_CheckValidity("Start") ) {
        return;
    }
    if ( m_TimerState == CStopWatch::eStart ) {
        ERR_POST_ONCE(Error << "CPerfLogger timer is already started");
        return;
    }
    if ( CPerfLogger::IsON() ) {
        m_StopWatch->Start();
    }
    m_TimerState = CStopWatch::eStart;
}


inline
void CPerfLogger::Suspend()
{
    if ( !x_CheckValidity("Suspend") ) {
        return;
    }
    if ( CPerfLogger::IsON() ) {
        m_StopWatch->Stop();
    }
    m_TimerState = CStopWatch::eStop;
}


inline CDiagContext_Extra
CPerfLogger::Post(CRequestStatus::ECode status,
                  CTempString           resource,
                  CTempString           status_msg)
{
    return Post((int)status, resource, status_msg);
}


inline
void CPerfLogger::Discard()
{
    // We don't need to "stop" CStopWatch here, it is nor actually running.
    m_TimerState  = CStopWatch::eStop;
    m_IsDiscarded = true;
}


inline
CPerfLogger::~CPerfLogger()
{
    if (IsON()  &&  !m_IsDiscarded  &&  m_TimerState != CStopWatch::eStop ) {
        ERR_POST_ONCE(Error << "CPerfLogger timer is still running");
    }
}


inline
bool CPerfLogger::x_CheckValidity(const CTempString& err_msg) const
{
    if ( m_IsDiscarded ) {
        ERR_POST_ONCE(Error << err_msg << "() cannot be done, " \
                      "CPerfLogger is already discarded");
        return false;
    }
    return true;
}


//
//  CPerfLogGuard
//

inline
CPerfLogGuard::CPerfLogGuard(CTempString resource, CPerfLogger::EStart state)
    : m_Logger(state), m_Resource(resource)   
{
    if ( resource.empty() ) {
        NCBI_THROW(CCoreException, eInvalidArg,
            "CPerfLogGuard:: resource name is not specified");
    }
}


inline
CPerfLogGuard::CPerfLogGuard(CTempString resource,
                             CStopWatch& stopwatch,
                             CPerfLogger::EStart state)
    : m_Logger(stopwatch, state), m_Resource(resource)   
{
    if ( resource.empty() ) {
        NCBI_THROW(CCoreException, eInvalidArg,
            "CPerfLogGuard:: resource name is not specified");
    }
}


inline
CPerfLogGuard::~CPerfLogGuard()
{
    try {
        if ( !m_Logger.m_IsDiscarded ) {
            Post(CRequestStatus::e500_InternalServerError);
        }
    } 
    catch (CCoreException&) {
    }
}


inline
void CPerfLogGuard::Start()
{
    m_Logger.Start();
}


inline
void CPerfLogGuard::Suspend()
{
    m_Logger.Suspend();
}


inline
CPerfLogGuard& CPerfLogGuard::AddParameter(CTempString name, CTempString value)
{
    if ( m_Logger.m_IsDiscarded ) {
        ERR_POST_ONCE(Error << "AddParameter() cannot be done, " \
                      "CPerfLogGuard is already discarded");
    } else {
        m_Parameters.push_back(SDiagMessage::TExtraArg(name, value));
    }
    return *this;
}


inline
void CPerfLogGuard::Post(CRequestStatus::ECode status,
                         CTempString           status_msg)
{
    return Post((int)status, status_msg);
}


inline
void CPerfLogGuard::Discard()
{
    m_Logger.Discard();
}



END_NCBI_SCOPE


#endif  /* CORELIB___PERF_LOG__HPP */
