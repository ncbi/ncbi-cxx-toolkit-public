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

#include "util.hpp"

#include <connect/ncbi_socket.hpp>

#include <corelib/reader_writer.hpp>
#include <corelib/ncbi_config.hpp>


BEGIN_NCBI_SCOPE

struct SNetServerImpl;                      ///< @internal
struct SNetServerConnectionImpl;            ///< @internal
struct SNetServerInfoImpl;                  ///< @internal
struct SNetServerMultilineCmdOutputImpl;    ///< @internal
struct INetServerConnectionListener;        ///< @internal

// A host:port pair.
struct SServerAddress {
    SServerAddress(unsigned h, unsigned short p) : host(h), port(p), name(h) {}
    SServerAddress(const string &n, unsigned short p)
        : host(g_NetService_gethostbyname(n)), port(p), name(n, host) {}

    bool operator ==(const SServerAddress& h) const
    {
        return host == h.host && port == h.port;
    }

    bool operator <(const SServerAddress& right) const
    {
        int cmp = int(host) - int(right.host);
        return cmp < 0 || (cmp == 0 && port < right.port);
    }

    string AsString() const
    {
        string address(name.get(host));
        address += ':';
        address += NStr::UIntToString(port);

        return address;
    }

    unsigned host;
    unsigned short port;

private:
    struct SName
    {
        SName() : host(0) {}
        SName(unsigned h) : host(h) {}
        SName(const string &n, unsigned h) : name(n), host(h) {}

        string get(unsigned h)
        {
            // Name was not looked up yet or host changed
            if (name.empty() || host != h) {
                host = h;
                name = g_NetService_gethostnamebyaddr(h);
            }

            return name;
        }

        string name;
        unsigned host;
    };

    mutable SName name;
};

///////////////////////////////////////////////////////////////////////////
//
class NCBI_XCONNECT_EXPORT CNetServerConnection
{
    NCBI_NET_COMPONENT(NetServerConnection);

    /// Execute remote command 'cmd', wait for the reply,
    /// check that it starts with 'OK:', and return the
    /// remaining characters of the reply as a string.
    string Exec(const string& cmd,
            bool multiline_output = false,
            STimeout* timeout = NULL,
            INetServerConnectionListener* conn_listener = NULL);
};


///////////////////////////////////////////////////////////////////////////
//
class NCBI_XCONNECT_EXPORT IEmbeddedStreamWriter : public IWriter
{
public:
    virtual void Close() = 0;
    virtual void Abort() = 0;
};

///////////////////////////////////////////////////////////////////////////
//
class NCBI_XCONNECT_EXPORT CNetServerInfo
{
    NCBI_NET_COMPONENT(NetServerInfo);

    /// Return the next attribute. If there are no more attributes,
    /// the method returns false, and attr_name and attr_value are
    /// left unchanged.
    bool GetNextAttribute(string& attr_name, string& attr_value);
};

NCBI_XCONNECT_EXPORT
CNetServerInfo g_ServerInfoFromString(const string& server_info);

///////////////////////////////////////////////////////////////////////////
//
class NCBI_XCONNECT_EXPORT CNetServer
{
    NCBI_NET_COMPONENT(NetServer);

    unsigned GetHost() const;
    unsigned short GetPort() const;
    string GetServerAddress() const;

    struct SExecResult {
        string response;
        CNetServerConnection conn;
    };

    /// Execute remote command 'cmd', wait for the reply,
    /// check if it starts with 'OK:', and return the
    /// remaining characters of the reply as a string.
    /// This method makes as many as TServConn_ConnMaxRetries
    /// attempts to connect to the server and execute
    /// the specified command.
    SExecResult ExecWithRetry(const string& cmd,
            bool multiline_output = false,
            INetServerConnectionListener* conn_listener = NULL);

    /// Retrieve basic information about the server as
    /// attribute name-value pairs.
    CNetServerInfo GetServerInfo();
};

///////////////////////////////////////////////////////////////////////////
//
class NCBI_XCONNECT_EXPORT CNetServerMultilineCmdOutput
{
    NCBI_NET_COMPONENT(NetServerMultilineCmdOutput);

    CNetServerMultilineCmdOutput(const CNetServer::SExecResult& exec_result);

    bool ReadLine(string& output);
};

#ifdef HAVE_LIBCONNEXT
#define NCBI_GRID_XSITE_CONN_SUPPORT 1
#endif


END_NCBI_SCOPE

#endif // CONNECT_SERVICES__SERVER_CONN_HPP
