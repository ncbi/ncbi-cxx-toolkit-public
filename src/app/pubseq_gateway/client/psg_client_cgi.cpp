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

#include "processing.hpp"
#include "performance.hpp"

USING_NCBI_SCOPE;

struct SPsgCgiEntries
{
    const TCgiEntries& entries;

    SPsgCgiEntries(const TCgiEntries& e) : entries(e) {}

    template <typename TF>
    invoke_result_t<TF, const string&> Get(const string& key, TF f, invoke_result_t<TF, const string&> def_value) const;

    const string& GetString(const string& key) const { return Get(key, [](const auto& v) -> const auto& { return v; }, kEmptyStr); }
    bool GetBool(const string& key) const { return Get(key, NStr::StringToBool, false); }
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
            GetService(entries),
            SPSG_UserArgs(),
            forward<TInitArgs>(init_args)...
        }
    {
        TParams::verbose = entries.GetBool("verbose");
    }

    static auto GetService(const SPsgCgiEntries& entries)
    {
        const auto& service = entries.GetString("service");
        return service.empty() ? "psg2" : service;
    }
};

struct SOneRequest : SBase<SOneRequestParams>
{
    SOneRequest(const SPsgCgiEntries& entries) :
        SBase{
            entries,
            false,
            false,
            entries.GetBool("blob-only") || entries.GetBool("annot-only"),
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
            1,
            false,
            false,
            forward<TInitArgs>(init_args)...
        }
    {
    }
};

struct SBatchResolve : SParallelProcessing<SBatchResolveParams>
{
    SBatchResolve(const SPsgCgiEntries& entries) :
        SParallelProcessing<SBatchResolveParams>{
            entries,
            entries.Get("type", SRequestBuilder::GetBioIdType, CPSG_BioId::TType()),
            SRequestBuilder::GetIncludeInfo(entries)
        }
    {
    }
};

}

template <>
struct SRequestBuilder::SReader<SPsgCgiEntries>
{
    const SPsgCgiEntries& input;

    SReader(const SPsgCgiEntries& i) : input(i) {}

    TSpecified GetSpecified() const;
    CPSG_BioId GetBioId() const;
    CPSG_BlobId GetBlobId() const;
    CPSG_ChunkId GetChunkId() const;
    vector<string> GetNamedAnnots() const { return input.GetStringList("na"); }
    string GetAccSubstitution() const { return input.GetString("acc-substitution"); }
    CTimeout GetResendTimeout() const { return CTimeout::eDefault; }
    void ForEachTSE(TExclude exclude) const;
    SPSG_UserArgs GetUserArgs() const { return NStr::URLDecode(input.GetString("user-args")); }
};

SRequestBuilder::TSpecified SRequestBuilder::SReader<SPsgCgiEntries>::GetSpecified() const
{
    return [&](const string& name) {
        return input.entries.find(name) != input.entries.end();
    };
}

CPSG_BioId SRequestBuilder::SReader<SPsgCgiEntries>::GetBioId() const
{
    const auto& id = input.GetString("id");
    return input.Get("type", [&](const auto& v) { return CPSG_BioId(id, GetBioIdType(v)); }, id);
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

    static int GetStatus(int rv);
};

void CPsgCgiApp::Init()
{
    SetRequestFlags(CCgiRequest::fDoNotParseContent);
}

int CPsgCgiApp::ProcessRequest(CCgiContext& ctx)
{
    const auto& request = ctx.GetRequest();
    SPsgCgiEntries entries = request.GetEntries();

    int rv;
    SResponse response = ctx.GetResponse();

    if (auto is = request.GetRequestMethod() == CCgiRequest::eMethod_POST ? request.GetInputStream() : nullptr) {
        rv = CProcessing::ParallelProcessing(NParamsBuilder::SBatchResolve(entries), *is);
    } else {
        const auto builder = NParamsBuilder::SOneRequest(entries);

        const auto type = entries.GetString("request");
        auto request = SRequestBuilder::Build(type, entries);

        response.DataOnly(builder.data_only);
        rv = CProcessing::OneRequest(builder, move(request));
    }

    const auto status = GetStatus(rv);
    SetHTTPStatus(status);
    response(status);
    return rv;
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

int main(int argc, const char* argv[])
{
    return CPsgCgiApp().AppMain(argc, argv);
}
