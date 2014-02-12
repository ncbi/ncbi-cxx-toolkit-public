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
 *  Government have not placed any restriction on its use or reproduction.
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
 * Author:  Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of the unified network blob storage API.
 *
 */

#include <ncbi_pch.hpp>

#include "netservice_api_impl.hpp"

#include <connect/services/netstorage_admin.hpp>
#include <connect/services/error_codes.hpp>

#include <util/ncbi_url.hpp>

#include <corelib/request_ctx.hpp>

#define NCBI_USE_ERRCODE_X  NetStorage_RPC

#define NST_PROTOCOL_VERSION 1

#define READ_CHUNK_SIZE (4 * 1024)

#define WRITE_BUFFER_SIZE (64 * 1024)
#define READ_BUFFER_SIZE (64 * 1024)

#define END_OF_DATA_MARKER '\n'

BEGIN_NCBI_SCOPE

const char* CNetStorageException::GetErrCodeString() const
{
    switch (GetErrCode()) {
    case eInvalidArg:           return "eInvalidArg";
    case eNotExists:            return "eNotExist";
    case eAuthError:            return "eAuthError";
    case eIOError:              return "eIOError";
    case eServerError:          return "eServerError";
    case eTimeout:              return "eTimeout";
    default:                    return CException::GetErrCodeString();
    }
}

static void s_WriteToSocket(CSocket* sock,
        const char* output_buffer, size_t output_buffer_size)
{
    size_t bytes_written;

    while (output_buffer_size > 0) {
        EIO_Status  status = sock->Write(output_buffer,
                output_buffer_size, &bytes_written);
        if (status != eIO_Success) {
            // Error writing to the socket.
            // Log what we can.
            string message_start;

            if (output_buffer_size > 32) {
                CTempString buffer_head(output_buffer, 32);
                message_start = NStr::PrintableString(buffer_head);
                message_start += " (TRUNCATED)";
            } else {
                CTempString buffer_head(output_buffer, output_buffer_size);
                message_start = NStr::PrintableString(buffer_head);
            }

            NCBI_THROW_FMT(CNetStorageException, eIOError,
                    "Error writing message to the NetStorage server " <<
                            sock->GetPeerAddress() << ". "
                    "Socket write error status: " <<
                            IO_StatusStr(status) << ". "
                    "Bytes written: " <<
                            NStr::NumericToString(bytes_written) << ". "
                    "Message begins with: " << message_start);
        }

        output_buffer += bytes_written;
        output_buffer_size -= bytes_written;
    }
}

class CSendJsonOverSocket
{
public:
    CSendJsonOverSocket() : m_JSONWriter(m_UTTPWriter) {}

    void SendMessage(const CJsonNode& message, CSocket* sock);

private:
    void x_SendOutputBuffer();

    CUTTPWriter m_UTTPWriter;
    CJsonOverUTTPWriter m_JSONWriter;
    CSocket* m_Socket;
};

void CSendJsonOverSocket::SendMessage(const CJsonNode& message, CSocket* sock)
{
    char write_buffer[WRITE_BUFFER_SIZE];

    m_UTTPWriter.Reset(write_buffer, WRITE_BUFFER_SIZE);

    m_Socket = sock;

    if (!m_JSONWriter.WriteMessage(message))
        do
            x_SendOutputBuffer();
        while (!m_JSONWriter.CompleteMessage());

    x_SendOutputBuffer();
}

void CSendJsonOverSocket::x_SendOutputBuffer()
{
    const char* output_buffer;
    size_t output_buffer_size;

    do {
        m_JSONWriter.GetOutputBuffer(&output_buffer, &output_buffer_size);

        s_WriteToSocket(m_Socket, output_buffer, output_buffer_size);
    } while (m_JSONWriter.NextOutputBuffer());
}

static void s_SendUTTPChunk(const char* chunk, size_t chunk_size, CSocket* sock)
{
    CUTTPWriter uttp_writer;

    char write_buffer[WRITE_BUFFER_SIZE];

    uttp_writer.Reset(write_buffer, WRITE_BUFFER_SIZE);

    uttp_writer.SendChunk(chunk, chunk_size, false);

    const char* output_buffer;
    size_t output_buffer_size;

    do {
        uttp_writer.GetOutputBuffer(&output_buffer, &output_buffer_size);
        s_WriteToSocket(sock, output_buffer, output_buffer_size);
    } while (uttp_writer.NextOutputBuffer());

    uttp_writer.GetOutputBuffer(&output_buffer, &output_buffer_size);
    s_WriteToSocket(sock, output_buffer, output_buffer_size);
}

