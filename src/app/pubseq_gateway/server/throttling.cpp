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
 * Design: Denis Vakatov
 * Implementation: Sergey Satskiy
 *
 * File Description:
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbithr.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp>

#include "throttling.hpp"
#include "http_request.hpp"
#include "http_reply.hpp"
#include "http_connection.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_exception.hpp"


USING_NCBI_SCOPE;



CPubseqGatewayApp::EPSGS_ThrottlingDecision
CPubseqGatewayApp::x_CheckThrottling(CHttpRequest &  req,
                                     shared_ptr<CPSGS_Reply>  reply,
                                     const string &  peer_ip,
                                     const string &  peer_id,
                                     const string &  user_agent)
{
    static long         throttling_data_valid_ms =
                            round(m_Settings.m_ThrottlingDataValidSec * 1000);

    // Check the current number of connections against the throttling limit
    size_t current_conn_num = m_HttpDaemon->NumOfConnections();
    if (current_conn_num <= m_Settings.m_ConnThrottleThreshold) {
        return ePSGS_Continue;
    }

    // Exception: ADMIN/connections_status request
    if (req.GetPath() == "/ADMIN/connections_status") {
        return ePSGS_Continue;
    }

    m_Alerts.Register(ePSGS_Throttling, "Throttling limit has been reached");


    // Throttling data needs to be collected or used if it is up to date
    shared_ptr<SThrottlingData>     throttling_data;

    while (m_ThrottlingDataLock.exchange(true)) {}    // lock
    if (m_ThrottlingData) {
        // The throttling data already collected. Check the timestamp.
        psg_time_point_t    now = psg_clock_t::now();
        if (chrono::duration_cast<chrono::milliseconds>
                (now - m_ThrottlingDataTimestamp).count() <= throttling_data_valid_ms) {
            // The throttling data is still good; take it
            throttling_data = m_ThrottlingData;
        } else {
            // The throttling data is obsolete:
            // - recollect it or
            // - take it if someone else is collecting
            if (m_ThrottlingDataInProgress) {
                // Someone else is recollecting data so use the existing
                throttling_data = m_ThrottlingData;
            } else {
                // Nobody else is recollecting the throttling data
                x_RebuildThrottlingData();
                throttling_data = m_ThrottlingData;
            }
        }
    } else {
        // The throttling data does not exist yet. It is one of the following:
        // - the very first time => needs to collect
        // - the other is collecting so consider this as throttling data is
        //   empty
        if (m_ThrottlingDataInProgress) {
            // Another thread is collecting data; treat it as throttling data
            // is empty
            throttling_data.reset(new SThrottlingData());
        } else {
            // Recollect the throttling data
            x_RebuildThrottlingData();
            throttling_data = m_ThrottlingData;
        }
    }

    // unlock
    m_ThrottlingDataLock = false;


    // Over all idle
    for (auto &  props : throttling_data->m_IdleConnProps) {
        if (props.m_CloseIssued) {
            continue;   // Someone else has issued the close request for this connection
        }

        if (props.m_PeerID.has_value()) {
            if (find(throttling_data->m_PeerIDOverLimit.begin(),
                     throttling_data->m_PeerIDOverLimit.end(),
                     props.m_PeerID.value()) != throttling_data->m_PeerIDOverLimit.end()) {
                // Idle candidate found basing on peer ID
                props.m_CloseIssued = true;
                PSG_WARNING("Throttling connection (idle peer ID)."
                            " Worker id: " + to_string(props.m_WorkerID) +
                            " Connection id: " + to_string(props.m_ConnID));
                x_CloseThrottledConnection(reply, props.m_WorkerID, props.m_ConnID);
                return ePSGS_OtherClosed;
            }
        }

        if (find(throttling_data->m_PeerIPOverLimit.begin(),
                 throttling_data->m_PeerIPOverLimit.end(),
                  props.m_PeerIP) != throttling_data->m_PeerIPOverLimit.end()) {
            // Idle candidate found basing on peer IP
            props.m_CloseIssued = true;
            PSG_WARNING("Throttling connection (idle peer IP)."
                        " Worker id: " + to_string(props.m_WorkerID) +
                        " Connection id: " + to_string(props.m_ConnID));
            x_CloseThrottledConnection(reply, props.m_WorkerID, props.m_ConnID);
            return ePSGS_OtherClosed;
        }

        string      site = GetSiteFromIP(props.m_PeerIP);
        if (find(throttling_data->m_PeerSiteOverLimit.begin(),
                 throttling_data->m_PeerSiteOverLimit.end(),
                 site) != throttling_data->m_PeerSiteOverLimit.end()) {
            // Idle candidate found basing on peer site
            props.m_CloseIssued = true;
            PSG_WARNING("Throttling connection (idle peer site)."
                        " Worker id: " + to_string(props.m_WorkerID) +
                        " Connection id: " + to_string(props.m_ConnID));
            x_CloseThrottledConnection(reply, props.m_WorkerID, props.m_ConnID);
            return ePSGS_OtherClosed;
        }

        if (props.m_UserAgent.has_value()) {
            if (find(throttling_data->m_UserAgentOverLimit.begin(),
                     throttling_data->m_UserAgentOverLimit.end(),
                     props.m_UserAgent.value()) != throttling_data->m_UserAgentOverLimit.end()) {
                // Idle candidate found basing on user agent
                props.m_CloseIssued = true;
                PSG_WARNING("Throttling connection (idle user agent)."
                            " Worker id: " + to_string(props.m_WorkerID) +
                            " Connection id: " + to_string(props.m_ConnID));
                x_CloseThrottledConnection(reply, props.m_WorkerID, props.m_ConnID);
                return ePSGS_OtherClosed;
            }
        }
    }

    // Check if the current connection had some activity before
    SConnectionRunTimeProperties    conn_props =
        reply->GetHttpReply()->GetHttpConnection()->GetProperties();
    if (conn_props.m_NumInitiatedRequests > 1) {
        // The initiated requests are counted before the throttling procedure
        // is triggered so 1 means "this very request"
        // This connection had activity before so there is no need to do
        // throttlig; it is a 'not guilty' connection
        return ePSGS_Continue;
    }


    // Here: there is no good candidate for throttling basing on the idle
    // connection properties. Let's check the current request properties
    // against the list of identifiers over the limits

    if (!peer_id.empty()) {
        if (find(throttling_data->m_PeerIDOverLimit.begin(),
                 throttling_data->m_PeerIDOverLimit.end(),
                 peer_id) != throttling_data->m_PeerIDOverLimit.end()) {
            // The connection associated with the current request needs to be
            // closed.
            PSG_WARNING("Throttling request connection (peer id)");
            return ePSGS_CloseThis;
        }
    }

    if (find(throttling_data->m_PeerIPOverLimit.begin(),
             throttling_data->m_PeerIPOverLimit.end(),
             peer_ip) != throttling_data->m_PeerIPOverLimit.end()) {
        // The connection associated with the current request needs to be
        // closed.
        PSG_WARNING("Throttling request connection (peer ip)");
        return ePSGS_CloseThis;
    }

    string      site = GetSiteFromIP(peer_ip);
    if (find(throttling_data->m_PeerSiteOverLimit.begin(),
             throttling_data->m_PeerSiteOverLimit.end(),
             site) != throttling_data->m_PeerSiteOverLimit.end()) {
        // The connection associated with the current request needs to be
        // closed.
        PSG_WARNING("Throttling request connection (peer site)");
        return ePSGS_CloseThis;
    }

    if (!user_agent.empty()) {
        if (find(throttling_data->m_UserAgentOverLimit.begin(),
                 throttling_data->m_UserAgentOverLimit.end(),
                 user_agent) != throttling_data->m_UserAgentOverLimit.end()) {
            // The connection associated with the current request needs to be
            // closed.
            PSG_WARNING("Throttling request connection (peer user agent)");
            return ePSGS_CloseThis;
        }
    }

    // Here: no candidate for throttling was picked. Let the request go.
    return ePSGS_Continue;
}


