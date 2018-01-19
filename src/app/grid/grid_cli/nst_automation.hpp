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
    using TSelf = SNetStorageService;

    virtual const string& GetType() const { return kName; }

    CNetService GetService() { return m_NetStorageAdmin.GetService(); }

    void ExecClientsInfo(const TArguments& args, SInputOutput& io);
    void ExecUsersInfo(const TArguments& args, SInputOutput& io);
    void ExecClientObjects(const TArguments& args, SInputOutput& io);
    void ExecUserObjects(const TArguments& args, SInputOutput& io);
    void ExecServerInfo(const TArguments& args, SInputOutput& io);
    void ExecOpenObject(const TArguments& args, SInputOutput& io);
    void ExecGetServers(const TArguments& args, SInputOutput& io);

    static CCommand CallCommand();
    static TCommands CallCommands();
    static CCommand NewCommand();
    static CAutomationObject* Create(const TArguments& args, CAutomationProc* automation_proc);

    static const string kName;

protected:
    void ExecRequest(const string& request_type, SInputOutput& io);

    CNetStorageAdmin m_NetStorageAdmin;

    SNetStorageService(CAutomationProc* automation_proc,
            CNetStorageAdmin nst_api);
};

struct SNetStorageServer : public SNetStorageService
{
    using TSelf = SNetStorageServer;

    SNetStorageServer(CAutomationProc* automation_proc,
            CNetStorageAdmin nst_api, CNetServer::TInstance server);

    virtual const string& GetType() const { return kName; }

    void ExecHealth(const TArguments& args, SInputOutput& io);
    void ExecConf(const TArguments& args, SInputOutput& io);
    void ExecMetadataInfo(const TArguments& args, SInputOutput& io);
    void ExecReconf(const TArguments& args, SInputOutput& io);
    void ExecAckAlert(const TArguments& args, SInputOutput& io);

    static CCommand CallCommand();
    static TCommands CallCommands();
    static CCommand NewCommand();
    static CAutomationObject* Create(const TArguments& args, CAutomationProc* automation_proc);

    static const string kName;
};

struct SNetStorageObject : public CAutomationObject
{
    using TSelf = SNetStorageObject;

    SNetStorageObject(CAutomationProc* automation_proc,
            CNetStorageObject::TInstance object);

    virtual const string& GetType() const override { return kName; }

    void ExecInfo(const TArguments& args, SInputOutput& io);
    void ExecAttrList(const TArguments& args, SInputOutput& io);
    void ExecGetAttr(const TArguments& args, SInputOutput& io);

    static CCommand CallCommand();
    static TCommands CallCommands();

    static const string kName;

private:
    CNetStorageObject m_Object;
};

}

END_NCBI_SCOPE

#endif // NST_AUTOMATION__HPP
