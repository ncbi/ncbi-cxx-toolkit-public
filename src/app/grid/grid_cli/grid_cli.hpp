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

#include <connect/services/netcache_api.hpp>
#include <connect/services/neticache_client.hpp>


#define PROGRAM_NAME "grid_cli"
#define PROGRAM_VERSION "0.1.0"


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
    eTotalNumberOfOptions
};

#define OPTION_ACCEPTED 1
#define OPTION_SET 2

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
        bool enable_mirroring;
        string ns_service;
        FILE* input_stream;
        FILE* output_stream;

        struct SICacheBlobKey {
            string key;
            int version;
            string subkey;

            SICacheBlobKey() : version(0) {}
        } icache_key;

        char option_flags[eTotalNumberOfOptions];

        SOptions() : offset(0), size(0), ttl(0), enable_mirroring(false),
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

    CNetCacheAPI m_NetCacheAPI;
    CNetCacheAdmin m_NetCacheAdmin;
    CNetICacheClient m_NetICacheClient;

// NetCache commands.
public:
    int Cmd_GetBlob();
    int Cmd_PutBlob();
    int Cmd_BlobInfo();
    int Cmd_RemoveBlob();
    int Cmd_ReinitNetCache();

// Miscellaneous commands.
public:
    int Cmd_WhatIs();
    int Cmd_Version();
    int Cmd_Stats();
    int Cmd_Health();
    int Cmd_GetConf();
    int Cmd_Reconf();
    int Cmd_Shutdown();

// Implementation details specific to NetCache commands.
private:
    enum ENetCacheAPIClass {
        eNetCacheAPI,
        eNetICacheClient,
        eNetCacheAdmin
    };
    bool SetUp_AdminCmd();
    void SetUp_NetCacheCmd(ENetCacheAPIClass api_class);
    void ParseICacheKey(bool permit_empty_version = false,
        bool* version_is_defined = NULL);
};

END_NCBI_SCOPE

#endif // GRID_CLI__HPP
