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

#include "grid_cli.hpp"

#include "json_over_uttp.hpp"

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

    string NextString();
    string NextString(const string& default_value);

    CJsonNode::TNumber NextNumber();
    CJsonNode::TNumber NextNumber(CJsonNode::TNumber default_value);

    bool NextBoolean();
    bool NextBoolean(bool default_value);

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
    return m_Position == m_Args.end() ? NULL : *m_Position++;
}

inline CJsonNode CArgArray::NextNode()
{
    CJsonNode next_node(NextNodeOrNull());
    if (next_node)
        return next_node;
    else
        Exception("insufficient number of arguments");
}

inline string CArgArray::NextString()
{
    CJsonNode next_node(NextNode());
    if (!next_node.IsString())
        Exception("invalid argument type (expected a string)");
    return next_node.GetString();
}

inline string CArgArray::NextString(const string& default_value)
{
    CJsonNode next_node(NextNodeOrNull());
    return next_node && next_node.IsString() ?
        next_node.GetString() : default_value;
}

inline CJsonNode::TNumber CArgArray::NextNumber()
{
    CJsonNode next_node(NextNode());
    if (!next_node.IsNumber())
        Exception("invalid argument type (expected a string)");
    return next_node.GetNumber();
}

inline CJsonNode::TNumber CArgArray::NextNumber(
        CJsonNode::TNumber default_value)
{
    CJsonNode next_node(NextNodeOrNull());
    return next_node && next_node.IsNumber() ?
        next_node.GetNumber() : default_value;
}

inline bool CArgArray::NextBoolean()
{
    CJsonNode next_node(NextNode());
    if (!next_node.IsBoolean())
        Exception("invalid argument type (expected a boolean)");
    return next_node.GetBoolean();
}

inline bool CArgArray::NextBoolean(bool default_value)
{
    CJsonNode next_node(NextNodeOrNull());
    return next_node && next_node.IsBoolean() ?
        next_node.GetBoolean() : default_value;
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
        NCBI_THROW_FMT(CAutomationException, eInvalidInput,
                m_Location << ": insufficient number of arguments");
    }
}

struct SAutomationObject : public CObject
{
    size_t m_Id;
    enum EObjectType {
        eNetCacheAPI,
        eNetScheduleAPI,
    } m_Type;
    SAutomationObject(size_t object_id, EObjectType object_type) :
        m_Id(object_id), m_Type(object_type)
    {
    }
    virtual CJsonNode Call(const string& method, CArgArray& arg_array) = 0;
};

struct SNetCacheAutomationObject : public SAutomationObject
{
    CNetCacheAPI m_NetCacheAPI;

    SNetCacheAutomationObject(size_t object_id,
            const string& service_name, const string& client_name) :
        SAutomationObject(object_id, eNetCacheAPI),
        m_NetCacheAPI(service_name, client_name)
    {
    }

    virtual CJsonNode Call(const string& method, CArgArray& arg_array);
};

CJsonNode SNetCacheAutomationObject::Call(const string& method,
        CArgArray& arg_array)
{
    CJsonNode reply(CJsonNode::NewArrayNode());
    reply.PushBoolean(true);
    reply.PushString(method);
    return reply;
}

struct SNetScheduleAutomationObject : public SAutomationObject
{
    CNetScheduleAPI m_NetScheduleAPI;

    SNetScheduleAutomationObject(size_t object_id,
            const string& service_name, const string& queue_name,
            const string& client_name) :
        SAutomationObject(object_id, eNetScheduleAPI),
        m_NetScheduleAPI(service_name, client_name, queue_name)
    {
    }

    virtual CJsonNode Call(const string& method, CArgArray& arg_array);
};

static string NormalizeStatKeyName(const CTempString& key)
{
    string key_norm(NStr::TruncateSpaces(key, NStr::eTrunc_Begin));
    NStr::ToLower(key_norm);
    NStr::ReplaceInPlace(key_norm, " ", "_");
    return key_norm;
}

static bool IsInteger(const CTempString& value)
{
    if (value.empty())
        return false;

    const char* digit = value.end();

    while (--digit > value.begin())
        if (!isdigit(*digit))
            return false;

    return isdigit(*digit) || (*digit == '-' && value.length() > 1);
}

static inline string UnquoteIfQuoted(const CTempString& str)
{
    switch (str[0]) {
    case '"':
    case '\'':
        return NStr::ParseQuoted(str);
    default:
        return str;
    }
}

#define CLIENT_PREFIX "CLIENT: "
#define CLIENT_PREFIX_LEN (sizeof(CLIENT_PREFIX) - 1)

