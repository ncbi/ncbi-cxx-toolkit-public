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

#include <ncbi_pch.hpp>

#include "ns_cmd_impl.hpp"
#include "util.hpp"

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

USING_NCBI_SCOPE;

#define PROTOCOL_VERSION 1

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
    CArgArray(const CJsonNode::TArray& args);

    CJsonNode NextNodeOrNull();
    CJsonNode NextNode();

    string GetString(const CJsonNode& node);
    string NextString() {return GetString(NextNode());}
    string NextString(const string& default_value);

    CJsonNode::TNumber GetNumber(const CJsonNode& node);
    CJsonNode::TNumber NextNumber() {return GetNumber(NextNode());}
    CJsonNode::TNumber NextNumber(CJsonNode::TNumber default_value);

    bool GetBoolean(const CJsonNode& node);
    bool NextBoolean() {return GetBoolean(NextNode());}
    bool NextBoolean(bool default_value);

    const CJsonNode::TArray& GetArray(const CJsonNode& node);
    const CJsonNode::TArray& NextArray() {return GetArray(NextNode());}

    void UpdateLocation(const string& location);
    void Exception(const char* what);

private:
    const CJsonNode::TArray& m_Args;
    CJsonNode::TArray::const_iterator m_Position;
    string m_Location;
};

inline CArgArray::CArgArray(const CJsonNode::TArray& args) : m_Args(args)
{
    m_Position = args.begin();
}

