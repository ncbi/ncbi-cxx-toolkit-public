#ifndef __ASN_CACHE_STORE__HPP
#define __ASN_CACHE_STORE_HPP
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

#include <objtools/data_loaders/asn_cache/asn_cache_iface.hpp>
 
BEGIN_NCBI_SCOPE

class CAsnCacheStore : public IAsnCacheStore
{
    std::string m_DbPath;
    std::unique_ptr<CAsnIndex> m_Index;
    std::unique_ptr<CAsnIndex> m_SeqIdIndex;

    CAsnIndex::TChunkId m_CurrChunkId;
    std::unique_ptr<CChunkFile> m_CurrChunk;

    std::unique_ptr<CSeqIdChunkFile> m_SeqIdChunk;

    static bool s_GetChunkAndOffset(const objects::CSeq_id_Handle&   idh,
                                    CAsnIndex&              index,
                                    vector<CAsnIndex::SIndexInfo>&  info,
                                    bool                    multiple);

    CAsnIndex & x_GetIndexRef () const { return *m_Index; }
    bool x_GetBlob(const CAsnIndex::SIndexInfo &info, objects::CCache_blob& blob);

    
    static bool s_GetChunkAndOffset(const objects::CSeq_id_Handle&   idh,
                                    CAsnIndex&              index,
                                    CAsnIndex::SIndexInfo&  info);
public:
    CAsnCacheStore() = delete;
    CAsnCacheStore(CAsnCacheStore const&) = delete;
    CAsnCacheStore& operator= (CAsnCacheStore const&) = delete;

    explicit CAsnCacheStore(string const& dbpath);

    /// Return the raw blob in an unformatted buffer.
    bool GetRaw(const objects::CSeq_id_Handle& id, vector<unsigned char>& buffer);
    bool GetMultipleRaw(const objects::CSeq_id_Handle& id, vector<vector<unsigned char>>& buffer);

    /// Return the cache blob, packed and uninterpreted
    bool GetBlob(const objects::CSeq_id_Handle& id, objects::CCache_blob& blob);
    bool GetMultipleBlobs(const objects::CSeq_id_Handle& id,
                                  vector< CRef<objects::CCache_blob> >& blob);

    ///
    /// Return the set of seq-ids associated with a given ID. By default, if
    /// the SeqId index is not available, and the SeqIds can't be retrieved
    /// cheaply, does nothing and return false. If cheap_only is set to false,
    /// will always retrieve the SeqIds, by retrieving the full blob if that is
    /// the only available way.
    /// 
    bool GetSeqIds(const objects::CSeq_id_Handle& id,
                           vector<objects::CSeq_id_Handle>& all_ids,
                           bool cheap_only);
            ///
    /// Check if the SeqId cache, for efficient retrieval of SeqIds, is
    /// available
    ///
#if 0 // Is not being used
    bool EfficientlyGetSeqIds() const;
#endif

    /// Return a blob as a CSeq_entry object.
    CRef<objects::CSeq_entry> GetEntry(const objects::CSeq_id_Handle& id);
    vector< CRef<objects::CSeq_entry> > GetMultipleEntries(const objects::CSeq_id_Handle& id);

    /// Return the GI and timestamp for a given seq_id.  This can be a very
    /// fast way to look up the GI for an accession.version because only the
    /// index is queried -- the blob is not retrieved.
    bool GetIdInfo(const objects::CSeq_id_Handle& id,
                            CAsnIndex::TGi& gi,
                            time_t& timestamp);

    /// Return the GI and timestamp for a given seq_id.  This can be a very
    /// fast way to look up the GI for an accession.version because only the
    /// index is queried -- the blob is not retrieved.
    bool GetIdInfo(const objects::CSeq_id_Handle& id,
                           objects::CSeq_id_Handle& accession,
                           CAsnIndex::TGi& gi,
                           time_t& timestamp,
                           Uint4& sequence_length,
                           Uint4& tax_id);
    /// Get the full ASN cache index entry.  This does not retrieve the full
    /// blob and is very fast.
    bool GetIndexEntry(const objects::CSeq_id_Handle & id,
                       CAsnIndex::SIndexInfo &info);
    bool GetMultipleIndexEntries(const objects::CSeq_id_Handle & id,
                                 vector<CAsnIndex::SIndexInfo> &info);


