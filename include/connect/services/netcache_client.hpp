#ifndef CONN___NETCACHE_CLIENT__HPP
#define CONN___NETCACHE_CLIENT__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Net cache client API.
 *
 */

/// @file netcache_client.hpp
/// NetCache client specs. 
///

#include <connect/connect_export.h>
#include <corelib/ncbistd.hpp>
#include <util/reader_writer.hpp>


BEGIN_NCBI_SCOPE


class CSocket;

/// NetCache internal exception
///
class CNetCacheException : public CException
{
public:
    enum EErrCode {
        eTimeout,
        eCommunicationError,
        eKeyFormatError
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eTimeout:            return "eTimeout";
        case eCommunicationError: return "eCommunicationError";
        case eKeyFormatError:     return "eKeyFormatError";
        default:                  return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetCacheException, CException);
};

/// Meaningful information encoded in the NetCache key
///
/// @sa CNetCache_ParseBlobKey
///
struct CNetCache_Key
{
    string       prefix;    ///< Key prefix
    unsigned     version;   ///< Key version
    unsigned     id;        ///< BLOB id
    string       hostname;  ///< server name
    unsigned     port;      ///< TCP/IP port number
};

/// Parse blob key string into a CNetCache_Key structure
extern NCBI_XCONNECT_EXPORT
void CNetCache_ParseBlobKey(CNetCache_Key* key, const string& key_str);

/// Generate blob key string
///
/// Please note that "id" is an integer issued by the netcache server.
/// Clients should not use this function with custom ids. 
/// Otherwise it may disrupt the interserver communication.
///
extern NCBI_XCONNECT_EXPORT
void CNetCache_GenerateBlobKey(string*        key, 
                               unsigned       id, 
                               const string&  host, 
                               unsigned short port);




/// Client API for NetCache server
///
/// After any Put, Get transactions connection socket
/// is closed (part of NetCache protocol implemenation)
///
class NCBI_XCONNECT_EXPORT CNetCacheClient
{
public:
    CNetCacheClient(const string&  client_name = kEmptyStr);

    CNetCacheClient(const string&  host,
                    unsigned short port,
                    const string&  client_name = kEmptyStr);

    /// Construction.
    /// @param sock
    ///    Connected socket to the server
    /// @param client_name
    ///    Identification name of connecting client
    CNetCacheClient(CSocket*      sock,
                    const string& client_name = kEmptyStr);

    ~CNetCacheClient();

    /// Put BLOB to server
    ///
    /// @param time_to_live
    ///    Timeout value in seconds. 0 - server side default assumed.
    /// 
    /// Please note that time_to_live is controlled by the server-side
    /// parameter so if you set time_to_live higher than server-side value,
    /// server side TTL will be in effect.
    string PutData(const void*   buf,
                   size_t        size,
                   unsigned int  time_to_live = 0);

    /// Update an existing BLOB
    ///
    string PutData(const string& key,
                   const void*   buf,
                   size_t        size,
                   unsigned int  time_to_live = 0);


    /// Retrieve BLOB from server by key
    /// If BLOB not found method returns NULL
    /// Caller is responsible for deletion of the IReader*
    ///
    /// @param key
    ///    BLOB key to read (returned by PutData)
    /// @return
    ///    IReader* (caller must delete this). 
    ///    When NULL BLOB was not found (expired).
    IReader* GetData(const string& key, 
                     size_t*       blob_size = 0);

    /// Remove BLOB
    void Remove(const string& key);

    enum EReadResult {
        eReadComplete, ///< The whole BLOB has been read
        eNotFound,     ///< BLOB not found or error
        eReadPart      ///< Read part of the BLOB (buffer capacity)
    };

    /// Retrieve BLOB from server by key
    ///
    /// @note
    ///    Function waits for enought data to arrive.
    EReadResult GetData(const string&  key,
                        void*          buf, 
                        size_t         buf_size, 
                        size_t*        n_read = 0,
                        size_t*        blob_size = 0);

    /// Shutdown the server daemon.
    void ShutdownServer();

    /// Connects to server to make sure it is running.
    bool IsAlive();

    /// Return version string
    string ServerVersion();

protected:
    bool ReadStr(CSocket& sock, string* str);
    void WriteStr(const char* str, size_t len);

    bool IsError(const char* str);

    void SendClientName();

    void CreateSocket(const string& hostname,
                      unsigned      port);

private:
    CSocket*       m_Sock;
    string         m_Host;
    unsigned short m_Port;
    EOwnership     m_OwnSocket;
    string         m_ClientName;
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2004/11/01 16:02:17  kuznets
 * GetData now returns BLOB size
 *
 * Revision 1.9  2004/11/01 14:39:30  kuznets
 * Implemented BLOB update
 *
 * Revision 1.8  2004/10/28 16:16:09  kuznets
 * +CNetCacheClient::Remove()
 *
 * Revision 1.7  2004/10/27 14:16:45  kuznets
 * BLOB key parser moved from netcached
 *
 * Revision 1.6  2004/10/25 14:36:03  kuznets
 * New methods IsAlive(), ServerVersion()
 *
 * Revision 1.5  2004/10/22 13:51:01  kuznets
 * Documentation chnages
 *
 * Revision 1.4  2004/10/08 12:30:34  lavr
 * Cosmetics
 *
 * Revision 1.3  2004/10/05 19:01:59  kuznets
 * Implemented ShutdownServer()
 *
 * Revision 1.2  2004/10/05 18:17:58  kuznets
 * comments, added GetData
 *
 * Revision 1.1  2004/10/04 18:44:33  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


#endif  /* CONN___NETCACHE_CLIENT__HPP */
