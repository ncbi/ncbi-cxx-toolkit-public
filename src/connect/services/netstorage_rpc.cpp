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

#include <connect/services/netstorage.hpp>
#include <connect/services/error_codes.hpp>

#include <util/ncbi_url.hpp>

#include <corelib/request_ctx.hpp>

#define NCBI_USE_ERRCODE_X  NetStorage_RPC

#define READ_CHUNK_SIZE (4 * 1024)

#define WRITE_BUFFER_SIZE (64 * 1024)
#define READ_BUFFER_SIZE (64 * 1024)

BEGIN_NCBI_SCOPE

const char* CNetStorageException::GetErrCodeString() const
{
    switch (GetErrCode()) {
    case eInvalidArg:
        return "eInvalidArg";
    case eNotExists:
        return "eNotExist";
    case eIOError:
        return "eIOError";
    case eTimeout:
        return "eTimeout";
    default:
        return CException::GetErrCodeString();
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

static void s_SendUTTPChunk(const char* chunk, size_t chunk_size,
        bool to_be_continued, CSocket* sock)
{
    CUTTPWriter uttp_writer;

    char write_buffer[WRITE_BUFFER_SIZE];

    uttp_writer.Reset(write_buffer, WRITE_BUFFER_SIZE);

    uttp_writer.SendChunk(chunk, chunk_size, to_be_continued);

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
                        sock->GetPeerAddress() << '.');
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

    Int8 request_sn = request.GetInteger("SN");
    Int8 reply_sn = reply.GetInteger("RE");

    if (reply_sn != request_sn) {
        NCBI_THROW_FMT(CNetStorageException, eServerError,
                "Serial number mismatch in NetStorage command " <<
                        request.GetString("Type") << " "
                "(server: " << sock->GetPeerAddress() << "; "
                "SN: " << request_sn << "; "
                "RE: " << reply_sn << ").");
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

    virtual CNetFile Create(TNetStorageFlags flags = 0);
    virtual CNetFile Open(const string& file_id, TNetStorageFlags flags = 0);
    virtual string Relocate(const string& file_id, TNetStorageFlags flags);
    virtual bool Exists(const string& file_id);
    virtual void Remove(const string& file_id);

    CJsonNode Exchange(const CJsonNode& request,
            CNetServerConnection* conn = NULL);

    void x_LoadStorageFlags(CJsonNode& node, TNetStorageFlags flags);
    CJsonNode MkStdRequest(const string& request_type);
    CJsonNode MkFileRequest(const string& request_type, const string& file_id);

    TNetStorageFlags m_DefaultFlags;
    CNetService m_Service;

    string m_NetStorageServiceName;
    string m_NetCacheServiceName;
    string m_CacheName;
    string m_ClientName;

    CAtomicCounter m_RequestNumber;
};

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

struct SNetFileRPC : public SNetFileImpl
{
    enum EState {
        eReady,
        eReading,
        eWriting
    };

    SNetFileRPC(SNetStorageRPC* netstorage_rpc,
            CJsonNode::TInstance original_request,
            CNetServerConnection::TInstance conn,
            const string& file_id, TNetStorageFlags flags,
            EState initial_state);

    virtual ~SNetFileRPC();

    virtual string GetID();
    virtual size_t Read(void* buffer, size_t buf_size);
    virtual bool Eof();
    virtual void Write(const void* buffer, size_t buf_size);
    virtual Uint8 GetSize();
    virtual CNetFileInfo GetInfo();
    virtual void Close();

    CRef<SNetStorageRPC,
            CNetComponentCounterLocker<SNetStorageRPC> > m_NetStorageRPC;
    CJsonNode m_OriginalRequest;
    CNetServerConnection m_Connection;
    char* m_ReadBuffer;
    CUTTPReader m_UTTPReader;
    const char* m_CurrentChunk;
    size_t m_CurrentChunkSize;
    string m_ID;
    TNetStorageFlags m_Flags;
    EState m_State;
    bool m_EOF;
};

SNetFileRPC::SNetFileRPC(SNetStorageRPC* netstorage_rpc,
        CJsonNode::TInstance original_request,
        CNetServerConnection::TInstance conn,
        const string& file_id, TNetStorageFlags flags,
        EState initial_state) :
    m_NetStorageRPC(netstorage_rpc),
    m_OriginalRequest(original_request),
    m_Connection(conn),
    m_ReadBuffer(NULL),
    m_ID(file_id),
    m_Flags(flags),
    m_State(initial_state)
{
}

SNetFileRPC::~SNetFileRPC()
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

    m_Service = new SNetServiceImpl("NetStorageAPI", m_ClientName,
            new CNetStorageServerListener(hello));

    m_Service->Init(this, m_NetStorageServiceName,
            NULL, kEmptyStr, s_NetStorageConfigSections);

    if (m_Service->m_ServiceType == CNetService::eServiceNotDefined) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "NetStorage service name is not defined");
    }
}

CNetFile SNetStorageRPC::Create(TNetStorageFlags flags)
{
    CJsonNode request(MkStdRequest("WRITE"));

    x_LoadStorageFlags(request, flags);

    CNetServerConnection conn;

    string file_id = Exchange(request, &conn).GetString("FileID");

    return new SNetFileRPC(this, request, conn,
            file_id, flags, SNetFileRPC::eWriting);
}

