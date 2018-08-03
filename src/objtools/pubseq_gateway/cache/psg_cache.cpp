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

#include <objtools/pubseq_gateway/cache/psg_cache.hpp>

#include "psg_cache_bioseq_info.hpp"
#include "psg_cache_si2csi.hpp"
#include "psg_cache_blob_prop.hpp"

USING_NCBI_SCOPE;

CPubseqGatewayCache::CPubseqGatewayCache(const string& bioseq_info_file_name, const string& si2csi_file_name, const string& blob_prop_file_name) :
    m_BioseqInfoPath(bioseq_info_file_name),
    m_Si2CsiPath(si2csi_file_name),
    m_BlobPropPath(blob_prop_file_name)
{
}

CPubseqGatewayCache::~CPubseqGatewayCache()
{
}

void CPubseqGatewayCache::Open(const vector<string>& sat_names)
{
    if (!m_BioseqInfoPath.empty()) {
        m_BioseqInfoCache.reset(new CPubseqGatewayCacheBioseqInfo(m_BioseqInfoPath));
        try {
            m_BioseqInfoCache->Open();
        }
        catch (const lmdb::error& e) {
            ERR_POST(Warning << "Failed to open " << m_BioseqInfoPath
                << " cache: " << e.what()
                << ", bioseq_info cache will not be used.");
            m_BioseqInfoCache.reset();
        }
    }
    if (!m_Si2CsiPath.empty()) {
        m_Si2CsiCache.reset(new CPubseqGatewayCacheSi2Csi(m_Si2CsiPath));
        try {
            m_Si2CsiCache->Open();
        }
        catch (const lmdb::error& e) {
            ERR_POST(Warning << "Failed to open " << m_Si2CsiPath
                << " cache: " << e.what()
                << ", si2csi cache will not be used.");
            m_Si2CsiCache.reset();
        }
    }
    if (!m_BlobPropPath.empty()) {
        m_BlobPropCache.reset(new CPubseqGatewayCacheBlobProp(m_BlobPropPath));
        try {
            m_BlobPropCache->Open(sat_names);
        }
        catch (const lmdb::error& e) {
            ERR_POST(Warning << "Failed to open " << m_BlobPropPath
                << " cache: " << e.what()
                << ", blob prop cache will not be used.");
            m_BlobPropCache.reset();
        }
    }
}

bool CPubseqGatewayCache::LookupBioseqInfoByAccession(const string& accession, int& version, int& seq_id_type, string& data)
{
    return m_BioseqInfoCache ? m_BioseqInfoCache->LookupByAccession(accession, version, seq_id_type, data) : false;
}

bool CPubseqGatewayCache::LookupBioseqInfoByAccessionVersion(const string& accession, int version, int& seq_id_type, string& data)
{
    return m_BioseqInfoCache ? m_BioseqInfoCache->LookupByAccessionVersion(accession, version, seq_id_type, data) : false;
}

bool CPubseqGatewayCache::LookupBioseqInfoByAccessionVersionIdType(const string& accession, int version, int seq_id_type, string& data)
{
    return m_BioseqInfoCache ? m_BioseqInfoCache->LookupByAccessionVersionIdType(accession, version, seq_id_type, data) : false;
}

string CPubseqGatewayCache::PackBioseqInfoKey(const string& accession, int version)
{
    return CPubseqGatewayCacheBioseqInfo::PackKey(accession, version);
}

string CPubseqGatewayCache::PackBioseqInfoKey(const string& accession, int version, int seq_id_type)
{
    return CPubseqGatewayCacheBioseqInfo::PackKey(accession, version, seq_id_type);
}

bool CPubseqGatewayCache::UnpackBioseqInfoKey(const char* key, size_t key_sz, int& version, int& seq_id_type)
{
    return CPubseqGatewayCacheBioseqInfo::UnpackKey(key, key_sz, version, seq_id_type);
}

bool CPubseqGatewayCache::UnpackBioseqInfoKey(const char* key, size_t key_sz, string& accession, int& version, int& seq_id_type)
{
    return CPubseqGatewayCacheBioseqInfo::UnpackKey(key, key_sz, accession, version, seq_id_type);
}

bool CPubseqGatewayCache::LookupCsiBySeqId(const string& sec_seqid, int& sec_seq_id_type, string& data)
{
    return m_Si2CsiCache ? m_Si2CsiCache->LookupBySeqId(sec_seqid, sec_seq_id_type, data) : false;
}

bool CPubseqGatewayCache::LookupCsiBySeqIdIdType(const string& sec_seqid, int sec_seq_id_type, string& data)
{
    return m_Si2CsiCache ? m_Si2CsiCache->LookupBySeqIdIdType(sec_seqid, sec_seq_id_type, data) : false;
}

string CPubseqGatewayCache::PackSiKey(const string& sec_seqid, int sec_seq_id_type)
{
    return CPubseqGatewayCacheSi2Csi::PackKey(sec_seqid, sec_seq_id_type);
}

bool CPubseqGatewayCache::UnpackSiKey(const char* key, size_t key_sz, int& sec_seq_id_type)
{
    return CPubseqGatewayCacheSi2Csi::UnpackKey(key, key_sz, sec_seq_id_type);
}

bool CPubseqGatewayCache::LookupBlobPropBySatKey(int32_t sat, int32_t sat_key, int64_t& last_modified, string& data)
{
    return m_BlobPropCache ? m_BlobPropCache->LookupBySatKey(sat, sat_key, last_modified, data) : false;
}

bool CPubseqGatewayCache::LookupBlobPropBySatKeyLastModified(int32_t sat, int32_t sat_key, int64_t last_modified, string& data)
{
    return m_BlobPropCache ? m_BlobPropCache->LookupBySatKeyLastModified(sat, sat_key, last_modified, data) : false;
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