static void s_SendEndOfData(CSocket* sock)
{
    CUTTPWriter uttp_writer;

    char write_buffer[WRITE_BUFFER_SIZE];

    uttp_writer.Reset(write_buffer, WRITE_BUFFER_SIZE);

    uttp_writer.SendControlSymbol(END_OF_DATA_MARKER);

    const char* output_buffer;
    size_t output_buffer_size;

    do {
        uttp_writer.GetOutputBuffer(&output_buffer, &output_buffer_size);
        s_WriteToSocket(sock, output_buffer, output_buffer_size);
    } while (uttp_writer.NextOutputBuffer());

    uttp_writer.GetOutputBuffer(&output_buffer, &output_buffer_size);
    s_WriteToSocket(sock, output_buffer, output_buffer_size);
}

static void s_ReadSocket(CSocket* sock, void* buffer,
        size_t buffer_size, size_t* bytes_read)
{
    EIO_Status status;

    while ((status = sock->Read(buffer,
            buffer_size, bytes_read)) == eIO_Interrupt)
        /* no-op */;

    if (status != eIO_Success) {
        // eIO_Timeout, eIO_Closed, eIO_InvalidArg,
        // eIO_NotSupported, eIO_Unknown
        NCBI_THROW_FMT(CNetStorageException, eIOError,
                "I/O error while reading from NetStorage server " <<
                        sock->GetPeerAddress() << ". "
                "Socket status: " << IO_StatusStr(status) << '.');
    }
}

class CReadJsonFromSocket
{
public:
    CJsonNode ReadMessage(CSocket* sock);

private:
    CUTTPReader m_UTTPReader;
    CJsonOverUTTPReader m_JSONReader;
};

CJsonNode CReadJsonFromSocket::ReadMessage(CSocket* sock)
{
    char read_buffer[READ_BUFFER_SIZE];

    size_t bytes_read;

    do {
        s_ReadSocket(sock, read_buffer, READ_BUFFER_SIZE, &bytes_read);

        m_UTTPReader.SetNewBuffer(read_buffer, bytes_read);

    } while (!m_JSONReader.ReadMessage(m_UTTPReader));

    if (m_UTTPReader.GetNextEvent() != CUTTPReader::eEndOfBuffer) {
        NCBI_THROW_FMT(CNetStorageException, eIOError,
                "Extra bytes past message end while reading from " <<
                        sock->GetPeerAddress() << " after receiving " <<
                        m_JSONReader.GetMessage().Repr() << '.');
    }

    return m_JSONReader.GetMessage();
}

static void s_TrapErrors(const CJsonNode& request,
        const CJsonNode& reply, CSocket* sock)
{
    CJsonNode issues(reply.GetByKeyOrNull("Warnings"));

    if (issues) {
        string server_address(sock->GetPeerAddress());

        for (CJsonIterator it = issues.Iterate(); it; ++it) {
            LOG_POST(Warning << "NetStorage server " << server_address <<
                    " issued warning #" << (*it).GetInteger("Code") <<
                    " (" << (*it).GetString("Message") << ").");
        }
    }

    if (reply.GetString("Status") != "OK") {
        string errors;

        issues = reply.GetByKeyOrNull("Errors");

        bool file_not_found = false;

        if (!issues)
            errors = reply.GetString("Status");
        else {
            const char* prefix = "error ";

            for (CJsonIterator it = issues.Iterate(); it; ++it) {
                errors += prefix;
                Int8 server_error_code = (*it).GetInteger("Code");
                if (server_error_code == 14)
                    file_not_found = true;
                errors += NStr::NumericToString(server_error_code);
                errors += ": ";
                errors += (*it).GetString("Message");
                prefix = ", error ";
            }
        }

        string err_msg = FORMAT("Error while executing " <<
                        request.GetString("Type") << " "
                "on NetStorage server " <<
                        sock->GetPeerAddress() << ". "
                "Server returned " << errors << '.');

        if (file_not_found) {
            NCBI_THROW_FMT(CNetStorageException, eNotExists, err_msg);
        } else {
            NCBI_THROW_FMT(CNetStorageException, eServerError, err_msg);
        }
    }

    if (reply.GetInteger("RE") != request.GetInteger("SN")) {
        NCBI_THROW_FMT(CNetStorageException, eServerError,
                "Message serial number mismatch "
                "(NetStorage server: " << sock->GetPeerAddress() << "; "
                "request: " << request.Repr() << "; "
                "reply: " << reply.Repr() << ").");
    }
}

