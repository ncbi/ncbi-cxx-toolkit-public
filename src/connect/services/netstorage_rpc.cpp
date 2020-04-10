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
#include "netstorage_direct_nc.hpp"

#include <connect/services/error_codes.hpp>

#include <util/ncbi_url.hpp>

#include <corelib/request_ctx.hpp>

#include <sstream>
#include <array>

#define NCBI_USE_ERRCODE_X  NetStorage_RPC

#define NST_PROTOCOL_VERSION "1.0.0"

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
    case eExpired:              return "eExpired";
    case eNotSupported:         return "eNotSupported";
    case eInterrupted:          return "eInterrupted";
    case eUnknown:              return "eUnknown";
    default:                    return CException::GetErrCodeString();
    }
}

static void s_WriteToSocket(CSocket& sock,
        const char* output_buffer, size_t output_buffer_size)
{
    size_t bytes_written;

    while (output_buffer_size > 0) {
        EIO_Status  status = sock.Write(output_buffer,
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
                            sock.GetPeerAddress() << ". "
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
    CSendJsonOverSocket(CSocket& sock) : m_JSONWriter(m_UTTPWriter), m_Socket(sock) {}

    void SendMessage(const CJsonNode& message);

private:
    void x_SendOutputBuffer();

    CUTTPWriter m_UTTPWriter;
    CJsonOverUTTPWriter m_JSONWriter;
    CSocket& m_Socket;
};

void CSendJsonOverSocket::SendMessage(const CJsonNode& message)
{
    char write_buffer[WRITE_BUFFER_SIZE];

    m_UTTPWriter.Reset(write_buffer, WRITE_BUFFER_SIZE);

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

void s_SendUTTP(CSocket& sock, function<void(CUTTPWriter&)> f)
{
    CUTTPWriter uttp_writer;

    char write_buffer[WRITE_BUFFER_SIZE];

    uttp_writer.Reset(write_buffer, WRITE_BUFFER_SIZE);

    f(uttp_writer);

    const char* output_buffer;
    size_t output_buffer_size;

    do {
        uttp_writer.GetOutputBuffer(&output_buffer, &output_buffer_size);
        s_WriteToSocket(sock, output_buffer, output_buffer_size);
    } while (uttp_writer.NextOutputBuffer());

    uttp_writer.GetOutputBuffer(&output_buffer, &output_buffer_size);
    s_WriteToSocket(sock, output_buffer, output_buffer_size);
}

template <class TContiguousContainer>
void s_ReadSocket(CSocket& sock, TContiguousContainer& buffer, CUTTPReader& uttp_reader)
{
    EIO_Status status;
    size_t bytes_read;

    do {
        status = sock.Read(buffer.data(), buffer.size(), &bytes_read);
    } while (status == eIO_Interrupt);

    if (status != eIO_Success) {
        // eIO_Timeout, eIO_Closed, eIO_InvalidArg,
        // eIO_NotSupported, eIO_Unknown
        NCBI_THROW_FMT(CNetStorageException, eIOError,
                "I/O error while reading from NetStorage server " <<
                        sock.GetPeerAddress() << ". "
                "Socket status: " << IO_StatusStr(status) << '.');
    }

    uttp_reader.SetNewBuffer(buffer.data(), bytes_read);
}

struct SIssue
{
    struct SBuilder;

    static const Int8 kEmptySubCode = -1;

    Int8 code;
    string message;
    string scope;
    Int8 sub_code;

    SIssue(SBuilder builder) :
        code(builder.GetCode()),
        message(builder.GetMessage()),
        scope(builder.GetScope()),
        sub_code(builder.GetSubCode())
    {}

    template <class TOstream>
    TOstream& Print(TOstream& os) const
    {
        if (!scope.empty()) os << scope << "::";
        os << code;
        if (sub_code) os << '.' << sub_code;
        return os << " (" << message << ')';
    }

    operator string() const
    {
        ostringstream oss;
        Print<ostream>(oss);
        return oss.str();
    }

    struct SBuilder
    {
        SBuilder(const CJsonNode& node) :
            m_Node(node)
        {
            _ASSERT(m_Node);
        }

        Int8 GetCode() const
        {
            return m_Node.GetInteger("Code");
        }

        string GetMessage() const
        {
            return m_Node.GetString("Message");
        }

        string GetScope() const
        {
            if (CJsonNode scope = m_Node.GetByKeyOrNull("Scope")) {
                return scope.AsString();
            } else {
                return string();
            }
        }

        Int8 GetSubCode() const
        {
            if (CJsonNode sub_code = m_Node.GetByKeyOrNull("SubCode")) {
                return sub_code.AsInteger();
            } else {
                return kEmptySubCode;
            }
        }

    private:
        CJsonNode m_Node;
    };

};

// Clang does not like templated operator (sees ambiguity with CNcbiDiag's one).
const CNcbiDiag& operator<< (const CNcbiDiag& diag, const SIssue& issue)
{
    return issue.Print(diag);
}

ostream& operator<< (ostream& os, const SIssue& issue)
{
    return issue.Print(os);
}

void s_ThrowError(Int8 code, Int8 sub_code, const string& err_msg)
{
    switch (code) {
    case 3010:
        throw CNetServiceException(DIAG_COMPILE_INFO, 0,
                static_cast<CNetServiceException::EErrCode>(sub_code), err_msg);
    case 3020:
        throw CNetStorageException(DIAG_COMPILE_INFO, 0,
                static_cast<CNetStorageException::EErrCode>(sub_code), err_msg);
    }

    switch (sub_code) {
    case CNetStorageServerError::eNetStorageObjectNotFound:
    case CNetStorageServerError::eRemoteObjectNotFound:
        NCBI_THROW(CNetStorageException, eNotExists, err_msg);

    case CNetStorageServerError::eNetStorageObjectExpired:
        NCBI_THROW(CNetStorageException, eExpired, err_msg);
    }

    NCBI_THROW(CNetStorageException, eServerError, err_msg);
}

void s_TrapErrors(const CJsonNode& request, const CJsonNode& reply, CNetServerConnection& conn,
        SNetStorage::SConfig::EErrMode err_mode, INetServerConnectionListener& listener)
{
    CSocket& sock(conn->m_Socket);
    CNetServer& server(conn->m_Server);
    const string server_address(sock.GetPeerAddress());
    CJsonNode issues(reply.GetByKeyOrNull("Warnings"));

    if (issues) {
        for (CJsonIterator it = issues.Iterate(); it; ++it) {
            const SIssue issue(*it);
            listener.OnWarning(issue, server);
        }
    }

    const string status = reply.GetString("Status");
    const bool status_ok = status == "OK";
    issues = reply.GetByKeyOrNull("Errors");

    // Got errors
    if (!status_ok || issues) {
        if (status_ok && err_mode != SNetStorage::SConfig::eThrow) {
            if (err_mode == SNetStorage::SConfig::eLog) {
                for (CJsonIterator it = issues.Iterate(); it; ++it) {
                    const SIssue issue(*it);
                    listener.OnError(issue, server);
                }
            }
        } else {
            Int8 code = CNetStorageServerError::eUnknownError;
            Int8 sub_code = SIssue::kEmptySubCode;
            ostringstream errors;

            if (!issues)
                errors << status;
            else {
                const char* prefix = "error ";

                for (CJsonIterator it = issues.Iterate(); it; ++it) {
                    const SIssue issue(*it);
                    code = issue.code;
                    sub_code = issue.sub_code;
                    errors << prefix << issue;
                    prefix = ", error ";
                }
            }

            string err_msg = FORMAT("Error while executing " <<
                            request.GetString("Type") << " "
                    "on NetStorage server " <<
                            sock.GetPeerAddress() << ". "
                    "Server returned " << errors.str());

            s_ThrowError(code, sub_code, err_msg);
        }
    }

    if (reply.GetInteger("RE") != request.GetInteger("SN")) {
        NCBI_THROW_FMT(CNetStorageException, eServerError,
                "Message serial number mismatch "
                "(NetStorage server: " << sock.GetPeerAddress() << "; "
                "request: " << request.Repr() << "; "
                "reply: " << reply.Repr() << ").");
    }
}

CJsonNode s_ReadMessage(const CJsonNode& request, CNetServerConnection& conn, SNetStorage::SConfig::EErrMode err_mode,
        INetServerConnectionListener& listener)
{
    CSocket& sock(conn->m_Socket);
    CUTTPReader uttp_reader;
    CJsonOverUTTPReader json_reader;

    try {
        array<char, READ_BUFFER_SIZE> read_buffer;

        do {
            s_ReadSocket(sock, read_buffer, uttp_reader);
        } while (!json_reader.ReadMessage(uttp_reader));
    }
    catch (...) {
        sock.Close();
        throw;
    }

    if (uttp_reader.GetNextEvent() != CUTTPReader::eEndOfBuffer) {
        string server_address(sock.GetPeerAddress());
        sock.Close();
        NCBI_THROW_FMT(CNetStorageException, eIOError,
                "Extra bytes past message end while reading from " <<
                        server_address << " after receiving " <<
                        json_reader.GetMessage().Repr() << '.');
    }

    CJsonNode reply(json_reader.GetMessage());
    s_TrapErrors(request, reply, conn, err_mode, listener);
    return reply;
}

void s_SetStorageFlags(CJsonNode& node, TNetStorageFlags flags)
{
    CJsonNode storage_flags(CJsonNode::NewObjectNode());

    if (flags & fNST_Fast)
        storage_flags.SetBoolean("Fast", true);
    if (flags & fNST_Persistent)
        storage_flags.SetBoolean("Persistent", true);
    if (flags & fNST_NetCache)
        storage_flags.SetBoolean("NetCache", true);
    if (flags & fNST_FileTrack)
        storage_flags.SetBoolean("FileTrack", true);
    if (flags & fNST_Movable)
        storage_flags.SetBoolean("Movable", true);
    if (flags & fNST_Cacheable)
        storage_flags.SetBoolean("Cacheable", true);
    if (flags & fNST_NoMetaData)
        storage_flags.SetBoolean("NoMetaData", true);

    node.SetByKey("StorageFlags", storage_flags);
}

class CNetStorageServerListener : public INetServerConnectionListener
{
public:
    CNetStorageServerListener(const CJsonNode& hello, SNetStorage::SConfig::EErrMode err_mode) :
        m_Hello(hello), m_ErrMode(err_mode)
    {
    }

    INetServerConnectionListener* Clone() override;

    void OnConnected(CNetServerConnection& connection) override;
 
private:
    void OnErrorImpl(const string& err_msg, CNetServer& server) override;
    void OnWarningImpl(const string& warn_msg, CNetServer& server) override;

    const CJsonNode m_Hello;
    const SNetStorage::SConfig::EErrMode m_ErrMode;
};

INetServerConnectionListener* CNetStorageServerListener::Clone()
{
    return new CNetStorageServerListener(*this);
}

void CNetStorageServerListener::OnConnected(
        CNetServerConnection& connection)
{
    CSendJsonOverSocket message_sender(connection->m_Socket);

    message_sender.SendMessage(m_Hello);

    s_ReadMessage(m_Hello, connection, m_ErrMode, *this);
}

void CNetStorageServerListener::OnErrorImpl(const string& err_msg,
        CNetServer& server)
{
    ERR_POST("NetStorage server " <<
            server->m_ServerInPool->m_Address.AsString() <<
            " issued error " << err_msg);
}

void CNetStorageServerListener::OnWarningImpl(const string& warn_msg,
        CNetServer& server)
{
        ERR_POST(Warning << "NetStorage server " <<
                server->m_ServerInPool->m_Address.AsString() <<
                " issued warning " << warn_msg);
}

class CJsonOverUTTPExecHandler : public INetServerExecHandler
{
public:
    CJsonOverUTTPExecHandler(const CJsonNode& request) : m_Request(request) {}

    virtual void Exec(CNetServerConnection::TInstance conn_impl,
            const STimeout* timeout);

    CNetServerConnection GetConnection() const {return m_Connection;}

private:
    CJsonNode m_Request;

    CNetServerConnection m_Connection;
};

void CJsonOverUTTPExecHandler::Exec(CNetServerConnection::TInstance conn_impl,
        const STimeout* timeout)
{
    CTimeoutKeeper timeout_keeper(&conn_impl->m_Socket, timeout);
    CSendJsonOverSocket sender(conn_impl->m_Socket);

    sender.SendMessage(m_Request);

    m_Connection = conn_impl;
}

struct SNetStorageObjectRPC : public INetStorageObjectState
{
private:
    struct SContext
    {
        string locator;
        const SNetStorage::SConfig::EErrMode m_ErrMode;
        CRef<INetServerConnectionListener> m_Listener;
        mutable CJsonNode m_OriginalRequest;
        mutable CNetServerConnection m_Connection;

        SContext(SNetStorageRPC* netstorage_rpc, const string& object_loc) :
            locator(object_loc),
            m_ErrMode(netstorage_rpc->m_Config.err_mode),
            m_Listener(netstorage_rpc->m_Service->m_Listener)
        {
        }

        void TrapErrors(const CJsonNode& reply)
        {
            s_TrapErrors(m_OriginalRequest, reply, m_Connection, m_ErrMode, *m_Listener);
        }

        CJsonNode ReadMessage()
        {
            return s_ReadMessage(m_OriginalRequest, m_Connection, m_ErrMode, *m_Listener);
        }
    };

    struct SIState : public SNetStorageObjectIState
    {
        ERW_Result Read(void* buf, size_t count, size_t* read) override;
        ERW_Result PendingCount(size_t* count) override;
        bool Eof() override;

        void Close() override;
        void Abort() override;

        string GetLoc() const override { return m_Context.locator; }

        void StartReading();

    protected:
        SIState(SContext& context) : m_Context(context) {}

    private:
        void ReadConfirmation();
        void ReadSocket() { s_ReadSocket(m_Context.m_Connection->m_Socket, m_ReadBuffer, m_UTTPReader); }

        SContext& m_Context;
        vector<char> m_ReadBuffer;
        CUTTPReader m_UTTPReader;
        const char* m_CurrentChunk;
        size_t m_CurrentChunkSize;
        bool m_EOF;
    };

    struct SOState : public SNetStorageObjectOState
    {
        ERW_Result Write(const void* buf, size_t count, size_t* written) override;
        ERW_Result Flush() override;

        void Close() override;
        void Abort() override;

        string GetLoc() const override { return m_Context.locator; }

    protected:
        SOState(SContext& context) : m_Context(context) {}

    private:
        SContext& m_Context;
    };

public:
    typedef function<CJsonNode(const string&, const string&)> TBuilder;

    SNetStorageObjectRPC(SNetStorageObjectImpl& fsm, SNetStorageRPC* netstorage_rpc, CNetService service, TBuilder builder,
            const string& object_loc);

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read) override;
    ERW_Result PendingCount(size_t* count) override;

    ERW_Result Write(const void* buf, size_t count, size_t* bytes_written) override;
    ERW_Result Flush() override;
    void Close() override;
    void Abort() override;

    string GetLoc() const override { return m_Context.locator; }
    bool Eof() override;
    Uint8 GetSize() override;
    list<string> GetAttributeList() const override;
    string GetAttribute(const string& attr_name) const override;
    void SetAttribute(const string& attr_name, const string& attr_value) override;
    CNetStorageObjectInfo GetInfo() override;
    void SetExpiration(const CTimeout&) override;

    string FileTrack_Path() override;
    string Relocate(TNetStorageFlags flags, TNetStorageProgressCb cb) override;
    bool Exists() override;
    ENetStorageRemoveResult Remove() override;

    void StartWriting(CJsonNode::TInstance request, CNetServerConnection::TInstance conn);

private:
    CJsonNode Exchange() const
    {
        return m_NetStorageRPC->Exchange(m_OwnService, m_Context.m_OriginalRequest, &m_Context.m_Connection);
    }

    void MkRequest(const string& request_type) const
    {
        m_Context.m_OriginalRequest = m_Builder(request_type, m_Context.locator);
    }

    CNetRef<SNetStorageRPC> m_NetStorageRPC;
    CNetService m_OwnService;
    TBuilder m_Builder;
    SContext m_Context;
    SNetStorageObjectState<SIState> m_IState;
    SNetStorageObjectState<SOState> m_OState;
};

void SNetStorageObjectRPC::StartWriting(CJsonNode::TInstance request, CNetServerConnection::TInstance conn)
{
    m_Context.m_OriginalRequest = request;
    m_Context.m_Connection = conn;
    EnterState(&m_OState);
}


SNetStorageObjectRPC::SNetStorageObjectRPC(SNetStorageObjectImpl& fsm, SNetStorageRPC* netstorage_rpc, CNetService service,
        TBuilder builder, const string& object_loc) :
    m_NetStorageRPC(netstorage_rpc),
    m_OwnService(service),
    m_Builder(builder),
    m_Context(netstorage_rpc, object_loc),
    m_IState(fsm, m_Context),
    m_OState(fsm, m_Context)
{
}

SNetStorage::SConfig::EDefaultStorage
SNetStorage::SConfig::GetDefaultStorage(const string& value)
{
    if (NStr::CompareNocase(value, "nst") == 0)
        return eNetStorage;
    else if (NStr::CompareNocase(value, "nc") == 0)
        return eNetCache;
    else if (NStr::CompareNocase(value, "nocreate") == 0 ||
            NStr::CompareNocase(value, "no_create") == 0)
        return eNoCreate;
    else {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "Invalid default_storage value '" << value << '\'');
        return eUndefined;
    }
}

SNetStorage::SConfig::EErrMode
SNetStorage::SConfig::GetErrMode(const string& value)
{
    if (NStr::CompareNocase(value, "strict") == 0)
        return eThrow;
    else if (NStr::CompareNocase(value, "ignore") == 0)
        return eIgnore;
    else
        return eLog;
}

void SNetStorage::SConfig::ParseArg(const string& name, const string& value)
{
    if (name == "domain")
        app_domain = value;
    else if (name == "default_storage")
        default_storage = GetDefaultStorage(value);
    else if (name == "metadata")
        metadata = value;
    else if (name == "namespace")
        app_domain = value;
    else if (name == "nst")
        service = value;
    else if (name == "nc")
        nc_service = value;
    else if (name == "cache")
        app_domain = value;
    else if (name == "client")
        client_name = value;
    else if (name == "err_mode")
        err_mode = GetErrMode(value);
    else if (name == "ticket")
        ticket = value;
    else if (name == "hello_service")
        hello_service = value;
}

void SNetStorage::SConfig::Validate(const string& init_string)
{
    SNetStorage::SLimits::Check<SNetStorage::SLimits::SNamespace>(app_domain);

    if (client_name.empty()) {
        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app != NULL) {
            string path;
            CDirEntry::SplitPath(app->GetProgramExecutablePath(),
                    &path, &client_name);
            if (NStr::EndsWith(path, CDirEntry::GetPathSeparator()))
                path.erase(path.length() - 1);
            string parent_dir;
            CDirEntry::SplitPath(path, NULL, &parent_dir);
            if (!parent_dir.empty()) {
                client_name += '-';
                client_name += parent_dir;
            }
        }
    }

    if (client_name.empty()) {
        NCBI_THROW_FMT(CNetStorageException, eAuthError,
                "Client name is required.");
    }

    switch (default_storage) {
    case eUndefined:
        default_storage =
                !service.empty() ? eNetStorage :
                !nc_service.empty() ? eNetCache :
                eNoCreate;
        break;

    case eNetStorage:
        if (service.empty()) {
            NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                    init_string << ": 'nst=' parameter is required "
                            "when 'default_storage=nst'");
        }
        break;

    case eNetCache:
        if (nc_service.empty()) {
            NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                    init_string << ": 'nc=' parameter is required "
                            "when 'default_storage=nc'");
        }
        break;

    default: /* eNoCreate */
        break;
    }

    if (hello_service.empty()) hello_service = service;
}

