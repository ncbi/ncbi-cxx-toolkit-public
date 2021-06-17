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

void ApplyInheritedSeqIds(CPubseqGatewayCacheBioseqInfo* cache, CBioseqInfoRecord& record)
{
    if (cache) {
        if (record.GetState() != ncbi::psg::retrieval::SEQ_STATE_LIVE) {
            CPubseqGatewayCache::TBioseqInfoRequest request;
            request.SetAccession(record.GetAccession()).SetSeqIdType(record.GetSeqIdType());
            CPubseqGatewayCache::TBioseqInfoResponse response = cache->Fetch(request);
            if (!response.empty()) {
                auto& latest_record = response[0];
                if (latest_record.GetState() == ncbi::psg::retrieval::SEQ_STATE_LIVE) {
                    set<int16_t> seq_id_types;
                    for (auto const & seq_id : record.GetSeqIds()) {
                        seq_id_types.insert(get<0>(seq_id));
                    }
                    for (auto const & seq_id : latest_record.GetSeqIds()) {
                        if (seq_id_types.count(get<0>(seq_id)) == 0) {
                            record.GetSeqIds().insert(seq_id);
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

CPubseqGatewayCache::TBioseqInfoResponse CPubseqGatewayCache::FetchBioseqInfo(TBioseqInfoRequest const& request)
{
    if (m_BioseqInfoCache && request.HasField(TBioseqInfoRequest::EFields::eAccession)) {
        TBioseqInfoResponse response = m_BioseqInfoCache->Fetch(request);
        for (auto & record : response) {
            ApplyInheritedSeqIds(m_BioseqInfoCache.get(), record);
        }
        return response;
    }
    return TBioseqInfoResponse();
}

CPubseqGatewayCache::TBioseqInfoResponse CPubseqGatewayCache::FetchBioseqInfoLast(void)
{
    if (m_BioseqInfoCache) {
        TBioseqInfoResponse response = m_BioseqInfoCache->FetchLast();
        for (auto & record : response) {
            ApplyInheritedSeqIds(m_BioseqInfoCache.get(), record);
        }
        return response;
    }
    return TBioseqInfoResponse();
}

CPubseqGatewayCache::TBlobPropResponse CPubseqGatewayCache::FetchBlobProp(TBlobPropRequest const& request)
{
    if (m_BlobPropCache) {
        return m_BlobPropCache->Fetch(request);
    }

    return TBlobPropResponse();
}

CPubseqGatewayCache::TBlobPropResponse CPubseqGatewayCache::FetchBlobPropLast(TBlobPropRequest const& request)
{
    if (m_BlobPropCache) {
        return m_BlobPropCache->FetchLast(request);
    }

    return TBlobPropResponse();
}

CPubseqGatewayCache::TSi2CsiResponse CPubseqGatewayCache::FetchSi2Csi(TSi2CsiRequest const& request)
{
    if (m_Si2CsiCache && request.HasField(TSi2CsiRequest::EFields::eSecSeqId)) {
        return m_Si2CsiCache->Fetch(request);
    }

    return TSi2CsiResponse();
}

CPubseqGatewayCache::TSi2CsiResponse CPubseqGatewayCache::FetchSi2CsiLast(void)
{
    if (m_Si2CsiCache) {
        return m_Si2CsiCache->FetchLast();
    }

    return TSi2CsiResponse();
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