static CJsonNode s_Exchange(const CJsonNode& request, CSocket* sock)
{
    CSendJsonOverSocket message_sender;

    message_sender.SendMessage(request, sock);

    CReadJsonFromSocket message_reader;

    CJsonNode reply(message_reader.ReadMessage(sock));

    s_TrapErrors(request, reply, sock);

    return reply;
}

class CNetStorageServerListener : public INetServerConnectionListener
{
public:
    CNetStorageServerListener(const CJsonNode& hello) : m_Hello(hello) {}

public:
    virtual CRef<INetServerProperties> AllocServerProperties();

public:
    virtual CConfig* LoadConfigFromAltSource(CObject* api_impl,
        string* new_section_name);
    virtual void OnInit(CObject* api_impl,
        CConfig* config, const string& config_section);
    virtual void OnConnected(CNetServerConnection::TInstance conn_impl);
    virtual void OnError(const string& err_msg, SNetServerImpl* server);
    virtual void OnWarning(const string& warn_msg, SNetServerImpl* server);

    CJsonNode m_Hello;
};

struct SNetStorageRPC : public SNetStorageImpl
{
    SNetStorageRPC(const string& init_string, TNetStorageFlags default_flags);

    virtual CNetStorageObject Create(TNetStorageFlags flags = 0);
    virtual CNetStorageObject Open(const string& object_loc,
            TNetStorageFlags flags = 0);
    virtual string Relocate(const string& object_loc, TNetStorageFlags flags);
    virtual bool Exists(const string& object_loc);
    virtual void Remove(const string& object_loc);

    CJsonNode Exchange(const CJsonNode& request,
            CNetServerConnection* conn = NULL,
            CNetServer::TInstance server_to_use = NULL);

    void x_SetStorageFlags(CJsonNode& node, TNetStorageFlags flags);
    void x_SetICacheNames(CJsonNode& node);
    CJsonNode MkStdRequest(const string& request_type);
    CJsonNode MkFileRequest(const string& request_type,
            const string& object_loc);
    CJsonNode MkFileRequest(const string& request_type,
            const string& unique_key, TNetStorageFlags flags);

    TNetStorageFlags m_DefaultFlags;
    CNetService m_Service;

    string m_NetStorageServiceName;
    string m_NetCacheServiceName;
    string m_CacheName;
    string m_ClientName;
    string m_AppDomain;

    CAtomicCounter m_RequestNumber;
};

CRef<INetServerProperties> CNetStorageServerListener::AllocServerProperties()
{
    return CRef<INetServerProperties>(new INetServerProperties);
}

CConfig* CNetStorageServerListener::LoadConfigFromAltSource(
        CObject* /*api_impl*/, string* /*new_section_name*/)
{
    return NULL;
}

void CNetStorageServerListener::OnInit(CObject* /*api_impl*/,
        CConfig* /*config*/, const string& /*config_section*/)
{
    //SNetStorageRPC* netstorage_rpc = static_cast<SNetStorageRPC*>(api_impl);
}

void CNetStorageServerListener::OnConnected(
        CNetServerConnection::TInstance conn_impl)
{
    s_Exchange(m_Hello, &conn_impl->m_Socket);
}

void CNetStorageServerListener::OnError(const string& /*err_msg*/,
        SNetServerImpl* /*server*/)
{
}

void CNetStorageServerListener::OnWarning(const string& warn_msg,
        SNetServerImpl* /*server*/)
{
}

struct SNetStorageObjectRPC : public SNetStorageObjectImpl
{
    enum EObjectIdentification {
        eByGeneratedID,
        eByUniqueKey
    };

    enum EState {
        eReady,
        eReading,
        eWriting
    };

    SNetStorageObjectRPC(SNetStorageRPC* netstorage_rpc,
            CJsonNode::TInstance original_request,
            CNetServerConnection::TInstance conn,
            EObjectIdentification object_identification,
            const string& object_loc_or_key,
            TNetStorageFlags flags,
            EState initial_state);

    virtual ~SNetStorageObjectRPC();

