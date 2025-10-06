#ifndef PSGS_DISPATCHER__HPP
#define PSGS_DISPATCHER__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description: PSG server request dispatcher
 *
 */

#include <list>
#include <mutex>
#include "ipsgs_processor.hpp"
#include "pubseq_gateway_logging.hpp"

// Must be more than the processor groups registered via the AddProcessor()
// call
#define MAX_PROCESSOR_GROUPS    16
#define PROC_BUCKETS            100

// libuv request timer callback
void request_timer_cb(uv_timer_t *  handle);
void request_timer_close_cb(uv_handle_t *handle);


// From the dispatcher point of view each request corresponds to a group of
// processors and each processor can be in one of the state:
// - created and doing what it needs (Up)
// - the Cancel() was called for a processor (Canceled)
// - a processor reported that it finished (Finished)
enum EPSGS_ProcessorStatus {
    ePSGS_Up,
    ePSGS_Canceled,
    ePSGS_Finished
};

// Auxiliary structure to store a processor properties
struct SProcessorData
{
    // The processor is shared between CPendingOperation and the dispatcher
    shared_ptr<IPSGS_Processor>     m_Processor;
    EPSGS_ProcessorStatus           m_DispatchStatus;
    IPSGS_Processor::EPSGS_Status   m_FinishStatus;
    bool                            m_DoneStatusRegistered;
    bool                            m_ProcPerformanceRegistered;
    IPSGS_Processor::EPSGS_Status   m_LastReportedTraceStatus;

    SProcessorData(shared_ptr<IPSGS_Processor>  processor,
                   EPSGS_ProcessorStatus  dispatch_status,
                   IPSGS_Processor::EPSGS_Status  finish_status) :
        m_Processor(processor),
        m_DispatchStatus(dispatch_status),
        m_FinishStatus(finish_status),
        m_DoneStatusRegistered(false),
        m_ProcPerformanceRegistered(false),
        m_LastReportedTraceStatus(IPSGS_Processor::ePSGS_InProgress)
    {}
};

// Auxiliary structure to store the group of processors data
struct SProcessorGroup
{
    size_t                      m_RequestId;
    vector<SProcessorData>      m_Processors;
    uv_timer_t                  m_RequestTimer;
    bool                        m_TimerActive;
    bool                        m_TimerClosed;

    // During the normal processing (no abrupt connection dropping by the
    // client) there are three conditions to delete the group. All the
    // flags below must be true when a group is deleted from memory
    bool                        m_FinallyFlushed;
    bool                        m_AllProcessorsFinished;
    bool                        m_Libh2oFinished;

    // In case of a low level close (abrupt connection dropping) there will
    // be no lib h2o finish notification and probably there will be no
    // final flush.
    bool                        m_LowLevelClose;

    // To control that the request stop is issued for this group
    bool                        m_RequestStopPrinted;

    // A processor which has started to supply data
    IPSGS_Processor *           m_StartedProcessing;

    SProcessorGroup(size_t  request_id) :
        m_RequestId(request_id),
        m_TimerActive(false), m_TimerClosed(true), m_FinallyFlushed(false),
        m_AllProcessorsFinished(false), m_Libh2oFinished(false),
        m_LowLevelClose(false),
        m_RequestStopPrinted(false),
        m_StartedProcessing(nullptr)
    {
        m_Processors.reserve(MAX_PROCESSOR_GROUPS);
    }

    ~SProcessorGroup()
    {
        if (!m_TimerClosed) {
            PSG_ERROR("The request timer (request id: " +
                      to_string(m_RequestId) +
                      ") must be stopped and its handle closed before the "
                      "processor group is destroyed");
        }
    }

    bool IsSafeToDelete(void) const
    {
        return m_TimerClosed &&
            (
               // Normal flow safe case
               (m_FinallyFlushed &&
                m_AllProcessorsFinished &&
                m_Libh2oFinished) ||
               // Abrupt connection drop case
               (m_LowLevelClose && m_AllProcessorsFinished)
            );
    }

    void StartRequestTimer(uv_loop_t *  uv_loop,
                           uint64_t  timer_millisec)
    {
        if (m_TimerActive)
            NCBI_THROW(CPubseqGatewayException, eInvalidTimerStart,
                       "Request timer cannot be started twice (request id: " +
                       to_string(m_RequestId) + ")");

        int     ret = uv_timer_init(uv_loop, &m_RequestTimer);
        if (ret < 0) {
            NCBI_THROW(CPubseqGatewayException, eInvalidTimerInit,
                       uv_strerror(ret));
        }
        m_RequestTimer.data = (void *)(m_RequestId);

        ret = uv_timer_start(&m_RequestTimer, request_timer_cb,
                             timer_millisec, 0);
        if (ret < 0) {
            NCBI_THROW(CPubseqGatewayException, eInvalidTimerStart,
                       uv_strerror(ret));
        }
        m_TimerActive = true;
        m_TimerClosed = false;
    }

