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

#include "psg_cache_blob_prop.hpp"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <util/lmdbxx/lmdb++.h>

#include "psg_cache_bytes_util.hpp"

BEGIN_SCOPE()
USING_PSG_SCOPE;

using TPackBytes = CPubseqGatewayCachePackBytes;
using TUnpackBytes = CPubseqGatewayCacheUnpackBytes;

static const constexpr unsigned kPackedSatKeySize = 4;
static const constexpr unsigned kPackedLastModifiedSize = 8;
static const constexpr unsigned kPackedKeySize = (kPackedSatKeySize + kPackedLastModifiedSize);

END_SCOPE()

BEGIN_PSG_SCOPE

CPubseqGatewayCacheBlobProp::CPubseqGatewayCacheBlobProp(const string& file_name)
    : CPubseqGatewayCacheBase(file_name)
{
}

CPubseqGatewayCacheBlobProp::~CPubseqGatewayCacheBlobProp() = default;

void CPubseqGatewayCacheBlobProp::Open(const set<int>& sat_ids)
{
    if (sat_ids.empty()) {
        lmdb::runtime_error::raise("List of satellites is empty", 0);
    }

    CPubseqGatewayCacheBase::Open();
    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    for (const auto & sat_id : sat_ids) {
        unique_ptr<lmdb::dbi, function<void(lmdb::dbi*)>> pdbi;
        string sat_dbi = string("#DATA[") + to_string(sat_id) + "]";
        try {
            pdbi = unique_ptr<lmdb::dbi, function<void(lmdb::dbi*)>>(
                new lmdb::dbi({lmdb::dbi::open(rdtxn, sat_dbi.c_str(), 0)}),
                [this](lmdb::dbi* dbi){
                    if (dbi && *dbi) {
                        dbi->close(*m_Env);
                    }
                    delete(dbi);
                }
            );
        }
        catch (const lmdb::error& e) {
            ERR_POST(Warning << "BlobProp cache: failed to open " << sat_dbi << " dbi: " << e.what()
                << ", cache for sat = " << sat_id << " will not be used.");
            pdbi = nullptr;
        }
        if (static_cast<size_t>(sat_id) > m_Dbis.size()) {
            m_Dbis.resize(sat_id);
        }
        m_Dbis.push_back(move(pdbi));
    }
    rdtxn.commit();
}

bool CPubseqGatewayCacheBlobProp::LookupBySatKey(int32_t sat, int32_t sat_key, int64_t& last_modified, string& data)
{
    if (!m_Env || sat < 0 || (size_t)sat >= m_Dbis.size() || !m_Dbis[sat]) {
        return false;
    }
    bool rv = false;
    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    {
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbis[sat]);
        string skey = PackKey(sat_key);
        rv = cursor.get(lmdb::val(skey), MDB_SET_RANGE);
        if (rv) {
            lmdb::val key, val;
            rv = cursor.get(key, val, MDB_GET_CURRENT);
            rv = rv && key.size() == kPackedKeySize && memcmp(key.data<const char>(), skey.c_str(), skey.size()) == 0;
            if (rv) {
                rv = UnpackKey(key.data<const char>(), key.size(), last_modified);
            }
            if (rv) {
                data.assign(val.data(), val.size());
            }
        }
    }
    rdtxn.commit();
    return rv;
}

bool CPubseqGatewayCacheBlobProp::LookupBySatKeyLastModified(
    int32_t sat, int32_t sat_key, int64_t last_modified, string& data)
{
    if (!m_Env || sat < 0 || (size_t)sat >= m_Dbis.size() || !m_Dbis[sat]) {
        return false;
    }
    bool rv = false;
    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    {
        string skey = PackKey(sat_key, last_modified);
        lmdb::val val;
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbis[sat]);
        rv = cursor.get(lmdb::val(skey), val, MDB_SET);
        if (rv) {
            data.assign(val.data(), val.size());
        }
    }

    rdtxn.commit();
    return rv;
}

string CPubseqGatewayCacheBlobProp::PackKey(int32_t sat_key)
{
    string rv;
    rv.reserve(kPackedSatKeySize);
    TPackBytes().Pack<kPackedSatKeySize>(rv, sat_key);
    return rv;
}

string CPubseqGatewayCacheBlobProp::PackKey(int32_t sat_key, int64_t last_modified)
{
    string rv;
    rv.reserve(kPackedKeySize);
    TPackBytes().Pack<kPackedSatKeySize>(rv, sat_key);
    int64_t lm = -last_modified;
    TPackBytes().Pack<kPackedLastModifiedSize>(rv, lm);
    return rv;
}

bool CPubseqGatewayCacheBlobProp::UnpackKey(const char* key, size_t key_sz, int64_t& last_modified)
{
    bool rv = key_sz == kPackedKeySize;
    if (rv) {
        last_modified = TUnpackBytes().Unpack<kPackedLastModifiedSize, int64_t>(key + 4);
        last_modified = -last_modified;
    }
    return rv;
}

bool CPubseqGatewayCacheBlobProp::UnpackKey(const char* key, size_t key_sz, int64_t& last_modified, int32_t& sat_key) {
    bool rv = key_sz == kPackedKeySize;
    if (rv) {
        sat_key = TUnpackBytes().Unpack<kPackedSatKeySize, int32_t>(key);
        last_modified = TUnpackBytes().Unpack<kPackedLastModifiedSize, int64_t>(key + 4);
        last_modified = -last_modified;
    }
    return rv;
}

END_PSG_SCOPE
