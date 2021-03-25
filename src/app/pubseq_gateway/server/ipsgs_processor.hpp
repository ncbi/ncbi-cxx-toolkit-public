#ifndef IPSGS_PROCESSOR__HPP
#define IPSGS_PROCESSOR__HPP

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
 * File Description: PSG processor interface
 *
 */

#include "psgs_request.hpp"
#include "psgs_reply.hpp"

USING_NCBI_SCOPE;


/// Interface class (and self-factory) for request processor objects that can
/// retrieve data from a given data source.
/// The overal life cycle of the processors is as follows.
/// There is a one-time processors registration stage. On this stage a default
/// processor constructor will be used.
/// Then at the time when a request comes, all the registred processors will
/// receive the CreateProcessor(...) call. All not NULL processors will be
/// considered as those which are able to process the request.
/// Later the infrastructure will call the created processors Process() method
/// in parallel and periodically will call GetStatus() method. When all
/// processors finished it is considered as the end of the request processing.
///
/// There are a few agreements for the processors.
/// - The server replies use PSG protocol. When something is needed to be sent
///   to the client then m_Reply method should be used.
/// - When a processor is finished it should call the
///   SignalProcessorFinished() method.
/// - If a processor needs to do any logging then two thing need to be done:
///   - Set request context fot the current thread.
///   - Use one of the macro PSG_TRACE, PSG_INFO, PSG_WARNING, PSG_ERROR,
///     PSG_CRITICAL, PSG_MESSAGE (pubseq_gateway_logging.hpp)
///   - Reset request context
///   E.g.:
///   { CRequestContextResetter     context_resetter;
///     m_Request->SetRequestContext();
///     ...
///     PSG_WARNING("Something"); }
/// - The ProcessEvents() method can be called periodically (in addition to
///   some events like Cassandra data ready)
class IPSGS_Processor
{
public:
    /// The GetStatus() method returns a processor current status.
    enum EPSGS_Status {
        ePSGS_InProgress,   //< Processor is still working.
        ePSGS_Found,        //< Processor finished and found what needed.
        ePSGS_NotFound,     //< Processor finished and did not find anything.
        ePSGS_Error,        //< Processor finished and there was an error.
        ePSGS_Cancelled     //< Processor finished because earlier it received
                            //< the Cancel() call.
    };

    /// Converts the processor status to a string for tracing and logging
    /// purposes.
    static string  StatusToString(EPSGS_Status  status);

public:
    IPSGS_Processor() :
        m_FinishSignalled(false)
    {}

    virtual ~IPSGS_Processor()
    {}

public:
    /// Create processor to fulfil PSG request using the data source
    /// @param request
    ///  PSG request to retrieve the data for. It is guaranteed to be not null.
    /// @param reply
    ///  The way to send reply chunks to the client. It is guaranteed to
    ///  be not null.
    /// @return
    ///  New processor object if this processor can theoretically fulfill
    ///  (all or a part of) the request; else NULL.
    virtual IPSGS_Processor* CreateProcessor(shared_ptr<CPSGS_Request> request,
                                             shared_ptr<CPSGS_Reply> reply,
                                             TProcessorPriority  priority) const = 0;

    /// Main processing function
    /// @throw
    ///  On error
    virtual void Process(void) = 0;

    /// The infrastructure request to cancel processing
    virtual void Cancel(void) = 0;

    /// Tells the processor status (if it has finished or in progress)
    /// @return
    ///  the current processor status
    virtual EPSGS_Status GetStatus(void) = 0;

    /// Tells the processor name (used in logging and tracing)
    /// @return
    ///  the processor name
    virtual string GetName(void) const = 0;

    /// Called when an event happened which may require to have
    /// some processing. By default nothing should be done.
    /// This method can be called as well on a timer event.
    virtual void ProcessEvent(void)
    {}

    /// Provides the user request
    /// @return
    ///  User request
    shared_ptr<CPSGS_Request> GetRequest(void) const
    {
        return m_Request;
    }

    /// Provides the reply wrapper
    /// @return
    ///  Reply wrapper which lets to send reply chunks to the user
    shared_ptr<CPSGS_Reply> GetReply(void) const
    {
        return m_Reply;
    }

    /// Provides the processor priority
    /// @return
    ///  The processor priority
    TProcessorPriority GetPriority(void) const
    {
        return m_Priority;
    }

public:
    /// Tells wether to continue or not after a processor called
    /// SignalStartProcessing() method.
    enum EPSGS_StartProcessing {
        ePSGS_Proceed,
        ePSGS_Cancel
    };

    /// A processor should call the method when it decides that it
    /// successfully started processing the request. The other processors
    /// which are handling this request in parallel will be cancelled.
    /// @return
    ///  The flag to continue or to stop further activity
    EPSGS_StartProcessing SignalStartProcessing(void);

    /// A processor should call this method when it decides that there is
    /// nothing else to be done.
    void SignalFinishProcessing(void);

protected:
    shared_ptr<CPSGS_Request>  m_Request;
    shared_ptr<CPSGS_Reply>    m_Reply;
    TProcessorPriority         m_Priority;

protected:
    bool        m_FinishSignalled;
};

#endif  // IPSGS_PROCESSOR__HPP

