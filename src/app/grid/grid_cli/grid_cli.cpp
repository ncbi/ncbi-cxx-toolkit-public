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
    SetVersion(CVersionInfo(PROGRAM_VERSION));
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
} static const s_OptionDefinitions[eNumberOfOptions] = {

    {CCommandLineParser::ePositionalArgument, eUntypedArg, "ARG", NULL},

    {CCommandLineParser::eOptionalPositional, eOptionalID, "ID", NULL},

    {CCommandLineParser::ePositionalArgument, eID, "ID", NULL},

    {CCommandLineParser::eOptionWithParameter, eAuth,
        "auth", "Authentication string (\"client_name\")."},

    {CCommandLineParser::eOptionWithParameter, eInput,
        INPUT_OPTION, "Provide input data on the command line. "
            "The standard input stream will not be read."},

    {CCommandLineParser::eOptionWithParameter, eInputFile,
        INPUT_FILE_OPTION, "Read input from the specified file."},

    {CCommandLineParser::eOptionWithParameter, eOutputFile,
        "o|" OUTPUT_FILE_OPTION, "Save output to the specified file."},

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

    {CCommandLineParser::eOptionWithParameter, eWorkerNode,
        "wn|worker-node", "Worker node address (a host:port pair)."},

    {CCommandLineParser::eOptionWithParameter, eBatch,
        BATCH_OPTION, "Enable batch mode and specify batch size."},

    {CCommandLineParser::eOptionWithParameter, eGroup,
        "group", "Define group membership for the created job(s)."},

    {CCommandLineParser::eOptionWithParameter, eAffinity,
        AFFINITY_OPTION, "Affinity token."},

    {CCommandLineParser::eOptionWithParameter, eAffinityList,
        "affinity-list", "Comma-separated list of affinity tokens."},

    {CCommandLineParser::eSwitch, eUsePreferredAffinities,
        "use-preferred-affinities", "Accept job with any of "
            "the affinities registered earlier as preferred."},

    {CCommandLineParser::eSwitch, eAnyAffinity,
        ANY_AFFINITY_OPTION, "Accept job with any available affinity."},

    {CCommandLineParser::eSwitch, eExclusiveJob,
        "exclusive-job", "Create an exclusive job."},

    {CCommandLineParser::eOptionWithParameter, eJobOutput,
        JOB_OUTPUT_OPTION, "Provide job output on the command line "
            "(inhibits reading from the standard input stream or an "
            "input file)."},

    {CCommandLineParser::eOptionWithParameter, eReturnCode,
        "return-code", "Job return code."},

    {CCommandLineParser::eSwitch, eGetNextJob,
        GET_NEXT_JOB_OPTION, "Request another job for execution."},

    {CCommandLineParser::eOptionWithParameter, eLimit,
        LIMIT_OPTION, "Maximum number of records to return."},

    {CCommandLineParser::eOptionWithParameter, eTimeout,
        TIMEOUT_OPTION, "Timeout in seconds."},

    {CCommandLineParser::eSwitch, eReliableRead,
        RELIABLE_READ_OPTION, "Enable reading confirmation mode."},

    {CCommandLineParser::eOptionWithParameter, eConfirmRead,
        CONFIRM_READ_OPTION, "For the reading reservation specified as "
            "the argument to this option, mark the job identified by "
            "'--" JOB_ID_OPTION "' as successfully retrieved."},

    {CCommandLineParser::eOptionWithParameter, eRollbackRead,
        ROLLBACK_READ_OPTION, "Release the specified reading "
            "reservation of the specified job."},

    {CCommandLineParser::eOptionWithParameter, eFailRead,
        FAIL_READ_OPTION, "Use the specified reading reservation "
            "to mark the job as impossible to read."},

    {CCommandLineParser::eOptionWithParameter, eErrorMessage,
        "error-message", "Provide an optional error message."},

    {CCommandLineParser::eOptionWithParameter, eJobId,
        JOB_ID_OPTION, "Job ID to operate on."},

    {CCommandLineParser::eSwitch, eJobGroupInfo,
        "job-group-info", "Print information on job groups."},

    {CCommandLineParser::eSwitch, eClientInfo,
        "client-info", "Print information on the recently "
            "connected clients."},

    {CCommandLineParser::eSwitch, eNotificationInfo,
        "notification-info", "Print a snapshot of the "
            "currently subscribed notification listeners."},

    {CCommandLineParser::eSwitch, eAffinityInfo,
        "affinity-info", "Print information on the "
            "currently handled affinities."},

    {CCommandLineParser::eSwitch, eActiveJobCount,
        "active-job-count", "Only print the total number of "
            "Pending and Running jobs in all queues combined."},

    {CCommandLineParser::eSwitch, eJobsByAffinity,
        "jobs-by-affinity", "For each affinity, print the number "
            "of pending jobs associated with it."},

    {CCommandLineParser::eSwitch, eJobsByStatus,
        "jobs-by-status", "Print the number of jobs itemized by their "
            "current status. If the '--" AFFINITY_OPTION "' option "
            "is given, only the jobs with the specified affinity "
            "will be counted."},

    {CCommandLineParser::eOptionWithParameter, eStartAfterJob,
        "start-after-job", "Specify the key of the last job "
            "in the previous dump batch."},

    {CCommandLineParser::eOptionWithParameter, eJobCount,
        "job-count", "Specify the maximum number of jobs in the output."},

    {CCommandLineParser::eOptionWithParameter, eSelectByStatus,
        "select-by-status", "Filter output by job status."},

    {CCommandLineParser::eSwitch, eVerbose,
        "verbose", "Produce more verbose output."},

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

    {CCommandLineParser::eOptionWithParameter, eProgressMessage,
        "progress-message", "Set job progress message."},

    {CCommandLineParser::eOptionWithParameter, eJobGroup,
        "job-group", "Select jobs by job group."},

    {CCommandLineParser::eSwitch, eAllJobs,
        "all-jobs", "Apply to all jobs in the queue."},

/*
    {CCommandLineParser::eOptionWithParameter, eRegisterWNode,
        "register-wnode", "Generate and print a new GUID "
            "and register the specified worker node control "
            "port with the generated GUID."},

    {CCommandLineParser::eOptionWithParameter, eUnregisterWNode,
        "unregister-wnode", "Unregister the worker node "
            "identified by the specified GUID."},
*/

    {CCommandLineParser::eOptionWithParameter, eWaitTimeout,
        WAIT_TIMEOUT_OPTION, "Wait up to the specified "
            "number of seconds for a response."},

    {CCommandLineParser::eOptionWithParameter, eFailJob,
        FAIL_JOB_OPTION, "Report the job as failed "
            "and specify an error message."},

    {CCommandLineParser::ePositionalArgument, eQueueArg, "QUEUE", NULL},

    {CCommandLineParser::ePositionalArgument, eModelQueue, "MODEL_QUEUE", NULL},

    {CCommandLineParser::eOptionWithParameter, eQueueDescription,
        "queue-description", "Optional queue description."},

    {CCommandLineParser::eSwitch, eNow,
        NOW_OPTION, "Take action immediately."},

    {CCommandLineParser::eSwitch, eDie,
        DIE_OPTION, "Terminate the server process abruptly."},

    {CCommandLineParser::eSwitch, eCompatMode,
        "compat-mode", "Enable backward compatibility tweaks."},

    {CCommandLineParser::eSwitch, eExtendedOptionDelimiter,
        NULL, NULL},

    {CCommandLineParser::eOptionWithParameter, eClientNode,
        "client-node", "Client application identifier."},

    {CCommandLineParser::eOptionWithParameter, eClientSession,
        "client-session", "Client session identifier."},

    {CCommandLineParser::ePositionalArgument, eCommand, "COMMAND", NULL},

    {CCommandLineParser::eSwitch, eMultiline,
        "multiline", "Expect multiple lines of output."},

    {CCommandLineParser::eOptionWithParameter, eProtocolDump,
        "protocol-dump", "Dump input and output messages of "
            "the automation protocol to the specified file."},

    {CCommandLineParser::eSwitch, eDumpNSNotifications,
        "dump-ns-notifications", "Suppress normal processing "
            "of this command, but print notifications received "
            "from the NetSchedule servers over UDP within "
            "the specified timeout."},

};

