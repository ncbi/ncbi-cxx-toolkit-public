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
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>
#include <connect/netcache_client.hpp>
#include <memory>


BEGIN_NCBI_SCOPE


CNetCacheClient::CNetCacheClient(const string&  host,
                                 unsigned short port,
                                 const string&  client_name)
    : m_Sock(0),
      m_Host(host),
      m_Port(port),
      m_OwnSocket(eTakeOwnership),
      m_ClientName(client_name)
{
    m_Sock = new CSocket(m_Host, m_Port);
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

    SendClientName();

    string request = "PUT ";
    if (time_to_live) {
        request += NStr::IntToString(time_to_live);
    }
   
    WriteStr(request.c_str(), request.length() + 1);
    
    s_WaitForServer(*m_Sock);

    // Read BLOB_ID answer from the server
    ReadStr(*m_Sock, &blob_id);
    if (NStr::FindCase(blob_id, "ID:") != 0) {
        // Answer is not in "ID:....." format
        // (Most likely it is an error)
        return kEmptyStr;
    }
    blob_id.erase(0, 3);
    
    if (blob_id.empty())
        return kEmptyStr;

    // Write the actual BLOB
    WriteStr((const char*) buf, size);

    m_Sock->Close();

    return blob_id;
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


IReader* CNetCacheClient::GetData(const string& key)
{
    SendClientName();

    string request = "GET ";
    request += key;
    WriteStr(request.c_str(), request.length() + 1);

    s_WaitForServer(*m_Sock);
    
    string answer;
    bool res = ReadStr(*m_Sock, &answer);
    if (res) {
        if (NStr::strncmp(answer.c_str(), "OK:", 3) != 0) {
            return 0;
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

    int loop_cnt = 0;
    do {
        EIO_Status io_st;
        size_t n_read;
        char ch;
        for (;;) {
             
            io_st = sock.Read(&ch, 1, &n_read, eIO_ReadPlain);
            switch (io_st) {
            case eIO_Success:
                if (ch == 0 || ch == '\r' || ch == '\n') {
                    goto out_of_loop;
                }
                *str += ch;
                break;
            case eIO_Timeout:
                // TODO: add repetition counter or another protector here
                break;
            default: // invalid socket or request, bailing out
                return false;
            }
        }
out_of_loop:
        io_st = sock.Read(&ch, 1, &n_read);
        if (++loop_cnt > 10) // protection from a client feeding empty strings
            return false;
    } while (str->empty());

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
