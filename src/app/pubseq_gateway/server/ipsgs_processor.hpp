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
/// retrieve data from a given data source
class IPSGS_Processor
{
public:
    IPSGS_Processor()
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
                                             shared_ptr<CPSGS_Reply> reply) const = 0;

    /// Main processing function
    /// @throw
    ///  On error
    virtual void Process(void) = 0;

    /// Cancel processing due to the user request
    virtual void Cancel(void) = 0;

    /// Tells if the processor did evrything needed
    /// @return
    ///  true if there is nothing else to do
    virtual bool IsFinished(void) = 0;

    /// Called when an event happened which may require to have
    /// some processing. By default nothing should be done.
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

protected:
    shared_ptr<CPSGS_Request>  m_Request;
    shared_ptr<CPSGS_Reply>    m_Reply;
};

#endif  // IPSGS_PROCESSOR__HPP

