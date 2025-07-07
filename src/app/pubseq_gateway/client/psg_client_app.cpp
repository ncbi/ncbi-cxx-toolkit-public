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

#include <unordered_map>

#include <corelib/ncbiapp.hpp>
#include <connect/services/grid_app_version_info.hpp>

#include "processing.hpp"
#include "performance.hpp"

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

class CPsgClientApp;

struct SCommand
{
    using TInit = function<void(CArgDescriptions&)>;
    using TRun = function<int(CPsgClientApp*, const CArgs&)>;
    enum EFlags { eDefault, fHidden = 1 << 0, fParallel = 1 << 1, fNoApi = 1 << 2 };

    const string name;
    const string desc;
    TInit init;
    TRun run;
    int flags;

    SCommand(string n, string d, TInit i, TRun r, int f) : name(n), desc(d), init(i), run(r), flags(f) {}
};

class CPsgClientApp : public CNcbiApplication
{
public:
    CPsgClientApp();
    virtual void Init();
    virtual int Run();

private:
    template <class TRequest>
    static SCommand s_GetCommand(string name, string desc, int flags = SCommand::eDefault);

    template <class TRequest>
    static void s_InitRequest(CArgDescriptions& arg_desc);

    template <class TRequest>
    static int s_RunRequest(CPsgClientApp* that, const CArgs& args) { _ASSERT(that); return that->RunRequest<TRequest>(args); }

    template <class TRequest>
    int RunRequest(const CArgs& args);

    vector<SCommand> m_Commands;
};

struct SInteractive {};
struct SInteractiveSchema {};
struct SPerformance {};
struct SJsonCheck {};

void s_InitPsgOptions(CArgDescriptions& arg_desc);
void s_SetPsgDefaults(const CArgs& args, bool parallel);

CPsgClientApp::CPsgClientApp() :
    m_Commands({
            s_GetCommand<CPSG_Request_Biodata>       ("biodata",     "Request biodata info and data by bio ID"),
            s_GetCommand<CPSG_Request_Blob>          ("blob",        "Request blob by blob ID"),
            s_GetCommand<CPSG_Request_Resolve>       ("resolve",     "Request biodata info by bio ID", SCommand::fParallel),
            s_GetCommand<CPSG_Request_NamedAnnotInfo>("named_annot", "Request named annotations info by bio ID(s)"),
            s_GetCommand<CPSG_Request_Chunk>         ("chunk",       "Request blob data chunk by chunk ID"),
            s_GetCommand<CPSG_Request_IpgResolve>    ("ipg_resolve", "Request IPG info", SCommand::fParallel),
            s_GetCommand<CPSG_Request_AccVerHistory> ("acc_ver_history", "Request accession version history"),
            s_GetCommand<SInteractive>               ("interactive", "Interactive JSON-RPC mode", SCommand::fParallel),
            s_GetCommand<SInteractiveSchema>         ("interactive_schema", "Output JSON schema for JSON-RPC requests", SCommand::fNoApi),
            s_GetCommand<SPerformance>               ("performance", "Performance testing", SCommand::fHidden),
            s_GetCommand<SJsonCheck>                 ("json_check",  "JSON document validate", SCommand::fHidden | SCommand::fNoApi),
        })
{
}

void CPsgClientApp::Init()
{
    const auto kFlags = CCommandArgDescriptions::eCommandOptional | CCommandArgDescriptions::eNoSortCommands;
    unique_ptr<CCommandArgDescriptions> cmd_desc(new CCommandArgDescriptions(true, nullptr, kFlags));

    cmd_desc->SetUsageContext(GetArguments().GetProgramBasename(), "PSG client");

    for (const auto& command : m_Commands) {
        unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());
        arg_desc->SetUsageContext("", command.desc);

        if (~command.flags & SCommand::fNoApi) {
            s_InitPsgOptions(*arg_desc);
        }

        command.init(*arg_desc);
        const auto hidden = command.flags & SCommand::fHidden;
        const auto flags = hidden ? CCommandArgDescriptions::eHidden : CCommandArgDescriptions::eDefault;
        cmd_desc->AddCommand(command.name, arg_desc.release(), kEmptyStr, flags);
    }

    SetupArgDescriptions(cmd_desc.release());
}

