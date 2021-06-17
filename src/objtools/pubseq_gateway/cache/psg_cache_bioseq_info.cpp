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
#include <utility>
#include <vector>

#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>

#include "psg_cache_bytes_util.hpp"

BEGIN_SCOPE()
USING_IDBLOB_SCOPE;

using TPackBytes = CPubseqGatewayCachePackBytes;
using TUnpackBytes = CPubseqGatewayCacheUnpackBytes;
using TField = CBioseqInfoFetchRequest::EFields;

static const unsigned kPackedZeroSz = 1;
static const unsigned kPackedVersionSz = 3;
static const unsigned kPackedSeqIdTypeSz = 2;
static const unsigned kPackedGISz = 8;

size_t PackedKeySize(size_t acc_sz)
{
    return acc_sz + (kPackedZeroSz + kPackedVersionSz + kPackedSeqIdTypeSz + kPackedGISz);
}

/*
void PrintKey(const string& rv)
{
    cout << "Result: " << rv.size() << " [ ";
    cout << hex;
    const char * r = rv.c_str();
    for (size_t i = 0; i < rv.size(); ++i) {
        cout << setw(2) << setfill('0') << static_cast<uint16_t>(r[i]) << " ";
    }
    cout << dec;
    cout << "]" << endl;

};
*/

END_SCOPE()


BEGIN_IDBLOB_SCOPE

CPubseqGatewayCacheBioseqInfo::CPubseqGatewayCacheBioseqInfo(const string& file_name)
    : CPubseqGatewayCacheBase(file_name)
{
}

CPubseqGatewayCacheBioseqInfo::~CPubseqGatewayCacheBioseqInfo() = default;

void CPubseqGatewayCacheBioseqInfo::Open()
{
    CPubseqGatewayCacheBase::Open();
    {
        auto rdtxn = BeginReadTxn();
        m_Dbi = unique_ptr<lmdb::dbi, function<void(lmdb::dbi*)>>(
            new lmdb::dbi({lmdb::dbi::open(rdtxn, "#DATA", 0)}),
            [this](lmdb::dbi* dbi){
                if (dbi && *dbi) {
                    dbi->close(*m_Env);
                }
                delete(dbi);
            }
        );
    }
}

string CPubseqGatewayCacheBioseqInfo::x_MakeLookupKey(CBioseqInfoFetchRequest const& request) const
{
    string accession = request.GetAccession();
    if (
        request.HasField(TField::eVersion)
        && request.HasField(TField::eSeqIdType)
        && request.HasField(TField::eGI))
    {
        return PackKey(accession, request.GetVersion(), request.GetSeqIdType(), request.GetGI());
    } else if (request.HasField(TField::eVersion) && request.HasField(TField::eSeqIdType)) {
        return PackKey(accession, request.GetVersion(), request.GetSeqIdType());
    } else if (request.HasField(TField::eVersion)) {
        return PackKey(accession, request.GetVersion());
    }

    return accession;
}

bool CPubseqGatewayCacheBioseqInfo::x_IsMatchingRecord(
    CBioseqInfoFetchRequest const& request,
    int version, int seq_id_type, int64_t gi
) const
{
    bool acceptable = true;
    if (request.HasField(TField::eGI)) {
        acceptable = acceptable && (gi == request.GetGI());
    }
    if (request.HasField(TField::eVersion)) {
        acceptable = acceptable && (version == request.GetVersion());
    }
    if (request.HasField(TField::eSeqIdType)) {
        acceptable = acceptable && (seq_id_type == request.GetSeqIdType());
    }
    return acceptable;
}

bool CPubseqGatewayCacheBioseqInfo::x_ExtractRecord(CBioseqInfoRecord& record, lmdb::val const& value) const
{
    ::psg::retrieval::BioseqInfoValue info;
    if (!info.ParseFromArray(value.data(), value.size())) {
        return false;
    }
    CBioseqInfoRecord::TSeqIds seq_ids;
    for (auto const & item : info.seq_ids()) {
        seq_ids.insert(make_tuple(static_cast<int16_t>(item.sec_seq_id_type()), item.sec_seq_id()));
    }
    record
        .SetHash(info.hash())
        .SetLength(info.length())
        .SetMol(info.mol())
        .SetName(info.name())
        .SetSat(info.blob_key().sat())
        .SetSatKey(info.blob_key().sat_key())
        .SetState(info.state())
        .SetSeqState(info.seq_state())
        .SetTaxId(info.tax_id())
        .SetDateChanged(info.date_changed())
        .SetSeqIds(move(seq_ids));
    return true;
}

