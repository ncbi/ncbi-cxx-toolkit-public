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


static CSpinLock s_RandomLock;
static CRandom s_NCKeyRandom((CRandom::TValue(time(NULL))));

#define KEY_PREFIX "NCID_"
#define KEY_PREFIX_LENGTH (sizeof(KEY_PREFIX) - 1)

#define KEY_EXTENSION_MARKER "_0MetA0"
#define KEY_EXTENSION_MARKER_LENGTH (sizeof(KEY_EXTENSION_MARKER) - 1)

#define PARSE_NUMERIC_KEY_PART(part_name) \
    const char* const part_name = ch; \
    while (ch != ch_end && isdigit(*ch)) \
        ++ch; \
    if (ch == ch_end  ||  *ch != '_') \
        return false; \
    ++ch;

bool CNetCacheKey::ParseBlobKey(const char* key_str,
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
    if (key_obj) {
        key_obj->m_ServiceName = kEmptyStr;
        key_obj->m_ServiceNameExtLen = key_obj->m_ServiceNameExtPos = 0;
    }

    if (ch < ch_end) {
        if (ch + KEY_EXTENSION_MARKER_LENGTH > ch_end || memcmp(ch,
                KEY_EXTENSION_MARKER, KEY_EXTENSION_MARKER_LENGTH) != 0)
            return false;
        if ((ch += KEY_EXTENSION_MARKER_LENGTH) == ch_end)
            return true;
        if (*ch != '_')
            return false;
        do {
            int underscores_to_skip = 0;
            const char* extension = ch;
            for (;;) {
                if (++ch == ch_end)
                    return false;
                if (*ch != '_')
                    break;
                ++underscores_to_skip;
            }
            char ext_tag = *ch;
            if (++ch == ch_end || *ch != '_')
                return false;
            const char* ext_value = ++ch;
            while (ch < ch_end && (*ch != '_' || --underscores_to_skip >= 0))
                ++ch;
            if (key_obj != NULL && ext_tag == 'S') {
                key_obj->m_ServiceName.assign(ext_value, ch - ext_value);
                key_obj->m_ServiceNameExtPos = extension - key_str;
                key_obj->m_ServiceNameExtLen = ch - extension;
            }
        } while (ch < ch_end);
    }

    return true;
}

string CNetCacheKey::StripKeyExtensions() const
{
    return HasExtensions() ? string(m_Key.data(), m_PrimaryKeyLength) : m_Key;
}

static unsigned CountUnderscores(const string& extension_value)
{
    unsigned underscore_count = 1;
    const char* underscore = strchr(extension_value.c_str(), '_');
    while (underscore != NULL) {
        ++underscore_count;
        underscore = strchr(underscore + 1, '_');
    }
    return underscore_count;
}

static void AppendServiceNameExtension(string& blob_id,
    const string& service_name)
{
    blob_id.append(CountUnderscores(service_name), '_');
    blob_id.append("S_");
    blob_id.append(service_name);
}

void CNetCacheKey::AddExtensions(string& blob_id, const string& service_name)
{
    blob_id.append(KEY_EXTENSION_MARKER, KEY_EXTENSION_MARKER_LENGTH);
    AppendServiceNameExtension(blob_id, service_name);
}

void CNetCacheKey::SetServiceName(const string& service_name)
{
    if (HasExtensions()) {
        string service_name_ext;
        AppendServiceNameExtension(service_name_ext, service_name);
        m_Key.replace(m_ServiceNameExtPos, m_ServiceNameExtLen,
            service_name_ext);
        m_ServiceNameExtLen = service_name_ext.length();
    } else {
        AddExtensions(m_Key, service_name);
        m_ServiceNameExtPos = m_PrimaryKeyLength + KEY_EXTENSION_MARKER_LENGTH;
        m_ServiceNameExtLen = m_Key.length() - m_ServiceNameExtPos;
    }
    m_ServiceName = service_name;
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

void
CNetCacheKey::GenerateBlobKey(string*        key,
                              unsigned int   id,
                              const string&  host,
                              unsigned short port,
                              unsigned int   ver /* = 1 */)
{
    s_RandomLock.Lock();
    unsigned int rnd_num = s_NCKeyRandom.GetRand();
    s_RandomLock.Unlock();
    GenerateBlobKey(key, id, host, port, ver, rnd_num);
}

void
CNetCacheKey::GenerateBlobKey(string*        key,
                              unsigned int   id,
                              const string&  host,
                              unsigned short port,
                              unsigned int   ver,
                              unsigned int   rnd_num)
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

    NStr::ULongToString(tmp, (unsigned long) ::time(0));
    key->append(1, '_');
    key->append(tmp);

    NStr::UIntToString(tmp, rnd_num);
    key->append(1, '_');
    key->append(tmp);
}

void CNetCacheKey::GenerateBlobKey(string* key, unsigned id,
    const string& host, unsigned short port, const string& service_name)
{
    GenerateBlobKey(key, id, host, port);
    AddExtensions(*key, service_name);
}

unsigned int
CNetCacheKey::GetBlobId(const string& key_str)
{
    CNetCacheKey key(key_str);
    return key.m_Id;
}

END_NCBI_SCOPE
