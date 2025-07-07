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
    SJsonOut(bool pipe = false, bool server = false) :
        m_Pipe(pipe),
        m_Flags(server ? fJson_Write_NoIndentation | fJson_Write_NoEol : fJson_Write_IndentWithSpace)
    {}

    ~SJsonOut();
    SJsonOut& operator<<(const CJson_Document& doc);

private:
    mutex m_Mutex;
    char m_Separator = '[';
    const bool m_Pipe;
    const TJson_Write_Flags m_Flags;
};

class CJsonResponse : public CJson_Document
{
public:
    enum EDoNotAddRequestID { eDoNotAddRequestID };

    template <class TItem, class... TArgs>
    CJsonResponse(EPSG_Status status, TItem item, TArgs&&... args);
    CJsonResponse(const string& id, const CJsonResponse& result);
    CJsonResponse(const string& id, int code, const string& message);

    static void SetReplyType(bool value) { sm_SetReplyType = value; }
    static void SetDataLimit(size_t value) { sm_DataLimit = value; }
    static void SetPreviewSize(size_t value) { sm_PreviewSize = value; }

    static CJsonResponse NewItem(const shared_ptr<CPSG_ReplyItem>& reply_item);

private:
    CJsonResponse() : m_JsonObj(SetObject()) {}
    CJsonResponse(const string& id);

    template <class TItem>
    void AddRequestID(TItem, EDoNotAddRequestID) {}

    template <class TItem, class... TArgs>
    void AddRequestID(TItem item, TArgs&&... args);

    void Fill(EPSG_Status status, shared_ptr<CPSG_Reply>);
    void Fill(EPSG_Status status, shared_ptr<CPSG_ReplyItem> item);

    void Fill(shared_ptr<CPSG_BlobData> blob_data);
    void Fill(shared_ptr<CPSG_BlobInfo> blob_info);
    void Fill(shared_ptr<CPSG_SkippedBlob> skipped_blob);
    void Fill(shared_ptr<CPSG_BioseqInfo> bioseq_info);
    void Fill(shared_ptr<CPSG_NamedAnnotInfo> named_annot_info);
    void Fill(shared_ptr<CPSG_NamedAnnotStatus> named_annot_status);
    void Fill(shared_ptr<CPSG_PublicComment> public_comment);
    void Fill(shared_ptr<CPSG_Processor> processor);
    void Fill(shared_ptr<CPSG_IpgInfo> ipg_info);
    void Fill(shared_ptr<CPSG_AccVerHistory> acc_ver_history);

    void AddMessage(const SPSG_Message& message);
    void AddMessage(EDoNotAddRequestID = eDoNotAddRequestID) {}

    template <class TReplyItem>
    void Set(const char* name, shared_ptr<TReplyItem>& reply_item)
    {
        if      (auto blob_id  = reply_item->template GetId<CPSG_BlobId> ()) Set(name, *blob_id);
        else if (auto chunk_id = reply_item->template GetId<CPSG_ChunkId>()) Set(name, *chunk_id);
    }

    template <typename T>
    void Set(const char* name, T&& v) { Set(m_JsonObj, name, std::forward<T>(v)); }

    template <typename T>
    static void Set(CJson_Object& obj, const char* name, T&& v) { Set(obj[name], std::forward<T>(v)); }

    template <typename T>
    static void Set(CJson_Object& obj, const char* name, const CNullable<T>& v) { if (!v.IsNull()) Set(obj[name], v.GetValue()); }

    template <typename T>
    static void Set(CJson_Object& obj, const char* name, const optional<T>& v) { if (v) Set(obj[name], *v); }

    // enable_if required here to avoid recursion as CJson_Value (returned by SetValue) is derived from CJson_Node
    template <class TNode, typename T, typename enable_if<is_same<TNode, CJson_Node>::value, int>::type = 0>
    static void Set(TNode node, T&& v) { Set(node.SetValue(), std::forward<T>(v)); }

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
    static void Set(CJson_Value value, double v)        { value.SetDouble(v); }
    static void Set(CJson_Value value, bool v)          { value.SetBool(v);   }

    CJson_Object m_JsonObj;
    bool m_Error = false;
    static bool sm_SetReplyType;
    inline static auto sm_DataLimit = numeric_limits<size_t>::max();
    inline static auto sm_PreviewSize = numeric_limits<size_t>::max();
};

struct SParams
{
    const string service;
    const CPSG_Request::TFlags request_flags;
    const SPSG_UserArgs user_args;

    static EDiagSev min_severity;
    static bool verbose;

