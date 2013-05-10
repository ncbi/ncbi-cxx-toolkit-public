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
 * File Description: Declaration of the automation processor.
 *
 */

#ifndef AUTOMATION__HPP
#define AUTOMATION__HPP

#include "ns_cmd_impl.hpp"
#include "util.hpp"

BEGIN_NCBI_SCOPE

class CAutomationException : public CException
{
public:
    enum EErrCode {
        eInvalidInput,
        eCommandProcessingError,
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eInvalidInput:
            return "eInvalidInput";
        case eCommandProcessingError:
            return "eCommandProcessingError";
        default:
            return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CAutomationException, CException);
};

class CArgArray
{
public:
    CArgArray(const CJsonNode& args);

    CJsonNode NextNodeOrNull();
    CJsonNode NextNode();

    string GetString(const CJsonNode& node);
    string NextString() {return GetString(NextNode());}
    string NextString(const string& default_value);

    Int8 GetInteger(const CJsonNode& node);
    Int8 NextInteger() {return GetInteger(NextNode());}
    Int8 NextInteger(Int8 default_value);

    bool GetBoolean(const CJsonNode& node);
    bool NextBoolean() {return GetBoolean(NextNode());}
    bool NextBoolean(bool default_value);

    CJsonNode GetArray(const CJsonNode& node);
    CJsonNode NextArray() {return GetArray(NextNode());}

    void UpdateLocation(const string& location);
    void Exception(const char* what);

private:
    CJsonNode m_Args;
    CJsonIterator m_Position;
    string m_Location;
};

inline CArgArray::CArgArray(const CJsonNode& args) :
    m_Args(args),
    m_Position(args.Iterate())
{
}

inline CJsonNode CArgArray::NextNodeOrNull()
{
    if (m_Position) {
        if (!(*m_Position).IsNull()) {
            CJsonNode result(*m_Position);
            m_Position.Next();
            return result;
        }
        m_Position.Next();
    }
    return CJsonNode();
}

inline CJsonNode CArgArray::NextNode()
{
    CJsonNode next_node(NextNodeOrNull());
    if (!next_node)
        Exception("insufficient number of arguments");
    return next_node;
}

inline string CArgArray::GetString(const CJsonNode& node)
{
    if (!node.IsString())
        Exception("invalid argument type (expected a string)");
    return node.AsString();
}

inline string CArgArray::NextString(const string& default_value)
{
    CJsonNode next_node(NextNodeOrNull());
    return next_node ? GetString(next_node) : default_value;
}

inline Int8 CArgArray::GetInteger(const CJsonNode& node)
{
    if (!node.IsInteger())
        Exception("invalid argument type (expected an integer)");
    return node.AsInteger();
}

inline Int8 CArgArray::NextInteger(Int8 default_value)
{
    CJsonNode next_node(NextNodeOrNull());
    return next_node ? GetInteger(next_node) : default_value;
}

inline bool CArgArray::GetBoolean(const CJsonNode& node)
{
    if (!node.IsBoolean())
        Exception("invalid argument type (expected a boolean)");
    return node.AsBoolean();
}

inline bool CArgArray::NextBoolean(bool default_value)
{
    CJsonNode next_node(NextNodeOrNull());
    return next_node ? GetBoolean(next_node) : default_value;
}

inline CJsonNode CArgArray::GetArray(const CJsonNode& node)
{
    if (!node.IsArray())
        Exception("invalid argument type (expected an array)");
    return node;
}

inline void CArgArray::UpdateLocation(const string& location)
{
    if (m_Location.empty())
        m_Location = location;
    else {
        m_Location.push_back(' ');
        m_Location.append(location);
    }
}

typedef Int8 TObjectID;

class CAutomationProc;

class CAutomationObject : public CObject
{
public:
    CAutomationObject(CAutomationProc* automation_proc) :
        m_AutomationProc(automation_proc)
    {
    }

    void SetID(TObjectID object_id) {m_Id = object_id;}

    TObjectID GetID() const {return m_Id;}

    virtual const string& GetType() const = 0;

    virtual const void* GetImplPtr() const = 0;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply) = 0;

protected:
    CAutomationProc* m_AutomationProc;

    TObjectID m_Id;
};

