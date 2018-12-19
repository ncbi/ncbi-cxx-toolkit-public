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

USING_NCBI_SCOPE;

class CPsgClientApp;

struct SCommand
{
    using TInit = function<void(CArgDescriptions&)>;
    using TRun = function<int(CPsgClientApp*, const CArgs&)>;

    const string name;
    const string desc;
    TInit init;
    TRun run;

    SCommand(string n, string d, TInit i, TRun r) : name(n), desc(d), init(i), run(r) {}
};

class CPsgClientApp : public CNcbiApplication
{
public:
    CPsgClientApp();
    virtual void Init();
    virtual int Run();

private:
    template <class TRequest>
    static SCommand s_GetCommand(string name, string desc);

    template <class TRequest>
    static void s_InitFlags(CArgDescriptions& arg_desc);

    template <class TRequest>
    static void s_InitRequest(CArgDescriptions& arg_desc);

    template <class TRequest>
    static void s_InitPositional(CArgDescriptions& arg_desc);

    template <class TRequest>
    static int s_RunRequest(CPsgClientApp* that, const CArgs& args) { _ASSERT(that); return that->RunRequest<TRequest>(args); }

    static void s_InitInteractive(CArgDescriptions& arg_desc);
    static int s_RunInteractive(CPsgClientApp* that, const CArgs& args) { _ASSERT(that); return that->RunInteractive(args); }

    template <class TRequest>
    int RunRequest(const CArgs& args);

    int RunInteractive(const CArgs& args);

    vector<SCommand> m_Commands;
};

template <class TRequest>
int CPsgClientApp::RunRequest(const CArgs& args)
{
    CProcessing processing(args["service"].AsString());

    auto request = CProcessing::CreateRequest<TRequest>(nullptr, args);

    return processing.OneRequest(request);
}

template <class TRequest>
SCommand CPsgClientApp::s_GetCommand(string name, string desc)
{
    return { move(name), move(desc), s_InitRequest<TRequest>, s_RunRequest<TRequest> };
}

CPsgClientApp::CPsgClientApp() :
    m_Commands({
            s_GetCommand<CPSG_Request_Biodata>("biodata", "Request biodata info and data by bio ID"),
            s_GetCommand<CPSG_Request_Blob>   ("blob",    "Request blob by blob ID"),
            s_GetCommand<CPSG_Request_Resolve>("resolve", "Request biodata info by bio ID"),
            { "interactive", "Interactive JSON-RPC mode", s_InitInteractive, s_RunInteractive },
        })
{
}

void CPsgClientApp::Init()
{
    unique_ptr<CCommandArgDescriptions> cmd_desc(new CCommandArgDescriptions());
    cmd_desc->SetUsageContext(GetArguments().GetProgramBasename(), "PSG client");

    for (const auto& command : m_Commands) {
        unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());
        arg_desc->SetUsageContext("", command.desc);
        command.init(*arg_desc);
        cmd_desc->AddCommand(command.name, arg_desc.release());
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

    return -1;
}

template <class TRequest>
void CPsgClientApp::s_InitFlags(CArgDescriptions& arg_desc)
{
    const auto& data_flags = CProcessing::GetDataFlags();

    for (auto i = data_flags.begin(); i != data_flags.end(); ++i) {
        arg_desc.AddFlag(i->name, i->desc);

        for (auto j = i + 1; j != data_flags.end(); ++j) {
            if (i->name != j->name) arg_desc.SetDependency(i->name, CArgDescriptions::eExcludes, j->name);
        }
    }
}

template <>
void CPsgClientApp::s_InitFlags<CPSG_Request_Resolve>(CArgDescriptions& arg_desc)
{
    for (const auto& f : CProcessing::GetInfoFlags()) {
        arg_desc.AddFlag(f.name, f.desc);
    }
}

template <class TRequest>
void CPsgClientApp::s_InitRequest(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("service", "SERVICE_NAME", "PSG service or host:port", CArgDescriptions::eString);
    s_InitPositional<TRequest>(arg_desc);
    s_InitFlags<TRequest>(arg_desc);
}

template <class TRequest>
void CPsgClientApp::s_InitPositional(CArgDescriptions& arg_desc)
{
    arg_desc.AddPositional("ID", "ID part of Bio ID", CArgDescriptions::eString);
    arg_desc.AddDefaultKey("type", "TYPE", "Type part of bio ID", CArgDescriptions::eString, "gi");
}

template <>
void CPsgClientApp::s_InitPositional<CPSG_Request_Blob>(CArgDescriptions& arg_desc)
{
    arg_desc.AddPositional("ID", "Blob ID", CArgDescriptions::eString);
    arg_desc.AddDefaultKey("last_modified", "LAST_MODIFIED", "LastModified", CArgDescriptions::eString, "");
}

void CPsgClientApp::s_InitInteractive(CArgDescriptions& arg_desc)
{
    arg_desc.AddKey("service", "SERVICE_NAME", "PSG service or host:port", CArgDescriptions::eString);
    arg_desc.AddFlag("echo", "Echo all incoming requests");
}

int CPsgClientApp::RunInteractive(const CArgs& args)
{
    CProcessing processing(args["service"].AsString(), true);
    return processing.Interactive(args["echo"].HasValue());
}

int main(int argc, const char* argv[])
{
    return CPsgClientApp().AppMain(argc, argv);
}
