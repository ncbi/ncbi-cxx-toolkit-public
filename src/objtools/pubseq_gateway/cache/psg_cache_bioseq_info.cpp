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

#include "psg_cache_bioseq_info.hpp"

USING_NCBI_SCOPE;

static const constexpr unsigned kPackedZeroSz = 1;
static const constexpr unsigned kPackedVersionSz = 3;
static const constexpr unsigned kPackedSeqIdTypeSz = 2;

static size_t PackedKeySize(size_t acc_sz) {
    return acc_sz + (kPackedZeroSz + kPackedVersionSz + kPackedSeqIdTypeSz);
}

CPubseqGatewayCacheBioseqInfo::CPubseqGatewayCacheBioseqInfo(const string& file_name) :
    CPubseqGatewayCacheBase(file_name)
{
    m_Dbi.reset(new lmdb::dbi({0}));
}

CPubseqGatewayCacheBioseqInfo::~CPubseqGatewayCacheBioseqInfo()
{
}

void CPubseqGatewayCacheBioseqInfo::Open()
{
    CPubseqGatewayCacheBase::Open();
    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    *m_Dbi = lmdb::dbi::open(rdtxn, "#DATA", 0);
    rdtxn.commit();
}

bool CPubseqGatewayCacheBioseqInfo::LookupByAccession(const string& accession, int& version, int& seq_id_type, string& data) {
    bool rv = false;

    if (!m_Env)
        return false;

    version = -1;
    seq_id_type = 0;
    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    {
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
        rv = cursor.get(lmdb::val(accession), MDB_SET_RANGE);
        if (rv) {
            lmdb::val key, val;
            rv = cursor.get(key, val, MDB_GET_CURRENT);
            rv = rv && key.size() == PackedKeySize(accession.size()) && accession.compare(key.data<const char>()) == 0;
            if (rv)
                rv = UnpackKey(key.data<const char>(), key.size(), version, seq_id_type);
            if (rv)
                data.assign(val.data(), val.size());
        }
    }

    rdtxn.commit();
    if (!rv)
        data.clear();
    return rv;
}
    
bool CPubseqGatewayCacheBioseqInfo::LookupByAccessionVersion(const string& accession, int version, int& seq_id_type, string& data) {
    bool rv = false;

    if (!m_Env)
        return false;

    seq_id_type = 0;
    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    {
    
        string skey = PackKey(accession, version);

        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
        rv = cursor.get(lmdb::val(skey), MDB_SET_RANGE);
        if (rv) {
            lmdb::val key, val;
            rv = cursor.get(key, val, MDB_GET_CURRENT);
            rv = rv && key.size() == PackedKeySize(accession.size()) && accession.compare(key.data<const char>()) == 0;
            if (rv) {
                int ver;
                rv = UnpackKey(key.data<const char>(), key.size(), ver, seq_id_type) && (ver == version);
                if (rv)
                    data.assign(val.data(), val.size());
            }
        }
    }
    
    rdtxn.commit();
    if (!rv)
        data.clear();
    return rv;
}

    
bool CPubseqGatewayCacheBioseqInfo::LookupByAccessionVersionSeqIdType(const string& accession, int version, int seq_id_type, string& data) {
    bool rv = false;

    if (!m_Env)
        return false;

    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    {
        lmdb::val val;
        if (version <= 0) {
            auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
            rv = cursor.get(lmdb::val(accession), MDB_SET_RANGE);
            if (rv) {
                lmdb::val key;
                rv = cursor.get(key, val, MDB_GET_CURRENT);
                while (rv) {
                    int found_seq_id_type = -1;
                    rv = key.size() == PackedKeySize(accession.size()) && accession.compare(key.data<const char>()) == 0;
                    if (!rv)
                        break;
                    if (rv)
                        rv = UnpackKey(key.data<const char>(), key.size(), version, found_seq_id_type);
                    rv = rv && (seq_id_type <= 0 || seq_id_type == found_seq_id_type);
                    if (rv)
                        break;
                    rv = cursor.get(key, val, MDB_NEXT);
                }
            }
        }
        else {
            string skey = PackKey(accession, version, seq_id_type);
            auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
            rv = cursor.get(lmdb::val(skey), val, MDB_SET);
        }
        if (rv)
            data.assign(val.data(), val.size());
    }
    
    rdtxn.commit();
    if (!rv)
        data.clear();
    return rv;
}

string CPubseqGatewayCacheBioseqInfo::PackKey(const string& accession, int version) {
    string rv;
    rv.reserve(accession.size() + 4);
    rv = accession;
    rv.append(1, 0);
    int32_t ver = -version;
    rv.append(1, (ver >> 16) & 0xFF);
    rv.append(1, (ver >>  8) & 0xFF);
    rv.append(1,  ver        & 0xFF);
    return rv;
}

string CPubseqGatewayCacheBioseqInfo::PackKey(const string& accession, int version, int seq_id_type) {
    string rv;
    rv.reserve(accession.size() + kPackedZeroSz + kPackedVersionSz + kPackedSeqIdTypeSz);
    rv = accession;
    rv.append(1, 0);
    int32_t ver = -version;
    rv.append(1, (ver >> 16) & 0xFF);
    rv.append(1, (ver >>  8) & 0xFF);
    rv.append(1,  ver        & 0xFF);
    rv.append(1, (seq_id_type >> 8) & 0xFF);
    rv.append(1,  seq_id_type       & 0xFF);
    return rv;
}

bool CPubseqGatewayCacheBioseqInfo::UnpackKey(const char* key, size_t key_sz, int& version, int& seq_id_type) {
    bool rv = key_sz > (kPackedZeroSz + kPackedVersionSz + kPackedSeqIdTypeSz);
    if (rv) {
        size_t ofs = key_sz - (kPackedZeroSz + kPackedVersionSz + kPackedSeqIdTypeSz);
        rv = key[ofs] == 0;
        if (rv) {
            ++ofs;
            int32_t ver = 0xFF000000 |
                          (uint8_t(key[ofs]) << 16) |
                          (uint8_t(key[ofs + 1]) << 8) |
                           uint8_t(key[ofs + 2]);
            version = -ver;
            seq_id_type = (uint8_t(key[ofs + 3]) << 8) |
                           uint8_t(key[ofs + 4]);
        }
    }
    return rv;
}

bool CPubseqGatewayCacheBioseqInfo::UnpackKey(const char* key, size_t key_sz, string& accession, int& version, int& seq_id_type) {
    bool rv = key_sz > (kPackedZeroSz + kPackedVersionSz + kPackedSeqIdTypeSz);
    if (rv) {
        size_t ofs = key_sz - (kPackedZeroSz + kPackedVersionSz + kPackedSeqIdTypeSz);
        accession.assign(key, ofs);
        rv = UnpackKey(key, key_sz, version, seq_id_type);
    }
    return rv;
}

USING_NCBI_SCOPE;

