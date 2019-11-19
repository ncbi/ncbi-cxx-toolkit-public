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

#include <objtools/pubseq_gateway/cache/psg_cache.hpp>

#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <util/lmdbxx/lmdb++.h>

#include "psg_cache_bioseq_info.hpp"
#include "psg_cache_si2csi.hpp"
#include "psg_cache_blob_prop.hpp"

#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>

BEGIN_SCOPE()
USING_IDBLOB_SCOPE;
USING_NCBI_SCOPE;

void sAddRuntimeError(
    CPubseqGatewayCache::TRuntimeErrorList& error_list,
    CPubseqGatewayCache::TRuntimeError error
)
{
    ERR_POST(Warning << error.message);
    if (CPubseqGatewayCache::kRuntimeErrorLimit > 0) {
        while (error_list.size() >= CPubseqGatewayCache::kRuntimeErrorLimit) {
            error_list.pop_front();
        }
    }
    error_list.emplace_back(error);
}

void ApplyInheritedSeqIds(
    CPubseqGatewayCacheBioseqInfo* cache,
    string const & accession,
    int seq_id_type,
    string& data)
{
    if (cache) {
        ncbi::psg::retrieval::BioseqInfoValue record;
        if (record.ParseFromString(data) && record.state() != ncbi::psg::retrieval::SEQ_STATE_LIVE) {
            CPubseqGatewayCache::TBioseqInfoRequest request;
            CPubseqGatewayCache::TBioseqInfoResponse response;
            request.SetAccession(accession).SetSeqIdType(seq_id_type);
            cache->Fetch(request, response);
            if (!response.empty()) {
                ncbi::psg::retrieval::BioseqInfoValue latest_record;
                if (
                    latest_record.ParseFromString(response[0].data)
                    && latest_record.state() == ncbi::psg::retrieval::SEQ_STATE_LIVE
                ) {
                    set<int16_t> seq_id_types;
                    for (auto const & seq_id : record.seq_ids()) {
                        seq_id_types.insert(seq_id.sec_seq_id_type());
                    }
                    bool needs_update{false};
                    for (auto const & seq_id : latest_record.seq_ids()) {
                        if (seq_id_types.count(seq_id.sec_seq_id_type()) == 0) {
                            auto item = record.mutable_seq_ids()->Add();
                            item->set_sec_seq_id_type(seq_id.sec_seq_id_type());
                            item->set_sec_seq_id(seq_id.sec_seq_id());
                            needs_update = true;
                        }
                    }

                    if (needs_update) {
                        string updated_data;
                        if (record.SerializeToString(&updated_data)) {
                            swap(data, updated_data);
                        }
                    }
                }
            }
        }
    }
}

END_SCOPE()

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

const size_t CPubseqGatewayCache::kRuntimeErrorLimit = 10;

CPubseqGatewayCache::CPubseqGatewayCache(
    const string& bioseq_info_file_name, const string& si2csi_file_name, const string& blob_prop_file_name
)
    : m_BioseqInfoPath(bioseq_info_file_name)
    , m_Si2CsiPath(si2csi_file_name)
    , m_BlobPropPath(blob_prop_file_name)
{
}

CPubseqGatewayCache::~CPubseqGatewayCache()
{
}

void CPubseqGatewayCache::Open(const set<int>& sat_ids)
{
    if (!m_BioseqInfoPath.empty()) {
        m_BioseqInfoCache.reset(new CPubseqGatewayCacheBioseqInfo(m_BioseqInfoPath));
        try {
            m_BioseqInfoCache->Open();
        } catch (const lmdb::error& e) {
            stringstream s;
            s << "Failed to open '" << m_BioseqInfoPath << "' cache: "
                << e.what() << ", bioseq_info cache will not be used.";
            TRuntimeError error(s.str());
            sAddRuntimeError(m_RuntimeErrors, error);
            m_BioseqInfoCache.reset();
        }
    }
    if (!m_Si2CsiPath.empty()) {
        m_Si2CsiCache.reset(new CPubseqGatewayCacheSi2Csi(m_Si2CsiPath));
        try {
            m_Si2CsiCache->Open();
        } catch (const lmdb::error& e) {
            stringstream s;
            s << "Failed to open '" << m_Si2CsiPath << "' cache: " << e.what() << ", si2csi cache will not be used.";
            TRuntimeError error(s.str());
            sAddRuntimeError(m_RuntimeErrors, error);
            m_Si2CsiCache.reset();
        }
    }
    if (!m_BlobPropPath.empty()) {
        m_BlobPropCache.reset(new CPubseqGatewayCacheBlobProp(m_BlobPropPath));
        try {
            m_BlobPropCache->Open(sat_ids);
        } catch (const lmdb::error& e) {
            stringstream s;
            s << "Failed to open '" << m_BlobPropPath
              << "' cache: " << e.what() << ", blob prop cache will not be used.";
            TRuntimeError error(s.str());
            sAddRuntimeError(m_RuntimeErrors, error);
            m_BlobPropCache.reset();
        }
    }
}

