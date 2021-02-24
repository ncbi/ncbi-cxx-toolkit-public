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
#include <objtools/pubseq_gateway/client/impl/misc.hpp>
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
    SJsonOut(bool pipe = false) : m_Pipe(pipe) {}
    ~SJsonOut();
    SJsonOut& operator<<(const CJson_Document& doc);

private:
    mutex m_Mutex;
    char m_Separator = '[';
    const bool m_Pipe;
};

class CJsonResponse : public CJson_Document
{
public:
    template <class TItem>
    CJsonResponse(EPSG_Status status, TItem item);

    CJsonResponse(const string& id, bool result);
    CJsonResponse(const string& id, const CJson_Document& result);
    CJsonResponse(const string& id, int code, const string& message);

    static void SetReplyType(bool value) { sm_SetReplyType = value; }
    static void Verbose(bool value) { sm_Verbose = value; }

private:
    CJsonResponse(const string& id);

    void Fill(EPSG_Status status, shared_ptr<CPSG_Reply> reply) { Fill(reply, status); }
    void Fill(EPSG_Status status, shared_ptr<CPSG_ReplyItem> item);

    void Fill(shared_ptr<CPSG_BlobData> blob_data);
    void Fill(shared_ptr<CPSG_BlobInfo> blob_info);
    void Fill(shared_ptr<CPSG_SkippedBlob> skipped_blob);
    void Fill(shared_ptr<CPSG_BioseqInfo> bioseq_info);
    void Fill(shared_ptr<CPSG_NamedAnnotInfo> named_annot_info);
    void Fill(shared_ptr<CPSG_PublicComment> public_comment);

    template <class TItem>
    void Fill(TItem item, EPSG_Status status);

    template <class TReplyItem>
    void Set(const char* name, shared_ptr<TReplyItem>& reply_item)
    {
        if      (auto blob_id  = reply_item->template GetId<CPSG_BlobId> ()) Set(name, *blob_id);
        else if (auto chunk_id = reply_item->template GetId<CPSG_ChunkId>()) Set(name, *chunk_id);
    }

    template <typename T>
    void Set(const char* name, T&& v) { Set(m_JsonObj[name], forward<T>(v)); }

    // enable_if required here to avoid recursion as CJson_Value (returned by SetValue) is derived from CJson_Node
    template <class TNode, typename T, typename enable_if<is_same<TNode, CJson_Node>::value, int>::type = 0>
    static void Set(TNode node, T&& v) { Set(node.SetValue(), forward<T>(v)); }

    static void Set(CJson_Node node, const CPSG_BioId& bio_id);
    static void Set(CJson_Node node, const vector<CPSG_BioId>& bio_ids);
    static void Set(CJson_Node node, const CPSG_BlobId& blob_id);
    static void Set(CJson_Node node, const CPSG_ChunkId& chunk_id);

    static void Set(CJson_Value value, const string& v) { value.SetString(v); }
    static void Set(CJson_Value value, const char* v)   { value.SetString(v); }
    static void Set(CJson_Value value, Int4 v)          { value.SetInt4(v);   }
    static void Set(CJson_Value value, Int8 v)          { value.SetInt8(v);   }
    static void Set(CJson_Value value, Uint4 v)         { value.SetUint4(v);  }
    static void Set(CJson_Value value, Uint8 v)         { value.SetUint8(v);  }
    static void Set(CJson_Value value, bool v)          { value.SetBool(v);   }

    CJson_Object m_JsonObj;
    static bool sm_SetReplyType;
    static bool sm_Verbose;
};

class CParallelProcessing
{
public:
    CParallelProcessing(const string& service, bool pipe, const CArgs& args, bool echo, bool batch_resolve);
    ~CParallelProcessing();

    void operator()(string id) { m_InputQueue.Push(move(id)); }

private:
    using TInputQueue = CPSG_WaitingStack<string>;
    using TReplyQueue = CPSG_WaitingStack<shared_ptr<CPSG_Reply>>;

    struct BatchResolve
    {
        static void Submitter(TInputQueue& input, CPSG_Queue& output, const CArgs& args);
        static void Reporter(TReplyQueue& input, SJsonOut& output);
    };

    static void Retriever(CPSG_Queue& input, TReplyQueue& output);

    struct Interactive
    {
        static void Submitter(TInputQueue& input, CPSG_Queue& output, SJsonOut& json_out, bool echo);
        static void Reporter(TReplyQueue& input, SJsonOut& output);
    };

    struct SThread
    {
        template <class TMethod, class... TArgs>
        SThread(TMethod method, TArgs&&... args) : m_Thread(method, forward<TArgs>(args)...) {}
        ~SThread() { m_Thread.join(); }

    private:
        thread m_Thread;
    };

    TInputQueue m_InputQueue;
    CPSG_Queue m_PsgQueue;
    TReplyQueue m_ReplyQueue;
    SJsonOut m_JsonOut;
    list<SThread> m_Threads;
};