void CPubseqGatewayApp::x_RebuildThrottlingData(void)
{
    // NOTE: it must be called with the spin lock acquired
    // It returns with the spin lock acquired as well

    m_ThrottlingDataInProgress = true;
    m_ThrottlingDataLock = false;                     // unlock

    shared_ptr<SThrottlingData>     throttling_data;
    throttling_data.reset(new SThrottlingData());
    m_HttpDaemon->PopulateThrottlingData(*throttling_data.get());
    while (m_ThrottlingDataLock.exchange(true)) {}    // lock

    m_ThrottlingDataInProgress = false;
    m_ThrottlingDataTimestamp = psg_clock_t::now();
    m_ThrottlingData = throttling_data;
}


void CPubseqGatewayApp::x_CloseThrottledConnection(shared_ptr<CPSGS_Reply>  reply,
                                                   unsigned int  worker_id,
                                                   int64_t  conn_id)
{
    // Closing some other connection so it will be done asynchronously
    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_OldConnThrottled);
    m_HttpDaemon->CloseThrottledConnection(worker_id, conn_id);
}


void CPubseqGatewayApp::x_CloseThrottledConnection(shared_ptr<CPSGS_Reply>  reply)
{
    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_NewConnThrottled);

    // Note: it is closing of a connection synchronously, i.e. there is an
    // initiated request. The request needs to be formally finished.
    // Also a PSG protocol chunk needs to be sent.

    x_SendMessageAndCompletionChunks(reply, psg_clock_t::now(),
                                     "The connection was throttled",
                                     CRequestStatus::e503_ServiceUnavailable,
                                     ePSGS_ShuttingDown,
                                     eDiag_Error);

    reply->GetHttpReply()->GetHttpConnection()->CloseThrottledConnection(
            CHttpConnection::ePSGS_SyncClose);

    // The x_SendMessageAndCompletionChunks() makes the invocations below:
    // reply->Flush(CPSGS_Reply::ePSGS_SendAndFinish);
    // reply->SetCompleted();
    // If there is no need to send a PSG protocol chunks then two statements
    // above need to be uncommented.
}

