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
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
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
    ~SJsonOut();
    SJsonOut& operator<<(const CJson_Document& doc);

private:
    mutex m_Mutex;
    atomic_bool m_Printed;
    bool m_Interactive;
};

class CJsonResponse : public CJson_Document
{
public:
    template <class TItem>
    CJsonResponse(EPSG_Status status, TItem item);

    CJsonResponse(const string& id, bool result);
    CJsonResponse(const string& id, const CJson_Document& result);
    CJsonResponse(const string& id, int code, const string& message);
    CJsonResponse(const string& id, int counter);

private:
    CJsonResponse(const string& id);

    void Fill(EPSG_Status status, shared_ptr<CPSG_Reply> reply);
    void Fill(EPSG_Status status, shared_ptr<CPSG_ReplyItem> item);

    void Fill(shared_ptr<CPSG_BlobData> blob_data);
    void Fill(shared_ptr<CPSG_BlobInfo> blob_info);
    void Fill(shared_ptr<CPSG_BioseqInfo> bioseq_info);
    void Fill(shared_ptr<CPSG_NamedAnnotInfo> named_annot_info);

    template <class TItem>
    void Fill(TItem item, string type);

    CJson_Object m_JsonObj;
};

class CProcessor
{
public:
    CProcessor() : m_Stop(false) {}

    void Stop()
    {
        unique_lock<mutex> lock(m_Mutex);
        m_Stop.store(true);
        m_CV.notify_one();
        lock.unlock();
        m_Thread.join();
    }

protected:
    thread m_Thread;
    atomic_bool m_Stop;
    mutex m_Mutex;
    condition_variable m_CV;
};

class CReporter : public CProcessor
{
public:
    CReporter(SJsonOut& json_out) : m_JsonOut(json_out) {}

    void Start()
    {
        m_Thread = thread([this]() { Run(); });
    }

    template <class TItem>
    void Add(TItem item)
    {
        unique_lock<mutex> lock(m_Mutex);
        Emplace(move(item));
        m_CV.notify_one();
    }

private:
    void Run();

    void Emplace(string request)                  { m_Requests.emplace(move(request)); }
    void Emplace(shared_ptr<CPSG_Reply> reply)    {  m_Replies.emplace(move(reply));   }
    void Emplace(shared_ptr<CPSG_ReplyItem> item) {    m_Items.emplace(move(item));    }

    SJsonOut& m_JsonOut;
    queue<string> m_Requests;
    queue<shared_ptr<CPSG_Reply>> m_Replies;
    queue<shared_ptr<CPSG_ReplyItem>> m_Items;
};

class CRetriever : public CProcessor
{
public:
    CRetriever(CPSG_Queue& queue, CReporter& reporter) : m_Queue(queue), m_Reporter(reporter) {}

    void Start()
    {
        m_Thread = thread([this]() { Run(); });
    }

private:
    void Run();

    bool ShouldStop()
    {
        unique_lock<mutex> lock(m_Mutex);
        return m_Stop;
    }

    CPSG_Queue& m_Queue;
    CReporter& m_Reporter;
    list<shared_ptr<CPSG_Reply>> m_Replies;
    list<shared_ptr<CPSG_ReplyItem>> m_Items;
};

class CSender : public CProcessor
{
public:
    CSender(CPSG_Queue& queue, SJsonOut& json_out) : m_Queue(queue), m_JsonOut(json_out) {}

    void Start()
    {
        m_Thread = thread([this]() { Run(); });
    }

    void Add(shared_ptr<CPSG_Request> request)
    {
        unique_lock<mutex> lock(m_Mutex);
        m_Requests.emplace(move(request));
        m_CV.notify_one();
    }

private:
    void Run();

    CPSG_Queue& m_Queue;
    SJsonOut& m_JsonOut;
    queue<shared_ptr<CPSG_Request>> m_Requests;
};

class CProcessing
{
public:
    CProcessing() :
        m_RequestsCounter(0),
        m_JsonOut(false),
        m_Reporter(m_JsonOut),
        m_Retriever(m_Queue, m_Reporter),
        m_Sender(m_Queue, m_JsonOut)
    {}

    CProcessing(string service, bool interactive = false) :
        m_Queue(service),
        m_RequestsCounter(0),
        m_JsonOut(interactive),
        m_Reporter(m_JsonOut),
        m_Retriever(m_Queue, m_Reporter),
        m_Sender(m_Queue, m_JsonOut)
    {}

    int OneRequest(shared_ptr<CPSG_Request> request);
    int Interactive(bool echo);
    int Performance(size_t user_threads, bool raw_metrics, const string& service);
    int Testing();

    static const initializer_list<SDataFlag>& GetDataFlags();
    static const initializer_list<SInfoFlag>& GetInfoFlags();

    template <class TRequest, class TInput>
    static shared_ptr<CPSG_Request> CreateRequest(shared_ptr<void> user_context, const TInput& input);

private:
    template <class TCreateContext>
    vector<shared_ptr<CPSG_Request>> ReadCommands(TCreateContext create_context) const;

    void PerformanceReply();

    static bool ReadRequest(string& request);
    static CJson_Schema& RequestSchema();
    static CPSG_BioId::TType GetBioIdType(string type);

    static shared_ptr<CPSG_Request> CreateRequest(const string& method, shared_ptr<void> user_context,
            const CJson_ConstObject& params_obj);

