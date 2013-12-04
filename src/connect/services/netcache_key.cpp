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

#include "util.hpp"

#include <connect/services/netcache_key.hpp>
#include <connect/services/netcache_api_expt.hpp>

#include <connect/ncbi_socket.hpp>

#include <util/random_gen.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbimtx.hpp>


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
        size_t key_len, CNetCacheKey* key_obj,
        CCompoundIDPool::TInstance id_pool)
{
    if (key_len > KEY_PREFIX_LENGTH &&
            memcmp(key_str, KEY_PREFIX, KEY_PREFIX_LENGTH) == 0) {
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

        if (key_obj != NULL) {
            key_obj->m_Port = (unsigned short) atoi(port_str);
            key_obj->m_CreationTime = (time_t) strtoul(creation_time_str, NULL, 10);
            key_obj->m_Random = (Uint4) strtoul(random_str, NULL, 10);
            key_obj->m_PrimaryKeyLength = ch - key_str;

            // Key extensions
            key_obj->m_ServiceName = kEmptyStr;
            key_obj->m_Flags = 0;
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
                if (key_obj != NULL)
                    switch (ext_tag) {
                    case 'S':
                        key_obj->m_ServiceName.assign(ext_value, ch - ext_value);
                        break;

                    case 'F':
                        while (ext_value < ch)
                            switch (*ext_value++) {
                            case '1':
                                if (ext_value == ch ||
                                        *ext_value < 'a' || *ext_value > 'z')
                                    key_obj->SetFlag(fNCKey_SingleServer);
                                break;
                            case 'N':
                                if (ext_value == ch ||
                                        *ext_value < 'a' || *ext_value > 'z')
                                    key_obj->SetFlag(fNCKey_NoServerCheck);
                            }
                    }
            } while (ch < ch_end);
        }
#ifndef NO_COMPOUND_ID_SUPPORT
    } else if (id_pool != NULL) {
        try {
            CCompoundIDPool pool_obj(id_pool);
            CCompoundID cid(pool_obj.FromString(string(key_str, key_len)));
            if (key_obj != NULL)
                key_obj->m_Version = 2;

            CCompoundIDField field(cid.GetFirst(eCIT_ID));
            if (!field)
                return false;
            if (key_obj != NULL)
                key_obj->m_Id = (unsigned) field.GetID();

            if (!(field = cid.GetFirst(eCIT_IPv4SockAddr))) {
                if (!(field = cid.GetFirst(eCIT_Host)))
                    return false;
                if (key_obj != NULL)
                    key_obj->m_Host = field.GetHost();
                if (!(field = cid.GetFirst(eCIT_Port)))
                    return false;
                if (key_obj != NULL)
                    key_obj->m_Port = field.GetPort();
            } else {
                if (key_obj != NULL) {
                    key_obj->m_Host = CSocketAPI::ntoa(field.GetIPv4Address());
                    key_obj->m_Port = field.GetPort();
                }
            }

            if (!(field = cid.GetFirst(eCIT_Timestamp)))
                return false;
            if (key_obj != NULL)
                key_obj->m_CreationTime = (time_t) field.GetTimestamp();

            if (!(field = cid.GetFirst(eCIT_Random)))
                return false;
            if (key_obj != NULL)
                key_obj->m_Random = field.GetRandom();

            if (key_obj != NULL) {
                GenerateBlobKey(&key_obj->m_Key,
                        key_obj->m_Id,
                        key_obj->m_Host,
                        key_obj->m_Port,
                        1,
                        key_obj->m_Random,
                        key_obj->m_CreationTime);

                key_obj->m_PrimaryKeyLength = key_obj->m_Key.length();

                field = cid.GetFirst(eCIT_ServiceName);
                key_obj->m_ServiceName = field ?
                        field.GetServiceName() : kEmptyStr;

                field = cid.GetFirst(eCIT_Flags);
                key_obj->m_Flags = field ? (unsigned) field.GetFlags() : 0;

                if (!key_obj->m_ServiceName.empty() || key_obj->m_Flags != 0)
                    AddExtensions(key_obj->m_Key,
                            key_obj->m_ServiceName, key_obj->m_Flags);
            }
        }
        catch (CCompoundIDException&) {
            return false;
        }
#endif /* NO_COMPOUND_ID_SUPPORT */
    } else {
        return false;
    }

    return true;
}

