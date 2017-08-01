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
 * File Description: NetCache automation implementation.
 *
 */

#include <ncbi_pch.hpp>

#include "nc_automation.hpp"

USING_NCBI_SCOPE;

using namespace NAutomation;

const string SNetCacheBlob::kName = "ncblob";
const string SNetCacheService::kName = "ncsvc";
const string SNetCacheServer::kName = "ncsrv";

SNetCacheBlob::SNetCacheBlob(
        SNetCacheService* nc_object,
        const string& blob_key) :
    CAutomationObject(nc_object->m_AutomationProc),
    m_NetCacheObject(nc_object),
    m_BlobKey(blob_key)
{
}

const void* SNetCacheBlob::GetImplPtr() const
{
    return this;
}

CCommand SNetCacheBlob::CallCommand()
{
    return CCommand(kName, CallCommands);
}

TCommands SNetCacheBlob::CallCommands()
{
    TCommands cmds =
    {
        { "write",   ExecMethod<TSelf, &TSelf::ExecWrite>, {
                { "value", CJsonNode::eString, },
            }},
        { "read",    ExecMethod<TSelf, &TSelf::ExecRead>, {
                { "buf_size", -1, },
            }},
        { "close",   ExecMethod<TSelf, &TSelf::ExecClose>, },
        { "get_key", ExecMethod<TSelf, &TSelf::ExecGetKey>, },
    };

    return cmds;
}

void SNetCacheBlob::ExecWrite(const TArguments& args, SInputOutput& io)
{
    auto& arg_array = io.arg_array;
    auto& reply = io.reply;
    auto value = arg_array.NextString();

    if (m_Reader.get() != NULL) {
        NCBI_THROW(CAutomationException, eCommandProcessingError,
                "Cannot write blob while it's being read");
    } else if (m_Writer.get() == NULL) {
        // SetWriter() puts blob ID into m_BlobKey, so we have to check it before that
        const bool return_blob_key = m_BlobKey.empty();
        SetWriter();
        if (return_blob_key) reply.AppendString(m_BlobKey);
    }
    m_Writer->Write(value.data(), value.length());
}

void SNetCacheBlob::ExecRead(const TArguments& args, SInputOutput& io)
{
    auto& arg_array = io.arg_array;
    auto& reply = io.reply;
    auto buf_size = (size_t) arg_array.NextInteger(-1);

    if (m_Writer.get() != NULL) {
        NCBI_THROW(CAutomationException, eCommandProcessingError,
                "Cannot read blob while it's being written");
    } else if (m_Reader.get() == NULL)
        SetReader();
    if (buf_size > m_BlobSize)
        buf_size = m_BlobSize;
    string buffer(buf_size, 0);
    char* buf_ptr = &buffer[0];
    size_t bytes_read;
    size_t total_bytes_read = 0;

    while (buf_size > 0) {
        ERW_Result rw_res = m_Reader->Read(buf_ptr, buf_size, &bytes_read);
        if (rw_res == eRW_Success) {
            total_bytes_read += bytes_read;
            buf_ptr += bytes_read;
            buf_size -= bytes_read;
        } else if (rw_res == eRW_Eof) {
            break;
        } else {
            NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                    "I/O error while reading " << m_BlobKey);
        }
    }
    buffer.resize(total_bytes_read);
    m_BlobSize -= total_bytes_read;

    reply.AppendString(buffer);
}

void SNetCacheBlob::ExecClose(const TArguments&, SInputOutput&)
{
    if (m_Reader.get() != NULL)
        m_Reader.reset();
    else if (m_Writer.get() != NULL) {
        m_Writer->Close();
        m_Writer.reset();
    }
}

bool SNetCacheBlob::Call(const string& method, const TArguments& args, SInputOutput& io)
{
    if (method == "write") {
        ExecWrite(args, io);
    } else if (method == "read") {
        ExecRead(args, io);
    } else if (method == "close") {
        ExecClose(args, io);
    } else if (method == "get_key") {
        ExecGetKey(args, io);
    } else
        return false;

    return true;
}