int CPsgClientApp::Run()
{
    const auto& args = GetArgs();
    auto name = args.GetCommand();

    for (const auto& command : m_Commands) {
        if (command.name == name) {
            const bool parallel = command.flags & SCommand::fParallel;

            if (~command.flags & SCommand::fNoApi) {
                s_SetPsgDefaults(args, parallel);
            }

            return command.run(this, args);
        }
    }

    NCBI_THROW(CArgException, eNoArg, "Command is required");
    return -1;
}

void s_InitDataFlags(CArgDescriptions& arg_desc)
{
    const auto& data_flags = SRequestBuilder::GetDataFlags();

    for (auto i = data_flags.begin(); i != data_flags.end(); ++i) {
        arg_desc.AddFlag(i->name, i->desc);

        for (auto j = i + 1; j != data_flags.end(); ++j) {
            if (i->name != j->name) arg_desc.SetDependency(i->name, CArgDescriptions::eExcludes, j->name);
        }
    }
}

void s_AddLatencyOptions(CArgDescriptions& arg_desc)
{
    auto names = { "First", "Last", "All" };
    auto option = [](string name) { return NStr::ToLower(name) + "-latency"; };

    for (string name : names) {
        arg_desc.AddFlag(option(name), name + " latency output", CArgDescriptions::eFlagHasValueIfSet, CArgDescriptions::fHidden);

        for (string excluded : names) {
            if (name != excluded) {
                arg_desc.SetDependency(option(name), CArgDescriptions::eExcludes, option(excluded));
            }
        }
    }

    arg_desc.AddAlias("latency", "last-latency");
}

