/*  $Id$
 * ===========================================================================
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
 *  Author:  Eugene Vasilchenko, Anatoliy Kuznetsov
 *
 *  File Description: Cached extension of data reader from ID1
 *
 */
#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/readers/id1/reader_id1_cache.hpp>
#include <objtools/data_loaders/genbank/reader_snp.hpp>
#include <objtools/data_loaders/genbank/split_parser.hpp>

#include <corelib/ncbitime.hpp>

#include <util/cache/blob_cache.hpp>
#include <util/cache/int_cache.hpp>
#include <util/cache/icache.hpp>

#include <util/rwstream.hpp>
#include <util/bytesrc.hpp>

#include <serial/objistr.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>

#include <util/compress/reader_zlib.hpp>
#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>

#include <connect/ncbi_conn_stream.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>
#include <objects/seqsplit/ID2S_Chunk_Id.hpp>

#include <objects/id1/id1__.hpp>
#include <objects/id2/ID2_Reply_Data.hpp>

#include <serial/serial.hpp>

#include <stdio.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/// Utility function to skip part of the input byte source
void Id1ReaderSkipBytes(CByteSourceReader& reader, size_t to_skip);


static size_t resolve_id_count = 0;
static double resolve_id_time = 0;

static size_t resolve_gi_count = 0;
static double resolve_gi_time = 0;

static size_t resolve_ver_count = 0;
static double resolve_ver_time = 0;

static size_t whole_blob_count = 0;
static double whole_bytes = 0;
static double whole_time = 0;

static size_t skel_blob_count = 0;
static double skel_bytes = 0;
static double skel_load_time = 0;
static double skel_parse_time = 0;

static size_t split_blob_count = 0;
static double split_bytes = 0;
static double split_load_time = 0;
static double split_parse_time = 0;

static size_t chunk_blob_count = 0;
static double chunk_bytes = 0;
static double chunk_load_time = 0;
static double chunk_parse_time = 0;

static size_t snp_load_count = 0;
static double snp_load_bytes = 0;
static double snp_load_time = 0;

static size_t snp_store_count = 0;
static double snp_store_bytes = 0;
static double snp_store_time = 0;

CCachedId1Reader::CCachedId1Reader(TConn noConn, 
                                   IBLOB_Cache* blob_cache,
                                   IIntCache* id_cache)
    : CId1Reader(noConn),
      m_BlobCache(0), m_IdCache(0),
      m_OldBlobCache(0), m_OldIdCache(0)
{
    SetBlobCache(blob_cache);
    SetIdCache(id_cache);
}


CCachedId1Reader::CCachedId1Reader(TConn noConn, 
                                   ICache* blob_cache,
                                   ICache* id_cache)
    : CId1Reader(noConn),
      m_BlobCache(0), m_IdCache(0),
      m_OldBlobCache(0), m_OldIdCache(0)
{
    SetBlobCache(blob_cache);
    SetIdCache(id_cache);
}


CCachedId1Reader::~CCachedId1Reader()
{
    if ( CollectStatistics() ) {
        PrintStatistics();
    }
}


void CCachedId1Reader::PrintStatistics(void) const
{
    PrintStat("Cache resolution: resolved",
                resolve_id_count, "ids", resolve_id_time);
    PrintStat("Cache resolution: resolved",
                resolve_gi_count, "gis", resolve_gi_time);
    PrintStat("Cache resolution: resolved",
              resolve_ver_count, "blob vers", resolve_ver_time);
    PrintBlobStat("Cache whole: loaded",
                  whole_blob_count, whole_bytes, whole_time);
    PrintBlobStat("Cache skeleton: loaded",
                  skel_blob_count, skel_bytes, skel_load_time);
    PrintBlobStat("Cache skeleton: parsed",
                  skel_blob_count, skel_bytes, skel_parse_time);
    PrintBlobStat("Cache split-info: loaded",
                  split_blob_count, split_bytes, split_load_time);
    PrintBlobStat("Cache split-info: parsed",
                  split_blob_count, split_bytes, split_parse_time);
    PrintBlobStat("Cache chunk: loaded",
                  chunk_blob_count, chunk_bytes, chunk_load_time);
    PrintBlobStat("Cache chunk: parsed",
                  chunk_blob_count, chunk_bytes, chunk_parse_time);
    PrintBlobStat("Cache SNP: loaded",
                  snp_load_count, snp_load_bytes, snp_load_time);
    PrintBlobStat("Cache SNP: stored",
                  snp_store_count, snp_store_bytes, snp_store_time);
}


void CCachedId1Reader::SetBlobCache(ICache* blob_cache)
{
    m_OldBlobCache = 0;
    m_BlobCache = blob_cache;
}


void CCachedId1Reader::SetIdCache(ICache* id_cache)
{
    m_OldIdCache = 0;
    m_IdCache = id_cache;
}


void CCachedId1Reader::SetBlobCache(IBLOB_Cache* blob_cache)
{
    m_BlobCache = 0;
    if ( blob_cache && blob_cache != m_OldBlobCache ) {
        ERR_POST(Warning << "CCachedId1Reader: "
                 "IBLOB_Cache is deprecated, use ICache instead");
    }
    m_OldBlobCache = blob_cache;
}


void CCachedId1Reader::SetIdCache(IIntCache* id_cache)
{
    m_IdCache = 0;
    if ( id_cache && id_cache != m_OldIdCache ) {
        ERR_POST(Warning << "CCachedId1Reader: "
                 "IIntCache is deprecated, use ICache instead");
    }
    m_OldIdCache = id_cache;
}


string CCachedId1Reader::GetBlobKey(const CSeqref& seqref) const
{
    int sat = seqref.GetSat();
    int sat_key = seqref.GetSatKey();

    char szBlobKeyBuf[256];
    sprintf(szBlobKeyBuf, "%i-%i", sat, sat_key);
    return szBlobKeyBuf;
}