void CPubseqGatewayCache::ResetErrors()
{
    m_RuntimeErrors.clear();
}

void CPubseqGatewayCache::FetchBioseqInfo(TBioseqInfoRequest const& request, TBioseqInfoResponse & response)
{
    if (m_BioseqInfoCache) {
        if (request.HasField(TBioseqInfoRequest::EFields::eAccession)) {
            TBioseqInfoResponse result;
            m_BioseqInfoCache->Fetch(request, result);
            for (auto & record : result) {
                ApplyInheritedSeqIds(m_BioseqInfoCache.get(), record.accession, record.seq_id_type, record.data);
            }
            swap(result, response);
        }
    }
}

void CPubseqGatewayCache::FetchBlobProp(TBlobPropRequest const& request, TBlobPropResponse & response)
{
    if (m_BlobPropCache) {
        if (
            request.HasField(TBlobPropRequest::EFields::eSat)
            && request.HasField(TBlobPropRequest::EFields::eSatKey)
        ) {
            TBlobPropResponse result;
            m_BlobPropCache->Fetch(request, result);
            swap(result, response);
        }
    }
}

string CPubseqGatewayCache::PackBioseqInfoKey(const string& accession, int version)
{
    return CPubseqGatewayCacheBioseqInfo::PackKey(accession, version);
}

string CPubseqGatewayCache::PackBioseqInfoKey(const string& accession, int version, int seq_id_type)
{
    return CPubseqGatewayCacheBioseqInfo::PackKey(accession, version, seq_id_type);
}

string CPubseqGatewayCache::PackBioseqInfoKey(const string& accession, int version, int seq_id_type, int64_t gi)
{
    return CPubseqGatewayCacheBioseqInfo::PackKey(accession, version, seq_id_type, gi);
}

bool CPubseqGatewayCache::UnpackBioseqInfoKey(
    const char* key, size_t key_sz, int& version, int& seq_id_type, int64_t& gi)
{
    return CPubseqGatewayCacheBioseqInfo::UnpackKey(key, key_sz, version, seq_id_type, gi);
}

bool CPubseqGatewayCache::UnpackBioseqInfoKey(
    const char* key, size_t key_sz, string& accession, int& version, int& seq_id_type, int64_t& gi)
{
    return CPubseqGatewayCacheBioseqInfo::UnpackKey(key, key_sz, accession, version, seq_id_type, gi);
}

bool CPubseqGatewayCache::LookupCsiBySeqId(const string& sec_seqid, int& sec_seq_id_type, string& data)
{
    return m_Si2CsiCache ? m_Si2CsiCache->LookupBySeqId(sec_seqid, sec_seq_id_type, data) : false;
}

bool CPubseqGatewayCache::LookupCsiBySeqIdSeqIdType(const string& sec_seqid, int sec_seq_id_type, string& data)
{
    return m_Si2CsiCache ? m_Si2CsiCache->LookupBySeqIdSeqIdType(sec_seqid, sec_seq_id_type, data) : false;
}

string CPubseqGatewayCache::PackSiKey(const string& sec_seqid, int sec_seq_id_type)
{
    return CPubseqGatewayCacheSi2Csi::PackKey(sec_seqid, sec_seq_id_type);
}

bool CPubseqGatewayCache::UnpackSiKey(const char* key, size_t key_sz, int& sec_seq_id_type)
{
    return CPubseqGatewayCacheSi2Csi::UnpackKey(key, key_sz, sec_seq_id_type);
}

string CPubseqGatewayCache::PackBlobPropKey(int32_t sat_key)
{
    return CPubseqGatewayCacheBlobProp::PackKey(sat_key);
}

string CPubseqGatewayCache::PackBlobPropKey(int32_t sat_key, int64_t last_modified)
{
    return CPubseqGatewayCacheBlobProp::PackKey(sat_key, last_modified);
}

bool CPubseqGatewayCache::UnpackBlobPropKey(const char* key, size_t key_sz, int64_t& last_modified)
{
    return CPubseqGatewayCacheBlobProp::UnpackKey(key, key_sz, last_modified);
}

bool CPubseqGatewayCache::UnpackBlobPropKey(const char* key, size_t key_sz, int64_t& last_modified, int32_t& sat_key)
{
    return CPubseqGatewayCacheBlobProp::UnpackKey(key, key_sz, last_modified, sat_key);
}

END_IDBLOB_SCOPE