SNetStorageRPC::SNetStorageRPC(const TConfig& config,
        TNetStorageFlags default_flags) :
    m_DefaultFlags(default_flags),
    m_Config(config)
{
    m_RequestNumber.Set(0);

    CJsonNode hello(MkStdRequest("HELLO"));

    hello.SetString("Client", m_Config.client_name);
    hello.SetString("Service", m_Config.hello_service);
    if (!m_Config.metadata.empty())
        hello.SetString("Metadata", m_Config.metadata);
    {{
        CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
        if (app)
            hello.SetString("Application", app->GetProgramExecutablePath());
    }}
    hello.SetString("ProtocolVersion", NST_PROTOCOL_VERSION);
    if (!m_Config.ticket.empty()) hello.SetString("Ticket", m_Config.ticket);

    CSynRegistryBuilder registry_builder;
    SRegSynonyms sections("netstorage_api");
    m_Service = SNetServiceImpl::Create("NetStorageAPI", m_Config.service, m_Config.client_name,
            new CNetStorageServerListener(hello, m_Config.err_mode),
            registry_builder, sections);
}

SNetStorageRPC::SNetStorageRPC(SNetServerInPool* server,
        SNetStorageRPC* parent) :
    m_DefaultFlags(parent->m_DefaultFlags),
    m_Service(SNetServiceImpl::Clone(server, parent->m_Service)),
    m_Config(parent->m_Config),
    m_CompoundIDPool(parent->m_CompoundIDPool),
    m_NetCacheAPI(parent->m_NetCacheAPI),
    m_ServiceMap(parent->m_ServiceMap)
{
}