void s_InitPsgOptions(CArgDescriptions& arg_desc)
{
    arg_desc.AddOptionalKey("service", "SERVICE", "PSG service name or host:port pair", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("io-threads", "THREADS_NUM", "Number of I/O threads", CArgDescriptions::eInteger, CArgDescriptions::fHidden);
    arg_desc.AddOptionalKey("requests-per-io", "REQUESTS_NUM", "Number of requests to submit consecutively per I/O thread", CArgDescriptions::eInteger, CArgDescriptions::fHidden);
    arg_desc.AddOptionalKey("max-streams", "REQUESTS_NUM", "Maximum number of concurrent streams per I/O thread", CArgDescriptions::eInteger, CArgDescriptions::fHidden);
    arg_desc.AddOptionalKey("use-cache", "USE_CACHE", "Whether to use LMDB cache (no|yes|default)", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("timeout", "SECONDS", "Set request timeout (in seconds)", CArgDescriptions::eInteger);
    arg_desc.AddOptionalKey("debug-printout", "WHAT", "Debug printout of PSG protocol (some|frames|all).", CArgDescriptions::eString, CArgDescriptions::fHidden);
    arg_desc.AddOptionalKey("user-args", "USER_ARGS", "Arbitrary request URL arguments (queue-wide)", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("test-identity", "IDENTITY", "Set test identity.", CArgDescriptions::eString, CArgDescriptions::fHidden);
    arg_desc.AddDefaultKey("min-severity", "SEVERITY", "Minimum severity level of messages to output", CArgDescriptions::eString, "Warning");
    arg_desc.AddFlag("include-hup", "Include HUP data");
    arg_desc.AddFlag("https", "Enable HTTPS");
    s_AddLatencyOptions(arg_desc);
    arg_desc.AddFlag("verbose", "Verbose output");
}

void s_InitDataOnly(CArgDescriptions& arg_desc, bool blob_only = true)
{
    const auto name = blob_only ? "blob-only" : "annot-only";
    const auto comment = blob_only ? "Output blob data only" : "Output annot info only";
    arg_desc.AddFlag(name, comment);
    arg_desc.AddOptionalKey("output-fmt", "FORMAT", "Format for blob data to return in (instead of raw data)", CArgDescriptions::eString);
    arg_desc.SetConstraint("output-fmt", new CArgAllow_Strings{"asn", "asnb", "xml", "json"});
    arg_desc.SetDependency("output-fmt", CArgDescriptions::eRequires, name);
}

template <class TRequest>
void CPsgClientApp::s_InitRequest(CArgDescriptions& arg_desc)
{
    arg_desc.AddOptionalKey("exclude-blob", "BLOB_ID", "Exclude blob by its ID", CArgDescriptions::eString, CArgDescriptions::fAllowMultiple);
    arg_desc.AddPositional("ID", "ID part of Bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("type", "TYPE", "Type part of bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("acc-substitution", "ACC_SUB", "ACC substitution", CArgDescriptions::eString);
    arg_desc.AddFlag("no-bio-id-resolution", "Do not try to resolve provided bio ID(s) before use");
    s_InitDataOnly(arg_desc);
    s_InitDataFlags(arg_desc);
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_Resolve>(CArgDescriptions& arg_desc)
{
    arg_desc.AddOptionalPositional("ID", "Bio ID", CArgDescriptions::eString);
    arg_desc.AddDefaultKey("input-file", "FILENAME", "File containing bio IDs to resolve (one per line)", CArgDescriptions::eInputFile, "-");
    arg_desc.SetDependency("input-file", CArgDescriptions::eExcludes, "ID");
    arg_desc.AddAlias("id-file", "input-file");
    arg_desc.AddFlag("server-mode", "Output one response per line");
    arg_desc.SetDependency("server-mode", CArgDescriptions::eExcludes, "ID");
    arg_desc.AddOptionalKey("type", "TYPE", "Type of bio ID(s)", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("acc-substitution", "ACC_SUB", "ACC substitution", CArgDescriptions::eString);
    arg_desc.AddFlag("no-bio-id-resolution", "Do not try to resolve provided bio ID(s) before use");
    arg_desc.AddDefaultKey("rate", "RATE", "Maximum number of requests to submit per second", CArgDescriptions::eDouble, "0.0");
    arg_desc.AddDefaultKey("worker-threads", "THREADS_CONF", "Numbers of worker threads of each type", CArgDescriptions::eInteger, "7", CArgDescriptions::fHidden);

    for (const auto& f : SRequestBuilder::GetInfoFlags()) {
        arg_desc.AddFlag(f.name, f.desc);
    }
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_Blob>(CArgDescriptions& arg_desc)
{
    arg_desc.AddPositional("ID", "Blob ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("last-modified", "LAST_MODIFIED", "LastModified", CArgDescriptions::eInt8);
    s_InitDataOnly(arg_desc);
    s_InitDataFlags(arg_desc);
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_NamedAnnotInfo>(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("na", "NAMED_ANNOT", "Named annotation", CArgDescriptions::eString, CArgDescriptions::fAllowMultiple);
    arg_desc.AddExtra(1, kMax_UInt, "Bio ID(s)", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("type", "TYPE", "Type of the first bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("acc-substitution", "ACC_SUB", "ACC substitution", CArgDescriptions::eString);
    arg_desc.AddFlag("no-bio-id-resolution", "Do not try to resolve provided bio ID(s) before use");
    arg_desc.AddOptionalKey("snp-scale-limit", "LIMIT", "SNP scale limit", CArgDescriptions::eString);
    arg_desc.SetConstraint("snp-scale-limit", new CArgAllow_Strings{"unit", "contig", "supercontig", "chromosome"});
    s_InitDataOnly(arg_desc, false);
    s_InitDataFlags(arg_desc);
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_Chunk>(CArgDescriptions& arg_desc)
{
    arg_desc.AddPositional("ID2_CHUNK", "ID2 chunk number", CArgDescriptions::eInteger);
    arg_desc.AddPositional("ID2_INFO", "ID2 info", CArgDescriptions::eString);
    s_InitDataOnly(arg_desc);
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_IpgResolve>(CArgDescriptions& arg_desc)
{
    arg_desc.AddOptionalKey("protein", "PROTEIN", "Protein", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("ipg", "IPG", "IPG", CArgDescriptions::eInt8);
    arg_desc.AddOptionalKey("nucleotide", "NUCLEOTIDE", "Nucleotide", CArgDescriptions::eString);
    arg_desc.AddDefaultKey("input-file", "FILENAME", "File containing proteins to resolve (one 'protein[,nucleotide]' per line)", CArgDescriptions::eInputFile, "-");
    arg_desc.SetDependency("input-file", CArgDescriptions::eExcludes, "protein");
    arg_desc.SetDependency("input-file", CArgDescriptions::eExcludes, "ipg");
    arg_desc.SetDependency("input-file", CArgDescriptions::eExcludes, "nucleotide");
    arg_desc.AddFlag("server-mode", "Output one response per line");
    arg_desc.AddDefaultKey("rate", "RATE", "Maximum number of requests to submit per second", CArgDescriptions::eDouble, "0.0");
    arg_desc.AddDefaultKey("worker-threads", "THREADS_CONF", "Numbers of worker threads of each type", CArgDescriptions::eInteger, "7", CArgDescriptions::fHidden);
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_AccVerHistory>(CArgDescriptions& arg_desc)
{
    arg_desc.AddPositional("ID", "ID part of Bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("type", "TYPE", "Type part of bio ID", CArgDescriptions::eString);
}

template<>
void CPsgClientApp::s_InitRequest<SInteractive>(CArgDescriptions& arg_desc)
{
    arg_desc.AddDefaultKey("input-file", "FILENAME", "File containing JSON-RPC requests (one per line)", CArgDescriptions::eInputFile, "-");
    arg_desc.AddOptionalKey("data-limit", "SIZE", "Show a data preview for any data larger the limit (if set)", CArgDescriptions::eDataSize);
    arg_desc.AddDefaultKey("preview-size", "SIZE", "How much of data to show as the preview", CArgDescriptions::eDataSize, "16");
    arg_desc.AddFlag("server-mode", "Output one JSON-RPC response per line and always output reply statuses");
    arg_desc.AddFlag("echo", "Echo all incoming requests");
    arg_desc.AddFlag("one-server", "Use only one server from the service", CArgDescriptions::eFlagHasValueIfSet, CArgDescriptions::fHidden);
    arg_desc.AddFlag("testing", "Testing mode adjustments", CArgDescriptions::eFlagHasValueIfSet, CArgDescriptions::fHidden);
    arg_desc.AddDefaultKey("rate", "RATE", "Maximum number of requests to submit per second", CArgDescriptions::eDouble, "0.0");
    arg_desc.AddDefaultKey("worker-threads", "THREADS_CONF", "Numbers of worker threads of each type", CArgDescriptions::eInteger, "7", CArgDescriptions::fHidden);
    arg_desc.SetDependency("preview-size", CArgDescriptions::eRequires, "data-limit");
}

template<>
void CPsgClientApp::s_InitRequest<SInteractiveSchema>(CArgDescriptions& arg_desc)
{
    arg_desc.AddDefaultKey("output-file", "FILENAME", "Output file to contain schema", CArgDescriptions::eOutputFile, "-");
}

template <>
void CPsgClientApp::s_InitRequest<SPerformance>(CArgDescriptions& arg_desc)
{
    arg_desc.AddDefaultKey("user-threads", "THREADS_NUM", "Number of user threads", CArgDescriptions::eInteger, "1");
    arg_desc.AddDefaultKey("delay", "SECONDS", "Delay between consecutive requests (in seconds)", CArgDescriptions::eDouble, "0.0");
    arg_desc.AddFlag("local-queue", "Whether user threads to use separate queues");
    arg_desc.AddFlag("report-immediately", "Whether to report metrics immediately (or at the end)");
    arg_desc.AddDefaultKey("output-file", "FILENAME", "Output file to contain raw performance metrics", CArgDescriptions::eOutputFile, "-");
}

template <>
void CPsgClientApp::s_InitRequest<SJsonCheck>(CArgDescriptions& arg_desc)
{
    arg_desc.AddOptionalKey("schema-file", "FILENAME", "JSON schema file (default: schema for JSON-RPC requests)", CArgDescriptions::eInputFile);
    arg_desc.AddDefaultKey("input-file", "FILENAME", "JSON file", CArgDescriptions::eInputFile, "-");
    arg_desc.AddFlag("single-doc", "Treat input as a single JSON document (instead of one per line)");
}

void s_SetPsgDefaults(const CArgs& args, bool parallel)
{
    TPSG_UserRequestIds::SetDefault(true);

    if (args["io-threads"].HasValue()) {
        auto io_threads = args["io-threads"].AsInteger();
        TPSG_NumIo::SetDefault(io_threads);
    }

    if (args["requests-per-io"].HasValue()) {
        auto requests_per_io = args["requests-per-io"].AsInteger();
        TPSG_RequestsPerIo::SetDefault(requests_per_io);
    } else if (parallel) {
        TPSG_RequestsPerIo::SetImplicitDefault(4);
    }

    if (args["max-streams"].HasValue()) {
        auto max_streams = args["max-streams"].AsInteger();
        TPSG_MaxConcurrentStreams::SetDefault(max_streams);
    }

    if (args["use-cache"].HasValue()) {
        auto use_cache = args["use-cache"].AsString();
        TPSG_UseCache::SetDefault(use_cache);
    }

    if (args["timeout"].HasValue()) {
        auto timeout = args["timeout"].AsInteger();
        TPSG_RequestTimeout::SetDefault(timeout);
    }

    if (args["https"].HasValue()) {
        TPSG_Https::SetDefault(true);
    }

    if (args["debug-printout"].HasValue()) {
        auto debug_printout = args["debug-printout"].AsString();
        TPSG_DebugPrintout::SetDefault(debug_printout);
    }

    if (args["test-identity"].HasValue()) {
        const auto& test_identity = args["test-identity"].AsString();
        CPSG_Misc::SetTestIdentity(test_identity);
    }
}

namespace NParamsBuilder
{

template <class TParams>
struct SBase : TParams
{
    template <class... TInitArgs>
    SBase(const CArgs& args, TInitArgs&&... init_args) :
        TParams{
            args["service"].HasValue() ? args["service"].AsString() : string(),
            args["include-hup"].HasValue() ? CPSG_Request::fIncludeHUP : CPSG_Request::eDefaultFlags,
            args["user-args"].HasValue() ? args["user-args"].AsString() : SPSG_UserArgs(),
            std::forward<TInitArgs>(init_args)...
        }
    {
        if (!CNcbiDiag::StrToSeverityLevel(args["min-severity"].AsString().c_str(), TParams::min_severity)) {
            TParams::min_severity = eDiag_Warning;
        }

        TParams::verbose = args["verbose"].HasValue();
    }
};

struct SOneRequest : SBase<SOneRequestParams>
{
    SOneRequest(const CArgs& args) :
        SBase{
            args,
            GetLatency(args),
            args["debug-printout"].HasValue(),
            GetDataOnlyEnabled(args),
            false,
            GetDataOnlyOutputFormat(args)
        }
    {
    }

    CLogLatencies::EWhich GetLatency(const CArgs& args)
    {
        if (args["first-latency"].HasValue()) return CLogLatencies::eFirst;
        if (args["last-latency"].HasValue())  return CLogLatencies::eLast;
        if (args["all-latency"].HasValue())   return CLogLatencies::eAll;
        return CLogLatencies::eOff;
    }

    static bool GetDataOnlyEnabled(const CArgs& args)
    {
        return (args.Exist("blob-only") && args["blob-only"].HasValue()) ||
            (args.Exist("annot-only") && args["annot-only"].HasValue());
    }

    static ESerialDataFormat GetDataOnlyOutputFormat(const CArgs& args)
    {
        if (args.Exist("output-fmt") && args["output-fmt"].HasValue()) {
            const auto& format = args["output-fmt"].AsString();

            if (format == "asn")  return eSerial_AsnText;
            if (format == "asnb") return eSerial_AsnBinary;
            if (format == "xml")  return eSerial_Xml;
            if (format == "json") return eSerial_Json;
        }

        return eSerial_None;
    }

};

template <class TParams>
struct SParallelProcessing : SBase<TParams>, SIoRedirector
{
    template <class... TInitArgs>
    SParallelProcessing(const CArgs& args, TInitArgs&&... init_args) :
        SBase<TParams>{
            args,
            args["rate"].AsDouble(),
            max(1, min(10, args["worker-threads"].AsInteger())),
            args["input-file"].AsString() == "-",
            args["server-mode"].AsBoolean(),
            std::forward<TInitArgs>(init_args)...
        },
        SIoRedirector(cin, args["input-file"].AsInputFile())
    {
    }
};

struct SBatchResolve : SParallelProcessing<SBatchResolveParams>
{
    SBatchResolve(const CArgs& args) :
        SParallelProcessing<SBatchResolveParams>{
            args,
            SRequestBuilder::GetResolveParams(args)
        }
    {
    }
};

using TIpgBatchResolve = SParallelProcessing<TIpgBatchResolveParams>;

struct SInteractive : SParallelProcessing<SInteractiveParams>
{
    SInteractive(const CArgs& args) :
        SParallelProcessing<SInteractiveParams>{
            args,
            GetDataLimit(args["data-limit"]),
            static_cast<size_t>(args["preview-size"].AsInt8()),
            args["echo"].HasValue(),
            args["one-server"].HasValue(),
            args["testing"].HasValue()
        }
    {
    }

    static size_t GetDataLimit(const CArgValue& value)
    {
        return value.HasValue() ? static_cast<size_t>(value.AsInt8()) : numeric_limits<size_t>::max();
    }
};

struct SPerformance : SBase<SPerformanceParams>, SIoRedirector
{
    SPerformance(const CArgs& args) :
        SBase<SPerformanceParams>{
            args,
            static_cast<size_t>(args["user-threads"].AsInteger()),
            args["delay"].AsDouble(),
            args["local-queue"].AsBoolean(),
            args["report-immediately"].AsBoolean(),
        },
        SIoRedirector(cout, args["output-file"].AsOutputFile())
    {
    }
};

}

template <>
struct SRequestBuilder::SReader<CArgs>
{
    const CArgs& input;

    SReader(const CArgs& i) : input(i) {}

    TSpecified GetSpecified() const;
    auto GetBioIdType() const { return input["type"].HasValue() ? SRequestBuilder::GetBioIdType(input["type"].AsString()) : CPSG_BioId::TType(); }
    CPSG_BioId GetBioId() const { return { input["ID"].AsString(), GetBioIdType() }; }
    CPSG_BioIds GetBioIds() const;
    CPSG_BlobId GetBlobId() const;
    CPSG_ChunkId GetChunkId() const;
    vector<string> GetNamedAnnots() const { return input["na"].GetStringList(); }
    auto GetAccSubstitution() const { return input["acc-substitution"].HasValue() ? SRequestBuilder::GetAccSubstitution(input["acc-substitution"].AsString()) : EPSG_AccSubstitution::Default; }
    EPSG_BioIdResolution GetBioIdResolution() const { return input["no-bio-id-resolution"].HasValue() ? EPSG_BioIdResolution::NoResolve : EPSG_BioIdResolution::Resolve; }
    CTimeout GetResendTimeout() const { return CTimeout::eDefault; }
    void ForEachTSE(TExclude exclude) const;
    auto GetProtein() const { return input["protein"].HasValue() ? input["protein"].AsString() : string(); }
    auto GetIpg() const { return input["ipg"].HasValue() ? input["ipg"].AsInt8() : 0; }
    auto GetNucleotide() const { return input["nucleotide"].HasValue() ? CPSG_Request_IpgResolve::TNucleotide(input["nucleotide"].AsString()) : null; }
    auto GetSNPScaleLimit() const { return input["snp-scale-limit"].HasValue() ? objects::CSeq_id::GetSNPScaleLimit_Value(input["snp-scale-limit"].AsString()) : CPSG_Request_NamedAnnotInfo::ESNPScaleLimit::eSNPScaleLimit_Default; }
    void SetRequestFlags(shared_ptr<CPSG_Request> request) const;
    SPSG_UserArgs GetUserArgs() const { return input["user-args"].HasValue() ? input["user-args"].AsString() : SPSG_UserArgs(); }
};

SRequestBuilder::TSpecified SRequestBuilder::SReader<CArgs>::GetSpecified() const
{
    return [&](const string& name) {
        return input[name].HasValue();
    };
}

CPSG_BioIds SRequestBuilder::SReader<CArgs>::GetBioIds() const
{
    const size_t n = input.GetNExtra();

    CPSG_BioIds rv;

    for (size_t i = 1; i <= n; ++i) {
        if (i == 1) {
            rv.emplace_back(input[i].AsString(), GetBioIdType());
        } else {
            rv.emplace_back(input[i].AsString());
        }
    }

    return rv;
}

CPSG_BlobId SRequestBuilder::SReader<CArgs>::GetBlobId() const
{
    const auto& id = input["ID"].AsString();
    const auto& last_modified = input["last-modified"];
    return last_modified.HasValue() ? CPSG_BlobId(id, last_modified.AsInt8()) : id;
}

CPSG_ChunkId SRequestBuilder::SReader<CArgs>::GetChunkId() const
{
    return { input["ID2_CHUNK"].AsInteger(), input["ID2_INFO"].AsString() };
}

void SRequestBuilder::SReader<CArgs>::ForEachTSE(TExclude exclude) const
{
    if (!input["exclude-blob"].HasValue()) return;

    auto blob_ids = input["exclude-blob"].GetStringList();

    for (const auto& blob_id : blob_ids) {
        exclude(blob_id);
    }
}

void SRequestBuilder::SReader<CArgs>::SetRequestFlags(shared_ptr<CPSG_Request> request) const
{
    if (input["include-hup"].HasValue()) {
        request->SetFlags(CPSG_Request::fIncludeHUP);
    }
}

template <class TRequest>
int CPsgClientApp::RunRequest(const CArgs& args)
{
    auto request = SRequestBuilder::Build<TRequest>(args);
    return CProcessing::OneRequest(NParamsBuilder::SOneRequest(args), request);
}

template<>
int CPsgClientApp::RunRequest<CPSG_Request_Resolve>(const CArgs& args)
{
    const auto single_request = args["ID"].HasValue();

    if (single_request) {
        auto request = SRequestBuilder::Build<CPSG_Request_Resolve>(args);
        return CProcessing::OneRequest(NParamsBuilder::SOneRequest(args), request);
    } else {
        return CProcessing::ParallelProcessing(NParamsBuilder::SBatchResolve(args));
    }
}

template<>
int CPsgClientApp::RunRequest<CPSG_Request_IpgResolve>(const CArgs& args)
{
    const auto single_request = args["protein"].HasValue() || args["ipg"].HasValue() || args["nucleotide"].HasValue();

    if (single_request) {
        auto request = SRequestBuilder::Build<CPSG_Request_IpgResolve>(args);
        return CProcessing::OneRequest(NParamsBuilder::SOneRequest(args), request);
    } else {
        return CProcessing::ParallelProcessing(NParamsBuilder::TIpgBatchResolve(args));
    }
}

template<>
int CPsgClientApp::RunRequest<SInteractive>(const CArgs& args)
{
    NParamsBuilder::SInteractive params_builder(args);

    if (params_builder.testing) {
        GetRWConfig().SetValue("log", "issued_subhit_limit", 0);
    }

    return CProcessing::ParallelProcessing(params_builder);
}

template<>
int CPsgClientApp::RunRequest<SInteractiveSchema>(const CArgs& args)
{
    SIoRedirector ior(cout, args["output-file"].AsOutputFile());
    CProcessing::RequestSchema().Write(cout);
    return 0;
}

template <>
int CPsgClientApp::RunRequest<SPerformance>(const CArgs& args)
{
    TPSG_PsgClientMode::SetDefault(EPSG_PsgClientMode::ePerformance);
    return CProcessing::Performance(NParamsBuilder::SPerformance(args));
}

template <>
int CPsgClientApp::RunRequest<SJsonCheck>(const CArgs& args)
{
    const auto& schema = args["schema-file"];
    const auto single_doc = args["single-doc"].HasValue();
    SIoRedirector ior(cin, args["input-file"].AsInputFile());
    return CProcessing::JsonCheck(schema.HasValue() ? &schema.AsInputFile() : nullptr, single_doc);
}

template <class TRequest>
SCommand CPsgClientApp::s_GetCommand(string name, string desc, int flags)
{
    return { std::move(name), std::move(desc), s_InitRequest<TRequest>, s_RunRequest<TRequest>, flags };
}

int main(int argc, const char* argv[])
{
    return CPsgClientApp().AppMain(argc, argv);
}
