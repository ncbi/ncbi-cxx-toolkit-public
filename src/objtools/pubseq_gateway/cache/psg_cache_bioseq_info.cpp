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

#include "psg_cache_bioseq_info.hpp"

#include <memory>
#include <string>

#include <util/lmdbxx/lmdb++.h>

USING_NCBI_SCOPE;

BEGIN_SCOPE()

static const unsigned kPackedZeroSz = 1;
static const unsigned kPackedVersionSz = 3;
static const unsigned kPackedSeqIdTypeSz = 2;
static const unsigned kPackedGISz = 8;

class CPackBytes
{
 public:
    template<int count, typename TInt>
    void Pack(string& value, TInt bytes) const
    {
        using TUInt = typename make_unsigned<TInt>::type;
        using TIsLast = typename conditional<count == 0, true_type, false_type>::type;
        TUInt unsigned_bytes = static_cast<TUInt>(bytes);
        PackImpl<count>(value, unsigned_bytes, TIsLast());
    }

 private:
    template<int count, typename TUInt>
    void PackImpl(string& value, TUInt unsigned_bytes, false_type /*zero_count*/) const
    {
        using TIsLast = typename conditional<count == 1, true_type, false_type>::type;
        unsigned char c = (unsigned_bytes >> ((count - 1) * 8)) & 0xFF;
        value.append(1, static_cast<char>(c));
        PackImpl<count - 1>(value, unsigned_bytes, TIsLast());
    }

    template<int count, typename TUInt>
    void PackImpl(string&, TUInt, true_type /*zero_count*/) const
    {
    }
};

class CUnpackBytes
{
 public:
    template<int count, typename TInt>
    TInt Unpack(const char* key)
    {
        using TUInt = typename make_unsigned<TInt>::type;
        using TIsLast = typename conditional<count == 0, true_type, false_type>::type;
        const unsigned char* ukey = reinterpret_cast<const unsigned char*>(key);
        TUInt result{};
        UnpackImpl<count>(ukey, result, TIsLast());
        return static_cast<TInt>(result);
    }

 private:
    template<int count, typename TUInt>
    void UnpackImpl(const unsigned char* ukey, TUInt& result, false_type /*zero_count*/) const
    {
        using TIsLast = typename conditional<count == 1, true_type, false_type>::type;
        result |= static_cast<TUInt>(ukey[0]) << ((count - 1) * 8);
        UnpackImpl<count - 1>(ukey + 1, result, TIsLast());
    }

    template<int count, typename TUInt>
    void UnpackImpl(const unsigned char*, TUInt&, true_type /*zero_count*/) const
    {
    }
};

size_t PackedKeySize(size_t acc_sz)
{
    return acc_sz + (kPackedZeroSz + kPackedVersionSz + kPackedSeqIdTypeSz + kPackedGISz);
}

/*
void PrintKey(const string& rv)
{
    cout << "Result: " << rv.size() << " [ ";
    cout << hex << setw(2) << setfill('0');
    const char * r = rv.c_str();
    for (size_t i = 0; i < rv.size(); ++i) {
        cout << static_cast<int>(r[i]) << " ";
    }
    cout << dec;
    cout << " ]" << endl;

};
*/

END_SCOPE()

CPubseqGatewayCacheBioseqInfo::CPubseqGatewayCacheBioseqInfo(const string& file_name) :
    CPubseqGatewayCacheBase(file_name)
{
}

CPubseqGatewayCacheBioseqInfo::~CPubseqGatewayCacheBioseqInfo()
{
}

void CPubseqGatewayCacheBioseqInfo::Open()
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

// LOOKUPS data for accession. Picks record with maximum version and minimum seq_id_type
// (latter two would appear first according to built-in sorting order)
bool CPubseqGatewayCacheBioseqInfo::LookupByAccession(
    const string& accession, string& data, int& found_version, int& found_seq_id_type, int64_t& found_gi)
{
    bool rv = false;
    if (!m_Env || accession.empty()) {
        return false;
    }

    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    {
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
        rv = cursor.get(lmdb::val(accession), MDB_SET_RANGE);
        if (rv) {
            lmdb::val key, val;
            rv = cursor.get(key, val, MDB_GET_CURRENT);
            rv = rv && key.size() == PackedKeySize(accession.size()) && accession.compare(key.data<const char>()) == 0;
            if (rv) {
                found_version = -1;
                found_seq_id_type = 0;
                rv = UnpackKey(key.data<const char>(), key.size(), found_version, found_seq_id_type, found_gi);
            }
            if (rv) {
                data.assign(val.data(), val.size());
            }
        }
    }

    rdtxn.commit();
    if (!rv) {
        data.clear();
    }
    return rv;
}

