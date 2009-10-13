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


static const char* kNetCache_KeyPrefix = "NCID";
static CRandom s_NCKeyRandom((CRandom::TValue(time(NULL))));



inline bool
CNetCacheKey::x_ParseBlobKey(const string& key_str, CNetCacheKey* key_obj)
{
    // NCID_01_1_MYHOST_9000

    const char* ch = key_str.c_str();

    // prefix
    const char* const prefix = ch;
    while (*ch  &&  *ch != '_') {
        ++ch;
    }
    if (*ch != '_'
        ||  NStr::strncmp(prefix, kNetCache_KeyPrefix, ch - prefix) != 0)
    {
        return false;
    }
    ++ch;

    // version
    if (key_obj)
        key_obj->m_Version = atoi(ch);
    while (*ch  &&  isdigit(*ch)) {
        ++ch;
    }
    if (*ch != '_') {
        return false;
    }
    ++ch;

    // id
    if (key_obj)
        key_obj->m_Id = atoi(ch);
    while (*ch  &&  isdigit(*ch)) {
        ++ch;
    }
    if (*ch != '_') {
        return false;
    }
    ++ch;

    // hostname
    const char* const hostname = ch;
    while (*ch  &&  *ch != '_') {
        ++ch;
    }
    if (*ch != '_') {
        return false;
    }
    if (key_obj)
        key_obj->m_Host.assign(hostname, ch - hostname);
    ++ch;

    // port
    if (key_obj)
        key_obj->m_Port = atoi(ch);
    while (*ch && isdigit(*ch)) {
        ++ch;
    }
    if (*ch  &&  *ch != '_') {
        return false;
    }

    return true;
}

CNetCacheKey::CNetCacheKey(const string& key_str)
{
    if (!x_ParseBlobKey(key_str, this)) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, "Key syntax error.");
    }
}

inline
CNetCacheKey::CNetCacheKey(void)
{
    // Do not initialize anything - it's private
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
    string tmp;
    *key = kNetCache_KeyPrefix;

    NStr::IntToString(tmp, ver);
    *key += "_";
    *key += tmp;

    NStr::IntToString(tmp, id);
    *key += "_";
    *key += tmp;

    *key += "_";
    *key += host;

    NStr::IntToString(tmp, port);
    *key += "_";
    *key += tmp;

    if (ver == 1) {
        NStr::IntToString(tmp, (long) ::time(0));
    }
    else if (ver == 2) {
        NStr::UIntToString(tmp, s_NCKeyRandom.GetRand());
    }
    else {
        NCBI_THROW(CNetCacheException, eKeyFormatError,
                   "Unsupported version of NetCache key -"
                   + NStr::IntToString(ver));
    }
    *key += "_";
    *key += tmp;
}

unsigned int
CNetCacheKey::GetBlobId(const string& key_str)
{
    CNetCacheKey key(key_str);
    return key.m_Id;
}

bool
CNetCacheKey::IsValidKey(const string& key_str)
{
    return x_ParseBlobKey(key_str, NULL);
}

END_NCBI_SCOPE
