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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  Implementation of net cache client.
 *
 */

#include <ncbi_pch.hpp>


#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <memory>

#include <connect/netcache_client.hpp>
#include <util/reader_writer.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>

BEGIN_NCBI_SCOPE

CNetCacheClient::CNetCacheClient(const string& host,
                                 unsigned      port,
                                 const string& client_name)
 : m_Sock(0),
   m_ClientName(client_name),
   m_Host(host),
   m_Port(port)
{
    m_Sock = new CSocket(m_Host, m_Port);
    m_OwnSocket = true;
}


CNetCacheClient::CNetCacheClient(CSocket*      sock, 
                                 const string& client_name)
: m_Sock(sock),
  m_ClientName(client_name),
  m_OwnSocket(false)
{
}

CNetCacheClient::~CNetCacheClient()
{
    if (m_OwnSocket)
        delete m_Sock;
}


string CNetCacheClient::PutData(void*        buf, 
                                size_t       size, 
                                unsigned int time_to_live)
{
    string blob_id;

    SendClientName();

    string request = "PUT ";
    if (time_to_live) {
        request += NStr::IntToString(time_to_live);
    }
   
    WriteStr(request.c_str(), request.length()+1);

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
    WriteStr((char*)buf, size);

    m_Sock->Close();

    return blob_id;
}

void CNetCacheClient::WriteStr(const char* str, size_t len)
{
    EIO_Status io_st;
    size_t n_written;

    const char* buf_ptr = str;
    size_t size_to_write = len;
    while (size_to_write) {
        io_st = m_Sock->Write(buf_ptr, size_to_write, &n_written);
        NCBI_IO_CHECK(io_st);
        size_to_write -= n_written;
        buf_ptr += n_written;
    } // while

}
void CNetCacheClient::SendClientName()
{
    const char* client = 
        !m_ClientName.empty() ? m_ClientName.c_str() : "noname";
    unsigned client_len = strlen(client);

    if (!client_len)
        return;
    ++client_len; // to send ending 0

    WriteStr(client, client_len);
}

IReader* CNetCacheClient::GetData(const string& key)
{
    IReader* reader = 0;
    string blob_id;

    SendClientName();

    string request = "GET ";
    request += key;
    WriteStr(request.c_str(), request.length()+1);

    string answer;
    bool res = ReadStr(*m_Sock, &answer);
    if (res) {
        if (NStr::strncmp(answer.c_str(), "OK:", 3) != 0) {
            return 0;
        }
    }

    reader = new CSocketReaderWriter(m_Sock);

    return reader;
}

CNetCacheClient::EReadResult 
CNetCacheClient::GetData(const string&  key,
                         void*          buf, 
                         size_t         buf_size, 
                         size_t*        n_read)
{
    _ASSERT(buf && buf_size);

    auto_ptr<IReader> rdr(GetData(key));
    if (rdr.get() == 0) {
        return CNetCacheClient::eNotFound;
    }
    size_t bytes_read;
    size_t buf_avail = buf_size;

    unsigned char* buf_ptr = (unsigned char*) buf;

    if (n_read)
        *n_read = 0;

    while (buf_avail) {
        bytes_read = 0;
        ERW_Result rw_res = rdr->Read(buf_ptr, buf_avail, &bytes_read);
        switch (rw_res) {
        case eRW_Success:
            if (n_read)
                *n_read += bytes_read;
            buf_avail -= bytes_read;
            buf_ptr += bytes_read;
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
    const char* command = "SHUTDOWN";
    unsigned len = ::strlen(command) + 1;
    WriteStr(command, len);

    m_Sock->Close();
}

bool CNetCacheClient::ReadStr(CSocket& sock, string* str)
{
    _ASSERT(str);

    str->clear();
    char ch;
    EIO_Status io_st;
    size_t  bytes_read;
    unsigned loop_cnt = 0;

    do {
        do {
            io_st = sock.Read(&ch, 1, &bytes_read, eIO_ReadPlain);
            switch (io_st) 
            {
            case eIO_Success:
                if (ch == 0 || ch == '\n' || ch == 13) {
                    goto out_of_loop;
                }
                *str += ch;
                break;
            case eIO_Timeout:
                // TODO: add repetition counter or another protector here
                break;
            default: // invalid socket or request, bailing out
                return false;
            };
        } while (true);
out_of_loop:
        io_st = sock.Read(&ch, 1, &bytes_read);
        if (++loop_cnt > 10) // protection from a client feeding empty strings
            return false;
    } while (str->empty());

    return true;

}



bool CNetCacheClient::IsError(const char* str)
{
    int cmp = NStr::strncasecmp(str, "ERR:", 4);
    return (cmp == 0);
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
