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
 *   Implementation of net cache client.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>
#include <connect/netcache_client.hpp>
#include <memory>


BEGIN_NCBI_SCOPE

const string kNetCache_KeyPrefix = "NCID";

void CNetCache_ParseBlobKey(CNetCache_Key* key, const string& key_str)
{
    _ASSERT(key);

    // NCID_01_1_MYHOST_9000

    const char* ch = key_str.c_str();
    key->hostname = key->prefix = kEmptyStr;

    // prefix

    for (;*ch && *ch != '_'; ++ch) {
        key->prefix += *ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, kEmptyStr);
    }
    ++ch;

    if (key->prefix != kNetCache_KeyPrefix) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, "Invalid prefix");
    }

    // version
    key->version = atoi(ch);
    while (*ch && *ch != '_') {
        ++ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, kEmptyStr);
    }
    ++ch;

    // id
    key->id = atoi(ch);
    while (*ch && *ch != '_') {
        ++ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, kEmptyStr);
    }
    ++ch;


    // hostname
    for (;*ch && *ch != '_'; ++ch) {
        key->hostname += *ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, kEmptyStr);
    }
    ++ch;

    // port
    key->port = atoi(ch);
}


CNetCacheClient::CNetCacheClient(const string&  client_name)
    : m_Sock(0),
      m_OwnSocket(eTakeOwnership),
      m_ClientName(client_name)
{
}


CNetCacheClient::CNetCacheClient(const string&  host,
                                 unsigned short port,
                                 const string&  client_name)
    : m_Sock(0),
      m_Host(host),
      m_Port(port),
      m_OwnSocket(eTakeOwnership),
      m_ClientName(client_name)
{
    CreateSocket(m_Host, m_Port);
}

void CNetCacheClient::CreateSocket(const string& hostname,
                                   unsigned      port)
{
    m_Sock = new CSocket(hostname, port);
    m_Sock->DisableOSSendDelay();
    m_OwnSocket = eTakeOwnership;
}


CNetCacheClient::CNetCacheClient(CSocket*      sock, 
                                 const string& client_name)
    : m_Sock(sock),
      m_Host(kEmptyStr),
      m_Port(0),
      m_OwnSocket(eNoOwnership),
      m_ClientName(client_name)
{
    if (m_Sock) {
        m_Sock->DisableOSSendDelay();

        unsigned int host;
        m_Sock->GetPeerAddress(&host, 0, eNH_NetworkByteOrder);
        m_Host = CSocketAPI::ntoa(host);
	    m_Sock->GetPeerAddress(0, &m_Port, eNH_HostByteOrder);
    }
}


CNetCacheClient::~CNetCacheClient()
{
    if (m_OwnSocket) {
        delete m_Sock;
    }
}


static
void s_WaitForServer(CSocket& sock)
{
    STimeout to = {2, 0};
    while (true) {
        EIO_Status io_st = sock.Wait(eIO_Read, &to);
        if (io_st == eIO_Timeout)
            continue;
        else 
            break;            
    }
}        

string CNetCacheClient::PutData(const void*  buf,
                                size_t       size,
                                unsigned int time_to_live)
{
    string blob_id;
    string request;
    
    const char* client = 
        !m_ClientName.empty() ? m_ClientName.c_str() : "noname";

    request = client;
    request.append("\r\nPUT");    

    if (time_to_live) {
        request += NStr::IntToString(time_to_live);
    }
   
    WriteStr(request.c_str(), request.length() + 1);
    s_WaitForServer(*m_Sock);

    // Read BLOB_ID answer from the server
    ReadStr(*m_Sock, &blob_id);
    if (NStr::FindCase(blob_id, "ID:") != 0) {
        // Answer is not in "ID:....." format
        NCBI_THROW(CNetCacheException, eCommunicationError, blob_id);
    }
    blob_id.erase(0, 3);
    
    if (blob_id.empty())
        NCBI_THROW(CNetCacheException, eCommunicationError, blob_id);

    // Write the actual BLOB
    WriteStr((const char*) buf, size);

    m_Sock->Close();

    return blob_id;
}


bool CNetCacheClient::IsAlive()
{
    string version = ServerVersion();
    return !version.empty();
}

string CNetCacheClient::ServerVersion()
{
    string request;
    string version;
    
    const char* client = 
        !m_ClientName.empty() ? m_ClientName.c_str() : "noname";

    request = client;
    request.append("\r\nVERSION");    

    WriteStr(request.c_str(), request.length() + 1);
    s_WaitForServer(*m_Sock);

    // Read BLOB_ID answer from the server
    ReadStr(*m_Sock, &version);
    if (NStr::FindCase(version, "OK:") != 0) {
        NCBI_THROW(CNetCacheException, eCommunicationError, version);
    }

    version.erase(0, 3);
    return version;
}


void CNetCacheClient::WriteStr(const char* str, size_t len)
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


void CNetCacheClient::SendClientName()
{
    const char* client = 
        !m_ClientName.empty() ? m_ClientName.c_str() : "noname";

    unsigned client_len = ::strlen(client);
    if (client_len) {
        WriteStr(client, client_len + 1);
    }
}

