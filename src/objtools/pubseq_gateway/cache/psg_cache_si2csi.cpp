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
#include <vector>

#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>

#include "psg_cache_bytes_util.hpp"

BEGIN_SCOPE()
USING_IDBLOB_SCOPE;

using TPackBytes = CPubseqGatewayCachePackBytes;
using TUnpackBytes = CPubseqGatewayCacheUnpackBytes;

static const unsigned kPackedSeqIdTypeSz = 2;
static const unsigned kPackedKeyZero = 1;

size_t PackedKeySize(size_t acc_sz)
{
    return acc_sz + kPackedKeyZero + kPackedSeqIdTypeSz;
}

END_SCOPE()

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CPubseqGatewayCacheSi2Csi::CPubseqGatewayCacheSi2Csi(const string& file_name)
    : CPubseqGatewayCacheBase(file_name)
{
}

CPubseqGatewayCacheSi2Csi::~CPubseqGatewayCacheSi2Csi() = default;

void CPubseqGatewayCacheSi2Csi::Open()
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

bool CPubseqGatewayCacheSi2Csi::x_ExtractRecord(CSI2CSIRecord& record, lmdb::val const& value) const
{
    ::psg::retrieval::BioseqInfoKey info;
    if (!info.ParseFromArray(value.data(), value.size())) {
        return false;
    }
    record
        .SetAccession(info.accession())
        .SetVersion(info.version())
        .SetSeqIdType(info.seq_id_type())
        .SetGI(info.gi());
    return true;
}

vector<CSI2CSIRecord> CPubseqGatewayCacheSi2Csi::Fetch(CSi2CsiFetchRequest const& request)
{
    vector<CSI2CSIRecord> response;
    if (!m_Env || !request.HasField(CSi2CsiFetchRequest::EFields::eSecSeqId)) {
        return response;
    }
    string sec_seqid = request.GetSecSeqId();
    bool with_type = request.HasField(CSi2CsiFetchRequest::EFields::eSecSeqIdType);
    string filter = with_type ? PackKey(sec_seqid, request.GetSecSeqIdType()) : sec_seqid;
    {
        auto rdtxn = BeginReadTxn();
        lmdb::val val;
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
        if (cursor.get(lmdb::val(filter), val, MDB_SET_RANGE)) {
            lmdb::val key;
            bool has_current = cursor.get(key, val, MDB_GET_CURRENT);
            while (has_current) {
                int sec_seq_it_type{-1};
                if (
                    key.size() != PackedKeySize(sec_seqid.size())
                    || sec_seqid.compare(key.data<const char>()) != 0
                ) {
                    break;
                }
                has_current = UnpackKey(key.data<const char>(), key.size(), sec_seq_it_type);
                if (has_current && (!with_type || sec_seq_it_type == request.GetSecSeqIdType())) {
                    response.resize(response.size() + 1);
                    auto& last_record = response[response.size() - 1];
                    last_record
                        .SetSecSeqId(sec_seqid)
                        .SetSecSeqIdType(sec_seq_it_type);
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

vector<CSI2CSIRecord> CPubseqGatewayCacheSi2Csi::FetchLast(void)
{
    vector<CSI2CSIRecord> response;
    {
        auto rdtxn = BeginReadTxn();
        auto cursor = lmdb::cursor::open(rdtxn, *m_Dbi);
        lmdb::val key, val;
        bool has_current = cursor.get(key, val, MDB_LAST);
        while (has_current) {
            int sec_seq_it_type{-1};
            string sec_seqid;
            if (UnpackKey(key.data<const char>(), key.size(), sec_seqid, sec_seq_it_type)) {
                response.resize(response.size() + 1);
                auto& last_record = response[response.size() - 1];
                last_record
                    .SetSecSeqId(sec_seqid)
                    .SetSecSeqIdType(sec_seq_it_type);
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

string CPubseqGatewayCacheSi2Csi::PackKey(const string& sec_seqid, int sec_seq_id_type)
{
    string rv;
    rv.reserve(sec_seqid.size() + kPackedKeyZero + kPackedSeqIdTypeSz);
    rv = sec_seqid;
    rv.append(1, 0);
    TPackBytes().Pack<kPackedSeqIdTypeSz>(rv, sec_seq_id_type);
    return rv;
}

bool CPubseqGatewayCacheSi2Csi::UnpackKey(const char* key, size_t key_sz, int& sec_seq_id_type)
{
    bool rv = key_sz > (kPackedKeyZero + kPackedSeqIdTypeSz);
    if (rv) {
        size_t ofs = key_sz - (kPackedKeyZero + kPackedSeqIdTypeSz);
        rv = key[ofs] == 0;
        if (rv) {
            sec_seq_id_type = TUnpackBytes().Unpack<kPackedSeqIdTypeSz, int>(key + ofs + 1);
        }
    }
    return rv;
}

bool CPubseqGatewayCacheSi2Csi::UnpackKey(const char* key, size_t key_sz, string& sec_seqid, int& sec_seq_id_type)
{
    if (UnpackKey(key, key_sz, sec_seq_id_type)) {
        sec_seqid.assign(key, key_sz - (kPackedKeyZero + kPackedSeqIdTypeSz));
        return true;
    }
    return false;
}

END_IDBLOB_SCOPE
