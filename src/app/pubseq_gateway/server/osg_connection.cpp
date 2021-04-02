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
 * Authors: Eugene Vasilchenko
 *
 * File Description: base class for processors which may generate os_gateway
 *                   fetches
 *
 */

#include <ncbi_pch.hpp>

#include "osg_connection.hpp"

#include <corelib/ncbithr.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <objects/id2/ID2_Request.hpp>
#include <objects/id2/ID2_Param.hpp>
#include <objects/id2/ID2_Params.hpp>
#include <objects/id2/ID2_Request_Packet.hpp>
#include <objects/id2/ID2_Reply.hpp>
#include <objects/id2/ID2_Reply_Data.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <cmath>
#include <corelib/ncbi_system.hpp>
#include <corelib/impl/ncbi_dbsvcmapper.hpp>
#include "osg_processor_base.hpp"
#include "osg_mapper.hpp"


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


// Configuration parameters' names
static const char kConfigSection[] = "OSG_PROCESSOR";
static const char kParamServiceName[] = "service";
static const char kParamMaxConnectionCount[] = "maxconn";
static const char kParamDebugLevel[] = "debug";
static const char kParamExpirationTimeout[] = "expiration_timeout";
static const char kParamReadTimeout[] = "read_timeout";
static const char kParamCDDRetryTimeout[] = "cdd_retry_timeout";
static const char kParamRetryCount[] = "retry_count";
static const char kParamPreferredServer[] = "preferred_server";
static const char kParamPreference[] = "preference";
static const char kParamEnabledCDD[] = "enabled_cdd";
static const char kParamEnabledSNP[] = "enabled_snp";
static const char kParamEnabledWGS[] = "enabled_wgs";

// Default configuration parameters' values
static const char kDefaultServiceName[] = "ID2_SNP2";
static const int kMinMaxConnectionCount = 1;
static const int kDefaultMaxConnectionCount = 64;
static const EDebugLevel kDefaultDebugLevel = eDebug_error;
static const double kMinExpirationTimeout = 1;
static const double kDefaultExpirationTimeout = 60;
static const double kMinReadTimeout = .1;
static const double kDefaultReadTimeout = 30;
static const double kMinCDDRetryTimeout = .1;
static const double kDefaultCDDRetryTimeout = 0.9;
static const int kMinRetryCount = 1;
static const int kDefaultRetryCount = 3;
static const char kDefaultPreferredServer[] = "localhost";
static const int kDefaultPreference = 90;
static const int kDefaultEnabledCDD = true;
static const int kDefaultEnabledSNP = true;
static const int kDefaultEnabledWGS = true;

static const int kNonResolutionTimeout = 5;


COSGConnection::COSGConnection(size_t connection_id)
    : m_ConnectionID(connection_id),
      m_RequestCount(0),
      m_InitRequestWasSent(false),
      m_Timestamp(CStopWatch::eStart)
{
}


COSGConnection::COSGConnection(size_t connection_id,
                               unique_ptr<CConn_IOStream>&& stream)
    : m_ConnectionID(connection_id),
      m_Stream(move(stream)),
      m_RequestCount(0),
      m_InitRequestWasSent(false),
      m_Timestamp(CStopWatch::eStart)
{
}


COSGConnection::~COSGConnection()
{
    if ( m_RemoveFrom ) {
        m_RemoveFrom->RemoveConnection(*this);
    }
    _ASSERT(!m_RemoveFrom);
}


double COSGConnection::UpdateTimestamp()
{
    return m_Timestamp.Restart();
}


void COSGConnection::AcceptFeedback(int feedback)
{
    if ( feedback != 0 && m_RemoveFrom && m_ServerInfo ) {
        m_RemoveFrom->m_Mapper->AcceptFeedback(m_ServiceName,
                                               m_ServerInfo->GetHost(), m_ServerInfo->GetPort(),
                                               (feedback < 0?
                                                COSGServiceMapper::eNegativeFeedback:
                                                COSGServiceMapper::ePositiveFeedback));
    }
}


template<class Type>
struct SConditionalASNLogger
{
    SConditionalASNLogger(const Type& obj, bool condition)
        : m_Object(obj), m_Condition(condition)
        {
        }