void SNetCacheBlob::SetWriter()
{
    m_Writer.reset(m_NetCacheObject->GetWriter(m_BlobKey));
}

void SNetCacheBlob::SetReader()
{
    m_Reader.reset(m_NetCacheObject->GetReader(m_BlobKey, m_BlobSize));
}

void SNetCacheBlob::ExecGetKey(const TArguments&, SInputOutput& io)
{
    auto& reply = io.reply;
    reply.AppendString(m_BlobKey);
}

SNetICacheBlob::SNetICacheBlob(SNetCacheService* nc_object,
        const string& blob_key, int blob_version, const string& blob_subkey) :
    SNetCacheBlob(nc_object, blob_key),
    m_BlobVersion(blob_version),
    m_BlobSubKey(blob_subkey)
{
}

void SNetICacheBlob::SetWriter()
{
    m_Writer.reset(m_NetCacheObject->GetWriter(m_BlobKey, m_BlobVersion, m_BlobSubKey));
}

void SNetICacheBlob::SetReader()
{
    m_Reader.reset(m_NetCacheObject->GetReader(m_BlobKey, m_BlobVersion, m_BlobSubKey, m_BlobSize));
}

void SNetICacheBlob::ExecGetKey(const TArguments&, SInputOutput& io)
{
    auto& reply = io.reply;
    reply.AppendString(m_BlobKey);
    reply.AppendInteger(m_BlobVersion);
    reply.AppendString(m_BlobSubKey);
}

void SNetCacheService::CEventHandler::OnWarning(
        const string& warn_msg, CNetServer server)
{
    m_AutomationProc->SendWarning(warn_msg, m_AutomationProc->
            ReturnNetCacheServerObject(m_NetICacheClient, server));
}

const void* SNetCacheService::GetImplPtr() const
{
    return m_NetICacheClient;
}

SNetCacheService::SNetCacheService(CAutomationProc* automation_proc,
        CNetICacheClientExt ic_api, CNetService::EServiceType type) :
    SNetService(automation_proc, type),
    m_NetICacheClient(ic_api),
    m_NetCacheAPI(ic_api.GetNetCacheAPI())
{
    m_Service = m_NetICacheClient.GetService();
    ic_api.SetEventHandler(new CEventHandler(automation_proc, m_NetICacheClient));
}

SNetCacheServer::SNetCacheServer(CAutomationProc* automation_proc,
        CNetICacheClientExt ic_api, CNetServer::TInstance server) :
    SNetCacheService(automation_proc, ic_api.GetServer(server),
            CNetService::eSingleServerService),
    m_NetServer(server)
{
    if (m_Service.IsLoadBalanced()) {
        NCBI_THROW(CAutomationException, eCommandProcessingError,
                "NetCacheServer constructor: "
                "'server_address' must be a host:port combination");
    }
}

CCommand SNetCacheService::NewCommand()
{
    return CCommand(kName, ExecNew<TSelf>, {
            { "service_name", "", },
            { "client_name", "", },
            { "cache_name", "", },
        });
}

CAutomationObject* SNetCacheService::Create(const TArguments& args, CAutomationProc* automation_proc)
{
    _ASSERT(args.size() == 3);

    const auto service_name = args[0].AsString();
    const auto client_name  = args[1].AsString();
    const auto cache_name   = args[2].AsString();

    CNetICacheClientExt ic_api(CNetICacheClient(service_name, cache_name, client_name));
    return new SNetCacheService(automation_proc, ic_api,
            CNetService::eLoadBalancedService);
}

CCommand SNetCacheServer::NewCommand()
{
    return CCommand(kName, ExecNew<TSelf>, {
            { "service_name", "", },
            { "client_name", "", },
            { "cache_name", "", },
        });
}

CAutomationObject* SNetCacheServer::Create(const TArguments& args, CAutomationProc* automation_proc)
{
    _ASSERT(args.size() == 3);

    const auto service_name = args[0].AsString();
    const auto client_name  = args[1].AsString();
    const auto cache_name   = args[2].AsString();

    CNetICacheClientExt ic_api(CNetICacheClient(service_name, cache_name, client_name));
    CNetServer server = ic_api.GetService().Iterate().GetServer();
    return new SNetCacheServer(automation_proc, ic_api, server);
}

