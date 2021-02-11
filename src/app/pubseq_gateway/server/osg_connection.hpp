#ifndef PSGS_OSGCONNECTION__HPP
#define PSGS_OSGCONNECTION__HPP

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
 * File Description: class for communication with OSG
 *
 */

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <dbapi/driver/impl/dbapi_pool_balancer.hpp>
#include <corelib/impl/ncbi_dbsvcmapper.hpp>
#include <list>

BEGIN_NCBI_NAMESPACE;

class CNcbiRegistry;
class CArgs;
class CConn_IOStream;

BEGIN_NAMESPACE(objects);

class CID2_Request;
class CID2_Request_Packet;
class CID2_Reply;

END_NAMESPACE(objects);

BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);

USING_SCOPE(objects);

class COSGConnectionPool;

class COSGConnection : public CObject
{
public:
    virtual ~COSGConnection();

    size_t GetConnectionID() const
        {
            return m_ConnectionID;
        }
    
    int GetNextRequestSerialNumber() const
        {
            return m_RequestCount;
        }
    int AllocateRequestSerialNumber()
        {
            return m_RequestCount++;
        }

    bool InitRequestWasSent() const
        {
            return m_InitRequestWasSent;
        }
    
    void SendRequestPacket(const CID2_Request_Packet& packet);
    CRef<CID2_Reply> ReceiveReply();

    static CRef<CID2_Request> MakeInitRequest();
    CRef<CID2_Request_Packet> MakeInitRequestPacket();
    
    double UpdateTimestamp();

protected:
    friend class COSGConnectionPool;

    COSGConnection(size_t connection_id);
    COSGConnection(size_t connection_id,
                   unique_ptr<CConn_IOStream>&& stream);
    
private:
    size_t m_ConnectionID;
    CRef<COSGConnectionPool> m_RemoveFrom;
    unique_ptr<CConn_IOStream> m_Stream;
    int m_RequestCount;
    bool m_InitRequestWasSent;
    CStopWatch m_Timestamp;
};


class COSGConnectionPool : public CObject
{
public:
    COSGConnectionPool();
    virtual ~COSGConnectionPool();

    void AppParseArgs(const CArgs& args);
    void LoadConfig(const CNcbiRegistry& registry, string section = string());
    void SetLogging(EDiagSev severity);

    size_t GetMaxConnectionCount() const {
        return size_t(m_MaxConnectionCount);
    }
    size_t GetRetryCount() const {
        return size_t(m_RetryCount);
    }

    double GetCDDRetryTimeout() const {
        return m_CDDRetryTimeout;
    }

    CRef<COSGConnection> AllocateConnection();
    void ReleaseConnection(CRef<COSGConnection>& conn);

protected:
    friend class COSGConnection;

    void RemoveConnection(COSGConnection& conn);

    void x_OpenConnection(COSGConnection& conn);
    TSvrRef x_GetServer();
    
private:
    string m_ServiceName;
    int m_MaxConnectionCount;
    double m_ExpirationTimeout;
    double m_ReadTimeout;
    double m_CDDRetryTimeout;
    int m_RetryCount;
    CMutex m_Mutex;
    CSemaphore m_WaitConnectionSlot;
    size_t m_NextConnectionID;
    int m_ConnectionCount;
    int m_ConnectFailureCount;
    list<CRef<COSGConnection>> m_FreeConnections;
    CRef<IDBServiceMapper> m_Mapper;
    CRef<CDBPoolBalancer>  m_Balancer;
    unique_ptr<CDeadline>  m_NonresolutionRetryDeadline;
};


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // PSGS_OSGCONNECTION__HPP
