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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description: Entry point and command line parsing
 *                   of the grid_cli application.
 *
 */

#include <ncbi_pch.hpp>

#include "grid_cli.hpp"

#include <connect/services/clparser.hpp>

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

USING_NCBI_SCOPE;

CGridCommandLineInterfaceApp::CGridCommandLineInterfaceApp(
        int argc, const char* argv[]) :
    m_ArgC(argc),
    m_ArgV(argv),
    m_NetICacheClient(eVoid)
{
    SetVersion(CVersionInfo(PROGRAM_VERSION, PROGRAM_NAME));
}

string CGridCommandLineInterfaceApp::GetProgramVersion() const
{
    return PROGRAM_NAME " version " PROGRAM_VERSION;
}

struct SOptionDefinition {
    CCommandLineParser::EOptionType type;
    int opt_id;
    const char* name_variants;
    const char* description;
} static const s_OptionDefinitions[eTotalNumberOfOptions] = {

    {CCommandLineParser::ePositionalArgument, eUntypedArg, "ARG", NULL},

    {CCommandLineParser::eOptionalPositional, eOptionalID, "ID", NULL},

    {CCommandLineParser::ePositionalArgument, eID, "ID", NULL},

    {CCommandLineParser::eOptionWithParameter, eAuth,
        "auth", "Authentication string (\"client_name\")."},

    {CCommandLineParser::eOptionWithParameter, eInputFile,
        "input-file", "Read input from the specified file."},

    {CCommandLineParser::eOptionWithParameter, eOutputFile,
        "o|output-file", "Save output to the specified file."},

    {CCommandLineParser::eOptionWithParameter, eOutputFormat,
        "of|output-format",
        "One of the output formats supported by this command." },

    {CCommandLineParser::eOptionWithParameter, eNetCache,
        "nc|netcache", "NetCache service name or server address."},

    {CCommandLineParser::eOptionWithParameter, eCache,
        "cache", "Enable ICache mode and specify cache name to use."},

    {CCommandLineParser::eOptionWithParameter, ePassword,
        "password", "Enable NetCache password protection."},

    {CCommandLineParser::eOptionWithParameter, eOffset,
        "offset", "Byte offset of the portion of data."},

    {CCommandLineParser::eOptionWithParameter, eSize,
        "size|length", "Length (in bytes) of the portion of data."},

    {CCommandLineParser::eOptionWithParameter, eTTL,
        "ttl", "Override the default time-to-live value."},

    {CCommandLineParser::eSwitch, eEnableMirroring,
        "enable-mirroring", "Enable NetCache mirroring functionality."},

    {CCommandLineParser::eOptionWithParameter, eNetSchedule,
        "ns|netschedule", "NetSchedule service name or server address."},

};

#define ICACHE_KEY_FORMAT_EXPLANATION \
    "\n\nBoth NetCache and ICache modes are supported. " \
    "ICache mode requires blob ID to be specified in the " \
    "following format: \"key,version,subkey\"."

