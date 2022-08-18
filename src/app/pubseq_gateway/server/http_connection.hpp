#ifndef HTTP_CONNECTION__HPP
#define HTTP_CONNECTION__HPP

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */

#include "psgs_reply.hpp"
#include "pending_operation.hpp"
#include "pubseq_gateway_logging.hpp"


// Settings for the CHttpConnection
const size_t    kHttpMaxBacklog = 1024;
const size_t    kHttpMaxRunning = 32;


class CHttpConnection
{
public:
    CHttpConnection() :
        m_IsClosed(false)
    {}

    ~CHttpConnection();

    bool IsClosed(void) const
    {
        return m_IsClosed;
    }

    void OnClientClosedConnection(void)
    {
        m_IsClosed = true;
        x_CancelAll();
    }

    void OnBeforeClosedConnection(void)
    {
        m_IsClosed = true;
        x_CancelAll();
    }

    static void s_OnBeforeClosedConnection(void *  data)
    {
        CHttpConnection *  p = static_cast<CHttpConnection*>(data);
        p->OnBeforeClosedConnection();
    }

    void PeekAsync(bool  chk_data_ready);
    void ResetForReuse(void);
    void Postpone(shared_ptr<CPSGS_Request>  request,
                  shared_ptr<CPSGS_Reply>  reply,
                  list<string>  processor_names);

    void OnTimer(void)
    {
        PeekAsync(false);
        x_MaintainFinished();
    }

private:
    volatile bool                       m_IsClosed;

    struct SRequestReplyAttributes
    {
        shared_ptr<CPSGS_Request>   m_Request;
        shared_ptr<CPSGS_Reply>     m_Reply;
        list<string>                m_PreliminaryDispatchedProcessors;
    };

    list<SRequestReplyAttributes>   m_BacklogRequests;
    list<shared_ptr<CPSGS_Reply>>   m_RunningRequests;

    void x_CancelAll(void);
    void x_CancelBacklog(void);

    using running_list_iterator_t = typename list<shared_ptr<CPSGS_Reply>>::iterator;
    using backlog_list_iterator_t = typename list<SRequestReplyAttributes>::iterator;

    void x_UnregisterRunning(running_list_iterator_t &  it);
    void x_UnregisterBacklog(backlog_list_iterator_t &  it);

    void x_RegisterPending(shared_ptr<CPSGS_Request>  request,
                           shared_ptr<CPSGS_Reply>  reply,
                           list<string>  processor_names);
    running_list_iterator_t x_Start(shared_ptr<CPSGS_Request>  request,
                                    shared_ptr<CPSGS_Reply>  reply,
                                    list<string>  processor_names);

    void x_MaintainFinished(void)
    {
        running_list_iterator_t     it = m_RunningRequests.begin();
        while (it != m_RunningRequests.end()) {
            if ((*it)->IsCompleted()) {
                auto    next = it;
                ++next;
                x_UnregisterRunning(it);
                it = next;
            } else {
                ++it;
            }
        }
    }

    void x_MaintainBacklog(void)
    {
        while (m_RunningRequests.size() < kHttpMaxRunning &&
               !m_BacklogRequests.empty()) {
            shared_ptr<CPSGS_Request>  request = m_BacklogRequests.front().m_Request;
            shared_ptr<CPSGS_Reply>    reply = m_BacklogRequests.front().m_Reply;
            list<string>               processor_names = m_BacklogRequests.front().m_PreliminaryDispatchedProcessors;

            m_BacklogRequests.pop_front();
            running_list_iterator_t     run_it = x_Start(request, reply,
                                                         move(processor_names));
            if (run_it != m_RunningRequests.end()) {
                if (reply->IsCompleted()) {
                    PSG_TRACE("Self-drained while maintaining backlog");
                    x_UnregisterRunning(run_it);
                }
            }
        }
    }
};

#endif

