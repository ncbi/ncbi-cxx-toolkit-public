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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbifile.hpp>
#include <db/bdb/bdb_cursor.hpp>

#include <serial/serial.hpp>
#include <serial/objistrasnb.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>

#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqloc/Seq_id_set.hpp>

#include <objtools/data_loaders/asn_cache/Cache_blob.hpp>
#include <objtools/data_loaders/asn_cache/chunk_file.hpp>
#include <objtools/data_loaders/asn_cache/seq_id_chunk_file.hpp>
#include <objtools/data_loaders/asn_cache/asn_index.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_util.hpp>
#include <objtools/data_loaders/asn_cache/file_names.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CAsnCache::CAsnCache(const string& db_path)
    : m_DbPath(db_path)
    , m_CurrChunkId(0)
{
    m_DbPath = CDirEntry::CreateAbsolutePath(m_DbPath);
    m_DbPath = CDirEntry::NormalizePath(m_DbPath, eFollowLinks);
    m_Index.reset(new CAsnIndex(CAsnIndex::e_main));
    m_Index->SetCacheSize(128 * 1024 * 1024);

    string main_fname =
        NASNCacheFileName::GetBDBIndex(db_path, CAsnIndex::e_main);
    if ( !CFile(main_fname).Exists() ) {
        NCBI_THROW(CException, eUnknown,
                   "cannot open ASN cache: failed to find file: " + main_fname);
    }

    m_Index->Open(main_fname, CBDB_RawFile::eReadOnly);

    string fname = NASNCacheFileName::GetBDBIndex(db_path, CAsnIndex::e_seq_id);
    if (CFile(fname).Exists()) {
        try {
            m_SeqIdIndex.reset(new CAsnIndex(CAsnIndex::e_seq_id));
            m_SeqIdIndex->SetCacheSize(128 * 1024 * 1024);
            m_SeqIdIndex->Open(fname, CBDB_RawFile::eReadOnly);
            m_SeqIdChunk.reset(new CSeqIdChunkFile);
            m_SeqIdChunk->OpenForRead( m_DbPath );
        }
        catch (CException& e) {
            ERR_POST(Error << "error opening seq-id cache: disabling: " << e);
            m_SeqIdIndex.reset();
            m_SeqIdChunk.reset();
        }
    }
}

CAsnCache::~CAsnCache()
{
}

bool CAsnCache::s_GetChunkAndOffset(const CSeq_id_Handle&   idh,
                                    CAsnIndex&              index,
                                    vector<CAsnIndex::SIndexInfo>&  info,
                                    bool                    multiple)
{
    bool    was_id_found = false;

    ///
    /// retrieve the correct flattened seq-id
    ///
    string seq_id;
    Uint4 version;
    GetNormalizedSeqId(idh, seq_id, version);
    // LOG_POST(Info << "scanning: " << seq_id << " | " << version);

    ///
    /// scan for the appropriate sequence
    ///
    CBDB_FileCursor cursor(index);
    cursor.SetCondition(CBDB_FileCursor::eGE, CBDB_FileCursor::eLE);
    cursor.From << seq_id << version;
    cursor.To   << seq_id;

    while (cursor.Fetch() == eBDB_Ok) {
        CAsnIndex::SIndexInfo current_info(index);

        if (current_info.seq_id != seq_id) {
            LOG_POST(Error << "error: bad seq-id");
            break;
        }

        bool should_report = (!version || version == current_info.version) &&
           (info.empty() || multiple ||
           ( !version &&
             /// versionless - choose best version and timestamp
             (info[0].version < current_info.version ||
              (info[0].version == current_info.version &&
               info[0].timestamp < current_info.timestamp)) ||
             /// version specified; choose best timestamp for this version
             (version && info[0].timestamp < current_info.timestamp)));
        if (should_report) {
            was_id_found = true;
            if (!multiple) {
                info.clear();
            }
            info.push_back(current_info);
        }
    }

    return  was_id_found;
}

