#ifndef ___ASN_CACHE__HPP
#define ___ASN_CACHE__HPP

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
 * Authors:  Mike DiCuccio Cheinan Marks Eyal Mozes
 *
 */
 
/** @file asn_cache.hpp
 * Contains the class definiton for CAsnCache, the main
 * client class for accessing the ASN cache data.
 *
 */

#include <corelib/ncbistd.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <util/simple_buffer.hpp>

#include <objtools/data_loaders/asn_cache/asn_cache_export.h>
#include <objtools/data_loaders/asn_cache/asn_index.hpp>
#include <objtools/data_loaders/asn_cache/Cache_blob.hpp>

BEGIN_NCBI_SCOPE


class CCompressionIStream;
class CSubCacheCreate;
class CChunkFile;
class CSeqIdChunkFile;
class CBitVectorWrapper;

/// CAsnCache is used by clients to access the ASN cache data.  The ASN
/// cache is a cache of the ID database that is designed for fast access
/// and retrieval of CSeq_entry blobs.
/// @note Data in the ASN cache can also be accessed via the object manager
/// and the ASN cache data loader, CAsnCache_DataLoader.
class CAsnCache : public CObject
{
public:
    /// Type used to hold raw (unformatted) blob data.
    typedef vector<unsigned char> TBuffer;

    /// Pass in the path to the ASN cache to construct an object.
    CAsnCache(const string& db_path);
    ~CAsnCache();

    /// Return the raw blob in an unformatted buffer.
    bool GetRaw(const objects::CSeq_id_Handle& id, TBuffer& buffer);
    bool GetMultipleRaw(const objects::CSeq_id_Handle& id, vector<TBuffer>& buffer);

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
                   bool cheap_only = true);
    ///
    /// Check if the SeqId cache, for efficient retrieval of SeqIds, is
    /// available
    ///
    bool EfficientlyGetSeqIds() const
    { return m_SeqIdIndex.get(); }

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

private:

    /// Get the full ASN cache index entry.  This does not retrieve the full
    /// blob and is very fast.
    bool GetIndexEntry(const objects::CSeq_id_Handle & id,
                       CAsnIndex::SIndexInfo &info);
    bool GetMultipleIndexEntries(const objects::CSeq_id_Handle & id,
                                 vector<CAsnIndex::SIndexInfo> &info);

    static bool s_GetChunkAndOffset(const objects::CSeq_id_Handle&   idh,
                                    CAsnIndex&              index,
                                    vector<CAsnIndex::SIndexInfo>&  info,
                                    bool                    multiple);
    
    static bool s_GetChunkAndOffset(const objects::CSeq_id_Handle&   idh,
                                    CAsnIndex&              index,
                                    CAsnIndex::SIndexInfo&  info);

    string m_DbPath;
    AutoPtr<CAsnIndex> m_Index;
    AutoPtr<CAsnIndex> m_SeqIdIndex;

    CAsnIndex::TChunkId m_CurrChunkId;
    AutoPtr<CChunkFile> m_CurrChunk;

    AutoPtr<CSeqIdChunkFile> m_SeqIdChunk;

    CAsnIndex & GetIndexRef () const { return *m_Index; }
    bool x_GetBlob(const CAsnIndex::SIndexInfo &info, objects::CCache_blob& blob);

    CAsnCache(const CAsnCache&);
    CAsnCache& operator=(const CAsnCache&);

    friend class CAsnCacheStats;
    friend class ::CAsnCacheApplication;
    friend class objects::CAsnCache_DataLoader;
};



END_NCBI_SCOPE


#endif  // ___ASN_CACHE__HPP
