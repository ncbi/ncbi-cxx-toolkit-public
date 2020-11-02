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

#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>

#include "psg_cache_bytes_util.hpp"

BEGIN_SCOPE()
USING_IDBLOB_SCOPE;

using TPackBytes = CPubseqGatewayCachePackBytes;
using TUnpackBytes = CPubseqGatewayCacheUnpackBytes;

static const constexpr unsigned kPackedSatKeySize = 4;
static const constexpr unsigned kPackedLastModifiedSize = 8;
static const constexpr unsigned kPackedKeySize = (kPackedSatKeySize + kPackedLastModifiedSize);

END_SCOPE()

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

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
    auto rdtxn = BeginReadTxn();
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
}

bool CPubseqGatewayCacheBlobProp::x_ExtractRecord(CBlobRecord& record, lmdb::val const& value) const
{
    ::psg::retrieval::BlobPropValue info;
    if (!info.ParseFromArray(value.data(), value.size())) {
        return false;
    }
    record
        .SetClass(info.class_())
        .SetDateAsn1(info.date_asn1())
        .SetHupDate(info.hup_date())
        .SetDiv(info.div())
        .SetFlags(info.flags())
        .SetNChunks(info.n_chunks())
        .SetId2Info(info.id2_info())
        .SetOwner(info.owner())
        .SetSize(info.size())
        .SetSizeUnpacked(info.size_unpacked())
        .SetUserName(info.username());
    return true;
}

vector<CBlobRecord> CPubseqGatewayCacheBlobProp::Fetch(CBlobFetchRequest const& request)
{
    vector<CBlobRecord> response;
    if (
        !request.HasField(CBlobFetchRequest::EFields::eSat)
        || !request.HasField(CBlobFetchRequest::EFields::eSatKey)
    ) {
        return response;
    }
    auto sat = request.GetSat();
    if (!m_Env || sat < 0 || static_cast<size_t>(sat) >= m_Dbis.size() || !m_Dbis[sat]) {
        return response;
    }
    auto sat_key = request.GetSatKey();
    bool with_modified = request.HasField(CBlobFetchRequest::EFields::eLastModified);
    string filter = with_modified ? PackKey(sat_key, request.GetLastModified()) : PackKey(sat_key);
    {
        auto rdtxn = BeginReadTxn();
        lmdb::val val;
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbis[sat]);
        if (cursor.get(lmdb::val(filter), val, MDB_SET_RANGE)) {
            lmdb::val key;
            bool has_current = cursor.get(key, val, MDB_GET_CURRENT);
            while (has_current) {
                int64_t last_modified{-1};
                if (
                    key.size() != kPackedKeySize
                    || memcmp(key.data<const char>(), filter.c_str(), filter.size()) != 0
                ) {
                    break;
                }

                has_current = UnpackKey(key.data<const char>(), key.size(), last_modified);
                if (has_current && (!with_modified || last_modified == request.GetLastModified())) {
                    response.resize(response.size() + 1);
                    auto& last_record = response[response.size() - 1];
                    last_record.SetKey(sat_key);
                    last_record.SetModified(last_modified);
                    // Skip record if we cannot parse protobuf data
                    if (!x_ExtractRecord(last_record, val)) {
                        response.resize(response.size() - 1);
                    }
                }
                has_current = cursor.get(key, val, MDB_NEXT);
            }
        }
    }
    return response;
}

vector<CBlobRecord> CPubseqGatewayCacheBlobProp::FetchLast(CBlobFetchRequest const& request)
{
    vector<CBlobRecord> response;
    if (!request.HasField(CBlobFetchRequest::EFields::eSat)) {
        return response;
    }
    auto sat = request.GetSat();
    if (!m_Env || sat < 0 || static_cast<size_t>(sat) >= m_Dbis.size() || !m_Dbis[sat]) {
        return response;
    }
    {
        auto rdtxn = BeginReadTxn();
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbis[sat]);
        lmdb::val key, val;
        bool has_current = cursor.get(key, val, MDB_LAST);
        while (has_current) {
            int64_t last_modified{-1};
            CBlobRecord::TSatKey sat_key{-1};
            if (key.size() != kPackedKeySize) {
                break;
            }
            has_current = UnpackKey(key.data<const char>(), key.size(), last_modified, sat_key);
            if (has_current) {
                response.resize(response.size() + 1);
                auto& last_record = response[response.size() - 1];
                last_record.SetKey(sat_key);
                last_record.SetModified(last_modified);
                // Skip record if we cannot parse protobuf data
                if (!x_ExtractRecord(last_record, val)) {
                    response.resize(response.size() - 1);
                }
            }
            has_current = cursor.get(key, val, MDB_NEXT);
        }
    }
    return response;
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

END_IDBLOB_SCOPE
