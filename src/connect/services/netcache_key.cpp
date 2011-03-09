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
 *   Implementation of net cache client.
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/netcache_key.hpp>
#include <connect/services/netcache_api_expt.hpp>

#include <util/random_gen.hpp>
#include <corelib/ncbistr.hpp>


BEGIN_NCBI_SCOPE


static CRandom s_NCKeyRandom((CRandom::TValue(time(NULL))));

#define KEY_PREFIX "NCID_"
#define KEY_PREFIX_LENGTH (sizeof(KEY_PREFIX) - 1)

#define SERVICE_PREFIX "_0MetA0_S_"
#define SERVICE_PREFIX_LENGTH (sizeof(SERVICE_PREFIX) - 1)

#define PARSE_NUMERIC_KEY_PART(part_name) \
    const char* const part_name = ch; \
    while (ch != ch_end && isdigit(*ch)) \
        ++ch; \
    if (ch == ch_end  ||  *ch != '_') \
        return false; \
    ++ch;

inline bool
CNetCacheKey::ParseBlobKey(const char* key_str,
    size_t key_len, CNetCacheKey* key_obj)
{
    if (memcmp(key_str, KEY_PREFIX, KEY_PREFIX_LENGTH) != 0)
        return false;

    const char* ch      = key_str + KEY_PREFIX_LENGTH;
    const char* ch_end  = key_str + key_len;

    // version
    PARSE_NUMERIC_KEY_PART(ver_str);

    // id
    PARSE_NUMERIC_KEY_PART(id_str);

    // hostname
    const char* const hostname = ch;
    while (ch != ch_end  &&  *ch != '_')
        ++ch;
    if (ch == ch_end  ||  *ch != '_')
        return false;
    if (key_obj) {
        key_obj->m_Key.assign(key_str, ch_end);
        key_obj->m_Version = atoi(ver_str);
        key_obj->m_Id = atoi(id_str);
        key_obj->m_Host.assign(hostname, ch - hostname);
    }
    ++ch;

    // port
    PARSE_NUMERIC_KEY_PART(port_str);

    // Creation time
    PARSE_NUMERIC_KEY_PART(creation_time_str);

    // The last part of the primary key -- the random number.
    const char* const random_str = ch;
    while (ch != ch_end  &&  isdigit(*ch))
        ++ch;

    if (key_obj) {
        key_obj->m_Port = atoi(port_str);
        key_obj->m_CreationTime = (time_t) strtoul(creation_time_str, NULL, 10);
        key_obj->m_Random = (Uint4) strtoul(random_str, NULL, 10);
        key_obj->m_PrimaryKeyLength = ch - key_str;
    }

    // Key extensions
    if (ch == ch_end) {
        if (key_obj)
            key_obj->m_ServiceName = kEmptyStr;
    } else {
        if (memcmp(ch, SERVICE_PREFIX, SERVICE_PREFIX_LENGTH) != 0)
            return false;
        if (key_obj)
            key_obj->m_ServiceName = ch + SERVICE_PREFIX_LENGTH;
    }

    return true;
}

string CNetCacheKey::StripKeyExtensions() const
{
    return HasExtensions() ? string(m_Key.data(), m_PrimaryKeyLength) : m_Key;
}

void CNetCacheKey::AppendServiceName(string& blob_id,
    const string& service_name)
{
    blob_id.append(SERVICE_PREFIX);
    blob_id.append(service_name);
}

void CNetCacheKey::SetServiceName(const string& service_name)
{
    if (HasExtensions()) {
        int old_service_name_pos = m_PrimaryKeyLength + SERVICE_PREFIX_LENGTH;
        m_Key.replace(old_service_name_pos,
            m_Key.length() - old_service_name_pos, service_name);
    } else
        AppendServiceName(m_Key, service_name);
}

CNetCacheKey::CNetCacheKey(const string& key_str)
{
    Assign(key_str);
}

void CNetCacheKey::Assign(const string& key_str)
{
    m_Key = key_str;

    if (!ParseBlobKey(key_str.c_str(), key_str.size(), this)) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, "Key syntax error.");
    }
}

CNetCacheKey::operator string() const
{
    string key;
    GenerateBlobKey(&key, m_Id, m_Host, m_Port);
    return key;
}

void
CNetCacheKey::GenerateBlobKey(string*        key,
                              unsigned int   id,
                              const string&  host,
                              unsigned short port,
                              unsigned int   ver /* = 1 */)
{
    key->assign(KEY_PREFIX, KEY_PREFIX_LENGTH);

    string tmp;

    NStr::IntToString(tmp, ver);
    key->append(tmp);

    NStr::IntToString(tmp, id);
    key->append(1, '_');
    key->append(tmp);

    key->append(1, '_');
    key->append(host);

    NStr::IntToString(tmp, port);
    key->append(1, '_');
    key->append(tmp);

    NStr::IntToString(tmp, (long) ::time(0));
    key->append(1, '_');
    key->append(tmp);

    NStr::UIntToString(tmp, s_NCKeyRandom.GetRand());
    key->append(1, '_');
    key->append(tmp);
}

void CNetCacheKey::GenerateBlobKey(string* key, unsigned id,
    const string& host, unsigned short port, const string& service_name)
{
    GenerateBlobKey(key, id, host, port);
    key->append(SERVICE_PREFIX);
    key->append(service_name);
}

unsigned int
CNetCacheKey::GetBlobId(const string& key_str)
{
    CNetCacheKey key(key_str);
    return key.m_Id;
}

END_NCBI_SCOPE