    const Type& m_Object;
    bool m_Condition;
};
template<class Type>
CNcbiOstream& operator<<(CNcbiOstream& out, const SConditionalASNLogger<Type>& logger)
{
    if ( logger.m_Condition ) {
        out << MSerial_AsnText << logger.m_Object;
    }
    else {
        out << Type::GetTypeInfo()->GetName();
    }
    return out;
}
template<class Type>
SConditionalASNLogger<Type> LogASNIf(const Type& obj, bool condition)
{
    return SConditionalASNLogger<Type>(obj, condition);
}


const bool kSimulateFailures = false;
const int kNoFailureCount = 8;
const int kFailureRate = 8;
static DECLARE_TLS_VAR(int, s_NoFailureCount);

static void s_SimulateFailure(const char* where)
{
    if ( !kSimulateFailures ) {
        return;
    }
    if ( s_NoFailureCount > 0 ) {
        --s_NoFailureCount;
    }
    else if ( random() % kFailureRate == 0 ) {
        s_NoFailureCount = kNoFailureCount;
        string msg = string("simulated OSG ")+where+" failure";
        if ( random() % 2 ) {
            throw runtime_error(msg);
        }
        else {
            NCBI_THROW(CIOException, eWrite, msg);
        }
    }
}


void COSGConnection::SendRequestPacket(const CID2_Request_Packet& packet)
{
    _ASSERT(m_RemoveFrom);
    if ( GetDebugLevel() >= eDebug_exchange ) {
        LOG_POST(GetDiagSeverity() << "OSG("<<GetConnectionID()<<"): "
                 "Sending "<<LogASNIf(packet, GetDebugLevel() >= eDebug_asn));
    }
    _ASSERT(!packet.Get().empty());
    _ASSERT(packet.Get().front()->GetSerial_number()+int(packet.Get().size()) == GetNextRequestSerialNumber());
    _ASSERT(packet.Get().back()->GetSerial_number()+1 == GetNextRequestSerialNumber());
    s_SimulateFailure("send");
    _ASSERT(m_InitRequestWasSent || packet.Get().front()->GetRequest().IsInit());
    *m_Stream << MSerial_AsnBinary << packet;
    if ( packet.Get().front()->GetRequest().IsInit() ) {
        m_InitRequestWasSent = true;
    }
    _ASSERT(m_InitRequestWasSent);
}


CRef<CID2_Reply> COSGConnection::ReceiveReply()
{
    s_SimulateFailure("read");
    CRef<CID2_Reply> reply(new CID2_Reply());
    *m_Stream >> MSerial_AsnBinary >> *reply;
    _ASSERT(m_RemoveFrom);
    if ( GetDebugLevel() >= eDebug_exchange ) {
        if ( GetDebugLevel() == eDebug_asn ) {
            CTypeIterator<CID2_Reply_Data> iter = Begin(*reply);
            if ( iter && iter->IsSetData() ) {
                CID2_Reply_Data::TData save;
                save.swap(iter->SetData());
                size_t size = 0, count = 0;
                ITERATE ( CID2_Reply_Data::TData, i, save ) {
                    ++count;
                    size_t chunk = (*i)->size();
                    size += chunk;
                }
                LOG_POST(GetDiagSeverity() << "OSG("<<GetConnectionID()<<"): "
                         "Received "<<MSerial_AsnText<<*reply<<
                         "Data: " << size << " bytes in "<<count<<" chunks");
                save.swap(iter->SetData());
            }
            else {
                LOG_POST(GetDiagSeverity() << "OSG("<<GetConnectionID()<<"): "
                         "Received "<<MSerial_AsnText<<*reply);
            }
        }
        else {
            LOG_POST(GetDiagSeverity() << "OSG("<<GetConnectionID()<<"): "
                     "Received "<<LogASNIf(*reply, GetDebugLevel() >= eDebug_raw));
        }
    }
    return reply;
}


