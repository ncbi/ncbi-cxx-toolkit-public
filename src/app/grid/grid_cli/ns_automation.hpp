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
 * File Description: Automation processor - NetSchedule declarations.
 *
 */

#ifndef NS_AUTOMATION__HPP
#define NS_AUTOMATION__HPP

#include "automation.hpp"

BEGIN_NCBI_SCOPE

namespace NAutomation
{

struct SNetScheduleServiceAutomationObject : public SNetServiceAutomationObject
{
    typedef SNetServiceAutomationObject TBase;

    class CEventHandler : public INetEventHandler
    {
    public:
        CEventHandler(CAutomationProc* automation_proc,
                CNetScheduleAPI::TInstance ns_api) :
            m_AutomationProc(automation_proc),
            m_NetScheduleAPI(ns_api)
        {
        }

        virtual void OnWarning(const string& warn_msg, CNetServer server);

    private:
        CAutomationProc* m_AutomationProc;
        CNetScheduleAPI::TInstance m_NetScheduleAPI;
    };

    virtual const string& GetType() const { return kName; }

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

    static NAutomation::CCommand CallCommand();
    static NAutomation::TCommands CallCommands();
    static NAutomation::CCommand NewCommand();
    static CAutomationObject* Create(CArgArray& arg_array,
            const string& class_name, CAutomationProc* automation_proc);

protected:
    CNetScheduleAPIExt m_NetScheduleAPI;

    SNetScheduleServiceAutomationObject(CAutomationProc* automation_proc,
            CNetScheduleAPI ns_api, CNetService::EServiceType type);

private:
    static const string kName;
};

struct SNetScheduleServerAutomationObject :
        public SNetScheduleServiceAutomationObject
{
    typedef SNetScheduleServiceAutomationObject TBase;

    SNetScheduleServerAutomationObject(CAutomationProc* automation_proc,
            CNetScheduleAPIExt ns_api, CNetServer::TInstance server);

    virtual const string& GetType() const { return kName; }

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

    static NAutomation::CCommand CallCommand();
    static NAutomation::TCommands CallCommands();
    static NAutomation::CCommand NewCommand();
    static CAutomationObject* Create(CArgArray& arg_array,
            const string& class_name, CAutomationProc* automation_proc);

private:
    CNetServer m_NetServer;

private:
    static const string kName;
};

}

END_NCBI_SCOPE

#endif // NS_AUTOMATION__HPP
