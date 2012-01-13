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

#include "suttp.hpp"

//#include <string.h>
//#include <ctype.h>

#ifndef WIN32
//#include <unistd.h>
#else
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
    //CNetCacheAPI GetNetCacheAPI() const;
    //CNetScheduleAPI GetNetScheduleAPI() const;
    SAutomationObject(size_t object_id, EObjectType object_type) :
        m_Id(object_id), m_Type(object_type)
    {
    }
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

    TSUTTPNodeRef Call(const string& method,
            const TSUTTPNodeArray& cmd_and_args);
};

TSUTTPNodeRef SNetCacheAutomationObject::Call(const string& method,
        const TSUTTPNodeArray& cmd_and_args)
{
    TSUTTPNodeRef reply(CSUTTPNode::NewArrayNode());
    reply->Push("ok");
    reply->Push(method);
    return reply;
}

/*inline CNetCacheAPI SAutomationObject::GetNetCacheAPI() const
{
    return static_cast<const SNetCacheAutomationObject*>(this)->m_NetCacheAPI;
}*/

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

    TSUTTPNodeRef Call(const string& method,
            const TSUTTPNodeArray& cmd_and_args);
};

/*inline CNetScheduleAPI SAutomationObject::GetNetScheduleAPI() const
{
    return static_cast<
        const SNetScheduleAutomationObject*>(this)->m_NetScheduleAPI;
}*/

TSUTTPNodeRef SNetScheduleAutomationObject::Call(const string& method,
        const TSUTTPNodeArray& cmd_and_args)
{
    TSUTTPNodeRef reply(CSUTTPNode::NewArrayNode());
    reply->Push("ok");

    if (method == "client_info") {
        TSUTTPNodeRef result(CSUTTPNode::NewHashNode());
        for (CNetServiceIterator it =
                m_NetScheduleAPI.GetService().Iterate(); it; ++it) {
            TSUTTPNodeRef clients(CSUTTPNode::NewArrayNode());
            TSUTTPNodeRef client_info;
            CNetServerMultilineCmdOutput output(
                    (*it).ExecWithRetry("STAT CLIENTS"));

            string line;

            while (output.ReadLine(line)) {
                if (NStr::StartsWith(line, "CLIENT ")) {
                    if (!client_info.IsNull())
                        clients->Push(client_info);
                    client_info.Reset(CSUTTPNode::NewHashNode());
                    client_info->Set("client_node",
                            string(line, sizeof("CLIENT ") - 1));
                } else if (!client_info.IsNull()) {
                    CTempString key, value;
                    NStr::SplitInTwo(line, ":", key, value);
                    string key_str(NStr::TruncateSpaces(key,
                                NStr::eTrunc_Begin));
                    NStr::ToLower(key_str);
                    NStr::ReplaceInPlace(key_str, " ", "_");
                    client_info->Set(key_str,
                            NStr::TruncateSpaces(value, NStr::eTrunc_Begin));
                }
            }
            if (!client_info.IsNull())
                clients->Push(client_info);
            result->Set((*it).GetServerAddress(), clients);
        }
        reply->Push(result);
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
    //CAutomationProc() {}

    const CSUTTPNode* GetGreeting();

    bool ProcessMessage(const CSUTTPNode* message);

    void PrepareError(const CTempString& error_message);

    const CSUTTPNode* GetOutputMessage() const;

private:
    void PrepareReply(const string& reply);

    vector<TAutomationObjectRef> m_Objects;

    TSUTTPNodeRef m_RootNode;
};

const CSUTTPNode* CAutomationProc::GetGreeting()
{
    m_RootNode = CSUTTPNode::NewArrayNode();
    m_RootNode->Push(PROGRAM_NAME);
    m_RootNode->Push(PROGRAM_VERSION);
    return m_RootNode.GetPointerOrNull();
}

bool CAutomationProc::ProcessMessage(const CSUTTPNode* message)
{
    const TSUTTPNodeArray& cmd_and_args(message->GetArray());

    if (cmd_and_args.empty()) {
        NCBI_THROW(CAutomationException, eInvalidInput,
                "Empty input is not allowed");
    }

    TSUTTPNodeRef command_node(cmd_and_args[0]);

    if (!command_node->IsScalar()) {
        NCBI_THROW(CAutomationException, eInvalidInput,
                "Invalid message format: must start with a command");
    }

    string command(command_node->GetScalar());

    if (command == "exit")
        return false;

    if (command == "call") {
        if (cmd_and_args.size() < 3) {
            NCBI_THROW(CAutomationException, eInvalidInput,
                    "Insufficient number of arguments to 'call'");
        }
        if (!cmd_and_args[1]->IsScalar() || !cmd_and_args[2]->IsScalar()) {
            NCBI_THROW(CAutomationException, eInvalidInput,
                    "Invalid type of argument in the 'call' command");
        }
        size_t object_id = NStr::StringToNumeric(cmd_and_args[1]->GetScalar());
        if (object_id >= m_Objects.size() || m_Objects[object_id].IsNull()) {
            NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                    "Object with id '" << object_id << "' does not exist");
        }
        string method(cmd_and_args[2]->GetScalar());
        TAutomationObjectRef callee(m_Objects[object_id]);
        if (callee->m_Type == SAutomationObject::eNetCacheAPI)
            m_RootNode = static_cast<SNetCacheAutomationObject*>(
                    callee.GetPointerOrNull())->Call(method, cmd_and_args);
        else // eNetScheduleAPI
            m_RootNode = static_cast<SNetScheduleAutomationObject*>(
                    callee.GetPointerOrNull())->Call(method, cmd_and_args);
    } else if (command == "new") {
        TSUTTPNodeArray::const_iterator it = cmd_and_args.begin();
        while (++it != cmd_and_args.end())
            if (!(*it)->IsScalar()) {
                NCBI_THROW(CAutomationException, eInvalidInput,
                        "Invalid type of argument in the 'new' command");
            }
        if (cmd_and_args.size() < 2) {
            NCBI_THROW(CAutomationException, eInvalidInput,
                    "Missing class name in the 'new' command");
        }
        string class_name(cmd_and_args[1]->GetScalar());
        TAutomationObjectRef new_object;
        size_t object_id = m_Objects.size();
        if (class_name == "NetCache") {
            if (cmd_and_args.size() != 4) {
                NCBI_THROW(CAutomationException, eInvalidInput,
                        "Incorrect number of parameters passed to the "
                        "NetCache constructor");
            }
            new_object.Reset(new SNetCacheAutomationObject(object_id,
                    cmd_and_args[2]->GetScalar(),
                    cmd_and_args[3]->GetScalar()));
        } else if (class_name == "NetSchedule") {
            if (cmd_and_args.size() != 5) {
                NCBI_THROW(CAutomationException, eInvalidInput,
                        "Incorrect number of parameters passed to the "
                        "NetSchedule constructor");
            }
            new_object.Reset(new SNetScheduleAutomationObject(object_id,
                    cmd_and_args[2]->GetScalar(),
                    cmd_and_args[3]->GetScalar(),
                    cmd_and_args[4]->GetScalar()));
        } else {
            NCBI_THROW_FMT(CAutomationException, eInvalidInput,
                    "Unknown class '" << class_name << "'");
        }
        m_Objects.push_back(new_object);

        PrepareReply(NStr::NumericToString(object_id));
    } else {
        NCBI_THROW_FMT(CAutomationException, eInvalidInput,
                "Unknown command '" << command << "'");
    }

    return true;
}