CRef<CID2_Request> COSGConnection::MakeInitRequest()
{
    CRef<CID2_Request> req(new CID2_Request());
    req->SetRequest().SetInit();
    if ( 1 ) {
        // set client name
        CRef<CID2_Param> param(new CID2_Param);
        param->SetName("log:client_name");
        param->SetValue().push_back("pubseq_gateway");
        req->SetParams().Set().push_back(param);
    }
    if ( 1 ) {
        CRef<CID2_Param> param(new CID2_Param);
        param->SetName("id2:allow");

        // allow new blob-state field in several ID2 replies
        param->SetValue().push_back("*.blob-state");
        // enable VDB-based WGS sequences
        param->SetValue().push_back("vdb-wgs");
        // enable VDB-based SNP sequences
        param->SetValue().push_back("vdb-snp");
        // enable VDB-based CDD sequences
        param->SetValue().push_back("vdb-cdd");
        req->SetParams().Set().push_back(param);
    }
    return req;
}


CRef<CID2_Request_Packet> COSGConnection::MakeInitRequestPacket()
{
    CRef<CID2_Request_Packet> packet(new CID2_Request_Packet);
    packet->Set().push_back(MakeInitRequest());
    packet->Set().back()->SetSerial_number(AllocateRequestSerialNumber());
    return packet;
}


COSGConnectionPool::COSGConnectionPool()
    : m_ServiceName(kDefaultServiceName),
      m_MaxConnectionCount(kDefaultMaxConnectionCount),
      m_ExpirationTimeout(kDefaultExpirationTimeout),
      m_ReadTimeout(kDefaultReadTimeout),
      m_CDDRetryTimeout(kDefaultCDDRetryTimeout),
      m_RetryCount(kDefaultRetryCount),
      m_EnabledCDD(kDefaultEnabledCDD),
      m_EnabledSNP(kDefaultEnabledSNP),
      m_EnabledWGS(kDefaultEnabledWGS),
      m_WaitConnectionSlot(0, kMax_Int),
      m_NextConnectionID(1),
      m_ConnectionCount(0),
      m_ConnectFailureCount(0)
{
}


COSGConnectionPool::~COSGConnectionPool()
{
}


void COSGConnectionPool::AppParseArgs(const CArgs& /*args*/)
{
    // TODO
}


static Uint4 g_OSG_GetPreferredAddress(const string& name)
{
    if (name.empty()) {
        return 0;
    } else if (NStr::EqualNocase(name, "localhost")) {
        return CSocketAPI::GetLocalHostAddress();
    } else {
        return CSocketAPI::gethostbyname(name);
    }
}


void COSGConnectionPool::LoadConfig(const CNcbiRegistry& registry, string section)
{
    if ( section.empty() ) {
        section = kConfigSection;
    }

#define CHECK_PARAM_MIN(value, name, min_value)                         \
    do {                                                                \
        if ( value < min_value ) {                                      \
            NCBI_THROW_FMT(CPubseqGatewayException, eConfigurationError, \
                           name<<"(="<<value<<") < "<<min_value);       \
        }                                                               \
    } while (0)
    
    m_ServiceName =
        registry.GetString(section,
                           kParamServiceName,
                           kDefaultServiceName);
    m_MaxConnectionCount =
        registry.GetInt(section,
                        kParamMaxConnectionCount,
                        kDefaultMaxConnectionCount);
    CHECK_PARAM_MIN(m_MaxConnectionCount, kParamMaxConnectionCount, kMinMaxConnectionCount);

    m_ExpirationTimeout =
        registry.GetDouble(section,
                           kParamExpirationTimeout,
                           kDefaultExpirationTimeout);
    CHECK_PARAM_MIN(m_ExpirationTimeout, kParamExpirationTimeout, kMinExpirationTimeout);
    
    m_ReadTimeout =
        registry.GetDouble(section,
                           kParamReadTimeout,
                           kDefaultReadTimeout);
    CHECK_PARAM_MIN(m_ReadTimeout, kParamReadTimeout, kMinReadTimeout);
    
    m_CDDRetryTimeout =
        registry.GetDouble(section,
                           kParamCDDRetryTimeout,
                           kDefaultCDDRetryTimeout);
    CHECK_PARAM_MIN(m_CDDRetryTimeout, kParamCDDRetryTimeout, kMinCDDRetryTimeout);

    m_RetryCount =
        registry.GetInt(section,
                           kParamRetryCount,
                           kDefaultRetryCount);
    CHECK_PARAM_MIN(m_RetryCount, kParamRetryCount, kMinRetryCount);

#undef CHECK_PARAM_MIN

    SetDebugLevel(registry.GetInt(section,
                                  kParamDebugLevel,
                                  eDebugLevel_default));
    if ( GetDebugLevel() >= eDebug_open ) {
        LOG_POST(GetDiagSeverity()<<"OSG: pool of "<<m_MaxConnectionCount<<
                 " connections to "<<m_ServiceName);
    }

    COSGServiceMapper::InitDefaults(const_cast<CNcbiRegistry&>(registry));
    CRef<COSGServiceMapper> service_mapper(new COSGServiceMapper(&registry));
    string preferred_server = registry.GetString(section,
                                                 kParamPreferredServer,
                                                 kDefaultPreferredServer);
    int preference = registry.GetInt(section,
                                     kParamPreference,
                                     kDefaultPreference);
    auto psg_ip = g_OSG_GetPreferredAddress(preferred_server);
    TSvrRef pref_info(new CDBServer(m_ServiceName, psg_ip));
    service_mapper->SetPreference(m_ServiceName, pref_info, preference);
    if ( GetDebugLevel() >= eDebug_open ) {
        LOG_POST(GetDiagSeverity()<<"OSG: prefer "<<preferred_server<<
                 " ["<<CSocketAPI::ntoa(psg_ip)<<"] by "<<preference);
    }
    m_Mapper = service_mapper;

    m_EnabledCDD = registry.GetBool(section,
                                    kParamEnabledCDD,
                                    kDefaultEnabledCDD);
    m_EnabledSNP = registry.GetBool(section,
                                    kParamEnabledSNP,
                                    kDefaultEnabledSNP);
    m_EnabledWGS = registry.GetBool(section,
                                    kParamEnabledWGS,
                                    kDefaultEnabledWGS);
}