const void* SNetCacheServer::GetImplPtr() const
{
    return m_NetServer;
}

TAutomationObjectRef CAutomationProc::ReturnNetCacheServerObject(
        CNetICacheClient::TInstance ic_api,
        CNetServer::TInstance server)
{
    TAutomationObjectRef object(FindObjectByPtr(server));
    if (!object) {
        object = new SNetCacheServer(this, ic_api, server);
        AddObject(object, server);
    }
    return object;
}

CCommand SNetCacheService::CallCommand()
{
    return CCommand(kName, CallCommands);
}

CCommand SNetCacheServer::CallCommand()
{
    return CCommand(kName, CallCommands);
}

TCommands SNetCacheService::CallCommands()
{
    TCommands cmds =
    {
        { "get_blob",    ExecMethod<TSelf, &TSelf::ExecGetBlob>, {
                { "blob_key", "", },
                { "blob_version", 0, },
                { "blob_subkey", "", },
            }},
        { "get_servers", ExecMethod<TSelf, &TSelf::ExecGetServers>, },
    };

    TCommands base_cmds = SNetService::CallCommands();
    cmds.insert(cmds.end(), base_cmds.begin(), base_cmds.end());

    return cmds;
}

void SNetCacheService::ExecGetBlob(const TArguments& args, SInputOutput& io)
{
    auto& arg_array = io.arg_array;
    auto& reply = io.reply;
    const string blob_key(arg_array.NextString(kEmptyStr));
    const int blob_version(static_cast<int>(arg_array.NextInteger(0)));
    const string blob_subkey(arg_array.NextString(kEmptyStr));

    TAutomationObjectRef blob;

    if (blob_key.empty() || CNetCacheKey::IsValidKey(blob_key)) {
        blob.Reset(new SNetCacheBlob(this, blob_key));
    } else {
        blob.Reset(new SNetICacheBlob(this, blob_key, blob_version, blob_subkey));
    }

    reply.AppendInteger(m_AutomationProc->AddObject(blob));
}

void SNetCacheService::ExecGetServers(const TArguments&, SInputOutput& io)
{
    auto& reply = io.reply;

    CJsonNode object_ids(CJsonNode::NewArrayNode());
    for (CNetServiceIterator it = m_NetICacheClient.GetService().Iterate(
            CNetService::eIncludePenalized); it; ++it)
        object_ids.AppendInteger(m_AutomationProc->
                ReturnNetCacheServerObject(m_NetICacheClient, *it)->GetID());
    reply.Append(object_ids);
}

bool SNetCacheService::Call(const string& method, const TArguments& args, SInputOutput& io)
{
    if (method == "get_blob") {
        ExecGetBlob(args, io);
    } else if (method == "get_servers") {
        ExecGetServers(args, io);
    } else
        return SNetService::Call(method, args, io);

    return true;
}

bool SNetCacheServer::Call(const string& method, const TArguments& args, SInputOutput& io)
{
    return SNetCacheService::Call(method, args, io);
}

IReader* SNetCacheService::GetReader(const string& blob_key, size_t& blob_size)
{
    return m_NetCacheAPI.GetReader(blob_key, &blob_size);
}

IReader* SNetCacheService::GetReader(const string& blob_key, int blob_version, const string& blob_subkey, size_t& blob_size)
{
    auto rv = m_NetICacheClient.GetReadStream(blob_key, blob_version, blob_subkey, &blob_size);
    if (!rv) NCBI_THROW(CNetCacheException, eBlobNotFound, "BLOB not found");
    return rv;
}

IEmbeddedStreamWriter* SNetCacheService::GetWriter(string& blob_key)
{
    return m_NetCacheAPI.PutData(&blob_key);
}

IEmbeddedStreamWriter* SNetCacheService::GetWriter(const string& blob_key, int blob_version, const string& blob_subkey)
{
    return m_NetICacheClient.GetNetCacheWriter(blob_key, blob_version, blob_subkey);
}
