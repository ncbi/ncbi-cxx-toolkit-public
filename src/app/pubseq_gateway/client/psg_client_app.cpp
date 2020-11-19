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

USING_NCBI_SCOPE;

class CPsgClientApp;

struct SCommand
{
    using TInit = function<void(CArgDescriptions&)>;
    using TRun = function<int(CPsgClientApp*, const string&, const CArgs&)>;
    enum EFlags { eDefault, fHidden = 1 << 0, fParallel = 1 << 1 };

    const string name;
    const string desc;
    TInit init;
    TRun run;
    EFlags flags;

    SCommand(string n, string d, TInit i, TRun r, EFlags f) : name(n), desc(d), init(i), run(r), flags(f) {}
};

class CPsgClientApp : public CNcbiApplication
{
public:
    CPsgClientApp();
    virtual void Init();
    virtual int Run();

private:
    template <class TRequest>
    static SCommand s_GetCommand(string name, string desc, SCommand::EFlags flags = SCommand::eDefault);

    template <class TRequest>
    static void s_InitRequest(CArgDescriptions& arg_desc);

    template <class TRequest>
    static int s_RunRequest(CPsgClientApp* that, const string& service, const CArgs& args) { _ASSERT(that); return that->RunRequest<TRequest>(service, args); }

    template <class TRequest>
    int RunRequest(const string& service, const CArgs& args);

    vector<SCommand> m_Commands;
};

struct SInteractive {};
struct SPerformance {};
struct SReport {};
struct STesting {};
struct SIo {};

void s_InitPsgOptions(CArgDescriptions& arg_desc);
const string& s_SetPsgDefaults(const CArgs& args, bool parallel);

CPsgClientApp::CPsgClientApp() :
    m_Commands({
            s_GetCommand<CPSG_Request_Biodata>       ("biodata",     "Request biodata info and data by bio ID"),
            s_GetCommand<CPSG_Request_Blob>          ("blob",        "Request blob by blob ID"),
            s_GetCommand<CPSG_Request_Resolve>       ("resolve",     "Request biodata info by bio ID", SCommand::fParallel),
            s_GetCommand<CPSG_Request_NamedAnnotInfo>("named_annot", "Request named annotations info by bio ID"),
            s_GetCommand<CPSG_Request_TSE_Chunk>     ("tse_chunk",   "Request particular version of split by TSE blob ID and chunk number"),
            s_GetCommand<SInteractive>               ("interactive", "Interactive JSON-RPC mode", SCommand::fParallel),
            s_GetCommand<SPerformance>               ("performance", "Performance testing", SCommand::fHidden),
            s_GetCommand<SReport>                    ("report",      "Performance reporting", SCommand::fHidden),
            s_GetCommand<STesting>                   ("test",        "Testing mode", SCommand::fHidden),
            s_GetCommand<SIo>                        ("io",          "IO mode", SCommand::fHidden),
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
        s_InitPsgOptions(*arg_desc);
        command.init(*arg_desc);
        const auto hidden = command.flags & SCommand::fHidden;
        const auto flags = hidden ? CCommandArgDescriptions::eHidden : CCommandArgDescriptions::eDefault;
        cmd_desc->AddCommand(command.name, arg_desc.release(), kEmptyStr, flags);
    }

    SetupArgDescriptions(cmd_desc.release());
}