void CAutomationProc::PrepareError(const CTempString& error_message)
{
    m_RootNode = CSUTTPNode::NewArrayNode();
    m_RootNode->Push("err");
    m_RootNode->Push(error_message);
}

inline const CSUTTPNode* CAutomationProc::GetOutputMessage() const
{
    return m_RootNode.GetPointerOrNull();
}

void CAutomationProc::PrepareReply(const string& reply)
{
    m_RootNode = CSUTTPNode::NewArrayNode();
    m_RootNode->Push("ok");
    m_RootNode->Push(reply);
}

int CGridCommandLineInterfaceApp::Cmd_Automate()
{
    CPipe pipe;

    pipe.OpenSelf();

#ifndef WIN32
    //dup2(1, 2);
#else
    //_dup2(fileno(stdout), fileno(stderr));
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
#endif

    size_t bytes_read;

    char read_buf[1024];
    char write_buf[1024];

    CUTTPReader reader;
    CSUTTPReader suttp_reader;

    CUTTPWriter writer;
    writer.Reset(write_buf, sizeof(write_buf));
    CSUTTPWriter suttp_writer(pipe, writer);

    CAutomationProc proc;

    suttp_writer.SendMessage(proc.GetGreeting());

    while (pipe.Read(read_buf, sizeof(read_buf), &bytes_read) == eIO_Success) {
        reader.SetNewBuffer(read_buf, bytes_read);

        CSUTTPReader::EParsingEvent ret_code =
            suttp_reader.ProcessParsingEvents(reader);

        switch (ret_code) {
        case CSUTTPReader::eEndOfMessage:
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

        case CSUTTPReader::eNextBuffer:
            break;

        default: // Parsing error
            return int(ret_code);
        }
    }

    return 0;
}
