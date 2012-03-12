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

private:
    void Exception(const char* what);

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
    return m_Position == m_Args.end() ? CJsonNode() : *m_Position++;
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

class CAutomationProc;

struct SAutomationObject : public CObject
{
    typedef Int8 TObjectID;

    TObjectID m_Id;

    enum EObjectType {
        eNetCacheAPI,
        eNetScheduleAPI,
        eNetScheduleServer,
    } m_Type;

    SAutomationObject(EObjectType object_type) :
        m_Type(object_type)
    {
    }

    void SetID(TObjectID object_id) {m_Id = object_id;}

    TObjectID GetID() const {return m_Id;}

    virtual CJsonNode Call(const string& method, CArgArray& arg_array,
            CAutomationProc* automation_proc) = 0;
};

struct SNetCacheAutomationObject : public SAutomationObject
{
    SNetCacheAutomationObject(const string& service_name,
            const string& client_name) :
        SAutomationObject(eNetCacheAPI),
        m_NetCacheAPI(service_name, client_name)
    {
    }

    virtual CJsonNode Call(const string& method, CArgArray& arg_array,
            CAutomationProc* automation_proc);

    CNetCacheAPI m_NetCacheAPI;
};

struct SNetScheduleBaseAutomationObject : public SAutomationObject
{
    SNetScheduleBaseAutomationObject(EObjectType object_type,
            CNetScheduleAPI ns_api) :
        SAutomationObject(object_type),
        m_NetScheduleAPI(ns_api)
    {
    }

    CNetScheduleAPI m_NetScheduleAPI;
};

struct SNetScheduleServerAutomationObject :
        public SNetScheduleBaseAutomationObject
{
    SNetScheduleServerAutomationObject(CNetScheduleAPI ns_api,
            CNetServer::TInstance server) :
        SNetScheduleBaseAutomationObject(eNetScheduleServer,
                ns_api.GetServer(server)),
        m_NetServer(server)
    {
    }

    virtual CJsonNode Call(const string& method, CArgArray& arg_array,
            CAutomationProc* automation_proc);

    CNetServer m_NetServer;
};

struct SNetScheduleServiceAutomationObject :
        public SNetScheduleBaseAutomationObject
{
    SNetScheduleServiceAutomationObject(const string& service_name,
            const string& queue_name, const string& client_name) :
        SNetScheduleBaseAutomationObject(eNetScheduleAPI,
                CNetScheduleAPI(service_name, client_name, queue_name))
    {
    }

    virtual CJsonNode Call(const string& method, CArgArray& arg_array,
            CAutomationProc* automation_proc);
};

typedef CRef<SAutomationObject> TAutomationObjectRef;

class CAutomationProc
{
public:
    CAutomationProc(CPipe& pipe, FILE* protocol_dump);

    bool ProcessMessage(const CJsonNode& message);

    void PrepareError(const CTempString& error_message);

    void SendMessage();

    void AddObject(TAutomationObjectRef new_object);

private:
    size_t ObjectIdToIndex(SAutomationObject::TObjectID object_id);

    void PrepareReply(const string& reply);

    char m_WriteBuf[1024];

    Int8 m_Pid;

    CPipe& m_Pipe;
    CUTTPWriter m_Writer;
    CJsonOverUTTPWriter m_JSONWriter;

    FILE* m_ProtocolDumpFile;
    string m_ProtocolDumpTimeFormat;
    string m_DumpInputHeaderFormat;
    string m_DumpOutputHeaderFormat;

    vector<TAutomationObjectRef> m_Objects;

    CJsonNode m_RootNode;

    CJsonNode m_OKNode;
    CJsonNode m_ErrNode;
    CJsonNode m_WarnNode;
};

inline void CAutomationProc::AddObject(TAutomationObjectRef new_object)
{
    new_object->SetID(m_Pid + m_Objects.size());
    m_Objects.push_back(new_object);
}

CJsonNode SNetCacheAutomationObject::Call(const string& method,
        CArgArray& /*arg_array*/, CAutomationProc* /*automation_proc*/)
{
    CJsonNode reply(CJsonNode::NewArrayNode());
    reply.PushBoolean(true);
    reply.PushString(method);
    return reply;
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

CJsonNode SNetScheduleServerAutomationObject::Call(const string& method,
        CArgArray& arg_array, CAutomationProc* /*automation_proc*/)
{
    CJsonNode reply(CJsonNode::NewArrayNode());
    reply.PushBoolean(true);

    if (method == "get_address") {
        switch (arg_array.NextNumber(0)) {
        case 0:
            reply.PushString(m_NetServer.GetServerAddress());
            break;
        case 1:
            reply.PushString(m_NetServer.GetHost());
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
        m_NetScheduleAPI.GetExecuter().ChangePreferredAffinities(
                affs_to_add, affs_to_del);
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
    } else {
        NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
            "Unknown NetScheduleServer method '" << method << "'");
    }

    return reply;
}

CJsonNode SNetScheduleServiceAutomationObject::Call(const string& method,
        CArgArray& arg_array, CAutomationProc* automation_proc)
{
    CJsonNode reply(CJsonNode::NewArrayNode());
    reply.PushBoolean(true);

    if (method == "set_node_session") {
        CJsonNode arg(arg_array.NextNode());
        if (!arg.IsNull())
            m_NetScheduleAPI.SetClientNode(arg_array.GetString(arg));
        arg = arg_array.NextNode();
        if (!arg.IsNull())
            m_NetScheduleAPI.SetClientSession(arg_array.GetString(arg));
    } else if (method == "exec") {
        string command(arg_array.NextString());
        reply.PushNode(ExecToJson(m_NetScheduleAPI.GetService(),
                command, arg_array.NextBoolean(false)));
    } else if (method == "get_servers") {
        CJsonNode object_ids(CJsonNode::NewArrayNode());
        for (CNetServiceIterator it = m_NetScheduleAPI.GetService().Iterate();
                it; ++it) {
            TAutomationObjectRef new_object(
                    new SNetScheduleServerAutomationObject(
                        m_NetScheduleAPI, *it));
            automation_proc->AddObject(new_object);
            object_ids.PushNumber(new_object->GetID());
        }
        reply.PushNode(object_ids);
    } else {
        NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                "Unknown NetSchedule method '" << method << "'");
    }

    return reply;
}

