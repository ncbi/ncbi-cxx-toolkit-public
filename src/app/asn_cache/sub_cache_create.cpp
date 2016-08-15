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
 * Authors:  Cheinan Marks, Eyal Mozes
 *
 * File Description:
 * Given a list of GIs, this program extracts an ASN.1 cache of the given GIs
 * into another ASN.1 cache, the subcache.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_signal.hpp>

#include <objtools/data_loaders/asn_cache/asn_cache.hpp>
#include <objtools/data_loaders/asn_cache/file_names.hpp>
#include <objtools/data_loaders/asn_cache/chunk_file.hpp>
#include <objtools/data_loaders/asn_cache/seq_id_chunk_file.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_util.hpp>

#include <serial/iterator.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#ifdef HAVE_NCBI_VDB
#  include <sra/data_loaders/wgs/wgsloader.hpp>
#endif

#include <misc/data_loaders_util/data_loaders_util.hpp>

#include <db/bdb/bdb_cursor.hpp>

#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <functional>


USING_NCBI_SCOPE;
USING_SCOPE(objects);


///////////////////////////////////////////////////////////////////////////////
///
/// Indexing primitives
///

struct SSeqIdIndex {
    CSeq_id_Handle      m_Idh;
    CAsnIndex::TSeqId   m_SeqId;
    CAsnIndex::TVersion m_Version;

    SSeqIdIndex(CSeq_id_Handle idh)
        : m_Idh(idh)
    {
        GetNormalizedSeqId(m_Idh, m_SeqId, m_Version);
    }

    bool operator<(const SSeqIdIndex& k2) const
    {
        return m_SeqId < k2.m_SeqId ||
               (m_SeqId == k2.m_SeqId && m_Version < k2.m_Version);
    }

    bool operator<(const CAsnIndex& index) const
    {
        return m_SeqId < index.GetSeqId() ||
               (m_SeqId == index.GetSeqId() && m_Version < index.GetVersion());
    }

    bool operator==(const CAsnIndex& index) const
    {
        return m_SeqId == index.GetSeqId() && m_Version == index.GetVersion();
    }

    bool operator>=(const CAsnIndex& index) const
    {
        return !(*this < index);
    }
};

struct SBlobLocator
{
    SBlobLocator(CSeq_id_Handle idh, const CDir &root_cache)
    : m_Idh(idh), m_CacheRoot(&root_cache)
    {}

    SBlobLocator &operator=(const CAsnIndex &main_index)
    {
        m_ChunkId = main_index.GetChunkId();
        m_Offset = main_index.GetOffset();
        return *this;
    }

    bool operator<(const SBlobLocator& k2) const
    {
        if (m_CacheRoot->GetPath() < k2.m_CacheRoot->GetPath()) { return true;  }
        if (k2.m_CacheRoot->GetPath() < m_CacheRoot->GetPath()) { return false; }
        if (m_ChunkId < k2.m_ChunkId) { return true;  }
        if (k2.m_ChunkId < m_ChunkId) { return false; }

        return (m_Offset < k2.m_Offset);
    }

    bool operator==(const SBlobLocator& k2) const
    {
        return m_CacheRoot->GetPath() == k2.m_CacheRoot->GetPath()
            && m_ChunkId == k2.m_ChunkId && m_Offset == k2.m_Offset;
    }

    CSeq_id_Handle          m_Idh;
    const CDir *            m_CacheRoot;
    CAsnIndex::TChunkId     m_ChunkId;
    CAsnIndex::TOffset      m_Offset;
};

struct SSubcacheIndexData
{
    SSubcacheIndexData()
    : m_Gi(0)
    , m_Timestamp(0)
    , m_BlobSize(0)
    {}

    SSubcacheIndexData &operator=(const CAsnIndex &main_index)
    {
        m_Gi = main_index.GetGi();
        m_Timestamp = main_index.GetTimestamp();
        m_BlobSize = main_index.GetSize();
        m_SeqLength = main_index.GetSeqLength();
        m_TaxId = main_index.GetTaxId();
        return *this;
    }

    operator bool() const
    { return m_BlobSize; }

    vector<CSeq_id_Handle> m_Ids;
    CAsnIndex::TGi      m_Gi;
    CAsnIndex::TTimestamp   m_Timestamp;

    CAsnIndex::TChunkId     m_ChunkId;
    CAsnIndex::TOffset      m_Offset;
    CAsnIndex::TSize        m_BlobSize;
    CAsnIndex::TOffset      m_SeqIdOffset;
    CAsnIndex::TSize        m_SeqIdSize;
    CAsnIndex::TSeqLength   m_SeqLength;
    CAsnIndex::TTaxId       m_TaxId;
};

typedef deque<SSubcacheIndexData> TBlobLocationList;
typedef SSubcacheIndexData *TBlobLocationEntry;
typedef map<SSeqIdIndex, TBlobLocationEntry> TIndexMapById;
typedef TIndexMapById::iterator TIndexRef;
typedef list<TIndexRef> TIndexRefList;
typedef vector< pair<SBlobLocator, TBlobLocationEntry> > TIndexMapByBlob;
typedef set<CSeq_id_Handle, CSeq_id_Handle::PLessOrdered>  TCachedSeqIds;


void ExtractExtraIds(CBioseq_Handle         bsh,
                     vector<CSeq_id_Handle>& extra_ids,
                     bool                   extract_delta,
                     bool                   extract_products)
{
    ///
    /// process any delta-seqs
    ///
    if (extract_delta  &&
        bsh.GetInst().IsSetExt()  &&
        bsh.GetInst().GetExt().IsDelta()) {
        ITERATE (CBioseq::TInst::TExt::TDelta::Tdata, iter,
                 bsh.GetInst().GetExt().GetDelta().Get()) {
            const CDelta_seq& seg = **iter;
            CTypeConstIterator<CSeq_id> id_iter(seg);
            for ( ;  id_iter;  ++id_iter) {
                extra_ids.push_back
                    (CSeq_id_Handle::GetHandle(*id_iter));
            }
        }
    }

    ///
    /// extract products
    ///
    if (extract_products) {
        for (CFeat_CI feat_iter(bsh); feat_iter; ++feat_iter) {
            if (feat_iter->IsSetProduct()) {
                CTypeConstIterator<CSeq_id> id_iter
                    (feat_iter->GetProduct());
                for ( ;  id_iter;  ++id_iter) {
                    extra_ids.push_back
                        (CSeq_id_Handle::GetHandle(*id_iter));
                }
            }
        }
    }
}