    virtual string GetID();
    virtual size_t Read(void* buffer, size_t buf_size);
    virtual bool Eof();
    virtual void Write(const void* buffer, size_t buf_size);
    virtual Uint8 GetSize();
    virtual string GetAttribute(const string& attr_name);
    virtual void SetAttribute(const string& attr_name,
            const string& attr_value);
    virtual CNetStorageObjectInfo GetInfo();
    virtual void Close();

    CJsonNode x_MkRequest(const string& request_type);

    CRef<SNetStorageRPC,
            CNetComponentCounterLocker<SNetStorageRPC> > m_NetStorageRPC;

    CJsonNode m_OriginalRequest;
    CNetServerConnection m_Connection;
    char* m_ReadBuffer;
    CUTTPReader m_UTTPReader;
    const char* m_CurrentChunk;
    size_t m_CurrentChunkSize;
    EObjectIdentification m_ObjectIdentification;
    string m_ID;
    string m_UniqueKey;
    TNetStorageFlags m_Flags;
    EState m_State;
    bool m_EOF;
};

SNetStorageObjectRPC::SNetStorageObjectRPC(SNetStorageRPC* netstorage_rpc,
        CJsonNode::TInstance original_request,
        CNetServerConnection::TInstance conn,
        EObjectIdentification object_identification,
        const string& object_loc_or_key,
        TNetStorageFlags flags,
        EState initial_state) :
    m_NetStorageRPC(netstorage_rpc),
    m_OriginalRequest(original_request),
    m_Connection(conn),
    m_ReadBuffer(NULL),
    m_ObjectIdentification(object_identification),
    m_Flags(flags),
    m_State(initial_state)
{
    if (object_identification == eByGeneratedID)
        m_ID = object_loc_or_key;
    else
        m_UniqueKey = object_loc_or_key;
}

SNetStorageObjectRPC::~SNetStorageObjectRPC()
{
    delete[] m_ReadBuffer;
}

static const char* const s_NetStorageConfigSections[] = {
    "netstorage_api",
    NULL
};

SNetStorageRPC::SNetStorageRPC(const string& init_string,
        TNetStorageFlags default_flags) :
    m_DefaultFlags(default_flags)
{
    if (init_string.empty()) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "initialization string cannot be empty");
    }

    CUrlArgs url_parser(init_string);

    ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
        if (field->name.empty())
            continue;
        switch (field->name[0]) {
        case 'd':
            if (field->name == "domain")
                m_AppDomain = field->value;
            break;
        case 'n':
            if (field->name == "nst")
                m_NetStorageServiceName = field->value;
            else if (field->name == "nc")
                m_NetCacheServiceName = field->value;
            break;
        case 'c':
            if (field->name == "cache")
                m_CacheName = field->value;
            else if (field->name == "client")
                m_ClientName = field->value;
        }
    }

    if (m_NetStorageServiceName.empty()) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "'nst' parameter is missing from the initialization string");
    }

    m_RequestNumber.Set(0);

    CJsonNode hello(MkStdRequest("HELLO"));

    hello.SetString("Client", m_ClientName);
    CNcbiApplication* app = CNcbiApplication::Instance();
    if (app != NULL)
        hello.SetString("Application", app->GetProgramExecutablePath());
    hello.SetInteger("ProtocolVersion", NST_PROTOCOL_VERSION);

    m_Service = new SNetServiceImpl("NetStorageAPI", m_ClientName,
            new CNetStorageServerListener(hello));

    m_Service->Init(this, m_NetStorageServiceName,
            NULL, kEmptyStr, s_NetStorageConfigSections);

    if (m_Service->m_ServiceType == CNetService::eServiceNotDefined) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "NetStorage service name is not defined");
    }
}

CNetStorageObject SNetStorageRPC::Create(TNetStorageFlags flags)
{
    CJsonNode request(MkStdRequest("WRITE"));

    x_SetStorageFlags(request, flags);
    x_SetICacheNames(request);

    CNetServerConnection conn;

    string object_loc = Exchange(request, &conn).GetString("ObjectLoc");

    return new SNetStorageObjectRPC(this, request, conn,
            SNetStorageObjectRPC::eByGeneratedID,
            object_loc, flags, SNetStorageObjectRPC::eWriting);
}

CNetStorageObject SNetStorageRPC::Open(const string& object_loc,
        TNetStorageFlags flags)
{
    return new SNetStorageObjectRPC(this, NULL, NULL,
            SNetStorageObjectRPC::eByGeneratedID,
            object_loc, flags, SNetStorageObjectRPC::eReady);
}

