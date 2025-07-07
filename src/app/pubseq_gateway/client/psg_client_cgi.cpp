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

#include <ncbi_pch.hpp>

#include <cgi/cgiapp.hpp>
#include <cgi/cgictx.hpp>
#include <cgi/cgi_exception.hpp>
#include <html/html.hpp>

#include "processing.hpp"
#include "performance.hpp"

USING_NCBI_SCOPE;

struct SPsgCgiEntries
{
    const TCgiEntries& entries;

    SPsgCgiEntries(const TCgiEntries& e) : entries(e) {}

    bool Has(const string& key) const { return entries.find(key) != entries.end(); }

    template <typename TF>
    invoke_result_t<TF, const string&> Get(const string& key, TF f, invoke_result_t<TF, const string&> def_value) const;

    template <typename T>
    T GetNumeric(const string& key) const { return Get(key, [](const auto& v) { return NStr::StringToNumeric<T>(v); }, T()); }
    const string& GetString(const string& key) const { return Get(key, [](const auto& v) -> const auto& { return v; }, kEmptyStr); }
    auto GetStringList(const string& key) const;
};

template <typename TF>
invoke_result_t<TF, const string&> SPsgCgiEntries::Get(const string& key, TF f, invoke_result_t<TF, const string&> def_value) const
{
    auto found = entries.find(key);
    return found == entries.end() ? def_value : f(found->second.GetValue());
}

auto SPsgCgiEntries::GetStringList(const string& key) const
{
    auto range = entries.equal_range(key);
    vector<string> rv;
    transform(range.first, range.second, back_inserter(rv), [](const auto& p) { return p.second.GetValue(); });
    return rv;
}

namespace NParamsBuilder
{

template <class TParams>
struct SBase : TParams
{
    template <class... TInitArgs>
    SBase(const SPsgCgiEntries& entries, TInitArgs&&... init_args) :
        TParams{
            {},
            CPSG_Request::eDefaultFlags,
            SPSG_UserArgs(),
            std::forward<TInitArgs>(init_args)...
        }
    {
        auto l = [](const auto& v) { EDiagSev s; return CNcbiDiag::StrToSeverityLevel(v.c_str(), s) ? s : eDiag_Warning; };
        TParams::min_severity = entries.Get("min-severity", l, eDiag_Warning);
        TParams::verbose = entries.Has("verbose");
    }
};

struct SOneRequest : SBase<SOneRequestParams>
{
    SOneRequest(const SPsgCgiEntries& entries) :
        SBase{
            entries,
            CLogLatencies::eOff,
            false,
            entries.Has("blob-only") || entries.Has("annot-only"),
            true,
            GetDataOnlyOutputFormat(entries)
        }
    {
    }

    static ESerialDataFormat GetDataOnlyOutputFormat(const SPsgCgiEntries& entries)
    {
        const auto& format = entries.GetString("output-fmt");

        if (format == "asn")  return eSerial_AsnText;
        if (format == "asnb") return eSerial_AsnBinary;
        if (format == "xml")  return eSerial_Xml;
        if (format == "json") return eSerial_Json;

        return eSerial_None;
    }
};

template <class TParams>
struct SParallelProcessing : SBase<TParams>
{
    template <class... TInitArgs>
    SParallelProcessing(const SPsgCgiEntries& entries, TInitArgs&&... init_args) :
        SBase<TParams>{
            entries,
            0.0,
            1,
            false,
            false,
            std::forward<TInitArgs>(init_args)...
        }
    {
    }
};

struct SBatchResolve : SParallelProcessing<SBatchResolveParams>
{
    SBatchResolve(const SPsgCgiEntries& entries) :
        SParallelProcessing<SBatchResolveParams>{
            entries,
            SRequestBuilder::GetResolveParams(entries)
        }
    {
    }
};

using TIpgBatchResolve = SParallelProcessing<TIpgBatchResolveParams>;

}

