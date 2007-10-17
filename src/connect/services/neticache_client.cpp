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
#include <corelib/ncbitime.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/services/neticache_client.hpp>
#include <connect/services/error_codes.hpp>
#include <util/cache/icache_cf.hpp>
#include <memory>


#define NCBI_USE_ERRCODE_X   ConnServ_NetCache


BEGIN_NCBI_SCOPE


CNetICacheClient::CNetICacheClient()
  : TParent("localhost", 9000, "netcache_client"),
    m_CacheName("default_cache"),
    m_Throttler(5000, CTimeSpan(60,0))
{
    m_Timeout.sec = 180000;
}


CNetICacheClient::CNetICacheClient(const string&  host,
                                   unsigned short port,
                                   const string&  cache_name,
                                   const string&  client_name)
  : TParent(host, port, client_name),
    m_CacheName(cache_name),
    m_Throttler(5000, CTimeSpan(60,0))
{
    m_Timeout.sec = 180000;
}

void CNetICacheClient::SetConnectionParams(const string&  host,
                                           unsigned short port,
                                           const string&  cache_name,
                                           const string&  client_name)
{
    m_Host = host;
    m_Port = port;
    m_CacheName = cache_name;
    m_ClientName = client_name;
}


CNetICacheClient::~CNetICacheClient()
{
}


void CNetICacheClient::ReturnSocket(CSocket* sock, const string& blob_comments)
{
    // check if socket has unattended input
    {{
        STimeout to = {0, 0};
        EIO_Status io_st = sock->Wait(eIO_Read, &to);
        string line;
        if (io_st == eIO_Success) {
            sock->ReadLine(line);
            ERR_POST_X(7, "ReturnSocket detected unread input " << blob_comments << " :" << line);
            delete sock;
            return;
        }
    }}

	{{
    CFastMutexGuard guard(m_Lock);
    if (m_Sock == 0) {
        m_Sock = sock;
        return;
    }
	}}
	CNetServiceClient::ReturnSocket(sock, blob_comments);
}


bool CNetICacheClient::CheckConnect()
{
    if (!m_Sock) {
        m_Sock = GetPoolSocket();
    }

    if (m_Sock && (eIO_Success == m_Sock->GetStatus(eIO_Open))) {
		// check if netcache session is in OK state
		// we have to do that, because if client program failed to 
		// read the whole BLOB (deserialization error?) the network protocol
		// stucks in an incorrect state (needs to be closed)
		try {
			WriteStr("A?", 3);
            WaitForServer();
    		if (!ReadStr(*m_Sock, &m_Tmp)) {
				delete m_Sock;m_Sock = 0;
				return CheckConnect();
    		}
			if (m_Tmp[0] != 'O' || m_Tmp[1] != 'K') {
				delete m_Sock; m_Sock = 0;
				return CheckConnect();
			}
		} 
		catch (exception& ) {
			delete m_Sock; m_Sock = 0;
			return CheckConnect();
		}

        return false; // we are connected, nothing to do
    }
    if (!m_Host.empty()) { // we can restore connection
//        m_Throttler.Approve(CRequestRateControl::eSleep);
        CSocketAPI::SetReuseAddress(eOn);
        CreateSocket(m_Host, m_Port);
        m_Sock->SetReuseAddress(eOn);
        return true;
    }
    NCBI_THROW(CNetServiceException, eCommunicationError,
        "Cannot establish connection with a server. Unknown host name.");
}


void CNetICacheClient::MakeCommandPacket(string* out_str, 
                                         const string& cmd_str,
                                         bool          connected) const
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

    if (connected) {
        // connection has been re-established, 
        //   need to send authentication line
        *out_str = m_ClientName;
        const string& client_name_comment = GetClientNameComment();
        if (!client_name_comment.empty()) {
            out_str->append(" ");
            out_str->append(client_name_comment);
        }
        out_str->append("\r\n");
    } else {
        out_str->erase();
    }
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


/*
struct CStackGuard
{
   CStackGuard(const string& name_) : name(name_)
   {
	   ERR_POST_X(8, "->" << name);
   }
   ~CStackGuard()
   {
	   ERR_POST_X(9, "  " << name << "->");
   }

	string name;
};
*/


void CNetICacheClient::RegisterSession(unsigned pid) 
{
    CFastMutexGuard guard(m_Lock);
    bool reconnected = CheckConnect();
    if (reconnected) { 
        // connection reestablished
        string auth = m_ClientName;
        const string& client_name_comment = GetClientNameComment();
        if (!client_name_comment.empty()) {
            auth.append(" ");
            auth.append(client_name_comment);
        }
        WriteStr(auth.c_str(), auth.length() + 1);
    }
    TParent::RegisterSession(pid);
}


