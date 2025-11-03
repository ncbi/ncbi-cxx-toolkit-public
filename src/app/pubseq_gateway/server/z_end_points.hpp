#ifndef Z_END_POINTS__HPP
#define Z_END_POINTS__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   PSG server z end points support
 *
 */


struct SCheckAttributes
{
    SCheckAttributes(bool  critical_check,
                     const string &  check_id, const string &  check_name,
                     const string &  check_description,
                     const string &  health_command, const CTimeout &  health_timeout) :
        m_CriticalCheck(critical_check),
        m_CheckId(check_id), m_CheckName(check_name),
        m_CheckDescription(check_description),
        m_HealthCommand(health_command), m_HealthTimeout(health_timeout),
        m_HttpStatus(CRequestStatus::e100_Continue)
    {}

    SCheckAttributes() :
        m_HttpStatus(CRequestStatus::e100_Continue)
    {}

    // input
    bool                    m_CriticalCheck;
    string                  m_CheckId;
    string                  m_CheckName;
    string                  m_CheckDescription;
    string                  m_HealthCommand;
    CTimeout                m_HealthTimeout;

    // output
    string                  m_Status;
    string                  m_Message;
    CRequestStatus::ECode   m_HttpStatus;
};


// Two callbacks for the internal PSG API event loop
void OnZEndPointItemComplete(EPSG_Status status,
                             const shared_ptr<CPSG_ReplyItem>& item);
void OnZEndPointReplyComplete(EPSG_Status status,
                              const shared_ptr<CPSG_Reply>& reply);


// Two callbacks to send reply in the same libuv loop as it was initiated and
// to cleanup the request associated data
void s_OnAsyncZEndPointFinilize(uv_async_t *  handle);
void s_RemoveZEndpointRequestData(uv_handle_t *  handle);


// Accumulates the results of the self requests (used in the z end points
// processing).
class CPSGS_ZEndPointRequests
{
    public:
        CPSGS_ZEndPointRequests();

        void RegisterRequest(size_t     request_id,
                             uv_loop_t *  loop,
                             shared_ptr<CPSGS_Reply>  reply,
                             bool  verbose,
                             CRef<CRequestContext> &  context,
                             const vector<SCheckAttributes> &  checks);
        void OnZEndPointRequestFinish(size_t  request_id,
                                      const string &  check_id,
                                      const string &  status,
                                      const string &  message,
                                      CRequestStatus::ECode  http_status);
        void OnFinilizeHealthZRequest(size_t  request_id);
        void OnCleanupHealthZRequest(size_t  request_id);

    private:
        struct SRequestAttributes
        {
            void Initialize(uv_loop_t *  loop,
                            bool  verbose, shared_ptr<CPSGS_Reply>  reply,
                            const CRef<CRequestContext> &  request_context,
                            const vector<SCheckAttributes> &  checks);

            CRequestStatus::ECode GetFinalHttpStatus(void) const;

            bool                        m_Verbose;
            shared_ptr<CPSGS_Reply>     m_Reply;
            CRef<CRequestContext>       m_RequestContext;
            uv_async_t                  m_FinilizeEvent;
            bool                        m_Closing;

            vector<SCheckAttributes>    m_Checks;
        };

        // request id -> request attributes
        map<size_t, SRequestAttributes> m_Requests;
        atomic<bool>                    m_RequestsLock;
};


#endif /* Z_END_POINTS__HPP */

