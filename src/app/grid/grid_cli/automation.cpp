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
    virtual CJsonNode Call(const string& method,
            const CJsonNode::TArray& cmd_and_args) = 0;
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

    virtual CJsonNode Call(const string& method,
            const CJsonNode::TArray& cmd_and_args);
};

CJsonNode SNetCacheAutomationObject::Call(const string& method,
        const CJsonNode::TArray& cmd_and_args)
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

    virtual CJsonNode Call(const string& method,
            const CJsonNode::TArray& cmd_and_args);
};

CJsonNode SNetScheduleAutomationObject::Call(const string& method,
        const CJsonNode::TArray& cmd_and_args)
{
    CJsonNode reply(CJsonNode::NewArrayNode());
    reply.PushBoolean(true);

    if (method == "client_info") {
        CJsonNode result(CJsonNode::NewObjectNode());
        for (CNetServiceIterator it =
                m_NetScheduleAPI.GetService().Iterate(); it; ++it) {
            CJsonNode clients(CJsonNode::NewArrayNode());
            CJsonNode client_info;
            CNetServerMultilineCmdOutput output(
                    (*it).ExecWithRetry("STAT CLIENTS"));

            string line;

            while (output.ReadLine(line)) {
                if (NStr::StartsWith(line, "CLIENT: ")) {
                    if (client_info)
                        clients.PushNode(client_info);
                    client_info = CJsonNode::NewObjectNode();
                    client_info.SetString("client_node",
                            string(line, sizeof("CLIENT: ") - 1));
                } else if (client_info) {
                    CTempString key, value;
                    NStr::SplitInTwo(line, ":", key, value);
                    string key_str(NStr::TruncateSpaces(key,
                                NStr::eTrunc_Begin));
                    NStr::ToLower(key_str);
                    NStr::ReplaceInPlace(key_str, " ", "_");
                    client_info.SetString(key_str,
                            NStr::TruncateSpaces(value, NStr::eTrunc_Begin));
                }
            }
            if (client_info)
                clients.PushNode(client_info);
            result.SetNode((*it).GetServerAddress(), clients);
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
    const CJsonNode::TArray& cmd_and_args(message.GetArray());

    if (cmd_and_args.empty()) {
        NCBI_THROW(CAutomationException, eInvalidInput,
                "Empty input is not allowed");
    }

    CJsonNode command_node(cmd_and_args[0]);

    if (!command_node.IsString()) {
        NCBI_THROW(CAutomationException, eInvalidInput,
                "Invalid message format: must start with a command");
    }

    string command(command_node.GetString());

    if (command == "exit")
        return false;

    if (command == "call") {
        if (cmd_and_args.size() < 3) {
            NCBI_THROW(CAutomationException, eInvalidInput,
                    "Insufficient number of arguments to 'call'");
        }
        if (!cmd_and_args[1].IsNumber() || !cmd_and_args[2].IsString()) {
            NCBI_THROW(CAutomationException, eInvalidInput,
                    "Invalid type of argument in the 'call' command");
        }
        size_t object_id = (size_t) cmd_and_args[1].GetNumber();
        if (object_id >= m_Objects.size() || !m_Objects[object_id]) {
            NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                    "Object with id '" << object_id << "' does not exist");
        }
        string method(cmd_and_args[2].GetString());
        TAutomationObjectRef callee(m_Objects[object_id]);
        m_RootNode = callee->Call(method, cmd_and_args);
    } else if (command == "new") {
        CJsonNode::TArray::const_iterator it = cmd_and_args.begin();
        while (++it != cmd_and_args.end())
            if (!(*it).IsString()) {
                NCBI_THROW(CAutomationException, eInvalidInput,
                        "Invalid type of argument in the 'new' command");
            }
        if (cmd_and_args.size() < 2) {
            NCBI_THROW(CAutomationException, eInvalidInput,
                    "Missing class name in the 'new' command");
        }
        string class_name(cmd_and_args[1].GetString());
        TAutomationObjectRef new_object;
        size_t object_id = m_Objects.size();
        if (class_name == "NetCache") {
            if (cmd_and_args.size() != 4) {
                NCBI_THROW(CAutomationException, eInvalidInput,
                        "Incorrect number of parameters passed to the "
                        "NetCache constructor");
            }
            new_object.Reset(new SNetCacheAutomationObject(object_id,
                    cmd_and_args[2].GetString(),
                    cmd_and_args[3].GetString()));
        } else if (class_name == "NetSchedule") {
            if (cmd_and_args.size() != 5) {
                NCBI_THROW(CAutomationException, eInvalidInput,
                        "Incorrect number of parameters passed to the "
                        "NetSchedule constructor");
            }
            new_object.Reset(new SNetScheduleAutomationObject(object_id,
                    cmd_and_args[2].GetString(),
                    cmd_and_args[3].GetString(),
                    cmd_and_args[4].GetString()));
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