static bool s_TrimLargeNucprots = false;
static bool s_RemoveAnnot = false;

bool s_RemoveAnnotsFromEntry(CSeq_entry &entry)
{
    if (entry.IsSeq()) {
        bool removed_annot = entry.GetSeq().IsSetAnnot();
        entry.SetSeq().ResetAnnot();
        return removed_annot;
    } else {
        bool removed_annots = entry.GetSet().IsSetAnnot();
        entry.SetSet().ResetAnnot();
        NON_CONST_ITERATE (CBioseq_set::TSeq_set, seq_it,
                           entry.SetSet().SetSeq_set())
        {
            removed_annots |= s_RemoveAnnotsFromEntry(**seq_it);
        }
        return removed_annots;
    }
}

/// If entry is a large nucprot set, Optionally create a new trimmed Seq-entry
/// containing only the needed Bioseq. Also optionally remove all Seq-annots.
bool TrimEntry(CConstRef<CSeq_entry> &entry, CBioseq_Handle bsh)
{
  bool trim_large_nucprot = false;
  bool trimmed = false;

  CSeq_entry *full_entry = const_cast<CSeq_entry *>(entry.GetPointer());
  CConstRef<CBioseq> genomic_seq;
  if (s_TrimLargeNucprots &&
      entry->IsSet() &&
      entry->GetSet().GetClass() == CBioseq_set::eClass_nuc_prot &&
      !entry->GetSet().GetSeq_set().empty() &&
      entry->GetSet().GetSeq_set().front()->IsSeq())
  {
    /// This is a nuc-prot set. It is a large nucprot set if first Bioseq
    /// is genomic
    genomic_seq.Reset(&entry->GetSet().GetSeq_set().front()->GetSeq());
    ITERATE (CSeq_descr::Tdata, desc_it, genomic_seq->GetDescr().Get()) {
        if ((*desc_it)->IsMolinfo()) {
            trim_large_nucprot = (*desc_it)->GetMolinfo().GetBiomol() ==
                                   CMolInfo::eBiomol_genomic;
            break;
        }
    }
  }

  if (trim_large_nucprot) {
    /// This is a large nucprot set; create a trimmed Seq-entry
    CRef<CSeq_entry> child_to_include(const_cast<CSeq_entry *>(
        bsh.GetSeq_entry_Handle().GetCompleteSeq_entry().GetPointer()));
    CSeq_entry *trimmed_entry = new CSeq_entry;
    trimmed_entry->SetSet().SetClass(CBioseq_set::eClass_nuc_prot);
    if (full_entry->GetSet().IsSetId()) {
        trimmed_entry->SetSet().SetId(full_entry->SetSet().SetId());
    }
    if (full_entry->GetSet().IsSetColl()) {
        trimmed_entry->SetSet().SetColl(full_entry->SetSet().SetColl());
    }
    if (full_entry->GetSet().IsSetLevel()) {
        trimmed_entry->SetSet().SetLevel(full_entry->GetSet().GetLevel());
    }
    if (full_entry->GetSet().IsSetRelease()) {
        trimmed_entry->SetSet().SetRelease(full_entry->GetSet().GetRelease());
    }
    if (full_entry->GetSet().IsSetDate()) {
        trimmed_entry->SetSet().SetDate(full_entry->SetSet().SetDate());
    }
    if (full_entry->GetSet().IsSetDescr()) {
        trimmed_entry->SetSet().SetDescr(full_entry->SetSet().SetDescr());
    }
    // Include Bioseq-set-level annot only if the Bioseq requested
    /// is the genomic one
    if (full_entry->GetSet().IsSetAnnot() &&
        genomic_seq.GetPointer() == &child_to_include->GetSeq())
    {
        trimmed_entry->SetSet().SetAnnot().insert(
            trimmed_entry->SetSet().SetAnnot().end(),
            full_entry->SetSet().SetAnnot().begin(),
            full_entry->SetSet().SetAnnot().end());
    }
    trimmed_entry->SetSet().SetSeq_set().push_back(child_to_include);
    entry.Reset(full_entry = trimmed_entry);
    trimmed = true;
  }
  if (s_RemoveAnnot) {
    trimmed |= s_RemoveAnnotsFromEntry(*full_entry);
  }
  return trimmed;
}


struct SBlobCopier
{
    SBlobCopier(const CDir& subcache_root,
                bool        extract_delta,
                bool        extract_product)
        : m_BlobCount(0), m_BioseqCount(0), m_SeqIdCount(0),
          m_Scope(*CObjectManager::GetInstance()),
          m_SubcacheRoot( subcache_root ),
          m_LastBlob(NULL),
          m_ExtractDelta(extract_delta),
          m_ExtractProducts(extract_product)
    {
        m_Buffer.reserve(128 * 1024 * 1024);
    }

