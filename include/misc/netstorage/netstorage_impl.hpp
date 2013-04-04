#ifndef MISC_NETSTORAGE___NETSTORAGE_IMPL__HPP
#define MISC_NETSTORAGE___NETSTORAGE_IMPL__HPP

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
 * Author:  Dmitry Kazimirov
 *
 * File Description:
 *   NetStorage implementation declarations.
 *
 */

#include "netstorage.hpp"

BEGIN_NCBI_SCOPE

/// @internal
enum ENetFileIDFields {
    fNFID_KeyAndNamespace   = (1 << 0),
    fNFID_NetICache         = (1 << 1),
    fNFID_AllowXSiteConn    = (1 << 2),
    fNFID_TTL               = (1 << 3),
};
///< @internal Bitwise OR of ENetFileIDFields
typedef unsigned char TNetFileIDFields;

/// @internal
class NCBI_XCONNECT_EXPORT CNetFileID
{
public:
    CNetFileID(TNetStorageFlags flags, Uint8 random_number);
    CNetFileID(TNetStorageFlags flags,
            const string& domain_name, const string& unique_key);
    CNetFileID(const string& packed_id);

    void SetStorageFlags(TNetStorageFlags storage_flags)
    {
        m_StorageFlags = storage_flags;
        m_Dirty = true;
    }

    TNetStorageFlags GetStorageFlags() const {return m_StorageFlags;}
    TNetFileIDFields GetFields() const {return m_Fields;}

    Uint8 GetTimestamp() const {return m_Timestamp;}
    Uint8 GetRandom() const {return m_Random;}

    string GetDomainName() const {return m_DomainName;}
    string GetKey() const {return m_Key;}

    string GetUniqueKey() {return m_UniqueKey;}

    void SetNetICacheClient(CNetICacheClient::TInstance icache_client);

    CNetICacheClient GetNetICacheClient() const {return m_NetICacheClient;}
    Uint4 GetNetCacheIP() const {return m_NetCacheIP;}
    Uint2 GetNetCachePort() const {return m_NetCachePort;}

    void SetCacheChunkSize(size_t cache_chunk_size)
    {
        m_CacheChunkSize = cache_chunk_size;
        m_Dirty = true;
    }

    Uint8 GetCacheChunkSize() const {return m_CacheChunkSize;}

    void SetTTL(Uint8 ttl)
    {
        if (ttl == 0)
            ClearFieldFlags(fNFID_TTL);
        else {
            m_TTL = ttl;
            SetFieldFlags(fNFID_TTL);
        }
        m_Dirty = true;
    }

    Uint8 GetTTL() const {return m_TTL;}

    string GetID()
    {
        if (m_Dirty)
            Pack();
        return m_PackedID;
    }

private:
    void x_SetUniqueKeyFromRandom();
    void x_SetUniqueKeyFromUserDefinedKey();

    void Pack();
    void SetFieldFlags(TNetFileIDFields flags) {m_Fields |= flags;}
    void ClearFieldFlags(TNetFileIDFields flags) {m_Fields &= ~flags;}

    TNetStorageFlags m_StorageFlags;
    TNetFileIDFields m_Fields;

    Uint8 m_Timestamp;
    Uint8 m_Random;

    string m_DomainName;
    string m_Key;

    string m_UniqueKey;

    CNetICacheClient m_NetICacheClient;
    Uint4 m_NetCacheIP;
    Uint2 m_NetCachePort;
    Uint8 m_CacheChunkSize;
    Uint8 m_TTL;

    bool m_Dirty;

    string m_PackedID;
};

END_NCBI_SCOPE

#endif  /* MISC_NETSTORAGE___NETSTORAGE_IMPL__HPP */
