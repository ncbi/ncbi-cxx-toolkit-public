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

#include <sstream>

#include "nc_automation.hpp"
#include "ns_automation.hpp"
#include "wn_automation.hpp"
#include "nst_automation.hpp"

#include <connect/ncbi_pipe.hpp>
#include <connect/services/grid_app_version_info.hpp>

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

#define PROTOCOL_VERSION 4

#define AUTOMATION_IO_BUFFER_SIZE 4096

#define PROTOCOL_ERROR_BASE_RETCODE 20

USING_NCBI_SCOPE;

using namespace NAutomation;

struct NAutomation::SCommandImpl
{
    virtual ~SCommandImpl() {}
    virtual CJsonNode Help(const string& name, CJsonIterator& input) = 0;
};

struct SSimpleCommandImpl : SCommandImpl
{
    SSimpleCommandImpl(TArguments args);
    CJsonNode Help(const string& name, CJsonIterator& input) override;

private:
    vector<CArgument> m_Args;
};

struct SVariadicCommandImpl : SCommandImpl
{
    SVariadicCommandImpl(string args);
    CJsonNode Help(const string& name, CJsonIterator& input) override;

private:
    const string m_Args;
};

struct SCommandGroupImpl : SCommandImpl
{
    SCommandGroupImpl(TCommandsGetter getter);
    CJsonNode Help(const string& name, CJsonIterator& input) override;

private:
    TCommands m_Commands;
};

CJsonNode CArgument::Help()
{
    CJsonNode help(CJsonNode::eObject);
    help.SetString("name", m_Name);
    help.SetString("type", m_TypeOrValue.GetTypeName());
    if (m_Optional) help.SetByKey("default", m_TypeOrValue);
    return help;
}

SSimpleCommandImpl::SSimpleCommandImpl(TArguments args) :
    m_Args(args)
{
}

CJsonNode SSimpleCommandImpl::Help(const string& name, CJsonIterator&)
{
    // Full mode
    CJsonNode help(CJsonNode::eObject);
    help.SetString("command", name);

    if (m_Args.size()) {
        CJsonNode elements(CJsonNode::eArray);

        for (auto& element : m_Args) elements.Append(element.Help());

        help.SetByKey("arguments", elements);
    }

    return help;
}

SVariadicCommandImpl::SVariadicCommandImpl(string args) :
    m_Args(args)
{
}

CJsonNode SVariadicCommandImpl::Help(const string& name, CJsonIterator&)
{
    // Full mode
    CJsonNode help(CJsonNode::eObject);
    help.SetString("command", name);
    help.SetString("arguments", m_Args);
    return help;
}

SCommandGroupImpl::SCommandGroupImpl(TCommandsGetter getter) :
    m_Commands(getter())
{
}

CJsonNode SCommandGroupImpl::Help(const string& name, CJsonIterator& input)
{
    // Full mode
    CJsonNode help(CJsonNode::eObject);
    help.SetString("command", name);

    if (m_Commands.size()) {
        CJsonNode elements(CJsonNode::eArray);

        for (auto& element : m_Commands) {
            if (CJsonNode sub_help = element.Help(input)) {

                // If help is called for a particular sub-command only
                if (sub_help.IsObject()) return sub_help;

                elements.Append(sub_help);
            }
        }

        help.SetByKey("sub-topics", elements);
    }

    return help;
}

CCommand::CCommand(string name, TArguments args) :
    m_Name(name),
    m_Impl(new SSimpleCommandImpl(args))
{
}

CCommand::CCommand(string name, const char * const args) :
    m_Name(name),
    m_Impl(new SVariadicCommandImpl(args))
{
}

CCommand::CCommand(string name, TCommandsGetter getter) :
    m_Name(name),
    m_Impl(new SCommandGroupImpl(getter))
{
}