bool CAsnCache::s_GetChunkAndOffset(const CSeq_id_Handle&   idh,
                                    CAsnIndex&              index,
                                    CAsnIndex::SIndexInfo&  info)
{
    vector<CAsnIndex::SIndexInfo> info_vector;
    if (!s_GetChunkAndOffset(idh, index, info_vector, false)) {
        return false;
    }
    info = info_vector[0];
    return true;
}

bool CAsnCache::GetIdInfo(const CSeq_id_Handle& idh,
                          CAsnIndex::TGi& this_gi,
                          time_t& this_timestamp)
{
    CAsnIndex::SIndexInfo info;
    ///
    /// It is generally better to get GI information out of the new SeqId
    /// index, because this is often followed by getting other SeqId info,
    /// so this way we're avoiding a page fault on the subsequent call.
    /// However, we need to check whether the cache is old-style, without
    /// a SeqId index, and in that case get the info out of the main index
    ///
    if ( s_GetChunkAndOffset(idh, m_SeqIdIndex.get() ? *m_SeqIdIndex : *m_Index,
                             info) )
    {
        this_gi = info.gi;
        this_timestamp = info.timestamp;
        return true;
    }

    return  false;
}

/// Get a partial index entry, returning only the externally useful
/// metadata.  This is a very fast call.
bool CAsnCache::GetIdInfo(const CSeq_id_Handle & id,
                          CSeq_id_Handle& accession,
                          CAsnIndex::TGi& gi,
                          time_t& timestamp,
                          Uint4& sequence_length,
                          Uint4& tax_id)
{
    CAsnIndex::SIndexInfo info;

    if ( !GetIndexEntry(id, info) ) {
        return false;
    }
    gi = info.gi;
    timestamp = info.timestamp;
    accession = CSeq_id_Handle::GetHandle(info.seq_id);
    sequence_length = info.sequence_length;
    tax_id = info.taxonomy_id;
    return true;
}

bool CAsnCache::GetSeqIds(const CSeq_id_Handle& id,
                          vector<CSeq_id_Handle>& all_ids,
                          bool cheap_only)
{
    bool    was_seqid_blob_found = false;

    CAsnIndex::SIndexInfo info;

    was_seqid_blob_found = m_SeqIdIndex.get() &&
        s_GetChunkAndOffset(id, *m_SeqIdIndex, info);
    _TRACE("GetSeqIds id=" << id.GetSeqId()->AsFastaString()
           << " gi=" << info.gi
           << " timestamp=" << info.timestamp
           << " offs=" << info.offs
           << " size=" << info.size);

    if ( was_seqid_blob_found ){
        try {
            m_SeqIdChunk->Read( all_ids, info.offs, info.size );
        }
        catch ( CException & e ) {
            ERR_POST( "Unable to read or unpack a SeqIds chunk."
                        << " offset = " << info.offs << " size = " << info.size );
            ERR_POST( "SeqId = " << id.AsString() << " gi = " << info.gi
                        << " timestamp = " << info.timestamp );
            ERR_POST( e );
            was_seqid_blob_found = false;
        }
    } else if(!cheap_only){
        ///
        /// Couldn't get IDs the cheap way; use the expensive way, by getting full entry
        ///
        CConstRef<CBioseq> bioseq = ExtractBioseq(GetEntry(id), id);
        if(bioseq.NotNull()){
            ITERATE(CBioseq::TId, it, bioseq->GetId())
                all_ids.push_back(CSeq_id_Handle::GetHandle(**it));
            was_seqid_blob_found = true;
        }
    }

    return  was_seqid_blob_found;
}

bool CAsnCache::GetBlob(const CSeq_id_Handle& idh,
                        CCache_blob& blob)
{
    bool    was_blob_found = false;

    CAsnIndex::SIndexInfo info;

    was_blob_found = s_GetChunkAndOffset(idh, *m_Index, info);

    if (! was_blob_found ) {
        return false;
    }

    return  x_GetBlob(info, blob);
}