SNetStorageObjectImpl* SNetStorageRPC::Create(TNetStorageFlags flags)
{
    switch (m_Config.default_storage) {
    /* TConfig::eUndefined is overridden in the constructor */

    case TConfig::eNetStorage:
        break; // This case is handled below the switch.

    case TConfig::eNetCache:
        x_InitNetCacheAPI();
        return SNetStorageObjectImpl::CreateAndStart<SNetStorage_NetCacheBlob>(
                [&](SNetStorage_NetCacheBlob& s) { s.StartWriting(); }, m_NetCacheAPI, kEmptyStr);

    default: /* TConfig::eNoCreate */
        NCBI_THROW_FMT(CNetStorageException, eAuthError,
                "Object creation is disabled.");
    }

    auto request = MkStdRequest("CREATE");
    s_SetStorageFlags(request, GetFlags(flags));

    CNetServerConnection conn;
    const auto reply = Exchange(m_Service, request, &conn);
    const auto object_loc = reply.GetString("ObjectLoc");
    auto service = GetServiceIfLocator(object_loc);
    auto builder = [this](const string& r, const string& l) { return MkObjectRequest(r, l); };
    auto starter = [&](SNetStorageObjectRPC& s) { s.StartWriting(request, conn); };

    return SNetStorageObjectImpl::CreateAndStart<SNetStorageObjectRPC>(starter, this, service, builder, object_loc);
}

