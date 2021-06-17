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
 *  2018-01-18: Adding support for hierarchical caches.
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

#include <objtools/data_loaders/asn_cache/asn_cache_store.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CAsnCache::CAsnCache(const string& db_path)
    : m_DbPath(db_path)
{
    m_DbPath = CDirEntry::CreateAbsolutePath(m_DbPath);
    m_DbPath = CDirEntry::NormalizePath(m_DbPath, eFollowLinks);
 
    vector<string> db_paths;

    // Add top-level directory to the collection of database paths.
    if ( CFile(NASNCacheFileName::GetBDBIndex(db_path, CAsnIndex::e_main)).Exists() ) {
        db_paths.push_back(db_path);
    }
 
    // Add sub-caches - sub-cache name starts with "subcache" prefix - to the collection
    // of database paths.

    string const kSubcacheMask = "subcache*";

    CDir::TEntries items = CDir(m_DbPath).GetEntries(kSubcacheMask, CDir::fIgnoreRecursive);
    for ( auto const& item: items ) {
        if ( item->IsDir() ) {
            string path = item->GetPath();
            path = CDirEntry::CreateAbsolutePath(path);
            path = CDirEntry::NormalizePath(path, eFollowLinks);

            string main_fname = NASNCacheFileName::GetBDBIndex(path, CAsnIndex::e_main);
            if ( CFile(main_fname).Exists() ) {
                db_paths.push_back(path);
            }
        }
    }

    if ( db_paths.empty() ) {
        NCBI_THROW(CException, eUnknown,
                   "No ASN.1 Cache database is found in " + db_path);
    }

    if ( 1 == db_paths.size() ) {
        m_Store.reset(new CAsnCacheStore(db_paths.at(0)));
    }
    else {
        m_Store.reset(new CAsnCacheStoreMany(db_paths));
    }
}

bool CAsnCache::GetIdInfo(const CSeq_id_Handle& idh,
                          CAsnIndex::TGi& this_gi,
                          time_t& this_timestamp)
{
    return m_Store->GetIdInfo(idh, this_gi, this_timestamp);
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
    return m_Store->GetIdInfo(id, accession, gi, timestamp, sequence_length, tax_id);
}

bool CAsnCache::GetSeqIds(const CSeq_id_Handle& id,
                          vector<CSeq_id_Handle>& all_ids,
                          bool cheap_only)
{
    return m_Store->GetSeqIds(id, all_ids, cheap_only);
}

bool CAsnCache::GetBlob(const CSeq_id_Handle& idh,
                        CCache_blob& blob)
{
    return m_Store->GetBlob(idh, blob);
}

bool CAsnCache::GetMultipleBlobs(const CSeq_id_Handle& id,
                                 vector< CRef<CCache_blob> >& blobs)
{
    return m_Store->GetMultipleBlobs(id, blobs);
}

bool CAsnCache::GetRaw(const CSeq_id_Handle& idh,
                       TBuffer& buffer)
{
    return m_Store->GetRaw(idh, buffer);
}

bool CAsnCache::GetMultipleRaw(const CSeq_id_Handle& id, vector<TBuffer>& buffer)
{
    return m_Store->GetMultipleRaw(id, buffer);
}

CRef<CSeq_entry> CAsnCache::GetEntry(const CSeq_id_Handle& idh)
{
    return m_Store->GetEntry(idh);
}

vector< CRef<CSeq_entry> > CAsnCache::GetMultipleEntries(const CSeq_id_Handle& id)
{
    return m_Store->GetMultipleEntries(id);
}

bool CAsnCache::GetIndexEntry(const objects::CSeq_id_Handle & id,
                              CAsnIndex::SIndexInfo &info)
{
    return m_Store->GetIndexEntry(id, info);
}

bool CAsnCache::GetMultipleIndexEntries(const objects::CSeq_id_Handle & id,
                                        vector<CAsnIndex::SIndexInfo> &info)
{
    return m_Store->GetMultipleIndexEntries(id, info);
}


// AsnCacheStats implementation

size_t CAsnCache::GetGiCount() const
{
    return m_Store->GetGiCount();
}

void CAsnCache::EnumSeqIds(IAsnCacheStore::TEnumSeqidCallback cb) const
{
    m_Store->EnumSeqIds(cb);
}

void CAsnCache::EnumIndex(IAsnCacheStore::TEnumIndexCallback cb) const
{
    m_Store->EnumIndex(cb);
}

END_NCBI_SCOPE