    SParams(string s, CPSG_Request::TFlags rf, SPSG_UserArgs ua) :
        service(std::move(s)),
        request_flags(rf),
        user_args(std::move(ua))
    {}
};

struct SOneRequestParams : SParams
{
    struct SLatency
    {
        const CLogLatencies::EWhich which;
        const bool debug;
    };

    struct SDataOnly
    {
        const bool enabled;
        const bool messages_only;
        const ESerialDataFormat output_format;
    };

    SLatency latency;
    SDataOnly data_only;

    SOneRequestParams(string s, CPSG_Request::TFlags rf, SPSG_UserArgs ua, CLogLatencies::EWhich lw, bool ld, bool de, bool dm, ESerialDataFormat df) :
        SParams(std::move(s), rf, std::move(ua)),
        latency{lw, ld},
        data_only{de, dm, df}
    {}
};

struct SParallelProcessingParams : SParams
{
    const double rate;
    const int worker_threads;
    const bool pipe;
    const bool server;

    SParallelProcessingParams(string s, CPSG_Request::TFlags rf, SPSG_UserArgs ua, double r, int wt, bool p, bool srv) :
        SParams(std::move(s), rf, std::move(ua)),
        rate(r),
        worker_threads(wt),
        pipe(p),
        server(srv)
    {}
};

struct SResolveParams
{
    const CPSG_BioId::TType type;
    const CPSG_Request_Resolve::TIncludeInfo include_info;
    const EPSG_AccSubstitution acc_substitution;
    const EPSG_BioIdResolution bio_id_resolution;
};

struct SBatchResolveParams : SParallelProcessingParams, SResolveParams
{
    SBatchResolveParams(string s, CPSG_Request::TFlags rf, SPSG_UserArgs ua, double r, int wt, bool p, bool srv, SResolveParams resolve_params) :
        SParallelProcessingParams(std::move(s), rf, std::move(ua), r, wt, p, srv),
        SResolveParams(std::move(resolve_params))
    {}
};

using TIpgBatchResolveParams = SParallelProcessingParams;

struct SInteractiveParams : SParallelProcessingParams
{
    const size_t data_limit;
    const size_t preview_size;
    const bool echo;
    const bool testing;

    SInteractiveParams(string s, CPSG_Request::TFlags rf, SPSG_UserArgs ua, double r, int wt, bool p, bool srv, size_t dl, size_t ps, bool e, bool os, bool t) :
        SParallelProcessingParams(GetService(std::move(s), os), rf, std::move(ua), r, wt, p, srv),
        data_limit(dl),
        preview_size(ps),
        echo(e),
        testing(t)
    {}

    static string GetService(string service, bool one_server);
};

struct SPerformanceParams : SParams
{
    const size_t user_threads;
    const double delay;
    const bool local_queue;
    const bool report_immediately;

    SPerformanceParams(string s, CPSG_Request::TFlags rf, SPSG_UserArgs ua, size_t ut, double d, bool lq, bool ri) :
        SParams(std::move(s), rf, std::move(ua)),
        user_threads(ut),
        delay(d),
        local_queue(lq),
        report_immediately(ri)
    {}
};

template <class TParams>
class CParallelProcessing
{
public:
    CParallelProcessing(const TParams& params);
    ~CParallelProcessing() { m_Impl.Stop(); }

    void operator()(string line) { m_Impl.Process(std::move(line)); }

private:
    struct SImpl
    {
        SJsonOut json_out;

        SImpl(const TParams& params);

        void Process(string line) { m_InputQueue.Push(std::move(line)); }
        void Stop() { m_InputQueue.Stop(m_InputQueue.eDrain); }

        void Submitter(CPSG_Queue& output);

        using TItemComplete = void (*)(SJsonOut&, EPSG_Status, const shared_ptr<CPSG_ReplyItem>&);
        using TReplyComplete = void (*)(SJsonOut&, EPSG_Status, const shared_ptr<CPSG_Reply>&);
        using TNewItem = void (*)(SJsonOut&, const shared_ptr<CPSG_ReplyItem>&);

        TItemComplete GetItemComplete();
        TReplyComplete GetReplyComplete();
        TNewItem GetNewItem();

    private:
        void Init(const TParams& params);

        const TParams& m_Params;
        CPSG_WaitingQueue<string> m_InputQueue;
    };

    struct SThread
    {
        template <class TMethod, class... TArgs>
        SThread(TMethod method, TArgs&&... args) : m_Thread(method, std::forward<TArgs>(args)...) {}
        ~SThread() { m_Thread.join(); }

    private:
        thread m_Thread;
    };