    void operator() (const SBlobLocator & main_cache_locator,
                     SSubcacheIndexData &sub_cache_locator)
    {
        if (!sub_cache_locator) {
            /// Blob has been invalidated (because it already exists in
            /// sub-cache)
            return;
        }

        if (CSignal::IsSignaled()) {
            NCBI_THROW(CException, eUnknown,
                       "trapped signal, exiting");
        }

        CBioseq_Handle bsh;
        if (m_LastBlob && *m_LastBlob == main_cache_locator) {
            /// This is the same blob we copied on the last call (this can
            /// happen if several bioseqs belong to the same seq-entry
            bsh = m_Scope.GetBioseqHandle(main_cache_locator.m_Idh);
            if (m_CurrentNucprotSeqEntry) {
                /// Current blob, containing the previous id and this one, is a
                /// large Nucprot; create a new trimmed entry with this new id
                CConstRef<CSeq_entry> trimmed_entry = m_CurrentNucprotSeqEntry;
                TrimEntry(trimmed_entry, bsh);

                m_OutputChunk.OpenForWrite( m_SubcacheRoot.GetPath() );
                sub_cache_locator.m_ChunkId = m_OutputChunk.GetChunkSerialNum();
                m_LastBlobOffset = sub_cache_locator.m_Offset
                                 = m_OutputChunk.GetOffset();

                CCache_blob small_blob;
                small_blob.SetTimestamp( m_LastBlobTimestamp );
                small_blob.Pack(*trimmed_entry);
                m_OutputChunk.Write(small_blob);
                sub_cache_locator.m_BlobSize = m_OutputChunk.GetOffset()
                                             - sub_cache_locator.m_Offset;
         
            } else {
                sub_cache_locator.m_ChunkId = m_OutputChunk.GetChunkSerialNum();
                sub_cache_locator.m_Offset = m_LastBlobOffset;
            }
        } else {
            m_Buffer.clear();
            m_Buffer.resize( sub_cache_locator.m_BlobSize );

            m_InputChunk.OpenForRead( main_cache_locator.m_CacheRoot->GetPath(),
                                      main_cache_locator.m_ChunkId );
            m_InputChunk.RawRead( main_cache_locator.m_Offset,
                                  &m_Buffer[0], m_Buffer.size());

            m_OutputChunk.OpenForWrite( m_SubcacheRoot.GetPath() );
            sub_cache_locator.m_ChunkId = m_OutputChunk.GetChunkSerialNum();
            m_LastBlobOffset = sub_cache_locator.m_Offset = m_OutputChunk.GetOffset();
            m_LastBlob = &main_cache_locator;

            CRef<CSeq_entry> entry(new CSeq_entry);
            CCache_blob blob;
            CNcbiIstrstream istr(&m_Buffer[0], m_Buffer.size());
            istr >> MSerial_AsnBinary >> blob;
            blob.UnPack(*entry);
            m_LastBlobTimestamp = blob.GetTimestamp();

            CConstRef<CSeq_entry> trimmed_entry = entry;
            m_Scope.ResetDataAndHistory();
            m_Scope.AddTopLevelSeqEntry(*entry);
            bsh = m_Scope.GetBioseqHandle(main_cache_locator.m_Idh);
            if (TrimEntry(trimmed_entry, bsh)) {
                /// Seq-entry was trimmed, so we need to create and write
                /// new smaller blob
                m_CurrentNucprotSeqEntry = entry;
                CCache_blob small_blob;
                small_blob.SetTimestamp( m_LastBlobTimestamp );
                small_blob.Pack(*trimmed_entry);
                m_OutputChunk.Write(small_blob);
                sub_cache_locator.m_BlobSize = m_OutputChunk.GetOffset()
                                             - sub_cache_locator.m_Offset;
            } else {
                m_CurrentNucprotSeqEntry.Reset();
                m_OutputChunk.RawWrite( &m_Buffer[0], m_Buffer.size());
            }

            ++m_BlobCount;
        }

        ++m_BioseqCount;
        m_SeqIdCount += bsh.GetId().size();
        sub_cache_locator.m_Ids = bsh.GetId();

        /// Write and index bioseq's seqids
        m_SeqIdChunk.OpenForWrite( m_SubcacheRoot.GetPath() );
        sub_cache_locator.m_SeqIdOffset = m_SeqIdChunk.GetOffset();
        m_SeqIdChunk.Write(bsh.GetId());
        sub_cache_locator.m_SeqIdSize = m_SeqIdChunk.GetOffset()
            - sub_cache_locator.m_SeqIdOffset;

        ExtractExtraIds(bsh, extra_ids,
                        m_ExtractDelta, m_ExtractProducts);
    }

public:
    size_t m_BlobCount;
    size_t m_BioseqCount;
    size_t m_SeqIdCount;
    vector<CSeq_id_Handle> extra_ids;

private:
    CScope       m_Scope;
    const CDir&  m_SubcacheRoot;
    const SBlobLocator* m_LastBlob;
    CAsnIndex::TOffset m_LastBlobOffset;
    int m_LastBlobTimestamp;
    CRef<CSeq_entry> m_CurrentNucprotSeqEntry;
    
    CChunkFile   m_InputChunk;
    CChunkFile   m_OutputChunk;
    CSeqIdChunkFile  m_SeqIdChunk;
    vector<char> m_Buffer;
    bool         m_ExtractDelta;
    bool         m_ExtractProducts;
};



struct SBlobInserter
{
    SBlobInserter(const CDir& subcache_root,
                  bool        extract_delta,
                  bool        extract_product)
        : m_BlobCount(0), m_BioseqCount(0), m_SeqIdCount(0),
          m_SubcacheRoot( subcache_root ),
          m_ExtractDelta(extract_delta),
          m_ExtractProducts(extract_product)
    {
        m_Timestamp = CTime(CTime::eCurrent).GetTimeT();
    }

    void operator() (CBioseq_Handle bsh,
                     vector<SSubcacheIndexData> &sub_cache_locator)
    {
        if (CSignal::IsSignaled()) {
            NCBI_THROW(CException, eUnknown,
                       "trapped signal, exiting");
        }

        CSeq_entry_Handle seh;
        CConstRef<CSeq_entry> entry;
        try {
            seh = bsh.GetTopLevelEntry();
            entry = seh.GetCompleteSeq_entry();
        } catch (...) {
            LOG_POST(Error << "Error fetching " << bsh.GetSeq_id_Handle().AsString());
            throw;
        }
        TrimEntry(entry, bsh);
        SSubcacheIndexData blob_locator;
        {{
             CCache_blob blob;
             blob.SetTimestamp( m_Timestamp );
             blob.Pack(*entry);
             m_OutputChunk.OpenForWrite( m_SubcacheRoot.GetPath() );
             blob_locator.m_Timestamp = m_Timestamp;
             blob_locator.m_ChunkId = m_OutputChunk.GetChunkSerialNum();
             blob_locator.m_Offset = m_OutputChunk.GetOffset();
             m_OutputChunk.Write(blob);
             blob_locator.m_BlobSize = m_OutputChunk.GetOffset()
                                     - blob_locator.m_Offset;
        }}
        ++m_BlobCount;

        vector<CBioseq_Handle> relevant_seqs;
        if (entry->IsSeq() || entry->GetSet().GetSeq_set().size() == 1) {
            relevant_seqs.push_back(bsh);
        } else {
            /// Blob written has multiple Bioseqs
            for (CBioseq_CI seq_ci(seh); seq_ci; ++seq_ci) {
                relevant_seqs.push_back(*seq_ci);
            }
        }
        ITERATE (vector<CBioseq_Handle>, seq_it, relevant_seqs) {
            CConstRef<CBioseq> bioseq = seq_it->GetCompleteBioseq();
            BioseqIndexData(*bioseq, blob_locator.m_Gi,
                                     blob_locator.m_SeqLength,
                                     blob_locator.m_TaxId);

            ++m_BioseqCount;
            m_SeqIdCount += seq_it->GetId().size();
            blob_locator.m_Ids = seq_it->GetId();

            /// Write and index bioseq's seqids
            m_SeqIdChunk.OpenForWrite( m_SubcacheRoot.GetPath() );
            blob_locator.m_SeqIdOffset = m_SeqIdChunk.GetOffset();
            m_SeqIdChunk.Write(seq_it->GetId());
            blob_locator.m_SeqIdSize = m_SeqIdChunk.GetOffset()
                                     - blob_locator.m_SeqIdOffset;
            sub_cache_locator.push_back(blob_locator);

            ExtractExtraIds(*seq_it, extra_ids,
                            m_ExtractDelta, m_ExtractProducts);
        }
    }

public:
    size_t m_BlobCount;
    size_t m_BioseqCount;
    size_t m_SeqIdCount;
    vector<CSeq_id_Handle> extra_ids;

private:
    const CDir&  m_SubcacheRoot;
    CChunkFile   m_OutputChunk;
    CSeqIdChunkFile   m_SeqIdChunk;
    vector<char> m_Buffer;
    bool         m_ExtractDelta;
    bool         m_ExtractProducts;
    time_t       m_Timestamp;
};



