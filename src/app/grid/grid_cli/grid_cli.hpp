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
#define PROGRAM_VERSION "0.1.0"

#define FAIL_JOB_OPTION "fail-job"
#define INPUT_FILE_OPTION "input-file"
#define QUEUE_OPTION "queue"
#define NOW_OPTION "now"
#define DIE_OPTION "die"

BEGIN_NCBI_SCOPE

enum EOption {
    eUntypedArg,
    eOptionalID,
    eID,
    eAuth,
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
    eWorkerNodes,
    eActiveJobCount,
    eBrief,
    eStatusOnly,
    eProgressMessageOnly,
    eDeferExpiration,
    eForceReschedule,
    eExtendLifetime,
    eProgressMessage,
    eAllJobs,
    eFailJob,
    eNow,
    eDie,
    eCompatMode,
    eTotalNumberOfOptions
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
    const char* const* m_ArgV;

    struct SOptions {
        string id;
        string auth;
        string nc_service;
        string cache_name;
        string password;
        size_t offset;
        size_t size;
        unsigned ttl;
        string ns_service;
        string queue;
        time_t extend_lifetime_by;
        string progress_message;
        string error_message;
        FILE* input_stream;
        FILE* output_stream;

        struct SICacheBlobKey {
            string key;
            int version;
            string subkey;

            SICacheBlobKey() : version(0) {}
        } icache_key;

        char option_flags[eTotalNumberOfOptions];

        SOptions() : offset(0), size(0), ttl(0),
            input_stream(NULL), output_stream(NULL)
        {
            memset(option_flags, 0, eTotalNumberOfOptions);
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
    int Cmd_GetJobOutput();
    int Cmd_CancelJob();
    int Cmd_Kill();
    int Cmd_RequestJob();
    int Cmd_CommitJob();
    int Cmd_ReturnJob();
    int Cmd_UpdateJob();

// Miscellaneous commands.
public:
    int Cmd_WhatIs();
    int Cmd_ServerInfo();
    int Cmd_Stats();
    int Cmd_Health();
    int Cmd_GetConf();
    int Cmd_Reconf();
    int Cmd_Shutdown();

// Implementation details.
private:
    void PrintLine(const string& line);
    enum EAPIClass {
        eUnknownAPI,
        eNetCacheAPI,
        eNetICacheClient,
        eNetCacheAdmin,
        eNetScheduleAPI,
        eNetScheduleAdmin,
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
};

END_NCBI_SCOPE

#endif // GRID_CLI__HPP