vector<CBioseqInfoRecord> CPubseqGatewayCacheBioseqInfo::Fetch(CBioseqInfoFetchRequest const& request)
{
    vector<CBioseqInfoRecord> response;
    if (!request.HasField(CBioseqInfoFetchRequest::EFields::eAccession)) {
        return response;
    }

    string filter = x_MakeLookupKey(request);
    {
        auto rdtxn = BeginReadTxn();
        lmdb::val val;
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
        if (cursor.get(lmdb::val(filter), val, MDB_SET_RANGE)) {
            lmdb::val key;
            string accession = request.GetAccession();
            bool has_current = cursor.get(key, val, MDB_GET_CURRENT);
            while (has_current) {
                int seq_id_type{-1}, version{-1};
                int64_t gi{-1};
                if (
                    key.size() != PackedKeySize(accession.size())
                    || accession.compare(key.data<const char>()) != 0
                ) {
                    break;
                }

                has_current = UnpackKey(key.data<const char>(), key.size(), version, seq_id_type, gi);
                if (has_current && x_IsMatchingRecord(request, version, seq_id_type, gi)) {
                    response.resize(response.size() + 1);
                    auto& last_record = response[response.size() - 1];
                    last_record
                        .SetAccession(accession)
                        .SetVersion(version)
                        .SetSeqIdType(seq_id_type)
                        .SetGI(gi);
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

vector<CBioseqInfoRecord> CPubseqGatewayCacheBioseqInfo::FetchLast(void)
{
    vector<CBioseqInfoRecord> response;
    {
        auto rdtxn = BeginReadTxn();
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
        lmdb::val key, val;
        bool current = cursor.get(key, val, MDB_LAST);
        while (current) {
            int seq_id_type{-1}, version{-1};
            int64_t gi{-1};
            string accession;
            if (UnpackKey(key.data<const char>(), key.size(), accession, version, seq_id_type, gi)) {
                response.resize(response.size() + 1);
                auto& last_record = response[response.size() - 1];
                last_record
                    .SetAccession(accession)
                    .SetVersion(version)
                    .SetSeqIdType(seq_id_type)
                    .SetGI(gi);
                // Skip record if we cannot parse protobuf data
                if (!x_ExtractRecord(last_record, val)) {
                    response.resize(response.size() - 1);
                }
            }
            current = cursor.get(key, val, MDB_NEXT);
        }
    }

    return response;
}

string CPubseqGatewayCacheBioseqInfo::PackKey(const string& accession, int version)
{
    string rv;
    rv.reserve(accession.size() + kPackedZeroSz + kPackedVersionSz);
    rv = accession;
    rv.append(1, 0);
    int32_t ver = ~version;
    TPackBytes().Pack<kPackedVersionSz>(rv, ver);
    return rv;
}

string CPubseqGatewayCacheBioseqInfo::PackKey(const string& accession, int version, int seq_id_type)
{
    string rv;
    rv.reserve(accession.size() + kPackedZeroSz + kPackedVersionSz + kPackedSeqIdTypeSz);
    rv = accession;
    rv.append(1, 0);
    int32_t ver = ~version;
    TPackBytes().Pack<kPackedVersionSz>(rv, ver);
    TPackBytes().Pack<kPackedSeqIdTypeSz>(rv, seq_id_type);
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
    TPackBytes().Pack<kPackedVersionSz>(rv, ver);
    TPackBytes().Pack<kPackedSeqIdTypeSz>(rv, seq_id_type);
    TPackBytes().Pack<kPackedGISz>(rv, gi);
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
            int32_t ver = TUnpackBytes().Unpack<kPackedVersionSz, int32_t>(key + ofs + 1);
            version = ~(ver | 0xFF000000);
            seq_id_type = TUnpackBytes().Unpack<kPackedSeqIdTypeSz, int>(key + ofs + 4);
            gi = TUnpackBytes().Unpack<kPackedGISz, int64_t>(key + ofs + 6);
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

END_IDBLOB_SCOPE