/////////////////////////////////////////////////////////////////////////////
//  CAsnCacheApplication::


class CAsnCacheApplication : public CNcbiApplication
{
public:
    CAsnCacheApplication()
        : m_TotalRecords(0),
          m_RecordsNotInMainCache(0),
          m_RecordsInSubCache(0),
          m_RecordsNotFound(0),
          m_RecordsWithdrawn(0),
          m_MaxRecursionLevel(kMax_Int)
    {
    }

private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    size_t    WriteBlobsInSubCache( const vector<CDir>& main_cache_roots,
                             const CDir& sub_cache_root,
                             TIndexMapById& index_map,
                             TBlobLocationList& blob_locations,
                             time_t      timestamp,
                             bool        extract_delta,
                             bool        extract_product,
                             bool        fetch_missing,
                             bool        update_existing,
                             int         recursion_level);

    void    x_FetchMissingBlobs( TIndexMapById& index_map,
                                 const TIndexRefList& missing_ids,
                                 TBlobLocationList& blob_locations,
                                 TIndexMapById&      extra_ids,
                                 const CDir &    subcache_root,
                                 bool            extract_delta,
                                 bool            extract_product );

    void x_LocateBlobsInCache(TIndexMapById& index_map,
                              TIndexMapByBlob& index_map_by_blob,
                              const vector<CDir>& main_cache_roots,
                              TBlobLocationList& blob_locations,
                              TIndexRefList& missing_ids,
                              time_t timestamp);

    void x_EliminateIdsAlreadyInCache(TIndexMapById& index_map,
                                      const string &    cache_index);

    void IndexNewBlobsInSubCache(const TIndexMapById& index_map,
                                 const CDir &    cache_root);

    size_t m_TotalRecords;
    size_t m_RecordsNotInMainCache;
    size_t m_RecordsInSubCache;
    size_t m_RecordsNotFound;
    size_t m_RecordsWithdrawn;
    int    m_MaxRecursionLevel;

    CStopWatch  m_Stopwatch;

    SSubcacheIndexData m_BlankIndexData;
    TCachedSeqIds m_cached_seq_ids;
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CAsnCacheApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddKey("cache", "Cache",
                     "Comma-separated paths of one or more main caches",
                     CArgDescriptions::eString);

    arg_desc->AddKey("subcache", "Subcache",
                     "Path to the ASN.1 subcache that will be created.",
                     CArgDescriptions::eString);

    arg_desc->AddDefaultKey( "i", "SeqIds",
                             "The list of Seq-ids is read from here.",
                             CArgDescriptions::eInputFile,
                             "-");
    arg_desc->AddAlias("-input", "i");

    arg_desc->AddOptionalKey( "input-manifest", "SeqIds",
                              "The list of Seq-ids is read from here.",
                              CArgDescriptions::eInputFile);
    arg_desc->SetDependency("i",
                            CArgDescriptions::eExcludes,
                            "input-manifest");

    arg_desc->AddOptionalKey( "timestamp", "Timestamp",
                              "Only GIs stamped earlier than this timestamp (YYYY-MM-DD) are cached",
                              CArgDescriptions::eString);

    arg_desc->AddFlag("extract-delta",
                      "Extract and index delta-seq far-pointers");