string CCachedId1Reader::GetIdKey(int gi) const
{
    return NStr::IntToString(gi);
}


string CCachedId1Reader::GetIdKey(const CSeq_id& id) const
{
    return id.IsGi()? GetIdKey(id.GetGi()): id.AsFastaString();
}


const char* CCachedId1Reader::GetSeqrefsSubkey(void) const
{
    return "srs";
}


const char* CCachedId1Reader::GetGiSubkey(void) const
{
    return "gi";
}


const char* CCachedId1Reader::GetBlobVersionSubkey(void) const
{
    return "ver";
}


const char* CCachedId1Reader::GetSeqEntrySubkey(void) const
{
    return "Seq-entry";
}


const char* CCachedId1Reader::GetSNPTableSubkey(void) const
{
    return "SNP table";
}


const char* CCachedId1Reader::GetSkeletonSubkey(void) const
{
    return "Skeleton";
}


const char* CCachedId1Reader::GetSplitInfoSubkey(void) const
{
    return "ID2S-Split-Info";
}


string CCachedId1Reader::GetChunkSubkey(int chunk_id) const
{
    return "ID2S-Chunk "+NStr::IntToString(chunk_id);
}


void CCachedId1Reader::PurgeSeqrefs(const TSeqrefs& srs, const CSeq_id& id)
{
    if ( m_IdCache ) {
        m_IdCache->Remove(GetIdKey(id));
        ITERATE ( TSeqrefs, it, srs ) {
            const CSeqref& sr = **it;
            m_IdCache->Remove(GetBlobKey(sr));
        }
    }
    else if ( m_OldIdCache ) {
        ITERATE ( TSeqrefs, it, srs ) {
            const CSeqref& sr = **it;
            m_OldIdCache->Remove(sr.GetGi(), 0);
            m_OldIdCache->Remove(sr.GetSatKey(), sr.GetSat());
        }
    }
}


bool CCachedId1Reader::x_GetIdCache(const string& key,
                                    const string& subkey,
                                    vector<int>& ints)
{
    CStopWatch sw;
    if ( CollectStatistics() ) {
        sw.Start();
    }

    size_t size = m_IdCache->GetSize(key, 0, subkey);
    ints.resize(size / sizeof(int));
    if ( size == 0 || size % sizeof(int) != 0 ||
         !m_IdCache->Read(key, 0, subkey, &ints[0], size) ) {
        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogStat("CId1Cache: failed to read id cache record for id",
                    key, subkey, time);
            resolve_id_count++;
            resolve_id_time += time;
        }
        return false;
    }
    if ( CollectStatistics() ) {
        double time = sw.Elapsed();
        LogStat("CId1Cache: resolved id", key, subkey, time);
        resolve_id_count++;
        resolve_id_time += time;
    }
    return true;
}


bool CCachedId1Reader::x_GetIdCache(const string& key,
                                    const string& subkey,
                                    int& value)
{
    CStopWatch sw;
    if ( CollectStatistics() ) {
        sw.Start();
    }

    size_t size = m_IdCache->GetSize(key, 0, subkey);
    if ( size != sizeof(int) ||
         !m_IdCache->Read(key, 0, subkey, &value, size) ) {
        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogStat("CId1Cache: failed to read id cache record for id",
                    key, subkey, time);
            resolve_id_count++;
            resolve_id_time += time;
        }
        return false;
    }
    if ( CollectStatistics() ) {
        double time = sw.Elapsed();
        LogStat("CId1Cache: resolved id", key, subkey, time);
        resolve_id_count++;
        resolve_id_time += time;
    }
    return true;
}


void CCachedId1Reader::x_StoreIdCache(const string& key,
                                      const string& subkey,
                                      const vector<int>& ints)
{
    CStopWatch sw;
    if ( CollectStatistics() ) {
        sw.Start();
    }
    
    m_IdCache->Store(key, 0, subkey, &ints[0], ints.size()*sizeof(int));
    
    if ( CollectStatistics() ) {
        double time = sw.Elapsed();
        LogStat("CId1Cache: stored id", key, subkey, time);
        resolve_id_count++;
        resolve_id_time += time;
    }
}


void CCachedId1Reader::x_StoreIdCache(const string& key,
                                      const string& subkey,
                                      const int& value)
{
    CStopWatch sw;
    if ( CollectStatistics() ) {
        sw.Start();
    }
    
    m_IdCache->Store(key, 0, subkey, &value, sizeof(value));
    
    if ( CollectStatistics() ) {
        double time = sw.Elapsed();
        LogStat("CId1Cache: stored id", key, subkey, time);
        resolve_id_count++;
        resolve_id_time += time;
    }
}


bool CCachedId1Reader::GetSeqrefs(const string& key, TSeqrefs& srs)
{
    vector<int> data;
    if ( !x_GetIdCache(key, GetSeqrefsSubkey(), data) ) {
        return false;
    }
    if ( data.size() % 5 != 0 || data.size() > 50 ) {
        return false;
    }
    ITERATE ( vector<int>, it, data ) {
        int gi      = *it++;
        int sat     = *it++;
        int satkey  = *it++;
        int version = *it++;
        int flags   = *it;
        CRef<CSeqref> sr(new CSeqref(gi, sat, satkey));
        sr->SetVersion(version);
        sr->SetFlags(flags);
        srs.push_back(sr);
    }
    return true;
}


