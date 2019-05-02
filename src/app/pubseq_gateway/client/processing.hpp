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
    void Fill(shared_ptr<CPSG_SkippedBlob> skipped_blob);
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

    static CPSG_BioId GetBioId(const CArgs& input);
    static CPSG_BioId GetBioId(const CJson_ConstObject& input);

    static CPSG_BlobId GetBlobId(const CArgs& input) { return input["ID"].AsString(); }
    static CPSG_BlobId GetBlobId(const CJson_ConstObject& input) { return input["blob_id"].GetValue().GetString(); }

    static string GetLastModified(const CArgs& input) { return input["last_modified"].AsString(); }
    static string GetLastModified(const CJson_ConstObject& input) { return input.has("last_modified") ? input["last_modified"].GetValue().GetString() : ""; }

    using TSpecified = function<bool(const string&)>;

    template <class TRequest>
    static TSpecified GetSpecified(const CArgs& input);
    template <class TRequest>
    static TSpecified GetSpecified(const CJson_ConstObject& input);

    static vector<string> GetNamedAnnots(const CArgs& input) { return input["na"].GetStringList(); }
    static vector<string> GetNamedAnnots(const CJson_ConstObject& input);

    template <class TInput>
    static void CreateRequestImpl(shared_ptr<CPSG_Request_Biodata>&, shared_ptr<void>, const TInput&);
    template <class TInput>
    static void CreateRequestImpl(shared_ptr<CPSG_Request_Resolve>&, shared_ptr<void>, const TInput&);
    template <class TInput>
    static void CreateRequestImpl(shared_ptr<CPSG_Request_Blob>&, shared_ptr<void>, const TInput&);
    template <class TInput>
    static void CreateRequestImpl(shared_ptr<CPSG_Request_NamedAnnotInfo>&, shared_ptr<void>, const TInput&);

    template <class TRequest>
    static void IncludeData(shared_ptr<TRequest> request, TSpecified specified);

    static void IncludeInfo(shared_ptr<CPSG_Request_Resolve> request, TSpecified specified);

    CPSG_Queue m_Queue;
    atomic_int m_RequestsCounter;
    SJsonOut m_JsonOut;
    CReporter m_Reporter;
    CRetriever m_Retriever;
    CSender m_Sender;
};

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

template <class TInput>
inline void CProcessing::CreateRequestImpl(shared_ptr<CPSG_Request_Biodata>& request, shared_ptr<void> user_context, const TInput& input)
{
    auto bio_id = GetBioId(input);
    request = make_shared<CPSG_Request_Biodata>(move(bio_id), move(user_context));
    auto specified = GetSpecified<CPSG_Request_Biodata>(input);
    IncludeData(request, specified);
}

template <class TInput>
inline void CProcessing::CreateRequestImpl(shared_ptr<CPSG_Request_Resolve>& request, shared_ptr<void> user_context, const TInput& input)
{
    auto bio_id = GetBioId(input);
    request = make_shared<CPSG_Request_Resolve>(move(bio_id), move(user_context));
    auto specified = GetSpecified<CPSG_Request_Resolve>(input);
    IncludeInfo(request, specified);
}

template <class TInput>
inline void CProcessing::CreateRequestImpl(shared_ptr<CPSG_Request_Blob>& request, shared_ptr<void> user_context, const TInput& input)
{
    auto blob_id = GetBlobId(input);
    auto last_modified = GetLastModified(input);
    request = make_shared<CPSG_Request_Blob>(move(blob_id), move(last_modified), move(user_context));
    auto specified = GetSpecified<CPSG_Request_Blob>(input);
    IncludeData(request, specified);
}

template <class TInput>
inline void CProcessing::CreateRequestImpl(shared_ptr<CPSG_Request_NamedAnnotInfo>& request, shared_ptr<void> user_context, const TInput& input)
{
    auto bio_id = GetBioId(input);
    auto named_annots = GetNamedAnnots(input);
    request = make_shared<CPSG_Request_NamedAnnotInfo>(move(bio_id), move(named_annots), move(user_context));
}

template <class TRequest>
inline void CProcessing::IncludeData(shared_ptr<TRequest> request, TSpecified specified)
{
    for (const auto& f : GetDataFlags()) {
        if (specified(f.name)) {
            request->IncludeData(f.value);
            return;
        }
    }
}

template <class TRequest, class TInput>
inline shared_ptr<CPSG_Request> CProcessing::CreateRequest(shared_ptr<void> user_context, const TInput& input)
{
    shared_ptr<TRequest> request;
    CreateRequestImpl(request, move(user_context), input);
    return request;
}

END_NCBI_SCOPE

#endif
