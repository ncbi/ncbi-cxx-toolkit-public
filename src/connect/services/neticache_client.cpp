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
 *   Implementation of netcache ICache client.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>
#include <connect/services/neticache_client.hpp>
#include <connect/services/netcache_client.hpp>
#include <util/transmissionrw.hpp>
#include <memory>


BEGIN_NCBI_SCOPE

CNetICacheClient::CNetICacheClient(const string&  host,
                                   unsigned short port,
                                   const string&  cache_name,
                                   const string&  client_name)
  : CNetServiceClient(host, port, client_name),
    m_CacheName(cache_name)
{
}

CNetICacheClient::~CNetICacheClient()
{}

void CNetICacheClient::CheckConnect()
{
    if (m_Sock && (eIO_Success == m_Sock->GetStatus(eIO_Open))) {
        return; // we are connected, nothing to do
    }
    if (!m_Host.empty()) { // we can restore connection
        CreateSocket(m_Host, m_Port);
        return;
    }
    NCBI_THROW(CNetServiceException, eCommunicationError,
        "Cannot establish connection with a server. Unknown host name.");

}

void CNetICacheClient::MakeCommandPacket(string* out_str, 
                                         const string& cmd_str) const
{
    _ASSERT(out_str);

    if (m_ClientName.length() < 3) {
        NCBI_THROW(CNetCacheException, 
                   eAuthenticationError, "Client name is too small or empty");
    }
    if (m_CacheName.empty()) {
        NCBI_THROW(CNetCacheException, 
                   eAuthenticationError, "Cache name unknown");
    }

    *out_str = m_ClientName;
    const string& client_name_comment = GetClientNameComment();
    if (!client_name_comment.empty()) {
        out_str->append(" ");
        out_str->append(client_name_comment);
    }
    out_str->append("\r\n");
    out_str->append("IC(");
    out_str->append(m_CacheName);
    out_str->append(") ");
    out_str->append(cmd_str);
}

void CNetICacheClient::TrimPrefix(string* str) const
{
    CheckOK(str);
    str->erase(0, 3); // "OK:"
}


void CNetICacheClient::CheckOK(string* str) const
{
    if (str->find("OK:") != 0) {
        // Answer is not in "OK:....." format
        string msg = "Server error:";
        TrimErr(str);

        if (str->find("Cache unknown") != string::npos) {
            NCBI_THROW(CNetCacheException, eUnknnownCache, *str);
        }

        msg += *str;
        NCBI_THROW(CNetServiceException, eCommunicationError, msg);
    }
}





void CNetICacheClient::SetTimeStampPolicy(TTimeStampFlags policy,
                                          unsigned int    timeout,
                                          unsigned int    max_timeout)
{
    CheckConnect();
    CSockGuard sg(*m_Sock);

    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "STSP ");
    cmd.append(NStr::IntToString(policy));
    cmd.append(" ");
    cmd.append(NStr::IntToString(timeout));
    cmd.append(" ");
    cmd.append(NStr::IntToString(max_timeout));

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    CheckOK(&m_Tmp);
}


ICache::TTimeStampFlags CNetICacheClient::GetTimeStampPolicy()
{
    CheckConnect();
    CSockGuard sg(*m_Sock);

    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "GTSP ");

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    ICache::TTimeStampFlags flags = atoi(m_Tmp.c_str());
    return flags;
}

int CNetICacheClient::GetTimeout()
{
    CheckConnect();
    CSockGuard sg(*m_Sock);

    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "GTOU ");
    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    int to = atoi(m_Tmp.c_str());
    return to;
}


bool CNetICacheClient::IsOpen() const
{
    // need this trick to get over intrinsic non-const-ness of 
    // the network protocol
    CNetICacheClient* cl = const_cast<CNetICacheClient*>(this);
    cl->CheckConnect();
    CSockGuard sg(*m_Sock);

    string cmd;
    MakeCommandPacket(&cmd, "ISOP ");
    cl->WriteStr(cmd.c_str(), cmd.length() + 1);
    cl->WaitForServer();
    string answer;
    if (!cl->ReadStr(*m_Sock, &answer)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&answer);

    int to = atoi(answer.c_str());
    return to != 0;

}

void CNetICacheClient::SetVersionRetention(EKeepVersions policy)
{
    CheckConnect();
    CSockGuard sg(*m_Sock);
    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "SVRP ");
    switch (policy) {
    case ICache::eKeepAll:
        cmd.append("KA");
        break;
    case ICache::eDropOlder:
        cmd.append("DO");
        break;
    case ICache::eDropAll:
        cmd.append("DA");
        break;
    default:
        _ASSERT(0);
    }
    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    CheckOK(&m_Tmp);
}

ICache::EKeepVersions CNetICacheClient::GetVersionRetention() const
{
    CNetICacheClient* cl = const_cast<CNetICacheClient*>(this);
    cl->CheckConnect();
    CSockGuard sg(*m_Sock);

    string cmd;
    MakeCommandPacket(&cmd, "ISOP ");
    cl->WriteStr(cmd.c_str(), cmd.length() + 1);
    cl->WaitForServer();
    string answer;
    if (!cl->ReadStr(*m_Sock, &answer)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&answer);
    int to = atoi(answer.c_str());

    return (ICache::EKeepVersions)to;
}