void CCachedId1Reader::StoreSeqrefs(const string& key, const TSeqrefs& srs)
{
    vector<int> data;
    ITERATE ( TSeqrefs, it, srs ) {
        const CSeqref& sr = **it;
        data.push_back(sr.GetGi());
        data.push_back(sr.GetSat());
        data.push_back(sr.GetSatKey());
        data.push_back(sr.GetVersion());
        data.push_back(sr.GetFlags());
    }
    x_StoreIdCache(key, GetSeqrefsSubkey(), data);
}


bool CCachedId1Reader::GetSeqrefs(int gi, TSeqrefs& srs)
{
    if ( m_IdCache ) {
        return GetSeqrefs(GetIdKey(gi), srs);
    }
    else if ( m_OldIdCache) {
        CStopWatch sw;
        if ( CollectStatistics() ) {
            sw.Start();
        }

        vector<int> data;
        if ( !m_OldIdCache->Read(gi, 0, data) ) {
            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                LogStat("CId1Cache: failed to resolve gi", gi, time);
                resolve_gi_count++;
                resolve_gi_time += time;
            }
            return false;
        }
    
        _ASSERT(data.size() == 4 || data.size() == 8);

        for ( size_t pos = 0; pos + 4 <= data.size(); pos += 4 ) {
            int sat = data[pos];
            int satkey = data[pos+1];
            int version = data[pos+2];
            int flags = data[pos+3];
            CRef<CSeqref> sr(new CSeqref(gi, sat, satkey));
            sr->SetVersion(version);
            sr->SetFlags(flags);
            srs.push_back(sr);
        }

        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogStat("CId1Cache: resolved gi", gi, time);
            resolve_gi_count++;
            resolve_gi_time += time;
        }
        return true;
    }
    else {
        return false;
    }
}


void CCachedId1Reader::StoreSeqrefs(int gi, const TSeqrefs& srs)
{
    if ( m_IdCache ) {
        StoreSeqrefs(GetIdKey(gi), srs);
    }
    else if ( m_OldIdCache ) {
        CStopWatch sw;
        if ( CollectStatistics() ) {
            sw.Start();
        }

        vector<int> data;

        ITERATE ( TSeqrefs, it, srs ) {
            const CSeqref& sr = **it;
            data.push_back(sr.GetSat());
            data.push_back(sr.GetSatKey());
            data.push_back(sr.GetVersion());
            data.push_back(sr.GetFlags());
        }

        _ASSERT(data.size() == 4 || data.size() == 8);

        m_OldIdCache->Store(gi, 0, data);

        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogStat("CId1Cache: saved gi", gi, time);
            resolve_gi_count++;
            resolve_gi_time += time;
        }
    }
}


bool CCachedId1Reader::GetSeqrefs(const CSeq_id& id, TSeqrefs& srs)
{
    if ( m_IdCache ) {
        return GetSeqrefs(GetIdKey(id), srs);
    }
    else {
        return false;
    }
}


void CCachedId1Reader::StoreSeqrefs(const CSeq_id& id, const TSeqrefs& srs)
{
    if ( m_IdCache ) {
        StoreSeqrefs(GetIdKey(id), srs);
    }
}


int CCachedId1Reader::GetBlobVersion(const CSeqref& seqref)
{
    if ( m_IdCache ) {
        int version = 0;
        if ( x_GetIdCache(GetBlobKey(seqref),
                          GetBlobVersionSubkey(),
                          version) ) {
            return version;
        }
    }
    else if ( m_OldIdCache ) {
        CStopWatch sw;
        if ( CollectStatistics() ) {
            sw.Start();
        }

        vector<int> data;
        if ( !m_OldIdCache->Read(seqref.GetSatKey(), seqref.GetSat(), data) ) {
            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                LogStat("CId1Cache: failed to get blob version",
                        seqref.printTSE(), time);
                resolve_ver_count++;
                resolve_ver_time += time;
            }
            return 0;
        }
    
        _ASSERT(data.size() == 1);

        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogStat("CId1Cache: got blob version", seqref.printTSE(), time);
            resolve_ver_count++;
            resolve_ver_time += time;
        }

        return data[0];
    }
    return 0;
}


void CCachedId1Reader::StoreBlobVersion(const CSeqref& seqref, int version)
{
    if ( m_IdCache ) {
        x_StoreIdCache(GetBlobKey(seqref),
                       GetBlobVersionSubkey(),
                       version);
    }
    else if ( m_OldIdCache ) {
        CStopWatch sw;
        if ( CollectStatistics() ) {
            sw.Start();
        }

        vector<int> data;
        data.push_back(version);

        _ASSERT(data.size() == 1);

        m_OldIdCache->Store(seqref.GetSatKey(), seqref.GetSat(), data);

        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogStat("CId1Cache: saved blob version", seqref.printTSE(), time);
            resolve_ver_count++;
            resolve_ver_time += time;
        }
    }
}


int CCachedId1Reader::ResolveSeq_id_to_gi(const CSeq_id& id, TConn conn)
{
    if ( m_IdCache ) {
        int gi = 0;
        string key = GetIdKey(id);
        string subkey = GetGiSubkey();
        if ( !x_GetIdCache(key, subkey, gi) ) {
            gi = CId1Reader::ResolveSeq_id_to_gi(id, conn);
            x_StoreIdCache(key, subkey, gi);
        }
        return gi;
    }
    else {
        return CId1Reader::ResolveSeq_id_to_gi(id, conn);
    }
}


void CCachedId1Reader::RetrieveSeqrefs(TSeqrefs& srs, int gi, TConn conn)
{
    if ( !GetSeqrefs(gi, srs) ) {
        CId1Reader::RetrieveSeqrefs(srs, gi, conn);
        StoreSeqrefs(gi, srs);
    }
}


