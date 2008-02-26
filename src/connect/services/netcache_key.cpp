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

#include <memory>


BEGIN_NCBI_SCOPE


const string kNetCache_KeyPrefix = "NCID";


CNetCacheKey::CNetCacheKey(const string& key_str)
{
    // NCID_01_1_MYHOST_9000

    const char* ch = key_str.c_str();
    string prefix;

    // prefix

    for (;*ch && *ch != '_'; ++ch) {
        prefix += *ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;

    if (prefix != kNetCache_KeyPrefix) {
        NCBI_THROW(CNetCacheException, eKeyFormatError,
                                       "Key syntax error. Invalid prefix.");
    }

    // version
    version = atoi(ch);
    while (*ch && *ch != '_') {
        ++ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;

    // id
    id = atoi(ch);
    while (*ch && *ch != '_') {
        ++ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;

    // host
    for (;*ch && *ch != '_'; ++ch) {
        host += *ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;

    // port
    port = atoi(ch);
}


CNetCacheKey::operator string() const
{
    string tmp;
    string key = "NCID_01";    // NetCacheId prefix plus version

    NStr::IntToString(tmp, id);
    key += "_";
    key += tmp;

    key += "_";
    key += host;

    NStr::IntToString(tmp, port);
    key += "_";
    key += tmp;

    NStr::IntToString(tmp, (long) time(0));
    key += "_";
    key += tmp;
    return key;
}



END_NCBI_SCOPE