enum ECommandCategory {
    eGeneralCommand,
    eNetCacheCommand,
    eNetScheduleCommand,
    eExtendedCLICommand,
    eNumberOfCommandCategories
};

struct SCommandCategoryDefinition {
    int cat_id;
    const char* title;
} static const s_CategoryDefinitions[eNumberOfCommandCategories] = {
    {eGeneralCommand, "General commands"},
    {eNetCacheCommand, "NetCache commands"},
    {eNetScheduleCommand, "NetSchedule commands"},
    {eExtendedCLICommand, "Extended commands"},
};

#define ICACHE_KEY_FORMAT_EXPLANATION \
    "\n\nBoth NetCache and ICache modes are supported. " \
    "ICache mode requires blob ID to be specified in the " \
    "following format: \"key,version,subkey\"."

#define WN_NOT_NOTIFIED_DISCLAIMER \
    "Worker nodes that may have already " \
    "started job processing will not be notified."

struct SCommandDefinition {
    int cat_id;
    int (CGridCommandLineInterfaceApp::*cmd_proc)();
    const char* name_variants;
    const char* synopsis;
    const char* usage;
    int options[eNumberOfOptions + 1];
    int output_formats[eNumberOfOutputFormats + 1];
} static const s_CommandDefinitions[] = {

    {eGeneralCommand, &CGridCommandLineInterfaceApp::Cmd_WhatIs,
        "whatis", "Determine argument type and characteristics.",
        "This command makes an attempt to guess the type of its "
        "argument. If the argument is successfully recognized "
        "as a token that represents a Grid object, the type-"
        "dependent information about the object is printed.",
        {eUntypedArg, -1}},

    {eNetCacheCommand, &CGridCommandLineInterfaceApp::Cmd_BlobInfo,
        "blobinfo|bi", "Retrieve metadata of a NetCache blob.",
        "Print vital information about the specified blob. "
        "Expired blobs will be reported as not found."
        ICACHE_KEY_FORMAT_EXPLANATION,
        {eID, eNetCache, eCache, eAuth, -1}},

    {eNetCacheCommand, &CGridCommandLineInterfaceApp::Cmd_GetBlob,
        "getblob|gb", "Retrieve a blob from NetCache.",
        "Read the blob identified by ID and send its contents "
        "to the standard output (or to the specified output "
        "file). Expired blobs will be reported as not found."
        ICACHE_KEY_FORMAT_EXPLANATION,
        {eID, eNetCache, eCache, ePassword, eOffset,
            eSize, eOutputFile, eAuth, -1}},

    {eNetCacheCommand, &CGridCommandLineInterfaceApp::Cmd_PutBlob,
        "putblob|pb", "Create or update a NetCache blob.",
        "Read data from the standard input (or a file) until EOF is "
        "encountered and save the received data as a NetCache blob."
        ICACHE_KEY_FORMAT_EXPLANATION,
        {eOptionalID, eNetCache, eCache, ePassword, eTTL, eEnableMirroring,
            eInput, eInputFile, eCompatMode, eAuth, -1}},

    {eNetCacheCommand, &CGridCommandLineInterfaceApp::Cmd_RemoveBlob,
        "rmblob|rb", "Remove a NetCache blob.",
        "Delete a blob if it exists. If the blob has expired "
        "(or never existed), no errors are reported."
        ICACHE_KEY_FORMAT_EXPLANATION,
        {eID, eNetCache, eCache, ePassword, eAuth, -1}},

    {eNetCacheCommand, &CGridCommandLineInterfaceApp::Cmd_ReinitNetCache,
        "reinitnc", "Delete all blobs and reset NetCache database.",
        "This command purges and resets the specified NetCache "
        "(or ICache) database. Administrative privileges are "
        "required.",
        {eNetCache, eCache, eAuth, -1}},

    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_JobInfo,
        "jobinfo|ji", "Print information about a NetSchedule job.",
        "Print vital information about the specified NetSchedule job. "
        "Expired jobs will be reported as not found.",
        {eID, eNetSchedule, eQueue, eBrief, eStatusOnly, eDeferExpiration,
            eProgressMessageOnly, eAuth,
            eClientNode, eClientSession, -1}},

    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_SubmitJob,
        "submitjob", "Submit one or more jobs to a NetSchedule queue.",
        "Create one or multiple jobs by submitting input data to "
        "a NetSchedule queue. The first submitted job will be "
        "executed immediately as long as there is a worker node "
        "waiting for a job on that queue.\n\n"
        "This command has two modes of operation: single job submission "
        "and batch submission. The latter mode is activated by the '--"
        BATCH_OPTION "' option, which takes the maximum batch size "
        "as its argument. When this mode is enabled, all options that "
        "define job attributes are ignored. Instead, job attributes "
        "are read from the standard input stream or the specified "
        "input file - one line per job. Each line must contain a "
        "space-separated list of job attributes as follows:\n\n"
        "  input=\"DATA\"\n"
        "  affinity=\"TOKEN\"\n"
        "  exclusive\n\n"
        "Special characters in all quoted strings must be properly "
        "escaped. It is OK to omit quotation marks for a string that "
        "doesn't contain spaces. The \"input\" attribute is required.\n\n"
        "Example:\n\n"
        "  input=\"db, 8548@394.701\" exclusive\n\n"
        "In batch mode, the IDs of the created jobs are printed to the "
        "standard output stream (or the specified output file) one job "
        "ID per line.\n\n"
        "In single job submission mode, unless the '--" INPUT_FILE_OPTION
        "' or '--" INPUT_OPTION "' options are given, job input is read "
        "from the standard input stream, and the rest of attributes are "
        "taken from their respective command line options.\n\n"
        "If the '--" WAIT_TIMEOUT_OPTION "' option is given in single "
        "job submission mode, " PROGRAM_NAME " will wait for the job "
        "to terminate, and if the job terminates within the specified "
        "number of seconds, its final status will be printed right after "
        "the job ID. In case the status is 'Done', job output will be "
        "printed on the next line (unless the '--" OUTPUT_FILE_OPTION "' "
        "option is given, in which case the output goes to the specified "
        "file).\n\n"
        "A NetCache server is required for saving job input if it "
        "exceeds the capability of the NetSchedule internal storage.",
        {eNetSchedule, eQueue, eBatch, eNetCache, eInput, eInputFile,
            eGroup, eAffinity, eExclusiveJob, eOutputFile,
            eWaitTimeout, eAuth, eClientNode, eClientSession,
            eDumpNSNotifications, -1}},

    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_GetJobInput,
        "getjobinput", "Read job input.",
        "Retrieve and print job input to the standard output stream or "
        "save it to a file.",
        {eID, eNetSchedule, eQueue, eOutputFile, eAuth,
            eClientNode, eClientSession, -1}},

    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_GetJobOutput,
        "getjoboutput", "Read job output if the job is completed.",
        "Retrieve and print job output to the standard output stream or "
        "save it to a file. If the job does not exist or has not been "
        "completed successfully, an appropriate error message is printed "
        "to the standard error stream and the program exits with a non-zero "
        "return code.",
        {eID, eNetSchedule, eQueue, eOutputFile, eAuth,
            eClientNode, eClientSession, -1}},

    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_ReadJob,
        READJOB_COMMAND, "Return the next job that's done or failed.",
        "Incrementally harvest IDs of completed and failed jobs. This "
        "command has two modes of operation: simple mode (without "
        "acknowledgment) and reliable mode (with acknowledgment). "
        "The former is the default; the latter is triggered by the "
        "'--" RELIABLE_READ_OPTION "' option.\n\n"
        "In simple mode, if any of the specified NetSchedule servers "
        "has a job that's done or failed, the ID of that job will be "
        "printed on the first line, and its status - 'Done' or 'Failed' "
        "- on the second line. Also, if the job is 'Done', its entire "
        "output will be printed as well, starting from the third line "
        "(unless the '--" OUTPUT_FILE_OPTION "' option is given, in "
        "which case the output goes to the specified file).\n\n"
        "After the job output has been successfully printed, the status "
        "of the job is immediately changed to 'Confirmed', which means "
        "that the job won't be available for reading anymore.\n\n"
        "In reliable mode, job reading is a two-step process. The first "
        "step, which is triggered by the '--" RELIABLE_READ_OPTION "' "
        "option, acquires a reading reservation. If there's a job that's "
        "done or failed, its ID is printed on the first line along "
        "with its final status ('Done' or 'Failed') on the next line "
        "and a unique reservation token on the third line. This first "
        "step changes the status of the returned job from 'Done' to "
        "'Reading'. The reading reservation is valid only for the "
        "number of seconds defined by the '--" TIMEOUT_OPTION "' option. "
        "If the server does not receive a reading confirmation (see "
        "below) within this time frame, the job will change its status "
        "back to the original status (either 'Done' or 'Failed').\n\n"
        "The second step is activated by one of the following "
        "finalization options: '--" CONFIRM_READ_OPTION "', '--"
        ROLLBACK_READ_OPTION "', or '--" FAIL_READ_OPTION "'. Each of "
        "these options requires the reservation token that was issued "
        "by NetSchedule during the first step to be specified as the "
        "argument for the option. The corresponding job ID must be "
        "provided with the '--" JOB_ID_OPTION "' option. The job must "
        "still be in the 'Reading' status. After the finalization "
        "step, the status of the job will change depending on the "
        "option given as per the following table:\n\n"
        "    Option              Resulting status\n"
        "    ================    ================\n"
        "    --" CONFIRM_READ_OPTION "      Confirmed\n"
        "    --" FAIL_READ_OPTION "         ReadFailed\n"
        "    --" ROLLBACK_READ_OPTION "     Done or Failed\n\n"
        "The 'Confirmed' status and the 'ReadFailed' status are final and "
        "cannot be changed, while '--" ROLLBACK_READ_OPTION "' makes the "
        "jobs available for subsequent '" READJOB_COMMAND "' commands.\n\n"
        "In either mode, if there are no completed or failed jobs "
        "in the queue, nothing will be printed and the exit code "
        "will be zero.",
        {eNetSchedule, eQueue, eOutputFile, eJobGroup,
            eReliableRead, eTimeout, eJobId, eConfirmRead,
            eRollbackRead, eFailRead, eErrorMessage,
            eAuth, eClientNode, eClientSession, -1}},

    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_CancelJob,
        "canceljob", "Cancel one or more NetSchedule jobs.",
        "Mark the specified job (or multiple jobs) as canceled. "
        "This command also instructs the worker node that may be "
        "processing those jobs to stop the processing.",
        {eOptionalID, eNetSchedule, eQueue, eAllJobs, eJobGroup,
            eAuth, eClientNode, eClientSession, -1}},