SNetStorageObjectImpl* SNetStorageRPC::Open(const string& object_loc)
{
    auto service = GetServiceIfLocator(object_loc);

    if (!service) {
        return SNetStorageObjectImpl::Create<SNetStorage_NetCacheBlob>(m_NetCacheAPI, object_loc);
    }

    auto builder = [this](const string& r, const string& l) { return MkObjectRequest(r, l); };

    return SNetStorageObjectImpl::Create<SNetStorageObjectRPC>(this, service, builder, object_loc);
}

string SNetStorageObjectRPC::Relocate(TNetStorageFlags flags, TNetStorageProgressCb cb)
{
    MkRequest("RELOCATE");

    CJsonNode new_location(CJsonNode::NewObjectNode());

    s_SetStorageFlags(new_location, flags);

    m_Context.m_OriginalRequest.SetByKey("NewLocation", new_location);

    // Always request progress report to avoid timing out on large objects
    m_Context.m_OriginalRequest.SetBoolean("NeedProgressReport", true);

    CNetServer server(*m_OwnService.Iterate(CNetService::eRandomize));

    CJsonOverUTTPExecHandler json_over_uttp_sender(m_Context.m_OriginalRequest);

    server->TryExec(json_over_uttp_sender);

    m_Context.m_Connection = json_over_uttp_sender.GetConnection();

    for (;;) {
        CJsonNode reply(m_Context.ReadMessage());
        CJsonNode object_loc(reply.GetByKeyOrNull("ObjectLoc"));

        if (object_loc) return object_loc.AsString();

        CJsonNode progress_info(reply.GetByKeyOrNull("ProgressInfo"));

        if (progress_info) {
            if (cb) cb(progress_info);
        } else {
            NCBI_THROW_FMT(CNetStorageException, eServerError, "Unexpected JSON received: " << reply.Repr());
        }
    }
}

