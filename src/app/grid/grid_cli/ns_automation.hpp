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

struct SNetScheduleServiceAutomationObject : public SNetServiceAutomationObject
{
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

    SNetScheduleServiceAutomationObject(CAutomationProc* automation_proc,
            const string& service_name, const string& queue_name,
            const string& client_name) :
        SNetServiceAutomationObject(automation_proc,
                CNetService::eLoadBalancedService),
        m_NetScheduleAPI(CNetScheduleAPIExt::CreateNoCfgLoad(
                    service_name, client_name, queue_name))
    {
        m_Service = m_NetScheduleAPI.GetService();
        m_NetScheduleAPI.SetEventHandler(
                new CEventHandler(automation_proc, m_NetScheduleAPI));
    }

    virtual const string& GetType() const;

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

    CNetScheduleAPIExt m_NetScheduleAPI;

protected:
    SNetScheduleServiceAutomationObject(CAutomationProc* automation_proc,
            const CNetScheduleAPI::TInstance ns_server) :
        SNetServiceAutomationObject(automation_proc,
                CNetService::eSingleServerService),
        m_NetScheduleAPI(ns_server)
    {
        m_Service = m_NetScheduleAPI.GetService();
    }
};

struct SNetScheduleServerAutomationObject :
        public SNetScheduleServiceAutomationObject
{
    SNetScheduleServerAutomationObject(CAutomationProc* automation_proc,
            CNetScheduleAPIExt ns_api, CNetServer::TInstance server) :
        SNetScheduleServiceAutomationObject(automation_proc,
                ns_api.GetServer(server)),
        m_NetServer(server)
    {
    }

    SNetScheduleServerAutomationObject(CAutomationProc* automation_proc,
            const string& service_name, const string& queue_name,
            const string& client_name);

    virtual const string& GetType() const;

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

    CNetServer m_NetServer;
};

END_NCBI_SCOPE

#endif // NS_AUTOMATION__HPP