/*
    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_RegWNode,
        "regwnode", "Register or unregister a worker node.",
        "This command initiates and terminates worker node sessions "
        "on NetSchedule servers.",
        {eNetSchedule, eQueue, eRegisterWNode, eUnregisterWNode, eAuth,
            eClientNode, eClientSession, -1}},
*/

    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_RequestJob,
        REQUESTJOB_COMMAND, "Get a job from NetSchedule for processing.",
        "Return a job pending for execution. The status of the job is changed "
        "from \"Pending\" to \"Running\" before the job is returned. "
        "This command makes it possible for " PROGRAM_NAME " to emulate a "
        "worker node.\n\n"
        "The affinity-related options affect how the job is selected. "
        "Unless the '--" ANY_AFFINITY_OPTION "' option is given, a job "
        "is returned only if its affinity matches one of the specified "
        "affinities.\n\n"
        "If a job is acquired, its ID and attributes are printed to the "
        "standard output stream on the first and the second lines "
        "respectively, followed by the input data of the job unless the '--"
        OUTPUT_FILE_OPTION "' option is specified, in which case the "
        "input data will be saved to that file.\n\n"
        "The format of the line with job attributes is as follows:\n\n"
        "[affinity=\"job_affinity\"] [exclusive]\n\n"
        "If none of the NetSchedule servers has pending jobs in the "
        "specified queue, nothing is printed and the exit code of zero "
        "is returned.",
        {eNetSchedule, eQueue, eAffinityList, eUsePreferredAffinities,
            eAnyAffinity, eOutputFile, eWaitTimeout,
            eAuth, eClientNode, eClientSession,
            eDumpNSNotifications, -1}},

    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_CommitJob,
        "commitjob", "Mark the job as complete or failed.",
        "Change the state of the job to either 'Done' or 'Failed'. This "
        "command can only be executed on jobs that are in the 'Running' "
        "state.\n\n"
        "Unless the '--" JOB_OUTPUT_OPTION "' option is given, job "
        "output is read from the standard input stream or a file.\n\n"
        "If the job is being reported as failed, an error message "
        "must be provided with the '--" FAIL_JOB_OPTION "' command "
        "line option.\n\n"
        "If the '--" GET_NEXT_JOB_OPTION "' option is given, this "
        "command makes an attempt to acquire another pending job and "
        "if it succeeds, its output is identical to that of the "
        REQUESTJOB_COMMAND " command. Otherwise, no output is produced.",
        {eID, eNetSchedule, eQueue,
            eNetCache, eReturnCode, eJobOutput, eInputFile, eFailJob,
            eGetNextJob, eAffinity, eOutputFile, eAuth,
            eClientNode, eClientSession, -1}},

    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_ReturnJob,
        "returnjob", "Return a previously accepted job.",
        "Due to insufficient resources or for any other reason, "
        "this command can be used by a worker node to return a "
        "previously accepted job back to the NetSchedule queue. "
        "The job will change its state from Running back to "
        "Pending, but the information about previous runs will "
        "not be discarded, and the expiration time will not be "
        "advanced.",
        {eID, eNetSchedule, eQueue, eAuth,
            eClientNode, eClientSession, -1}},

    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_UpdateJob,
        "updatejob", "Modify attributes of an existing job.",
        "Change one or more job properties. The outcome depends "
        "on the current state of the job.",
        {eID, eNetSchedule, eQueue,
            eExtendLifetime, eProgressMessage, eAuth,
            eClientNode, eClientSession, -1}},

    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_QueueInfo,
        "queueinfo|qi", "Get information about a NetSchedule queue.",
        "Print queue type (static or dynamic). For dynamic queues, "
        "print also their model queue name and description.",
        {eQueueArg, eNetSchedule, eAuth,
            eClientNode, eClientSession, -1}},

    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_DumpQueue,
        "dumpqueue", "Dump a NetSchedule queue.",
        "This command dumps detailed information about jobs in a "
        "NetSchedule queue. It is possible to limit the number of "
        "records printed and also to filter the output by job status "
        "and/or job group.",
        {eNetSchedule, eQueue, eStartAfterJob, eJobCount, eJobGroup,
            eSelectByStatus, eAuth, eClientNode, eClientSession, -1}},

    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_CreateQueue,
        "createqueue", "Create a dynamic NetSchedule queue.",
        "This command creates a new NetSchedule queue using "
        "a template known as a model queue.",
        {eQueueArg, eModelQueue, eNetSchedule, eQueueDescription, eAuth,
            eClientNode, eClientSession, -1}},

    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_GetQueueList,
        "getqueuelist", "Print the list of available NetSchedule queues.",
        "This command takes a NetSchedule service name (or server "
        "address) and queries each server participating that service "
        "for the list of configured or dynamically created queues. "
        "The collected lists are then combined in a single list of "
        "queues available on all servers in the service. For each "
        "queue available only on a subset of servers, its servers "
        "are listed in parentheses after the queue name.",
        {eNetSchedule, eAuth, eClientNode, eClientSession, -1}},

    {eNetScheduleCommand, &CGridCommandLineInterfaceApp::Cmd_DeleteQueue,
        "deletequeue", "Delete a dynamic NetSchedule queue.",
        WN_NOT_NOTIFIED_DISCLAIMER "\n\n"
        "Static queues cannot be deleted, although it is "
        "possible to cancel all jobs in a static queue.",
        {eQueueArg, eNetSchedule, eAuth, eClientNode, eClientSession, -1}},

    {eGeneralCommand, &CGridCommandLineInterfaceApp::Cmd_ServerInfo,
        "serverinfo|si", "Print information about a Grid server.",
        "Query and print information about a running "
        "NetCache, NetSchedule, or worker node process.\n\n"
        "If the '--" QUEUE_OPTION "' option is specified "
        "for a NetSchedule server, queue parameters will "
        "be printed instead.",
        {eNetCache, eNetSchedule, eWorkerNode, eQueue,
            eCompatMode, eAuth, eClientNode, eClientSession, -1}},

    {eGeneralCommand, &CGridCommandLineInterfaceApp::Cmd_Stats,
        "stats", "Show server access statistics.",
        "Dump accumulated statistics on server access and "
        "performance.\n\n"
        "When applied to a NetSchedule server, this operation "
        "supports the following format options: \"raw\", "
        "\"human-readable\", \"json\".  If none specified, "
        "\"human-readable\" is assumed.",
        {eNetCache, eNetSchedule, eWorkerNode, eQueue, eBrief,
            eJobGroupInfo, eClientInfo, eNotificationInfo, eAffinityInfo,
            eActiveJobCount, eJobsByAffinity, eJobsByStatus,
            eAffinity, eJobGroup, eVerbose, eOutputFormat,
            eCompatMode, eAuth, eClientNode, eClientSession, -1},
            {eHumanReadable, eRaw, eJSON, -1}},

    {eNetCacheCommand, &CGridCommandLineInterfaceApp::Cmd_Health,
        "health", "Evaluate availability of a server.",
        "Retrieve vital parameters of a running NetCache "
        "or NetSchedule server and estimate its availability "
        "coefficient."
        /*"\n\nValid output format options for this operation: "
        "\"raw\""/ *, \"xml\", \"json\"* /".  If none specified, "
        "\"raw\" is assumed."*/,
        {eNetCache, /*eOutputFormat,*/ eAuth, -1}},

    {eGeneralCommand, &CGridCommandLineInterfaceApp::Cmd_GetConf,
        "getconf", "Dump actual configuration of a server.",
        "Print the effective configuration parameters of a "
        "running NetCache or NetSchedule server.\n\n"
        "If the '--" QUEUE_OPTION "' option is specified "
        "for a NetSchedule server, a reduced number of "
        "configuration parameters relevant to that queue "
        "only will be printed.",
        {eNetCache, eNetSchedule, eQueue, eAuth,
            eClientNode, eClientSession, -1}},

    {eGeneralCommand, &CGridCommandLineInterfaceApp::Cmd_Reconf,
        "reconf", "Reload server configuration.",
        "Update configuration parameters of a running server. "
        "The server will look for a configuration file in the "
        "same location that was used during start-up.",
        {eNetCache, eNetSchedule, eAuth, -1}},

    {eGeneralCommand, &CGridCommandLineInterfaceApp::Cmd_Shutdown,
        "shutdown", "Send a shutdown request to a remote server.",
        "Depending on the option specified, this command sends "
        "a shutdown request to a NetCache or NetSchedule server "
        "or a worker node process.\n\n"
        "Additional options '--" NOW_OPTION "' and '--" DIE_OPTION
        "' are applicable only to NetSchedule servers and worker "
        "nodes.",
        {eNetCache, eNetSchedule, eWorkerNode, eNow, eDie,
            eCompatMode, eAuth, eClientNode, eClientSession, -1}},

    {eExtendedCLICommand, &CGridCommandLineInterfaceApp::Cmd_Exec,
        "exec", "Execute an arbitrary command on one or more servers.",
        "This command is intended for testing and debugging purposes."
        "\n\nThe following output formats are supported: \"raw\" and "
        "\"json\". The default is \"raw\".",
        {eCommand, eNetCache, eNetSchedule, eQueue, eMultiline,
            eAuth, eClientNode, eClientSession, eOutputFormat, -1},
        {eRaw, eJSON, -1}},

    {eExtendedCLICommand, &CGridCommandLineInterfaceApp::Cmd_Automate,
        "automate", "Start as a pipe-based automation server.",
        "This command starts " PROGRAM_NAME " as an automation "
        "server that can be used to interact with Grid objects "
        "through a Python module (ncbi.grid).",
        {eProtocolDump, -1}},
};