bool SNetStorageObjectRPC::Exists()
{
    MkRequest("EXISTS");

    try {
        const auto reply = Exchange();
        return reply.GetBoolean("Exists");
    }
    catch (CNetStorageException& e) {
        if (e.GetErrCode() != CNetStorageException::eExpired) throw;
    }

    return false;
}

ENetStorageRemoveResult SNetStorageObjectRPC::Remove()
{
    MkRequest("DELETE");

    try {
        const auto reply = Exchange();
        const auto not_found = reply.GetByKeyOrNull("NotFound");

        return not_found && not_found.AsBoolean() ? eNSTRR_NotFound : eNSTRR_Removed;
    }
    catch (CNetStorageException& e) {
        if (e.GetErrCode() != CNetStorageException::eExpired) throw;
    }

    return eNSTRR_NotFound;
}

CJsonNode SNetStorageRPC::Exchange(CNetService service,
        const CJsonNode& request,
        CNetServerConnection* pconn,
        CNetServer::TInstance server_to_use) const
{
    CNetServer server(server_to_use != NULL ? server_to_use :
            (CNetServer::TInstance)
                    *service.Iterate(CNetService::eRandomize));

    CJsonOverUTTPExecHandler json_over_uttp_sender(request);

    server->TryExec(json_over_uttp_sender);

    CNetServerConnection conn(json_over_uttp_sender.GetConnection());

    if (pconn) *pconn = conn;

    return s_ReadMessage(request, conn, m_Config.err_mode, *service->m_Listener);
}