    static CPSG_BioId CreateBioId(const CArgs& input);
    static CPSG_BioId CreateBioId(const CJson_ConstObject& input);

    template <class TRequest, class TInput>
    static shared_ptr<TRequest> CreateRequestImpl(shared_ptr<void> user_context, const TInput& input);

    using TSpecified = function<bool(const string&)>;

    template <class TRequest>
    static TSpecified GetSpecified(const CArgs& input);
    template <class TRequest>
    static TSpecified GetSpecified(const CJson_ConstObject& input);

    template <class TRequest>
    static void SetInclude(shared_ptr<TRequest> request, TSpecified specified);

    CPSG_Queue m_Queue;
    atomic_int m_RequestsCounter;
    SJsonOut m_JsonOut;
    CReporter m_Reporter;
    CRetriever m_Retriever;
    CSender m_Sender;
};

template <class TRequest, class TInput>
inline shared_ptr<TRequest> CProcessing::CreateRequestImpl(shared_ptr<void> user_context, const TInput& input)
{
    return make_shared<TRequest>(CreateBioId(input), move(user_context));
}

template <>
inline shared_ptr<CPSG_Request_Blob> CProcessing::CreateRequestImpl<CPSG_Request_Blob>(shared_ptr<void> user_context, const CArgs& input)
{
    const auto& id = input["ID"].AsString();
    const auto& last_modified = input["last_modified"].AsString();
    return make_shared<CPSG_Request_Blob>(id, last_modified, move(user_context));
}

template <>
inline shared_ptr<CPSG_Request_NamedAnnotInfo> CProcessing::CreateRequestImpl<CPSG_Request_NamedAnnotInfo>(shared_ptr<void> user_context, const CArgs& input)
{
    const auto& named_annots = input["na"].GetStringList();
    return make_shared<CPSG_Request_NamedAnnotInfo>(CreateBioId(input), named_annots, move(user_context));
}

template <>
inline shared_ptr<CPSG_Request_Blob> CProcessing::CreateRequestImpl<CPSG_Request_Blob>(shared_ptr<void> user_context, const CJson_ConstObject& input)
{
    auto blob_id = input["blob_id"].GetValue().GetString();
    auto last_modified = input.has("last_modified") ? input["last_modified"].GetValue().GetString() : "";
    return make_shared<CPSG_Request_Blob>(blob_id, last_modified, move(user_context));
}

template <>
inline shared_ptr<CPSG_Request_NamedAnnotInfo> CProcessing::CreateRequestImpl<CPSG_Request_NamedAnnotInfo>(shared_ptr<void> user_context, const CJson_ConstObject& input)
{
    auto na_array = input["named_annots"].GetArray();
    CPSG_Request_NamedAnnotInfo::TAnnotNames names;

    for (const auto& na : na_array) {
        names.push_back(na.GetValue().GetString());
    }

    return make_shared<CPSG_Request_NamedAnnotInfo>(CreateBioId(input), move(names), move(user_context));
}

template <class TRequest>
inline CProcessing::TSpecified CProcessing::GetSpecified(const CArgs& input)
{
    return [&](const string& name) {
        return input[name].HasValue();
    };
}

template <class TRequest>
inline CProcessing::TSpecified CProcessing::GetSpecified(const CJson_ConstObject& input)
{
    return [&](const string& name) {
        return input.has("include_data") && (name == input["include_data"].GetValue().GetString());
    };
}

template <>
inline CProcessing::TSpecified CProcessing::GetSpecified<CPSG_Request_Resolve>(const CJson_ConstObject& input)
{
    return [&](const string& name) {
        if (!input.has("include_info")) return false;

        auto include_info = input["include_info"].GetArray();
        auto equals_to = [&](const CJson_ConstNode& node) { return node.GetValue().GetString() == name; };
        return find_if(include_info.begin(), include_info.end(), equals_to) != include_info.end();
    };
}

template <class TRequest>
inline void CProcessing::SetInclude(shared_ptr<TRequest> request, TSpecified specified)
{
    for (const auto& f : GetDataFlags()) {
        if (specified(f.name)) {
            request->IncludeData(f.value);
            return;
        }
    }
}

template <>
inline void CProcessing::SetInclude<CPSG_Request_Resolve>(shared_ptr<CPSG_Request_Resolve> request, TSpecified specified)
{
    const auto& info_flags = CProcessing::GetInfoFlags();

    auto i = info_flags.begin();
    bool all_info_except = specified(i->name);
    unsigned include_info = all_info_except ? CPSG_Request_Resolve::fAllInfo : 0u;

    for (++i; i != info_flags.end(); ++i) {
        if (specified(i->name)) {
            if (all_info_except) {
                include_info &= ~i->value;
            } else {
                include_info |= i->value;
            }
        }
    }

    request->IncludeInfo(include_info);
}

template <>
inline void CProcessing::SetInclude<CPSG_Request_NamedAnnotInfo>(shared_ptr<CPSG_Request_NamedAnnotInfo>, TSpecified)
{
}

template <class TRequest, class TInput>
inline shared_ptr<CPSG_Request> CProcessing::CreateRequest(shared_ptr<void> user_context, const TInput& input)
{
    auto request = CreateRequestImpl<TRequest>(move(user_context), input);
    auto specified = GetSpecified<TRequest>(input);
    SetInclude(request, specified);
    return request;
}

END_NCBI_SCOPE

#endif