#define TOTAL_NUMBER_OF_COMMANDS int(sizeof(s_CommandDefinitions) / \
    sizeof(*s_CommandDefinitions))

static const char* const s_OutputFormats[eNumberOfOutputFormats] = {
    "human-readable",   /* eHumanReadable   */
    "raw",              /* eRaw             */
    "json"              /* eJSON            */
};

int CGridCommandLineInterfaceApp::Run()
{
    // Override connection defaults.
    CNcbiRegistry& reg = GetConfig();
    const string netservice_api_section("netservice_api");
    const string max_find_lbname_retries("max_find_lbname_retries");
    if (!reg.HasEntry(netservice_api_section, max_find_lbname_retries))
        reg.Set(netservice_api_section, max_find_lbname_retries, "0");
    const string connection_max_retries("connection_max_retries");
    if (!reg.HasEntry(netservice_api_section, connection_max_retries))
        reg.Set(netservice_api_section, connection_max_retries, "0");

    const SCommandDefinition* cmd_def;

    {
        bool enable_extended_cli = false;

        int argc = m_ArgC - 1;
        const char** argv = m_ArgV + 1;

        while (--argc >= 0)
            if (strcmp(*argv, "--extended-cli") != 0)
                ++argv;
            else {
                enable_extended_cli = true;
                --m_ArgC;
                if (argc > 0)
                    memcpy(argv, argv + 1, argc * sizeof(*argv));
            }

        CCommandLineParser clparser(PROGRAM_NAME, PROGRAM_VERSION,
            "Utility to access and control NCBI Grid services.");

        const SOptionDefinition* opt_def = s_OptionDefinitions;
        int i = eNumberOfOptions;
        do {
            if (opt_def->opt_id != eExtendedOptionDelimiter)
                clparser.AddOption(opt_def->type, opt_def->opt_id,
                    opt_def->name_variants, opt_def->description ?
                        opt_def->description : kEmptyStr);
            else if (!enable_extended_cli)
                break;
            ++opt_def;
        } while (--i > 0);

        const SCommandCategoryDefinition* cat_def = s_CategoryDefinitions;
        i = eNumberOfCommandCategories;
        do {
            clparser.AddCommandCategory(cat_def->cat_id, cat_def->title);
            ++cat_def;
        } while (--i > 0);

        cmd_def = s_CommandDefinitions;
        for (i = 0; i < TOTAL_NUMBER_OF_COMMANDS; ++i, ++cmd_def) {
            if (cmd_def->cat_id == eExtendedCLICommand && !enable_extended_cli)
                continue;
            clparser.AddCommand(i, cmd_def->name_variants,
                cmd_def->synopsis, cmd_def->usage, cmd_def->cat_id);
            for (const int* opt_id = cmd_def->options; *opt_id >= 0; ++opt_id)
                if (*opt_id < eExtendedOptionDelimiter || enable_extended_cli)
                    clparser.AddAssociation(i, *opt_id);
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

        if (m_Opts.option_flags[eOutputFormat])
            m_Opts.output_format = (EOutputFormat) *cmd_def->output_formats;

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
            case eQueueArg:
            case eJobId:
                m_Opts.id = opt_value;
                break;
            case eAuth:
                m_Opts.auth = opt_value;
                break;
            case eOutputFormat:
                {
                    const int* format = cmd_def->output_formats;
                    while (NStr::CompareNocase(opt_value,
                                s_OutputFormats[*format]) != 0)
                        if (*++format < 0) {
                            fprintf(stderr, PROGRAM_NAME
                                    " %s: invalid output format '%s'.\n",
                                    cmd_def->name_variants, opt_value);
                            return 2;
                        }
                    m_Opts.output_format = (EOutputFormat) *format;
                }
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
            case eWorkerNode:
                m_Opts.ns_service = opt_value;
                break;
            case eQueue:
            case eModelQueue:
                m_Opts.queue = opt_value;
                break;
            case eAffinity:
            case eAffinityList:
                m_Opts.affinity = opt_value;
                break;
            case eJobOutput:
                m_Opts.job_output = opt_value;
                break;
            case eReturnCode:
                m_Opts.return_code = NStr::StringToInt(opt_value);
                break;
            case eBatch:
                if ((m_Opts.batch_size = NStr::StringToUInt(opt_value)) == 0) {
                    fprintf(stderr, PROGRAM_NAME
                        " %s: batch size must be greater that zero.\n",
                            cmd_def->name_variants);
                    return 2;
                }
                break;
            case eLimit:
                m_Opts.limit = NStr::StringToUInt(opt_value);
                break;
            case eTimeout:
            case eWaitTimeout:
                m_Opts.timeout = NStr::StringToUInt(opt_value);
                break;
            case eConfirmRead:
            case eRollbackRead:
            case eFailRead:
                m_Opts.auth_token = opt_value;
                break;
            case eStartAfterJob:
                m_Opts.start_after_job = opt_value;
                break;
            case eJobCount:
                m_Opts.job_count = NStr::StringToSizet(opt_value);
                break;
            case eSelectByStatus:
                if ((m_Opts.job_status = CNetScheduleAPI::StringToStatus(
                        opt_value)) == CNetScheduleAPI::eJobNotFound) {
                    fprintf(stderr, PROGRAM_NAME
                        ": invalid job status '%s'\n", opt_value);
                    return 2;
                }
                break;
            case eExtendLifetime:
                m_Opts.extend_lifetime_by = NStr::StringToUInt(opt_value);
                break;
            case eClientNode:
                m_Opts.client_node = opt_value;
                break;
            case eClientSession:
                m_Opts.client_session = opt_value;
                break;
            case eProgressMessage:
                m_Opts.progress_message = opt_value;
                break;
            case eGroup:
            case eJobGroup:
                m_Opts.job_group = opt_value;
                break;
            case eErrorMessage:
            case eFailJob:
                m_Opts.error_message = opt_value;
                break;
            case eQueueDescription:
                m_Opts.queue_description = opt_value;
                break;
            case eInput:
                m_Opts.input = opt_value;
                break;
            case eCommand:
                m_Opts.command = opt_value;
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
            case eProtocolDump:
                if ((m_Opts.protocol_dump = fopen(opt_value, "a")) == NULL) {
                    fprintf(stderr, "%s: %s\n", opt_value, strerror(errno));
                    return 2;
                }
                break;
            default: // Just to silence the compiler.
                break;
            }
        }

        if (m_Opts.auth.empty())
            m_Opts.auth = PROGRAM_NAME;

        if (IsOptionAcceptedButNotSet(eInputFile)) {
            m_Opts.input_stream = stdin;
#ifdef WIN32
            setmode(fileno(stdin), O_BINARY);
#endif
        } else if (IsOptionSet(eInput) && IsOptionSet(eInputFile)) {
            fprintf(stderr, PROGRAM_NAME ": options '--" INPUT_FILE_OPTION
                "' and '--" INPUT_OPTION "' are mutually exclusive.\n");
        }
        if (IsOptionAcceptedButNotSet(eOutputFile)) {
            m_Opts.output_stream = stdout;
#ifdef WIN32
            setmode(fileno(stdout), O_BINARY);
#endif
        }
    }

    try {
        return (this->*cmd_def->cmd_proc)();
    }
    catch (CConfigException& e) {
        fprintf(stderr, "%s\n", e.GetMsg().c_str());
        return 2;
    }
    catch (CArgException& e) {
        fprintf(stderr, "%s\n", e.GetErrCode() == CArgException::eInvalidArg ?
            e.GetMsg().c_str() : e.what());
        return 2;
    }
    catch (CException& e) {
        fprintf(stderr, "%s\n", e.what());
        return 3;
    }
    catch (string& s) {
        fprintf(stderr, "%s\n", s.c_str());
        return 4;
    }
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