void CCachedId1Reader::GetTSEChunk(const CSeqref& seqref,
                                   CTSE_Chunk_Info& chunk_info,
                                   TConn /*conn*/)
{
    if ( m_BlobCache ) {
        CID2_Reply_Data chunk_data;
        string key = GetBlobKey(seqref);
        string subkey = GetChunkSubkey(chunk_info.GetChunkId());
        CStopWatch sw;
        if ( CollectStatistics() ) {
            sw.Start();
        }
        if ( !LoadData(key, seqref.GetVersion(), subkey.c_str(),
                       chunk_data) ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "CCachedId1Reader::GetTSEChunk: chunk is missing");
        }
        if ( chunk_data.GetData_type() !=
             CID2_Reply_Data::eData_type_id2s_chunk ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "CCachedId1Reader::GetTSEChunk: wrong data type");
        }
        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            size_t size = m_BlobCache->GetSize(key, seqref.GetVersion(),
                                               subkey.c_str());
            LogBlobStat("CId1Cache: read chunk data",
                        seqref, size, time);
            chunk_blob_count++;
            chunk_bytes += size;
            chunk_load_time += time;
            sw.Start();
        }
        CRef<CID2S_Chunk> chunk(new CID2S_Chunk);
        size_t size = 0;
        {{
            auto_ptr<CObjectIStream> in(OpenData(chunk_data));
            CReader::SetSeqEntryReadHooks(*in);
            *in >> *chunk;
            size = in->GetStreamOffset();
        }}
        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogBlobStat("CId1Cache: parse chunk data",
                        seqref, size, time);
            chunk_parse_time += time;
        }
        CSplitParser::Load(chunk_info, *chunk);
        // everything is fine
    }
    else if ( m_OldBlobCache ) {
        CStopWatch sw;
        if ( CollectStatistics() ) {
            sw.Start();
        }

        CID2_Reply_Data chunk_data;
        string key = GetBlobKey(seqref);
        string suffix = "-chunk-"+NStr::IntToString(chunk_info.GetChunkId());
        if ( !LoadData(key, suffix.c_str(), seqref.GetVersion(),
                       chunk_data) ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "CCachedId1Reader::GetTSEChunk: chunk is missing");
        }
        CRef<CID2S_Chunk> chunk(new CID2S_Chunk);
        size_t size = 0;
        {{
            auto_ptr<CObjectIStream> in(OpenData(chunk_data));
            CReader::SetSeqEntryReadHooks(*in);
            *in >> *chunk;
            size = in->GetStreamOffset();
        }}
        CSplitParser::Load(chunk_info, *chunk);
    
        // everything is fine
        if ( CollectStatistics() ) {
            double time = sw.Elapsed();
            LogBlobStat("CId1Cache: read chunk", seqref, size, time);
            chunk_blob_count++;
            chunk_bytes += size;
            chunk_load_time += time;
        }
    }
    else {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CCachedId1Reader::GetTSEChunk: chunk is missing");
    }
}


int CCachedId1Reader::x_GetVersion(const CSeqref& seqref, TConn conn)
{
    int version = GetBlobVersion(seqref);
    if ( version == 0 ) {
        version = CId1Reader::x_GetVersion(seqref, conn);
        _ASSERT(version != 0);
        StoreBlobVersion(seqref, version);
    }
    return version;
}


void CCachedId1Reader::x_GetTSEBlob(CID1server_back& id1_reply,
                                    CRef<CID2S_Split_Info>& split_info,
                                    const CSeqref& seqref,
                                    TConn conn)
{
    // update seqref's version
    GetVersion(seqref, conn);
    if ( !LoadBlob(id1_reply, split_info, seqref) ) {
        // we'll intercept loading deeper and write loaded data on the fly
        CId1Reader::x_GetTSEBlob(id1_reply, split_info, seqref, conn);
    }
}


void CCachedId1Reader::x_ReadTSEBlob(CID1server_back& id1_reply,
                                     const CSeqref& seqref,
                                     CNcbiIstream& stream)
{
    if ( m_BlobCache ) {
        string key = GetBlobKey(seqref);
        int ver = seqref.GetVersion();
        string subkey = GetSeqEntrySubkey();

        try {
            auto_ptr<IWriter> writer(
                m_BlobCache->GetWriteStream(key, ver, subkey));
            
            if ( writer.get() ) {
                {{
                    CWriterByteSourceReader proxy(&stream, writer.get());
                    
                    CObjectIStreamAsnBinary obj_stream(proxy);
                    
                    CStreamDelayBufferGuard guard(obj_stream);
                    
                    CId1Reader::x_ReadTSEBlob(id1_reply, obj_stream);
                }}

                writer->Flush();

                // everything is fine
                return;
            }
        }
        catch ( ... ) {
            // In case of an error we need to remove incomplete BLOB
            // from the cache.
            try {
                m_BlobCache->Remove(key);
            }
            catch ( exception& /*exc*/ ) {
                // ignored
            }

            // continue with exception
            throw;
        }
    }
    else if ( m_OldBlobCache ) {
        string key = GetBlobKey(seqref);
        int version = seqref.GetVersion();

        try {
            auto_ptr<IWriter> writer(m_OldBlobCache->GetWriteStream(key,
                                                                    version));
            
            if ( writer.get() ) {
                {{
                    CWriterByteSourceReader proxy(&stream, writer.get());
                    
                    CObjectIStreamAsnBinary obj_stream(proxy);
                    
                    CStreamDelayBufferGuard guard(obj_stream);
                    
                    CId1Reader::x_ReadTSEBlob(id1_reply, obj_stream);
                }}

                writer->Flush();
                writer.reset();
                // everything is fine
                return;
            }
        }
        catch ( ... ) {
            // In case of an error we need to remove incomplete BLOB
            // from the cache.
            try {
                m_OldBlobCache->Remove(key);
            }
            catch ( exception& /*exc*/ ) {
                // ignored
            }

            // continue with exception
            throw;
        }
    }
    // by deault read from ID1
    CId1Reader::x_ReadTSEBlob(id1_reply, seqref, stream);
}


