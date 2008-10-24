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
#include <corelib/ncbistr.hpp>

#include <connect/services/netcache_key.hpp>
#include <connect/services/netcache_api_expt.hpp>


BEGIN_NCBI_SCOPE


const string kNetCache_KeyPrefix = "NCID";


CNetCacheKey::CNetCacheKey(const string& key_str)
{
    if (!ParseBlobKey(key_str)) {
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

bool
CNetCacheKey::ParseBlobKey(const string& key_str)
{
    // NCID_01_1_MYHOST_9000

    const char* ch = key_str.c_str();
    m_Host = kEmptyStr;

    // prefix
    string prefix;
    for (;*ch && *ch != '_'; ++ch) {
        prefix += *ch;
    }
    if (*ch != '_') {
        return false;
    }
    ++ch;

    if (prefix != kNetCache_KeyPrefix) {
        return false;
    }

    // version
    m_Version = atoi(ch);
    while (*ch && isdigit(*ch)) {
        ++ch;
    }
    if (*ch != '_') {
        return false;
    }
    ++ch;

    // id
    m_Id = atoi(ch);
    while (*ch && isdigit(*ch)) {
        ++ch;
    }
    if (*ch != '_') {
        return false;
    }
    ++ch;

    // hostname
    for (;*ch && *ch != '_'; ++ch) {
        m_Host += *ch;
    }
    if (*ch != '_') {
        return false;
    }
    ++ch;

    // port
    m_Port = atoi(ch);
    while (*ch && isdigit(*ch)) {
        ++ch;
    }
    if (*ch && *ch != '_') {
        return false;
    }

    return true;
}


void
CNetCacheKey::GenerateBlobKey(string*        key,
                              unsigned int   id,
                              const string&  host,
                              unsigned short port)
{
    string tmp;
    *key = kNetCache_KeyPrefix;
    *key += "_01";    // NetCacheId version

    NStr::IntToString(tmp, id);
    *key += "_";
    *key += tmp;

    *key += "_";
    *key += host;

    NStr::IntToString(tmp, port);
    *key += "_";
    *key += tmp;

    NStr::IntToString(tmp, (long) time(0));
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
    CNetCacheKey key;
    return key.ParseBlobKey(key_str);
}



END_NCBI_SCOPE