string SNetStorageRPC::Relocate(const string& object_loc, TNetStorageFlags flags)
{
    CJsonNode request(MkFileRequest("RELOCATE", object_loc));

    CJsonNode new_location(CJsonNode::NewObjectNode());

    x_SetStorageFlags(new_location, flags);

    request.SetByKey("NewLocation", new_location);

    return Exchange(request).GetString("ObjectLoc");
}

bool SNetStorageRPC::Exists(const string& object_loc)
{
    CJsonNode request(MkFileRequest("EXISTS", object_loc));

    return Exchange(request).GetBoolean("Exists");
}

void SNetStorageRPC::Remove(const string& object_loc)
{
    CJsonNode request(MkFileRequest("DELETE", object_loc));

    Exchange(request);
}

class CJsonOverUTTPExecHandler : public INetServerExecHandler
{
public:
    CJsonOverUTTPExecHandler(const CJsonNode& request) : m_Request(request) {}

    virtual void Exec(CNetServerConnection::TInstance conn_impl,
            STimeout* timeout);

    CNetServerConnection GetConnection() const {return m_Connection;}

private:
    CJsonNode m_Request;

    CNetServerConnection m_Connection;
};

void CJsonOverUTTPExecHandler::Exec(CNetServerConnection::TInstance conn_impl,
        STimeout* timeout)
{
    CSendJsonOverSocket sender;

    sender.SendMessage(m_Request, &conn_impl->m_Socket);

    m_Connection = conn_impl;
}

CJsonNode SNetStorageRPC::Exchange(const CJsonNode& request,
        CNetServerConnection* conn,
        CNetServer::TInstance server_to_use)
{
    CNetServer server(server_to_use != NULL ? server_to_use :
            (CNetServer::TInstance)
                    *m_Service.Iterate(CNetService::eRandomize));

    CJsonOverUTTPExecHandler json_over_uttp_sender(request);

    server->TryExec(json_over_uttp_sender);

    CReadJsonFromSocket message_reader;

    if (conn != NULL)
        *conn = json_over_uttp_sender.GetConnection();

    CSocket* sock = &json_over_uttp_sender.GetConnection()->m_Socket;

    CJsonNode reply(message_reader.ReadMessage(sock));

    s_TrapErrors(request, reply, sock);

    return reply;
}

void SNetStorageRPC::x_SetStorageFlags(CJsonNode& node, TNetStorageFlags flags)
{
    CJsonNode storage_flags(CJsonNode::NewObjectNode());

    if (flags & fNST_Fast)
        storage_flags.SetBoolean("Fast", true);
    if (flags & fNST_Persistent)
        storage_flags.SetBoolean("Persistent", true);
    if (flags & fNST_Movable)
        storage_flags.SetBoolean("Movable", true);
    if (flags & fNST_Cacheable)
        storage_flags.SetBoolean("Cacheable", true);
    if (flags & fNST_NoMetaData)
        storage_flags.SetBoolean("NoMetaData", true);

    node.SetByKey("StorageFlags", storage_flags);
}

void SNetStorageRPC::x_SetICacheNames(CJsonNode& node)
{
    CJsonNode icache(CJsonNode::NewObjectNode());
    icache.SetString("ServiceName", m_NetCacheServiceName);
    icache.SetString("CacheName", m_CacheName);
    node.SetByKey("ICache", icache);
}

CJsonNode SNetStorageRPC::MkStdRequest(const string& request_type)
{
    CJsonNode new_request(CJsonNode::NewObjectNode());

    new_request.SetString("Type", request_type);
    new_request.SetInteger("SN", (Int8) m_RequestNumber.Add(1));

    CRequestContext& req = CDiagContext::GetRequestContext();

    if (req.IsSetClientIP()) {
        new_request.SetString("ClientIP", req.GetClientIP());
        new_request.SetString("SessionID", req.IsSetSessionID() ?
                req.GetSessionID() : "UNKNOWN");
    }

    return new_request;
}

CJsonNode SNetStorageRPC::MkFileRequest(const string& request_type,
        const string& object_loc)
{
    CJsonNode new_request(MkStdRequest(request_type));

    new_request.SetString("ObjectLoc", object_loc);

    return new_request;
}

