#ifndef CONN___NETCACHE_KEY__HPP
#define CONN___NETCACHE_KEY__HPP

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
 * Authors:  Anatoliy Kuznetsov, Maxim Didenko
 *
 * File Description:
 *   Net cache client API.
 *
 */

/// @file netcache_client.hpp
/// NetCache client specs.
///

#include <connect/connect_export.h>
#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup NetCacheClient
 *
 * @{
 */


/// Meaningful information encoded in the NetCache key
///
///
struct NCBI_XCONNECT_EXPORT CNetCacheKey
{
public:
    CNetCacheKey(unsigned int  _id,
                 const string& _host,
                 unsigned int  _port,
                 unsigned int  ver = 1);
    /// Create the key out of string
    /// @sa ParseBlobKey()
    explicit CNetCacheKey(const string& key_str);

    operator string() const;

    /// Parse blob key string into a CNetCache_Key structure
    bool ParseBlobKey(const string& key_str);

    /// Generate blob key string
    ///
    /// Please note that "id" is an integer issued by the NetCache server.
    /// Clients should not use this function with custom ids.
    /// Otherwise it may disrupt the inter-server communication.
    static
    void GenerateBlobKey(string*        key,
                         unsigned int   id,
                         const string&  host,
                         unsigned short port);

    /// Parse blob key, extract id
    static
    unsigned int GetBlobId(const string& key_str);

    static
    bool IsValidKey(const string& key_str);

    unsigned int  GetId     (void) const;
    const string& GetHost   (void) const;
    unsigned int  GetPort   (void) const;
    unsigned int  GetVersion(void) const;

private:
    CNetCacheKey(void);

    unsigned int m_Id;        ///< BLOB id
    string       m_Host;      ///< server name
    unsigned int m_Port;      ///< TCP/IP port number
    unsigned int m_Version;   ///< Key version
};


//////////////////////////////////////////////////////////////////////////
// Inline functions
//////////////////////////////////////////////////////////////////////////

inline
CNetCacheKey::CNetCacheKey(unsigned int  _id,
                           const string& _host,
                           unsigned int  _port,
                           unsigned int  ver)
    : m_Id(_id), m_Host(_host), m_Port(_port), m_Version(ver)
{}

inline unsigned int
CNetCacheKey::GetId(void) const
{
    return m_Id;
}

inline const string&
CNetCacheKey::GetHost(void) const
{
    return m_Host;
}

inline unsigned int
CNetCacheKey::GetPort(void) const
{
    return m_Port;
}

inline unsigned int
CNetCacheKey::GetVersion(void) const
{
    return m_Version;
}


/* @} */


END_NCBI_SCOPE

#endif  /* CONN___NETCACHE_KEY__HPP */