void COSGConnectionPool::SetLogging(EDiagSev severity)
{
    SetDiagSeverity(severity);
}


CRef<COSGConnection> COSGConnectionPool::AllocateConnection()
{
    CRef<COSGConnection> conn;
    while ( !conn ) {
        {{
            CMutexGuard guard(m_Mutex);
            while ( !conn && !m_FreeConnections.empty() ) {
                conn = move(m_FreeConnections.front());
                m_FreeConnections.pop_front();
                _ASSERT(!conn->m_RemoveFrom);
                if ( conn->UpdateTimestamp() > m_ExpirationTimeout ) {
                    if ( GetDebugLevel() >= eDebug_open ) {
                        LOG_POST(GetDiagSeverity()<<"OSG("<<conn->GetConnectionID()<<"): "
                                 "Closing expired connection");
                    }
                    --m_ConnectionCount;
                    conn = nullptr;
                }
            }
            if ( !conn && m_ConnectionCount < m_MaxConnectionCount ) {
                conn = new COSGConnection(m_NextConnectionID++);
                ++m_ConnectionCount;
            }
        }}
        if ( !conn ) {
            m_WaitConnectionSlot.Wait();
        }
    }
    _ASSERT(m_ConnectionCount > 0);
    _ASSERT(!conn->m_RemoveFrom);
    conn->m_RemoveFrom = this;
    if ( !conn->m_Stream ) {
        try {
            x_OpenConnection(*conn);
            m_ConnectFailureCount = 0;
        }
        catch ( ... ) {
            ++m_ConnectFailureCount;
            throw;
        }
    }
    return conn;
}


