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

#include <algorithm>

#include <connect/services/netcache_key.hpp>
#include <connect/services/netcache_api_expt.hpp>

#include <connect/ncbi_socket.hpp>

#include <util/random_gen.hpp>
#include <util/checksum.hpp>

#include <corelib/ncbistr.hpp>
#include <corelib/ncbimtx.hpp>


BEGIN_NCBI_SCOPE


#define KEY_PREFIX "NCID_"
#define KEY_PREFIX_LENGTH (sizeof(KEY_PREFIX) - 1)
static const CTempString s_KeyPrefix(KEY_PREFIX, KEY_PREFIX_LENGTH);

#define KEY_EXTENSION_MARKER "_0MetA0"
#define KEY_EXTENSION_MARKER_LENGTH (sizeof(KEY_EXTENSION_MARKER) - 1)

#define PARSE_KEY_PART(part_name, test_function) \
    const char* const part_name = ch; \
    while (ch != ch_end && test_function(*ch)) \
        ++ch; \
    if (ch == ch_end  ||  *ch != '_') \
        return false; \
    ++ch;

#define PARSE_NUMERIC_KEY_PART(part_name) PARSE_KEY_PART(part_name, isdigit)

bool CNetCacheKey::ParseBlobKey(const char* key_str,
        size_t key_len, CNetCacheKey* key_obj,
        CCompoundIDPool::TInstance id_pool)
{
    CTempString key(key_str, key_len);

    if (key_obj != NULL)
        key_obj->m_Key = key;

    CTempString service_name, key_v3;

    if (NStr::SplitInTwo(key, CTempString("/", 1), service_name, key_v3)) {
        if (key_obj != NULL) {
            key_obj->m_ServiceName = service_name;
            key_obj->m_Flags = 0;
        }
        key = key_v3;
    } else if (key_obj != NULL) {
        key_obj->m_ServiceName = kEmptyStr;
        key_obj->m_Flags = 0;
    }

    if (NStr::StartsWith(key, s_KeyPrefix)) {
        const char* ch = key.data() + KEY_PREFIX_LENGTH;
        const char* ch_end = key.data() + key.length();

        // version
        PARSE_NUMERIC_KEY_PART(ver_str);

        unsigned key_version = (unsigned) atoi(ver_str);

        // id
        PARSE_NUMERIC_KEY_PART(id_str);

        if (key_obj) {
            key_obj->m_Version = key_version;
            key_obj->m_Id = (unsigned) atoi(id_str);
        }

        if (key_version == 1) {
            // hostname
            const char* const hostname = ch;
            while (ch != ch_end  &&  *ch != '_')
                ++ch;
            if (ch == ch_end  ||  *ch != '_')
                return false;
            if (key_obj != NULL)
                key_obj->m_Host.assign(hostname, ch - hostname);
            ++ch;

            // port
            PARSE_NUMERIC_KEY_PART(port_str);
            if (key_obj != NULL) {
                key_obj->m_Port = (unsigned short) atoi(port_str);
                key_obj->m_HostPortCRC32 = 0;
            }
        } else if (key_version == 3) {
            // CRC32 of the hostname:port combination.
            PARSE_KEY_PART(crc32, isalnum);
            if (key_obj != NULL) {
                key_obj->m_Host = kEmptyStr;
                key_obj->m_Port = 0;
                key_obj->m_HostPortCRC32 = (unsigned int) strtoul(crc32, NULL, 16);
            }
        } else
            return false;

        // Creation time
        PARSE_NUMERIC_KEY_PART(creation_time_str);

        // The last part of the primary key -- the random number.
        const char* const random_str = ch;
        while (ch != ch_end  &&  isdigit(*ch))
            ++ch;

        if (key_obj != NULL) {
            key_obj->m_CreationTime = (time_t) strtoul(creation_time_str, NULL, 10);
            key_obj->m_Random = (Uint4) strtoul(random_str, NULL, 10);
            key_obj->m_PrimaryKey = string(key.data(), ch);
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
        return true;
    }

#ifndef NO_COMPOUND_ID_SUPPORT
    if (id_pool != NULL) {
        try {
            CCompoundIDPool pool_obj(id_pool);
            CCompoundID cid(pool_obj.FromString(key));
            if (cid.GetClass() != eCIC_NetCacheBlobKey)
                return false;
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

                key_obj->m_PrimaryKey = key_obj->m_Key;

                field = cid.GetFirst(eCIT_ServiceName);
                if (field)
                    key_obj->m_ServiceName = field.GetServiceName();

                field = cid.GetFirst(eCIT_Flags);
                if (field)
                    key_obj->m_Flags = (unsigned) field.GetFlags();

                if (!key_obj->m_ServiceName.empty() || key_obj->m_Flags != 0)
                    AddExtensions(key_obj->m_Key,
                            key_obj->m_ServiceName, key_obj->m_Flags);
            }
        }
        catch (CCompoundIDException&) {
            return false;
        }
        return true;
    }
#endif /* NO_COMPOUND_ID_SUPPORT */

    return false;
}

static void AppendServiceNameExtension(string& blob_id,
    const string& service_name)
{
    auto c = 1 + count(service_name.begin(), service_name.end(), '_');
    blob_id.append(c, '_');
    blob_id.append("S_", 2);
    blob_id.append(service_name);
}

void CNetCacheKey::AddExtensions(string& blob_id, const string& service_name,
        CNetCacheKey::TNCKeyFlags flags, unsigned ver)
{
    if (ver != 3) {
        blob_id.append(KEY_EXTENSION_MARKER, KEY_EXTENSION_MARKER_LENGTH);
        AppendServiceNameExtension(blob_id, service_name);
        if (flags == 0)
            return;
        blob_id.append("_F_", 3);
    } else {
        blob_id.insert(0, 1, '/');
        blob_id.insert(0, service_name);
        if (flags == 0)
            return;
        blob_id.append(KEY_EXTENSION_MARKER "_F_",
                KEY_EXTENSION_MARKER_LENGTH + 3);
    }
    if (flags & fNCKey_SingleServer)
        blob_id.append(1, '1');
    if (flags & fNCKey_NoServerCheck)
        blob_id.append(1, 'N');
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

Uint4 CNetCacheKey::CalculateChecksum(const string& host, unsigned short port)
{
    string server_address(host);
    server_address.append(1, ':');
    server_address.append(NStr::NumericToString(port));
    CChecksum crc32(CChecksum::eCRC32);
    crc32.AddChars(server_address.data(), server_address.size());
    return crc32.GetChecksum();
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

    key->append(1, '_');
    NStr::IntToString(tmp, id);
    key->append(tmp);

    key->append(1, '_');

    if (ver == 3)
        key->append(NStr::NumericToString(
                CNetCacheKey::CalculateChecksum(host, port), 0, 16));
    else {
        key->append(host);
        key->append(1, '_');
        NStr::IntToString(tmp, port);
        key->append(tmp);
    }

    key->append(1, '_');
    NStr::UInt8ToString(tmp,
            (Uint8) (creation_time ? creation_time : ::time(NULL)));
    key->append(tmp);

    key->append(1, '_');
    NStr::UIntToString(tmp, rnd_num);
    key->append(tmp);
}

#ifndef NO_COMPOUND_ID_SUPPORT
string CNetCacheKey::KeyToCompoundID(const string& key_str,
        CCompoundIDPool id_pool)
{
    CNetCacheKey key_obj(key_str, id_pool);

    CCompoundID nc_key_cid(id_pool.NewID(eCIC_NetCacheBlobKey));

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