CJsonNode CCommand::Help(CJsonIterator& input)
{
    // Short mode (enumeration)
    if (!input.IsValid()) {
        return CJsonNode(m_Name);
    }

    // Help for a different element
    if (input.GetNode().AsString() != m_Name) {
        return CJsonNode();
    }

    return m_Impl->Help(m_Name, ++input);
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

TAutomationObjectRef CAutomationProc::FindObjectByPtr(const void* impl_ptr) const
{
    TPtrToObjectRefMap::const_iterator it = m_ObjectByPointer.find(impl_ptr);
    return it != m_ObjectByPointer.end() ? it->second : TAutomationObjectRef();
}

CJsonNode SServerAddressToJson::ExecOn(CNetServer server)
{
    switch (m_WhichPart) {
    case 1:
        return CJsonNode::NewStringNode(g_NetService_gethostnamebyaddr(
                server.GetHost()));
    case 2:
        return CJsonNode::NewIntegerNode(server.GetPort());
    }
    return CJsonNode::NewStringNode(server.GetServerAddress());
}

TCommands SNetServiceBase::CallCommands()
{
    TCommands cmds =
    {
        { "get_name", },
        { "get_address", {
                { "which_part", 0, }
            }},
// TODO: Remove this after GRID Python module stops using it
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        { "allow_xsite_connections", },
#endif
    };

    return cmds;
}

bool SNetServiceBase::Call(const string& method,
        CArgArray& arg_array, CJsonNode& reply)
{
    if (method == "get_name")
        reply.AppendString(m_Service.GetServiceName());
    else if (method == "get_address") {
        SServerAddressToJson server_address_proc(
                (int) arg_array.NextInteger(0));

        reply.Append(g_ExecToJson(server_address_proc,
                m_Service, m_ActualServiceType));
    } else
// TODO: Remove this after GRID Python module stops using it
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        if (method == "allow_xsite_connections")
            CNetService::AllowXSiteConnections();
        else
#endif
        return false;

    return true;
}

TCommands SNetService::CallCommands()
{
    TCommands cmds =
    {
        { "server_info", },
        { "exec", {
                { "command", CJsonNode::eString, },
                { "multiline", false, },
            }},
    };

    TCommands base_cmds = SNetServiceBase::CallCommands();
    cmds.insert(cmds.end(), base_cmds.begin(), base_cmds.end());

    return cmds;
}

bool SNetService::Call(const string& method,
        CArgArray& arg_array, CJsonNode& reply)
{
    if (method == "server_info")
        reply.Append(g_ServerInfoToJson(m_Service, m_ActualServiceType, true));
    else if (method == "exec") {
        string command(arg_array.NextString());
        reply.Append(g_ExecAnyCmdToJson(m_Service, m_ActualServiceType,
                command, arg_array.NextBoolean(false)));
    } else
        return SNetServiceBase::Call(method, arg_array, reply);

    return true;
}

CAutomationProc::CAutomationProc(IMessageSender* message_sender) :
    m_MessageSender(message_sender),
    m_OKNode(CJsonNode::NewBooleanNode(true)),
    m_ErrNode(CJsonNode::NewBooleanNode(false)),
    m_WarnNode(CJsonNode::NewStringNode("WARNING"))
{
}

const int kFirstStep = 0;

CAutomationObject* CAutomationProc::CreateObject(const string& class_name,
        CArgArray& arg_array, int step)
{
    switch (step) {
    case kFirstStep:
        return SNetCacheService::Create(arg_array, class_name, this);

    case kFirstStep + 1:
        return SNetCacheServer::Create(arg_array, class_name, this);

    case kFirstStep + 2:
        return SNetScheduleService::Create(arg_array, class_name, this);

    case kFirstStep + 3:
        return SNetScheduleServer::Create(arg_array, class_name, this);

    case kFirstStep + 4:
        return SWorkerNode::Create(arg_array, class_name, this);

    case kFirstStep + 5:
        return SNetStorageService::Create(arg_array, class_name, this);

    case kFirstStep + 6:
        return SNetStorageServer::Create(arg_array, class_name, this);

    case kFirstStep + 7:
        NCBI_THROW_FMT(CAutomationException, eInvalidInput,
                "Unknown class '" << class_name << "'");
        break; // Not reached
    }

    _TROUBLE;
    return nullptr;
}

TObjectID CAutomationProc::CreateObject(CArgArray& arg_array)
{
    string class_name(arg_array.NextString());
    arg_array.UpdateLocation(class_name);

    TAutomationObjectRef new_object;

    try {
        for (int step = kFirstStep; !new_object; ++step) {
            new_object.Reset(CreateObject(class_name, arg_array, step));
        }
    }
    catch (CException& e) {
        NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                "Error in '" << class_name << "' constructor: " << e.GetMsg());
    }
    return AddObject(new_object, new_object->GetImplPtr());
}