    arg_desc->AddOptionalKey("delta-level", "RecursionLevel",
                             "Number of levels to descend when retrieving "
                             "items in delta sequences",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("max-retrieval-failures", "MaximumAllowedFailures",
                             "Maximum number of sequences we're allowed to "
                             "fail to retrieve from ID and still consider "
                             "execution a success; does not include withdrawn "
                             "sequences, which are counted separately",
                             CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("max-withdrawn", "MaximumWithdrawnSequences",
                             "Maximum number of withdrawn sequences allowed in "
                             "the input Seq-ids",
                             CArgDescriptions::eInteger);

    arg_desc->AddFlag("extract-product",
                      "Extract and index product far-pointers");

    arg_desc->AddFlag("fetch-missing",
                      "Retrieve ASN.1 blobs from ID directly if a look-up in "
                      "the main cache fails");

    arg_desc->AddFlag("no-update-existing",
                      "Don't update sequences that are already in the subcache");

    arg_desc->AddFlag("trim-large-nucprots",
                      "Divide large nucprots into separate Seq-entry per "
                      "sequences, to avoid fetching huge blobs when only one "
                      "protein is needed");

    arg_desc->AddFlag("remove-annotation",
                      "Remove all annotation from caches entries");

    arg_desc->SetDependency("max-retrieval-failures",
                            CArgDescriptions::eRequires, "fetch-missing");
    arg_desc->SetDependency("max-withdrawn",
                            CArgDescriptions::eRequires, "fetch-missing");
                            
    arg_desc->AddOptionalKey("oseqids","oseqids","Seqids that actually made it to the cache", 
        CArgDescriptions::eOutputFile, CArgDescriptions::fPreOpen);

    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


static void s_ReadIdsFromFile(CNcbiIstream&  istr,
                              TIndexMapById& index_map,
                              TBlobLocationList& blob_locations,
                              TCachedSeqIds& cached_seq_ids
                              )
{
    string line;
    while ( NcbiGetlineEOL( istr, line ) ) {
        if ( line.empty()  ||  line[0] == '#') {
            continue;
        }

        try {
            /// Put id in index map; blob location points to blank data
            /// until we look up the data
            index_map.insert(TIndexMapById::value_type
                             (CSeq_id_Handle::GetHandle(line),
                              &blob_locations.front()));
            cached_seq_ids.insert(CSeq_id_Handle::GetHandle(line));
        }
        catch (CException&) {
            LOG_POST(Error << "ignoring invalid line: " << line);
        }
    }
}



int CAsnCacheApplication::Run(void)
{
    const CArgs& args = GetArgs();
    {{
         GetDiagContext().GetRequestContext().GetRequestTimer().Start();
         CDiagContext_Extra extra = GetDiagContext().PrintRequestStart();
     }}

    GetConfig().Set("OBJMGR", "BLOB_CACHE", "500");

    s_TrimLargeNucprots = args["trim-large-nucprots"];
    s_RemoveAnnot = args["remove-annotation"];

    vector<CDir> main_cache_roots;
    vector<string> main_cache_paths;
    NStr::Split(args["cache"].AsString(), ",", main_cache_paths);
    ITERATE (vector<string>, it, main_cache_paths) {
        CDir cache_root(CDirEntry::NormalizePath(*it, eFollowLinks));
        if (! cache_root.Exists() ) {
            LOG_POST( Error << "Cache " << cache_root.GetPath()
                      << " does not exist!" );
            return 1;
        } else if ( ! cache_root.IsDir() ) {
            LOG_POST( Error << cache_root.GetPath() << " does not point to a "
                      << "valid cache path!" );
            return 2;
        }
        main_cache_roots.push_back(cache_root);
    }

    CDir    subcache_root( args["subcache"].AsString() );
    if ( subcache_root.Exists() ) {
        LOG_POST( Warning << "Subcache " << subcache_root.GetPath()
                  << " already exists!" );
    } else if ( ! subcache_root.CreatePath() ) {
        LOG_POST( Error << "Unable to create a path to a subcache at "
                  << subcache_root.GetPath() );
    }

    CTime timestamp( CTime::eCurrent );
    if ( args["timestamp"].HasValue() ) {
        string timestamp_string( args["timestamp"].AsString() );
        timestamp = CTime( timestamp_string,
                           CTimeFormat::GetPredefined( CTimeFormat::eISO8601_Date ) );
        LOG_POST( Info << "Timestamp: " << timestamp.AsString() );
    }

    bool extract_delta   = args["extract-delta"];
    bool extract_product = args["extract-product"];
    bool fetch_missing   = args["fetch-missing"];
    bool update_existing = !args["no-update-existing"];
    LOG_POST(Error << "update existing = "
             << (update_existing ? "true" : "false"));

    unsigned max_retrieval_failures =
         args["max-retrieval-failures"]
             ? args["max-retrieval-failures"].AsInteger() : UINT_MAX;

    unsigned max_withdrawn =
         args["max-withdrawn"]
             ? args["max-withdrawn"].AsInteger() : UINT_MAX;

    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CDataLoadersUtil::SetupObjectManager(args, *om);

    CStopWatch  sw;
    sw.Start();

    ///
    /// read our list of IDs
    ///
    TBlobLocationList blob_locations;
    blob_locations.push_back(m_BlankIndexData);
    TIndexMapById index_map;
    {{
         if (args["input-manifest"]) {
             string fname;
             CNcbiIstream& mft_istr = args["input-manifest"].AsInputFile();
             while (NcbiGetlineEOL(mft_istr, fname)) {
                 NStr::TruncateSpacesInPlace(fname);
                 if ( fname.empty()  ||  fname[0] == '#') {
                     continue;
                 }

                 CNcbiIfstream istr(fname.c_str());
                 s_ReadIdsFromFile(istr, index_map, blob_locations, m_cached_seq_ids);
             }
         }
         else {
             CNcbiIstream& istr = args["i"].AsInputFile();
             s_ReadIdsFromFile(istr, index_map, blob_locations, m_cached_seq_ids);
         }
         LOG_POST( Error << index_map.size() << " IDs read from the bag." );
    }}


    if (args["delta-level"].HasValue()) {
        m_MaxRecursionLevel = args["delta-level"].AsInteger();
    }

    size_t  total_count =
        WriteBlobsInSubCache( main_cache_roots, subcache_root, index_map,
                       blob_locations, timestamp.GetTimeT(),
                       extract_delta, extract_product, fetch_missing,
                       update_existing, 0 );

    IndexNewBlobsInSubCache(index_map, subcache_root);

    double e = sw.Elapsed();
    LOG_POST( Error << "done: copied "
              << total_count << " items into cache ("
              << e << " seconds, " << total_count/e << " items/sec)");
    LOG_POST(Error << "total records requested:      " << m_TotalRecords);
    LOG_POST(Error << "total records not found:      " << m_RecordsNotInMainCache);
    LOG_POST(Error << "total records already cached: " << m_RecordsInSubCache);
                size_t total_mem = 0;
                size_t resident_mem = 0;
                size_t shared_mem = 0;
                GetMemoryUsage(&total_mem,
                                   &resident_mem,
                                   &shared_mem);
    LOG_POST(Error << "total memory consumed: " << total_mem);
    if (fetch_missing) {
        LOG_POST(Error << "total record retrieval failures: " << m_RecordsNotFound);
        LOG_POST(Error << "total records withdrawn:         " << m_RecordsWithdrawn);
        if (m_RecordsNotFound > max_retrieval_failures) {
            return 3;
        } else if (m_RecordsWithdrawn > max_withdrawn) {
            return 4;
        }
    }
    
    if(args["oseqids"]) { 
        args["oseqids"].AsOutputFile() <<"#seq-id"<<endl;
        ITERATE (TCachedSeqIds, it, m_cached_seq_ids) {
            args["oseqids"].AsOutputFile() << *it << endl;
        }
    }

    GetDiagContext().GetRequestContext().SetRequestStatus(200);
    GetDiagContext().PrintRequestStop();

    return 0;
}


size_t CAsnCacheApplication::WriteBlobsInSubCache(const vector<CDir>& main_cache_roots,
                                                  const CDir& subcache_root,
                                                  TIndexMapById& index_map,
                                                  TBlobLocationList& blob_locations,
                                                  time_t      timestamp,
                                                  bool        extract_delta,
                                                  bool        extract_product,
                                                  bool        fetch_missing,
                                                  bool        update_existing,
                                                  int         recursion_level)
{
    if (recursion_level >= m_MaxRecursionLevel) {
        return 0;
    }

    m_Stopwatch.Start();

    string subcache_main_index =
        NASNCacheFileName::GetBDBIndex(subcache_root.GetPath(),
                                       CAsnIndex::e_main);

    size_t input_ids = index_map.size();

    ///
    /// step 1: if update_existing is false, eliminate blobs that are already
    ///         in the subcache
    ///
    if (!update_existing) {
        /// At this point all ids are pointing to blob_locations.begin(), which is the blank
        /// blob locator; give it a timestamp of 0, so any entry that's already in the
        /// subcache will be considered up-to-date
        blob_locations.begin()->m_Timestamp = 0;
        x_EliminateIdsAlreadyInCache(index_map, subcache_main_index);
    }

    m_TotalRecords += index_map.size();
    ///
    /// step 2: locate blobs in main cache
    ///
    TIndexRefList ids_missing;
    TIndexMapByBlob index_by_blob;
    x_LocateBlobsInCache(index_map, index_by_blob, main_cache_roots, blob_locations,
                         ids_missing, timestamp);
    size_t found = m_TotalRecords - m_RecordsNotInMainCache;

    LOG_POST(Error << input_ids
             << " unique source records in " << m_Stopwatch.Elapsed()
             << " seconds." );

    ///
    /// step 3: if update-existing is true, eliminate blobs that are already
    ///         up-to-date in cache
    ///
    if (update_existing) {
        /// At this point ids still pointing to blob_locations.begin(), the
        /// blank blob locator, are those for which no blob was found in the
        /// main cache.  for all of them we have to assume that the entry in
        /// genbank is more up-to-date and needs to be fetched; give it the
        /// most up-to-date timestamp, so blobs not found in the main cache
        /// will not be eliminated
        blob_locations.begin()->m_Timestamp = timestamp;
        x_EliminateIdsAlreadyInCache(index_map, subcache_main_index);
    }
    LOG_POST(Error << index_map.size() << " new records in "
                   << m_Stopwatch.Elapsed() << " seconds." );



    /// If we're at recursion level 0, then this is the point just before we
    /// start writing to the cache; don't allow interruptions from now on
    if (recursion_level == 0) {
        CSignal::TrapSignals(CSignal::eSignal_HUP |
                             CSignal::eSignal_QUIT |
                             CSignal::eSignal_INT |
                             CSignal::eSignal_TERM);
    }

    ///
    /// determine if we need to fetch any of the missing sequences
    ///
    TIndexMapById extra_ids;
    if (fetch_missing) {
        x_FetchMissingBlobs(index_map, ids_missing, blob_locations, extra_ids,
                            subcache_root,
                            extract_delta, extract_product );
    } else {
        /// We're not going to fetch the missing ids, so erase them from map
        ITERATE (TIndexRefList, it, ids_missing) {
            index_map.erase(*it);
        }
    }

    ///
    /// now, write these blobs
    /// while writing, we also extract indexable information
    ///
    {{
        SBlobCopier blob_writer(subcache_root,
                                 extract_delta, extract_product);

         ITERATE (TIndexMapByBlob, it, index_by_blob) {
           try {
             blob_writer(it->first, *it->second);
             /// Add all biodeq's other ids to index map, pointing to same blob
             ITERATE (vector<CSeq_id_Handle>, id_it, it->second->m_Ids) {
                 if (*id_it != it->first.m_Idh) {
                     index_map.insert(TIndexMapById::value_type(*id_it, it->second));
                 }
             }
           } catch (...) {
             LOG_POST(Error << "Error trying to copy " << it->first.m_Idh.AsString());
             throw;
           }
         }

         LOG_POST(Error << "copied "
                  << blob_writer.m_BlobCount << " blobs / "
                  << blob_writer.m_BioseqCount << " sequences / "
                  << blob_writer.m_SeqIdCount << " identifiers");

         ITERATE (vector<CSeq_id_Handle>, it, blob_writer.extra_ids) {
             SSeqIdIndex new_id(*it);
             if (!index_map.count(new_id)) {
                 extra_ids.insert(TIndexMapById::value_type(new_id, &blob_locations.front()));
             }
         }

         LOG_POST( Error << found
                   << " records found in " << m_Stopwatch.Elapsed() << " seconds." );
    }}

    if (extra_ids.size() && recursion_level++ < m_MaxRecursionLevel) {
        found += WriteBlobsInSubCache(main_cache_roots, subcache_root,
                               extra_ids, blob_locations, timestamp,
                               extract_delta, extract_product, fetch_missing,
                               update_existing, recursion_level);
        index_map.insert(extra_ids.begin(), extra_ids.end());
    }

    return found;
}



/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CAsnCacheApplication::Exit(void)
{
    SetDiagStream(0);
}



void
CAsnCacheApplication::x_FetchMissingBlobs(TIndexMapById& index_map,
                                 const TIndexRefList& ids_missing,
                                 TBlobLocationList& blob_locations,
                                 TIndexMapById&      extra_ids,
                                 const CDir &    subcache_root,
                                 bool            extract_delta,
                                 bool            extract_product )
{
    if (! ids_missing.empty()) {
        SBlobInserter inserter(subcache_root,
                               extract_delta, extract_product);
        CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
        scope->AddDefaults();
        ITERATE (TIndexRefList, it, ids_missing) {

            if (CSignal::IsSignaled()) {
                NCBI_THROW(CException, eUnknown,
                           "trapped signal, exiting");
            }

            if (*(*it)->second) {
                /// Already fetched a blob for this id
               continue;
            }

            CBioseq_Handle bsh;
            bool is_withdrawn = false;
            try {
                bsh = scope->GetBioseqHandle((*it)->first.m_Idh);
                if ( !bsh ) {
                    is_withdrawn =
                        bsh.GetState() & CBioseq_Handle::fState_withdrawn;
                    NCBI_THROW(CException, eUnknown,
                               is_withdrawn ? "bioseq withdrawn"
                                            : "empty bioseq handle");
                }
            }
            catch (CException& e) {
                LOG_POST(Error << "failed to retrieve sequence: "
                         << (*it)->first.m_Idh.AsString()
                         << ": " << e);
                {{
                    TCachedSeqIds::const_iterator idh
                        = m_cached_seq_ids.find((*it)->first.m_Idh);
                    if(idh != m_cached_seq_ids.end()) {
                        m_cached_seq_ids.erase(idh);
                    }
                }}
                index_map.erase(*it);
                ++ (is_withdrawn ? m_RecordsWithdrawn : m_RecordsNotFound);
                continue;
            }
            vector<SSubcacheIndexData> index_data;
            inserter(bsh, index_data);

            ITERATE (vector<SSubcacheIndexData>, blob_it, index_data) {
                /// Insert index data in blob locations list and in index map
                blob_locations.push_back(*blob_it);

                /// Add all biodeq's ids to index map, pointing to same blob
                ITERATE (vector<CSeq_id_Handle>, id_it, blob_it->m_Ids) {
                    index_map[*id_it] = &blob_locations.back();;
                }
            }
        }

        ITERATE (vector<CSeq_id_Handle>, it, inserter.extra_ids) {
            SSeqIdIndex new_id(*it);
            if (!index_map.count(new_id)) {
                extra_ids.insert(TIndexMapById::value_type(new_id, &blob_locations.front()));
            }
        }
    }
}


static bool s_ShouldFetchOneByOne(TIndexMapById& ids)
{
    if (ids.size() <= 500) {
        return true;
    }
    else {
        string key1 = ids.begin()->first.m_SeqId;
        string key2 = (--ids.end())->first.m_SeqId;
        size_t half_length = max(key1.size(), key2.size())/2;
        return key1.substr(0, half_length) != key2.substr(0, half_length);
    }
}


void CAsnCacheApplication::x_LocateBlobsInCache(TIndexMapById& index_map,
                               TIndexMapByBlob& index_map_by_blob,
                               const vector<CDir>& main_cache_roots,
                               TBlobLocationList& blob_locations,
                               TIndexRefList& missing_ids,
                               time_t timestamp)
{
    LOG_POST(Error << "searching for " << index_map.size() << " ids");
    bool one_by_one = s_ShouldFetchOneByOne(index_map);

    NON_CONST_ITERATE (TIndexMapById, it, index_map) {
        missing_ids.push_back(it);
    }
    m_RecordsNotInMainCache += index_map.size();

    ITERATE (vector<CDir>, dir_it, main_cache_roots) {
      string cache_index =
        NASNCacheFileName::GetBDBIndex(dir_it->GetPath(), CAsnIndex::e_main);
      LOG_POST(Error << "locate blobs in " << cache_index << ", missing " << missing_ids.size());
      if (CFile(cache_index).Exists() && !missing_ids.empty()) {
        CAsnIndex asn_index(CAsnIndex::e_main);
        asn_index.SetCacheSize(1 * 1024 * 1024 * 1024);
        asn_index.Open(cache_index, CBDB_RawFile::eReadOnly);

        if(one_by_one) {
            LOG_POST(Error << "  retrieval: one-by-one");
            TIndexRefList::iterator iter = missing_ids.begin();
            while (iter != missing_ids.end()) {
                CBDB_FileCursor cursor(asn_index);
                cursor.SetCondition(CBDB_FileCursor::eGE, CBDB_FileCursor::eLE);
                const SSeqIdIndex &key = (*iter)->first;
                cursor.From << key.m_SeqId << key.m_Version;
                cursor.To   << key.m_SeqId << key.m_Version;
                SSubcacheIndexData sub_cache_index_data;
                SBlobLocator main_cache_locator(key.m_Idh, *dir_it);
                while (cursor.Fetch() == eBDB_Ok ) {
                    if (asn_index.GetTimestamp() > sub_cache_index_data.m_Timestamp &&
                        asn_index.GetTimestamp() <= timestamp)
                    {
                        /// current ASN index record should be saved
                        sub_cache_index_data = asn_index;
                        main_cache_locator = asn_index;
                    }
                }
                if (sub_cache_index_data) {
                    /// Found a blob in the main cache. Insert in blob_locations list, and enter
                    /// into two maps
                    blob_locations.push_back(sub_cache_index_data);
                    (*iter)->second = &blob_locations.back();
                    index_map_by_blob.push_back(
                        TIndexMapByBlob::value_type(main_cache_locator, &blob_locations.back()));
                    iter = missing_ids.erase(iter);
                    --m_RecordsNotInMainCache;
                } else {
                    ++iter;
                }
            }
        } else {
            LOG_POST(Error << "  retrieval: bulk");
            TIndexRefList::iterator iter = missing_ids.begin();
            CBDB_FileCursor cursor(asn_index);
            cursor.InitMultiFetch(256 * 1024);
            cursor.SetCondition(CBDB_FileCursor::eGE, CBDB_FileCursor::eLE);
            const SSeqIdIndex &start = (*iter)->first,
                              &end = (*--missing_ids.end())->first;
            cursor.From << start.m_SeqId << start.m_Version;
            cursor.To << end.m_SeqId << end.m_Version;

            LOG_POST(Error << "scan range: "
                     << start.m_SeqId << '.' << start.m_Version << " - "
                     << end.m_SeqId << '.' << end.m_Version);

            bool valid_index = cursor.Fetch() == eBDB_Ok;
            while (valid_index && iter != missing_ids.end()) {
                SSubcacheIndexData sub_cache_index_data;
                SBlobLocator main_cache_locator((*iter)->first.m_Idh, *dir_it);
                for (; valid_index && (*iter)->first >= asn_index;
                       valid_index = cursor.Fetch() == eBDB_Ok)
                {
                    if ((*iter)->first == asn_index &&
                        asn_index.GetTimestamp() > sub_cache_index_data.m_Timestamp &&
                        asn_index.GetTimestamp() <= timestamp)
                    {
                        /// current ASN index record should be saved
                        sub_cache_index_data = asn_index;
                        main_cache_locator = asn_index;
                    }
                }
                if (sub_cache_index_data) {
                    /// Found a blob in the main cache. Insert in blob_locations list, and enter
                    /// into two maps
                    blob_locations.push_back(sub_cache_index_data);
                    (*iter)->second = &blob_locations.back();
                    index_map_by_blob.push_back(
                        TIndexMapByBlob::value_type(main_cache_locator, &blob_locations.back()));
                    iter = missing_ids.erase(iter);
                    --m_RecordsNotInMainCache;
                } else {
                    ++iter;
                }
            }
        }
      }
    }

    sort(index_map_by_blob.begin(), index_map_by_blob.end());
}


void
CAsnCacheApplication::x_EliminateIdsAlreadyInCache(TIndexMapById& index_map,
                                                   const string& cache_index)
{
    if (CFile(cache_index).Exists()) {
        /// now, scan the index to see if we have what we expect
        CAsnIndex asn_index(CAsnIndex::e_main);
        asn_index.SetCacheSize(1 * 1024 * 1024 * 1024);
        asn_index.Open(cache_index, CBDB_RawFile::eReadWriteCreate);

        bool one_by_one = s_ShouldFetchOneByOne(index_map);
        vector<TIndexRef> ids_found;

        if(one_by_one) {
            LOG_POST(Error << "  retrieval: one-by-one");
            NON_CONST_ITERATE (TIndexMapById, iter, index_map) {
                CBDB_FileCursor cursor(asn_index);
                cursor.SetCondition(CBDB_FileCursor::eGE, CBDB_FileCursor::eLE);
                const SSeqIdIndex &key = iter->first;
                cursor.From << key.m_SeqId << key.m_Version;
                cursor.To   << key.m_SeqId << key.m_Version;
                bool found_match = false;
                while (!found_match && cursor.Fetch() == eBDB_Ok ) {
                    /// If update_existing is true, this is called after locating blobs in
                    /// main cache, and iter->second->m_Tmestamp will conatin timestamp from main
                    /// cache ; we consider an entry to already exist in sub-cache only if its timestamp
                    /// is up-to-date compared to main cache. If update_existing is false, this is called
                    /// before locating blobs in main cache, to iter->second->m_Timestamp is 0
                    if (asn_index.GetTimestamp() >= iter->second->m_Timestamp)
                    {
                        found_match = true;
                        ids_found.push_back(iter);
                    }
                }
            }
        } else {
            LOG_POST(Error << "  retrieval: bulk");
            CBDB_FileCursor cursor(asn_index);
        cursor.InitMultiFetch(256 * 1024);
        cursor.SetCondition(CBDB_FileCursor::eGE, CBDB_FileCursor::eLE);
            const SSeqIdIndex &start = index_map.begin()->first,
                              &end = (--index_map.end())->first;
            cursor.From << start.m_SeqId << start.m_Version;
            cursor.To << end.m_SeqId << end.m_Version;

            LOG_POST(Error << "scan range: "
                     << start.m_SeqId << '.' << start.m_Version << " - "
                     << end.m_SeqId << '.' << end.m_Version);

            bool valid_index = cursor.Fetch() == eBDB_Ok;
            NON_CONST_ITERATE (TIndexMapById, iter, index_map) {
                if (!valid_index) {
                    break;
                }
                bool found_match = false;
                for (; valid_index && iter->first >= asn_index;
                       valid_index = cursor.Fetch() == eBDB_Ok)
                {
                    if (iter->first == asn_index &&
                        asn_index.GetTimestamp() >= iter->second->m_Timestamp)
                    {
                        found_match = true;
                    }
                }
                if (found_match) {
                    ids_found.push_back(iter);
                }
            }
        }

        m_RecordsInSubCache += ids_found.size();
        ITERATE (vector<TIndexRef>, iter, ids_found) {
            /// We already have this blob in the sub-cache; invalidate index data, so
            /// it won't be copied
            *(*iter)->second = m_BlankIndexData;
            index_map.erase(*iter);
        }
    }
}

void CAsnCacheApplication::
IndexNewBlobsInSubCache(const TIndexMapById& index_map,
                        const CDir &    cache_root)
{
    string main_index_path =
        NASNCacheFileName::GetBDBIndex(cache_root.GetPath(),
                                       CAsnIndex::e_main);
    string seq_id_index_path =
        NASNCacheFileName::GetBDBIndex(cache_root.GetPath(),
                                       CAsnIndex::e_seq_id);

    CAsnIndex main_index(CAsnIndex::e_main);
    main_index.SetCacheSize(1 * 1024 * 1024 * 1024);
    main_index.Open(main_index_path,
                     CBDB_RawFile::eReadWriteCreate);

    CAsnIndex seq_id_index(CAsnIndex::e_seq_id);
    seq_id_index.SetCacheSize(1 * 1024 * 1024 * 1024);
    seq_id_index.Open(seq_id_index_path,
                       CBDB_RawFile::eReadWriteCreate);

    ITERATE (TIndexMapById, it, index_map) {
        main_index.SetSeqId( it->first.m_SeqId );
        main_index.SetVersion( it->first.m_Version );
        main_index.SetGi( it->second->m_Gi );
        main_index.SetTimestamp( it->second->m_Timestamp );
        main_index.SetChunkId( it->second->m_ChunkId );
        main_index.SetOffset( it->second->m_Offset );
        main_index.SetSize( it->second->m_BlobSize );
        main_index.SetSeqLength( it->second->m_SeqLength );
        main_index.SetTaxId( it->second->m_TaxId );
        if ( eBDB_Ok != main_index.UpdateInsert() ) {
            LOG_POST( Error << "Main index failed to index SeqId "
                        << it->first.m_SeqId );
        }

        seq_id_index.SetSeqId( it->first.m_SeqId );
        seq_id_index.SetVersion( it->first.m_Version );
        seq_id_index.SetGi( it->second->m_Gi );
        seq_id_index.SetTimestamp( it->second->m_Timestamp );
        seq_id_index.SetOffset( it->second->m_SeqIdOffset );
        seq_id_index.SetSize( it->second->m_SeqIdSize );
        if ( eBDB_Ok != seq_id_index.UpdateInsert() ) {
            LOG_POST( Error << "SeqId index failed to index SeqId "
                        << it->first.m_SeqId );
        }
    }
}




/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CAsnCacheApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
