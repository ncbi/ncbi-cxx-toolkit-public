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
 * File Description: Automation processor - NetCache declarations.
 *
 */

#ifndef NC_AUTOMATION__HPP
#define NC_AUTOMATION__HPP

#include "automation.hpp"

#include <connect/services/impl/netcache_api_int.hpp>

BEGIN_NCBI_SCOPE

namespace NAutomation
{

struct SNetCacheService : public SNetService
{
    using TSelf = SNetCacheService;

    virtual const string& GetType() const { return kName; }

    CNetService GetService() { return m_NetICacheClient.GetService(); }

    void ExecGetBlob(const TArguments& args, SInputOutput& io);
    void ExecGetServers(const TArguments& args, SInputOutput& io);

    static CCommand CallCommand();
    static TCommands CallCommands();
    static CCommand NewCommand();
    static CAutomationObject* Create(const TArguments& args, CAutomationProc* automation_proc);

    IReader* GetReader(const string& blob_key, size_t& blob_size);
    IReader* GetReader(const string& blob_key, int blob_version, const string& blob_subkey, size_t& blob_size);

    IEmbeddedStreamWriter* GetWriter(string& blob_key);
    IEmbeddedStreamWriter* GetWriter(const string& blob_key, int blob_version, const string& blob_subkey);

    static const string kName;

protected:
    SNetCacheService(CAutomationProc* automation_proc,
            CNetICacheClientExt ic_api);

    CNetICacheClientExt m_NetICacheClient;
    CNetCacheAPI m_NetCacheAPI;
};

struct SNetCacheBlob : public CAutomationObject
{
    using TSelf = SNetCacheBlob;

    SNetCacheBlob(SNetCacheService* nc_object, const string& blob_key);

    virtual const string& GetType() const { return kName; }

    void ExecWrite(const TArguments& args, SInputOutput& io);
    void ExecRead(const TArguments& args, SInputOutput& io);
    void ExecClose(const TArguments& args, SInputOutput& io);

    static CCommand CallCommand();
    static TCommands CallCommands();

    CRef<SNetCacheService> m_NetCacheObject;
    string m_BlobKey;
    size_t m_BlobSize;
    unique_ptr<IReader> m_Reader;
    unique_ptr<IEmbeddedStreamWriter> m_Writer;

    static const string kName;

private:
    virtual void SetWriter();
    virtual void SetReader();
    virtual void ExecGetKey(const TArguments& args, SInputOutput& io);
};

struct SNetICacheBlob : SNetCacheBlob
{
    SNetICacheBlob(SNetCacheService* nc_object,
            const string& blob_key, int blob_version, const string& blob_subkey);

private:
    void SetWriter() override;
    void SetReader() override;
    void ExecGetKey(const TArguments& args, SInputOutput& io) override;

    int m_BlobVersion;
    string m_BlobSubKey;
};

struct SNetCacheServer : public SNetCacheService
{
    using TSelf = SNetCacheServer;

    SNetCacheServer(CAutomationProc* automation_proc,
            CNetICacheClientExt ic_api, CNetServer::TInstance server);

    virtual const string& GetType() const { return kName; }

    static CCommand CallCommand();
    static CCommand NewCommand();
    static CAutomationObject* Create(const TArguments& args, CAutomationProc* automation_proc);

    static const string kName;
};

}

END_NCBI_SCOPE

#endif // NC_AUTOMATION__HPP