TCommands CAutomationProc::CallCommands()
{
    return TCommands
    {
        SNetCacheBlob::CallCommand(),
        SNetCacheService::CallCommand(),
        SNetCacheServer::CallCommand(),
        SNetScheduleService::CallCommand(),
        SNetScheduleServer::CallCommand(),
        SNetStorageService::CallCommand(),
        SNetStorageServer::CallCommand(),
        SNetStorageObject::CallCommand(),
        SWorkerNode::CallCommand(),
    };
}

TCommands CAutomationProc::NewCommands()
{
    return TCommands
    {
        SNetCacheService::NewCommand(),
        SNetCacheServer::NewCommand(),
        SNetScheduleService::NewCommand(),
        SNetScheduleServer::NewCommand(),
        SNetStorageService::NewCommand(),
        SNetStorageServer::NewCommand(),
        SWorkerNode::NewCommand(),
    };
}

TCommands CAutomationProc::Commands()
{
    TCommands cmds =
    {
        { "exit", },
        { "call", CAutomationProc::CallCommands },
        { "new", CAutomationProc::NewCommands },
        { "del", {
                { "object_id", CJsonNode::eInteger, },
            }},
        { "whatis", {
                { "some_id", CJsonNode::eString, },
            }},
        { "echo", "any" },
        { "version", },
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        { "allow_xsite_connections", },
#endif
    };

    return cmds;
}

CCommand CAutomationProc::HelpCommand()
{
    return CCommand("help", CAutomationProc::Commands);
}

CJsonNode CAutomationProc::ProcessMessage(const CJsonNode& message)
{
    CJsonIterator input(message.Iterate());
    CJsonNode reply(CJsonNode::NewArrayNode());

    // Empty input (help)
    if (!input.IsValid()) {
        reply.AppendString("Type `help` (in single quotes) for usage. "
                "For help on a sub-topic, add it to the command, "
                "e.g. `help`, `call`, `nstobj`");
        return reply;
    }

    // Help
    if (CJsonNode help = HelpCommand().Help(input)) {
        return help;
    }

    CArgArray arg_array(message);

    string command(arg_array.NextString());

    arg_array.UpdateLocation(command);

    if (command == "exit")
        return CJsonNode();

    reply.Append(m_OKNode);

    if (command == "call") {
        TAutomationObjectRef object_ref(ObjectIdToRef(
                (TObjectID) arg_array.NextInteger()));
        string method(arg_array.NextString());
        arg_array.UpdateLocation(method);
        if (!object_ref->Call(method, arg_array, reply)) {
            NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                    "Unknown " << object_ref->GetType() <<
                            " method '" << method << "'");
        }
    } else if (command == "new") {
        reply.AppendInteger(CreateObject(arg_array));
    } else if (command == "del") {
        TAutomationObjectRef& object(ObjectIdToRef(
                (TObjectID) arg_array.NextInteger()));
        m_ObjectByPointer.erase(object->GetImplPtr());
        object = NULL;
    } else if (command == "whatis") {
        CJsonNode result(g_WhatIs(arg_array.NextString()));

        if (result) {
            reply.Append(result);
        } else {
            NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                    "Unable to recognize the specified token.");
        }
    } else if (command == "echo") {
        CJsonNode echo_reply(message);
        echo_reply.SetAt(0, CJsonNode::NewBooleanNode(true));
        return echo_reply;
    } else if (command == "version") {
        reply.AppendString(GRID_APP_VERSION);
        reply.AppendString(__DATE__);
    } else
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        if (command == "allow_xsite_connections")
            CNetService::AllowXSiteConnections();
        else
#endif
        arg_array.Exception("unknown command");

    return reply;
}

void CAutomationProc::SendWarning(const string& warn_msg,
        TAutomationObjectRef source)
{
    CJsonNode warning(CJsonNode::NewArrayNode());
    warning.Append(m_WarnNode);
    warning.AppendString(warn_msg);
    warning.AppendString(source->GetType());
    warning.AppendInteger(source->GetID());
    m_MessageSender->SendMessage(warning);
}