void CNetICacheClient::Store(const string&  key,
                             int            version,
                             const string&  subkey,
                             const void*    data,
                             size_t         size,
                             unsigned int   time_to_live,
                             const string&  owner)
{
    auto_ptr<IWriter> wrt(GetWriteStream(key,
                                         version,
                                         subkey,
                                         time_to_live,
                                         owner));
    _ASSERT(wrt.get());
    size_t     bytes_written;
    const char* ptr = (const char*) data;
    while (size) {
        ERW_Result res = wrt->Write(ptr, size, &bytes_written);
        if (res != eRW_Success) {
            NCBI_THROW(CNetServiceException, eCommunicationError, 
                       "Communication error");
        }
        ptr += bytes_written;
        size -= bytes_written;
    }
}

size_t CNetICacheClient::GetSize(const string&  key,
                                 int            version,
                                 const string&  subkey)
{
    CheckConnect();
    CSockGuard sg(*m_Sock);

    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "GSIZ ");
    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    size_t sz = (size_t)atoi(m_Tmp.c_str());
    return sz;
}


void CNetICacheClient::GetBlobOwner(const string&  key,
                                    int            version,
                                    const string&  subkey,
                                    string*        owner)
{
    CheckConnect();
    CSockGuard sg(*m_Sock);

    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "GBLW ");
    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    *owner = m_Tmp;
}

bool CNetICacheClient::Read(const string& key,
                            int           version,
                            const string& subkey,
                            void*         buf,
                            size_t        buf_size)
{
    auto_ptr<IReader> rdr(GetReadStream(key, version, subkey));

    if (rdr.get() == 0) {
        return false;
    }
    size_t x_blob_size = 0;
    size_t x_read = 0;

    size_t buf_avail = buf_size;
    unsigned char* buf_ptr = (unsigned char*) buf;

    while (buf_avail) {
        size_t bytes_read;
        ERW_Result rw_res = rdr->Read(buf_ptr, buf_avail, &bytes_read);
        switch (rw_res) {
        case eRW_Success:
            buf_avail -= bytes_read;
            buf_ptr   += bytes_read;
            break;
        case eRW_Eof:
            if (x_read == 0)
                return false;
            return true;
        case eRW_Timeout:
            break;
        default:
            return false;
        } // switch
    } // while

    return true;
}

void CNetICacheClient::GetBlobAccess(const string&     key,
                                     int               version,
                                     const string&     subkey,
                                     BlobAccessDescr*  blob_descr)
{
    blob_descr->reader = GetReadStream(key, version, subkey);
    // TO DO: fill blob_size and blob_found methods
}


IWriter* CNetICacheClient::GetWriteStream(const string&    key,
                                          int              version,
                                          const string&    subkey,
                                          unsigned int     time_to_live,
                                          const string&    /*owner*/)
{
    CheckConnect();
    CSockGuard sg(*m_Sock);
    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "STOR ");
    cmd.append(NStr::UIntToString(time_to_live));
    cmd.append(" ");
    cmd.append(NStr::UIntToString(version));
    cmd.append(" ");
    cmd.append(subkey);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    CNetCacheSock_RW* writer = new CNetCacheSock_RW(m_Sock);
    DetachSocket();
    writer->OwnSocket();

    CNetCache_WriterErrCheck* err_writer =
            new CNetCache_WriterErrCheck(writer, eTakeOwnership, 0);
    return err_writer;
}


void CNetICacheClient::Remove(const string& key)
{
    CheckConnect();
    CSockGuard sg(*m_Sock);
    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "REMK ");
    cmd.append(key);
    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    CheckOK(&m_Tmp);
}

void CNetICacheClient::Remove(const string&    key,
                              int              version,
                              const string&    subkey)
{
    CheckConnect();
    CSockGuard sg(*m_Sock);
    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "REMO ");
    cmd.append(key);
    cmd.append(" ");
    cmd.append(NStr::UIntToString(version));
    cmd.append(" ");
    cmd.append(subkey);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    CheckOK(&m_Tmp);
}

time_t CNetICacheClient::GetAccessTime(const string&  key,
                                       int            version,
                                       const string&  subkey)
{
    CheckConnect();
    CSockGuard sg(*m_Sock);
    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "GACT ");
    cmd.append(key);
    cmd.append(" ");
    cmd.append(NStr::UIntToString(version));
    cmd.append(" ");
    cmd.append(subkey);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    return NStr::StringToInt(m_Tmp);
}

bool CNetICacheClient::HasBlobs(const string&  key,
                                const string&  subkey)
{
    CheckConnect();
    CSockGuard sg(*m_Sock);
    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "HASB ");
    cmd.append(key);
    cmd.append(" ");
    cmd.append("0");
    cmd.append(" ");
    cmd.append(subkey);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    return NStr::StringToInt(m_Tmp) != 0;
}

void CNetICacheClient::Purge(time_t           access_timeout,
                             EKeepVersions    keep_last_version)
{
}

void CNetICacheClient::Purge(const string&    key,
                             const string&    subkey,
                             time_t           access_timeout,
                             EKeepVersions    keep_last_version)
{
}

string CNetICacheClient::GetCacheName(void) const
{
    return m_CacheName;
}

bool CNetICacheClient::SameCacheParams(const TCacheParams* params) const
{
    return false;
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/01/03 15:36:44  kuznets
 * Added network ICache client
 *
 *
 * ===========================================================================
 */