template <>
struct SRequestBuilder::SReader<SPsgCgiEntries>
{
    const SPsgCgiEntries& input;

    SReader(const SPsgCgiEntries& i) : input(i) {}

    TSpecified GetSpecified() const;
    auto GetBioIdType() const { return input.Get("type", SRequestBuilder::GetBioIdType, CPSG_BioId::TType()); }
    CPSG_BioId GetBioId() const { return { input.GetString("id"), GetBioIdType() }; }
    CPSG_BioIds GetBioIds() const;
    CPSG_BlobId GetBlobId() const;
    CPSG_ChunkId GetChunkId() const;
    vector<string> GetNamedAnnots() const { return input.GetStringList("na"); }
    auto GetAccSubstitution() const { return SRequestBuilder::GetAccSubstitution(input.GetString("acc-substitution")); }
    EPSG_BioIdResolution GetBioIdResolution() const { return input.Has("no-bio-id-resolution") ? EPSG_BioIdResolution::NoResolve : EPSG_BioIdResolution::Resolve; }
    CTimeout GetResendTimeout() const { return CTimeout::eDefault; }
    void ForEachTSE(TExclude exclude) const;
    auto GetProtein() const { return input.GetString("protein"); }
    auto GetIpg() const { return input.Get("ipg", [](const auto& v) { return NStr::StringToInt8(v); }, 0); }
    auto GetNucleotide() const { return input.Get("nucleotide", [](const auto& v) { return CPSG_Request_IpgResolve::TNucleotide(v); }, null); }
    auto GetSNPScaleLimit() const { return objects::CSeq_id::GetSNPScaleLimit_Value(input.GetString("snp-scale-limit")); }
    auto GetAbsPathRef() const { NCBI_THROW(CPSG_Exception, eParameterMissing, "request cannot be empty"); return string(); } // Imitating unknown request
    void SetRequestFlags(shared_ptr<CPSG_Request> request) const;
    SPSG_UserArgs GetUserArgs() const { return NStr::URLDecode(input.GetString("user-args")); }
};

SRequestBuilder::TSpecified SRequestBuilder::SReader<SPsgCgiEntries>::GetSpecified() const
{
    return [&](const string& name) {
        return input.Has(name);
    };
}

CPSG_BioIds SRequestBuilder::SReader<SPsgCgiEntries>::GetBioIds() const
{
    CPSG_BioIds rv;

    auto ids = input.GetStringList("id");

    for (const auto& id : ids) {
        if (rv.empty()) {
            rv.emplace_back(id, GetBioIdType());
        } else {
            rv.emplace_back(id);
        }
    }

    return rv;
}

CPSG_BlobId SRequestBuilder::SReader<SPsgCgiEntries>::GetBlobId() const
{
    const auto& id = input.GetString("id");
    return input.Get("last-modified", [&](const auto& v) { return CPSG_BlobId(id, NStr::StringToInt8(v)); }, id);
}

CPSG_ChunkId SRequestBuilder::SReader<SPsgCgiEntries>::GetChunkId() const
{
    return { input.Get("id2-chunk", [](const auto& v) { return NStr::StringToInt(v); }, 0), input.GetString("id2-info") };
}

void SRequestBuilder::SReader<SPsgCgiEntries>::ForEachTSE(TExclude exclude) const
{
    auto blob_ids = input.GetStringList("exclude-blob");

    for (const auto& blob_id : blob_ids) {
        exclude(blob_id);
    }
}

void SRequestBuilder::SReader<SPsgCgiEntries>::SetRequestFlags(shared_ptr<CPSG_Request> request) const
{
    if (input.Has("include-hup")) {
        request->SetFlags(CPSG_Request::fIncludeHUP);
    }
}

struct SResponse
{
    SResponse(CCgiResponse& response) :
        m_Response(response),
        m_Out(cout),
        m_Err(cerr)
    {}

    void DataOnly(const SOneRequestParams::SDataOnly& data_only)
    {
        if (data_only.enabled) {
            m_Format = data_only.output_format;
        }
    }

