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

#include "processing.hpp"
#include "performance.hpp"

USING_NCBI_SCOPE;

class CPsgClientApp;

struct SCommand
{
    using TInit = function<void(CArgDescriptions&)>;
    using TRun = function<int(CPsgClientApp*, const CArgs&)>;
    using TFlags = CCommandArgDescriptions::ECommandFlags;

    const string name;
    const string desc;
    TInit init;
    TRun run;
    TFlags flags;

    SCommand(string n, string d, TInit i, TRun r, TFlags f) : name(n), desc(d), init(i), run(r), flags(f) {}
};

class CPsgClientApp : public CNcbiApplication
{
public:
    CPsgClientApp();
    virtual void Init();
    virtual int Run();

private:
    template <class TRequest>
    static SCommand s_GetCommand(string name, string desc, SCommand::TFlags flags = SCommand::TFlags::eDefault);

    template <class TRequest>
    static void s_InitRequest(CArgDescriptions& arg_desc);

    template <class TRequest>
    static int s_RunRequest(CPsgClientApp* that, const CArgs& args) { _ASSERT(that); return that->RunRequest<TRequest>(args); }

    template <class TRequest>
    int RunRequest(const CArgs& args);

    vector<SCommand> m_Commands;
};

struct SInteractive {};
struct SPerformance {};
struct SReport {};
struct STesting {};
struct SIo {};

CPsgClientApp::CPsgClientApp() :
    m_Commands({
            s_GetCommand<CPSG_Request_Biodata>       ("biodata",     "Request biodata info and data by bio ID"),
            s_GetCommand<CPSG_Request_Blob>          ("blob",        "Request blob by blob ID"),
            s_GetCommand<CPSG_Request_Resolve>       ("resolve",     "Request biodata info by bio ID"),
            s_GetCommand<CPSG_Request_NamedAnnotInfo>("named_annot", "Request named annotations info by bio ID"),
            s_GetCommand<CPSG_Request_TSE_Chunk>     ("tse_chunk",   "Request particular version of split by TSE blob ID and chunk number"),
            s_GetCommand<SInteractive>               ("interactive", "Interactive JSON-RPC mode"),
            s_GetCommand<SPerformance>               ("performance", "Performance testing", SCommand::TFlags::eHidden),
            s_GetCommand<SReport>                    ("report",      "Performance reporting", SCommand::TFlags::eHidden),
            s_GetCommand<STesting>                   ("test",        "Testing mode", SCommand::TFlags::eHidden),
            s_GetCommand<SIo>                        ("io",          "IO mode", SCommand::TFlags::eHidden),
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
        command.init(*arg_desc);
        arg_desc->AddOptionalKey("timeout", "SECONDS", "Set request timeout (in seconds)", CArgDescriptions::eInteger);
        cmd_desc->AddCommand(command.name, arg_desc.release(), kEmptyStr, command.flags);
    }

    SetupArgDescriptions(cmd_desc.release());
}

