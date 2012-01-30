#ifndef GRID_CLI__HPP
#define GRID_CLI__HPP

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
 * Authors:  Dmitry Kazimirov
 *
 */

/// @file grid_cli.hpp
/// Declarations of command line interface arguments and handlers.
///

#include <corelib/ncbiapp.hpp>

#include <connect/services/grid_client.hpp>
#include <connect/services/neticache_client.hpp>


#define PROGRAM_NAME "grid_cli"
#define PROGRAM_VERSION "1.2.0"

#define INPUT_OPTION "input"
#define INPUT_FILE_OPTION "input-file"
#define OUTPUT_FILE_OPTION "output-file"
#define QUEUE_OPTION "queue"
#define BATCH_OPTION "batch"
#define AFFINITY_OPTION "affinity"
#define JOB_OUTPUT_OPTION "job-output"
#define GET_NEXT_JOB_OPTION "get-next-job"
#define LIMIT_OPTION "limit"
#define TIMEOUT_OPTION "timeout"
#define CONFIRM_READ_OPTION "confirm-read"
#define ROLLBACK_READ_OPTION "rollback-read"
#define FAIL_READ_OPTION "fail-read"
#define JOB_ID_OPTION "job-id"
#define LISTENING_PORT_OPTION "listening-port"
#define WAIT_TIMEOUT_OPTION "wait-timeout"
#define FAIL_JOB_OPTION "fail-job"
#define NOW_OPTION "now"
#define DIE_OPTION "die"

#define READJOBS_COMMAND "readjobs"
#define REQUESTJOB_COMMAND "requestjob"

BEGIN_NCBI_SCOPE

enum EOption {
    eUntypedArg,
    eOptionalID,
    eID,
    eAuth,
    eInput,
    eInputFile,
    eOutputFile,
    eOutputFormat,
    eNetCache,
    eCache,
    ePassword,
    eOffset,
    eSize,
    eTTL,
    eEnableMirroring,
    eNetSchedule,
    eQueue,
    eWorkerNode,
    eBatch,
    eAffinity,
    eExclusiveJob,
    eJobOutput,
    eReturnCode,
    eGetNextJob,
    eLimit,
    eTimeout,
    eConfirmRead,
    eRollbackRead,
    eFailRead,
    eErrorMessage,
    eJobId,
    eClientInfo,
    eNotificationInfo,
    eAffinityInfo,
    eActiveJobCount,
    eJobsByAffinity,
    eJobsByStatus,
    eStartAfterJob,
    eJobCount,
    eSelectByStatus,
    eVerbose,
    eBrief,
    eStatusOnly,
    eProgressMessageOnly,
    eDeferExpiration,
    eExtendLifetime,
    eProgressMessage,
    eAllJobs,
    eDropJobs,
    eWaitTimeout,
    eListeningPort,
    eFailJob,
    eQueueArg,
    eModelQueue,
    eQueueDescription,
    eNow,
    eDie,
    eCompatMode,
    eExtendedOptionDelimiter,
    eClientNode,
    eClientSession,
    eCommand,
    eMultiline,
    eProtocolDump,
    eNumberOfOptions
};

enum EOutputFormat {
    eHumanReadable,
    eRaw,
    eJSON,
    eNumberOfOutputFormats
};

enum ENetScheduleStatTopic {
    eNetScheduleStatClients,
    eNetScheduleStatNotifications,
    eNetScheduleStatAffinities
};

#define OPTION_ACCEPTED 1
#define OPTION_SET 2
#define OPTION_N(number) (1 << number)

class CGridCommandLineInterfaceApp : public CNcbiApplication
{
public:
    CGridCommandLineInterfaceApp(int argc, const char* argv[]);

    virtual string GetProgramVersion() const;

    virtual int Run();

    virtual ~CGridCommandLineInterfaceApp();

private:
    int m_ArgC;
    const char** m_ArgV;

    struct SOptions {
        string id;
        string auth;
        EOutputFormat output_format;
        string nc_service;
        string cache_name;
        string password;
        size_t offset;
        size_t size;
        unsigned ttl;
        string ns_service;
        string queue;
        string affinity;
        string job_output;
        int return_code;
        unsigned batch_size;
        unsigned limit;
        unsigned timeout;
        string reservation_token;
        std::vector<std::string> job_ids;
        string start_after_job;
        size_t job_count;
        CNetScheduleAPI::EJobStatus job_status;
        time_t extend_lifetime_by;
        unsigned short listening_port;
        string client_node;
        string client_session;
        string progress_message;
        string queue_description;
        string error_message;
        string input;
        string command;
        FILE* input_stream;
        FILE* output_stream;
        FILE* protocol_dump;