    void operator()(int status)
    {
        m_Out.Reset();
        m_Err.Reset();
        return status != CRequestStatus::e200_Ok ? Error(status) : Success();
    }

private:
    struct SSsR : stringstream, SIoRedirector
    {
        SSsR(ios& what) : SIoRedirector(what, *this) {}
    };

    void Success();
    void Error(int status);

    CCgiResponse& m_Response;
    SSsR m_Out;
    SSsR m_Err;
    optional<ESerialDataFormat> m_Format;
};

void SResponse::Success()
{
    switch (m_Format.value_or(eSerial_Json))
    {
        case eSerial_None:
            m_Response.SetContentType("application/octet-stream");
            break;

        case eSerial_AsnBinary:
            m_Response.SetContentType("x-ncbi-data/x-asn-binary");
            break;

        case eSerial_AsnText:
            m_Response.SetContentType("x-ncbi-data/x-asn-text");
            break;

        case eSerial_Xml:
            m_Response.SetContentType("text/xml");
            break;

        case eSerial_Json:
            m_Response.SetContentType("application/json");
            break;
    }

    m_Response.WriteHeader() << m_Out.rdbuf();

    // Output debug if there is any
    cerr << m_Err.rdbuf();
}

void SResponse::Error(int status)
{
    // If it's data-only
    if (m_Format) {
        m_Response.SetContentType("application/problem+json");
        CJson_Document doc;
        auto obj = doc.SetObject();
        obj["status"].SetValue().SetInt8(status);
        obj["detail"].SetValue().SetString(m_Err.str());
        m_Response.WriteHeader() << doc;
    } else {
        m_Response.WriteHeader() << m_Err.rdbuf();
    }
}

class CPsgCgiApp : public CCgiApplication
{
    void Init() override;
    int ProcessRequest(CCgiContext& ctx) override;
    int Help(const string& request, bool json, CCgiResponse& response);

    static int GetStatus(int rv);
    static void AddParamsTable(CHTML_body* body, const string& name, const CJson_ConstObject_pair& params);

    template <class TChild, class TParent, class... TArgs>
    static TChild* NewChild(TParent* parent, TArgs&&... args)
    {
        _ASSERT(parent);
        auto child = new TChild(std::forward<TArgs>(args)...);
        parent->AppendChild(child);
        return child;
    }

private:
    CPSG_Queue::TApiLock m_ApiLock;
};

void CPsgCgiApp::Init()
{
    SetRequestFlags(CCgiRequest::fDoNotParseContent | CCgiRequest::fDisableParsingAsIndex);
    TPSG_UserRequestIds::SetDefault(true);

    m_ApiLock = CPSG_Queue::GetApiLock();
}

int CPsgCgiApp::ProcessRequest(CCgiContext& ctx)
{
    const auto& request = ctx.GetRequest();
    SPsgCgiEntries entries = request.GetEntries();
    const auto type = entries.GetString("request");

    if (type.empty() || entries.Has("help")) {
        return Help(type, entries.Has("json"), ctx.GetResponse());
    }

    int rv;
    SResponse response = ctx.GetResponse();

    if (auto is = request.GetRequestMethod() == CCgiRequest::eMethod_POST ? request.GetInputStream() : nullptr) {
        if (type == "resolve") {
            rv = CProcessing::ParallelProcessing(NParamsBuilder::SBatchResolve(entries), *is);
        } else if (type == "ipg_resolve") {
            rv = CProcessing::ParallelProcessing(NParamsBuilder::TIpgBatchResolve(entries), *is);
        } else {
            rv = int(EPSG_Status::eError);
        }
    } else {
        const auto builder = NParamsBuilder::SOneRequest(entries);

        response.DataOnly(builder.data_only);
        rv = CProcessing::OneRequest(builder, SRequestBuilder::Build(type, entries));
    }

    const auto status = GetStatus(rv);
    SetHTTPStatus(status);
    response(status);
    return rv;
}

