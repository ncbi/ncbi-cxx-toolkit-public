#ifndef PSG_CACHE_BIOSEQ_INFO__HPP
#define PSG_CACHE_BIOSEQ_INFO__HPP

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
 * File Description: bioseq_info table cache
 *
 */

#include <memory>
#include <string>
#include <vector>

#include "psg_cache_base.hpp"

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/request.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>

BEGIN_IDBLOB_SCOPE

class CPubseqGatewayCacheBioseqInfo
    : public CPubseqGatewayCacheBase
{
 public:
    explicit CPubseqGatewayCacheBioseqInfo(const string& file_name);
    virtual ~CPubseqGatewayCacheBioseqInfo() override;
    void Open();

    vector<CBioseqInfoRecord> Fetch(CBioseqInfoFetchRequest const& request);
    vector<CBioseqInfoRecord> FetchLast(void);

    static string PackKey(const string& accession, int version);
    static string PackKey(const string& accession, int version, int seq_id_type);
    static string PackKey(const string& accession, int version, int seq_id_type, int64_t gi);
    static bool UnpackKey(const char* key, size_t key_sz, int& version, int& seq_id_type, int64_t& gi);
    static bool UnpackKey(
        const char* key, size_t key_sz, string& accession, int& version, int& seq_id_type, int64_t& gi);

 private:
    bool x_ExtractRecord(CBioseqInfoRecord& record, lmdb::val const& value) const;
    string x_MakeLookupKey(CBioseqInfoFetchRequest const& request) const;
    bool x_IsMatchingRecord(CBioseqInfoFetchRequest const& request, int version, int seq_id_type, int64_t gi) const;
    void ResetDbi();
    unique_ptr<lmdb::dbi, function<void(lmdb::dbi*)>> m_Dbi;
};

END_IDBLOB_SCOPE

#endif  // PSG_CACHE_BIOSEQ_INFO__HPP