class CProcessing
{
public:
    struct SParams
    {
        struct SLatency
        {
            const bool enabled;
            const bool debug;
        };

        struct SDataOnly
        {
            const bool enabled;
            const ESerialDataFormat output_format;
        };

        SLatency latency;
        SDataOnly data_only;

        SParams(const CArgs& args);
    };

    static int OneRequest(const string& service, shared_ptr<CPSG_Request> request, SParams params);
    static int ParallelProcessing(const string& service, const CArgs& args, bool batch_resolve, bool echo);
    static int Performance(const string& service, size_t user_threads, double delay, bool local_queue, ostream& os);
    static int Report(istream& is, ostream& os, double percentage);
    static int Testing(const string& service);
    static int Io(const string& service, time_t start_time, int duration, int user_threads, int download_size);
    static int JsonCheck(istream* schema_is, istream& doc_is);

    static CJson_Document RequestSchema();

private:
    template <class TCreateContext>
    static vector<shared_ptr<CPSG_Request>> ReadCommands(TCreateContext create_context);

    static bool ReadLine(string& line, istream& is = cin);
};

struct SRequestBuilder
{
    template <class TRequest, class TInput, class... TArgs>
    static shared_ptr<TRequest> Build(const TInput& input, TArgs&&... args);

    template <class... TArgs>
    static shared_ptr<CPSG_Request> Build(const string& name, const CJson_ConstObject& input, TArgs&&... args);

    static const initializer_list<SDataFlag>& GetDataFlags();
    static const initializer_list<SInfoFlag>& GetInfoFlags();

    static CPSG_BioId::TType GetBioIdType(string type);

    using TSpecified = function<bool(const string&)>;

    template <class TRequest>
    static TSpecified GetSpecified(const CArgs& input);
    template <class TRequest>
    static TSpecified GetSpecified(const CJson_ConstObject& input);

    static CPSG_Request_Resolve::TIncludeInfo GetIncludeInfo(TSpecified specified);

private:
    template <class TInput>
    struct SImpl;

    static CPSG_BioId GetBioId(const CArgs& input);
    static CPSG_BioId GetBioId(const CJson_ConstObject& input);

    static CPSG_BlobId GetBlobId(const CArgs& input);
    static CPSG_BlobId GetBlobId(const CJson_ConstObject& input);

    static CPSG_ChunkId GetChunkId(const CArgs& input);
    static CPSG_ChunkId GetChunkId(const CJson_ConstObject& input);

    static vector<string> GetNamedAnnots(const CArgs& input) { return input["na"].GetStringList(); }
    static vector<string> GetNamedAnnots(const CJson_ConstObject& input);

    static string GetAccSubstitution(const CArgs& input) { return input["acc-substitution"].HasValue() ? input["acc-substitution"].AsString() : ""; }
    static string GetAccSubstitution(const CJson_ConstObject& input) { return input.has("acc_substitution") ? input["acc_substitution"].GetValue().GetString() : ""; }

    static ESwitch GetAutoBlobSkipping(const CArgs& input) { return eDefault; }
    static ESwitch GetAutoBlobSkipping(const CJson_ConstObject& input) { return !input.has("auto_blob_skipping") ? eDefault : input["auto_blob_skipping"].GetValue().GetBool() ? eOn : eOff; }

    template <class TRequest>
    static void IncludeData(shared_ptr<TRequest> request, TSpecified specified);

    static void ExcludeTSEs(shared_ptr<CPSG_Request_Biodata> request, const CArgs& input);
    static void ExcludeTSEs(shared_ptr<CPSG_Request_Biodata> request, const CJson_ConstObject& input);

    template <class TRequest, class TInput>
    static void SetAccSubstitution(shared_ptr<TRequest>& request, const TInput& input);
};

// Helper class to 'overload' on return type and specify input parameters once
template <class TInput>
struct SRequestBuilder::SImpl
{
    const TInput& input;
    shared_ptr<void> user_context;
    CRef<CRequestContext> request_context;

    SImpl(const TInput& i, shared_ptr<void> uc = {}, CRef<CRequestContext> rc = {}) :
        input(i),
        user_context(move(uc)),
        request_context(move(rc))
    {}

    operator shared_ptr<CPSG_Request_Biodata>();
    operator shared_ptr<CPSG_Request_Resolve>();
    operator shared_ptr<CPSG_Request_Blob>();
    operator shared_ptr<CPSG_Request_NamedAnnotInfo>();
    operator shared_ptr<CPSG_Request_Chunk>();
};

template <class TRequest>
inline SRequestBuilder::TSpecified SRequestBuilder::GetSpecified(const CArgs& input)
{
    return [&](const string& name) {
        return input[name].HasValue();
    };
}