    SImpl m_Impl;
    list<CPSG_EventLoop> m_PsgQueues;
    list<SThread> m_Threads;
};

class CProcessing
{
public:
    static int OneRequest(const SOneRequestParams& params, shared_ptr<CPSG_Request> request);

    template <class TParams>
    static int ParallelProcessing(const TParams& params, istream& is = cin);
    static int Performance(const SPerformanceParams& params);
    static int JsonCheck(istream* schema_is, bool single_doc);

    static CJson_Document RequestSchema();

private:
    template <class TCreateContext>
    static vector<shared_ptr<CPSG_Request>> ReadCommands(TCreateContext create_context, size_t report_progress_after = 0);

    static bool ReadLine(string& line, istream& is = cin, bool single_doc = false);
    static CParallelProcessing<SBatchResolveParams> CreateParallelProcessing(const SBatchResolveParams& params);
    static CParallelProcessing<TIpgBatchResolveParams> CreateParallelProcessing(const TIpgBatchResolveParams& params);
    static CParallelProcessing<SInteractiveParams> CreateParallelProcessing(const SInteractiveParams& params);
};

template <class TParams>
inline int CProcessing::ParallelProcessing(const TParams& params, istream& is)
{
    using namespace chrono;

    auto parallel_processing = CreateParallelProcessing(params);
    auto start = params.rate > 0 ? system_clock::now() : system_clock::time_point{};
    auto wait = params.rate > 0 ? 1s / params.rate : 0s;
    auto n = 0;
    string line;

    while (ReadLine(line, is)) {
        _ASSERT(!line.empty()); // ReadLine makes sure it's not empty
        parallel_processing(std::move(line));

        if (wait > 0s) {
            this_thread::sleep_for(wait);

            // Recalculate wait once per second or after each sleep, whichever is longer
            if (++n >= params.rate) {
                auto now = system_clock::now();
                wait -= (now - start) / n - 1s / params.rate;
                start = now;
                n = 0;
            }
        }
    }

    return 0;
}

using CRawRequest = CPSG_Request;

struct SRequestBuilder
{
    template <class TRequest, class TInput, class... TArgs>
    static shared_ptr<TRequest> Build(const TInput& input, TArgs&&... args);

    template <class TInput, class... TArgs>
    static shared_ptr<CPSG_Request> Build(const string& name, const TInput& input, TArgs&&... args);

    static const initializer_list<SDataFlag>& GetDataFlags();
    static const initializer_list<SInfoFlag>& GetInfoFlags();

    template <class TInput>
    static SResolveParams GetResolveParams(const TInput& input);

private:
    using TSpecified = function<bool(const string&)>;
    using TExclude = function<void(string)>;

    template <class TInput>
    struct SReader;

    template <class TRequest>
    struct SImpl;

    static CPSG_BioId::TType GetBioIdType(const string& type);
    static EPSG_AccSubstitution GetAccSubstitution(const string& acc_substitution);
};

template <>
struct SRequestBuilder::SReader<CJson_ConstObject>
{
    const CJson_ConstObject& input;

    SReader(const CJson_ConstObject& i) : input(i) {}

    TSpecified GetSpecified() const;
    CPSG_BioId GetBioId() const { return GetBioId(input["bio_id"].GetArray()); }
    CPSG_BioIds GetBioIds() const;
    CPSG_BlobId GetBlobId() const;
    CPSG_ChunkId GetChunkId() const;
    vector<string> GetNamedAnnots() const;
    auto GetAccSubstitution() const { return input.has("acc_substitution") ? SRequestBuilder::GetAccSubstitution(input["acc_substitution"].GetValue().GetString()) : EPSG_AccSubstitution::Default; }
    EPSG_BioIdResolution GetBioIdResolution() const { return input.has("bio_id_resolution") && !input["bio_id_resolution"].GetValue().GetBool() ? EPSG_BioIdResolution::NoResolve : EPSG_BioIdResolution::Resolve; }
    CTimeout GetResendTimeout() const { return !input.has("resend_timeout") ? CTimeout::eDefault : CTimeout(input["resend_timeout"].GetValue().GetDouble()); }
    void ForEachTSE(TExclude exclude) const;
    auto GetProtein() const { return input.has("protein") ? input["protein"].GetValue().GetString() : string(); }
    auto GetIpg() const { return input.has("ipg") ? input["ipg"].GetValue().GetInt8() : 0; }
    auto GetNucleotide() const { return input.has("nucleotide") ? CPSG_Request_IpgResolve::TNucleotide(input["nucleotide"].GetValue().GetString()) : null; }
    auto GetSNPScaleLimit() const { return input.has("snp_scale_limit") ? objects::CSeq_id::GetSNPScaleLimit_Value(input["snp_scale_limit"].GetValue().GetString()) : CPSG_Request_NamedAnnotInfo::ESNPScaleLimit::eSNPScaleLimit_Default; }
    auto GetAbsPathRef() const { return input["abs_path_ref"].GetValue().GetString(); }
    void SetRequestFlags(shared_ptr<CPSG_Request> request) const;
    SPSG_UserArgs GetUserArgs() const { return input.has("user_args") ? input["user_args"].GetValue().GetString() : SPSG_UserArgs(); }

private:
    CPSG_BioId GetBioId(const CJson_ConstArray& array) const;
};