        struct SICacheBlobKey {
            string key;
            int version;
            string subkey;

            SICacheBlobKey() : version(0) {}
        } icache_key;

        char option_flags[eNumberOfOptions];

        SOptions() : offset(0), size(0), ttl(0), return_code(0),
            batch_size(0), limit(0), timeout(0), job_count(0),
            extend_lifetime_by(0), listening_port(0),
            input_stream(NULL), output_stream(NULL), protocol_dump(NULL)
        {
            memset(option_flags, 0, eNumberOfOptions);
        }
    } m_Opts;

private:
    bool IsOptionAcceptedButNotSet(EOption option)
    {
        return m_Opts.option_flags[option] == OPTION_ACCEPTED;
    }

    bool IsOptionSet(EOption option)
    {
        return m_Opts.option_flags[option] == OPTION_SET;
    }

    int IsOptionSet(EOption option, int mask)
    {
        return m_Opts.option_flags[option] == OPTION_SET ? mask : 0;
    }

    CNetCacheAPI m_NetCacheAPI;
    CNetCacheAdmin m_NetCacheAdmin;
    CNetICacheClient m_NetICacheClient;
    CNetScheduleAPI m_NetScheduleAPI;
    CNetScheduleAdmin m_NetScheduleAdmin;
    CNetScheduleSubmitter m_NetScheduleSubmitter;
    CNetScheduleExecuter m_NetScheduleExecutor;
    auto_ptr<CGridClient> m_GridClient;

// NetCache commands.
public:
    int Cmd_GetBlob();
    int Cmd_PutBlob();
    int Cmd_BlobInfo();
    int Cmd_RemoveBlob();
    int Cmd_ReinitNetCache();

// NetSchedule commands.
public:
    int Cmd_JobInfo();
    int Cmd_SubmitJob();
    int Cmd_GetJobInput();
    int Cmd_GetJobOutput();
    int Cmd_ReadJobs();
    int Cmd_CancelJob();
    int Cmd_RequestJob();
    int Cmd_CommitJob();
    int Cmd_ReturnJob();
    int Cmd_UpdateJob();
    int Cmd_QueueInfo();
    int Cmd_DumpQueue();
    int Cmd_CreateQueue();
    int Cmd_GetQueueList();
    int Cmd_DeleteQueue();

// Miscellaneous commands.
public:
    int Cmd_WhatIs();
    int Cmd_ServerInfo();
    int Cmd_Stats();
    int Cmd_Health();
    int Cmd_GetConf();
    int Cmd_Reconf();
    int Cmd_Shutdown();
    int Cmd_Exec();
    int Cmd_Automate();

// Implementation details.
private:
    static void PrintLine(const string& line);
    enum EAPIClass {
        eUnknownAPI,
        eNetCacheAPI,
        eNetICacheClient,
        eNetCacheAdmin,
        eNetScheduleAPI,
        eNetScheduleAdmin,
        eNetScheduleSubmitter,
        eNetScheduleExecutor,
        eWorkerNodeAdmin,
        eGridClient
    };
    EAPIClass SetUp_AdminCmd();
    void SetUp_NetCacheCmd(EAPIClass api_class);
    void PrintBlobMeta(const CNetCacheKey& key);
    void ParseICacheKey(bool permit_empty_version = false,
        bool* version_is_defined = NULL);
    void PrintICacheServerUsed();
    void SetUp_NetScheduleCmd(EAPIClass api_class);
    void SetUp_GridClient();
    void PrintJobMeta(const CNetScheduleKey& key);
    static void PrintStorageType(const string& data, const char* prefix);
    static bool MatchPrefixAndPrintStorageTypeAndData(const string& line,
        const CTempString& prefix, const char* new_prefix);
    static bool ParseAndPrintJobEvents(const string& line);
    int DumpJobInputOutput(const string& data_or_blob_id);
    int PrintJobAttrsAndDumpInput(const CNetScheduleJob& job);

    EOutputFormat GetOutputFormatOption(
            EOutputFormat* allowed_formats,
            EOutputFormat default_format);

    void PrintNetScheduleStats();
    void PrintNetScheduleStats_Generic(ENetScheduleStatTopic topic);
};

END_NCBI_SCOPE

#endif // GRID_CLI__HPP