CJsonNode SNetStorageRPC::MkFileRequest(const string& request_type,
        const string& unique_key, TNetStorageFlags flags)
{
    CJsonNode new_request(MkStdRequest(request_type));

    CJsonNode user_key(CJsonNode::NewObjectNode());
    user_key.SetString("AppDomain", m_AppDomain);
    user_key.SetString("UniqueID", unique_key);
    new_request.SetByKey("UserKey", user_key);

    x_SetStorageFlags(new_request, flags);
    x_SetICacheNames(new_request);

    return new_request;
}

CNetStorage::CNetStorage(const string& init_string,
        TNetStorageFlags default_flags) :
    m_Impl(new SNetStorageRPC(init_string, default_flags))
{
}

string SNetStorageObjectRPC::GetID()
{
    return m_ID;
}

size_t SNetStorageObjectRPC::Read(void* buffer, size_t buf_size)
{
    if (m_State == eWriting) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Cannot read a NetStorage file while writing to it");
    }

    size_t bytes_read;

    if (m_State == eReady) {
        m_OriginalRequest = x_MkRequest("READ");

        if (m_ReadBuffer == NULL)
            m_ReadBuffer = new char[READ_BUFFER_SIZE];

        CNetServer server(*m_NetStorageRPC->m_Service.Iterate(
                CNetService::eRandomize));

        CJsonOverUTTPExecHandler json_over_uttp_sender(m_OriginalRequest);

        server->TryExec(json_over_uttp_sender);

        m_Connection = json_over_uttp_sender.GetConnection();

        CSocket* sock = &json_over_uttp_sender.GetConnection()->m_Socket;

        CJsonOverUTTPReader json_reader;

        do {
            s_ReadSocket(sock, m_ReadBuffer, READ_BUFFER_SIZE, &bytes_read);

            m_UTTPReader.SetNewBuffer(m_ReadBuffer, bytes_read);

        } while (!json_reader.ReadMessage(m_UTTPReader));

        s_TrapErrors(m_OriginalRequest, json_reader.GetMessage(), sock);

        m_CurrentChunkSize = 0;

        m_State = eReading;
        m_EOF = false;
    }

    char* buf_pos = reinterpret_cast<char*>(buffer);

    if (m_CurrentChunkSize >= buf_size) {
        if (buf_size > 0) {
            memcpy(buf_pos, m_CurrentChunk, buf_size);
            m_CurrentChunk += buf_size;
            m_CurrentChunkSize -= buf_size;
        }
        return buf_size;
    }

    if (m_CurrentChunkSize > 0) {
        memcpy(buf_pos, m_CurrentChunk, m_CurrentChunkSize);
        buf_pos += m_CurrentChunkSize;
        buf_size -= m_CurrentChunkSize;
    }

    size_t bytes_copied = m_CurrentChunkSize;

    m_CurrentChunkSize = 0;

    if (m_EOF)
        return bytes_copied;

    while (buf_size > 0) {
        switch (m_UTTPReader.GetNextEvent()) {
        case CUTTPReader::eChunkPart:
        case CUTTPReader::eChunk:
            m_CurrentChunk = m_UTTPReader.GetChunkPart();
            m_CurrentChunkSize = m_UTTPReader.GetChunkPartSize();

            if (m_CurrentChunkSize >= buf_size) {
                memcpy(buf_pos, m_CurrentChunk, buf_size);
                m_CurrentChunk += buf_size;
                m_CurrentChunkSize -= buf_size;
                return bytes_copied + buf_size;
            }

            memcpy(buf_pos, m_CurrentChunk, m_CurrentChunkSize);
            buf_pos += m_CurrentChunkSize;
            buf_size -= m_CurrentChunkSize;
            bytes_copied += m_CurrentChunkSize;
            m_CurrentChunkSize = 0;
            break;

        case CUTTPReader::eControlSymbol:
            if (m_UTTPReader.GetControlSymbol() != END_OF_DATA_MARKER) {
                NCBI_THROW_FMT(CNetStorageException, eIOError,
                        "NetStorage API: invalid end-of-data-stream "
                        "terminator: " <<
                                (int) m_UTTPReader.GetControlSymbol());
            }
            m_EOF = true;
            return bytes_copied;

        case CUTTPReader::eEndOfBuffer:
            s_ReadSocket(&m_Connection->m_Socket, m_ReadBuffer,
                    READ_BUFFER_SIZE, &bytes_read);

            m_UTTPReader.SetNewBuffer(m_ReadBuffer, bytes_read);
            break;

        default:
            NCBI_THROW_FMT(CNetStorageException, eIOError,
                    "NetStorage API: invalid UTTP status "
                    "while reading " << m_ID);
        }
    }

    return bytes_copied;
}