struct SNetServiceAutomationObject : public CAutomationObject
{
    SNetServiceAutomationObject(CAutomationProc* automation_proc,
            CNetService::EServiceType actual_service_type) :
        CAutomationObject(automation_proc),
        m_ActualServiceType(actual_service_type)
    {
    }

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

    CNetService m_Service;
    CNetService::EServiceType m_ActualServiceType;
};

struct SNetCacheServiceAutomationObject : public SNetServiceAutomationObject
{
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
        SNetServiceAutomationObject(automation_proc,
                CNetService::eLoadBalancedService),
        m_NetCacheAPI(service_name, client_name)
    {
        m_Service = m_NetCacheAPI.GetService();
        m_NetCacheAPI.SetEventHandler(
                new CEventHandler(automation_proc, m_NetCacheAPI));
    }

    virtual const string& GetType() const;

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

    CNetCacheAPI m_NetCacheAPI;

protected:
    SNetCacheServiceAutomationObject(CAutomationProc* automation_proc,
            const CNetCacheAPI& nc_server) :
        SNetServiceAutomationObject(automation_proc,
                CNetService::eSingleServerService),
        m_NetCacheAPI(nc_server)
    {
        m_Service = m_NetCacheAPI.GetService();
    }
};

struct SNetCacheServerAutomationObject : public SNetCacheServiceAutomationObject
{
    SNetCacheServerAutomationObject(CAutomationProc* automation_proc,
            CNetCacheAPI nc_api, CNetServer::TInstance server) :
        SNetCacheServiceAutomationObject(automation_proc,
                nc_api.GetServer(server)),
        m_NetServer(server)
    {
    }

    SNetCacheServerAutomationObject(CAutomationProc* automation_proc,
        const string& service_name, const string& client_name);

    virtual const string& GetType() const;

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

    CNetServer m_NetServer;
};

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
        m_NetScheduleAPI(service_name, client_name, queue_name)
    {
        m_Service = m_NetScheduleAPI.GetService();
        m_NetScheduleAPI.SetEventHandler(
                new CEventHandler(automation_proc, m_NetScheduleAPI));
    }

    virtual const string& GetType() const;

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

    CNetScheduleAPI m_NetScheduleAPI;

protected:
    SNetScheduleServiceAutomationObject(CAutomationProc* automation_proc,
            const CNetScheduleAPI& ns_server) :
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
            CNetScheduleAPI ns_api, CNetServer::TInstance server) :
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

typedef CRef<CAutomationObject> TAutomationObjectRef;

class IMessageSender
{
public:
    virtual void SendMessage(const CJsonNode& message) = 0;

    virtual ~IMessageSender() {}
};

class CAutomationProc
{
public:
    CAutomationProc(IMessageSender* message_sender);

    TAutomationObjectRef CreateObject(const string& class_name,
            CArgArray& arg_array);

    CJsonNode ProcessMessage(const CJsonNode& message);

    void SendWarning(const string& warn_msg, TAutomationObjectRef source);

    void SendError(const CTempString& error_message);

    void AddObject(TAutomationObjectRef new_object, const void* impl_ptr);

    TAutomationObjectRef FindObjectByPtr(const void* impl_ptr) const;

    TAutomationObjectRef ReturnNetCacheServerObject(
            CNetCacheAPI::TInstance ns_api,
            CNetServer::TInstance server);

    TAutomationObjectRef ReturnNetScheduleServerObject(
            CNetScheduleAPI::TInstance ns_api,
            CNetServer::TInstance server);

private:
    TAutomationObjectRef& ObjectIdToRef(TObjectID object_id);

    IMessageSender* m_MessageSender;

    Int8 m_Pid;

    vector<TAutomationObjectRef> m_ObjectByIndex;
    typedef map<const void*, TAutomationObjectRef> TPtrToObjectRefMap;
    TPtrToObjectRefMap m_ObjectByPointer;

    CJsonNode m_OKNode;
    CJsonNode m_ErrNode;
    CJsonNode m_WarnNode;
};

inline void CAutomationProc::AddObject(TAutomationObjectRef new_object,
        const void* impl_ptr)
{
    new_object->SetID(m_Pid + m_ObjectByIndex.size());
    m_ObjectByIndex.push_back(new_object);
    m_ObjectByPointer[impl_ptr] = new_object;
}

END_NCBI_SCOPE

#endif // AUTOMATION__HPP
