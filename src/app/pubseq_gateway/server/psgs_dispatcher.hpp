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

// libuv request timer callback
void request_timer_cb(uv_timer_t *  handle);
void request_timer_close_cb(uv_handle_t *  handle);


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

    /// Return list of processors which can be used to process the request.
    /// The caller accepts the ownership.
    list<shared_ptr<IPSGS_Processor>>
        DispatchRequest(shared_ptr<CPSGS_Request> request,
                        shared_ptr<CPSGS_Reply> reply);

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

    void EraseProcessorGroup(size_t  request_id);

private:
    void x_PrintRequestStop(shared_ptr<CPSGS_Request> request,
                            CRequestStatus::ECode  status);
    CRequestStatus::ECode
    x_MapProcessorFinishToStatus(IPSGS_Processor::EPSGS_Status  status) const;
    void x_SendTrace(bool  need_trace, const string &  msg,
                     shared_ptr<CPSGS_Request> request,
                     shared_ptr<CPSGS_Reply> reply);
    void x_SendProgressMessage(IPSGS_Processor::EPSGS_Status  finish_status,
                               IPSGS_Processor *  processor,
                               shared_ptr<CPSGS_Request> request,
                               shared_ptr<CPSGS_Reply> reply);

private:
    // Registered processors
    list<unique_ptr<IPSGS_Processor>>   m_RegisteredProcessors;

private:
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

        SProcessorData(shared_ptr<IPSGS_Processor>  processor,
                       EPSGS_ProcessorStatus  dispatch_status,
                       IPSGS_Processor::EPSGS_Status  finish_status) :
            m_Processor(processor),
            m_DispatchStatus(dispatch_status),
            m_FinishStatus(finish_status)
        {}
    };

    // Auxiliary structure to store the group of processors data
    struct SProcessorGroup
    {
        list<SProcessorData>        m_Processors;
        uv_timer_t *                m_RequestTimer;
        bool                        m_TimerActive;
        // true if the reply has been already flushed and finished
        bool                        m_FlushedAndFinished;
        // A processor which has started to supply data
        IPSGS_Processor *           m_StartedProcessing;

        SProcessorGroup() :
            m_TimerActive(false), m_FlushedAndFinished(false),
            m_StartedProcessing(nullptr)
        {}

        ~SProcessorGroup()
        {
            StopRequestTimer();
        }

        void StartRequestTimer(uv_loop_t *  uv_loop,
                               uint64_t  timer_millisec,
                               size_t  request_id)
        {
            // NOTE: deallocation of memory is done in request_timer_close_cb()
            m_RequestTimer = new uv_timer_t;

            int     ret = uv_timer_init(uv_loop, m_RequestTimer);
            if (ret < 0) {
                delete m_RequestTimer;
                NCBI_THROW(CPubseqGatewayException, eInvalidTimerInit,
                           uv_strerror(ret));
            }
            m_RequestTimer->data = (void *)(request_id);

            ret = uv_timer_start(m_RequestTimer, request_timer_cb,
                                 timer_millisec, 0);
            if (ret < 0) {
                delete m_RequestTimer;
                NCBI_THROW(CPubseqGatewayException, eInvalidTimerStart,
                           uv_strerror(ret));
            }
            m_TimerActive = true;
        }

        void StopRequestTimer(void)
        {
            if (m_TimerActive) {
                m_TimerActive = false;

                int     ret = uv_timer_stop(m_RequestTimer);
                if (ret < 0) {
                    PSG_ERROR("Stop request timer error: " +
                              string(uv_strerror(ret)));
                }

                uv_close(reinterpret_cast<uv_handle_t*>(m_RequestTimer),
                         request_timer_close_cb);
            }
        }

        void RestartTimer(uint64_t  timer_millisec)
        {
            if (m_TimerActive) {
                // Consequent call just updates the timer
                int     ret = uv_timer_start(m_RequestTimer, request_timer_cb,
                                             timer_millisec, 0);
                if (ret < 0) {
                    delete m_RequestTimer;
                    NCBI_THROW(CPubseqGatewayException, eInvalidTimerStart,
                               uv_strerror(ret));
                }
            }
        }
    };

    // The dispatcher owns the created processors. The map below makes a
    // correspondance between the request id (size_t; generated in the request
    // constructor) and a list of processors with their properties.
    map<size_t, unique_ptr<SProcessorGroup>>    m_ProcessorGroups;
    mutex                                       m_GroupsLock;

    uint64_t                                    m_RequestTimeoutMillisec;

private:
    // Processor concurrency support:
    // Each group of processors may have its own limits and needs to store a
    // current number of running instances

    void x_EraseProcessorGroup(map<size_t,
                                   unique_ptr<SProcessorGroup>>::iterator  to_erase);

    struct SProcessorConcurrency
    {
        size_t                  m_Limit;
        size_t                  m_CurrentCount;
        mutex                   m_CountLock;

        SProcessorConcurrency() :
            m_Limit(0), m_CurrentCount(0)
        {}
    };

    SProcessorConcurrency       m_ProcessorConcurrency[MAX_PROCESSOR_GROUPS];
    vector<string>              m_RegisteredProcessorGroups;
};


#endif  // PSGS_DISPATCHER__HPP