void CNetICacheClient::UnRegisterSession(unsigned pid)
{
    CFastMutexGuard guard(m_Lock);
    bool reconnected = CheckConnect();
    if (reconnected) { 
        // connection reestablished
        string auth = m_ClientName;
        const string& client_name_comment = GetClientNameComment();
        if (!client_name_comment.empty()) {
            auth.append(" ");
            auth.append(client_name_comment);
        }
        WriteStr(auth.c_str(), auth.length() + 1);
    }
    TParent::UnRegisterSession(pid);
}


void CNetICacheClient::SetTimeStampPolicy(TTimeStampFlags policy,
                                          unsigned int    timeout,
                                          unsigned int    max_timeout)
{
    CFastMutexGuard guard(m_Lock);

    CNetICacheClient* cl = const_cast<CNetICacheClient*>(this);
    bool reconnected = cl->CheckConnect();
    CSockGuard sg(*m_Sock);

    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "STSP ", reconnected);
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


ICache::TTimeStampFlags CNetICacheClient::GetTimeStampPolicy() const
{
    CFastMutexGuard guard(m_Lock);

    CNetICacheClient* cl = const_cast<CNetICacheClient*>(this);
    bool reconnected = cl->CheckConnect();
    CSockGuard sg(*m_Sock);

    string& cmd = cl->m_Tmp;
    MakeCommandPacket(&cmd, "GTSP ", reconnected);

    cl->WriteStr(cmd.c_str(), cmd.length() + 1);
    cl->WaitForServer();
    if (!cl->ReadStr(*m_Sock, &cl->m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&cl->m_Tmp);

    ICache::TTimeStampFlags flags = atoi(m_Tmp.c_str());
    return flags;
}


int CNetICacheClient::GetTimeout() const
{
    CFastMutexGuard guard(m_Lock);

    CNetICacheClient* cl = const_cast<CNetICacheClient*>(this);
    bool reconnected = cl->CheckConnect();
    CSockGuard sg(*m_Sock);

    string& cmd = cl->m_Tmp;
    MakeCommandPacket(&cmd, "GTOU ", reconnected);
    cl->WriteStr(cmd.c_str(), cmd.length() + 1);
    cl->WaitForServer();
    if (!cl->ReadStr(*m_Sock, &cl->m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&cl->m_Tmp);

    int to = atoi(m_Tmp.c_str());
    return to;
}


bool CNetICacheClient::IsOpen() const
{
    CFastMutexGuard guard(m_Lock);

    // need this trick to get over intrinsic non-const-ness of 
    // the network protocol
    CNetICacheClient* cl = const_cast<CNetICacheClient*>(this);
    bool reconnected = cl->CheckConnect();
    CSockGuard sg(*m_Sock);

    string cmd;
    MakeCommandPacket(&cmd, "ISOP ", reconnected);
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
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
    CSockGuard sg(*m_Sock);
    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "SVRP ", reconnected);
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
    CFastMutexGuard guard(m_Lock);

    CNetICacheClient* cl = const_cast<CNetICacheClient*>(this);
    bool reconnected = cl->CheckConnect();
    CSockGuard sg(*m_Sock);

    string cmd;
    MakeCommandPacket(&cmd, "ISOP ", reconnected);
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
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
//    CSockGuard sg(*m_Sock);
    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "STRS ", reconnected);
    cmd.append(NStr::UIntToString(time_to_live));
    cmd.push_back(' ');
    cmd.append(NStr::UIntToString(size));
    AddKVS(&cmd, key, version, subkey);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    {{
    auto_ptr<CNetCacheSock_RW> wrt(new CNetCacheSock_RW(m_Sock));
    wrt->SetSocketParent(this);
    DetachSocket();
    guard.Release();
    auto_ptr<CNetCache_WriterErrCheck> writer(
       new CNetCache_WriterErrCheck(wrt.release(), eTakeOwnership, 0));

    size_t     bytes_written;
    const char* ptr = (const char*) data;
    while (size) {
        ERW_Result res = writer->Write(ptr, size, &bytes_written);
        if (res != eRW_Success) {
            NCBI_THROW(CNetServiceException, eCommunicationError, 
                       "Communication error");
        }
        ptr += bytes_written;
        size -= bytes_written;
    }
    }}

}