    void StopRequestTimer(void)
    {
        if (m_TimerActive) {
            m_TimerActive = false;

            int     ret = uv_timer_stop(&m_RequestTimer);
            if (ret < 0) {
                PSG_ERROR("Stop request timer error: " +
                          string(uv_strerror(ret)));
            }

            uv_close(reinterpret_cast<uv_handle_t*>(&m_RequestTimer),
                     request_timer_close_cb);
        }
    }

    void RestartTimer(uint64_t  timer_millisec)
    {
        if (m_TimerActive) {
            // Consequent call just updates the timer
            int     ret = uv_timer_start(&m_RequestTimer, request_timer_cb,
                                         timer_millisec, 0);
            if (ret < 0) {
                NCBI_THROW(CPubseqGatewayException, eInvalidTimerStart,
                           uv_strerror(ret));
            }
        }
    }
};


/// Based on various attributes of the request: {{seq_id}}; NA name;
/// {{blob_id}}; etc (or a combination thereof)...
/// provide a list of Processors to retrieve the requested data
class CPSGS_Dispatcher
{
public:
    // The information that a processor has finished may come from a processor
    // itself or from the framework.
    enum EPSGS_SignalSource {
        ePSGS_Processor,
        ePSGS_Fromework
    };

    static string  SignalSourceToString(EPSGS_SignalSource  source)
    {
        switch (source) {
            case ePSGS_Processor: return "processor";
            case ePSGS_Fromework: return "framework";
            default:              break;
        }
        return "unknown";
    }

public:
    CPSGS_Dispatcher(double  request_timeout)
    {
        m_RequestTimeoutMillisec = static_cast<uint64_t>(request_timeout * 1000);
    }

    // Low level can have the pending request removed e.g. due to a canceled
    // connection. This method is used to notify the dispatcher that the
    // request is deleted and that the processors group is not needed anymore.
    void NotifyRequestFinished(size_t  request_id);

    /// Register processor (one to serve as a processor factory)
    void AddProcessor(unique_ptr<IPSGS_Processor> processor);

    /// Provides a map between a processor group name and a unique zero-based
    /// index of the group
    map<string, size_t> GetProcessorGroupToIndexMap(void) const;

    /// Return list of processor names which reported that they can process the
    /// request.
    list<string>
        PreliminaryDispatchRequest(shared_ptr<CPSGS_Request> request,
                                   shared_ptr<CPSGS_Reply> reply);

    /// Return list of processors which can be used to process the request.
    /// The caller accepts the ownership.
    list<shared_ptr<IPSGS_Processor>>
        DispatchRequest(shared_ptr<CPSGS_Request> request,
                        shared_ptr<CPSGS_Reply> reply,
                        const list<string> &  processor_names);

    /// The processor signals that it is going to provide data to the client
    IPSGS_Processor::EPSGS_StartProcessing
        SignalStartProcessing(IPSGS_Processor *  processor);

    /// The processor signals that it finished one way or another; including
    /// when a processor is canceled.
    void SignalFinishProcessing(IPSGS_Processor *  processor,
                                EPSGS_SignalSource  source);

    /// An http connection can be canceled so this method will be invoked for
    /// such a case
    void SignalConnectionCanceled(size_t  request_id);

    void CancelAll(void);

    void OnRequestTimer(size_t  request_id);
    void StartRequestTimer(size_t  request_id);

    void EraseProcessorGroup(size_t  request_id);
    void OnLibh2oFinished(size_t  request_id);
    void OnRequestTimerClose(size_t  request_id);

    map<string, size_t>  GetConcurrentCounters(void);
    map<string, pair<size_t, size_t>>  GetIPThrottlingSettings(void);
    bool IsGroupAlive(size_t  request_id);
    void PopulateStatus(CJsonNode &  status);
    void RegisterProcessorsForMomentousCounters(void);

private:
    void x_PrintRequestStop(shared_ptr<CPSGS_Request> request,
                            CRequestStatus::ECode  status,
                            size_t  bytes_sent);
    CRequestStatus::ECode
    x_MapProcessorFinishToStatus(IPSGS_Processor::EPSGS_Status  status) const;
    CRequestStatus::ECode
    x_ConcludeIDGetNARequestStatus(shared_ptr<CPSGS_Request> request,
                                   shared_ptr<CPSGS_Reply> reply,
                                   bool low_level_close);
    CRequestStatus::ECode
    x_ConcludeRequestStatus(shared_ptr<CPSGS_Request> request,
                            shared_ptr<CPSGS_Reply> reply,
                            vector<IPSGS_Processor::EPSGS_Status>  proc_statuses,
                            bool low_level_close);
    void x_SendTrace(const string &  msg,
                     shared_ptr<CPSGS_Request> request,
                     shared_ptr<CPSGS_Reply> reply);
    void x_SendProgressMessage(IPSGS_Processor::EPSGS_Status  finish_status,
                               IPSGS_Processor *  processor,
                               shared_ptr<CPSGS_Request> request,
                               shared_ptr<CPSGS_Reply> reply);
    void x_LogProcessorFinish(IPSGS_Processor *  processor);

private:
    // Registered processors
    list<unique_ptr<IPSGS_Processor>>   m_RegisteredProcessors;

private:
    // Note: the data are spread between buckets so that there is less
    // contention on the data protecting mutexes