    // IAsnCacheStats
    size_t GetGiCount() const;
    void EnumSeqIds(IAsnCacheStore::TEnumSeqidCallback cb) const;
    void EnumIndex(IAsnCacheStore::TEnumIndexCallback cb) const;
};

class CAsnCacheStoreMany : public IAsnCacheStore
{
    std::vector<std::unique_ptr<IAsnCacheStore> > m_Stores;
    std::vector<int> m_Index;

public:
    CAsnCacheStoreMany() = delete;
    CAsnCacheStoreMany(CAsnCacheStoreMany const&) = delete;
    CAsnCacheStoreMany& operator= (CAsnCacheStoreMany const&) = delete;

    explicit CAsnCacheStoreMany(vector<string> const& db_paths);

    /// Return the raw blob in an unformatted buffer.
    bool GetRaw(const objects::CSeq_id_Handle& id, vector<unsigned char>& buffer);
    bool GetMultipleRaw(const objects::CSeq_id_Handle& id, vector<vector<unsigned char>>& buffer);

    /// Return the cache blob, packed and uninterpreted
    bool GetBlob(const objects::CSeq_id_Handle& id, objects::CCache_blob& blob);
    bool GetMultipleBlobs(const objects::CSeq_id_Handle& id,
                                  vector< CRef<objects::CCache_blob> >& blob);

    ///
    /// Return the set of seq-ids associated with a given ID. By default, if
    /// the SeqId index is not available, and the SeqIds can't be retrieved
    /// cheaply, does nothing and return false. If cheap_only is set to false,
    /// will always retrieve the SeqIds, by retrieving the full blob if that is
    /// the only available way.
    /// 
    bool GetSeqIds(const objects::CSeq_id_Handle& id,
                           vector<objects::CSeq_id_Handle>& all_ids,
                           bool cheap_only);
            ///
    /// Check if the SeqId cache, for efficient retrieval of SeqIds, is
    /// available
    ///
#if 0
    bool EfficientlyGetSeqIds() const;
#endif

    /// Return a blob as a CSeq_entry object.
    CRef<objects::CSeq_entry> GetEntry(const objects::CSeq_id_Handle& id);
    vector< CRef<objects::CSeq_entry> > GetMultipleEntries(const objects::CSeq_id_Handle& id);

    /// Return the GI and timestamp for a given seq_id.  This can be a very
    /// fast way to look up the GI for an accession.version because only the
    /// index is queried -- the blob is not retrieved.
    bool GetIdInfo(const objects::CSeq_id_Handle& id,
                            CAsnIndex::TGi& gi,
                            time_t& timestamp);

    /// Return the GI and timestamp for a given seq_id.  This can be a very
    /// fast way to look up the GI for an accession.version because only the
    /// index is queried -- the blob is not retrieved.
    bool GetIdInfo(const objects::CSeq_id_Handle& id,
                           objects::CSeq_id_Handle& accession,
                           CAsnIndex::TGi& gi,
                           time_t& timestamp,
                           Uint4& sequence_length,
                           Uint4& tax_id);

    /// Get the full ASN cache index entry.  This does not retrieve the full
    /// blob and is very fast.
    bool GetIndexEntry(const objects::CSeq_id_Handle & id,
                       CAsnIndex::SIndexInfo &info);
    bool GetMultipleIndexEntries(const objects::CSeq_id_Handle & id,
                                 vector<CAsnIndex::SIndexInfo> &info);


    // IAsnCacheStats
    size_t GetGiCount() const;
    void EnumSeqIds(IAsnCacheStore::TEnumSeqidCallback cb) const;
    void EnumIndex(IAsnCacheStore::TEnumIndexCallback cb) const;
};


END_NCBI_SCOPE

#endif // __ASN_CACHE_STORE__HPP
