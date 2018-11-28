#ifndef APP__PUBSEQ_GATEWAY__CLIENT__PROCESSING_HPP
#define APP__PUBSEQ_GATEWAY__CLIENT__PROCESSING_HPP

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
 * Author: Rafael Sadyrov
 *
 */

#include <atomic>
#include <memory>
#include <queue>
#include <utility>

#include <misc/jsonwrapp/jsonwrapp.hpp>
#include <objtools/pubseq_gateway/client/psg_client.hpp>
#include <serial/objectinfo.hpp>


BEGIN_NCBI_SCOPE

struct SDataFlag
{
    const string name;
    const string desc;
    CPSG_Request_Biodata::EIncludeData value;
};

struct SInfoFlag
{
    const string name;
    const string desc;
    CPSG_Request_Resolve::EIncludeInfo value;
};

struct SJsonOut
{
    SJsonOut(bool interactive = false) : m_Printed(false), m_Interactive(interactive) {}
    SJsonOut(SJsonOut&& other) : m_Printed(other.m_Printed.load()), m_Interactive(other.m_Interactive) {}
    SJsonOut& operator=(SJsonOut&& other) { m_Printed = other.m_Printed.load(); m_Interactive = other.m_Interactive; return *this; }
    ~SJsonOut();
    void operator()(const CJson_Document& doc);

private:
    atomic_bool m_Printed;
    bool m_Interactive;
};

class CProcessing
{
public:
    CProcessing() {}
    CProcessing(string service, bool interactive = false) : m_Queue(service), m_JsonOut(interactive) {}
    CProcessing(CProcessing&& other) = default;
    CProcessing& operator=(CProcessing&& other) = default;

    int OneRequest(shared_ptr<CPSG_Request> request);
    int Interactive(bool echo);

    static CPSG_BioId::TType GetBioIdType(string type);
    static const initializer_list<SDataFlag>& GetDataFlags();
    static const initializer_list<SInfoFlag>& GetInfoFlags();

    using TSpecified = function<bool(const string&)>;

    template <class TRequest>
    static void SetInclude(TRequest request, TSpecified specified);
    static void SetInclude(shared_ptr<CPSG_Request_Resolve> request, TSpecified specified);

private:
    CJson_Document Report(string reply);
    CJson_Document Report(EPSG_Status reply_item_status, shared_ptr<CPSG_ReplyItem> reply_item);
    CJson_Document Report(shared_ptr<CPSG_BlobData> blob_data);
    CJson_Document Report(shared_ptr<CPSG_BlobInfo> blob_info);
    CJson_Document Report(shared_ptr<CPSG_BioseqInfo> bioseq_info);

    template <class TItem>
    CJson_Document ReportErrors(shared_ptr<TItem> item, string type);

    template <class TItem>
    void AddRequest(const string& id, const CJson_ConstNode& params);

    void AddRequest(string request, bool echo);
    void Error(int code, string message, const CJson_Document& req_doc);
    void SendRequests(CDeadline deadline);
    void GetReplies(CDeadline deadline);
    void MoveReplies(CDeadline deadline);

    CPSG_Queue m_Queue;
    SJsonOut m_JsonOut;
    queue<shared_ptr<CPSG_Request>> m_Requests;
    queue<string> m_RepliesRequested;
    list<shared_ptr<CPSG_Reply>> m_Replies;
    list<shared_ptr<CPSG_ReplyItem>> m_ReplyItems;
};

template <class TRequest>
void CProcessing::SetInclude(TRequest request, TSpecified specified)
{
    for (const auto& f : GetDataFlags()) {
        if (specified(f.name)) {
            request->IncludeData(f.value);
            return;
        }
    }
}

END_NCBI_SCOPE

#endif
