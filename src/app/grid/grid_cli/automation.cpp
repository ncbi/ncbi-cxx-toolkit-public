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

#include "nc_automation.hpp"
#include "ns_automation.hpp"
#include "wn_automation.hpp"

#include <connect/ncbi_pipe.hpp>

#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

USING_NCBI_SCOPE;

#define PROTOCOL_VERSION 4

#define AUTOMATION_IO_BUFFER_SIZE 4096

#define PROTOCOL_ERROR_BASE_RETCODE 20

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

bool SNetServiceAutomationObject::Call(const string& method,
        CArgArray& arg_array, CJsonNode& reply)
{
    if (method == "get_name")
        reply.AppendString(m_Service.GetServiceName());
    else if (method == "get_address") {
        SServerAddressToJson server_address_proc(
                (int) arg_array.NextInteger(0));

        reply.Append(g_ExecToJson(server_address_proc,
                m_Service, m_ActualServiceType));
    } else if (method == "server_info")
        reply.Append(g_ServerInfoToJson(m_Service, m_ActualServiceType, true));
    else if (method == "exec") {
        string command(arg_array.NextString());
        reply.Append(g_ExecAnyCmdToJson(m_Service, m_ActualServiceType,
                command, arg_array.NextBoolean(false)));
    } else
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        if (method == "allow_xsite_connections")
            m_Service.AllowXSiteConnections();
        else
#endif
            return false;

    return true;
}

CAutomationProc::CAutomationProc(IMessageSender* message_sender) :
    m_MessageSender(message_sender),
    m_Pid((Int8) CProcess::GetCurrentPid()),
    m_OKNode(CJsonNode::NewBooleanNode(true)),
    m_ErrNode(CJsonNode::NewBooleanNode(false)),
    m_WarnNode(CJsonNode::NewStringNode("WARNING"))
{
}

TObjectID CAutomationProc::CreateObject(const string& class_name,
        CArgArray& arg_array)
{
    const void* impl_ptr;
    TAutomationObjectRef new_object;
    try {
        if (class_name == "ncsvc") {
            string service_name(arg_array.NextString(kEmptyStr));
            string client_name(arg_array.NextString(kEmptyStr));
            SNetCacheServiceAutomationObject* ncsvc_object_ptr =
                    new SNetCacheServiceAutomationObject(this,
                            service_name, client_name);
            impl_ptr = ncsvc_object_ptr->m_NetCacheAPI;
            new_object.Reset(ncsvc_object_ptr);
        } else if (class_name == "ncsrv") {
            string service_name(arg_array.NextString(kEmptyStr));
            string client_name(arg_array.NextString(kEmptyStr));
            SNetCacheServerAutomationObject* ncsrv_object_ptr =
                    new SNetCacheServerAutomationObject(this,
                            service_name, client_name);
            impl_ptr = ncsrv_object_ptr->m_NetCacheAPI;
            new_object.Reset(ncsrv_object_ptr);
        } else if (class_name == "nssvc") {
            string service_name(arg_array.NextString(kEmptyStr));
            string queue_name(arg_array.NextString(kEmptyStr));
            string client_name(arg_array.NextString(kEmptyStr));
            SNetScheduleServiceAutomationObject* nssvc_object_ptr =
                    new SNetScheduleServiceAutomationObject(this,
                        service_name, queue_name, client_name);
            impl_ptr = nssvc_object_ptr->m_NetScheduleAPI;
            new_object.Reset(nssvc_object_ptr);
        } else if (class_name == "nssrv") {
            string service_name(arg_array.NextString(kEmptyStr));
            string queue_name(arg_array.NextString(kEmptyStr));
            string client_name(arg_array.NextString(kEmptyStr));
            SNetScheduleServiceAutomationObject* nssrv_object_ptr =
                    new SNetScheduleServerAutomationObject(this,
                        service_name, queue_name, client_name);
            impl_ptr = nssrv_object_ptr->m_NetScheduleAPI;
            new_object.Reset(nssrv_object_ptr);
        } else if (class_name == "wn") {
            string wn_address(arg_array.NextString());
            string client_name(arg_array.NextString(kEmptyStr));
            SWorkerNodeAutomationObject* wn_object_ptr =
                    new SWorkerNodeAutomationObject(this,
                            wn_address, client_name);
            impl_ptr = wn_object_ptr->m_NetScheduleAPI;
            new_object.Reset(wn_object_ptr);
        } else {
            NCBI_THROW_FMT(CAutomationException, eInvalidInput,
                    "Unknown class '" << class_name << "'");
        }
    }
    catch (CException& e) {
        NCBI_THROW_FMT(CAutomationException, eCommandProcessingError,
                "Error in '" << class_name << "' constructor: " << e.GetMsg());
    }
    return AddObject(new_object, impl_ptr);
}

CJsonNode CAutomationProc::ProcessMessage(const CJsonNode& message)
{
    CArgArray arg_array(message);

    string command(arg_array.NextString());

    arg_array.UpdateLocation(command);

    if (command == "exit")
        return NULL;

    CJsonNode reply(CJsonNode::NewArrayNode());
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
        string class_name(arg_array.NextString());
        arg_array.UpdateLocation(class_name);
        reply.AppendInteger(CreateObject(class_name, arg_array));
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
        CJsonNode reply(message);
        reply.SetAt(0, CJsonNode::NewBooleanNode(true));
        return reply;
    } else
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
    size_t index = (size_t) (object_id - m_Pid);

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
                    proc.SendError(e.GetMsg());
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

    CNetScheduleStructuredOutputParser parser;

    while (fgets(read_buf, sizeof(read_buf), stdin) != NULL) {
        try {
            CJsonNode input_message(parser.ParseArray(read_buf));

            try {
                dumper_and_sender.DumpInputMessage(input_message);

                CJsonNode reply(proc.ProcessMessage(input_message));

                if (!reply)
                    return 0;

                dumper_and_sender.SendMessage(reply);
            }
            catch (CException& e) {
                proc.SendError(e.GetMsg());
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