// LOOKUPS data for accession and version. Picks record with minimum seq_id_type
// (latter would appear first according to built-in sorting order)
bool CPubseqGatewayCacheBioseqInfo::LookupByAccessionVersion(
    const string& accession, int version, string& data, int& found_seq_id_type, int64_t& found_gi)
{
    bool rv = false;
    if (!m_Env) {
        return false;
    }

    if (version < 0) {
        int _found_version;
        return LookupByAccessionVersionSeqIdType(
            accession, version, 0, data, _found_version, found_seq_id_type, found_gi);
    }

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
                int _found_version;
                rv = UnpackKey(key.data<const char>(), key.size(), _found_version, found_seq_id_type, found_gi)
                    && (_found_version == version);
                if (rv) {
                    data.assign(val.data(), val.size());
                }
            }
        }
    }

    rdtxn.commit();
    if (!rv) {
        data.clear();
    }
    return rv;
}


// LOOKUPS data for accession, potentially version (if >= 0) and potentially seq_id_type (if > 0).
// Picks record with matched version (or maximum version if < 0) and matched seq_id_type
// or minimum seq_id_type (if <= 0)
bool CPubseqGatewayCacheBioseqInfo::LookupByAccessionVersionSeqIdType(
    const string& accession,
    int version,
    int seq_id_type,
    string& data,
    int& found_version,
    int& found_seq_id_type,
    int64_t& found_gi
)
{
    bool rv = false;
    if (!m_Env) {
        return false;
        data.clear();
    }

    if (version >= 0 && seq_id_type <= 0) {
        bool rv = LookupByAccessionVersion(accession, version, data, found_seq_id_type, found_gi);
        if (rv) {
            found_version = version;
        }
        return rv;
    }

    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    {
        lmdb::val val;
        // Request for MAX version or unkown seq_id_type
        if (version < 0) {
            auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
            rv = cursor.get(lmdb::val(accession), MDB_SET_RANGE);
            if (rv) {
                lmdb::val key;
                rv = cursor.get(key, val, MDB_GET_CURRENT);
                while (rv) {
                    int _found_seq_id_type = -1;
                    int _found_version = -1;
                    int64_t _found_gi = -1;
                    rv = key.size() == PackedKeySize(accession.size())
                        && accession.compare(key.data<const char>()) == 0;
                    if (!rv) {
                        break;
                    }
                    if (rv) {
                        rv = UnpackKey(
                            key.data<const char>(), key.size(), _found_version, _found_seq_id_type, _found_gi);
                    }
                    rv = rv && (seq_id_type <= 0 || seq_id_type == _found_seq_id_type);
                    if (rv) {
                        found_version = _found_version;
                        found_seq_id_type = _found_seq_id_type;
                        found_gi = _found_gi;
                        break;
                    }
                    rv = cursor.get(key, val, MDB_NEXT);
                }
            }
        } else {
            string skey = PackKey(accession, version, seq_id_type);
            auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
            rv = cursor.get(lmdb::val(skey), val, MDB_SET_RANGE);
            if (rv) {
                lmdb::val key;
                rv = cursor.get(key, val, MDB_GET_CURRENT);
                while (rv) {
                    int _found_seq_id_type = -1;
                    int _found_version = -1;
                    int64_t _found_gi = -1;
                    rv = key.size() == PackedKeySize(accession.size())
                        && accession.compare(key.data<const char>()) == 0;
                    if (!rv) {
                        break;
                    }
                    if (rv) {
                        rv = UnpackKey(
                            key.data<const char>(), key.size(), _found_version, _found_seq_id_type, _found_gi);
                    }
                    rv = rv && (version == _found_version && seq_id_type == _found_seq_id_type);
                    if (rv) {
                        found_version = _found_version;
                        found_seq_id_type = _found_seq_id_type;
                        found_gi = _found_gi;
                        break;
                    }
                    rv = cursor.get(key, val, MDB_NEXT);
                }
            }
        }
        if (rv) {
            data.assign(val.data(), val.size());
        }
    }

    rdtxn.commit();
    if (!rv) {
        data.clear();
    }
    return rv;
}