bool SNetStorageObjectRPC::Eof()
{
    switch (m_State) {
    case eReady:
        return false;

    case eReading:
        return m_CurrentChunkSize == 0 && m_EOF;

    default: /* case eWriting: */
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Cannot read a NetStorage file while writing");
    }
}

void SNetStorageObjectRPC::Write(const void* buf_pos, size_t buf_size)
{
    switch (m_State) {
    case eReady:
        {
            m_OriginalRequest = x_MkRequest("WRITE");

            m_ID = m_NetStorageRPC->Exchange(m_OriginalRequest,
                    &m_Connection).GetString("ObjectLoc");

            m_State = eWriting;
        }
        /* FALL THROUGH */

    case eWriting:
        s_SendUTTPChunk(reinterpret_cast<const char*>(buf_pos),
                buf_size, &m_Connection->m_Socket);
        break;

    default: /* case eReading: */
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Cannot write a NetStorage file while reading");
    }
}

Uint8 SNetStorageObjectRPC::GetSize()
{
    CJsonNode request(x_MkRequest("GETSIZE"));

    return (Uint8) m_NetStorageRPC->Exchange(request).GetInteger("Size");
}

string SNetStorageObjectRPC::GetAttribute(const string& attr_name)
{
    CJsonNode request(x_MkRequest("GETATTR"));

    request.SetString("AttrName", attr_name);

    return m_NetStorageRPC->Exchange(request).GetString("AttrValue");
}

void SNetStorageObjectRPC::SetAttribute(const string& attr_name,
        const string& attr_value)
{
    CJsonNode request(x_MkRequest("SETATTR"));

    request.SetString("AttrName", attr_name);
    request.SetString("AttrValue", attr_value);

    m_NetStorageRPC->Exchange(request);
}

CNetStorageObjectInfo SNetStorageObjectRPC::GetInfo()
{
    CJsonNode request(x_MkRequest("GETOBJECTINFO"));

    return CNetStorageObjectInfo(m_ID, m_NetStorageRPC->Exchange(request));
}

void SNetStorageObjectRPC::Close()
{
    switch (m_State) {
    case eReading:
        if (!m_EOF)
            m_Connection->Close();
        else {
            CSocket* sock = &m_Connection->m_Socket;

            CJsonOverUTTPReader json_reader;
            size_t bytes_read;

            while (!json_reader.ReadMessage(m_UTTPReader)) {
                s_ReadSocket(sock, m_ReadBuffer, READ_BUFFER_SIZE, &bytes_read);

                m_UTTPReader.SetNewBuffer(m_ReadBuffer, bytes_read);
            }

            if (m_UTTPReader.GetNextEvent() != CUTTPReader::eEndOfBuffer) {
                    NCBI_THROW_FMT(CNetStorageException, eIOError,
                            "Extra bytes past confirmation message "
                            "while reading " << m_ID << " from " <<
                            sock->GetPeerAddress() << '.');
            }

            s_TrapErrors(m_OriginalRequest, json_reader.GetMessage(), sock);
        }
        m_Connection = NULL;
        m_State = eReady;
        break;

    case eWriting:
        {
            s_SendEndOfData(&m_Connection->m_Socket);

            CReadJsonFromSocket message_reader;

            CSocket* sock = &m_Connection->m_Socket;

            s_TrapErrors(m_OriginalRequest,
                    message_reader.ReadMessage(sock), sock);

            m_Connection = NULL;
            m_State = eReady;
        }
        break;

    default:
        break;
    }
}

CJsonNode SNetStorageObjectRPC::x_MkRequest(const string& request_type)
{
    if (m_ObjectIdentification == eByGeneratedID)
        return m_NetStorageRPC->MkFileRequest(request_type, m_ID);
    else
        return m_NetStorageRPC->MkFileRequest(request_type,
                m_UniqueKey, m_Flags);
}