CAutomationProc::CAutomationProc(CPipe& pipe, FILE* protocol_dump) :
    m_Pipe(pipe),
    m_JSONWriter(pipe, m_Writer),
    m_ProtocolDumpFile(protocol_dump),
    m_OKNode(CJsonNode::NewStringNode("ok")),
    m_ErrNode(CJsonNode::NewStringNode("err")),
    m_WarnNode(CJsonNode::NewStringNode("warn"))
{
    m_Writer.Reset(m_WriteBuf, sizeof(m_WriteBuf));

    m_Pid = (Int8) CProcess::GetCurrentPid();

    m_RootNode = CJsonNode::NewArrayNode();
    m_RootNode.PushString(PROGRAM_NAME);
    m_RootNode.PushString(PROGRAM_VERSION);

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

    SendMessage();
}

bool CAutomationProc::ProcessMessage(const CJsonNode& message)
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
        return false;

    if (command == "call") {
        TAutomationObjectRef callee(m_Objects[ObjectIdToIndex(
                (SAutomationObject::TObjectID) arg_array.NextNumber())]);
        string method(arg_array.NextString());
        arg_array.UpdateLocation(method);
        m_RootNode = callee->Call(method, arg_array, this);
    } else if (command == "new") {
        string class_name(arg_array.NextString());
        arg_array.UpdateLocation(class_name);
        TAutomationObjectRef new_object;
        if (class_name == "NetCache") {
            string service_name(arg_array.NextString());
            string client_name(arg_array.NextString());
            new_object.Reset(new SNetCacheAutomationObject(
                    service_name, client_name));
        } else if (class_name == "NetSchedule") {
            string service_name(arg_array.NextString());
            string queue_name(arg_array.NextString());
            string client_name(arg_array.NextString());
            new_object.Reset(new SNetScheduleServiceAutomationObject(
                    service_name, queue_name, client_name));
        } else {
            NCBI_THROW_FMT(CAutomationException, eInvalidInput,
                    "Unknown class '" << class_name << "'");
        }
        AddObject(new_object);

        m_RootNode = CJsonNode::NewArrayNode();
        m_RootNode.PushBoolean(true);
        m_RootNode.PushNumber(new_object->GetID());
    } else if (command == "del") {
        m_Objects[ObjectIdToIndex(
                (SAutomationObject::TObjectID) arg_array.NextNumber())] = NULL;
        m_RootNode = CJsonNode::NewArrayNode();
        m_RootNode.PushBoolean(true);
    } else {
        NCBI_THROW_FMT(CAutomationException, eInvalidInput,
                "Unknown command '" << command << "'");
    }

    return true;
}

void CAutomationProc::PrepareError(const CTempString& error_message)
{
    m_RootNode = CJsonNode::NewArrayNode();
    m_RootNode.PushBoolean(false);
    m_RootNode.PushString(error_message);
}

void CAutomationProc::SendMessage()
{
    if (m_ProtocolDumpFile != NULL) {
        fprintf(m_ProtocolDumpFile, m_DumpOutputHeaderFormat.c_str(),
                GetFastLocalTime().AsString(m_ProtocolDumpTimeFormat).c_str());
        PrintJSON(m_ProtocolDumpFile, m_RootNode);
    }

    m_JSONWriter.SendMessage(m_RootNode);
}

size_t CAutomationProc::ObjectIdToIndex(SAutomationObject::TObjectID object_id)
{
    size_t index = (size_t) (object_id - m_Pid);

    if (index >= m_Objects.size() || !m_Objects[index]) {
        NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                "Object with id '" << object_id << "' does not exist");
    }
    return index;
}

void CAutomationProc::PrepareReply(const string& reply)
{
    m_RootNode = CJsonNode::NewArrayNode();
    m_RootNode.PushBoolean(true);
    m_RootNode.PushString(reply);
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
                if (!proc.ProcessMessage(suttp_reader.GetMessage()))
                    return 0;
            }
            catch (CConfigException& e) {
                proc.PrepareError(e.GetMsg());
            }
            catch (CNetSrvConnException& e) {
                proc.PrepareError(e.GetMsg());
            }
            catch (CNetServiceException& e) {
                proc.PrepareError(e.GetMsg());
            }
            catch (CAutomationException& e) {
                switch (e.GetErrCode()) {
                case CAutomationException::eInvalidInput:
                    proc.PrepareError(e.GetMsg());
                    proc.SendMessage();
                    return 2;
                default:
                    proc.PrepareError(e.GetMsg());
                }
            }

            suttp_reader.Reset();

            proc.SendMessage();

            /* FALL THROUGH */

        case CJsonOverUTTPReader::eNextBuffer:
            break;

        default: // Parsing error
            return int(ret_code);
        }
    }

    return 0;
}