CJsonNode SNetStorageRPC::MkStdRequest(const string& request_type) const
{
    CJsonNode new_request(CJsonNode::NewObjectNode());

    new_request.SetString("Type", request_type);
    new_request.SetInteger("SN", (Int8) m_RequestNumber.Add(1));

    CRequestContext& req = CDiagContext::GetRequestContext();

    if (req.IsSetClientIP()) {
        new_request.SetString("ClientIP", req.GetClientIP());
    }

    if (req.IsSetSessionID()) {
        new_request.SetString("SessionID", req.GetSessionID());
    }

    req.GetNextSubHitID();

    const auto format = CRequestContext_PassThrough::eFormat_UrlEncoded;
    const CRequestContext_PassThrough context;
    const string ncbi_context(context.Serialize(format));

    if (!ncbi_context.empty()) {
        new_request.SetString("ncbi_context", ncbi_context);
    }

    return new_request;
}

CJsonNode SNetStorageRPC::MkObjectRequest(const string& request_type,
        const string& object_loc) const
{
    CJsonNode new_request(MkStdRequest(request_type));

    new_request.SetString("ObjectLoc", object_loc);

    return new_request;
}

CJsonNode SNetStorageRPC::MkObjectRequest(const string& request_type,
        const string& unique_key, TNetStorageFlags flags) const
{
    CJsonNode new_request(MkStdRequest(request_type));

    CJsonNode user_key(CJsonNode::NewObjectNode());
    user_key.SetString("AppDomain", m_Config.app_domain);
    user_key.SetString("UniqueID", unique_key);
    new_request.SetByKey("UserKey", user_key);

    s_SetStorageFlags(new_request, GetFlags(flags));
    return new_request;
}

CNetService SNetStorageRPC::GetServiceIfLocator(const string& object_loc)
{
    const bool direct_nc = m_Config.default_storage == TConfig::eNetCache;
    auto IsBlobID = [&]() { return CNetCacheKey::ParseBlobKey(object_loc.data(), object_loc.length(), nullptr); };

    if (direct_nc && IsBlobID()) return x_InitNetCacheAPI();

    CCompoundID cid;

    try {
        cid = m_CompoundIDPool.FromString(object_loc);
    }
    catch (...) {
        if (!direct_nc && IsBlobID()) return x_InitNetCacheAPI();

        throw;
    }

    if (cid.GetClass() == eCIC_NetCacheBlobKey) return x_InitNetCacheAPI();

    string service_name = CNetStorageObjectLoc::GetServiceName(cid);

    if (service_name.empty())
        return m_Service;

    auto i = m_ServiceMap.find(service_name);

    if (i != m_ServiceMap.end()) return i->second;

    CNetService service(m_Service.Clone(service_name));
    m_ServiceMap.insert(make_pair(service_name, service));
    return service;
}

EVoid SNetStorageRPC::x_InitNetCacheAPI()
{
    if (!m_NetCacheAPI) {
        CNetCacheAPI nc_api(m_Config.nc_service, m_Config.client_name);
        nc_api.SetCompoundIDPool(m_CompoundIDPool);
        nc_api.SetDefaultParameters(nc_use_compound_id = true);
        m_NetCacheAPI = nc_api;
    }

    return eVoid;
}

void SNetStorageObjectRPC::SIState::ReadConfirmation()
{
    if (m_UTTPReader.GetControlSymbol() != END_OF_DATA_MARKER) {
        NCBI_THROW_FMT(CNetStorageException, eIOError,
                "NetStorage API: invalid end-of-data-stream terminator: " << (int) m_UTTPReader.GetControlSymbol());
    }

    m_EOF = true;

    CJsonOverUTTPReader json_reader;

    while (!json_reader.ReadMessage(m_UTTPReader)) {
        ReadSocket();
    }

    if (m_UTTPReader.GetNextEvent() != CUTTPReader::eEndOfBuffer) {
        NCBI_THROW_FMT(CNetStorageException, eIOError,
                "Extra bytes past confirmation message "
                "while reading " << m_Context.locator << " from " <<
                m_Context.m_Connection->m_Socket.GetPeerAddress() << '.');
    }

    m_Context.TrapErrors(json_reader.GetMessage());
}

