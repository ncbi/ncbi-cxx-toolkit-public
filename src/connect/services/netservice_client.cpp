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
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/services/netservice_client.hpp>
#include <connect/services/netservice_api_expt.hpp>
#include <corelib/ncbi_param.hpp>
#include <memory>


BEGIN_NCBI_SCOPE

NCBI_PARAM_DECL(bool,   service_connector, use_linger2);
//NCBI_PARAM_DEF (bool,   service_connector, use_linger2, false);
typedef NCBI_PARAM_TYPE(service_connector, use_linger2) TServConn_UserLinger2;

NCBI_PARAM_DECL(double, service_connector, communication_timeout);
//NCBI_PARAM_DEF (double, service_connector, communication_timeout, 12.0);
typedef NCBI_PARAM_TYPE(service_connector, communication_timeout) TServConn_CommTimeout;

NCBI_PARAM_DECL(unsigned int, service_connector, connection_max_retries);
//NCBI_PARAM_DEF (unsigned int, service_connector, connection_max_retries, 10);
typedef NCBI_PARAM_TYPE(      service_connector, connection_max_retries) TServConn_ConnMaxRetries;

static bool s_DefaultCommTimeout_Initialized = false;
static STimeout s_DefaultCommTimeout;

static STimeout s_GetDefaultCommTimeout()
{
    if (s_DefaultCommTimeout_Initialized)
        return s_DefaultCommTimeout;
    double ftm = TServConn_CommTimeout::GetDefault();
    NcbiMsToTimeout(&s_DefaultCommTimeout, (unsigned long)(ftm * 1000.0 + 0.5));
    s_DefaultCommTimeout_Initialized = true;
    return s_DefaultCommTimeout;
}
static void s_SetDefaultCommTimeout(const STimeout& tm)
{
    s_DefaultCommTimeout = tm;
    s_DefaultCommTimeout_Initialized = true;
}

//static unsigned int s_DefaultMaxRetries = 10;


static string  s_GlobalClientName;
static CNetServiceClient::EUseName  s_UseName = 
                        CNetServiceClient::eUseName_Both;


void CNetServiceClient::SetGlobalName(const string&  global_name,
                                      EUseName       use_name)
{
    s_GlobalClientName = global_name;
    s_UseName = use_name;
}


const string& CNetServiceClient::GetGlobalName()
{
    return s_GlobalClientName;
}


CNetServiceClient::EUseName CNetServiceClient::GetNameUse()
{
    return s_UseName;
}


CNetServiceClient::CNetServiceClient(const string& client_name)
    : m_Sock(0),
      m_OwnSocket(eNoOwnership),
      m_Timeout(s_GetDefaultCommTimeout()),
      m_MaxRetries(TServConn_ConnMaxRetries::GetDefault())
{
    if ((s_UseName == eUseName_Both) && !s_GlobalClientName.empty()) {
        m_ClientName = s_GlobalClientName + "::" + client_name;
    } else {
        m_ClientName = client_name;
    }
}


CNetServiceClient::CNetServiceClient(const string&  host,
                                     unsigned short port,
                                     const string&  client_name)
    : m_Sock(0),
      m_Host(host),
      m_Port(port),
      m_OwnSocket(eNoOwnership),
      m_Timeout(s_GetDefaultCommTimeout()),
      m_MaxRetries(TServConn_ConnMaxRetries::GetDefault())
{
    if ((s_UseName == eUseName_Both) && !s_GlobalClientName.empty()) {
        m_ClientName = s_GlobalClientName + "::" + client_name;
    } else {
        m_ClientName = client_name;
    }
}