static void AppendServiceNameExtension(string& blob_id,
    const string& service_name)
{
    blob_id.append(g_NumberOfUnderscoresPlusOne(service_name), '_');
    blob_id.append("S_", 2);
    blob_id.append(service_name);
}

void CNetCacheKey::AddExtensions(string& blob_id, const string& service_name,
        CNetCacheKey::TNCKeyFlags flags)
{
    blob_id.append(KEY_EXTENSION_MARKER, KEY_EXTENSION_MARKER_LENGTH);
    AppendServiceNameExtension(blob_id, service_name);

    if (flags != 0) {
        blob_id.append("_F_", 3);
        if (flags & fNCKey_SingleServer)
            blob_id.append(1, '1');
        if (flags & fNCKey_NoServerCheck)
            blob_id.append(1, 'N');
    }
}

CNetCacheKey::CNetCacheKey(const string& key_str,
        CCompoundIDPool::TInstance id_pool)
{
    Assign(key_str, id_pool);
}

void CNetCacheKey::Assign(const string& key_str,
        CCompoundIDPool::TInstance id_pool)
{
    m_Key = key_str;

    if (!ParseBlobKey(key_str.c_str(), key_str.size(), this, id_pool)) {
        NCBI_THROW_FMT(CNetCacheException, eKeyFormatError,
                "Invalid blob key format: '" <<
                        NStr::PrintableString(key_str) << '\'');
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
                              unsigned int   rnd_num,
                              time_t         creation_time)
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

    if (creation_time == 0)
        creation_time = ::time(0);
    NStr::UInt8ToString(tmp, (Uint8) creation_time);
    key->append(1, '_');
    key->append(tmp);

    NStr::UIntToString(tmp, rnd_num);
    key->append(1, '_');
    key->append(tmp);
}

void CNetCacheKey::GenerateBlobKey(string* key, unsigned id,
    const string& host, unsigned short port, const string& service_name,
    CNetCacheKey::TNCKeyFlags flags)
{
    GenerateBlobKey(key, id, host, port);
    AddExtensions(*key, service_name, flags);
}

#ifndef NO_COMPOUND_ID_SUPPORT
string CNetCacheKey::KeyToCompoundID(const string& key_str,
        CCompoundIDPool id_pool)
{
    CNetCacheKey key_obj(key_str, id_pool);

    CCompoundID nc_key_cid(id_pool.NewID(eCIC_NetCacheKey));

    nc_key_cid.AppendID(key_obj.GetId());

    string host(key_obj.GetHost());
    if (CSocketAPI::isip(host, true))
        nc_key_cid.AppendIPv4SockAddr(
                CSocketAPI::gethostbyname(host), key_obj.GetPort());
    else {
        nc_key_cid.AppendHost(host);
        nc_key_cid.AppendPort(key_obj.GetPort());
    }

    nc_key_cid.AppendTimestamp(key_obj.GetCreationTime());

    nc_key_cid.AppendRandom(key_obj.GetRandomPart());

    if (!key_obj.GetServiceName().empty())
        nc_key_cid.AppendServiceName(key_obj.GetServiceName());

    if (key_obj.GetFlags() != 0)
        nc_key_cid.AppendFlags(key_obj.GetFlags());

    return nc_key_cid.ToString();
}
#endif /* NO_COMPOUND_ID_SUPPORT */

unsigned int
CNetCacheKey::GetBlobId(const string& key_str)
{
    CNetCacheKey key(key_str);
    return key.m_Id;
}

END_NCBI_SCOPE