int CPsgClientApp::Run()
{
    auto& args = GetArgs();
    auto name = args.GetCommand();

    for (const auto& command : m_Commands) {
        if (command.name == name) {
            const bool parallel = command.flags & SCommand::fParallel;
            const auto& service = s_SetPsgDefaults(args, parallel);
            return command.run(this, service, args);
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

void s_InitPsgOptions(CArgDescriptions& arg_desc)
{
    arg_desc.AddDefaultKey("service", "SERVICE", "PSG service name or host:port pair", CArgDescriptions::eString, "PSG");
    arg_desc.AddOptionalKey("io-threads", "THREADS_NUM", "Number of I/O threads", CArgDescriptions::eInteger, CArgDescriptions::fHidden);
    arg_desc.AddOptionalKey("requests-per-io", "REQUESTS_NUM", "Number of requests to submit consecutively per I/O thread", CArgDescriptions::eInteger, CArgDescriptions::fHidden);
    arg_desc.AddOptionalKey("max-streams", "REQUESTS_NUM", "Maximum number of concurrent streams per I/O thread", CArgDescriptions::eInteger, CArgDescriptions::fHidden);
    arg_desc.AddOptionalKey("use-cache", "USE_CACHE", "Whether to use LMDB cache (no|yes|default)", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("timeout", "SECONDS", "Set request timeout (in seconds)", CArgDescriptions::eInteger);
    arg_desc.AddOptionalKey("debug-printout", "WHAT", "Debug printout of PSG protocol (some|all).", CArgDescriptions::eString, CArgDescriptions::fHidden);
    arg_desc.AddFlag("latency", "Latency output", CArgDescriptions::eFlagHasValueIfSet, CArgDescriptions::fHidden);
    arg_desc.AddFlag("verbose", "Verbose output");
}

void s_InitBlobOnly(CArgDescriptions& arg_desc, initializer_list<string> blob_types = {})
{
    arg_desc.AddFlag("blob-only", "Output blob data only");
    arg_desc.AddOptionalKey("output-fmt", "FORMAT", "Format for blob data to return in (instead of raw data)", CArgDescriptions::eString);
    arg_desc.SetConstraint("output-fmt", new CArgAllow_Strings{"asn", "asnb", "xml", "json"});
    arg_desc.SetDependency("output-fmt", CArgDescriptions::eRequires, "blob-only");

    if (blob_types.size()) {
        arg_desc.AddDefaultKey("blob-type", "TYPE", "Blob data type", CArgDescriptions::eString, "seqentry");
        arg_desc.SetConstraint("blob-type", new CArgAllow_Strings(move(blob_types)));
        arg_desc.SetDependency("blob-type", CArgDescriptions::eRequires, "blob-only");
        arg_desc.SetDependency("blob-type", CArgDescriptions::eRequires, "output-fmt");
    }
}

template <class TRequest>
void CPsgClientApp::s_InitRequest(CArgDescriptions& arg_desc)
{
    arg_desc.AddOptionalKey("exclude-blob", "BLOB_ID", "Exclude blob by its ID", CArgDescriptions::eString, CArgDescriptions::fAllowMultiple);
    arg_desc.AddPositional("ID", "ID part of Bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("type", "TYPE", "Type part of bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("acc-substitution", "ACC_SUB", "ACC substitution", CArgDescriptions::eString);
    s_InitBlobOnly(arg_desc, {"seqentry", "splitinfo"});
    s_InitDataFlags(arg_desc);
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_Resolve>(CArgDescriptions& arg_desc)
{
    arg_desc.AddOptionalPositional("ID", "Bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("id-file", "FILENAME", "File containing bio IDs to resolve (one per line)", CArgDescriptions::eInputFile);
    arg_desc.SetDependency("id-file", CArgDescriptions::eExcludes, "ID");
    arg_desc.AddOptionalKey("type", "TYPE", "Type of bio ID(s)", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("acc-substitution", "ACC_SUB", "ACC substitution", CArgDescriptions::eString);
    arg_desc.AddDefaultKey("worker-threads", "THREADS_CONF", "Numbers of different worker threads", CArgDescriptions::eString, "", CArgDescriptions::fHidden);

    for (const auto& f : SRequestBuilder::GetInfoFlags()) {
        arg_desc.AddFlag(f.name, f.desc);
    }
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_Blob>(CArgDescriptions& arg_desc)
{
    arg_desc.AddPositional("ID", "Blob ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("last-modified", "LAST_MODIFIED", "LastModified", CArgDescriptions::eString);
    s_InitBlobOnly(arg_desc, {"seqentry", "seqannot", "splitinfo", "chunk"});
    s_InitDataFlags(arg_desc);
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_NamedAnnotInfo>(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("na", "NAMED_ANNOT", "Named annotation", CArgDescriptions::eString, CArgDescriptions::fAllowMultiple);
    arg_desc.AddPositional("ID", "ID part of Bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("type", "TYPE", "Type part of bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("acc-substitution", "ACC_SUB", "ACC substitution", CArgDescriptions::eString);
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_TSE_Chunk>(CArgDescriptions& arg_desc)
{
    arg_desc.AddPositional("ID", "TSE Blob ID", CArgDescriptions::eString);
    arg_desc.AddPositional("CHUNK_NO", "Chunk number", CArgDescriptions::eInteger);
    arg_desc.AddPositional("SPLIT_VER", "Split version", CArgDescriptions::eInteger);
    s_InitBlobOnly(arg_desc);
}

template<>
void CPsgClientApp::s_InitRequest<SInteractive>(CArgDescriptions& arg_desc)
{
    arg_desc.AddDefaultKey("input-file", "FILENAME", "File containing JSON-RPC requests (one per line)", CArgDescriptions::eInputFile, "-");
    arg_desc.AddFlag("echo", "Echo all incoming requests");
    arg_desc.AddDefaultKey("worker-threads", "THREADS_CONF", "Numbers of different worker threads", CArgDescriptions::eString, "", CArgDescriptions::fHidden);
}

template <>
void CPsgClientApp::s_InitRequest<SPerformance>(CArgDescriptions& arg_desc)
{
    arg_desc.AddDefaultKey("user-threads", "THREADS_NUM", "Number of user threads", CArgDescriptions::eInteger, "1");
    arg_desc.AddDefaultKey("delay", "SECONDS", "Delay between consecutive requests (in seconds)", CArgDescriptions::eDouble, "0.0");
    arg_desc.AddFlag("raw-metrics", "No-op, for backward compatibility", CArgDescriptions::eFlagHasValueIfSet, CArgDescriptions::fHidden);
    arg_desc.AddFlag("local-queue", "Whether user threads to use separate queues");
    arg_desc.AddDefaultKey("output-file", "FILENAME", "Output file to contain raw performance metrics", CArgDescriptions::eOutputFile, "psg_client.raw.txt");
}

template <>
void CPsgClientApp::s_InitRequest<SReport>(CArgDescriptions& arg_desc)
{
    arg_desc.AddDefaultKey("input-file", "FILENAME", "Input file containing raw performance metrics", CArgDescriptions::eInputFile, "-");
    arg_desc.AddDefaultKey("output-file", "FILENAME", "Output file to contain result performance metrics", CArgDescriptions::eOutputFile, "psg_client.table.txt");
    arg_desc.AddDefaultKey("percentage", "PERCENTAGE", "Use only PERCENTAGE of top values for results", CArgDescriptions::eDouble, "99.9");
}

template <>
void CPsgClientApp::s_InitRequest<STesting>(CArgDescriptions&)
{
}

template <>
void CPsgClientApp::s_InitRequest<SIo>(CArgDescriptions& arg_desc)
{
    arg_desc.AddPositional("START_TIME", "Start time (time_t)", CArgDescriptions::eInteger);
    arg_desc.AddPositional("DURATION", "Duration (seconds)", CArgDescriptions::eInteger);
    arg_desc.AddPositional("USER_THREADS", "Number of user threads", CArgDescriptions::eInteger);
    arg_desc.AddPositional("DOWNLOAD_SIZE", "Download size", CArgDescriptions::eInteger);
}

const string& s_SetPsgDefaults(const CArgs& args, bool parallel)
{
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

    if (args["debug-printout"].HasValue()) {
        auto debug_printout = args["debug-printout"].AsString();
        TPSG_DebugPrintout::SetDefault(debug_printout);
    }

    CJsonResponse::Verbose(args["verbose"].HasValue());
    return args["service"].AsString();
}

template <class TRequest>
int CPsgClientApp::RunRequest(const string& service, const CArgs& args)
{
    auto request = SRequestBuilder::Build<TRequest>(args);

    if (!args.Exist("blob-only") || !args["blob-only"].HasValue()) {
        return CProcessing::OneRequest(service, request, args);
    }

    SBlobOnly blob_only;

    if (args.Exist("output-fmt") && args["output-fmt"].HasValue()) {
        blob_only.output.format = &args["output-fmt"].AsString();

        if (args.Exist("blob-type")) {
            blob_only.output.type = &args["blob-type"].AsString();
        }
    }

    return CProcessing::OneRequest(service, request, args, &blob_only);
}

template<>
int CPsgClientApp::RunRequest<CPSG_Request_Resolve>(const string& service, const CArgs& args)
{
    const auto single_request = args["ID"].HasValue();

    if (single_request) {
        auto request = SRequestBuilder::Build<CPSG_Request_Resolve>(args);
        return CProcessing::OneRequest(service, request, args);
    } else {
        auto& ctx = CDiagContext::GetRequestContext();

        ctx.SetRequestID();
        ctx.SetSessionID();
        ctx.SetHitID();

        CJsonResponse::SetReplyType(false);
        return CProcessing::ParallelProcessing(service, args, true, false);
    }
}

template<>
int CPsgClientApp::RunRequest<SInteractive>(const string& service, const CArgs& args)
{
    TPSG_PsgClientMode::SetDefault(EPSG_PsgClientMode::eInteractive);

    const auto echo = args["echo"].HasValue();
    return CProcessing::ParallelProcessing(service, args, false, echo);
}

template <>
int CPsgClientApp::RunRequest<SPerformance>(const string& service, const CArgs& args)
{
    TPSG_PsgClientMode::SetDefault(EPSG_PsgClientMode::ePerformance);

    auto user_threads = static_cast<size_t>(args["user-threads"].AsInteger());
    auto delay = args["delay"].AsDouble();
    auto local_queue = args["local-queue"].AsBoolean();
    auto& os = args["output-file"].AsOutputFile();

    if (delay < 0.0) {
        cerr << "DELAY must be non-negative" << endl;
        return -1;
    }

    return CProcessing::Performance(service, user_threads, delay, local_queue, os);
}

template <>
int CPsgClientApp::RunRequest<SReport>(const string&, const CArgs& args)
{
    const auto pipe = args["input-file"].AsString() == "-";
    auto& is = pipe ? cin : args["input-file"].AsInputFile();
    auto& os = args["output-file"].AsOutputFile();
    const auto percentage = args["percentage"].AsDouble();

    if ((percentage <= 0.0) || (percentage > 100.0)) {
        cerr << "PERCENTAGE should be in range (0.0, 100.0]" << endl;
        return -1;
    }

    return CProcessing::Report(is, os, percentage);
}

template <>
int CPsgClientApp::RunRequest<STesting>(const string& service, const CArgs&)
{
    TPSG_PsgClientMode::SetDefault(EPSG_PsgClientMode::eInteractive);
    TPSG_FailOnUnknownItems::SetDefault(true);

    return CProcessing::Testing(service);
}

template <>
int CPsgClientApp::RunRequest<SIo>(const string& service, const CArgs& args)
{
    auto start_time = args["START_TIME"].AsInteger();
    auto duration = args["DURATION"].AsInteger();
    auto user_threads = args["USER_THREADS"].AsInteger();
    auto download_size = args["DOWNLOAD_SIZE"].AsInteger();
    return CProcessing::Io(service, start_time, duration, user_threads, download_size);
}

template <class TRequest>
SCommand CPsgClientApp::s_GetCommand(string name, string desc, SCommand::EFlags flags)
{
    return { move(name), move(desc), s_InitRequest<TRequest>, s_RunRequest<TRequest>, flags };
}

int main(int argc, const char* argv[])
{
    return CPsgClientApp().AppMain(argc, argv);
}
