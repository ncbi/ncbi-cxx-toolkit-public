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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */
#include <ncbi_pch.hpp>
#include <util/lmdbxx/lmdb++.h>

#include "psg_cache_blob_prop.hpp"

USING_NCBI_SCOPE;

static const constexpr unsigned kPackedSatKeySize = 4;
static const constexpr unsigned kPackedLastModifiedSize = 8;
static const constexpr unsigned kPackedKeySize = (kPackedSatKeySize + kPackedLastModifiedSize);

CPubseqGatewayCacheBlobProp::CPubseqGatewayCacheBlobProp(const string& file_name) :
    CPubseqGatewayCache(file_name)
{
}

CPubseqGatewayCacheBlobProp::~CPubseqGatewayCacheBlobProp()
{
}

void CPubseqGatewayCacheBlobProp::Open(const vector<string>& sat_names) {
    if (sat_names.empty())
        lmdb::runtime_error::raise("List of satellites is empty", 0);

    CPubseqGatewayCache::Open();
    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    int sat = 0;
    for (const auto & it : sat_names) {
        lmdb::dbi dbi = 0;
        if (!it.empty()) {
            string sat_dbi = string("#SAT") + to_string(sat);
            try {
                dbi = lmdb::dbi::open(rdtxn, sat_dbi.c_str(), 0);
            }
            catch (const lmdb::error& e) {
                ERR_POST(Warning << "BlobProp cache: failed to open " << sat_dbi << " dbi: " << e.what()
                    << ", cache for sat = " << sat 
                    << " will not be used.");
                dbi = 0;
            }
        }
        m_Dbis.emplace_back(dbi ? new lmdb::dbi(move(dbi)) : nullptr);
        ++sat;
    }
    rdtxn.commit();
}

bool CPubseqGatewayCacheBlobProp::LookupBySatKey(int32_t sat, int32_t sat_key, int64_t& last_modified, string& data) {
    bool rv = false;

    if (!m_Env || sat < 0 || (size_t)sat >= m_Dbis.size() || !m_Dbis[sat])
        return false;

    last_modified = 0;
    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    {
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbis[sat]);
        string skey = PackKey(sat_key);
        rv = cursor.get(lmdb::val(skey), MDB_SET_RANGE);
        if (rv) {
            lmdb::val key, val;
            rv = cursor.get(key, val, MDB_GET_CURRENT);
            rv = rv && key.size() == kPackedKeySize && memcmp(key.data<const char>(), skey.c_str(), skey.size()) == 0;
            if (rv)
                rv = UnpackKey(key.data<const char>(), key.size(), last_modified);
            if (rv)
                data.assign(val.data(), val.size());
        }
    }

    rdtxn.commit();
    if (!rv)
        data.clear();
    return rv;
}

bool CPubseqGatewayCacheBlobProp::LookupBySatKeyLastModified(int32_t sat, int32_t sat_key, int64_t last_modified, string& data) {
    bool rv = false;

    if (!m_Env || sat < 0 || (size_t)sat >= m_Dbis.size() || !m_Dbis[sat])
        return false;

    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    {
        string skey = PackKey(sat_key, last_modified);
        lmdb::val val;
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbis[sat]);
        rv = cursor.get(lmdb::val(skey), val, MDB_SET);
        if (rv)
            data.assign(val.data(), val.size());
    }
    
    rdtxn.commit();
    if (!rv)
        data.clear();
    return rv;
}

string CPubseqGatewayCacheBlobProp::PackKey(int32_t sat_key) {
    string rv;
    rv.reserve(kPackedSatKeySize);
    rv.append(1, (sat_key >> 24) & 0xFF);
    rv.append(1, (sat_key >> 16) & 0xFF);
    rv.append(1, (sat_key >>  8) & 0xFF);
    rv.append(1,  sat_key        & 0xFF);
    return rv;
}

string CPubseqGatewayCacheBlobProp::PackKey(int32_t sat_key, int64_t last_modified) {
    string rv;
    rv.reserve(kPackedKeySize);
    rv.append(1, (sat_key >> 24) & 0xFF);
    rv.append(1, (sat_key >> 16) & 0xFF);
    rv.append(1, (sat_key >>  8) & 0xFF);
    rv.append(1,  sat_key        & 0xFF);
    int64_t lm = -last_modified;
    rv.append(1, (lm >> 56) & 0xFF);
    rv.append(1, (lm >> 48) & 0xFF);
    rv.append(1, (lm >> 40) & 0xFF);
    rv.append(1, (lm >> 32) & 0xFF);
    rv.append(1, (lm >> 24) & 0xFF);
    rv.append(1, (lm >> 16) & 0xFF);
    rv.append(1, (lm >>  8) & 0xFF);
    rv.append(1,  lm        & 0xFF);
    return rv;
}

bool CPubseqGatewayCacheBlobProp::UnpackKey(const char* key, size_t key_sz, int64_t& last_modified) {
    bool rv = key_sz == kPackedKeySize;
    if (rv) {
        last_modified = (int64_t(uint8_t(key[4])) << 56) |
                        (int64_t(uint8_t(key[5])) << 48) |
                        (int64_t(uint8_t(key[6])) << 40) |
                        (int64_t(uint8_t(key[7])) << 32) |
                        (int64_t(uint8_t(key[8])) << 24) |
                        (uint8_t(key[9]) << 16) |
                        (uint8_t(key[10]) << 8) |
                         uint8_t(key[11]);
        last_modified = -last_modified;
    }
    return rv;
}

USING_NCBI_SCOPE;