void CCachedId1Reader::x_GetSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                                     const CSeqref& seqref,
                                     TConn conn)
{
    // update seqref's version
    GetVersion(seqref, conn);
    if ( !LoadSNPTable(snp_info, seqref) ) {
        snp_info.Reset();
        // load SNP table from GenBank
        CId1Reader::x_GetSNPAnnot(snp_info, seqref, conn);

        // and store SNP table in cache
        StoreSNPTable(snp_info, seqref);
    }
}


bool CCachedId1Reader::LoadBlob(CID1server_back& id1_reply,
                                CRef<CID2S_Split_Info>& split_info,
                                const CSeqref& seqref)
{
    return LoadSplitBlob(id1_reply, split_info, seqref) ||
        LoadWholeBlob(id1_reply, seqref);
}


bool CCachedId1Reader::LoadWholeBlob(CID1server_back& id1_reply,
                                     const CSeqref& seqref)
{
    if ( m_BlobCache ) {
        string key = GetBlobKey(seqref);
        int ver = seqref.GetVersion();
        string subkey = GetSeqEntrySubkey();

        CStopWatch sw;
        if ( CollectStatistics() ) {
            sw.Start();
        }
        try {
            auto_ptr<IReader> reader(
                m_BlobCache->GetReadStream(key, ver, subkey));
            if ( !reader.get() ) {
                return false;
            }

            CIRByteSourceReader rd(reader.get());
        
            CObjectIStreamAsnBinary in(rd);
        
            CReader::SetSeqEntryReadHooks(in);
            in >> id1_reply;
        
            // everything is fine
            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                size_t size = in.GetStreamOffset();
                LogBlobStat("CId1Cache: read blob", seqref, size, time);
                whole_blob_count++;
                whole_bytes += size;
                whole_time += time;
            }

            return true;
        }
        catch ( exception& exc ) {
            ERR_POST("CId1Cache: Exception while loading cached blob: " <<
                     seqref.printTSE() << ": " << exc.what());

            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                LogBlobStat("CId1Cache: read fail blob",
                            seqref, 0, time);
                whole_blob_count++;
                whole_time += time;
            }

            return false;
        }
    }
    else if ( m_OldBlobCache ) {
        string key = GetBlobKey(seqref);
        int ver = seqref.GetVersion();

        CStopWatch sw;
        if ( CollectStatistics() ) {
            sw.Start();
        }
        try {
            auto_ptr<IReader> reader(m_OldBlobCache->GetReadStream(key, ver));
            if ( !reader.get() ) {
                return false;
            }

            CIRByteSourceReader rd(reader.get());
        
            CObjectIStreamAsnBinary in(rd);
        
            CReader::SetSeqEntryReadHooks(in);
            in >> id1_reply;
        
            // everything is fine
            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                size_t size = in.GetStreamOffset();
                LogBlobStat("CId1Cache: read blob", seqref, size, time);
                whole_blob_count++;
                whole_bytes += size;
                whole_time += time;
            }

            return true;
        }
        catch ( exception& exc ) {
            ERR_POST("CId1Cache: Exception while loading cached blob: " <<
                     seqref.printTSE() << ": " << exc.what());

            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                LogBlobStat("CId1Cache: read fail blob",
                            seqref, 0, time);
                whole_blob_count++;
                whole_time += time;
            }

            return false;
        }
    }
    else {
        return false;
    }
}


bool CCachedId1Reader::LoadSplitBlob(CID1server_back& id1_reply,
                                     CRef<CID2S_Split_Info>& split_info,
                                     const CSeqref& seqref)
{
    if ( m_BlobCache ) {
        string key = GetBlobKey(seqref);
        int ver = seqref.GetVersion();

        CStopWatch sw;
        if ( CollectStatistics() ) {
            sw.Start();
        }
        try {
            CID2_Reply_Data main_data;
            if ( !LoadData(key, ver, GetSkeletonSubkey(), main_data) ) {
                return false;
            }
            if ( main_data.GetData_type() !=
                 CID2_Reply_Data::eData_type_seq_entry ) {
                return false;
            }
            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                size_t size = m_BlobCache->GetSize(key, ver,
                                                   GetSkeletonSubkey());
                LogBlobStat("CId1Cache: read skeleton data",
                            seqref, size, time);
                skel_blob_count++;
                skel_bytes += size;
                skel_load_time += time;
                sw.Start();
            }
            CID2_Reply_Data split_data;
            if ( !LoadData(key, ver, GetSplitInfoSubkey(), split_data) ) {
                return false;
            }
            if ( split_data.GetData_type() !=
                 CID2_Reply_Data::eData_type_id2s_split_info ) {
                return false;
            }
            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                size_t size = m_BlobCache->GetSize(key, ver,
                                                   GetSplitInfoSubkey());
                LogBlobStat("CId1Cache: read split-info data",
                            seqref, size, time);
                split_blob_count++;
                split_bytes += size;
                split_load_time += time;
                sw.Start();
            }
            size_t size = 0;
            CRef<CSeq_entry> main(new CSeq_entry);
            {{
                auto_ptr<CObjectIStream> in(OpenData(main_data));
                CReader::SetSeqEntryReadHooks(*in);
                *in >> *main;
                size += in->GetStreamOffset();
            }}
            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                LogBlobStat("CId1Cache: parse skeleton data",
                            seqref, size, time);
                skel_parse_time += time;
                sw.Start();
            }
            CRef<CID2S_Split_Info> split(new CID2S_Split_Info);
            {{
                auto_ptr<CObjectIStream> in(OpenData(split_data));
                CReader::SetSeqEntryReadHooks(*in);
                *in >> *split;
                size += in->GetStreamOffset();
            }}
            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                LogBlobStat("CId1Cache: parse split-info data",
                            seqref, size, time);
                split_parse_time += time;
            }
        
            id1_reply.SetGotseqentry(*main);
            split_info = split;

            // everything is fine
            return true;
        }
        catch ( exception& exc ) {
            ERR_POST("CId1Cache: Exception while loading cached blob: " <<
                     seqref.printTSE() << ": " << exc.what());
            return false;
        }
        return false;
    }
    else if ( m_OldBlobCache ) {
        string key = GetBlobKey(seqref);
        int ver = seqref.GetVersion();

        CStopWatch sw;
        if ( CollectStatistics() ) {
            sw.Start();
        }
        try {
            CID2_Reply_Data main_data, split_data;
            if ( !LoadData(key, "-main", ver, main_data) ||
                 !LoadData(key, "-split", ver, split_data) ) {
                return false;
            }
            size_t size = 0;
            CRef<CSeq_entry> main(new CSeq_entry);
            {{
                auto_ptr<CObjectIStream> in(OpenData(main_data));
                CReader::SetSeqEntryReadHooks(*in);
                *in >> *main;
                size += in->GetStreamOffset();
            }}
            CRef<CID2S_Split_Info> split(new CID2S_Split_Info);
            {{
                auto_ptr<CObjectIStream> in(OpenData(split_data));
                CReader::SetSeqEntryReadHooks(*in);
                *in >> *split;
                size += in->GetStreamOffset();
            }}
        
            id1_reply.SetGotseqentry(*main);
            split_info = split;

            // everything is fine
            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                LogBlobStat("CId1Cache: read blob", seqref, size, time);
                whole_blob_count++;
                whole_bytes += size;
                whole_time += time;
            }
        
            return true;
        }
        catch ( exception& exc ) {
            ERR_POST("CId1Cache: Exception while loading cached blob: " <<
                     seqref.printTSE() << ": " << exc.what());

            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                LogBlobStat("CId1Cache: read fail blob",
                            seqref, 0, time);
                whole_blob_count++;
                whole_time += time;
            }

            return false;
        }
    }
    else {
        return false;
    }
}


