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
        INPUT_FILE_OPTION, "Read input from the specified file."},

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

    {CCommandLineParser::eOptionWithParameter, eQueue,
        QUEUE_OPTION, "NetSchedule queue."},

    {CCommandLineParser::eSwitch, eWorkerNodes,
        "worker-nodes", "Print the list of active worker nodes."},

    {CCommandLineParser::eSwitch, eActiveJobCount,
        "active-job-count", "Only print the total number of "
            "Pending and Running jobs in all queues combined."},

    {CCommandLineParser::eSwitch, eBrief,
        "brief", "Produce less verbose output."},

    {CCommandLineParser::eSwitch, eStatusOnly,
        "status-only", "Print job status only."},

    {CCommandLineParser::eSwitch, eProgressMessageOnly,
        "progress-message-only", "Print only the progress message."},

    {CCommandLineParser::eSwitch, eDeferExpiration,
        "defer-expiration", "Prolong job lifetime by "
            "updating its last access timestamp."},

    {CCommandLineParser::eOptionWithParameter, eExtendLifetime,
        "extend-lifetime", "Extend job lifetime by "
            "the specified number of seconds."},

    {CCommandLineParser::eSwitch, eAllJobs,
        "all-jobs", "Apply to all jobs in the queue."},

    {CCommandLineParser::eOptionWithParameter, eFailJob,
        FAIL_JOB_OPTION, "Report the job as failed."},

    {CCommandLineParser::eSwitch, eNow,
        NOW_OPTION, "Take action immediately."},

    {CCommandLineParser::eSwitch, eDie,
        DIE_OPTION, "Terminate the server process abruptly."},

    {CCommandLineParser::eSwitch, eCompatMode,
        "compat-mode", "Enable backward compatibility tweaks."},

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

    {&CGridCommandLineInterfaceApp::Cmd_BlobInfo,
        "blobinfo|bi", "Retrieve meta information on a NetCache blob.",
        "Print vital information about the specified blob. "
        "Expired blobs will be reported as not found."
        ICACHE_KEY_FORMAT_EXPLANATION,
        {eID, eNetCache, eCache, eAuth, -1}},

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

    {&CGridCommandLineInterfaceApp::Cmd_JobInfo,
        "jobinfo|ji", "Retrieve meta information on a NetSchedule job.",
        "Print vital information about the specified NetSchedule job. "
        "Expired jobs will be reported as not found.",
        {eID, eQueue, eBrief, eStatusOnly, eDeferExpiration,
            eProgressMessageOnly, eAuth, -1}},

/*
    {&CGridCommandLineInterfaceApp::Cmd_SubmitJob,
        "submitjob", "Submit a job to a NetSchedule queue.",
        "Create a job by submitting input data to a NetSchedule queue. "
        "If a worker node is waiting for a job on that queue, "
        "the newly created job will get executed immediately.\n\n"
        "This command requires a NetCache server for saving job "
        "input if it exceeds the capability of the NetSchedule "
        "internal storage.\n\n"
        "Unless '--" INPUT_FILE_OPTION "' option is given, the input "
        "is read from the standard input stream.",
        {eNetSchedule, eQueue, eNetCache, eInputFile, eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_GetJobOutput,
        "getjoboutput", "Read job output if the job is completed.",
        "Retrieve and print job output to the standard output stream or "
        "save it to a file. If the job does not exist or has not been "
        "completed successfully, an appropriate error message is printed "
        "to the standard error stream and the program exits with a non-zero "
        "return code.",
        {eID, eQueue, eOutputFile, eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_CancelJob,
        "canceljob", "Cancel a NetSchedule job.",
        "Mark the job as canceled. This command also instructs the worker "
        "node that may be processing this job to stop the processing.",
        {eID, eQueue, eAuth, -1}},
*/

    {&CGridCommandLineInterfaceApp::Cmd_Kill,
        "kill", "Delete NetSchedule job(s).",
        "Delete one or all job records from the specified NetSchedule "
        "queue. Information about the jobs is completely wiped out as "
        "if the jobs never existed. Worker nodes that may have already "
        "started job processing will not be notified.",
        {eOptionalID, eNetSchedule, eQueue, eAllJobs, eCompatMode, eAuth, -1}},

/*
    {&CGridCommandLineInterfaceApp::Cmd_RequestJob,
        "requestjob", "Get a job from NetSchedule for processing.",
        "Return a job pending for execution. The status of the job is changed "
        "from \"Pending\" to \"Running\" before the job is returned. "
        "This command makes it possible for " PROGRAM_NAME " to emulate a "
        "worker node. If none of the NetSchedule servers has pending jobs "
        "in the specified queue, an error is printed and the program "
        "terminates with a non-zero exit code.",
        {eNetSchedule, eQueue, eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_CommitJob,
        "commitjob", "Mark the job as complete or failed.",
        "Change the state of the job to either 'Done' or 'Failed'. This "
        "command can only be executed on jobs that are in the 'Running' "
        "state.\n\n"
        "If the job is being reported as successfully completed, job output "
        "is read from the standard input stream or a file. If the job is being "
        "reported as failed, an error message must be provided along with the "
        "'--" FAIL_JOB_OPTION "' command line option.",
        {eID, eQueue, eNetCache, eInputFile, eFailJob, eAuth, -1}},
*/

    {&CGridCommandLineInterfaceApp::Cmd_ServerInfo,
        "serverinfo|si", "Print server information.",
        "Query and print information about a running "
        "NetCache, NetSchedule, or worker node process.\n\n"
        "If the '--" QUEUE_OPTION "' option is specified "
        "for a NetSchedule server, queue parameters will "
        "be printed as well.",
        {eNetCache, eNetSchedule, eQueue, eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_Stats,
        "stats", "Show server access statistics.",
        "Dump accumulated statistics on server access and "
        "performance.",
        {eNetCache, eNetSchedule, eQueue, eBrief,
            eWorkerNodes, eActiveJobCount, eAuth, -1}},

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
        {eNetCache, eNetSchedule, eQueue, eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_Reconf,
        "reconf", "Reload server configuration.",
        "Update configuration parameters of a running server. "
        "The server will look for a configuration file in the "
        "same location that was used during start-up.",
        {eNetCache, eNetSchedule, eAuth, -1}},

    {&CGridCommandLineInterfaceApp::Cmd_Shutdown,
        "shutdown", "Send a shutdown request to a remote server.",
        "Depending on the option specified, this command sends "
        "a shutdown request to a NetCache or NetSchedule server "
        "or a worker node process.\n\n"
        "Additional options '--" NOW_OPTION "' and '--" DIE_OPTION
        "' are applicable to NetSchedule servers only.",
        {eNetCache, eNetSchedule, eNow, eDie, eCompatMode, eAuth, -1}},
};

#define TOTAL_NUMBER_OF_COMMANDS int(sizeof(s_CommandDefinitions) / \
    sizeof(*s_CommandDefinitions))

int CGridCommandLineInterfaceApp::Run()
{
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
            case eNetSchedule:
                m_Opts.ns_service = opt_value;
                break;
            case eQueue:
                m_Opts.queue = opt_value;
                break;
            case eExtendLifetime:
                m_Opts.extend_lifetime_by = NStr::StringToUInt(opt_value);
                break;
            case eFailJob:
                m_Opts.error_message = opt_value;
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
            default: // Just to silence the compiler.
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

void CGridCommandLineInterfaceApp::PrintLine(const string& line)
{
    printf("%s\n", line.c_str());
}

int main(int argc, const char* argv[])
{
    return CGridCommandLineInterfaceApp(argc, argv).AppMain(1, argv);
}