int CPsgCgiApp::Help(const string& request, bool json, CCgiResponse& response)
{
    CJson_Document help_doc;

    if (!help_doc.Read("pubseq_gateway.json")) {
        SetHTTPStatus(CRequestStatus::e500_InternalServerError);
        response.WriteHeader() << "Missing 'pubseq_gateway.json'\n";
        return 0;
    }

    auto help_obj = help_doc.SetObject();
    auto request_obj = help_obj["request"].SetObject();

    // Do not display unrelated requests
    if (request_obj.has(request)) {
        auto i = request_obj.begin();

        while (i != request_obj.end()) {
            if (i->name != request) {
                i = request_obj.erase(i);
            } else {
                ++i;
            }
        }
    }

    if (json) {
        response.SetContentType("application/json");
        response.WriteHeader() << help_doc;
        return 0;
    }

    CRef<CHTML_html> html(new CHTML_html);
    NewChild<CHTML_title>(html.GetPointer())->AppendPlainText("help");
    auto body = NewChild<CHTML_body>(html.GetPointer());

    NewChild<CHTML_h2>(body)->AppendPlainText("pubseq_gateway.cgi");
    NewChild<CHTMLPlainText>(body, "A user-friendly CGI gateway to PSG.");

    auto i = help_obj.find("Common Parameters");

    if (i != help_obj.end()) {
        AddParamsTable(body, i->name, *i);
    }

    for (const auto& r : request_obj) {
        NewChild<CHTML_hr>(body);

        auto h2 = NewChild<CHTML_h2>(body);
        NewChild<CHTML_i>(h2)->AppendPlainText("request");
        h2->AppendPlainText(string("=") + r.name);

        auto r_obj = r.value.GetObject();
        auto k = r_obj.find("method");

        if (k != r_obj.end()) {
            for (const auto& m : k->value.GetObject()) {
                auto m_obj = m.value.GetObject();
                NewChild<CHTML_h3>(body, m.name + string(":"));
                NewChild<CHTMLPlainText>(body, m_obj["description"].GetValue().GetString());

                auto j = m_obj.find("Method-specific Parameters");

                if (j != m_obj.end()) {
                    AddParamsTable(body, m.name + string("-specific Parameters"), *j);
                }
            }
        } else {
            NewChild<CHTMLPlainText>(body, r_obj["description"].GetValue().GetString());
        }

        auto j = r_obj.find("Request-specific Parameters");

        if (j != r_obj.end()) {
            AddParamsTable(body, j->name, *j);
        }
    }

    html->Print(response.WriteHeader());
    response.Flush();
    return 0;
}

int CPsgCgiApp::GetStatus(int rv)
{
    switch (rv)
    {
        case int(EPSG_Status::eSuccess):   return CRequestStatus::e200_Ok;
        case int(EPSG_Status::eNotFound):  return CRequestStatus::e404_NotFound;
        case int(EPSG_Status::eForbidden): return CRequestStatus::e403_Forbidden;
        default:                           return CRequestStatus::e400_BadRequest;
    }
}

void CPsgCgiApp::AddParamsTable(CHTML_body* body, const string& name, const CJson_ConstObject_pair& params)
{
    NewChild<CHTML_h4>(body, name);
    auto table = NewChild<CHTML_table>(body);

    auto r = 0;

    for (const auto& type : params.value.GetObject()) {
        auto header = table->HeaderCell(r, 0);
        header->AppendPlainText(type.name);
        header->SetColSpan(2);
        ++r;

        for (const auto& param : type.value.GetObject()) {
            NewChild<CHTML_i>(table->DataCell(r, 0))->AppendPlainText(param.name);
            table->DataCell(r, 1)->AppendPlainText(param.value.GetValue().GetString());
            ++r;
        }
    }
}

int main(int argc, const char* argv[])
{
    return CPsgCgiApp().AppMain(argc, argv);
}