    size_t  x_GetBucketIndex(size_t  request_id) const
    {
        return request_id % PROC_BUCKETS;
    }

    // The dispatcher shares the created processors with pending operation.
    // The map below makes a correspondance between the request id (size_t;
    // generated in the request constructor) and a list of processors with
    // their properties.
    unordered_map<size_t,
                  unique_ptr<SProcessorGroup>>      m_ProcessorGroups[PROC_BUCKETS];
    mutex                                           m_GroupsLock[PROC_BUCKETS];

    uint64_t                                        m_RequestTimeoutMillisec;

public:
    static string ProcessorStatusToString(EPSGS_ProcessorStatus  st)
    {
        switch (st) {
            case ePSGS_Up:       return "Up";
            case ePSGS_Canceled: return "Canceled";
            case ePSGS_Finished: return "Finished";
            default: ;
        }
        return "Unknown";
    }

private:
    // Processor concurrency support:
    // Each group of processors may have its own limits and needs to store a
    // current number of running instances

    void x_DecrementConcurrencyCounter(IPSGS_Processor *  processor);

    struct SProcessorConcurrency
    {
        size_t                  m_Limit;
        size_t                  m_CurrentCount;
        mutable atomic<bool>    m_CountLock;

        SProcessorConcurrency() :
            m_Limit(0), m_CurrentCount(0),
            m_CountLock(false)
        {}

        size_t GetCurrentCount(void) const
        {
            CSpinlockGuard      guard(&m_CountLock);
            return m_CurrentCount;
        }

        void IncrementCurrentCount(void)
        {
            CSpinlockGuard      guard(&m_CountLock);
            ++m_CurrentCount;
        }

        void DecrementCurrentCount(void)
        {
            CSpinlockGuard      guard(&m_CountLock);
            --m_CurrentCount;
        }
    };

    SProcessorConcurrency       m_ProcessorConcurrency[MAX_PROCESSOR_GROUPS];


    // Throttling by ip support
    // Each ip may be allowed no more then a certain number of processors
    struct SProcessorThrottling
    {
        size_t                  m_Threshold;    // 0: special value to switch off throttling
        size_t                  m_LimitByIp;
        size_t                  m_ThrottledCounter;
        mutable atomic<bool>    m_ProcCountPerIpLock;

        // ip -> processors taken
        map<string, size_t>     m_ProcCountPerIp;

        SProcessorThrottling() :
            m_Threshold(0), m_LimitByIp(0), m_ProcCountPerIpLock(false)
        {}

        bool ShouldThrottle(shared_ptr<CPSGS_Request> request,
                            size_t  current_proc_cnt)
        {
            if (m_Threshold == 0)
                return false;

            string      client_end_ip = request->GetRequestContext()->GetClientIP();
            if (client_end_ip.empty())
                return false;

            map<string, size_t>::iterator   it;
            CSpinlockGuard                  guard(&m_ProcCountPerIpLock);

            it = m_ProcCountPerIp.find(client_end_ip);
            if (it == m_ProcCountPerIp.end()) {
                m_ProcCountPerIp.insert({client_end_ip, 1});
                return false;     // At least one is always allowed
            }

            if (current_proc_cnt < m_Threshold) {
                ++it->second;
                return false;
            }

            // Threshold has been reached. Need to check the current # of proc
            // for the client ip
            if (it->second < m_LimitByIp) {
                ++it->second;
                return false;
            }

            // There will be no instantiation of the processor
            return true;
        }

        void Finished(shared_ptr<CPSGS_Request> request)
        {
            if (m_Threshold == 0)
                return;

            string      client_end_ip = request->GetRequestContext()->GetClientIP();
            if (client_end_ip.empty())
                return;

            map<string, size_t>::iterator   it;
            CSpinlockGuard                  guard(&m_ProcCountPerIpLock);

            it = m_ProcCountPerIp.find(client_end_ip);
            if (it == m_ProcCountPerIp.end())
                return;     // should not happened

            if (it->second == 1) {
                // Last instance for that ip => remove
                m_ProcCountPerIp.erase(it);
            } else {
                --it->second;
            }
        }
    };

    SProcessorThrottling        m_ProcessorThrottling[MAX_PROCESSOR_GROUPS];

    vector<string>              m_RegisteredProcessorGroups;
    map<string, string>         m_LowerCaseProcessorGroups;
};


#endif  // PSGS_DISPATCHER__HPP

