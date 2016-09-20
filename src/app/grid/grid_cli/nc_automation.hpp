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

BEGIN_NCBI_SCOPE

struct SNetCacheServiceAutomationObject : public SNetServiceAutomationObject
{
    typedef SNetServiceAutomationObject TBase;

    class CEventHandler : public INetEventHandler
    {
    public:
        CEventHandler(CAutomationProc* automation_proc,
                CNetCacheAPI::TInstance ns_api) :
            m_AutomationProc(automation_proc),
            m_NetCacheAPI(ns_api)
        {
        }

        virtual void OnWarning(const string& warn_msg, CNetServer server);

    private:
        CAutomationProc* m_AutomationProc;
        CNetCacheAPI::TInstance m_NetCacheAPI;
    };

    SNetCacheServiceAutomationObject(CAutomationProc* automation_proc,
            const string& service_name, const string& client_name) :
        TBase(automation_proc, CNetService::eLoadBalancedService),
        m_NetCacheAPI(service_name, client_name)
    {
        m_Service = m_NetCacheAPI.GetService();
        m_NetCacheAPI.SetEventHandler(
                new CEventHandler(automation_proc, m_NetCacheAPI));
    }

    virtual const string& GetType() const { return kName; }

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

    static NAutomation::CCommand CallCommand();
    static NAutomation::TCommands CallCommands();

protected:
    CNetCacheAPI m_NetCacheAPI;

    SNetCacheServiceAutomationObject(CAutomationProc* automation_proc,
            const CNetCacheAPI& nc_server) :
        TBase(automation_proc, CNetService::eSingleServerService),
        m_NetCacheAPI(nc_server)
    {
        m_Service = m_NetCacheAPI.GetService();
    }

    friend struct SNetCacheBlobAutomationObject;

private:
    static const string kName;
};

struct SNetCacheBlobAutomationObject : public CAutomationObject
{
    SNetCacheBlobAutomationObject(SNetCacheServiceAutomationObject* nc_object,
            const string& blob_key) :
        CAutomationObject(nc_object->m_AutomationProc),
        m_NetCacheObject(nc_object),
        m_BlobKey(blob_key)
    {
    }

    virtual const string& GetType() const { return kName; }

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

    static NAutomation::CCommand CallCommand();
    static NAutomation::TCommands CallCommands();

    CRef<SNetCacheServiceAutomationObject> m_NetCacheObject;
    string m_BlobKey;
    size_t m_BlobSize;
    auto_ptr<IReader> m_Reader;
    auto_ptr<IEmbeddedStreamWriter> m_Writer;

private:
    static const string kName;
};

struct SNetCacheServerAutomationObject : public SNetCacheServiceAutomationObject
{
    typedef SNetCacheServiceAutomationObject TBase;

    SNetCacheServerAutomationObject(CAutomationProc* automation_proc,
            CNetCacheAPI nc_api, CNetServer::TInstance server) :
        TBase(automation_proc, nc_api.GetServer(server)),
        m_NetServer(server)
    {
    }

    SNetCacheServerAutomationObject(CAutomationProc* automation_proc,
        const string& service_name, const string& client_name);

    virtual const string& GetType() const { return kName; }

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

private:
    CNetServer m_NetServer;

private:
    static const string kName;
};

END_NCBI_SCOPE

#endif // NC_AUTOMATION__HPP