CNetFile SNetStorageRPC::Open(const string& file_id,
        TNetStorageFlags flags)
{
    return new SNetFileRPC(this, NULL, NULL,
            file_id, flags, SNetFileRPC::eReady);
}

string SNetStorageRPC::Relocate(const string& file_id, TNetStorageFlags flags)
{
    CJsonNode request(MkFileRequest("RELOCATE", file_id));

    CJsonNode new_location(CJsonNode::NewObjectNode());

    x_LoadStorageFlags(new_location, flags);

    request.SetByKey("NewLocation", new_location);

    return Exchange(request).GetString("FileID");
}

bool SNetStorageRPC::Exists(const string& file_id)
{
    CJsonNode request(MkFileRequest("EXISTS", file_id));

    return Exchange(request).GetBoolean("Exists");
}

void SNetStorageRPC::Remove(const string& file_id)
{
    CJsonNode request(MkFileRequest("DELETE", file_id));

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
        CNetServerConnection* conn)
{
    CNetServer server(*m_Service.Iterate(CNetService::eRandomize));

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

void SNetStorageRPC::x_LoadStorageFlags(CJsonNode& node, TNetStorageFlags flags)
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

    node.SetByKey("StorageFlags", storage_flags);
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
        const string& file_id)
{
    CJsonNode new_request(MkStdRequest(request_type));

    new_request.SetString("FileID", file_id);

    return new_request;
}

CNetStorage::CNetStorage(const string& init_string,
        TNetStorageFlags default_flags) :
    m_Impl(new SNetStorageRPC(init_string, default_flags))
{
}

string SNetFileRPC::GetID()
{
    return m_ID;
}

size_t SNetFileRPC::Read(void* buffer, size_t buf_size)
{
    if (m_State == eWriting) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Cannot read a NetStorage file while writing");
    }

    size_t bytes_read;

    if (m_State == eReady) {
        m_OriginalRequest = m_NetStorageRPC->MkFileRequest("READ", m_ID);

        m_NetStorageRPC->Exchange(m_OriginalRequest, &m_Connection);

        if (m_ReadBuffer == NULL)
            m_ReadBuffer = new char[READ_BUFFER_SIZE];

        s_ReadSocket(&m_Connection->m_Socket, m_ReadBuffer,
                READ_BUFFER_SIZE, &bytes_read);

        m_UTTPReader.SetNewBuffer(m_ReadBuffer, bytes_read);

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
        case CUTTPReader::eChunk:
            m_EOF = true;
            /* FALL THROUGH */

        case CUTTPReader::eChunkPart:
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

            if (m_EOF)
                return bytes_copied;

            break;

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

bool SNetFileRPC::Eof()
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

void SNetFileRPC::Write(const void* buf_pos, size_t buf_size)
{
    switch (m_State) {
    case eReady:
        {
            m_OriginalRequest = m_NetStorageRPC->MkFileRequest("WRITE", m_ID);

            m_ID = m_NetStorageRPC->Exchange(m_OriginalRequest,
                    &m_Connection).GetString("FileID");

            m_State = eWriting;
        }

    case eWriting:
        s_SendUTTPChunk(reinterpret_cast<const char*>(buf_pos),
                buf_size, true, &m_Connection->m_Socket);
        break;

    default: /* case eReading: */
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
            "Cannot write a NetStorage file while reading");
    }
}

Uint8 SNetFileRPC::GetSize()
{
    CJsonNode request(m_NetStorageRPC->MkFileRequest("GETSIZE", m_ID));

    return (Uint8) m_NetStorageRPC->Exchange(request).GetInteger("Size");
}

CNetFileInfo SNetFileRPC::GetInfo()
{
    CJsonNode request(m_NetStorageRPC->MkFileRequest("GETOBJECTINFO", m_ID));

    return CNetFileInfo(m_NetStorageRPC->Exchange(request));
}

void SNetFileRPC::Close()
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
            s_SendUTTPChunk("", 0, false, &m_Connection->m_Socket);

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

void SNetFileImpl::Read(string* data)
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

    *data = NStr::Join(chunks, kEmptyStr);
}

struct SNetStorageByKeyRPC : public SNetStorageByKeyImpl
{
    SNetStorageByKeyRPC(const string& init_string,
            TNetStorageFlags default_flags)
    {
    }

    virtual CNetFile Open(const string& unique_key,
            TNetStorageFlags flags = 0);
    virtual string Relocate(const string& unique_key,
            TNetStorageFlags flags, TNetStorageFlags old_flags = 0);
    virtual bool Exists(const string& key, TNetStorageFlags flags = 0);
    virtual void Remove(const string& key, TNetStorageFlags flags = 0);
};


CNetFile SNetStorageByKeyRPC::Open(const string& /*unique_key*/,
        TNetStorageFlags /*flags*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

string SNetStorageByKeyRPC::Relocate(const string& /*unique_key*/,
        TNetStorageFlags /*flags*/, TNetStorageFlags /*old_flags*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

bool SNetStorageByKeyRPC::Exists(const string& /*key*/,
        TNetStorageFlags /*flags*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

void SNetStorageByKeyRPC::Remove(const string& /*key*/,
        TNetStorageFlags /*flags*/)
{
    NCBI_THROW_FMT(CNetStorageException, eInvalidArg, "Method not implemented");
}

END_NCBI_SCOPE
