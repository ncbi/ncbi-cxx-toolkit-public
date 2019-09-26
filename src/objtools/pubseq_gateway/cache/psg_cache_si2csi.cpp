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

#include "psg_cache_si2csi.hpp"

#include <memory>
#include <string>
#include <utility>

#include <util/lmdbxx/lmdb++.h>

USING_NCBI_SCOPE;

BEGIN_SCOPE()

static const unsigned kPackedSeqIdTypeSz = 2;
static const unsigned kPackedKeyZero = 1;

size_t PackedKeySize(size_t acc_sz)
{
    return acc_sz + kPackedKeyZero + kPackedSeqIdTypeSz;
}

END_SCOPE()

CPubseqGatewayCacheSi2Csi::CPubseqGatewayCacheSi2Csi(const string& file_name) :
    CPubseqGatewayCacheBase(file_name)
{
}

CPubseqGatewayCacheSi2Csi::~CPubseqGatewayCacheSi2Csi()
{
}

void CPubseqGatewayCacheSi2Csi::Open()
{
    CPubseqGatewayCacheBase::Open();
    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    m_Dbi = unique_ptr<lmdb::dbi, function<void(lmdb::dbi*)>>(
        new lmdb::dbi({lmdb::dbi::open(rdtxn, "#DATA", 0)}),
        [this](lmdb::dbi* dbi){
            if (dbi && *dbi) {
                dbi->close(*m_Env);
            }
            delete(dbi);
        }
    );
    rdtxn.commit();
}

bool CPubseqGatewayCacheSi2Csi::LookupBySeqId(const string& sec_seqid, int& sec_seq_id_type, string& data)
{
    if (!m_Env || sec_seqid.empty()) {
        return false;
    }
    bool rv = false;
    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    {
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
        rv = cursor.get(lmdb::val(sec_seqid), MDB_SET_RANGE);
        if (rv) {
            lmdb::val key, val;
            rv = cursor.get(key, val, MDB_GET_CURRENT);
            rv = rv && key.size() == PackedKeySize(sec_seqid.size()) && sec_seqid.compare(key.data<const char>()) == 0;
            if (rv) {
                rv = UnpackKey(key.data<const char>(), key.size(), sec_seq_id_type);
            }
            if (rv) {
                data.assign(val.data(), val.size());
            }
        }
    }

    rdtxn.commit();
    return rv;
}

bool CPubseqGatewayCacheSi2Csi::LookupBySeqIdSeqIdType(const string& sec_seqid, int sec_seq_id_type, string& data)
{
    if (!m_Env || sec_seqid.empty()) {
        return false;
    }
    bool rv = false;
    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    {
        string skey = PackKey(sec_seqid, sec_seq_id_type);
        lmdb::val val;
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
        rv = cursor.get(lmdb::val(skey), val, MDB_SET);
        if (rv) {
            data.assign(val.data(), val.size());
        }
    }

    rdtxn.commit();
    return rv;
}

string CPubseqGatewayCacheSi2Csi::PackKey(const string& sec_seqid, int sec_seq_id_type)
{
    string rv;
    rv.reserve(sec_seqid.size() + kPackedKeyZero + kPackedSeqIdTypeSz);
    rv = sec_seqid;
    rv.append(1, 0);
    rv.append(1, (sec_seq_id_type >> 8) & 0xFF);
    rv.append(1, sec_seq_id_type        & 0xFF);
    return rv;
}

bool CPubseqGatewayCacheSi2Csi::UnpackKey(const char* key, size_t key_sz, int& sec_seq_id_type)
{
    bool rv = key_sz > (kPackedKeyZero + kPackedSeqIdTypeSz);
    if (rv) {
        size_t ofs = key_sz - (kPackedKeyZero + kPackedSeqIdTypeSz);
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