ERW_Result SNetStorageObjectRPC::Read(void* buffer, size_t buf_size,
        size_t* bytes_read)
{
    MkRequest("READ");

    CNetServer server(*m_OwnService.Iterate(CNetService::eRandomize));

    CJsonOverUTTPExecHandler json_over_uttp_sender(m_Context.m_OriginalRequest);

    server->TryExec(json_over_uttp_sender);

    EnterState(&m_IState);
    m_Context.m_Connection = json_over_uttp_sender.GetConnection();
    m_IState.StartReading();

    return m_IState.Read(buffer, buf_size, bytes_read);
}

void SNetStorageObjectRPC::SIState::StartReading()
{
    m_ReadBuffer = vector<char>(READ_BUFFER_SIZE);
    m_UTTPReader.Reset();
    m_CurrentChunk = nullptr;
    m_CurrentChunkSize = 0;
    m_EOF = false;

    CJsonOverUTTPReader json_reader;

    try {
        do {
            ReadSocket();
        } while (!json_reader.ReadMessage(m_UTTPReader));

        m_Context.TrapErrors(json_reader.GetMessage());
    }
    catch (...) {
        Abort();
        throw;
    }
}

ERW_Result SNetStorageObjectRPC::SIState::Read(void* buf_pos, size_t buf_size, size_t* bytes_read)
{
    if (bytes_read) *bytes_read = 0;

    if (!m_CurrentChunkSize && m_EOF) return eRW_Eof;

    if (!buf_size) return eRW_Success;

    try {
        while (!m_CurrentChunkSize) {
            switch (m_UTTPReader.GetNextEvent()) {
            case CUTTPReader::eChunkPart:
            case CUTTPReader::eChunk:
                m_CurrentChunk = m_UTTPReader.GetChunkPart();
                m_CurrentChunkSize = m_UTTPReader.GetChunkPartSize();
                continue;

            case CUTTPReader::eControlSymbol:
                ReadConfirmation();
                return eRW_Eof;

            case CUTTPReader::eEndOfBuffer:
                ReadSocket();
                continue;

            default:
                NCBI_THROW_FMT(CNetStorageException, eIOError,
                        "NetStorage API: invalid UTTP status "
                        "while reading " << m_Context.locator);
            }
        }
    }
    catch (...) {
        Abort();
        throw;
    }

    if (auto bytes_copied = min(m_CurrentChunkSize, buf_size)) {
        memcpy(buf_pos, m_CurrentChunk, bytes_copied);
        m_CurrentChunk += bytes_copied;
        m_CurrentChunkSize -= bytes_copied;

        if (bytes_read) *bytes_read = bytes_copied;
    }

    return eRW_Success;
}

bool SNetStorageObjectRPC::Eof()
{
        return false;
}

bool SNetStorageObjectRPC::SIState::Eof()
{
    return m_CurrentChunkSize == 0 && m_EOF;
}

ERW_Result SNetStorageObjectRPC::Write(const void* buf_pos, size_t buf_size,
        size_t* bytes_written)
{
    MkRequest("WRITE");

    m_Context.locator = Exchange().GetString("ObjectLoc");

    EnterState(&m_OState);
    return m_OState.Write(buf_pos, buf_size, bytes_written);
}

ERW_Result SNetStorageObjectRPC::SOState::Write(const void* buf_pos, size_t buf_size, size_t* bytes_written)
{
    try {
        const char* chunk = reinterpret_cast<const char*>(buf_pos);
        auto f = [&](CUTTPWriter& w) { w.SendChunk(chunk, buf_size, false); };
        s_SendUTTP(m_Context.m_Connection->m_Socket, f);

        if (bytes_written != NULL)
            *bytes_written = buf_size;
        return eRW_Success;
    }
    catch (exception&) {
        Abort();
        throw;
    }
}

Uint8 SNetStorageObjectRPC::GetSize()
{
    MkRequest("GETSIZE");

    return (Uint8) Exchange().GetInteger("Size");
}

list<string> SNetStorageObjectRPC::GetAttributeList() const
{
    MkRequest("GETATTRLIST");

    CJsonNode reply(Exchange());
    CJsonNode names(reply.GetByKeyOrNull("AttributeNames"));
    list<string> result;

    if (names) {
        for (CJsonIterator it = names.Iterate(); it; ++it) {
            result.push_back((*it).AsString());
        }
    }

    return result;
}

string SNetStorageObjectRPC::GetAttribute(const string& attr_name) const
{
    MkRequest("GETATTR");

    m_Context.m_OriginalRequest.SetString("AttrName", attr_name);

    return Exchange().GetString("AttrValue");
}

void SNetStorageObjectRPC::SetAttribute(const string& attr_name,
        const string& attr_value)
{
    MkRequest("SETATTR");

    m_Context.m_OriginalRequest.SetString("AttrName", attr_name);
    m_Context.m_OriginalRequest.SetString("AttrValue", attr_value);

    Exchange();
}

