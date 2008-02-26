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
 *   Implementation of net BVStore client.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/services/netbvstore_client.hpp>
#include <memory>


BEGIN_NCBI_SCOPE



CNetBVStoreClientBase::CNetBVStoreClientBase(const string& client_name)
    : CNetServiceClient(client_name),
      m_CheckAlive(false)
{
}


CNetBVStoreClientBase::CNetBVStoreClientBase(const string&  host,
                                         unsigned short     port,
                                         const string&      store_name,
                                         const string&      client_name)
    : CNetServiceClient(host, port, client_name),
      m_StoreName(store_name),
      m_CheckAlive(false)
{
}

CNetBVStoreClient::~CNetBVStoreClient()
{}

bool CNetBVStoreClientBase::CheckAlive()
{
    _ASSERT(m_Sock);
	try {
		WriteStr("A?", 3);
		WaitForServer();
    	if (!ReadStr(*m_Sock, &m_Tmp)) {
			delete m_Sock;m_Sock = 0;
            return false;
    	}
		if (m_Tmp[0] != 'O' && m_Tmp[1] != 'K') {
			delete m_Sock; m_Sock = 0;
			return false;
		}
	}
	catch (exception&) {
		delete m_Sock; m_Sock = 0;
		return false;
	}
    return true;
}


void CNetBVStoreClientBase::CheckOK(string* str) const
{
    _ASSERT(0);
}


void CNetBVStoreClientBase::TrimPrefix(string* str) const
{
    _ASSERT(0);
}



CNetBVStoreClient::CNetBVStoreClient(const string&  host,
                                     unsigned short port,
                                     const string&  store_name,
                                     const string&  client_name)
 : CNetBVStoreClientBase(host, port, store_name, client_name)
{
}


bool CNetBVStoreClient::ReadRealloc(unsigned id,
                                    vector<char>& buffer, size_t* buf_size,
                                    unsigned from,
                                    unsigned to)
{
    size_t bsize=0;

    /*bool reconnected = */CheckConnect();
    string& cmd = m_Tmp;
    cmd = "BGET ";
    cmd.append(NStr::UIntToString(id));
    cmd.push_back(' ');
    cmd.append(NStr::UIntToString(from));
    cmd.push_back(' ');
    cmd.append(NStr::UIntToString(to));

    WriteStr(cmd.c_str(), cmd.length() + 1);

    WaitForServer();

    string& answer = m_Tmp;

    bool res = ReadStr(*m_Sock, &answer);
    if (res) {
        bool blob_found = x_CheckErrTrim(answer);

        if (!blob_found) {
            return false;
        }
        string::size_type pos = answer.find("SIZE=");
        if (pos != string::npos) {
            const char* ch = answer.c_str() + pos + 5;
            bsize = (size_t)atoi(ch);

            if (buf_size) {
                *buf_size = bsize;
            }
        }
    } else {
        NCBI_THROW(CNetServiceException,
                   eCommunicationError, "Communication error");
    }
    if (bsize > buffer.size()) {
        buffer.resize(bsize);
    }
    char* ptr = &buffer[0];

    while (bsize) {
        size_t nn_read = 0;
        EIO_Status io_st = m_Sock->Read(ptr, bsize, &nn_read);
        switch (io_st)
        {
        case eIO_Success:
            break;
        case eIO_Timeout:
            if (nn_read == 0) {
                NCBI_THROW(CNetServiceException, eTimeout, kEmptyStr);
            }
            break;
        case eIO_Closed:
            return false;
            break;
        default: // invalid socket or request
            NCBI_THROW(CNetServiceException, eCommunicationError, kEmptyStr);
        };

        ptr += nn_read;
        bsize -= nn_read;
    } // while
    return true;
}


bool CNetBVStoreClient::CheckConnect()
{
    if (m_Sock && (eIO_Success == m_Sock->GetStatus(eIO_Open))) {

        if (m_CheckAlive) {
            bool alive = CNetBVStoreClientBase::CheckAlive();
            if (alive) {
                return false; // we are connected, nothing to do
            } else {
                return CheckConnect();
            }
        } else {
			return false; // presumably, we are connected
		}
    }
    if (!m_Host.empty()) { // we can restore connection
        CSocketAPI::SetReuseAddress(eOn);
        CreateSocket(m_Host, m_Port);
        m_Sock->SetReuseAddress(eOn);

        WriteStr(m_ClientName.c_str(), m_ClientName.length() + 1);

        WriteStr(m_StoreName.c_str(), m_StoreName.length() + 1);

        return true;
    }
    NCBI_THROW(CNetServiceException, eCommunicationError,
        "Cannot establish connection with a server. Unknown host name.");

}


bool CNetBVStoreClient::x_CheckErrTrim(string& answer)
{
    if (NStr::strncmp(answer.c_str(), "OK:", 3) == 0) {
        answer.erase(0, 3);
    } else {
        string msg = "Server error:";
        if (NStr::strncmp(answer.c_str(), "ERR:", 4) == 0) {
            answer.erase(0, 4);
        }
        if (NStr::strncmp(answer.c_str(), "BLOB not found", 14) == 0) {
            return false;
        }

        msg += answer;
        NCBI_THROW(CNetBVStoreException, eServerError, msg);
    }
    return true;
}




END_NCBI_SCOPE