bool CCachedId1Reader::LoadSNPTable(CSeq_annot_SNP_Info& snp_info,
                                    const CSeqref& seqref)
{
    if ( m_BlobCache ) {
        string key = GetBlobKey(seqref);
        int ver = seqref.GetVersion();
        string subkey = GetSNPTableSubkey();

        CStopWatch sw;
        if ( CollectStatistics() ) {
            sw.Start();
        }
        try {
            auto_ptr<IReader> reader(
                m_BlobCache->GetReadStream(key, ver, subkey));
            if ( !reader.get() ) {
                return false;
            }
            CRStream stream(reader.get());

            // table
            CSeq_annot_SNP_Info_Reader::Read(stream, snp_info);

            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                size_t size = m_BlobCache->GetSize(key, ver, subkey);
                LogBlobStat("CId1Cache: read SNP blob",
                            seqref, size, time);
                snp_load_count++;
                snp_load_bytes += size;
                snp_load_time += time;
            }

            return true;
        }
        catch ( exception& exc ) {
            ERR_POST("CId1Cache: "
                     "Exception while loading cached SNP table: "<<
                     seqref.printTSE() << ": " << exc.what());
            snp_info.Reset();

            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                LogBlobStat("CId1Cache: read fail SNP blob",
                            seqref, 0, time);
                snp_load_count++;
                snp_load_time += time;
            }

            return false;
        }
    }
    else if ( m_OldBlobCache ) {
        string key = GetBlobKey(seqref);
        int ver = seqref.GetVersion();

        CStopWatch sw;
        if ( CollectStatistics() ) {
            sw.Start();
        }
        try {
            auto_ptr<IReader> reader(m_OldBlobCache->GetReadStream(key, ver));
            if ( !reader.get() ) {
                return false;
            }
            CRStream stream(reader.get());

            // blob type
            char type[4];
            if ( !stream.read(type, 4) || memcmp(type, "STBL", 4) != 0 ) {
                if ( CollectStatistics() ) {
                    double time = sw.Elapsed();
                    LogBlobStat("CId1Cache: read fail SNP blob",
                                seqref, 0, time);
                    snp_load_count++;
                    snp_load_time += time;
                }

                return false;
            }

            // table
            CSeq_annot_SNP_Info_Reader::Read(stream, snp_info);

            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                size_t size = m_OldBlobCache->GetSize(key, ver);
                LogBlobStat("CId1Cache: read SNP blob",
                            seqref, size, time);
                snp_load_count++;
                snp_load_bytes += size;
                snp_load_time += time;
            }

            return true;
        }
        catch ( exception& exc ) {
            ERR_POST("CId1Cache: "
                     "Exception while loading cached SNP table: "<<
                     seqref.printTSE() << ": " << exc.what());
            snp_info.Reset();

            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                LogBlobStat("CId1Cache: read fail SNP blob",
                            seqref, 0, time);
                snp_load_count++;
                snp_load_time += time;
            }

            return false;
        }
    }
    else {
        return false;
    }
}


