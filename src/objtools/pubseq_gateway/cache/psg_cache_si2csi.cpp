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

#include "psg_cache_si2csi.hpp"

USING_NCBI_SCOPE;

static const constexpr unsigned kPackedIdTypeSz = 2;
static const constexpr unsigned kPackedKeyZero = 1;

static size_t PackedKeySize(size_t acc_sz) {
    return acc_sz + kPackedKeyZero + kPackedIdTypeSz;
}

CPubseqGatewayCacheSi2Csi::CPubseqGatewayCacheSi2Csi(const string& file_name) :
    CPubseqGatewayCacheBase(file_name)
{
    m_Dbi.reset(new lmdb::dbi({0}));
}

CPubseqGatewayCacheSi2Csi::~CPubseqGatewayCacheSi2Csi()
{
}

void CPubseqGatewayCacheSi2Csi::Open() {
    CPubseqGatewayCacheBase::Open();
    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    *m_Dbi = lmdb::dbi::open(rdtxn, "#DATA", 0);
    rdtxn.commit();
}

bool CPubseqGatewayCacheSi2Csi::LookupBySeqId(const string& sec_seqid, int& sec_seq_id_type, string& data) {
    bool rv = false;

    if (!m_Env)
        return false;

    sec_seq_id_type = 0;
    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    {
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
        rv = cursor.get(lmdb::val(sec_seqid), MDB_SET_RANGE);
        if (rv) {
            lmdb::val key, val;
            rv = cursor.get(key, val, MDB_GET_CURRENT);
            rv = rv && key.size() == PackedKeySize(sec_seqid.size()) && sec_seqid.compare(key.data<const char>()) == 0;
            if (rv)
                rv = UnpackKey(key.data<const char>(), key.size(), sec_seq_id_type);
            if (rv)
                data.assign(val.data(), val.size());
        }
    }

    rdtxn.commit();
    if (!rv)
        data.clear();
    return rv;
}

bool CPubseqGatewayCacheSi2Csi::LookupBySeqIdIdType(const string& sec_seqid, int sec_seq_id_type, string& data) {
    bool rv = false;

    if (!m_Env)
        return false;

    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    {

        string skey = PackKey(sec_seqid, sec_seq_id_type);
        lmdb::val val;
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
        rv = cursor.get(lmdb::val(skey), val, MDB_SET);
        if (rv)
            data.assign(val.data(), val.size());
    }

    rdtxn.commit();
    if (!rv)
        data.clear();
    return rv;
}

string CPubseqGatewayCacheSi2Csi::PackKey(const string& sec_seqid, int sec_seq_id_type) {
    string rv;
    rv.reserve(sec_seqid.size() + kPackedKeyZero + kPackedIdTypeSz);
    rv = sec_seqid;
    rv.append(1, 0);
    rv.append(1, (sec_seq_id_type >> 8) & 0xFF);
    rv.append(1, sec_seq_id_type        & 0xFF);
    return rv;
}

bool CPubseqGatewayCacheSi2Csi::UnpackKey(const char* key, size_t key_sz, int& sec_seq_id_type) {
    bool rv = key_sz > (kPackedKeyZero + kPackedIdTypeSz);
    if (rv) {
        size_t ofs = key_sz - (kPackedKeyZero + kPackedIdTypeSz);
        rv = key[ofs] == 0;
        if (rv) {
            ++ofs;
            sec_seq_id_type = (uint8_t(key[ofs]) << 8) |
                               uint8_t(key[ofs + 1]);
        }
    }
    return rv;
}

USING_NCBI_SCOPE;

