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

#include <connect/ncbi_conn_stream.hpp>
#include <objects/id2/ID2_Request_Packet.hpp>
#include <objects/id2/ID2_Reply.hpp>
#include <objects/id2/ID2_Reply_Data.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include "osg_processor_base.hpp"


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
static const char kParamRetryCount[] = "retry_count";

// Default configuration parameters' values
static const char kDefaultServiceName[] = "ID2_SNP2";
static const int kDefaultMaxConnectionCount = 8;
static const EDebugLevel kDefaultDebugLevel = eDebug_error;
static const double kDefaultExpirationTimeout = 60;
static const double kDefaultReadTimeout = 30;
static const double kDefaultRetryCount = 3;


COSGConnection::COSGConnection(size_t connection_id,
                               unique_ptr<CConn_IOStream>&& stream)
    : m_ConnectionID(connection_id),
      m_Stream(move(stream)),
      m_RequestCount(0),
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

void COSGConnection::SendRequestPacket(const CID2_Request_Packet& packet)
{
    _ASSERT(m_RemoveFrom);
    if ( GetDebugLevel() >= eDebug_exchange ) {
        LOG_POST(GetDiagSeverity() << "OSG("<<GetConnectionID()<<"): "
                 "Sending "<<LogASNIf(packet, GetDebugLevel() >= eDebug_asn));
    }
    *m_Stream << MSerial_AsnBinary << packet;
}


CRef<CID2_Reply> COSGConnection::ReceiveReply()
{
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


COSGConnectionPool::COSGConnectionPool()
    : m_ServiceName(kDefaultServiceName),
      m_MaxConnectionCount(kDefaultMaxConnectionCount),
      m_WaitConnectionSlot(0, kMax_Int),
      m_NextConnectionID(1),
      m_ConnectionCount(0)
{
}


COSGConnectionPool::~COSGConnectionPool()
{
}


void COSGConnectionPool::AppParseArgs(const CArgs& /*args*/)
{
    // TODO
}


void COSGConnectionPool::LoadConfig(const CNcbiRegistry& registry, string section)
{
    if ( section.empty() ) {
        section = kConfigSection;
    }
    m_ServiceName =
        registry.GetString(section,
                           kParamServiceName,
                           kDefaultServiceName);
    m_MaxConnectionCount =
        registry.GetInt(section,
                        kParamMaxConnectionCount,
                        kDefaultMaxConnectionCount);
    m_ExpirationTimeout =
        registry.GetDouble(section,
                           kParamExpirationTimeout,
                           kDefaultExpirationTimeout);
    m_ReadTimeout =
        registry.GetDouble(section,
                           kParamReadTimeout,
                           kDefaultReadTimeout);
    m_RetryCount =
        registry.GetInt(section,
                           kParamRetryCount,
                           kDefaultRetryCount);
    SetDebugLevel(registry.GetInt(section,
                                  kParamDebugLevel,
                                  eDebugLevel_default));
    if ( GetDebugLevel() >= eDebug_exchange ) {
        LOG_POST(GetDiagSeverity()<<"OSG: pool of "<<m_MaxConnectionCount<<
                 " connections to "<<m_ServiceName);
    }
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
                conn = x_CreateConnection();
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
    return conn;
}


CRef<COSGConnection> COSGConnectionPool::x_CreateConnection()
{
    _ASSERT(m_ConnectionCount < m_MaxConnectionCount);
    size_t connection_id = m_NextConnectionID++;
    if ( GetDebugLevel() >= eDebug_open ) {
        LOG_POST(GetDiagSeverity() << "OSG("<<connection_id<<"): "
                 "Connecting to "<<m_ServiceName);
    }
    unique_ptr<CConn_IOStream> stream = make_unique<CConn_ServiceStream>(m_ServiceName);
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
    return Ref(new COSGConnection(connection_id, move(stream)));
}


void COSGConnectionPool::ReleaseConnection(CRef<COSGConnection>& conn)
{
    CMutexGuard guard(m_Mutex);
    _ASSERT(conn);
    _ASSERT(m_ConnectionCount > 0);
    _ASSERT(conn->m_RemoveFrom == this);
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
    conn.m_RemoveFrom = nullptr;
    --m_ConnectionCount;
    m_WaitConnectionSlot.Post();
}


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
