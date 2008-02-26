#ifndef CONN___NETBVS_CLIENT__HPP
#define CONN___NETBVS_CLIENT__HPP

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
 *   Net BVStore client API.
 *
 */

/// @file netcache_client.hpp
/// NetCache client specs.
///

#include <connect/ncbi_conn_reader_writer.hpp>
#include <connect/services/netservice_client.hpp>
#include <connect/services/netservice_api_expt.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/version.hpp>


BEGIN_NCBI_SCOPE


/// Base client class for NetBVStore
///
class NCBI_XCONNECT_EXPORT CNetBVStoreClientBase : public CNetServiceClient
{
public:
    CNetBVStoreClientBase(const string& client_name);

    CNetBVStoreClientBase(const string&  host,
                          unsigned short port,
                          const string&  store_name,
                          const string&  client_name);
    void SetCheckAlive(bool on_off) { m_CheckAlive = on_off; }

protected:
    bool CheckAlive();

    virtual
    void CheckOK(string* str) const;
    virtual
    void TrimPrefix(string* str) const;
protected:
    string  m_StoreName;
    bool    m_CheckAlive;
};




/// NetCache internal exception
///
class NCBI_XCONNECT_EXPORT CNetBVStoreException : public CNetServiceException
{
public:
    typedef CNetServiceException TParent;
    enum EErrCode {
        ///< If client is not allowed to run this operation
        eAuthenticationError,
        ///< Server side error
        eServerError,
        ///< Cache name unknown
        eUnknnownCache
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eAuthenticationError: return "eAuthenticationError";
        case eServerError:         return "eServerError";
        case eUnknnownCache:       return "eUnknnownCache";
        default:                   return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetBVStoreException, CNetServiceException);
};


/// Client to NetBVStore server
///
/// @note This implementation is thread safe and syncronized
///
class NCBI_XCONNECT_EXPORT CNetBVStoreClient : public CNetBVStoreClientBase
{
public:
    typedef CNetBVStoreClientBase TParent;
public:
    CNetBVStoreClient();
    CNetBVStoreClient(const string&  host,
                      unsigned short port,
                      const string&  store_name,
                      const string&  client_name);
    virtual ~CNetBVStoreClient();

    bool ReadRealloc(unsigned id,
                     vector<char>& buffer, size_t* buf_size,
                     unsigned  from,
                     unsigned  to);

protected:
    /// Connect to server
    /// Function returns true if connection has been re-established
    /// flase if connection has been established before and
    /// throws an exception if it cannot establish connection
    bool CheckConnect();
private:
    bool x_CheckErrTrim(string& answer);
    string  m_Tmp;
};


END_NCBI_SCOPE

#endif
