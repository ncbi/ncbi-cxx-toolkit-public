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

namespace NAutomation
{

struct SNetStorageService : public SNetServiceBase
{
    typedef SNetServiceBase TBase;

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

    SNetStorageService(CAutomationProc* automation_proc,
            CArgArray& arg_array);

    virtual const string& GetType() const { return kName; }

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

    static CCommand CallCommand();
    static TCommands CallCommands();
    static CCommand NewCommand() { return NewCommand(kName); }
    static CAutomationObject* Create(CArgArray& arg_array,
            const string& class_name, CAutomationProc* automation_proc);

protected:
    CNetStorageAdmin m_NetStorageAdmin;

    SNetStorageService(CAutomationProc* automation_proc,
            CNetStorageAdmin nst_api, CNetService::EServiceType type);

    static CCommand NewCommand(const string& name);

private:
    static const string kName;
};

struct SNetStorageServer : public SNetStorageService
{
    typedef SNetStorageService TBase;

    SNetStorageServer(CAutomationProc* automation_proc,
            CNetStorageAdmin nst_api, CNetServer::TInstance server);

    virtual const string& GetType() const { return kName; }

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

    static CCommand CallCommand();
    static TCommands CallCommands();
    static CCommand NewCommand() { return TBase::NewCommand(kName); }
    static CAutomationObject* Create(CArgArray& arg_array,
            const string& class_name, CAutomationProc* automation_proc);

private:
    CNetServer m_NetServer;

    static const string kName;
};

struct SNetStorageObject : public CAutomationObject
{
    SNetStorageObject(CAutomationProc* automation_proc,
            CNetStorageObject::TInstance object);

    virtual const string& GetType() const override { return kName; }
    virtual const void* GetImplPtr() const override;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply) override;

    static CCommand CallCommand();
    static TCommands CallCommands();

private:
    CNetStorageObject m_Object;

    static const string kName;
};

}

END_NCBI_SCOPE

#endif // NST_AUTOMATION__HPP
