#ifndef __ASN_CACHE_INTERFACE_HPP
#define __ASN_CACHE_INTERFACE_HPP
/* 
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
 * Author:  Alex Kotliarov
 *
 * File Description:
 */
#include <corelib/ncbistd.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <util/simple_buffer.hpp>

#include <objtools/data_loaders/asn_cache/asn_cache_export.h>
#include <objtools/data_loaders/asn_cache/asn_index.hpp>
#include <objtools/data_loaders/asn_cache/Cache_blob.hpp>

#include <functional> 


BEGIN_NCBI_SCOPE

class IAsnCacheStore
{
public:
    virtual ~IAsnCacheStore() {}

    /// Return the raw blob in an unformatted buffer.
    virtual bool GetRaw(const objects::CSeq_id_Handle& id, vector<unsigned char>& buffer) = 0;
    virtual bool GetMultipleRaw(const objects::CSeq_id_Handle& id, vector<vector<unsigned char>>& buffer) = 0;

    /// Return the cache blob, packed and uninterpreted
    virtual bool GetBlob(const objects::CSeq_id_Handle& id, objects::CCache_blob& blob) = 0;
    virtual bool GetMultipleBlobs(const objects::CSeq_id_Handle& id,
                                  vector< CRef<objects::CCache_blob> >& blob) = 0;

    ///
    /// Return the set of seq-ids associated with a given ID. By default, if
    /// the SeqId index is not available, and the SeqIds can't be retrieved
    /// cheaply, does nothing and return false. If cheap_only is set to false,
    /// will always retrieve the SeqIds, by retrieving the full blob if that is
    /// the only available way.
    /// 
    virtual bool GetSeqIds(const objects::CSeq_id_Handle& id,
                           vector<objects::CSeq_id_Handle>& all_ids,
                           bool cheap_only) = 0;
            ///
    /// Check if the SeqId cache, for efficient retrieval of SeqIds, is
    /// available
    ///
#if 0
    virtual bool EfficientlyGetSeqIds() const = 0;
#endif
    /// Return a blob as a CSeq_entry object.
    virtual CRef<objects::CSeq_entry> GetEntry(const objects::CSeq_id_Handle& id) = 0;
    virtual vector< CRef<objects::CSeq_entry> > GetMultipleEntries(const objects::CSeq_id_Handle& id) = 0;

    /// Return the GI and timestamp for a given seq_id.  This can be a very
    /// fast way to look up the GI for an accession.version because only the
    /// index is queried -- the blob is not retrieved.
    virtual bool GetIdInfo(const objects::CSeq_id_Handle& id,
                            CAsnIndex::TGi& gi,
                            time_t& timestamp) = 0;

    /// Return the GI and timestamp for a given seq_id.  This can be a very
    /// fast way to look up the GI for an accession.version because only the
    /// index is queried -- the blob is not retrieved.
    virtual bool GetIdInfo(const objects::CSeq_id_Handle& id,
                           objects::CSeq_id_Handle& accession,
                           CAsnIndex::TGi& gi,
                           time_t& timestamp,
                           Uint4& sequence_length,
                           Uint4& tax_id) = 0;
    /// Get the full ASN cache index entry.  This does not retrieve the full
    /// blob and is very fast.
    virtual bool GetIndexEntry(const objects::CSeq_id_Handle & id,
                       CAsnIndex::SIndexInfo &info) = 0;

    virtual bool GetMultipleIndexEntries(const objects::CSeq_id_Handle & id,
                                 vector<CAsnIndex::SIndexInfo> &info) = 0;



    using TEnumSeqidCallback = std::function<void(string /*seq_id*/, uint32_t /*version*/,  uint64_t /*gi*/, uint32_t /*timestamp*/)>;
    using TEnumIndexCallback = std::function<void(string /*seq_id*/, uint32_t /*version*/,  uint64_t /*gi*/, uint32_t /*timestamp*/, uint32_t /*chunk*/, uint32_t /*offset*/, uint32_t /*size*/, uint32_t /*seq_len*/, uint32_t /*taxid*/)>;

    virtual size_t GetGiCount() const = 0;
    virtual void EnumSeqIds(TEnumSeqidCallback cb) const = 0;
    virtual void EnumIndex(TEnumIndexCallback cb) const = 0;
};

END_NCBI_SCOPE

#endif // __ASN_CACHE_INTERFACE_HPP