bool CAsnCache::x_GetBlob(const CAsnIndex::SIndexInfo &info, CCache_blob& blob)
{
    _TRACE("id=" << info.seq_id
           << " gi=" << info.gi
           << " timestamp=" << info.timestamp
           << " chunk=" << info.chunk
           << " offs=" << info.offs
           << " size=" << info.size);
    try {
        if ( !m_CurrChunk.get()  ||  info.chunk != m_CurrChunkId) {
            try {
                m_CurrChunk.reset(new CChunkFile(m_DbPath, info.chunk));
                m_CurrChunk->OpenForRead( );
                m_CurrChunkId = info.chunk;
            }
            catch (CException& e) {
                ERR_POST(Error << e);
                m_CurrChunk.reset();
                throw;
            }
        }
        m_CurrChunk->Read( blob, info.offs, info.size );
    }
    catch ( CException & e ) {
        ERR_POST( "Unable to read or unpack a raw chunk.  ChunkId = " << info.chunk
                    << " offset = " << info.offs << " size = " << info.size );
        ERR_POST( "SeqId = " << info.seq_id << " gi = " << info.gi
                    << " timestamp = " << info.timestamp );
        ERR_POST( e );
        return false;
    }
    return true;
}

bool CAsnCache::GetMultipleBlobs(const CSeq_id_Handle& id,
                                 vector< CRef<CCache_blob> >& blobs)
{
    vector<CAsnIndex::SIndexInfo> info;

    bool was_blob_found = s_GetChunkAndOffset(id, *m_Index, info, true);

    if (! was_blob_found ) {
        return false;
    }

    ITERATE (vector<CAsnIndex::SIndexInfo>, blob_it, info) {
        CRef<CCache_blob> blob(new CCache_blob);
        if (x_GetBlob(*blob_it, *blob)) {
            blobs.push_back(blob);
        }
    }
    return !blobs.empty();
}

bool CAsnCache::GetRaw(const CSeq_id_Handle& idh,
                       TBuffer& buffer)
{
    CCache_blob blob;
    if ( !GetBlob(idh, blob) ) {
        return false;
    }

    blob.UnPack(buffer);
    return true;
}

bool CAsnCache::GetMultipleRaw(const CSeq_id_Handle& id, vector<TBuffer>& buffer)
{
    vector< CRef<CCache_blob> > blobs;
    if ( !GetMultipleBlobs(id, blobs) ) {
        return false;
    }
    buffer.resize(blobs.size());
    ITERATE (vector< CRef<CCache_blob> >, blob_it, blobs) {
        (*blob_it)->UnPack(buffer[blob_it - blobs.begin()]);
    }
    return true;
}

CRef<CSeq_entry> CAsnCache::GetEntry(const CSeq_id_Handle& idh)
{
    CCache_blob blob;
    if ( !GetBlob(idh, blob) ) {
        return CRef<CSeq_entry>();
    }

    CRef<CSeq_entry> entry(new CSeq_entry);
    blob.UnPack(*entry);
    return entry;
}

vector< CRef<CSeq_entry> > CAsnCache::
GetMultipleEntries(const CSeq_id_Handle& id)
{
    vector< CRef<CSeq_entry> > entries;
    vector< CRef<CCache_blob> > blobs;
    if ( GetMultipleBlobs(id, blobs) ) {
        ITERATE (vector< CRef<CCache_blob> >, blob_it, blobs) {
            CRef<CSeq_entry> entry(new CSeq_entry);
            (*blob_it)->UnPack(*entry);
            entries.push_back(entry);
        }
    }

    return entries;
}


bool CAsnCache::GetIndexEntry( const CSeq_id_Handle& id_handle,
                               CAsnIndex::SIndexInfo& info )
{
    return  s_GetChunkAndOffset(id_handle, *m_Index, info);
}

bool CAsnCache::GetMultipleIndexEntries(const objects::CSeq_id_Handle & id,
                                        vector<CAsnIndex::SIndexInfo> &info)
{
    return s_GetChunkAndOffset(id, *m_Index, info, true);
}

END_NCBI_SCOPE