// LOOKUPS data for accession and GI pair
bool CPubseqGatewayCacheBioseqInfo::LookupBioseqInfoByAccessionGi(
    const string& accession, int64_t gi, string& data, int& found_version, int& found_seq_id_type)
{
    bool rv = false;
    if (!m_Env || accession.empty()) {
        return false;
        data.clear();
    }

    auto rdtxn = lmdb::txn::begin(*m_Env, nullptr, MDB_RDONLY);
    {
        lmdb::val val;
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
        rv = cursor.get(lmdb::val(accession), MDB_SET_RANGE);
        if (rv) {
            lmdb::val key;
            rv = cursor.get(key, val, MDB_GET_CURRENT);
            while (rv) {
                int _found_seq_id_type = -1;
                int _found_version = -1;
                int64_t _found_gi = -1;
                rv = key.size() == PackedKeySize(accession.size()) && accession.compare(key.data<const char>()) == 0;
                if (!rv) {
                    break;
                }
                if (rv) {
                    rv = UnpackKey(key.data<const char>(), key.size(), _found_version, _found_seq_id_type, _found_gi);
                }
                rv = rv && (gi == _found_gi);
                if (rv) {
                    found_version = _found_version;
                    found_seq_id_type = _found_seq_id_type;
                    break;
                }
                rv = cursor.get(key, val, MDB_NEXT);
            }
        }
        if (rv) {
            data.assign(val.data(), val.size());
        }
    }

    rdtxn.commit();
    if (!rv) {
        data.clear();
    }
    return rv;
}

string CPubseqGatewayCacheBioseqInfo::PackKey(const string& accession, int version)
{
    string rv;
    rv.reserve(accession.size() + kPackedZeroSz + kPackedVersionSz);
    rv = accession;
    rv.append(1, 0);
    int32_t ver = ~version;
    CPackBytes().Pack<3>(rv, ver);
    return rv;
}

string CPubseqGatewayCacheBioseqInfo::PackKey(const string& accession, int version, int seq_id_type)
{
    string rv;
    rv.reserve(accession.size() + kPackedZeroSz + kPackedVersionSz + kPackedSeqIdTypeSz);
    rv = accession;
    rv.append(1, 0);
    int32_t ver = ~version;
    CPackBytes().Pack<3>(rv, ver);
    CPackBytes().Pack<2>(rv, seq_id_type);
    return rv;
}

string CPubseqGatewayCacheBioseqInfo::PackKey(const string& accession, int version, int seq_id_type, int64_t gi)
{
    string rv;
    rv.reserve(accession.size() + kPackedZeroSz + kPackedVersionSz + kPackedSeqIdTypeSz + kPackedGISz);
    rv = accession;
    rv.append(1, 0);
    int32_t ver = ~version;
    gi = ~gi;
    CPackBytes().Pack<3>(rv, ver);
    CPackBytes().Pack<2>(rv, seq_id_type);
    CPackBytes().Pack<8>(rv, gi);
    return rv;
}

bool CPubseqGatewayCacheBioseqInfo::UnpackKey(
    const char* key, size_t key_sz, int& version, int& seq_id_type, int64_t& gi)
{
    bool rv = key_sz > (kPackedZeroSz + kPackedVersionSz + kPackedSeqIdTypeSz + kPackedGISz);
    if (rv) {
        size_t ofs = key_sz - (kPackedZeroSz + kPackedVersionSz + kPackedSeqIdTypeSz + kPackedGISz);
        rv = key[ofs] == 0;
        if (rv) {
            int32_t ver = CUnpackBytes().Unpack<3, int32_t>(key + ofs + 1);
            version = ~(ver | 0xFF000000);
            seq_id_type = CUnpackBytes().Unpack<2, int>(key + ofs + 4);
            gi = CUnpackBytes().Unpack<8, int64_t>(key + ofs + 6);
            gi = ~gi;
        }
    }
    return rv;
}

bool CPubseqGatewayCacheBioseqInfo::UnpackKey(
    const char* key, size_t key_sz, string& accession, int& version, int& seq_id_type, int64_t& gi
) {
    bool rv = key_sz > (kPackedZeroSz + kPackedVersionSz + kPackedSeqIdTypeSz + kPackedGISz);
    if (rv) {
        size_t ofs = key_sz - (kPackedZeroSz + kPackedVersionSz + kPackedSeqIdTypeSz + kPackedGISz);
        accession.assign(key, ofs);
        rv = UnpackKey(key, key_sz, version, seq_id_type, gi);
    }
    return rv;
}

USING_NCBI_SCOPE;