CNetStorageObjectInfo SNetStorageObjectRPC::GetInfo()
{
    MkRequest("GETOBJECTINFO");

    return g_CreateNetStorageObjectInfo(Exchange());
}

void SNetStorageObjectRPC::SetExpiration(const CTimeout& ttl)
{
    MkRequest("SETEXPTIME");

    const auto ttl_str = ttl.IsFinite() ? ttl.GetAsTimeSpan().AsString("dTh:m:s") : "infinity";
    m_Context.m_OriginalRequest.SetString("TTL", ttl_str);

    Exchange();
}

string SNetStorageObjectRPC::FileTrack_Path()
{
    MkRequest("LOCKFTPATH");

    return Exchange().GetString("Path");
}

struct SConnReset
{
    CNetServerConnection& conn;
    SConnReset(CNetServerConnection& c) : conn(c) {}
    ~SConnReset() { conn = nullptr; }
};

void SNetStorageObjectRPC::SIState::Close()
{
    SConnReset conn_reset(m_Context.m_Connection);

    ExitState();
    m_UTTPReader.Reset();

    if (!Eof()) {
        m_Context.m_Connection->Abort();
    }
}

void SNetStorageObjectRPC::SOState::Close()
{
    SConnReset conn_reset(m_Context.m_Connection);

    ExitState();

    auto f = [](CUTTPWriter& w) { w.SendControlSymbol(END_OF_DATA_MARKER); };
    s_SendUTTP(m_Context.m_Connection->m_Socket, f);
    m_Context.ReadMessage();
}

void SNetStorageObjectRPC::Close()
{
}

ERW_Result SNetStorageObjectRPC::SIState::PendingCount(size_t* count)
{
    *count = 0;
    return eRW_Success;
}

ERW_Result SNetStorageObjectRPC::PendingCount(size_t* count)
{
    *count = 0;
    return eRW_Success;
}

ERW_Result SNetStorageObjectRPC::SOState::Flush()
{
    return eRW_Success;
}

ERW_Result SNetStorageObjectRPC::Flush()
{
    return eRW_Success;
}

void SNetStorageObjectRPC::SIState::Abort()
{
    SConnReset conn_reset(m_Context.m_Connection);

    ExitState();
    m_UTTPReader.Reset();
    m_Context.m_Connection->Close();
}

void SNetStorageObjectRPC::SOState::Abort()
{
    SConnReset conn_reset(m_Context.m_Connection);

    ExitState();
    m_Context.m_Connection->Close();
}

void SNetStorageObjectRPC::Abort()
{
    Close();
}

struct SNetStorageByKeyRPC : public SNetStorageByKeyImpl
{
    SNetStorageByKeyRPC(const TConfig& config,
            TNetStorageFlags default_flags);

    SNetStorageObjectImpl* Open(const string& unique_key, TNetStorageFlags flags) override;

    CNetRef<SNetStorageRPC> m_NetStorageRPC;
};

SNetStorageByKeyRPC::SNetStorageByKeyRPC(const TConfig& config,
        TNetStorageFlags default_flags) :
    m_NetStorageRPC(new SNetStorageRPC(config, default_flags))
{
    if (m_NetStorageRPC->m_Config.app_domain.empty()) {
        NCBI_THROW_FMT(CNetStorageException, eInvalidArg,
                "'domain' parameter is missing from the initialization string");
    }
}

SNetStorageObjectImpl* SNetStorageByKeyRPC::Open(const string& unique_key,
        TNetStorageFlags flags)
{
    auto& rpc = m_NetStorageRPC;
    auto builder = [=](const string& r, const string&) {
        return rpc->MkObjectRequest(r, unique_key, flags);
    };

    return SNetStorageObjectImpl::Create<SNetStorageObjectRPC>(rpc, rpc->m_Service, builder, kEmptyStr);
}

struct SNetStorageAdminImpl : public CObject
{
    SNetStorageAdminImpl(CNetStorage::TInstance netstorage_impl) :
        m_NetStorageRPC(static_cast<SNetStorageRPC*>(netstorage_impl))
    {
    }

    CNetRef<SNetStorageRPC> m_NetStorageRPC;
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
    return m_Impl->m_NetStorageRPC->Exchange(m_Impl->m_NetStorageRPC->m_Service,
            request, conn, server_to_use);
}

CNetStorageAdmin CNetStorageAdmin::GetServer(CNetServer::TInstance server)
{
    return new SNetStorageRPC(server->m_ServerInPool, m_Impl->m_NetStorageRPC);
}

CNetStorageObject CNetStorageAdmin::Open(const string& object_loc)
{
    return m_Impl->m_NetStorageRPC->Open(object_loc);
}

SNetStorageImpl* SNetStorage::CreateImpl(const SConfig& config,
        TNetStorageFlags default_flags)
{
    return new SNetStorageRPC(config, default_flags);
}

SNetStorageByKeyImpl* SNetStorage::CreateByKeyImpl(const SConfig& config,
        TNetStorageFlags default_flags)
{
    return new SNetStorageByKeyRPC(config, default_flags);
}

END_NCBI_SCOPE
