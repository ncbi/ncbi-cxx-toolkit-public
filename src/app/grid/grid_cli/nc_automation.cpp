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

const string SNetCacheBlobAutomationObject::kName = "ncblob";
const string SNetCacheServiceAutomationObject::kName = "ncsvc";
const string SNetCacheServerAutomationObject::kName = "ncsrv";

SNetCacheBlobAutomationObject::SNetCacheBlobAutomationObject(
        SNetCacheServiceAutomationObject* nc_object,
        const string& blob_key) :
    CAutomationObject(nc_object->m_AutomationProc),
    m_NetCacheObject(nc_object),
    m_BlobKey(blob_key)
{
}

const void* SNetCacheBlobAutomationObject::GetImplPtr() const
{
    return this;
}

NAutomation::CCommand SNetCacheBlobAutomationObject::CallCommand()
{
    return NAutomation::CCommand(kName, CallCommands);
}

NAutomation::TCommands SNetCacheBlobAutomationObject::CallCommands()
{
    NAutomation::TCommands cmds =
    {
        { "write", {
                { "value", CJsonNode::eString, },
            }},
        { "read", {
                { "buf_size", -1, },
            }},
        { "close", },
        { "get_key", },
    };

    return cmds;
}

bool SNetCacheBlobAutomationObject::Call(const string& method,
        CArgArray& arg_array, CJsonNode& reply)
{
    if (method == "write") {
        string value(arg_array.NextString());
        if (m_Reader.get() != NULL) {
            NCBI_THROW(CAutomationException, eCommandProcessingError,
                    "Cannot write blob while it's being read");
        } else if (m_Writer.get() == NULL) {
            bool return_key = m_BlobKey.empty();
            m_Writer.reset(m_NetCacheObject->m_NetCacheAPI.PutData(&m_BlobKey));
            if (return_key)
                reply.AppendString(m_BlobKey);
        }
        m_Writer->Write(value.data(), value.length());
    } else if (method == "read") {
        if (m_Writer.get() != NULL) {
            NCBI_THROW(CAutomationException, eCommandProcessingError,
                    "Cannot read blob while it's being written");
        } else if (m_Reader.get() == NULL)
            m_Reader.reset(m_NetCacheObject->m_NetCacheAPI.GetReader(
                    m_BlobKey, &m_BlobSize));
        size_t buf_size = (size_t) arg_array.NextInteger(-1);
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
    } else if (method == "close") {
        if (m_Reader.get() != NULL)
            m_Reader.reset();
        else if (m_Writer.get() != NULL) {
            m_Writer->Close();
            m_Writer.reset();
        }
    } else if (method == "get_key") {
        reply.AppendString(m_BlobKey);
    } else
        return false;

    return true;
}

void SNetCacheServiceAutomationObject::CEventHandler::OnWarning(
        const string& warn_msg, CNetServer server)
{
    m_AutomationProc->SendWarning(warn_msg, m_AutomationProc->
            ReturnNetCacheServerObject(m_NetCacheAPI, server));
}

const void* SNetCacheServiceAutomationObject::GetImplPtr() const
{
    return m_NetCacheAPI;
}

SNetCacheServiceAutomationObject::SNetCacheServiceAutomationObject(
        CAutomationProc* automation_proc,
        CNetCacheAPI nc_api, CNetService::EServiceType type) :
    TBase(automation_proc, type),
    m_NetCacheAPI(nc_api)
{
    m_Service = m_NetCacheAPI.GetService();
    m_NetCacheAPI.SetEventHandler(
            new CEventHandler(automation_proc, m_NetCacheAPI));
}

SNetCacheServerAutomationObject::SNetCacheServerAutomationObject(
        CAutomationProc* automation_proc,
        CNetCacheAPI nc_api, CNetServer::TInstance server) :
    TBase(automation_proc, nc_api.GetServer(server),
            CNetService::eSingleServerService),
    m_NetServer(server)
{
    if (m_Service.IsLoadBalanced()) {
        NCBI_THROW(CAutomationException, eCommandProcessingError,
                "NetCacheServer constructor: "
                "'server_address' must be a host:port combination");
    }
}

NAutomation::CCommand SNetCacheServiceAutomationObject::NewCommand()
{
    return NAutomation::CCommand(kName, {
            { "service_name", "", },
            { "client_name", "", },
        });
}

CAutomationObject* SNetCacheServiceAutomationObject::Create(
        CArgArray& arg_array, const string& class_name,
        CAutomationProc* automation_proc)
{
    if (class_name != kName) return nullptr;

    const string service_name(arg_array.NextString(kEmptyStr));
    const string client_name(arg_array.NextString(kEmptyStr));
    CNetCacheAPI nc_api(service_name, client_name);
    return new SNetCacheServiceAutomationObject(automation_proc, nc_api,
            CNetService::eLoadBalancedService);
}

NAutomation::CCommand SNetCacheServerAutomationObject::NewCommand()
{
    return NAutomation::CCommand(kName, {
            { "service_name", "", },
            { "client_name", "", },
        });
}

CAutomationObject* SNetCacheServerAutomationObject::Create(
        CArgArray& arg_array, const string& class_name,
        CAutomationProc* automation_proc)
{
    if (class_name != kName) return nullptr;

    const string service_name(arg_array.NextString(kEmptyStr));
    const string client_name(arg_array.NextString(kEmptyStr));
    CNetCacheAPI nc_api(service_name, client_name);
    CNetServer server = nc_api.GetService().Iterate().GetServer();
    return new SNetCacheServerAutomationObject(automation_proc, nc_api, server);
}

const void* SNetCacheServerAutomationObject::GetImplPtr() const
{
    return m_NetServer;
}

TAutomationObjectRef CAutomationProc::ReturnNetCacheServerObject(
        CNetCacheAPI::TInstance ns_api,
        CNetServer::TInstance server)
{
    TAutomationObjectRef object(FindObjectByPtr(server));
    if (!object) {
        object = new SNetCacheServerAutomationObject(this, ns_api, server);
        AddObject(object, server);
    }
    return object;
}

NAutomation::CCommand SNetCacheServiceAutomationObject::CallCommand(
        const string& name)
{
    return NAutomation::CCommand(name, CallCommands);
}

NAutomation::TCommands SNetCacheServiceAutomationObject::CallCommands()
{
    NAutomation::TCommands cmds =
    {
        { "get_blob", {
                { "blob_key", "", },
            }},
        { "get_servers", },
    };

    NAutomation::TCommands base_cmds = TBase::CallCommands();
    cmds.insert(cmds.end(), base_cmds.begin(), base_cmds.end());

    return cmds;
}

bool SNetCacheServiceAutomationObject::Call(const string& method,
        CArgArray& arg_array, CJsonNode& reply)
{
    if (method == "get_blob") {
        reply.AppendInteger(m_AutomationProc->AddObject(
                TAutomationObjectRef(new SNetCacheBlobAutomationObject(this,
                        arg_array.NextString(kEmptyStr)))));
    } else if (method == "get_servers") {
        CJsonNode object_ids(CJsonNode::NewArrayNode());
        for (CNetServiceIterator it = m_NetCacheAPI.GetService().Iterate(
                CNetService::eIncludePenalized); it; ++it)
            object_ids.AppendInteger(m_AutomationProc->
                    ReturnNetCacheServerObject(m_NetCacheAPI, *it)->GetID());
        reply.Append(object_ids);
    } else
        return TBase::Call(method, arg_array, reply);

    return true;
}

bool SNetCacheServerAutomationObject::Call(const string& method,
        CArgArray& arg_array, CJsonNode& reply)
{
    return TBase::Call(method, arg_array, reply);
}