void CAutomationProc::SendError(const CTempString& error_message)
{
    CJsonNode error(CJsonNode::NewArrayNode());
    error.Append(m_ErrNode);
    error.AppendString(error_message);
    m_MessageSender->SendMessage(error);
}

TAutomationObjectRef& CAutomationProc::ObjectIdToRef(TObjectID object_id)
{
    size_t index = (size_t) (object_id);

    if (index >= m_ObjectByIndex.size() || !m_ObjectByIndex[index]) {
        NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                "Object with id '" << object_id << "' does not exist");
    }
    return m_ObjectByIndex[index];
}

class CMessageSender : public IMessageSender
{
public:
    CMessageSender(CJsonOverUTTPWriter& json_writer, CPipe& pipe) :
        m_JSONWriter(json_writer),
        m_Pipe(pipe)
    {
    }

    virtual void SendMessage(const CJsonNode& message);

private:
    void SendOutputBuffer();

    CJsonOverUTTPWriter& m_JSONWriter;
    CPipe& m_Pipe;
};

void CMessageSender::SendMessage(const CJsonNode& message)
{
    if (!m_JSONWriter.WriteMessage(message))
        do
            SendOutputBuffer();
        while (!m_JSONWriter.CompleteMessage());

    SendOutputBuffer();
}

void CMessageSender::SendOutputBuffer()
{
    const char* output_buffer;
    size_t output_buffer_size;
    size_t bytes_written;

    do {
        m_JSONWriter.GetOutputBuffer(&output_buffer, &output_buffer_size);
        while (output_buffer_size > 0) {
            if (m_Pipe.Write(output_buffer, output_buffer_size,
                    &bytes_written) != eIO_Success) {
                NCBI_THROW(CIOException, eWrite,
                    "Error while writing to the pipe");
            }
            output_buffer += bytes_written;
            output_buffer_size -= bytes_written;
        }
    } while (m_JSONWriter.NextOutputBuffer());
}

class CMessageDumperSender : public IMessageSender
{
public:
    CMessageDumperSender(IMessageSender* actual_sender,
            FILE* protocol_dump_file);

    void DumpInputMessage(const CJsonNode& message);

    virtual void SendMessage(const CJsonNode& message);

private:
    IMessageSender* m_ActualSender;

    FILE* m_ProtocolDumpFile;
    string m_ProtocolDumpTimeFormat;
    string m_DumpInputHeaderFormat;
    string m_DumpOutputHeaderFormat;
};

CMessageDumperSender::CMessageDumperSender(
        IMessageSender* actual_sender,
        FILE* protocol_dump_file) :
    m_ActualSender(actual_sender),
    m_ProtocolDumpFile(protocol_dump_file),
    m_ProtocolDumpTimeFormat("Y/M/D h:m:s.l")
{
    string pid_str(NStr::NumericToString((Int8) CProcess::GetCurrentPid()));

    m_DumpInputHeaderFormat.assign(1, '\n');
    m_DumpInputHeaderFormat.append(71 + pid_str.length(), '=');
    m_DumpInputHeaderFormat.append("\n----- IN ------ %s (pid: ");
    m_DumpInputHeaderFormat.append(pid_str);
    m_DumpInputHeaderFormat.append(") -----------------------\n");

    m_DumpOutputHeaderFormat.assign("----- OUT ----- %s (pid: ");
    m_DumpOutputHeaderFormat.append(pid_str);
    m_DumpOutputHeaderFormat.append(") -----------------------\n");
}

void CMessageDumperSender::DumpInputMessage(const CJsonNode& message)
{
    fprintf(m_ProtocolDumpFile, m_DumpInputHeaderFormat.c_str(),
            GetFastLocalTime().AsString(m_ProtocolDumpTimeFormat).c_str());
    g_PrintJSON(m_ProtocolDumpFile, message);
}

void CMessageDumperSender::SendMessage(const CJsonNode& message)
{
    fprintf(m_ProtocolDumpFile, m_DumpOutputHeaderFormat.c_str(),
            GetFastLocalTime().AsString(m_ProtocolDumpTimeFormat).c_str());
    g_PrintJSON(m_ProtocolDumpFile, message);

    if (m_ActualSender != NULL)
        m_ActualSender->SendMessage(message);
}