void CNetCacheClient::Remove(const string& key)
{
    string request;
    
    const char* client = 
        !m_ClientName.empty() ? m_ClientName.c_str() : "noname";

    request = client;
    request.append("\r\nREMOVE");    
    request += key;
    WriteStr(request.c_str(), request.length() + 1);
}


IReader* CNetCacheClient::GetData(const string& key)
{
    string request;
    
    const char* client = 
        !m_ClientName.empty() ? m_ClientName.c_str() : "noname";

    request = client;
    request.append("\r\nGET");    
    
    request += key;
    WriteStr(request.c_str(), request.length() + 1);

    s_WaitForServer(*m_Sock);
    
    string answer;
    bool res = ReadStr(*m_Sock, &answer);
    if (res) {
        if (NStr::strncmp(answer.c_str(), "OK:", 3) != 0) {
            NCBI_THROW(CNetCacheException, eCommunicationError, answer);
        }
    }

    IReader* reader = new CSocketReaderWriter(m_Sock);
    return reader;
}


CNetCacheClient::EReadResult
CNetCacheClient::GetData(const string&  key,
                         void*          buf, 
                         size_t         buf_size, 
                         size_t*        n_read)
{
    _ASSERT(buf && buf_size);

    auto_ptr<IReader> reader(GetData(key));
    if (reader.get() == 0)
        return CNetCacheClient::eNotFound;

    size_t buf_avail = buf_size;
    unsigned char* buf_ptr = (unsigned char*) buf;

    if (n_read)
        *n_read = 0;

    while (buf_avail) {
        size_t bytes_read;
        ERW_Result rw_res = reader->Read(buf_ptr, buf_avail, &bytes_read);
        switch (rw_res) {
        case eRW_Success:
            if (n_read)
                *n_read += bytes_read;
            buf_avail -= bytes_read;
            buf_ptr   += bytes_read;
            break;
        case eRW_Eof:
            return CNetCacheClient::eReadComplete;
        case eRW_Timeout:
            break;
        default:
            return CNetCacheClient::eNotFound;
        } // switch
    } // while

    return CNetCacheClient::eReadPart;
}


void CNetCacheClient::ShutdownServer()
{
    SendClientName();

    const char command[] = "SHUTDOWN";
    WriteStr(command, sizeof(command));

    m_Sock->Close();
}


bool CNetCacheClient::ReadStr(CSocket& sock, string* str)
{
    _ASSERT(str);

    str->erase();
    str->erase();
    char ch;
    EIO_Status io_st;

    char szBuf[1024] = {0,};
    unsigned str_len = 0;
    size_t n_read = 0;

    for (bool flag = true; flag; ) {
        io_st = sock.Read(szBuf, 256, &n_read, eIO_ReadPeek);
        switch (io_st) 
        {
        case eIO_Success:
            flag = false;
            break;
        case eIO_Timeout:
            NCBI_THROW(CNetCacheException, eTimeout, kEmptyStr);
        default: // invalid socket or request, bailing out
            return false;
        };
    }

    for (str_len = 0; str_len < n_read; ++str_len) {
        ch = szBuf[str_len];
        if (ch == 0 || ch == '\n' || ch == 13) {
            break;
        }
        *str += ch;
    }

    if (str_len == 0) {
        return false;
    }
    io_st = sock.Read(szBuf, str_len + 2);
    return true;
}


bool CNetCacheClient::IsError(const char* str)
{
    int cmp = NStr::strncasecmp(str, "ERR:", 4);
    return cmp == 0;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2004/10/28 16:16:20  kuznets
 * +CNetCacheClient::Remove()
 *
 * Revision 1.12  2004/10/27 14:16:59  kuznets
 * BLOB key parser moved from netcached
 *
 * Revision 1.11  2004/10/25 14:36:39  kuznets
 * New methods IsAlive(), ServerVersion()
 *
 * Revision 1.10  2004/10/21 15:52:56  kuznets
 * removed unused variable
 *
 * Revision 1.9  2004/10/20 14:51:05  kuznets
 * Optimization in networking
 *
 * Revision 1.8  2004/10/20 13:46:22  kuznets
 * Optimized networking
 *
 * Revision 1.7  2004/10/13 14:46:38  kuznets
 * Optimization in the networking
 *
 * Revision 1.6  2004/10/08 12:30:35  lavr
 * Cosmetics
 *
 * Revision 1.5  2004/10/06 16:49:10  ucko
 * CNetCacheClient::ReadStr: clear str with erase(), since clear() isn't
 * 100% portable.
 *
 * Revision 1.4  2004/10/06 15:27:24  kuznets
 * Removed unused variable
 *
 * Revision 1.3  2004/10/05 19:02:05  kuznets
 * Implemented ShutdownServer()
 *
 * Revision 1.2  2004/10/05 18:18:46  kuznets
 * +GetData, fixed bugs in protocol
 *
 * Revision 1.1  2004/10/04 18:44:59  kuznets
 * Initial revision
 *
 * ===========================================================================
 */
