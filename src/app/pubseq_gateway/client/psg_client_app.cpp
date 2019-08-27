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
struct STesting {};
struct SIo {};

CPsgClientApp::CPsgClientApp() :
    m_Commands({
            s_GetCommand<CPSG_Request_Biodata>       ("biodata",     "Request biodata info and data by bio ID"),
            s_GetCommand<CPSG_Request_Blob>          ("blob",        "Request blob by blob ID"),
            s_GetCommand<CPSG_Request_Resolve>       ("resolve",     "Request biodata info by bio ID"),
            s_GetCommand<CPSG_Request_NamedAnnotInfo>("named_annot", "Request named annotations info by bio ID"),
            s_GetCommand<SInteractive>               ("interactive", "Interactive JSON-RPC mode"),
            s_GetCommand<SPerformance>               ("performance", "Performance testing mode", SCommand::TFlags::eHidden),
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
            return command.run(this, args);
        }
    }

    cerr << "Command is required" << endl;
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

template <class TRequest>
void CPsgClientApp::s_InitRequest(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("service", "SERVICE_NAME", "PSG service or host:port", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("exclude-blob", "BLOB_ID", "Exclude blob by its ID", CArgDescriptions::eString, CArgDescriptions::fAllowMultiple);
    arg_desc.AddPositional("ID", "ID part of Bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("type", "TYPE", "Type part of bio ID", CArgDescriptions::eString);
    s_InitDataFlags(arg_desc);
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_Resolve>(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("service", "SERVICE_NAME", "PSG service or host:port", CArgDescriptions::eString);
    arg_desc.AddOptionalPositional("ID", "Bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("id-file", "FILENAME", "File containing bio IDs to resolve (one per line)", CArgDescriptions::eInputFile);
    arg_desc.AddOptionalKey("type", "TYPE", "Type of bio ID(s)", CArgDescriptions::eString);

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
    arg_desc.AddDefaultKey("last_modified", "LAST_MODIFIED", "LastModified", CArgDescriptions::eString, "");
    s_InitDataFlags(arg_desc);
}

template <>
void CPsgClientApp::s_InitRequest<CPSG_Request_NamedAnnotInfo>(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("service", "SERVICE_NAME", "PSG service or host:port", CArgDescriptions::eString);
    arg_desc.AddKey("na", "NAMED_ANNOT", "Named annotation", CArgDescriptions::eString, CArgDescriptions::fAllowMultiple);
    arg_desc.AddPositional("ID", "ID part of Bio ID", CArgDescriptions::eString);
    arg_desc.AddOptionalKey("type", "TYPE", "Type part of bio ID", CArgDescriptions::eString);
}

template<>
void CPsgClientApp::s_InitRequest<SInteractive>(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("service", "SERVICE_NAME", "PSG service or host:port", CArgDescriptions::eString);
    arg_desc.AddFlag("echo", "Echo all incoming requests");
}

template <>
void CPsgClientApp::s_InitRequest<SPerformance>(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("service", "SERVICE_NAME", "PSG service or host:port", CArgDescriptions::eString);
    arg_desc.AddDefaultKey("user-threads", "THREADS_NUM", "Number of user threads", CArgDescriptions::eInteger, "1");
    arg_desc.AddOptionalKey("io-threads", "THREADS_NUM", "Number of I/O threads", CArgDescriptions::eInteger);
    arg_desc.AddOptionalKey("requests-per-io", "REQUESTS_NUM", "Number of requests to submit consecutively per I/O thread", CArgDescriptions::eInteger);
    arg_desc.AddOptionalKey("max-streams", "REQUESTS_NUM", "Maximum number of concurrent streams per I/O thread", CArgDescriptions::eInteger);
    arg_desc.AddOptionalKey("use-cache", "USE_CACHE", "Whether to use LMDB cache (no|yes|default)", CArgDescriptions::eString);
    arg_desc.AddFlag("no-delayed-completion", "Whether to use delayed completion", CArgDescriptions::eFlagHasValueIfMissed);
    arg_desc.AddFlag("raw-metrics", "Whether to output raw metrics");
    arg_desc.AddFlag("local-queue", "Whether user threads to use separate queues");
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

template <class TRequest>
int CPsgClientApp::RunRequest(const CArgs& args)
{
    CProcessing processing(args["service"].AsString());

    auto request = SRequestBuilder::CreateRequest<TRequest>(nullptr, args);

    return processing.OneRequest(request);
}

template<>
int CPsgClientApp::RunRequest<CPSG_Request_Resolve>(const CArgs& args)
{
    const auto service = args["service"].AsString();
    const auto single_request = args["ID"].HasValue();
    const auto regular_file = args["id-file"].HasValue() && (args["id-file"].AsString() != "-");
    const auto interactive = !single_request && !regular_file;
    CProcessing processing(service, interactive, true);

    if (single_request) {
        auto request = SRequestBuilder::CreateRequest<CPSG_Request_Resolve>(nullptr, args);
        return processing.OneRequest(request);
    } else {
        auto& is = regular_file ? args["id-file"].AsInputFile() : cin;
        return processing.BatchResolve(args, is);
    }
}

template<>
int CPsgClientApp::RunRequest<SInteractive>(const CArgs& args)
{
    CProcessing processing(args["service"].AsString(), true);

    TPSG_PsgClientMode::SetDefault(EPSG_PsgClientMode::eInteractive);

    return processing.Interactive(args["echo"].HasValue());
}

// TDescription is not publicly available in CParam, but it's needed for string to enum conversion.
// This templated function circumvents that shortcoming.
template <class TDescription>
EPSG_UseCache s_GetUseCacheValue(const CParam<TDescription>&, const string& use_cache)
{
    return CParam<TDescription>::TParamParser::StringToValue(use_cache, TDescription::sm_ParamDescription);
}

template <>
int CPsgClientApp::RunRequest<SPerformance>(const CArgs& args)
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

    auto service = args["service"].AsString();
    CProcessing processing(service, true);

    auto user_threads = static_cast<size_t>(args["user-threads"].AsInteger());
    auto delayed_completion = args["no-delayed-completion"].AsBoolean();
    auto raw_metrics = args["raw-metrics"].AsBoolean();

    if (!args["local-queue"].AsBoolean()) service.clear();

    TPSG_DelayedCompletion::SetDefault(delayed_completion);
    TPSG_PsgClientMode::SetDefault(EPSG_PsgClientMode::ePerformance);

    return processing.Performance(user_threads, raw_metrics, service);
}

template <>
int CPsgClientApp::RunRequest<STesting>(const CArgs&)
{
    TPSG_PsgClientMode::SetDefault(EPSG_PsgClientMode::eInteractive);

    CProcessing processing;
    return processing.Testing();
}

template <>
int CPsgClientApp::RunRequest<SIo>(const CArgs& args)
{
    auto service = args["SERVICE"].AsString();
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
    SetDiagPostLevel(eDiag_Warning);
    return CPsgClientApp().AppMain(argc, argv);
}