static CJsonNode StatOutputToJsonArray(CNetServerMultilineCmdOutput output)
{
    CJsonNode clients(CJsonNode::NewArrayNode());
    CJsonNode client_info;
    CJsonNode array_value;

    string line;

    while (output.ReadLine(line)) {
        if (NStr::StartsWith(line, CLIENT_PREFIX)) {
            if (client_info)
                clients.PushNode(client_info);
            client_info = CJsonNode::NewObjectNode();
            client_info.SetString("client_node", UnquoteIfQuoted(
                    CTempString(line.data() + CLIENT_PREFIX_LEN,
                    line.length() - CLIENT_PREFIX_LEN)));
        } else if (client_info && NStr::StartsWith(line, "  ")) {
            if (NStr::StartsWith(line, "    ") && array_value) {
                array_value.PushString(NStr::TruncateSpaces(line,
                            NStr::eTrunc_Begin));
            } else {
                if (array_value)
                    array_value = NULL;
                CTempString key, value;
                NStr::SplitInTwo(line, ":", key, value);
                string key_norm(NormalizeStatKeyName(key));
                value = NStr::TruncateSpaces(value, NStr::eTrunc_Begin);
                if (value.empty())
                    client_info.SetNode(key_norm, array_value =
                            CJsonNode::NewArrayNode());
                else if (IsInteger(value))
                    client_info.SetNumber(key_norm, NStr::StringToInt8(value));
                else if (NStr::CompareNocase(value, "FALSE") == 0)
                    client_info.SetBoolean(key_norm, false);
                else if (NStr::CompareNocase(value, "TRUE") == 0)
                    client_info.SetBoolean(key_norm, true);
                else if (NStr::CompareNocase(value, "NONE") == 0)
                    client_info.SetNull(key_norm);
                else
                    client_info.SetString(key_norm, UnquoteIfQuoted(value));
            }
        }
    }
    if (client_info)
        clients.PushNode(client_info);
    return clients;
}

CJsonNode SNetScheduleAutomationObject::Call(const string& method,
        CArgArray& arg_array)
{
    CJsonNode reply(CJsonNode::NewArrayNode());
    reply.PushBoolean(true);

    if (method == "client_info") {
        string ns_cmd(arg_array.NextBoolean(false) ?
                "STAT CLIENTS VERBOSE" : "STAT CLIENTS");
        CJsonNode result(CJsonNode::NewObjectNode());
        for (CNetServiceIterator it =
                m_NetScheduleAPI.GetService().Iterate(); it; ++it) {
            result.SetNode((*it).GetServerAddress(),
                StatOutputToJsonArray((*it).ExecWithRetry(ns_cmd)));
        }
        reply.PushNode(result);
    } else {
        NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                "Unknown NetSchedule method '" << method << "'");
    }

    return reply;
}

typedef CRef<SAutomationObject> TAutomationObjectRef;

class CAutomationProc
{
public:
    const CJsonNode& GetGreeting();

    bool ProcessMessage(const CJsonNode& message);

    void PrepareError(const CTempString& error_message);

    const CJsonNode& GetOutputMessage() const;

private:
    void PrepareReply(const string& reply);

    vector<TAutomationObjectRef> m_Objects;

    CJsonNode m_RootNode;
};

const CJsonNode& CAutomationProc::GetGreeting()
{
    m_RootNode = CJsonNode::NewArrayNode();
    m_RootNode.PushString(PROGRAM_NAME);
    m_RootNode.PushString(PROGRAM_VERSION);
    return m_RootNode;
}

bool CAutomationProc::ProcessMessage(const CJsonNode& message)
{
    CArgArray arg_array(message.GetArray());

    string command(arg_array.NextString());

    arg_array.UpdateLocation(command);

    if (command == "exit")
        return false;

    if (command == "call") {
        size_t object_id = (size_t) arg_array.NextNumber();
        if (object_id >= m_Objects.size() || !m_Objects[object_id]) {
            NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                    "Object with id '" << object_id << "' does not exist");
        }
        string method(arg_array.NextString());
        arg_array.UpdateLocation(method);
        TAutomationObjectRef callee(m_Objects[object_id]);
        m_RootNode = callee->Call(method, arg_array);
    } else if (command == "new") {
        string class_name(arg_array.NextString());
        arg_array.UpdateLocation(class_name);
        TAutomationObjectRef new_object;
        size_t object_id = m_Objects.size();
        if (class_name == "NetCache") {
            string service_name(arg_array.NextString());
            string client_name(arg_array.NextString());
            new_object.Reset(new SNetCacheAutomationObject(object_id,
                    service_name, client_name));
        } else if (class_name == "NetSchedule") {
            string service_name(arg_array.NextString());
            string queue_name(arg_array.NextString());
            string client_name(arg_array.NextString());
            new_object.Reset(new SNetScheduleAutomationObject(object_id,
                    service_name, queue_name, client_name));
        } else {
            NCBI_THROW_FMT(CAutomationException, eInvalidInput,
                    "Unknown class '" << class_name << "'");
        }
        m_Objects.push_back(new_object);

        m_RootNode = CJsonNode::NewArrayNode();
        m_RootNode.PushBoolean(true);
        m_RootNode.PushNumber(object_id);
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

inline const CJsonNode& CAutomationProc::GetOutputMessage() const
{
    return m_RootNode;
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
    char write_buf[1024];

    CUTTPReader reader;
    CJsonOverUTTPReader suttp_reader;

    CUTTPWriter writer;
    writer.Reset(write_buf, sizeof(write_buf));
    CJsonOverUTTPWriter suttp_writer(pipe, writer);

    CAutomationProc proc;

    suttp_writer.SendMessage(proc.GetGreeting());

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
            catch (CNetSrvConnException& e) {
                proc.PrepareError(e.GetMsg());
            }
            catch (CAutomationException& e) {
                switch (e.GetErrCode()) {
                case CAutomationException::eInvalidInput:
                    //fprintf(stderr, "%s\n", e.GetMsg().c_str());
                    proc.PrepareError(e.GetMsg());
                    suttp_writer.SendMessage(proc.GetOutputMessage());
                    return 2;
                default:
                    proc.PrepareError(e.GetMsg());
                }
            }

            suttp_reader.Reset();

            suttp_writer.SendMessage(proc.GetOutputMessage());

            /* FALL THROUGH */

        case CJsonOverUTTPReader::eNextBuffer:
            break;

        default: // Parsing error
            return int(ret_code);
        }
    }

    return 0;
}