void CCachedId1Reader::StoreSNPTable(const CSeq_annot_SNP_Info& snp_info,
                                     const CSeqref& seqref)
{
    if ( m_BlobCache ) {
        string key = GetBlobKey(seqref);
        int ver = seqref.GetVersion();
        string subkey = GetSNPTableSubkey();

        CStopWatch sw;
        if ( CollectStatistics() ) {
            sw.Start();
        }
        try {
            {{
                auto_ptr<IWriter> writer;
                writer.reset(m_BlobCache->GetWriteStream(key, ver, subkey));
                if ( !writer.get() ) {
                    return;
                }

                {{
                    CWStream stream(writer.get());
                    CSeq_annot_SNP_Info_Reader::Write(stream, snp_info);
                }}
                writer->Flush();
                writer.reset();
            }}

            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                size_t size = m_BlobCache->GetSize(key, ver, subkey);
                LogBlobStat("CId1Cache: saved SNP blob",
                            seqref, size, time);
                snp_store_count++;
                snp_store_bytes += size;
                snp_store_time += time;
            }
        }
        catch ( exception& exc ) {
            ERR_POST("CId1Cache: "
                     "Exception while storing SNP table: "<<
                     seqref.printTSE() << ": " << exc.what());
            try {
                m_BlobCache->Remove(key);
            }
            catch ( exception& /*exc*/ ) {
                // ignored
            }

            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                LogBlobStat("CId1Cache: save fail SNP blob",
                            seqref, 0, time);
                snp_store_count++;
                snp_store_time += time;
            }
        }
    }
    else if ( m_OldBlobCache ) {
        string key = GetBlobKey(seqref);
        int ver = seqref.GetVersion();

        CStopWatch sw;
        if ( CollectStatistics() ) {
            sw.Start();
        }
        try {
            {{
                auto_ptr<IWriter> writer;
                writer.reset(m_OldBlobCache->GetWriteStream(key, ver));
                if ( !writer.get() ) {
                    return;
                }

                {{
                    CWStream stream(writer.get());
                    stream.write("STBL", 4);
                    CSeq_annot_SNP_Info_Reader::Write(stream, snp_info);
                }}
                writer->Flush();
                writer.reset();
            }}

            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                size_t size = m_OldBlobCache->GetSize(key, ver);
                LogBlobStat("CId1Cache: saved SNP blob",
                            seqref, size, time);
                snp_store_count++;
                snp_store_bytes += size;
                snp_store_time += time;
            }
        }        
        catch ( exception& exc ) {
            ERR_POST("CId1Cache: "
                     "Exception while storing SNP table: "<<
                     seqref.printTSE() << ": " << exc.what());
            try {
                m_OldBlobCache->Remove(key);
            }
            catch ( exception& /*exc*/ ) {
                // ignored
            }

            if ( CollectStatistics() ) {
                double time = sw.Elapsed();
                LogBlobStat("CId1Cache: save fail SNP blob",
                            seqref, 0, time);
                snp_store_count++;
                snp_store_time += time;
            }
        }
    }
}


bool CCachedId1Reader::LoadData(const string& key, int version,
                                const char* suffix,
                                CID2_Reply_Data& data)
{
    auto_ptr<IReader> reader(m_BlobCache->GetReadStream(key, version, suffix));
    if ( !reader.get() ) {
        return false;
    }
    
    CIRByteSourceReader rd(reader.get());
    
    CObjectIStreamAsnBinary in(rd);
    
    in >> data;

    // TEMP: TODO: remove this
    if ( data.GetData_format() == CID2_Reply_Data::eData_format_xml &&
         data.GetData_compression()==CID2_Reply_Data::eData_compression_gzip ){
        // FIX old/wrong split fields
        const_cast<CID2_Reply_Data&>(data)
            .SetData_format(CID2_Reply_Data::eData_format_asn_binary);
        const_cast<CID2_Reply_Data&>(data)
            .SetData_compression(CID2_Reply_Data::eData_compression_nlmzip);
        if ( data.GetData_type() > CID2_Reply_Data::eData_type_seq_entry ) {
            const_cast<CID2_Reply_Data&>(data)
                .SetData_type(data.GetData_type()+1);
        }
    }

    return true;
}


bool CCachedId1Reader::LoadData(const string& key, const char* suffix,
                                int version, CID2_Reply_Data& data)
{
    auto_ptr<IReader> reader(m_OldBlobCache->GetReadStream(key + suffix,
                                                          version));
    if ( !reader.get() ) {
        return false;
    }
    
    CIRByteSourceReader rd(reader.get());
    
    CObjectIStreamAsnBinary in(rd);
    
    in >> data;

    return true;
}


class CVectorListReader : public CByteSourceReader
{
public:
    typedef list< vector< char >* > TData;

    CVectorListReader(const TData& data)
        : m_Data(data),
          m_CurrentIter(data.begin()),
          m_CurrentOffset(0)
        {
        }

    size_t Read(char* buffer, size_t bufferLength)
        {
            while ( m_CurrentIter != m_Data.end() ) {
                const vector<char> curr = **m_CurrentIter;
                if ( m_CurrentOffset < curr.size() ) {
                    size_t remaining = curr.size() - m_CurrentOffset;
                    size_t count = min(bufferLength, remaining);
                    memcpy(buffer, &curr[m_CurrentOffset], count);
                    m_CurrentOffset += count;
                    return count;
                }
                ++m_CurrentIter;
                m_CurrentOffset = 0;
            }
            return 0;
        }
    
private:
    const TData& m_Data;
    TData::const_iterator m_CurrentIter;
    size_t m_CurrentOffset;
};


class COctetStringSequenceStream : public CConn_MemoryStream
{
public:
    typedef vector<char> TOctetString;
    typedef list<TOctetString*> TOctetStringSequence;
    COctetStringSequenceStream(const TOctetStringSequence& in)
        {
            ITERATE( TOctetStringSequence, it, in ) {
                write(&(**it)[0], (*it)->size());
            }
        }
};


