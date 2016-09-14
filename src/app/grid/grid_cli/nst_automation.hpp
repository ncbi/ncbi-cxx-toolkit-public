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
 * Authors: Rafael Sadyrov
 *
 * File Description: Automation processor - NetStorage declarations.
 *
 */

#ifndef NST_AUTOMATION__HPP
#define NST_AUTOMATION__HPP

#include "automation.hpp"

BEGIN_NCBI_SCOPE

struct SNetStorageServiceAutomationObject : public SNetServiceBaseAutomationObject
{
    class CEventHandler : public INetEventHandler
    {
    public:
        CEventHandler(CAutomationProc* automation_proc,
                CNetStorageAdmin::TInstance ns_api) :
            m_AutomationProc(automation_proc),
            m_NetStorageAdmin(ns_api)
        {
        }

        virtual void OnWarning(const string& warn_msg, CNetServer server);

    private:
        CAutomationProc* m_AutomationProc;
        CNetStorageAdmin::TInstance m_NetStorageAdmin;
    };

    SNetStorageServiceAutomationObject(CAutomationProc* automation_proc,
            CArgArray& arg_array);

    virtual const string& GetType() const { return kName; }

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

protected:
    CNetStorageAdmin m_NetStorageAdmin;

    SNetStorageServiceAutomationObject(CAutomationProc* automation_proc,
            const CNetStorageAdmin& nst_server) :
        SNetServiceBaseAutomationObject(automation_proc,
                CNetService::eSingleServerService),
        m_NetStorageAdmin(nst_server)
    {
        m_Service = m_NetStorageAdmin.GetService();
    }

private:
    static const string kName;
};

struct SNetStorageServerAutomationObject : public SNetStorageServiceAutomationObject
{
    SNetStorageServerAutomationObject(CAutomationProc* automation_proc,
            CNetStorageAdmin nst_api, CNetServer::TInstance server) :
        SNetStorageServiceAutomationObject(automation_proc,
                nst_api.GetServer(server)),
        m_NetServer(server)
    {
    }

    SNetStorageServerAutomationObject(CAutomationProc* automation_proc,
            CArgArray& arg_array);

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

#endif // NST_AUTOMATION__HPP