size_t CNetICacheClient::GetSize(const string&  key,
                                 int            version,
                                 const string&  subkey)
{
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();

    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "GSIZ ", reconnected);
    AddKVS(&cmd, key, version, subkey);

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
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();

    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "GBLW ", reconnected);
    AddKVS(&cmd, key, version, subkey);

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
    size_t blob_size;
    auto_ptr<IReader> rdr;
    {{
        CFastMutexGuard guard(m_Lock);
        rdr.reset((GetReadStream_NoLock(key, version, subkey)));
        blob_size = m_BlobSize;
    }}
    

    if (rdr.get() == 0) {
        return false;
    }

    size_t x_read = 0;

    size_t buf_avail = buf_size;
    unsigned char* buf_ptr = (unsigned char*) buf;

    while (buf_avail && blob_size) {
        size_t bytes_read;
        ERW_Result rw_res = 
            rdr->Read(buf_ptr, buf_avail, &bytes_read);
        switch (rw_res) {
        case eRW_Success:
            buf_avail -= bytes_read;
            buf_ptr   += bytes_read;
            blob_size -= bytes_read;
            x_read += bytes_read;
            break;
        case eRW_Eof:
            return (x_read == blob_size);
        case eRW_Timeout:
            break;
        default:
            return false;
        } // switch
    } // while

    return (x_read == blob_size);
}


void CNetICacheClient::GetBlobAccess(const string&     key,
                                     int               version,
                                     const string&     subkey,
                                     SBlobAccessDescr* blob_descr)
{
    size_t blob_size;
    {{
        CFastMutexGuard guard(m_Lock);
        blob_descr->reader.reset(GetReadStream_NoLock(key, version, subkey));
        blob_size = m_BlobSize;
    }}
    if (blob_descr->reader.get()) {
        blob_descr->blob_size = blob_size;
		blob_descr->blob_found = true;

        if (blob_descr->buf && blob_descr->buf_size > blob_size) {
            // read to buffer
            size_t to_read = blob_size;
            char* buf_ptr = blob_descr->buf;
            while (to_read) {
                size_t nn_read;
                ERW_Result rw_res = 
                    blob_descr->reader->Read(buf_ptr, to_read, &nn_read);
                if (!nn_read || rw_res != eRW_Success) {
                    blob_descr->reader.reset(0);
                    NCBI_THROW(CNetServiceException, eCommunicationError, 
                               "Communication error");
                }
                buf_ptr += nn_read;
                to_read -= nn_read;
            } // while
            blob_descr->reader.reset(0);
        }
    } else {
        blob_descr->blob_size = 0;
		blob_descr->blob_found = false;
    }
}


IWriter* CNetICacheClient::GetWriteStream(const string&    key,
                                          int              version,
                                          const string&    subkey,
                                          unsigned int     time_to_live,
                                          const string&    /*owner*/)
{
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
    CSockGuard sg(*m_Sock);
    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "STOR ", reconnected);
    cmd.append(NStr::UIntToString(time_to_live));
    AddKVS(&cmd, key, version, subkey);

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
    sg.Release();
    writer->SetSocketParent(this);
    CNetCache_WriterErrCheck* err_writer =
        new CNetCache_WriterErrCheck(writer, eTakeOwnership, 0, CTransmissionWriter::eSendEofPacket);

    // Add BLOB stream comments (diagnostics):
    //      key-version-subkey[writer]
    //
    m_Tmp.erase();
    m_Tmp.append(key);
    m_Tmp.push_back('-');
    m_Tmp.append(NStr::IntToString(version));
    m_Tmp.push_back('-');
    m_Tmp.append(subkey);
    m_Tmp.append("[writer]");

    writer->SetBlobComment(m_Tmp);

    return err_writer;
}


void CNetICacheClient::Remove(const string& key)
{
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
    CSockGuard sg(*m_Sock);
    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "REMK ", reconnected);
    cmd.push_back('"');
    cmd.append(key);
    cmd.push_back('"');
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
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
    CSockGuard sg(*m_Sock);
    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "REMO ", reconnected);
    AddKVS(&cmd, key, version, subkey);

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
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
    CSockGuard sg(*m_Sock);
    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "GACT ", reconnected);
    AddKVS(&cmd, key, version, subkey);

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
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "HASB ", reconnected);
    AddKVS(&cmd, key, 0, subkey);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();

    string& answer = m_Tmp;
    if (!ReadStr(*m_Sock, &answer)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    TrimPrefix(&answer);

    return NStr::StringToInt(answer) != 0;
}


void CNetICacheClient::Purge(time_t           access_timeout,
                             EKeepVersions    keep_last_version)
{
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "PRG1 ", reconnected);
    cmd.append(NStr::IntToString(access_timeout));
    cmd.append(" ");
    cmd.append(NStr::IntToString((int)keep_last_version));
    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    CheckOK(&m_Tmp);
}


void CNetICacheClient::Purge(const string&    key,
                             const string&    subkey,
                             time_t           access_timeout,
                             EKeepVersions    keep_last_version)
{
}