CObjectIStream* CCachedId1Reader::OpenData(CID2_Reply_Data& data)
{
    ESerialDataFormat format;
    switch ( data.GetData_format() ) {
    case CID2_Reply_Data::eData_format_asn_binary:
        format = eSerial_AsnBinary;
        break;
    case CID2_Reply_Data::eData_format_asn_text:
        format = eSerial_AsnText;
        break;
    case CID2_Reply_Data::eData_format_xml:
        format = eSerial_Xml;
        break;
    default:
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "unknown serial format");
    }

    CRef<CByteSourceReader> reader;
    auto_ptr<CNcbiIstream> stream;
    switch ( data.GetData_compression() ) {
    case CID2_Reply_Data::eData_compression_none:
        reader.Reset(new CVectorListReader(data.GetData()));
        break;
    case CID2_Reply_Data::eData_compression_nlmzip:
        reader.Reset(new CVectorListReader(data.GetData()));
        reader.Reset(new CNlmZipBtRdr(reader));
        break;
    case CID2_Reply_Data::eData_compression_gzip:
        stream.reset(new CCompressionIStream
                     (*new COctetStringSequenceStream(data.GetData()),
                      new CZipStreamDecompressor,
                      CCompressionIStream::fOwnAll));
        break;
    default:
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "unknown compression");
    }

    if ( stream.get() ) {
        return CObjectIStream::Open(format, *stream.release(), true);
    }
    else {
        return CObjectIStream::Create(format, *reader);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * $Log$
 * Revision 1.26  2004/06/29 14:27:21  vasilche
 * Fixed enum values in ID2-Reply-Data (compression/type/format).
 * Added recognition of old & incorrect values.
 *
 * Revision 1.25  2004/05/21 21:42:52  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.24  2004/04/28 17:06:25  vasilche
 * Load split blobs from new ICache.
 *
 * Revision 1.23  2004/02/17 21:20:11  vasilche
 * Fixed 'unused argument' warning.
 *
 * Revision 1.22  2004/01/22 20:53:31  vasilche
 * Fixed include path.
 *
 * Revision 1.21  2004/01/22 20:10:37  vasilche
 * 1. Splitted ID2 specs to two parts.
 * ID2 now specifies only protocol.
 * Specification of ID2 split data is moved to seqsplit ASN module.
 * For now they are still reside in one resulting library as before - libid2.
 * As the result split specific headers are now in objects/seqsplit.
 * 2. Moved ID2 and ID1 specific code out of object manager.
 * Protocol is processed by corresponding readers.
 * ID2 split parsing is processed by ncbi_xreader library - used by all readers.
 * 3. Updated OBJMGR_LIBS correspondingly.
 *
 * Revision 1.20  2004/01/20 16:56:04  vasilche
 * Allow storing version of any blob (not only SNP).
 *
 * Revision 1.19  2004/01/13 21:54:50  vasilche
 * Requrrected new version
 *
 * Revision 1.5  2004/01/13 16:55:56  vasilche
 * CReader, CSeqref and some more classes moved from xobjmgr to separate lib.
 * Headers moved from include/objmgr to include/objtools/data_loaders/genbank.
 *
 * Revision 1.4  2003/12/30 22:14:42  vasilche
 * Updated genbank loader and readers plugins.
 *
 * Revision 1.17  2003/12/30 16:00:25  vasilche
 * Added support for new ICache (CBDB_Cache) interface.
 *
 * Revision 1.16  2003/12/09 17:30:32  ucko
 * +<stdio.h> for sprintf
 *
 * Revision 1.15  2003/11/26 17:55:59  vasilche
 * Implemented ID2 split in ID1 cache.
 * Fixed loading of splitted annotations.
 *
 * Revision 1.14  2003/10/27 15:05:41  vasilche
 * Added correct recovery of cached ID1 loader if gi->sat/satkey cache is invalid.
 * Added recognition of ID1 error codes: private, etc.
 * Some formatting of old code.
 *
 * Revision 1.13  2003/10/24 15:36:46  vasilche
 * Fixed incorrect order of objects' destruction - IWriter before object stream.
 *
 * Revision 1.12  2003/10/24 13:27:40  vasilche
 * Cached ID1 reader made more safe. Process errors and exceptions correctly.
 * Cleaned statistics printing methods.
 *
 * Revision 1.11  2003/10/23 13:48:38  vasilche
 * Use CRStream and CWStream instead of strstreams.
 *
 * Revision 1.10  2003/10/21 16:32:50  vasilche
 * Cleaned ID1 statistics messages.
 * Now by setting GENBANK_ID1_STATS=1 CId1Reader collects and displays stats.
 * And by setting GENBANK_ID1_STATS=2 CId1Reader logs all activities.
 *
 * Revision 1.9  2003/10/21 14:27:35  vasilche
 * Added caching of gi -> sat,satkey,version resolution.
 * SNP blobs are stored in cache in preprocessed format (platform dependent).
 * Limit number of connections to GenBank servers.
 * Added collection of ID1 loader statistics.
 *
 * Revision 1.8  2003/10/14 21:06:25  vasilche
 * Fixed compression statistics.
 * Disabled caching of SNP blobs.
 *
 * Revision 1.7  2003/10/14 19:31:18  kuznets
 * Removed unnecessary hook in SNP deserialization.
 *
 * Revision 1.6  2003/10/14 18:31:55  vasilche
 * Added caching support for SNP blobs.
 * Added statistics collection of ID1 connection.
 *
 * Revision 1.5  2003/10/08 18:58:23  kuznets
 * Implemented correct ID1 BLOB versions
 *
 * Revision 1.4  2003/10/03 17:41:44  kuznets
 * Added an option, that cache is owned by the ID1 reader.
 *
 * Revision 1.3  2003/10/02 19:29:14  kuznets
 * First working revision
 *
 * Revision 1.2  2003/10/01 19:32:22  kuznets
 * Work in progress
 *
 * Revision 1.1  2003/09/30 19:38:26  vasilche
 * Added support for cached id1 reader.
 *
 */