template <class TRequest>
struct SRequestBuilder::SImpl
{
    shared_ptr<void> user_context;
    CRef<CRequestContext> request_context;

    SImpl(shared_ptr<void> uc = {}, CRef<CRequestContext> rc = {}) :
        user_context(std::move(uc)),
        request_context(std::move(rc))
    {}

    template <class... TArgs>
    auto Create(TArgs&&... args)
    {
        return make_shared<TRequest>(std::forward<TArgs>(args)..., std::move(user_context), std::move(request_context));
    }

    template <class TReader>
    shared_ptr<TRequest> Build(const TReader& reader);

    template <class TReader>
    static TSpecified GetSpecified(const TReader& reader) { return reader.GetSpecified(); }
    static CPSG_Request_Resolve::TIncludeInfo GetIncludeInfo(TSpecified specified);
    static void IncludeData(shared_ptr<TRequest> request, TSpecified specified);
};

inline SRequestBuilder::TSpecified SRequestBuilder::SReader<CJson_ConstObject>::GetSpecified() const
{
    return [&](const string& name) {
        return input.has("include_data") && (name == input["include_data"].GetValue().GetString());
    };
}

template <>
template <>
inline SRequestBuilder::TSpecified SRequestBuilder::SImpl<CPSG_Request_Resolve>::GetSpecified(const SReader<CJson_ConstObject>& reader)
{
    return [&](const string& name) {
        if (!reader.input.has("include_info")) return false;

        auto include_info = reader.input["include_info"].GetArray();
        auto equals_to = [&](const CJson_ConstNode& node) { return node.GetValue().GetString() == name; };
        return find_if(include_info.begin(), include_info.end(), equals_to) != include_info.end();
    };
}

template <>
CPSG_Request_Resolve::TIncludeInfo SRequestBuilder::SImpl<CPSG_Request_Resolve>::GetIncludeInfo(TSpecified specified);

template <class TInput>
SResolveParams SRequestBuilder::GetResolveParams(const TInput& input)
{
    using TImpl = SImpl<CPSG_Request_Resolve>;
    SReader<TInput> reader(input);
    return { reader.GetBioIdType(), TImpl::GetIncludeInfo(TImpl::GetSpecified(reader)), reader.GetAccSubstitution(), reader.GetBioIdResolution() };
}

template <class TRequest, class TInput, class... TArgs>
shared_ptr<TRequest> SRequestBuilder::Build(const TInput& input, TArgs&&... args)
{
    SReader<TInput> reader(input);
    auto rv = SImpl<TRequest>(std::forward<TArgs>(args)...).Build(reader);
    reader.SetRequestFlags(rv);
    rv->SetUserArgs(reader.GetUserArgs());
    return rv;
}

template <class TInput, class... TArgs>
shared_ptr<CPSG_Request> SRequestBuilder::Build(const string& name, const TInput& input, TArgs&&... args)
{
    if (name == "biodata") {
        return Build<CPSG_Request_Biodata>(input, std::forward<TArgs>(args)...);
    } else if (name == "blob") {
        return Build<CPSG_Request_Blob>(input, std::forward<TArgs>(args)...);
    } else if (name == "resolve") {
        return Build<CPSG_Request_Resolve>(input, std::forward<TArgs>(args)...);
    } else if (name == "named_annot") {
        return Build<CPSG_Request_NamedAnnotInfo>(input, std::forward<TArgs>(args)...);
    } else if (name == "chunk") {
        return Build<CPSG_Request_Chunk>(input, std::forward<TArgs>(args)...);
    } else if (name == "ipg_resolve") {
        return Build<CPSG_Request_IpgResolve>(input, std::forward<TArgs>(args)...);
    } else if (name == "acc_ver_history") {
        return Build<CPSG_Request_AccVerHistory>(input, std::forward<TArgs>(args)...);
    } else if (name == "raw") {
        return Build<CRawRequest>(input, std::forward<TArgs>(args)...);
    } else {
        return {};
    }
}