int CPsgClientApp::Run()
{
    auto& args = GetArgs();
    auto name = args.GetCommand();

    for (const auto& command : m_Commands) {
        if (command.name == name) {
            if (args["timeout"].HasValue()) {
                auto timeout = static_cast<unsigned>(args["timeout"].AsInteger());
                TPSG_RequestTimeout::SetDefault(timeout);
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

void s_InitPsgOptions(CArgDescriptions& arg_desc, int flags = 0)
{
    arg_desc.AddOptionalKey("io-threads", "THREADS_NUM", "Number of I/O threads", CArgDescriptions::eInteger, flags);
    arg_desc.AddOptionalKey("requests-per-io", "REQUESTS_NUM", "Number of requests to submit consecutively per I/O thread", CArgDescriptions::eInteger, flags);
    arg_desc.AddOptionalKey("max-streams", "REQUESTS_NUM", "Maximum number of concurrent streams per I/O thread", CArgDescriptions::eInteger, flags);
    arg_desc.AddOptionalKey("use-cache", "USE_CACHE", "Whether to use LMDB cache (no|yes|default)", CArgDescriptions::eString, flags);
}

template <class TRequest>
void CPsgClientApp::s_InitRequest(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("service", "SERVICE_NAME", "PSG service or host:port", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("exclude-blob", "BLOB_ID", "Exclude blob by its ID", CArgDescriptions::eString, CArgDescriptions::fAllowMultiple);
    arg_desc.AddPositional("ID", "ID part of Bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("type", "TYPE", "Type part of bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("acc-substitution", "ACC_SUB", "ACC substitution", CArgDescriptions::eString);
    s_InitDataFlags(arg_desc);
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_Resolve>(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("service", "SERVICE_NAME", "PSG service or host:port", CArgDescriptions::eString);
    arg_desc.AddOptionalPositional("ID", "Bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("id-file", "FILENAME", "File containing bio IDs to resolve (one per line)", CArgDescriptions::eInputFile);
    arg_desc.AddOptionalKey("type", "TYPE", "Type of bio ID(s)", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("acc-substitution", "ACC_SUB", "ACC substitution", CArgDescriptions::eString);
    arg_desc.AddDefaultKey("worker-threads", "THREADS_CONF", "Numbers of different worker threads", CArgDescriptions::eString, "", CArgDescriptions::fHidden);
    s_InitPsgOptions(arg_desc, CArgDescriptions::fHidden);

    for (const auto& f : SRequestBuilder::GetInfoFlags()) {
        arg_desc.AddFlag(f.name, f.desc);
    }

    auto id_group = CArgDependencyGroup::Create("ID GROUP", "The group consists of arguments to specify bio ID(s)");
    id_group->Add("ID");
    id_group->Add("id-file");
    id_group->SetMinMembers(1).SetMaxMembers(1);
    arg_desc.AddDependencyGroup(id_group);
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_Blob>(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("service", "SERVICE_NAME", "PSG service or host:port", CArgDescriptions::eString);
    arg_desc.AddPositional("ID", "Blob ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("last-modified", "LAST_MODIFIED", "LastModified", CArgDescriptions::eString);
    s_InitDataFlags(arg_desc);
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_NamedAnnotInfo>(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("service", "SERVICE_NAME", "PSG service or host:port", CArgDescriptions::eString);
    arg_desc.AddKey("na", "NAMED_ANNOT", "Named annotation", CArgDescriptions::eString, CArgDescriptions::fAllowMultiple);
    arg_desc.AddPositional("ID", "ID part of Bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("type", "TYPE", "Type part of bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("acc-substitution", "ACC_SUB", "ACC substitution", CArgDescriptions::eString);
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_TSE_Chunk>(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("service", "SERVICE_NAME", "PSG service or host:port", CArgDescriptions::eString);
    arg_desc.AddPositional("ID", "TSE Blob ID", CArgDescriptions::eString);
    arg_desc.AddPositional("CHUNK_NO", "Chunk number", CArgDescriptions::eInteger);
    arg_desc.AddPositional("SPLIT_VER", "Split version", CArgDescriptions::eInteger);
}

template<>
void CPsgClientApp::s_InitRequest<SInteractive>(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("service", "SERVICE_NAME", "PSG service or host:port", CArgDescriptions::eString);
    arg_desc.AddDefaultKey("input-file", "FILENAME", "File containing JSON-RPC requests (one per line)", CArgDescriptions::eInputFile, "-");
    arg_desc.AddFlag("echo", "Echo all incoming requests");
    arg_desc.AddDefaultKey("worker-threads", "THREADS_CONF", "Numbers of different worker threads", CArgDescriptions::eString, "", CArgDescriptions::fHidden);
    s_InitPsgOptions(arg_desc, CArgDescriptions::fHidden);
}

template <>
void CPsgClientApp::s_InitRequest<SPerformance>(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("service", "SERVICE_NAME", "PSG service or host:port", CArgDescriptions::eString);
    arg_desc.AddDefaultKey("user-threads", "THREADS_NUM", "Number of user threads", CArgDescriptions::eInteger, "1");
    s_InitPsgOptions(arg_desc);
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
    arg_desc.AddPositional("SERVICE", "PSG service or host:port", CArgDescriptions::eString);
    arg_desc.AddPositional("START_TIME", "Start time (time_t)", CArgDescriptions::eInteger);
    arg_desc.AddPositional("DURATION", "Duration (seconds)", CArgDescriptions::eInteger);
    arg_desc.AddPositional("USER_THREADS", "Number of user threads", CArgDescriptions::eInteger);
    arg_desc.AddPositional("DOWNLOAD_SIZE", "Download size", CArgDescriptions::eInteger);
}

// TDescription is not publicly available in CParam, but it's needed for string to enum conversion.
// This templated function circumvents that shortcoming.
template <class TDescription>
EPSG_UseCache s_GetUseCacheValue(const CParam<TDescription>&, const string& use_cache)
{
    return CParam<TDescription>::TParamParser::StringToValue(use_cache, TDescription::sm_ParamDescription);
}

void s_SetPsgDefaults(const CArgs& args)
{
    if (args["io-threads"].HasValue()) {
        auto io_threads = static_cast<size_t>(args["io-threads"].AsInteger());
        TPSG_NumIo::SetDefault(io_threads);
    }

    if (args["requests-per-io"].HasValue()) {
        auto requests_per_io = static_cast<size_t>(args["requests-per-io"].AsInteger());
        TPSG_RequestsPerIo::SetDefault(requests_per_io);
    }

    if (args["max-streams"].HasValue()) {
        auto max_streams = static_cast<size_t>(args["max-streams"].AsInteger());
        TPSG_MaxConcurrentStreams::SetDefault(max_streams);
    }

    if (args["use-cache"].HasValue()) {
        auto use_cache = args["use-cache"].AsString();
        auto use_cache_value = s_GetUseCacheValue(TPSG_UseCache(), use_cache);
        TPSG_UseCache::SetDefault(use_cache_value);
    }
}

template <class TRequest>
int CPsgClientApp::RunRequest(const CArgs& args)
{
    const auto& service = args["service"].AsString();
    auto request = SRequestBuilder::Build<TRequest>(args);
    return CProcessing::OneRequest(service, request);
}

template<>
int CPsgClientApp::RunRequest<CPSG_Request_Resolve>(const CArgs& args)
{
    const auto single_request = args["ID"].HasValue();

    if (single_request) {
        const auto& service = args["service"].AsString();
        auto request = SRequestBuilder::Build<CPSG_Request_Resolve>(args);
        return CProcessing::OneRequest(service, request);
    } else {
        auto& ctx = CDiagContext::GetRequestContext();

        ctx.SetRequestID();
        ctx.SetSessionID();
        ctx.SetHitID();

        s_SetPsgDefaults(args);
        return CProcessing::ParallelProcessing(args, true, false);
    }
}

template<>
int CPsgClientApp::RunRequest<SInteractive>(const CArgs& args)
{
    s_SetPsgDefaults(args);
    TPSG_PsgClientMode::SetDefault(EPSG_PsgClientMode::eInteractive);

    const auto echo = args["echo"].HasValue();
    return CProcessing::ParallelProcessing(args, false, echo);
}

template <>
int CPsgClientApp::RunRequest<SPerformance>(const CArgs& args)
{
    s_SetPsgDefaults(args);
    TPSG_PsgClientMode::SetDefault(EPSG_PsgClientMode::ePerformance);

    const auto& service = args["service"].AsString();
    auto user_threads = static_cast<size_t>(args["user-threads"].AsInteger());
    auto local_queue = args["local-queue"].AsBoolean();
    auto& os = args["output-file"].AsOutputFile();
    return CProcessing::Performance(service, user_threads, local_queue, os);
}

template <>
int CPsgClientApp::RunRequest<SReport>(const CArgs& args)
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
int CPsgClientApp::RunRequest<STesting>(const CArgs&)
{
    TPSG_PsgClientMode::SetDefault(EPSG_PsgClientMode::eInteractive);

    return CProcessing::Testing();
}

template <>
int CPsgClientApp::RunRequest<SIo>(const CArgs& args)
{
    const auto& service = args["service"].AsString();
    auto start_time = args["START_TIME"].AsInteger();
    auto duration = args["DURATION"].AsInteger();
    auto user_threads = args["USER_THREADS"].AsInteger();
    auto download_size = args["DOWNLOAD_SIZE"].AsInteger();
    return CProcessing::Io(service, start_time, duration, user_threads, download_size);
}

template <class TRequest>
SCommand CPsgClientApp::s_GetCommand(string name, string desc, SCommand::TFlags flags)
{
    return { move(name), move(desc), s_InitRequest<TRequest>, s_RunRequest<TRequest>, flags };
}

int main(int argc, const char* argv[])
{
    return CPsgClientApp().AppMain(argc, argv);
}