struct SCommandDefinition {
    int (CGridCommandLineInterfaceApp::*cmd_proc)();
    const char* name_variants;
    const char* synopsis;
    const char* usage;
    int options[eTotalNumberOfOptions + 1];
} static const s_CommandDefinitions[] = {

    {&CGridCommandLineInterfaceApp::Cmd_WhatIs,
        "whatis", "Determine argument type and characteristics.",
        "This command makes an attempt to guess the type of its "
        "argument. If the argument is successfully recognized "
        "as a token that represents a Grid object, the type-"
        "dependent information about the object is printed.",
        {eUntypedArg, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_GetBlob,
        "getblob|gb", "Retrieve a blob from NetCache.",
        "Read the blob identified by ID and send its contents "
        "to the standard output (or to the specified output "
        "file). Expired blobs will be reported as not found."
        ICACHE_KEY_FORMAT_EXPLANATION,
        {eID, eNetCache, eCache, ePassword, eOffset,
            eSize, eOutputFile, eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_PutBlob,
        "putblob|pb", "Create or update a NetCache blob.",
        "Read data from the standard input (or a file) until EOF is "
        "encountered and save the received data as a NetCache blob."
        ICACHE_KEY_FORMAT_EXPLANATION,
        {eOptionalID, eNetCache, eCache, ePassword,
            eTTL, eEnableMirroring, eInputFile, eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_BlobInfo,
        "blobinfo|bi", "Retrieve meta information on a NetCache blob.",
        "Print vital information about the specified blob. "
        "Expired blobs will be reported as not found."
        ICACHE_KEY_FORMAT_EXPLANATION,
        {eID, eNetCache, eCache, eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_RemoveBlob,
        "rmblob|rb", "Remove a NetCache blob.",
        "Delete a blob if it exists. If the blob has expired "
        "(or never existed), no errors are reported."
        ICACHE_KEY_FORMAT_EXPLANATION,
        {eID, eNetCache, eCache, ePassword, eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_ReinitNetCache,
        "reinitnc", "Delete all blobs and reset NetCache database.",
        "This command purges and resets the specified NetCache "
        "(or ICache) database. Administrative privileges are "
        "required.",
        {eNetCache, eCache, eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_Version,
        "version", "Print server version.",
        "Query and print version information of a running "
        "NetCache, NetSchedule, or worker node process.",
        {eNetCache, eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_Stats,
        "stats", "Show server access statistics.",
        "Dump accumulated statistics on server access and "
        "performance.",
        {eNetCache, eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_Health,
        "health", "Evaluate availability of a server.",
        "Retrieve vital parameters of a running NetCache "
        "or NetSchedule server and estimate its availability "
        "coefficient."
        /*"\n\nValid output format options for this operation: "
        "\"raw\""/ *, \"xml\", \"json\"* /".  If none specified, "
        "\"raw\" is assumed."*/,
        {eNetCache, /*eOutputFormat,*/ eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_GetConf,
        "getconf", "Dump actual configuration of a server.",
        "Print the effective configuration parameters of a "
        "running NetCache or NetSchedule server.",
        {eNetCache, eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_Reconf,
        "reconf", "Reload server configuration.",
        "Update configuration parameters of a running server. "
        "The server will look for a configuration file in the "
        "same location that was used during start-up.",
        {eNetCache, eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_Shutdown,
        "shutdown", "Send a shutdown request to a remote server.",
        "Depending on the option specified, this command sends "
        "a shutdown request to a NetCache or NetSchedule server "
        "or a worker node process.",
        {eNetCache, eAuth, -1}},
};

#define TOTAL_NUMBER_OF_COMMANDS int(sizeof(s_CommandDefinitions) / \
    sizeof(*s_CommandDefinitions))

int CGridCommandLineInterfaceApp::Run()
{
    CONNECT_Init(&GetConfig());

    const SCommandDefinition* cmd_def;

    {
        CCommandLineParser clparser(PROGRAM_NAME, PROGRAM_VERSION,
            "Utility to access and control NCBI Grid services.");

        const SOptionDefinition* opt_def = s_OptionDefinitions;
        int i = eTotalNumberOfOptions;
        do {
            clparser.AddOption(opt_def->type, opt_def->opt_id,
                opt_def->name_variants, opt_def->description ?
                    opt_def->description : kEmptyStr);
            ++opt_def;
        } while (--i > 0);

        cmd_def = s_CommandDefinitions;
        for (i = 0; i < TOTAL_NUMBER_OF_COMMANDS; ++i) {
            clparser.AddCommand(i, cmd_def->name_variants,
                cmd_def->synopsis, cmd_def->usage);
            for (const int* opt_id = cmd_def->options; *opt_id >= 0; ++opt_id)
                clparser.AddAssociation(i, *opt_id);
            ++cmd_def;
        }

        try {
            int cmd_id = clparser.Parse(m_ArgC, m_ArgV);
            if (cmd_id < 0)
                return 0;
            cmd_def = s_CommandDefinitions + cmd_id;
        }
        catch (exception& e) {
            NcbiCerr << e.what();
            return 1;
        }

        for (const int* opt_id = cmd_def->options; *opt_id >= 0; ++opt_id)
            m_Opts.option_flags[*opt_id] = OPTION_ACCEPTED;

        int opt_id;
        const char* opt_value;

        while (clparser.NextOption(&opt_id, &opt_value)) {
            m_Opts.option_flags[opt_id] = OPTION_SET;
            switch (EOption(opt_id)) {
            case eUntypedArg:
                /* FALL THROUGH */
            case eOptionalID:
                m_Opts.option_flags[eID] = OPTION_SET;
                /* FALL THROUGH */
            case eID:
                m_Opts.id = opt_value;
                break;
            case eAuth:
                m_Opts.auth = opt_value;
                break;
            case eOutputFormat:
                break;
            case eNetCache:
                m_Opts.nc_service = opt_value;
                break;
            case eCache:
                m_Opts.cache_name = opt_value;
                break;
            case ePassword:
                m_Opts.password = opt_value;
                break;
            case eOffset:
                m_Opts.offset = size_t(NStr::StringToUInt8(opt_value));
                break;
            case eSize:
                m_Opts.size = size_t(NStr::StringToUInt8(opt_value));
                break;
            case eTTL:
                m_Opts.ttl = NStr::StringToUInt(opt_value);
                break;
            case eEnableMirroring:
                m_Opts.enable_mirroring = true;
                break;
            case eNetSchedule:
                m_Opts.ns_service = opt_value;
                break;
            case eInputFile:
                if ((m_Opts.input_stream = fopen(opt_value, "rb")) == NULL) {
                    fprintf(stderr, "%s: %s\n", opt_value, strerror(errno));
                    return 2;
                }
                break;
            case eOutputFile:
                if ((m_Opts.output_stream = fopen(opt_value, "wb")) == NULL) {
                    fprintf(stderr, "%s: %s\n", opt_value, strerror(errno));
                    return 2;
                }
                break;
            case eTotalNumberOfOptions: // Just to silence the compiler.
                break;
            }
        }

        if (IsOptionAcceptedButNotSet(eInputFile)) {
            m_Opts.input_stream = stdin;
#ifdef WIN32
            setmode(fileno(stdin), O_BINARY);
#endif
        }
        if (IsOptionAcceptedButNotSet(eOutputFile)) {
            m_Opts.output_stream = stdout;
#ifdef WIN32
            setmode(fileno(stdout), O_BINARY);
#endif
        }
    }

    return (this->*cmd_def->cmd_proc)();
}

CGridCommandLineInterfaceApp::~CGridCommandLineInterfaceApp()
{
    if (IsOptionSet(eInputFile) && m_Opts.input_stream != NULL)
        fclose(m_Opts.input_stream);
    if (IsOptionSet(eOutputFile) && m_Opts.output_stream != NULL)
        fclose(m_Opts.output_stream);
}

int main(int argc, const char* argv[])
{
    return CGridCommandLineInterfaceApp(argc, argv).AppMain(1, argv);
}
