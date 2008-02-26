#ifndef CONNECT_SERVICES__SERVER_CONN_HPP_1
#define CONNECT_SERVICES__SERVER_CONN_HPP_1

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
 * Authors:  Maxim Didneko, Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include <corelib/ncbimisc.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbimtx.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/connect_export.h>
#include <connect/ncbi_socket.hpp>


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////
//
struct SNetServerConnectionImpl;

class NCBI_XCONNECT_EXPORT CNetServerConnection
{
public:
    explicit CNetServerConnection(SNetServerConnectionImpl* impl);
    CNetServerConnection(const CNetServerConnection& src);
    CNetServerConnection& operator =(const CNetServerConnection& src);

    bool ReadStr(string& str);
    void WriteStr(const string& str);
    void WriteBuf(const void* buf, size_t len);
     // if wait_sec is set to 0 m_Timeout will be used
    void WaitForServer(unsigned int wait_sec = 0);

    class IStringProcessor {
    public:
        // if returns false the telnet method will stop
        virtual bool Process(string& line) = 0;
        virtual ~IStringProcessor() {}
    };
     // out and processor can be NULL
    void Telnet(CNcbiOstream* out,  IStringProcessor* processor);

    CSocket* GetSocket();
    void CheckConnect();
    void Close();
    void Abort();

    const string& GetHost() const;
    unsigned int GetPort() const;

    ~CNetServerConnection();

private:
    SNetServerConnectionImpl* m_ConnectionImpl;
};


///////////////////////////////////////////////////////////////////////////
//
class CNetServerConnectionPool;

class INetServerConnectionListener
{
public:
    virtual void OnConnected(CNetServerConnection) = 0;
    virtual void OnDisconnected(CNetServerConnectionPool*) = 0;

    virtual ~INetServerConnectionListener() {}
};


///////////////////////////////////////////////////////////////////////////
//
class NCBI_XCONNECT_EXPORT CNetServerConnectionPool
{
public:
    CNetServerConnectionPool(const string& host, unsigned short port,
                         INetServerConnectionListener* listener = NULL);

    const string& GetHost() const { return m_Host; }
    unsigned short GetPort() const { return m_Port; }

    INetServerConnectionListener* GetEventListener() const { return m_EventListener; }

    static void SetDefaultCommunicationTimeout(const STimeout& to);
    void SetCommunicationTimeout(const STimeout& to);
    const STimeout& GetCommunicationTimeout() const { return m_Timeout; }

    static void SetDefaultCreateSocketMaxReties(unsigned int retires);
    void SetCreateSocketMaxRetries(unsigned int retries) { m_MaxRetries = retries; }
    unsigned int GetCreateSocketMaxRetries() const { return m_MaxRetries; }

    void PermanentConnection(ESwitch type) { m_PermanentConnection = type; }

    CNetServerConnection GetConnection();

    ~CNetServerConnectionPool();

private:
    friend struct SNetServerConnectionImpl;

    void Put(SNetServerConnectionImpl* impl);

    string m_Host;
    unsigned short m_Port;

    STimeout m_Timeout;
    INetServerConnectionListener* m_EventListener;
    unsigned int m_MaxRetries;

    SNetServerConnectionImpl* m_FreeConnectionListHead;
    int m_FreeConnectionListSize;
    CFastMutex m_FreeConnectionListLock;

    ESwitch m_PermanentConnection;
};


///////////////////////////////////////////////////////////////////////////
inline string GetHostDNSName(const string& host)
{
    return CSocketAPI::gethostbyaddr(CSocketAPI::gethostbyname(host));
}

END_NCBI_SCOPE

#endif // CONNECT_SERVICES__SERVER_CONN_HPP
