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



/// Based on various attributes of the request: {{seq_id}}; NA name;
/// {{blob_id}}; etc (or a combination thereof)...
/// provide a list of Processors to retrieve the requested data
class CPSGS_Dispatcher
{
public:
    CPSGS_Dispatcher()
    {}

    /// Register processor (one to serve as a processor factory)
    void AddProcessor(unique_ptr<IPSGS_Processor> processor);

    /// Return list of processors which can be used to process the request.
    /// The caller accepts the ownership.
    list<IPSGS_Processor *> DispatchRequest(shared_ptr<CPSGS_Request> request,
                                            shared_ptr<CPSGS_Reply> reply);

    /// The processor signals that it is going to provide data to the client
    IPSGS_Processor::EPSGS_StartProcessing
        SignalStartProcessing(IPSGS_Processor *  processor);

    /// The processor signals that it finished one way or another; including
    /// when a processor is cancelled.
    void SignalFinishProcessing(IPSGS_Processor *  processor);

    /// An http connection can be cancelled so this method will be invoked for
    /// such a case
    void SignalConnectionCancelled(IPSGS_Processor *  processor);

private:
    void x_PrintRequestStop(shared_ptr<CPSGS_Request> request,
                            CRequestStatus::ECode  status);

private:
    // Registered processors
    list<unique_ptr<IPSGS_Processor>>   m_RegisteredProcessors;

private:
    // From the dispatcher point of view each request corresponds to a group of
    // processors and each processor can be in one of the state:
    // - created and doing what it needs (Up)
    // - the Cancel() was called for a processor (Cancelled)
    // - a processor reported that it finished (Finished)
    enum EPSGS_ProcessorStatus {
        ePSGS_Up,
        ePSGS_Cancelled,
        ePSGS_Finished
    };

    // Auxiliary structure to store a processor properties
    struct SProcessorData
    {
        // It does not owe the processor.
        // CPendingOperation owns it.
        IPSGS_Processor *               m_Processor;
        EPSGS_ProcessorStatus           m_DispatchStatus;
        IPSGS_Processor::EPSGS_Status   m_FinishStatus;

        SProcessorData(IPSGS_Processor *  processor,
                       EPSGS_ProcessorStatus  dispatch_status,
                       IPSGS_Processor::EPSGS_Status  finish_status) :
            m_Processor(processor), m_DispatchStatus(dispatch_status),
            m_FinishStatus(finish_status)
        {}
    };

    // The dispatcher owns the created processors. The map below makes a
    // correspondance between the request id (size_t; generated in the request
    // constructor) and a list of processors with their properties.
    map<size_t,
        list<SProcessorData>>       m_ProcessorGroups;
    mutex                           m_GroupsLock;
};


#endif  // PSGS_DISPATCHER__HPP