template <>
template <class TReader>
shared_ptr<CPSG_Request_Biodata> SRequestBuilder::SImpl<CPSG_Request_Biodata>::Build(const TReader& reader)
{
    auto bio_id = reader.GetBioId();
    auto bio_id_resolution = reader.GetBioIdResolution();
    auto request = Create(std::move(bio_id), bio_id_resolution);
    auto specified = GetSpecified(reader);
    IncludeData(request, specified);
    auto exclude = [&](string blob_id) { request->ExcludeTSE(blob_id); };
    reader.ForEachTSE(exclude);
    request->SetAccSubstitution(reader.GetAccSubstitution());
    const auto resend_timeout = reader.GetResendTimeout();

    if (!resend_timeout.IsDefault()) {
        request->SetResendTimeout(resend_timeout);
    }

    return request;
}

template <>
template <class TReader>
shared_ptr<CPSG_Request_Resolve> SRequestBuilder::SImpl<CPSG_Request_Resolve>::Build(const TReader& reader)
{
    auto bio_id = reader.GetBioId();
    auto bio_id_resolution = reader.GetBioIdResolution();
    auto request = Create(std::move(bio_id), bio_id_resolution);
    auto specified = GetSpecified(reader);
    const auto include_info = GetIncludeInfo(specified);
    request->IncludeInfo(include_info);
    request->SetAccSubstitution(reader.GetAccSubstitution());
    return request;
}

template <>
template <class TReader>
shared_ptr<CPSG_Request_Blob> SRequestBuilder::SImpl<CPSG_Request_Blob>::Build(const TReader& reader)
{
    auto blob_id = reader.GetBlobId();
    auto request = Create(std::move(blob_id));
    auto specified = GetSpecified(reader);
    IncludeData(request, specified);
    return request;
}

template <>
template <class TReader>
shared_ptr<CPSG_Request_NamedAnnotInfo> SRequestBuilder::SImpl<CPSG_Request_NamedAnnotInfo>::Build(const TReader& reader)
{
    auto bio_ids = reader.GetBioIds();
    auto named_annots = reader.GetNamedAnnots();
    auto bio_id_resolution = reader.GetBioIdResolution();
    auto request = Create(std::move(bio_ids), std::move(named_annots), bio_id_resolution);
    auto specified = GetSpecified(reader);
    IncludeData(request, specified);
    request->SetAccSubstitution(reader.GetAccSubstitution());
    request->SetSNPScaleLimit(reader.GetSNPScaleLimit());
    return request;
}

template <>
template <class TReader>
shared_ptr<CPSG_Request_Chunk> SRequestBuilder::SImpl<CPSG_Request_Chunk>::Build(const TReader& reader)
{
    auto chunk_id = reader.GetChunkId();
    return Create(std::move(chunk_id));
}

template <>
template <class TReader>
shared_ptr<CPSG_Request_IpgResolve> SRequestBuilder::SImpl<CPSG_Request_IpgResolve>::Build(const TReader& reader)
{
    auto protein = reader.GetProtein();
    auto ipg = reader.GetIpg();
    auto nucleotide = reader.GetNucleotide();
    return Create(std::move(protein), ipg, std::move(nucleotide));
}

template <>
template <class TReader>
shared_ptr<CPSG_Request_AccVerHistory> SRequestBuilder::SImpl<CPSG_Request_AccVerHistory>::Build(const TReader& reader)
{
    auto bio_id = reader.GetBioId();
    return Create(std::move(bio_id));
}

template <>
template <class TReader>
shared_ptr<CRawRequest> SRequestBuilder::SImpl<CRawRequest>::Build(const TReader& reader)
{
    return CPSG_Misc::CreateRawRequest(reader.GetAbsPathRef(), std::move(user_context), std::move(request_context));
}

template <class TRequest>
inline void SRequestBuilder::SImpl<TRequest>::IncludeData(shared_ptr<TRequest> request, TSpecified specified)
{
    for (const auto& f : GetDataFlags()) {
        if (specified(f.name)) {
            request->IncludeData(f.value);
            return;
        }
    }
}

inline EPSG_AccSubstitution SRequestBuilder::GetAccSubstitution(const string& acc_substitution)
{
    if (acc_substitution == "limited") {
        return EPSG_AccSubstitution::Limited;
    } else if (acc_substitution == "never") {
        return EPSG_AccSubstitution::Never;
    } else {
        return EPSG_AccSubstitution::Default;
    }
}

END_NCBI_SCOPE

#endif