void SNetStorageObjectImpl::Read(string* data)
{
    data->resize(data->capacity() == 0 ? READ_CHUNK_SIZE : data->capacity());

    size_t bytes_read = Read(const_cast<char*>(data->data()), data->capacity());

    data->resize(bytes_read);

    if (bytes_read < data->capacity())
        return;

    vector<string> chunks(1, *data);

    while (!Eof()) {
        string buffer(READ_CHUNK_SIZE, '\0');

        bytes_read = Read(const_cast<char*>(buffer.data()), buffer.length());

        if (bytes_read < buffer.length()) {
            buffer.resize(bytes_read);
            chunks.push_back(buffer);
            break;
        }

        chunks.push_back(buffer);
    }

    Close();

    *data = NStr::Join(chunks, kEmptyStr);
}

struct SNetStorageByKeyRPC : public SNetStorageByKeyImpl
{
    SNetStorageByKeyRPC(const string& init_string,
            TNetStorageFlags default_flags);

    virtual CNetStorageObject Open(const string& unique_key,
            TNetStorageFlags flags = 0);
    virtual string Relocate(const string& unique_key,
            TNetStorageFlags flags, TNetStorageFlags old_flags = 0);
    virtual bool Exists(const string& key, TNetStorageFlags flags = 0);
    virtual void Remove(const string& key, TNetStorageFlags flags = 0);

    CRef<SNetStorageRPC,
            CNetComponentCounterLocker<SNetStorageRPC> > m_NetStorageRPC;
};

SNetStorageByKeyRPC::SNetStorageByKeyRPC(const string& init_string,
        TNetStorageFlags default_flags) :
    m_NetStorageRPC(new SNetStorageRPC(init_string, default_flags))
{
    if (m_NetStorageRPC->m_AppDomain.empty()) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "'domain' parameter is missing from the initialization string");
    }
}

CNetStorageObject SNetStorageByKeyRPC::Open(const string& unique_key,
        TNetStorageFlags flags)
{
    return new SNetStorageObjectRPC(m_NetStorageRPC, NULL, NULL,
            SNetStorageObjectRPC::eByUniqueKey,
            unique_key, flags, SNetStorageObjectRPC::eReady);
}

string SNetStorageByKeyRPC::Relocate(const string& unique_key,
        TNetStorageFlags flags, TNetStorageFlags old_flags)
{
    CJsonNode request(m_NetStorageRPC->MkFileRequest(
            "RELOCATE", unique_key, old_flags));

    CJsonNode new_location(CJsonNode::NewObjectNode());

    m_NetStorageRPC->x_SetStorageFlags(new_location, flags);

    request.SetByKey("NewLocation", new_location);

    return m_NetStorageRPC->Exchange(request).GetString("ObjectLoc");
}

bool SNetStorageByKeyRPC::Exists(const string& key, TNetStorageFlags flags)
{
    CJsonNode request(m_NetStorageRPC->MkFileRequest("EXISTS", key, flags));

    return m_NetStorageRPC->Exchange(request).GetBoolean("Exists");
}

void SNetStorageByKeyRPC::Remove(const string& key, TNetStorageFlags flags)
{
    CJsonNode request(m_NetStorageRPC->MkFileRequest("DELETE", key, flags));

    m_NetStorageRPC->Exchange(request);
}

CNetStorageByKey::CNetStorageByKey(const string& init_string,
        TNetStorageFlags default_flags) :
    m_Impl(new SNetStorageByKeyRPC(init_string, default_flags))
{
}

struct SNetStorageAdminImpl : public CObject
{
    SNetStorageAdminImpl(CNetStorage::TInstance netstorage_impl) :
        m_NetStorageRPC(static_cast<SNetStorageRPC*>(netstorage_impl))
    {
    }

    CRef<SNetStorageRPC,
            CNetComponentCounterLocker<SNetStorageRPC> > m_NetStorageRPC;
};

CNetStorageAdmin::CNetStorageAdmin(CNetStorage::TInstance netstorage_impl) :
    m_Impl(new SNetStorageAdminImpl(netstorage_impl))
{
}

CNetService CNetStorageAdmin::GetService()
{
    return m_Impl->m_NetStorageRPC->m_Service;
}

CJsonNode CNetStorageAdmin::MkNetStorageRequest(const string& request_type)
{
    return m_Impl->m_NetStorageRPC->MkStdRequest(request_type);
}

CJsonNode CNetStorageAdmin::ExchangeJson(const CJsonNode& request,
        CNetServer::TInstance server_to_use, CNetServerConnection* conn)
{
    return m_Impl->m_NetStorageRPC->Exchange(request, conn, server_to_use);
}

END_NCBI_SCOPE