void COSGConnectionPool::x_OpenConnection(COSGConnection& conn)
{
    _ASSERT(conn.m_RemoveFrom == this);
    size_t connection_id = conn.GetConnectionID();
    if ( GetDebugLevel() >= eDebug_open ) {
        LOG_POST(GetDiagSeverity() << "OSG("<<connection_id<<"): "
                 "Connecting to "<<m_ServiceName);
    }
    int wait_count = m_ConnectFailureCount;
    if ( wait_count > 0 ) {
        // delay before opening new connection to a failing server
        double wait_seconds = .5*pow(2., wait_count-1)+.5*wait_count;
        wait_seconds = min(wait_seconds, 10.);
        if ( GetDebugLevel() >= eDebug_open ) {
            LOG_POST(GetDiagSeverity() << "OSG("<<connection_id<<"): waiting "<<
                     wait_seconds<<"s before new connection");
        }
        SleepMicroSec((unsigned long)(wait_seconds*1e6));
    }
    unique_ptr<CConn_IOStream> stream;
    conn.m_ServiceName = m_ServiceName;
    conn.m_ServerInfo = move(x_GetServer());
    if ( conn.m_ServerInfo ) {
        string host = CSocketAPI::ntoa(conn.m_ServerInfo->GetHost());
        if ( GetDebugLevel() >= eDebug_open ) {
            LOG_POST(GetDiagSeverity() << "OSG("<<connection_id<<"): "
                     "Connecting to "<<host<<":"<<conn.m_ServerInfo->GetPort());
        }
        stream = make_unique<CConn_SocketStream>(host, conn.m_ServerInfo->GetPort());
    }
    else {
        stream = make_unique<CConn_ServiceStream>(m_ServiceName);
    }
    if ( !stream || !*stream ) {
        NCBI_THROW(CIOException, eWrite, "failed to open connection");
    }
    if ( GetDebugLevel() >= eDebug_open ) {
        string descr = m_ServiceName;
        if ( CONN conn = stream->GetCONN() ) {
            AutoPtr<char, CDeleter<char> > conn_descr(CONN_Description(conn));
            if ( conn_descr ) {
                descr += " -> ";
                descr += conn_descr.get();
            }
        }
        LOG_POST(GetDiagSeverity() << "OSG("<<connection_id<<"): "
                 "Connected to "<<descr);
    }
    conn.m_Stream = move(stream);
    if ( 1 ) {
        auto req_packet = conn.MakeInitRequestPacket();
        conn.SendRequestPacket(*req_packet);
        _ASSERT(conn.InitRequestWasSent());
        auto reply = conn.ReceiveReply();
        if ( !reply->GetReply().IsInit() || !reply->IsSetEnd_of_reply() ) {
            NCBI_THROW(CIOException, eRead, "bad init reply");
        }
        conn.AcceptFeedback(+1);
    }
}


TSvrRef COSGConnectionPool::x_GetServer()
{
    if ( !m_Mapper ) {
        return null;
    }
    CMutexGuard guard(m_Mutex);
    if ( m_NonresolutionRetryDeadline && !m_NonresolutionRetryDeadline->IsExpired() ) {
        return null;
    }
    TSvrRef server;
    do {
        if ( !m_Balancer ) {
            IDBServiceMapper::TOptions options;
            m_Mapper->GetServerOptions(m_ServiceName, &options);
            m_Balancer.Reset(new CPoolBalancer(m_ServiceName, options, true));
        }
        server = m_Balancer->GetServer();
        if ( !server ) {
            ERR_POST(Warning <<
                     "Unable to resolve OSG service name "
                     << m_ServiceName
                     << " via supplied mapper; passing it as is.");
            m_NonresolutionRetryDeadline.reset(new CDeadline(kNonResolutionTimeout));
        }
        else if ( server->GetExpireTime() < CCurrentTime().GetTimeT() ) {
            m_Balancer.Reset();
        }
    } while ( !m_Balancer );
    if ( !server || server->GetHost() == 0 || server->GetPort() == 0 ) {
        return null;
    }
    else {
        return server;
    }
}


void COSGConnectionPool::ReleaseConnection(CRef<COSGConnection>& conn)
{
    CMutexGuard guard(m_Mutex);
    _ASSERT(conn);
    _ASSERT(m_ConnectionCount > 0);
    _ASSERT(conn->m_RemoveFrom == this);
    conn->AcceptFeedback(+1);
    conn->m_RemoveFrom = nullptr;
    m_FreeConnections.push_back(move(conn));
    _ASSERT(!conn);
    m_WaitConnectionSlot.Post();
}


void COSGConnectionPool::RemoveConnection(COSGConnection& conn)
{
    if ( GetDebugLevel() >= eDebug_open ) {
        LOG_POST(GetDiagSeverity()<<"OSG("<<conn.GetConnectionID()<<"): "
                 "Closing failed connection");
    }
    CMutexGuard guard(m_Mutex);
    _ASSERT(m_ConnectionCount > 0);
    _ASSERT(conn.m_RemoveFrom == this);
    conn.AcceptFeedback(-1);
    conn.m_RemoveFrom = nullptr;
    --m_ConnectionCount;
    m_WaitConnectionSlot.Post();
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
