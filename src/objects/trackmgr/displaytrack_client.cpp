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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors: Peter Meric
 *
 * File Description:  NetSchedule grid client for TrackManager display-track request/reply
 *
 */

#include <ncbi_pch.hpp>
#include <objects/trackmgr/trackmgr__.hpp>
#include <objects/trackmgr/displaytrack_client.hpp>
#include <serial/rpcbase.hpp>


BEGIN_NCBI_SCOPE


CTMS_DisplayTrack_Client::CTMS_DisplayTrack_Client(const string& NS_service,
                                                   const string& NS_queue,
                                                   const string& client_name,
                                                   const string& NC_registry_section
                                                  )
    : TBaseClient(NS_service, NS_queue, client_name, NC_registry_section)
{
}

CTMS_DisplayTrack_Client::CTMS_DisplayTrack_Client(const string& NS_registry_section,
                                                   const string& NC_registry_section
                                                  )
    : TBaseClient(NS_registry_section, NC_registry_section)
{
}

CTMS_DisplayTrack_Client::CTMS_DisplayTrack_Client(const HttpService&& http_svc)
    : m_Http_svc(http_svc.service),
      m_Http_session(new CHttpSession())
{
}

CTMS_DisplayTrack_Client
CTMS_DisplayTrack_Client::CreateServiceClient(const string& http_svc)
{
    return CTMS_DisplayTrack_Client{HttpService{http_svc}};
}

bool
CTMS_DisplayTrack_Client::FetchRawStream(CNcbiIstream& requeststr, CNcbiOstream& replystr) const
{
    if (m_Http_session.NotNull()) {
        NCBI_THROW(CException, eUnknown, "FetchRawStream() does not support HTTP-based client");
    }
    try {
        const auto retval = TBaseClient::AskStream(requeststr, replystr);
        return retval.second; // timed_out
    }
    catch (const CException& e) {
        NCBI_REPORT_EXCEPTION("Exception communicating with TMS-DisplayTrack service ", e);
        return false;
    }
    NCBI_THROW(CException, eUnknown, "FetchRawStream(): unexpected code path");
}

string
RequestToString(const CTMS_DisplayTrack_Client::TRequest& request)
{
    CConn_MemoryStream mem_str;
    // need the asnbincompressed stream to be destroyed in order for stream to be flushed
    *CAsnBinCompressed::GetOStream(mem_str) << request;
    string data;
    mem_str.ToString(&data);
    ERR_POST(Trace << "encoded request " << mem_str.tellp() << " bytes");
    return data;
}

auto
CTMS_DisplayTrack_Client::x_HttpFetch(const TRequest& request) const
-> TReplyRef
{
    CPerfLogGuard pl("Fetch via HTTP");
    const CTimeout timeout{ 20, 400000 }; // 20400ms
    const auto& content_type = kEmptyStr;
    // read data from stream
    const auto reqdata = RequestToString(request);
    ERR_POST(Trace << "encoded request " << reqdata.size() << " bytes");
    // connect to http service
    const auto response = m_Http_session->Post(m_Http_svc, reqdata, content_type, timeout);
    if (!response.CanGetContentStream()) {
        pl.Post(CRequestStatus::e200_Ok, "No response");
        CConn_MemoryStream mem_str;
        NcbiStreamCopy(mem_str, response.ErrorStream());
        string err;
        mem_str.ToString(&err);
        ERR_POST(response.GetStatusText() << " -- " << err);
        return TReplyRef{};
    }
    ERR_POST(Trace << "status_code: " << response.GetStatusCode());
    auto objistr = CAsnBinCompressed::GetIStream(response.ContentStream());
    auto reply = Ref(new TReply());
    *objistr >> *reply;
    pl.Post(CRequestStatus::e200_Ok, "Fetch");
    return reply;
}

auto
CTMS_DisplayTrack_Client::Fetch(const TRequest& request) const
-> TReplyRef
{
    TReplyRef reply;
    try {
        if (m_Http_session.NotNull()) {
            return x_HttpFetch(request);
        }
        reply.Reset(new TReply());
        TBaseClient::Ask(request, reply.GetObject());
    }
    catch (const CException& e) {
        NCBI_REPORT_EXCEPTION("Exception communicating with TMS-DisplayTracks service ", e);
        reply.Reset();
    }
    return reply;
}


END_NCBI_SCOPE