template <class TRequest>
inline SRequestBuilder::TSpecified SRequestBuilder::GetSpecified(const CJson_ConstObject& input)
{
    return [&](const string& name) {
        return input.has("include_data") && (name == input["include_data"].GetValue().GetString());
    };
}

template <>
inline SRequestBuilder::TSpecified SRequestBuilder::GetSpecified<CPSG_Request_Resolve>(const CJson_ConstObject& input)
{
    return [&](const string& name) {
        if (!input.has("include_info")) return false;

        auto include_info = input["include_info"].GetArray();
        auto equals_to = [&](const CJson_ConstNode& node) { return node.GetValue().GetString() == name; };
        return find_if(include_info.begin(), include_info.end(), equals_to) != include_info.end();
    };
}

template <class TRequest, class TInput, class... TArgs>
shared_ptr<TRequest> SRequestBuilder::Build(const TInput& input, TArgs&&... args)
{
    return SImpl<TInput>(input, forward<TArgs>(args)...);
}

template <class... TArgs>
shared_ptr<CPSG_Request> SRequestBuilder::Build(const string& name, const CJson_ConstObject& input, TArgs&&... args)
{
    SImpl<CJson_ConstObject> build(input, forward<TArgs>(args)...);

    if (name == "biodata") {
        return static_cast<shared_ptr<CPSG_Request_Biodata>>(build);
    } else if (name == "blob") {
        return static_cast<shared_ptr<CPSG_Request_Blob>>(build);
    } else if (name == "resolve") {
        return static_cast<shared_ptr<CPSG_Request_Resolve>>(build);
    } else if (name == "named_annot") {
        return static_cast<shared_ptr<CPSG_Request_NamedAnnotInfo>>(build);
    } else if (name == "chunk") {
        return static_cast<shared_ptr<CPSG_Request_Chunk>>(build);
    } else {
        return {};
    }
}

template <class TInput>
SRequestBuilder::SImpl<TInput>::operator shared_ptr<CPSG_Request_Biodata>()
{
    auto bio_id = GetBioId(input);
    auto request = make_shared<CPSG_Request_Biodata>(move(bio_id), move(user_context));
    auto specified = GetSpecified<CPSG_Request_Biodata>(input);
    IncludeData(request, specified);
    ExcludeTSEs(request, input);
    SetAccSubstitution(request, input);
    const auto auto_blob_skipping = GetAutoBlobSkipping(input);

    if (auto_blob_skipping != eDefault) {
        request->SetAutoBlobSkipping(auto_blob_skipping == eOn);
    }

    return request;
}

template <class TInput>
SRequestBuilder::SImpl<TInput>::operator shared_ptr<CPSG_Request_Resolve>()
{
    auto bio_id = GetBioId(input);
    auto request = make_shared<CPSG_Request_Resolve>(move(bio_id), move(user_context));
    auto specified = GetSpecified<CPSG_Request_Resolve>(input);
    const auto include_info = GetIncludeInfo(specified);
    request->IncludeInfo(include_info);
    SetAccSubstitution(request, input);
    return request;
}

template <class TInput>
SRequestBuilder::SImpl<TInput>::operator shared_ptr<CPSG_Request_Blob>()
{
    auto blob_id = GetBlobId(input);
    auto request = make_shared<CPSG_Request_Blob>(move(blob_id), move(user_context));
    auto specified = GetSpecified<CPSG_Request_Blob>(input);
    IncludeData(request, specified);
    return request;
}

template <class TInput>
SRequestBuilder::SImpl<TInput>::operator shared_ptr<CPSG_Request_NamedAnnotInfo>()
{
    auto bio_id = GetBioId(input);
    auto named_annots = GetNamedAnnots(input);
    auto request =  make_shared<CPSG_Request_NamedAnnotInfo>(move(bio_id), move(named_annots), move(user_context));
    SetAccSubstitution(request, input);
    return request;
}

template <class TInput>
SRequestBuilder::SImpl<TInput>::operator shared_ptr<CPSG_Request_Chunk>()
{
    auto chunk_id = GetChunkId(input);
    return make_shared<CPSG_Request_Chunk>(move(chunk_id), move(user_context));
}

template <class TRequest>
inline void SRequestBuilder::IncludeData(shared_ptr<TRequest> request, TSpecified specified)
{
    for (const auto& f : GetDataFlags()) {
        if (specified(f.name)) {
            request->IncludeData(f.value);
            return;
        }
    }
}

template <class TRequest, class TInput>
void SRequestBuilder::SetAccSubstitution(shared_ptr<TRequest>& request, const TInput& input)
{
    const auto& acc_substitution = GetAccSubstitution(input);

    if (acc_substitution == "limited") {
        request->SetAccSubstitution(EPSG_AccSubstitution::Limited);
    } else if (acc_substitution == "never") {
        request->SetAccSubstitution(EPSG_AccSubstitution::Never);
    }
}

END_NCBI_SCOPE

#endif
