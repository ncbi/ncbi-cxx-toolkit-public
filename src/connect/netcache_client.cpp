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

#include <connect/netcache_client.hpp>
#include <util/reader_writer.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>

BEGIN_NCBI_SCOPE

CNetCacheClient::CNetCacheClient(CSocket* sock, const char* client_name)
: m_Sock(sock),
  m_ClientName(client_name)
{
}

string CNetCacheClient::PutData(void* buf, size_t size)
{
    EIO_Status io_st;
    string blob_id;

    const char* client = m_ClientName ? m_ClientName : "noname";
    unsigned client_len = strlen(client);

    if (!client_len)
        return kEmptyStr;

    ++client_len; // to send ending 0

    io_st = m_Sock->Write(client, client_len);
    NCBI_IO_CHECK(io_st);

    const char* request = "PUT";
    unsigned request_len = strlen(request) + 1;

    io_st = m_Sock->Write(request, request_len);
    NCBI_IO_CHECK(io_st);

    // Read BLOB_ID answer from the server
    char szbuf[1024];
    size_t n_read;
    io_st = m_Sock->Read(szbuf, sizeof(szbuf) &n_read);
    NCBI_IO_CHECK(io_st);

    // TODO: check for error response
    bool is_error = IsError(szbuf);
    if (is_error) {
        LOG_POST(Error << szbuf);
        return kEmptyStr;
    }


    for (unsigned i = 0; i < (unsigned) n_read; ++i) {
        char ch = szbuf[i];
        if (ch == '\r' || ch == '\n')
            break;
        blob_id += ch;
    }

    if (blob_id.empty())
        return kEmptyStr;

    // Write the actual BLOB
    io_st = m_Sock->Write(buf, size);
    NCBI_IO_CHECK(io_st);

    m_Sock->Close();

    return blob_id;
}

IReader* CNetCacheClient::GetData(const string& key)
{
    IReader* reader = 0;
    EIO_Status io_st;
    string blob_id;

    const char* client = m_ClientName ? m_ClientName : "noname";
    unsigned client_len = strlen(client);

    if (!client_len)
        return 0;

    ++client_len; // to send ending 0

    io_st = m_Sock->Write(client, client_len);
    NCBI_IO_CHECK(io_st);


    string request = "GET ";
    request += key;
    io_st = m_Sock->Write(request.c_str(), request.length());
    NCBI_IO_CHECK(io_st);

    string answer;
    bool res = ReadStr(*m_Sock, &answer);
    if (res) {
        if (NStr::strncmp(answer.c_str(), "OK:", 3) != 0) {
        }
    }

    reader = new CSocketReaderWriter(m_Sock);

    return reader;
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
            io_st = sock.Read(&ch, 1, &bytes_read);
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
 * Revision 1.1  2004/10/04 18:44:59  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