inline CJsonNode CArgArray::NextNodeOrNull()
{
    if (m_Position != m_Args.end()) {
        if (!m_Position->IsNull())
            return *m_Position++;
        ++m_Position;
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
    return node.GetString();
}

inline string CArgArray::NextString(const string& default_value)
{
    CJsonNode next_node(NextNodeOrNull());
    return next_node ? GetString(next_node) : default_value;
}

inline CJsonNode::TNumber CArgArray::GetNumber(const CJsonNode& node)
{
    if (!node.IsNumber())
        Exception("invalid argument type (expected a number)");
    return node.GetNumber();
}

inline CJsonNode::TNumber CArgArray::NextNumber(
        CJsonNode::TNumber default_value)
{
    CJsonNode next_node(NextNodeOrNull());
    return next_node ? GetNumber(next_node) : default_value;
}

inline bool CArgArray::GetBoolean(const CJsonNode& node)
{
    if (!node.IsBoolean())
        Exception("invalid argument type (expected a boolean)");
    return node.GetBoolean();
}

inline bool CArgArray::NextBoolean(bool default_value)
{
    CJsonNode next_node(NextNodeOrNull());
    return next_node ? GetBoolean(next_node) : default_value;
}

inline const CJsonNode::TArray& CArgArray::GetArray(const CJsonNode& node)
{
    if (!node.IsArray())
        Exception("invalid argument type (expected an array)");
    return node.GetArray();
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

void CArgArray::Exception(const char* what)
{
    if (m_Location.empty()) {
        NCBI_THROW(CAutomationException, eInvalidInput, what);
    } else {
        NCBI_THROW_FMT(CAutomationException, eInvalidInput, m_Location <<
                ": " << what);
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

struct SNetCacheAutomationObject : public CAutomationObject
{
    SNetCacheAutomationObject(CAutomationProc* automation_proc,
            const string& service_name, const string& client_name) :
        CAutomationObject(automation_proc),
        m_NetCacheAPI(service_name, client_name)
    {
    }

    virtual const string& GetType() const;

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

    CNetCacheAPI m_NetCacheAPI;
};

const string& SNetCacheAutomationObject::GetType() const
{
    static const string object_type("ncsvc");

    return object_type;
}

const void* SNetCacheAutomationObject::GetImplPtr() const
{
    return m_NetCacheAPI;
}

struct SNetScheduleServiceAutomationObject : public CAutomationObject
{
    class CEventHandler : public CNetScheduleAPI::IEventHandler
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
        CAutomationObject(automation_proc),
        m_NetScheduleAPI(service_name, client_name, queue_name)
    {
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
            CNetScheduleAPI ns_api) :
        CAutomationObject(automation_proc),
        m_NetScheduleAPI(ns_api)
    {
    }
};

const string& SNetScheduleServiceAutomationObject::GetType() const
{
    static const string object_type("nssvc");

    return object_type;
}

const void* SNetScheduleServiceAutomationObject::GetImplPtr() const
{
    return m_NetScheduleAPI;
}

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

    virtual const string& GetType() const;

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

    CNetServer m_NetServer;
};

const string& SNetScheduleServerAutomationObject::GetType() const
{
    static const string object_type("nssrv");

    return object_type;
}

const void* SNetScheduleServerAutomationObject::GetImplPtr() const
{
    return m_NetServer;
}

typedef CRef<CAutomationObject> TAutomationObjectRef;

class CAutomationProc
{
public:
    CAutomationProc(CPipe& pipe, FILE* protocol_dump);

    CJsonNode ProcessMessage(const CJsonNode& message);

    void SendMessage(const CJsonNode& message);

    void SendWarning(const string& warn_msg, TAutomationObjectRef source);

    void SendError(const CTempString& error_message);

    void AddObject(TAutomationObjectRef new_object, const void* impl_ptr);

    TAutomationObjectRef FindObjectByPtr(const void* impl_ptr) const;

    TAutomationObjectRef ReturnNetScheduleServerObject(CNetScheduleAPI::TInstance ns_api,
            CNetServer::TInstance server);

private:
    TAutomationObjectRef& ObjectIdToRef(TObjectID object_id);

    char m_WriteBuf[1024];

    Int8 m_Pid;

    CPipe& m_Pipe;
    CUTTPWriter m_Writer;
    CJsonOverUTTPWriter m_JSONWriter;

    FILE* m_ProtocolDumpFile;
    string m_ProtocolDumpTimeFormat;
    string m_DumpInputHeaderFormat;
    string m_DumpOutputHeaderFormat;

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

TAutomationObjectRef CAutomationProc::FindObjectByPtr(const void* impl_ptr) const
{
    TPtrToObjectRefMap::const_iterator it = m_ObjectByPointer.find(impl_ptr);
    return it != m_ObjectByPointer.end() ? it->second : TAutomationObjectRef();
}

TAutomationObjectRef CAutomationProc::ReturnNetScheduleServerObject(
        CNetScheduleAPI::TInstance ns_api,
        CNetServer::TInstance server)
{
    TAutomationObjectRef object(FindObjectByPtr(server));
    if (!object) {
        object = new SNetScheduleServerAutomationObject(this, ns_api, server);
        AddObject(object, server);
    }
    return object;
}

bool SNetCacheAutomationObject::Call(const string& method,
        CArgArray& /*arg_array*/, CJsonNode& reply)
{
    return false;
}

static void ExtractVectorOfStrings(CArgArray& arg_array,
        vector<string>& result)
{
    CJsonNode arg(arg_array.NextNode());
    if (!arg.IsNull())
        ITERATE(CJsonNode::TArray, it, arg.GetArray()) {
            result.push_back(arg_array.GetString(*it));
        }
}

bool SNetScheduleServerAutomationObject::Call(const string& method,
        CArgArray& arg_array, CJsonNode& reply)
{
    if (method == "get_address") {
        switch (arg_array.NextNumber(0)) {
        case 0:
            reply.PushString(m_NetServer.GetServerAddress());
            break;
        case 1:
            reply.PushString(g_NetService_gethostnamebyaddr(
                    m_NetServer.GetHost()));
            break;
        case 2:
            reply.PushNumber(m_NetServer.GetPort());
        }
    } else if (method == "server_info") {
        CJsonNode server_info_node(CJsonNode::NewObjectNode());

        CNetServerInfo server_info(m_NetServer.GetServerInfo());

        string attr_name, attr_value;

        while (server_info.GetNextAttribute(attr_name, attr_value))
            server_info_node.SetString(attr_name, attr_value);

        reply.PushNode(server_info_node);
    } else if (method == "server_status")
        reply.PushNode(LegacyStatToJson(m_NetServer,
                arg_array.NextBoolean(false)));
    else if (method == "job_group_info")
        reply.PushNode(GenericStatToJson(m_NetServer,
                eNetScheduleStatJobGroups, arg_array.NextBoolean(false)));
    else if (method == "client_info")
        reply.PushNode(GenericStatToJson(m_NetServer,
                eNetScheduleStatClients, arg_array.NextBoolean(false)));
    else if (method == "notification_info")
        reply.PushNode(GenericStatToJson(m_NetServer,
                eNetScheduleStatNotifications, arg_array.NextBoolean(false)));
    else if (method == "affinity_info")
        reply.PushNode(GenericStatToJson(m_NetServer,
                eNetScheduleStatAffinities, arg_array.NextBoolean(false)));
    else if (method == "change_preferred_affinities") {
        vector<string> affs_to_add;
        ExtractVectorOfStrings(arg_array, affs_to_add);
        vector<string> affs_to_del;
        ExtractVectorOfStrings(arg_array, affs_to_del);
        m_NetScheduleAPI.GetExecutor().ChangePreferredAffinities(
                &affs_to_add, &affs_to_del);
    } else if (method == "exec") {
        string command(arg_array.NextString());

        if (!arg_array.NextBoolean(false))
            reply.PushString(m_NetServer.ExecWithRetry(command).response);
        else {
            CNetServerMultilineCmdOutput output(
                m_NetServer.ExecWithRetry(command));

            CJsonNode lines(CJsonNode::NewArrayNode());
            string line;

            while (output.ReadLine(line))
                lines.PushString(line);

            reply.PushNode(lines);
        }
    } else
        return SNetScheduleServiceAutomationObject::Call(
                method, arg_array, reply);

    return true;
}

void SNetScheduleServiceAutomationObject::CEventHandler::OnWarning(
        const string& warn_msg, CNetServer server)
{
    m_AutomationProc->SendWarning(warn_msg, m_AutomationProc->
            ReturnNetScheduleServerObject(m_NetScheduleAPI, server));
}

bool SNetScheduleServiceAutomationObject::Call(const string& method,
        CArgArray& arg_array, CJsonNode& reply)
{
    if (method == "set_node_session") {
        CJsonNode arg(arg_array.NextNode());
        if (!arg.IsNull())
            m_NetScheduleAPI.SetClientNode(arg_array.GetString(arg));
        arg = arg_array.NextNode();
        if (!arg.IsNull())
            m_NetScheduleAPI.SetClientSession(arg_array.GetString(arg));
    } else if (method == "job_info") {
        CJobInfoToJSON job_info_to_json;
        string job_key(arg_array.NextString());
        ProcessJobInfo(m_NetScheduleAPI, job_key,
            &job_info_to_json, arg_array.NextBoolean(true));
        reply.PushNode(job_info_to_json.GetRootNode());
    } else if (method == "jobs_by_status") {
        CNetScheduleAdmin::TStatusMap status_map;
        string affinity(arg_array.NextString(kEmptyStr));
        string job_group(arg_array.NextString(kEmptyStr));
        m_NetScheduleAPI.GetAdmin().StatusSnapshot(status_map,
                affinity, job_group);
        CJsonNode jobs_by_status(CJsonNode::NewObjectNode());
        ITERATE(CNetScheduleAdmin::TStatusMap, it, status_map) {
            jobs_by_status.SetNumber(it->first, it->second);
        }
        reply.PushNode(jobs_by_status);
    } else if (method == "exec") {
        string command(arg_array.NextString());
        reply.PushNode(ExecToJson(m_NetScheduleAPI.GetService(),
                command, arg_array.NextBoolean(false)));
    } else if (method == "get_servers") {
        CJsonNode object_ids(CJsonNode::NewArrayNode());
        for (CNetServiceIterator it = m_NetScheduleAPI.GetService().Iterate();
                it; ++it)
            object_ids.PushNumber(m_AutomationProc->
                    ReturnNetScheduleServerObject(m_NetScheduleAPI, *it)->
                    GetID());
        reply.PushNode(object_ids);
    } else
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        if (method == "allow_xsite_connections")
            m_NetScheduleAPI.GetService().AllowXSiteConnections();
        else
#endif
            return false;

    return true;
}

CAutomationProc::CAutomationProc(CPipe& pipe, FILE* protocol_dump) :
    m_Pipe(pipe),
    m_JSONWriter(pipe, m_Writer),
    m_ProtocolDumpFile(protocol_dump),
    m_OKNode(CJsonNode::NewBooleanNode(true)),
    m_ErrNode(CJsonNode::NewBooleanNode(false)),
    m_WarnNode(CJsonNode::NewStringNode("WARNING"))
{
    m_Writer.Reset(m_WriteBuf, sizeof(m_WriteBuf));

    m_Pid = (Int8) CProcess::GetCurrentPid();

    CJsonNode greeting(CJsonNode::NewArrayNode());
    greeting.PushString(GRID_APP_NAME);
    greeting.PushNumber(PROTOCOL_VERSION);

    if (protocol_dump != NULL) {
        string pid_str(NStr::NumericToString(m_Pid));

        m_DumpInputHeaderFormat.assign(1, '\n');
        m_DumpInputHeaderFormat.append(71 + pid_str.length(), '=');
        m_DumpInputHeaderFormat.append("\n----- IN ------ %s (pid: ");
        m_DumpInputHeaderFormat.append(pid_str);
        m_DumpInputHeaderFormat.append(") -----------------------\n");

        m_DumpOutputHeaderFormat.assign("----- OUT ----- %s (pid: ");
        m_DumpOutputHeaderFormat.append(pid_str);
        m_DumpOutputHeaderFormat.append(") -----------------------\n");

        m_ProtocolDumpTimeFormat = "Y/M/D h:m:s.l";
    }

    SendMessage(greeting);
}

CJsonNode CAutomationProc::ProcessMessage(const CJsonNode& message)
{
    if (m_ProtocolDumpFile != NULL) {
        fprintf(m_ProtocolDumpFile, m_DumpInputHeaderFormat.c_str(),
                GetFastLocalTime().AsString(m_ProtocolDumpTimeFormat).c_str());
        PrintJSON(m_ProtocolDumpFile, message);
    }

    CArgArray arg_array(message.GetArray());

    string command(arg_array.NextString());

    arg_array.UpdateLocation(command);

    if (command == "exit")
        return NULL;

    CJsonNode reply(CJsonNode::NewArrayNode());
    reply.PushNode(m_OKNode);

    if (command == "call") {
        TAutomationObjectRef object_ref(ObjectIdToRef(
                (TObjectID) arg_array.NextNumber()));
        string method(arg_array.NextString());
        arg_array.UpdateLocation(method);
        if (!object_ref->Call(method, arg_array, reply)) {
            NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                    "Unknown " << object_ref->GetType() <<
                            " method '" << method << "'");
        }
    } else if (command == "new") {
        string class_name(arg_array.NextString());
        arg_array.UpdateLocation(class_name);
        TAutomationObjectRef new_object;
        const void* impl_ptr;
        if (class_name == "ncsvc") {
            string service_name(arg_array.NextString());
            string client_name(arg_array.NextString());
            SNetCacheAutomationObject* new_object_ptr =
                    new SNetCacheAutomationObject(this,
                            service_name, client_name);
            new_object.Reset(new_object_ptr);
            impl_ptr = new_object_ptr->m_NetCacheAPI;
        } else if (class_name == "nssvc") {
            string service_name(arg_array.NextString());
            string queue_name(arg_array.NextString());
            string client_name(arg_array.NextString());
            SNetScheduleServiceAutomationObject* new_object_ptr =
                new SNetScheduleServiceAutomationObject(this,
                        service_name, queue_name, client_name);
            new_object.Reset(new_object_ptr);
            impl_ptr = new_object_ptr->m_NetScheduleAPI;
        } else {
            NCBI_THROW_FMT(CAutomationException, eInvalidInput,
                    "Unknown class '" << class_name << "'");
        }
        AddObject(new_object, impl_ptr);

        reply.PushNumber(new_object->GetID());
    } else if (command == "del") {
        TAutomationObjectRef& object(ObjectIdToRef(
                (TObjectID) arg_array.NextNumber()));
        m_ObjectByPointer.erase(object->GetImplPtr());
        object = NULL;
    } else {
        arg_array.Exception("unknown command");
    }

    return reply;
}

void CAutomationProc::SendMessage(const CJsonNode& message)
{
    if (m_ProtocolDumpFile != NULL) {
        fprintf(m_ProtocolDumpFile, m_DumpOutputHeaderFormat.c_str(),
                GetFastLocalTime().AsString(m_ProtocolDumpTimeFormat).c_str());
        PrintJSON(m_ProtocolDumpFile, message);
    }

    m_JSONWriter.SendMessage(message);
}

void CAutomationProc::SendWarning(const string& warn_msg,
        TAutomationObjectRef source)
{
    CJsonNode warning(CJsonNode::NewArrayNode());
    warning.PushNode(m_WarnNode);
    warning.PushString(warn_msg);
    warning.PushString(source->GetType());
    warning.PushNumber(source->GetID());
    SendMessage(warning);
}

void CAutomationProc::SendError(const CTempString& error_message)
{
    CJsonNode error(CJsonNode::NewArrayNode());
    error.PushNode(m_ErrNode);
    error.PushString(error_message);
    SendMessage(error);
}

TAutomationObjectRef& CAutomationProc::ObjectIdToRef(TObjectID object_id)
{
    size_t index = (size_t) (object_id - m_Pid);

    if (index >= m_ObjectByIndex.size() || !m_ObjectByIndex[index]) {
        NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                "Object with id '" << object_id << "' does not exist");
    }
    return m_ObjectByIndex[index];
}

int CGridCommandLineInterfaceApp::Cmd_Automate()
{
    CPipe pipe;

    pipe.OpenSelf();

#ifdef WIN32
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
#endif

    size_t bytes_read;

    char read_buf[1024];

    CUTTPReader reader;
    CJsonOverUTTPReader suttp_reader;

    CAutomationProc proc(pipe, m_Opts.protocol_dump);

    while (pipe.Read(read_buf, sizeof(read_buf), &bytes_read) == eIO_Success) {
        reader.SetNewBuffer(read_buf, bytes_read);

        CJsonOverUTTPReader::EParsingEvent ret_code =
            suttp_reader.ProcessParsingEvents(reader);

        switch (ret_code) {
        case CJsonOverUTTPReader::eEndOfMessage:
            try {
                CJsonNode reply(proc.ProcessMessage(suttp_reader.GetMessage()));

                if (!reply)
                    return 0;

                proc.SendMessage(reply);
            }
            catch (CConfigException& e) {
                proc.SendError(e.GetMsg());
            }
            catch (CNetSrvConnException& e) {
                proc.SendError(e.GetMsg());
            }
            catch (CNetServiceException& e) {
                proc.SendError(e.GetMsg());
            }
            catch (CAutomationException& e) {
                switch (e.GetErrCode()) {
                case CAutomationException::eInvalidInput:
                    proc.SendError(e.GetMsg());
                    return 2;
                default:
                    proc.SendError(e.GetMsg());
                }
            }

            suttp_reader.Reset();

            /* FALL THROUGH */

        case CJsonOverUTTPReader::eNextBuffer:
            break;

        default: // Parsing error
            return int(ret_code);
        }
    }

    return 0;
}