int CGridCommandLineInterfaceApp::Automation_PipeServer()
{
    CPipe pipe;

    pipe.OpenSelf();

#ifdef WIN32
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
#endif

    char read_buf[AUTOMATION_IO_BUFFER_SIZE];
    char write_buf[AUTOMATION_IO_BUFFER_SIZE];

    CUTTPReader reader;
    CUTTPWriter writer;

    writer.Reset(write_buf, sizeof(write_buf));

    CJsonOverUTTPReader json_reader;
    CJsonOverUTTPWriter json_writer(writer);

    CMessageSender actual_message_sender(json_writer, pipe);

    IMessageSender* message_sender;

    auto_ptr<CMessageDumperSender> dumper_and_sender;

    if (!IsOptionSet(eProtocolDump))
        message_sender = &actual_message_sender;
    else {
        dumper_and_sender.reset(new CMessageDumperSender(&actual_message_sender,
                m_Opts.protocol_dump));
        message_sender = dumper_and_sender.get();
    }

    {
        CJsonNode greeting(CJsonNode::NewArrayNode());
        greeting.AppendString(GRID_APP_NAME);
        greeting.AppendInteger(PROTOCOL_VERSION);

        message_sender->SendMessage(greeting);
    }

    CAutomationProc proc(message_sender);

    size_t bytes_read;

    try {
        while (pipe.Read(read_buf,
                sizeof(read_buf), &bytes_read) == eIO_Success) {
            reader.SetNewBuffer(read_buf, bytes_read);

            while (json_reader.ReadMessage(reader)) {
                try {
                    if (dumper_and_sender.get() != NULL)
                        dumper_and_sender->DumpInputMessage(
                                json_reader.GetMessage());

                    CJsonNode reply(proc.ProcessMessage(
                            json_reader.GetMessage()));

                    if (!reply)
                        return 0;

                    message_sender->SendMessage(reply);
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
                catch (CException& e) {
                    ostringstream os;
                    os << e.GetMsg() << ": ";
                    e.ReportExtra(os);
                    proc.SendError(os.str());
                }

                json_reader.Reset();
            }
        }
    }
    catch (CJsonOverUTTPException& e) {
        return PROTOCOL_ERROR_BASE_RETCODE + e.GetErrCode();
    }

    return 0;
}

#define DEBUG_CONSOLE_PROMPT ">>> "

int CGridCommandLineInterfaceApp::Automation_DebugConsole()
{
    printf("[" GRID_APP_NAME " automation debug console; "
            "protocol version " NCBI_AS_STRING(PROTOCOL_VERSION) "]\n"
            "Press ^D or type \"exit\" (in double qoutes) to quit.\n"
            "\n" DEBUG_CONSOLE_PROMPT);

    char read_buf[AUTOMATION_IO_BUFFER_SIZE];

    CMessageDumperSender dumper_and_sender(NULL, stdout);

    CAutomationProc proc(&dumper_and_sender);

    while (fgets(read_buf, sizeof(read_buf), stdin) != NULL) {
        try {
            CJsonNode input_message(CJsonNode::ParseArray(read_buf));

            try {
                dumper_and_sender.DumpInputMessage(input_message);

                CJsonNode reply(proc.ProcessMessage(input_message));

                if (!reply)
                    return 0;

                dumper_and_sender.SendMessage(reply);
            }
            catch (CException& e) {
                ostringstream os;
                os << e.GetMsg() << ": ";
                e.ReportExtra(os);
                proc.SendError(os.str());
            }
        }
        catch (CStringException& e) {
            printf("%*c syntax error\n", int(e.GetPos() +
                    sizeof(DEBUG_CONSOLE_PROMPT) - 1), int('^'));
        }
        printf(DEBUG_CONSOLE_PROMPT);
    }

    puts("");

    return 0;
}

int CGridCommandLineInterfaceApp::Cmd_Automate()
{
    return IsOptionSet(eDebugConsole) ?
            Automation_DebugConsole() : Automation_PipeServer();
}