IReader* 
CNetICacheClient::GetReadStream_NoLock(const string&  key,
                                       int            version,
                                       const string&  subkey)
{
    bool reconnected = CheckConnect();
    CSockGuard sg(*m_Sock);
    string& cmd = m_Tmp;
    MakeCommandPacket(&cmd, "READ ", reconnected);
    AddKVS(&cmd, key, version, subkey);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    string& answer = m_Tmp;
    if (!ReadStr(*m_Sock, &answer)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }

    bool blob_found = CNetCacheClient::CheckErrTrim(answer);
    if (!blob_found) {
        sg.Release();
        return NULL;
    }
    string::size_type pos = answer.find("SIZE=");
    if (pos != string::npos) {
        const char* ch = answer.c_str() + pos + 5;
        m_BlobSize = (size_t)atoi(ch);
    } else {
        m_BlobSize = 0;
    }

    sg.Release();
    auto_ptr<CNetCacheSock_RW> rw(new CNetCacheSock_RW(m_Sock, m_BlobSize));
    
    rw->OwnSocket();
    DetachSocket();
    rw->SetSocketParent(this);

    // Add BLOB stream comments (diagnostics):
    //      key-version-subkey(BLOB size)
    //
    m_Tmp.erase();
    m_Tmp.append(key);
    m_Tmp.push_back('-');
    m_Tmp.append(NStr::IntToString(version));
    m_Tmp.push_back('-');
    m_Tmp.append(subkey);
    m_Tmp.push_back('(');
    m_Tmp.append(NStr::UIntToString(m_BlobSize));
    m_Tmp.push_back(')');

    rw->SetBlobComment(m_Tmp);

    return rw.release();
}


IReader* CNetICacheClient::GetReadStream(const string&  key,
                                         int            version,
                                         const string&  subkey)
{
    CFastMutexGuard guard(m_Lock);
    return GetReadStream_NoLock(key, version, subkey);
}


void CNetICacheClient::AddKVS(string*          out_str, 
                              const string&    key,
                              int              version,
                              const string&    subkey) const
{
    _ASSERT(out_str);

    out_str->push_back('"');
    out_str->append(key);
    out_str->push_back('"');
    out_str->append(" ");
    out_str->append(NStr::UIntToString(version));
    out_str->append(" ");
    out_str->push_back('"');
    out_str->append(subkey);
    out_str->push_back('"');
}



string CNetICacheClient::GetCacheName(void) const
{
    CFastMutexGuard guard(m_Lock);

    return m_CacheName;
}


bool CNetICacheClient::SameCacheParams(const TCacheParams* params) const
{
    CFastMutexGuard guard(m_Lock);

    return false;
}


const char* kNetICacheDriverName = "netcache";

/// Class factory for NetCache implementation of ICache
///
/// @internal
///
class CNetICacheCF : public CICacheCF<CNetICacheClient>
{
public:
    typedef CICacheCF<CNetICacheClient> TParent;
public:
    CNetICacheCF() : TParent(kNetICacheDriverName, 0)
    {
    }
    ~CNetICacheCF()
    {
    }

    virtual
    ICache* CreateInstance(
                   const string&    driver  = kEmptyStr,
                   CVersionInfo     version = NCBI_INTERFACE_VERSION(ICache),
                   const TPluginManagerParamTree* params = 0) const;

};


static const string kCFParam_server          = "server";
static const string kCFParam_host            = "host";
static const string kCFParam_port            = "port";
static const string kCFParam_cache_name2     = "cache_name";
static const string kCFParam_cache_name      = "name";
static const string kCFParam_client          = "client";


ICache* CNetICacheCF::CreateInstance(
           const string&                  driver,
           CVersionInfo                   version,
           const TPluginManagerParamTree* params) const
{
    auto_ptr<CNetICacheClient> drv;
    if (driver.empty() || driver == m_DriverName) {
        if (version.Match(NCBI_INTERFACE_VERSION(ICache))
                            != CVersionInfo::eNonCompatible) {
            drv.reset(new CNetICacheClient());
        }
    } else {
        return 0;
    }

    if (!params)
        return drv.release();

    // cache client configuration

    string host;
    try {
        host =
            GetParam(params, kCFParam_server, true);
    } 
    catch (exception&)
    {
        host =
            GetParam(params, kCFParam_host, true);
    }
    int port =
        GetParamInt(params,kCFParam_port, true, 9000);
    string cache_name; 
    try {
        cache_name =
            GetParam(params, kCFParam_cache_name, true);
    } 
    catch (exception&)
    {
        cache_name =
            GetParam(params, kCFParam_cache_name2, true);
    }

    const string& client_name =
        GetParam(params, kCFParam_client, true);

    drv->SetConnectionParams(host, port, cache_name, client_name);

    return drv.release();
}


void NCBI_EntryPoint_xcache_netcache(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CNetICacheCF>::NCBI_EntryPointImpl(info_list, method);
}


void Cache_RegisterDriver_NetCache(void)
{
    RegisterEntryPoint<ICache>( NCBI_EntryPoint_xcache_netcache );
}

END_NCBI_SCOPE
