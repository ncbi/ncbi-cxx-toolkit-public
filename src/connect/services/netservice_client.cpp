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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Implementation of Net Service client base classes.
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_system.hpp>

#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/services/netservice_client.hpp>

#include <memory>


BEGIN_NCBI_SCOPE


static 
STimeout s_DefaultCommTimeout = {6, 0};



CNetServiceClient::CNetServiceClient(const string& client_name)
    : m_Sock(0),
      m_OwnSocket(eNoOwnership),
      m_ClientName(client_name),
      m_Timeout(s_DefaultCommTimeout)
{
}

CNetServiceClient::CNetServiceClient(const string&  host,
                                     unsigned short port,
                                     const string&  client_name)
    : m_Sock(0),
      m_Host(host),
      m_Port(port),
      m_OwnSocket(eNoOwnership),
      m_ClientName(client_name),
      m_Timeout(s_DefaultCommTimeout)
{
}

CNetServiceClient::CNetServiceClient(CSocket*      sock,
                                     const string& client_name)
    : m_Sock(sock),
      m_Host(kEmptyStr),
      m_Port(0),
      m_OwnSocket(eNoOwnership),
      m_ClientName(client_name),
      m_Timeout(s_DefaultCommTimeout)
{
    if (m_Sock) {
        m_Sock->DisableOSSendDelay();
        RestoreHostPort();
    }
}

CNetServiceClient::~CNetServiceClient()
{
    if (m_OwnSocket == eTakeOwnership) {
        delete m_Sock;
    }
}

void CNetServiceClient::SetDefaultCommunicationTimeout(const STimeout& to)
{
    s_DefaultCommTimeout = to;
}

void CNetServiceClient::SetCommunicationTimeout(const STimeout& to)
{
    m_Timeout = to;
    if (m_Sock) {
        m_Sock->SetTimeout(eIO_ReadWrite, &m_Timeout);
    }
}
STimeout& CNetServiceClient::SetCommunicationTimeout() 
{ 
    return m_Timeout; 
}

STimeout CNetServiceClient::GetCommunicationTimeout() const
{ 
    return m_Timeout;
}

void CNetServiceClient::RestoreHostPort()
{
    unsigned int host;
    m_Sock->GetPeerAddress(&host, 0, eNH_NetworkByteOrder);
    m_Host = CSocketAPI::gethostbyaddr(host);
    string::size_type pos = m_Host.find_first_of(".");
    if (pos != string::npos) {
        m_Host.erase(pos, m_Host.length());
    }
	m_Sock->GetPeerAddress(0, &m_Port, eNH_HostByteOrder);
    //cerr << m_Host << " ";
}

void CNetServiceClient::SetSocket(CSocket* sock, EOwnership own)
{
    if (m_OwnSocket == eTakeOwnership) {
        delete m_Sock;
    }
    if ((m_Sock=sock) != 0) {
        m_Sock->DisableOSSendDelay();
        m_OwnSocket = own;
        RestoreHostPort();
    }
}

EIO_Status CNetServiceClient::Connect(unsigned int addr, unsigned short port)
{
    if (m_Sock) {

        m_Host = CSocketAPI::gethostbyaddr(addr);
        m_Port = port;
        m_Sock->Connect(m_Host, m_Port);
    } else {
        SetSocket(new CSocket(addr, port), eTakeOwnership);
    }
    return m_Sock->GetStatus(eIO_Open);
}

CSocket* CNetServiceClient::DetachSocket() 
{
    CSocket* s = m_Sock; m_Sock = 0; return s; 
}

bool CNetServiceClient::ReadStr(CSocket& sock, string* str)
{
    _ASSERT(str);

    EIO_Status io_st = sock.ReadLine(*str);
    switch (io_st) 
    {
    case eIO_Success:
        return true;
    case eIO_Timeout:
        {
        string msg = "Communication timeout reading from server.";
        NCBI_THROW(CNetServiceException, eTimeout, msg);
        }
        break;
    default: // invalid socket or request, bailing out
        return false;
    };

    return true;    
}

void CNetServiceClient::WriteStr(const char* str, size_t len)
{
    const char* buf_ptr = str;
    size_t size_to_write = len;
    while (size_to_write) {
        size_t n_written;
        EIO_Status io_st = m_Sock->Write(buf_ptr, size_to_write, &n_written);
        NCBI_IO_CHECK(io_st);
        size_to_write -= n_written;
        buf_ptr       += n_written;
    } // while
}

void CNetServiceClient::CreateSocket(const string& hostname,
                                     unsigned      port)
{
    if (m_Sock == 0) {
        m_Sock = new CSocket(hostname, port);
        m_Sock->SetTimeout(eIO_ReadWrite, &m_Timeout);
    } else {
        unsigned conn_repeats = 0;
        const unsigned max_repeats = 3;
        
        do {
            EIO_Status io_st = 
                m_Sock->Connect(hostname, port, &m_Timeout, eOn);
            
            if (io_st != eIO_Success) {
                if (io_st == eIO_Unknown) {
                    
                    // most likely this is an indication we have too many 
                    // open ports on the client side 
                    // (this kernel limitation manifests itself on Linux)
                    //
                    
                    if (++conn_repeats > max_repeats) {
                        NCBI_IO_CHECK(io_st);
                    }
                    // give system a chance to recover
                    
                    SleepMilliSec(1000 * conn_repeats);
                } else {
                    NCBI_IO_CHECK(io_st);
                }
            } else {
                break;
            }
            
        } while (1);
        
        m_Sock->SetDataLogging(eDefault);
        
    }
    m_Sock->DisableOSSendDelay();
    m_OwnSocket = eTakeOwnership;

    m_Host = hostname;
    m_Port = port;
}

void CNetServiceClient::WaitForServer()
{
    STimeout to = {2, 0};
    while (true) {
        EIO_Status io_st = m_Sock->Wait(eIO_Read, &to);
        if (io_st == eIO_Timeout)
            continue;
        else 
            break;            
    }
}

void CNetServiceClient::TrimErr(string* err_msg)
{
    _ASSERT(err_msg);
    if (err_msg->find("ERR:") == 0) {
        err_msg->erase(0, 4);
        *err_msg = NStr::ParseEscapes(*err_msg);
    }
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2005/03/22 18:54:07  kuznets
 * Changed project tree layout
 *
 * Revision 1.6  2005/03/17 17:17:29  kuznets
 * +Connect()
 *
 * Revision 1.5  2005/02/28 12:19:15  kuznets
 * +Unescaping error messages
 *
 * Revision 1.4  2005/02/14 13:49:33  kuznets
 * Added delay if client cannot connect to the server when there are no available TCP/IP ports
 *
 * Revision 1.3  2005/02/11 14:28:27  kuznets
 * Cleanup.Removed unused static function
 *
 * Revision 1.2  2005/02/09 18:58:59  kuznets
 * +TrimErr() method
 *
 * Revision 1.1  2005/02/07 12:59:10  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
