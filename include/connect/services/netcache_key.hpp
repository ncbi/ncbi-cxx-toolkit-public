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

/// @file netcache_key.hpp
/// NetCache client specs.
///

#include "compound_id.hpp"

#include <connect/connect_export.h>

#include <corelib/ncbistl.hpp>
#include <corelib/ncbitype.h>

#include <string>

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
    /// Blob and blob key features.
    enum ENCKeyFlag {
        fNCKey_SingleServer = 1 << 0,   ///< Mark this blob as not mirrored.
        fNCKey_NoServerCheck = 1 << 1,  ///< Disable the check for whether
                                        ///< the server IP is still in service.
    };
    typedef unsigned TNCKeyFlags;       ///< Binary OR of ENCKeyFlag.

    /// Create the key out of string
    explicit CNetCacheKey(const string& key_str,
            CCompoundIDPool::TInstance id_pool = NULL);

    /// Create an empty object for later use with ParseBlobKey() or Assign().
    CNetCacheKey() {}

    /// Parse the specified blob ID and initializes this object.
    /// @throws CNetCacheException if key format is not recognized.
    void Assign(const string& key_str,
            CCompoundIDPool::TInstance id_pool = NULL);

    /// Parse blob key string into a CNetCacheKey structure
    static bool ParseBlobKey(const char* key_str,
            size_t key_len, CNetCacheKey* key_obj,
            CCompoundIDPool::TInstance id_pool = NULL);

    bool HasExtensions() const {return m_PrimaryKeyLength < m_Key.length();}

    /// If the blob key has been parsed successfully,
    /// this method returns a trimmed "base" version
    /// of the key with "0MetA0" extensions removed.
    string StripKeyExtensions() const;

    /// Unconditionally append a service name to the specified string.
    static void AddExtensions(string& blob_id, const string& service_name,
            TNCKeyFlags flags);

    TNCKeyFlags GetFlags() const {return m_Flags;}
    bool GetFlag(ENCKeyFlag flag) const {return (m_Flags & flag) != 0;}
    void SetFlag(ENCKeyFlag flag) {m_Flags |= (TNCKeyFlags) flag;}
    void ClearFlag(ENCKeyFlag flag) {m_Flags &= ~(TNCKeyFlags) flag;}

    /// Generate blob key string
    ///
    /// Please note that "id" is an integer issued by the NetCache server.
    /// Clients should not use this function with custom ids.
    /// Otherwise it may disrupt the inter-server communication.
    static
    void GenerateBlobKey(string*        key,
                         unsigned int   id,
                         const string&  host,
                         unsigned short port,
                         unsigned int   ver = 1);
    static
    void GenerateBlobKey(string*        key,
                         unsigned int   id,
                         const string&  host,
                         unsigned short port,
                         unsigned int   ver,
                         unsigned int   rnd_num,
                         time_t         creation_time = 0);

    /// Generate a key with extensions.
    static void GenerateBlobKey(
        string* key,
        unsigned id,
        const string& host,
        unsigned short port,
        const string& service_name,
        TNCKeyFlags flags);

    static string KeyToCompoundID(
        const string& key_str,
        CCompoundIDPool id_pool);

    /// Parse blob key, extract id
    static unsigned int GetBlobId(const string& key_str);

    static bool IsValidKey(const char* key_str, size_t key_len,
            CCompoundIDPool::TInstance id_pool = NULL)
        { return ParseBlobKey(key_str, key_len, NULL, id_pool); }

    static bool IsValidKey(const string& key,
            CCompoundIDPool::TInstance id_pool = NULL)
        { return IsValidKey(key.c_str(), key.length(), id_pool); }

    const string& GetKey() const;
    unsigned GetId() const;
    const string& GetHost() const;
    unsigned short GetPort() const;
    unsigned GetVersion() const;
    time_t GetCreationTime() const;
    Uint4 GetRandomPart() const;
    const string& GetServiceName() const;

private:
    string m_Key;
    unsigned int m_Id; ///< BLOB id
    string m_Host; ///< server name
    unsigned short m_Port; ///< TCP/IP port number
    unsigned m_Version; ///< Key version
    time_t m_CreationTime;
    Uint4 m_Random;
    size_t m_PrimaryKeyLength;

    // Key extensions
    string m_ServiceName;
    TNCKeyFlags m_Flags;
};


//////////////////////////////////////////////////////////////////////////
// Inline functions
//////////////////////////////////////////////////////////////////////////

inline string CNetCacheKey::StripKeyExtensions() const
{
    return HasExtensions() ? string(m_Key.data(), m_PrimaryKeyLength) : m_Key;
}

inline const string& CNetCacheKey::GetKey() const
{
    return m_Key;
}

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

inline unsigned short
CNetCacheKey::GetPort(void) const
{
    return m_Port;
}

inline unsigned int
CNetCacheKey::GetVersion(void) const
{
    return m_Version;
}

inline time_t CNetCacheKey::GetCreationTime() const
{
    return m_CreationTime;
}

inline Uint4 CNetCacheKey::GetRandomPart() const
{
    return m_Random;
}

inline const string& CNetCacheKey::GetServiceName() const
{
    return m_ServiceName;
}


/* @} */


END_NCBI_SCOPE

#endif  /* CONN___NETCACHE_KEY__HPP */