CNetServiceClient::CNetServiceClient(CSocket*      sock,
                                     const string& client_name)
    : m_Sock(sock),
      m_Host(kEmptyStr),
      m_Port(0),
      m_OwnSocket(eNoOwnership),
      m_ClientName(client_name),
      m_Timeout(s_GetDefaultCommTimeout()),
      m_MaxRetries(TServConn_ConnMaxRetries::GetDefault())
{
    if ((s_UseName == eUseName_Both) && !s_GlobalClientName.empty()) {
        m_ClientName = s_GlobalClientName + "::" + client_name;
    } else {
        m_ClientName = client_name;
    }

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

/* static */
void CNetServiceClient::SetDefaultCreateSocketMaxReties(unsigned int retries)
{
    TServConn_ConnMaxRetries::SetDefault(retries);
}

void CNetServiceClient::SetDefaultCommunicationTimeout(const STimeout& to)
{
    s_SetDefaultCommTimeout(to);
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
    m_Host = CSocketAPI::ntoa(host);
    /*
    m_Host = CSocketAPI::gethostbyaddr(host);
    string::size_type pos = m_Host.find_first_of(".");
    if (pos != string::npos) {
        m_Host.erase(pos, m_Host.length());
    }
    */
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
        if ( TServConn_UserLinger2::GetDefault() ) {
           STimeout zero = {0,0};
           m_Sock->SetTimeout(eIO_Close,&zero);
        }
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
    }

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
    EIO_Status io_st;
    if (m_Sock == 0) {
        m_Sock = new CSocket();
        if ( TServConn_UserLinger2::GetDefault() ) {
           STimeout zero = {0,0};
           m_Sock->SetTimeout(eIO_Close,&zero);
        }
        m_OwnSocket = eTakeOwnership;
    } //else {

    unsigned conn_repeats = 0;
    
    do {
        io_st = m_Sock->Connect(hostname, port, &m_Timeout, eOn);
        if (io_st != eIO_Success) {
            if (io_st == eIO_Unknown) {

                m_Sock->Close();
                
                // most likely this is an indication we have too many 
                // open ports on the client side 
                // (this kernel limitation manifests itself on Linux)
                //
                
                if (++conn_repeats > m_MaxRetries) {
                    if ( io_st != eIO_Success) {
                        throw CIO_Exception(DIAG_COMPILE_INFO,
                        0, (CIO_Exception::EErrCode)io_st, 
                            "IO error. Failed to connect to server.");
                    } 
                    //NCBI_IO_CHECK(io_st);
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
        
//    }
    m_Sock->SetTimeout(eIO_ReadWrite, &m_Timeout);
    m_Sock->DisableOSSendDelay();

    m_Host = hostname;
    m_Port = port;
}


void CNetServiceClient::WaitForServer(unsigned wait_sec)
{
    STimeout to = {wait_sec, 0};
    if ( wait_sec == 0)
        to = m_Timeout;
    while (true) {
        EIO_Status io_st = m_Sock->Wait(eIO_Read, &to);
        if (io_st == eIO_Timeout) {
            NCBI_THROW(CNetServiceException, eTimeout, 
                       "No response from the server for " + NStr::IntToString(to.sec) + " sec.");
        }
        else {
            break;
        }
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


void CNetServiceClient::PrintServerOut(CNcbiOstream & out)
{
    WaitForServer();
    while (1) {
        if (!ReadStr(*m_Sock, &m_Tmp))
            break;
        CheckServerOK(&m_Tmp);
        if (m_Tmp == "END")
            break;
        out << m_Tmp << "\n";
    }
}


void CNetServiceClient::CheckServerOK(string* response)
{
    if (NStr::StartsWith(*response, "OK:")) {
        m_Tmp.erase(0, 3); // "OK:"
    } else if (NStr::StartsWith(*response, "ERR:")) {
        ProcessServerError(response, eTrimErr);
    }
}


void CNetServiceClient::ProcessServerError(string* response, ETrimErr trim_err)
{
    if (trim_err == eTrimErr)
        TrimErr(response);
    NCBI_THROW(CNetServiceException, eCommunicationError, *response);
}


void CNetServiceClient::ReturnSocket(CSocket* sock, 
                                     const string& /* blob_comment */)
{
    _ASSERT(sock);
    CFastMutexGuard guard(m_SockPool_Lock);
    m_SockPool.Put(sock);
}


CSocket* CNetServiceClient::GetPoolSocket()
{
    CFastMutexGuard guard(m_SockPool_Lock);
    return m_SockPool.GetIfAvailable();
}


END_NCBI_SCOPE
