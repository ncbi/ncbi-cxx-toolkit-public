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
#include <objtools/data_loaders/genbank/request_result.hpp>

#include <corelib/ncbitime.hpp>

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
#include <serial/iterator.hpp>

#include <stdio.h>

#include <objtools/data_loaders/genbank/readers/id1/statistics.hpp>

#define FIX_BAD_ID1S_REPLY_DATA 1

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/// Utility function to skip part of the input byte source
void Id1ReaderSkipBytes(CByteSourceReader& reader, size_t to_skip);


#ifdef ID1_COLLECT_STATS
static STimeStatistics resolve_id_load;
static STimeStatistics resolve_gi_load;
static STimeStatistics resolve_ver_load;
static STimeStatistics resolve_id_store;
static STimeStatistics resolve_gi_store;
static STimeStatistics resolve_ver_store;

static STimeSizeStatistics whole_load;
static STimeSizeStatistics whole_store;
static STimeSizeStatistics skel_load;
static STimeSizeStatistics skel_parse;
static STimeSizeStatistics split_load;
static STimeSizeStatistics split_parse;
static STimeSizeStatistics chunk_load;
static STimeSizeStatistics chunk_parse;
static STimeSizeStatistics snp_load;
static STimeSizeStatistics snp_store;
#endif


CCachedId1Reader::CCachedId1Reader(TConn /*noConn*/,
                                   ICache* blob_cache,
                                   ICache* id_cache)
    : CId1Reader(1),
      m_BlobCache(0), m_IdCache(0)
{
    SetBlobCache(blob_cache);
    SetIdCache(id_cache);
}


CCachedId1Reader::~CCachedId1Reader()
{
#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() ) {
        PrintStatistics();
    }
#endif
}


void CCachedId1Reader::PrintStatistics(void) const
{
#ifdef ID1_COLLECT_STATS
    PrintStat("Cache resolution: loaded", "ids", resolve_id_load);
    PrintStat("Cache resolution: stored", "ids", resolve_id_store);
    PrintStat("Cache resolution: loaded", "gis", resolve_gi_load);
    PrintStat("Cache resolution: stored", "gis", resolve_gi_store);
    PrintStat("Cache resolution: loaded", "vers", resolve_ver_load);
    PrintStat("Cache resolution: stored", "vers", resolve_ver_store);
    PrintStat("Cache whole: loaded", whole_load);
    PrintStat("Cache whole: stored", whole_store);
    PrintStat("Cache skel: loaded", skel_load);
    PrintStat("Cache skel: parsed", skel_parse);
    PrintStat("Cache split: loaded", split_load);
    PrintStat("Cache split: parsed", split_parse);
    PrintStat("Cache chunk: loaded", chunk_load);
    PrintStat("Cache chunk: parsed", chunk_parse);
    PrintStat("Cache SNP: loaded", snp_load);
    PrintStat("Cache SNP: stored", snp_store);
#endif
}


void CCachedId1Reader::SetBlobCache(ICache* blob_cache)
{
    m_BlobCache = blob_cache;
}


void CCachedId1Reader::SetIdCache(ICache* id_cache)
{
    m_IdCache = id_cache;
}


string CCachedId1Reader::GetBlobKey(const CBlob_id& blob_id) const
{
    char szBlobKeyBuf[256];
    if ( blob_id.GetSubSat() == eSubSat_main ) {
        sprintf(szBlobKeyBuf, "%i-%i", blob_id.GetSat(), blob_id.GetSatKey());
    }
    else {
        sprintf(szBlobKeyBuf, "%i.%i-%i",
                blob_id.GetSat(), blob_id.GetSubSat(), blob_id.GetSatKey());
    }
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


const char* CCachedId1Reader::GetBlob_idsSubkey(void) const
{
    return "blobs";
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


const char* CCachedId1Reader::GetSeqEntryWithSNPSubkey(void) const
{
    return "Seq-entry+SNP";
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


void CCachedId1Reader::LogIdLoadStat(const char* type,
                                     const string& key,
                                     const string& subkey,
                                     CStopWatch& sw)
{
    STimeStatistics* stat;
    if ( subkey == GetGiSubkey() ) {
        stat = &resolve_gi_load;
    }
    else if ( subkey == GetBlobVersionSubkey() ) {
        stat = &resolve_ver_load;
    }
    else {
        stat = &resolve_id_load;
    }
    LogIdStat(type, subkey.c_str(), key, *stat, sw);
}


void CCachedId1Reader::LogIdStoreStat(const char* type,
                                      const string& key,
                                      const string& subkey,
                                      CStopWatch& sw)
{
    STimeStatistics* stat;
    if ( subkey == GetGiSubkey() ) {
        stat = &resolve_gi_store;
    }
    else if ( subkey == GetBlobVersionSubkey() ) {
        stat = &resolve_ver_store;
    }
    else {
        stat = &resolve_id_store;
    }
    LogIdStat(type, subkey.c_str(), key, *stat, sw);
}


bool CCachedId1Reader::x_LoadIdCache(const string& key,
                                     const string& subkey,
                                     TBlob_idsData& data)
{
#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif

    size_t size = m_IdCache->GetSize(key, 0, subkey);
    data.resize(size / sizeof(int));
    if ( size == 0 ) {
        return false;
    }
    if ( size % sizeof(int) != 0 ||
         !m_IdCache->Read(key, 0, subkey, &data[0], size) ) {
#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogIdLoadStat("CId1Cache: failed", key, subkey, sw);
        }
#endif
        return false;
    }
#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() ) {
        LogIdLoadStat("CId1Cache: loaded", key, subkey, sw);
    }
#endif
    return true;
}


bool CCachedId1Reader::x_LoadIdCache(const string& key,
                                     const string& subkey,
                                     int& value)
{
#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif

    size_t size = m_IdCache->GetSize(key, 0, subkey);
    if ( size == 0 ) {
        return false;
    }
    if ( size != sizeof(int) ||
         !m_IdCache->Read(key, 0, subkey, &value, size) ) {
#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogIdLoadStat("CId1Cache: failed", key, subkey, sw);
        }
#endif
        return false;
    }
#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() ) {
        LogIdLoadStat("CId1Cache: loaded", key, subkey, sw);
    }
#endif
    return true;
}


void CCachedId1Reader::x_StoreIdCache(const string& key,
                                      const string& subkey,
                                      const TBlob_idsData& data)
{
#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif
    
    m_IdCache->Store(key, 0, subkey, &data[0], data.size()*sizeof(int));
    
#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() ) {
        LogIdStoreStat("CId1Cache: stored", key, subkey, sw);
    }
#endif
}


void CCachedId1Reader::x_StoreIdCache(const string& key,
                                      const string& subkey,
                                      const int& value)
{
#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif
    
    m_IdCache->Store(key, 0, subkey, &value, sizeof(value));
    
#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() ) {
        LogIdStoreStat("CId1Cache: stored", key, subkey, sw);
    }
#endif
}


static const int IDS_MAGIC = 0x32fc0104;
static const size_t IDS_SIZE = 4;

/*
  Blobs' ids are stored as vector of ints in the following order:
  
  IDS_MAGIC
  zero or more of {
        sat
        sub_sat
        sat_key
        flags // blob contents
  }
*/

bool CCachedId1Reader::LoadIds(const string& key,
                               CLoadLockBlob_ids& ids)
{
    TBlob_idsData data;
    if ( !x_LoadIdCache(key, GetBlob_idsSubkey(), data) ||
         data.size() % IDS_SIZE != 1 || data.front() != IDS_MAGIC ) {
        return false;
    }
    for ( size_t i = 1; i+IDS_SIZE <= data.size(); i += IDS_SIZE ) {
        CBlob_id id;
        id.SetSat(data[i+0]);
        id.SetSubSat(data[i+1]);
        id.SetSatKey(data[i+2]);
        ids.AddBlob_id(id, data[i+3]);
    }
    return true;
}


void CCachedId1Reader::StoreIds(const string& key,
                                const CLoadLockBlob_ids& ids)
{
    TBlob_idsData data;
    data.push_back(IDS_MAGIC);
    ITERATE ( CLoadInfoBlob_ids, it, *ids ) {
        const CBlob_id& id = it->first;
        const CBlob_Info& info = it->second;
        data.push_back(id.GetSat());
        data.push_back(id.GetSubSat());
        data.push_back(id.GetSatKey());
        data.push_back(info.GetContentsMask());
    }
    _ASSERT(data.size() % IDS_SIZE == 1 && data.front() == IDS_MAGIC);
    x_StoreIdCache(key, GetBlob_idsSubkey(), data);
}


bool CCachedId1Reader::LoadIds(int gi, CLoadLockBlob_ids& ids)
{
    if ( m_IdCache ) {
        return LoadIds(GetIdKey(gi), ids);
    }
    return false;
}


bool CCachedId1Reader::LoadIds(const CSeq_id& id, CLoadLockBlob_ids& ids)
{
    if ( m_IdCache ) {
        return LoadIds(GetIdKey(id), ids);
    }
    return false;
}


void CCachedId1Reader::StoreIds(int gi, const CLoadLockBlob_ids& ids)
{
    if ( m_IdCache ) {
        StoreIds(GetIdKey(gi), ids);
    }
}


void CCachedId1Reader::StoreIds(const CSeq_id& id,
                                const CLoadLockBlob_ids& ids)
{
    if ( m_IdCache ) {
        StoreIds(GetIdKey(id), ids);
    }
}


bool CCachedId1Reader::LoadVersion(const string& key, TBlobVersion& version)
{
    if ( m_IdCache ) {
        return x_LoadIdCache(key, GetBlobVersionSubkey(), version);
    }
    return false;
}


void CCachedId1Reader::StoreVersion(const string& key, TBlobVersion version)
{
    if ( m_IdCache ) {
        x_StoreIdCache(key, GetBlobVersionSubkey(), version);
    }
}


int CCachedId1Reader::ResolveSeq_id_to_gi(const CSeq_id& id, TConn conn)
{
    if ( m_IdCache ) {
        int gi = 0;
        string key = GetIdKey(id);
        string subkey = GetGiSubkey();
        if ( !x_LoadIdCache(key, subkey, gi) ) {
            gi = TParent::ResolveSeq_id_to_gi(id, conn);
            x_StoreIdCache(key, subkey, gi);
        }
        return gi;
    }
    return TParent::ResolveSeq_id_to_gi(id, conn);
}


void CCachedId1Reader::ResolveGi(CLoadLockBlob_ids& ids,
                                 int gi, TConn conn)
{
    if ( m_IdCache ) {
        if ( !LoadIds(gi, ids) ) {
            TParent::ResolveGi(ids, gi, conn);
            StoreIds(gi, ids);
        }
        return;
    }
    TParent::ResolveGi(ids, gi, conn);
}


CReader::TBlobVersion CCachedId1Reader::GetVersion(const CBlob_id& blob_id,
                                                   TConn conn)
{
    if ( m_IdCache ) {
        string key = GetBlobKey(blob_id);
        TBlobVersion version;
        if ( !LoadVersion(key, version) ) {
            version = TParent::GetVersion(blob_id, conn);
            StoreVersion(key, version);
        }
        return version;
    }
    return TParent::GetVersion(blob_id, conn);
}


void CCachedId1Reader::GetTSEBlob(CTSE_Info& tse_info,
                                  const CBlob_id& blob_id,
                                  TConn conn)
{
    if ( m_BlobCache ) {
        string key = GetBlobKey(blob_id);
        TBlobVersion version;
        if ( !LoadVersion(key, version) ) {
            version = TParent::GetVersion(blob_id, conn);
            StoreVersion(key, version);
        }
        if ( LoadSplitBlob(tse_info, key, version) ) {
            return;
        }
#if GENBANK_USE_SNP_SATELLITE_15
        if ( LoadWholeBlob(tse_info, key, version) ) {
            return;
        }
#else
        if ( blob_id.GetSubSat() == eSubSat_SNP ) {
            if ( LoadSNPBlob(tse_info, key, version) ) {
                return;
            }
        }
        else {
            if ( LoadWholeBlob(tse_info, key, version) ) {
                return;
            }
        }
#endif
    }
    TParent::GetTSEBlob(tse_info, blob_id, conn);
}


CRef<CSeq_annot_SNP_Info>
CCachedId1Reader::GetSNPAnnot(const CBlob_id& blob_id, TConn conn)
{
    if ( m_BlobCache ) {
        string key = GetBlobKey(blob_id);
        TBlobVersion version;
        if ( !LoadVersion(key, version) ) {
            version = TParent::GetVersion(blob_id, conn);
            StoreVersion(key, version);
        }
        CRef<CSeq_annot_SNP_Info> snp_annot_info = LoadSNPTable(key, version);
        if ( snp_annot_info ) {
            return snp_annot_info;
        }

        // load SNP table from GenBank
        snp_annot_info = TParent::GetSNPAnnot(blob_id, conn);
        
        // and store SNP table in cache
        StoreSNPTable(*snp_annot_info, key, version);

        return snp_annot_info;
    }

    return TParent::GetSNPAnnot(blob_id, conn);
}


void CCachedId1Reader::GetTSEChunk(CTSE_Chunk_Info& chunk_info,
                                   const CBlob_id& blob_id,
                                   TConn conn)
{
    if ( !m_BlobCache ) {
        TParent::GetTSEChunk(chunk_info, blob_id, conn);
        return;
    }

    string key = GetBlobKey(blob_id);
    int version = chunk_info.GetBlobVersion();
    size_t size;

#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif

    CID2_Reply_Data chunk_data;
    string subkey = GetChunkSubkey(chunk_info.GetChunkId());
    size = LoadData(key, version, subkey,
                    chunk_data, CID2_Reply_Data::eData_type_id2s_chunk);
    if ( !size ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CCachedId1Reader::GetTSEChunk: chunk is missing");
    }

#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() ) {
        LogStat("CId1Cache: load chunk",
                key+' '+subkey, chunk_load, sw, size);
    }
#endif

    CRef<CID2S_Chunk> chunk(new CID2S_Chunk);
    {{
        auto_ptr<CObjectIStream> in(OpenData(chunk_data));
        CReader::SetSeqEntryReadHooks(*in);
        *in >> *chunk;
        size = in->GetStreamOffset();
    }}
    CSplitParser::Load(chunk_info, *chunk);

#ifdef ID1_COLLECT_STATS
    if ( CollectStatistics() ) {
        LogStat("CId1Cache: parse chunk",
                key+' '+subkey, chunk_parse, sw, size);
    }
#endif
}


bool CCachedId1Reader::LoadSplitBlob(CTSE_Info& tse_info,
                                     const string& key, TBlobVersion version)
{
    size_t size;

#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif

    try {
        CID2_Reply_Data main_data;
        size = LoadData(key, version, GetSkeletonSubkey(),
                        main_data, CID2_Reply_Data::eData_type_seq_entry);
        if ( !size ) {
            return false;
        }

#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: load skel", key, skel_load, sw, size);
        }
#endif

        CID2_Reply_Data split_data;
        size = LoadData(key, version, GetSplitInfoSubkey(), split_data,
                        CID2_Reply_Data::eData_type_id2s_split_info); 
        if ( !size ) {
            return false;
        }

#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: load split", key, split_load, sw, size);
        }
#endif

        CRef<CSeq_entry> main(new CSeq_entry);
        {{
            auto_ptr<CObjectIStream> in(OpenData(main_data));
            CReader::SetSeqEntryReadHooks(*in);
            *in >> *main;
            size = in->GetStreamOffset();
        }}

#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: parse skel", key, skel_parse, sw, size);
        }
#endif

        CRef<CID2S_Split_Info> split(new CID2S_Split_Info);
        {{
            auto_ptr<CObjectIStream> in(OpenData(split_data));
            CReader::SetSeqEntryReadHooks(*in);
            *in >> *split;
            size = in->GetStreamOffset();
        }}

        tse_info.SetSeq_entry(*main);
        tse_info.SetBlobVersion(version);
        CSplitParser::Attach(tse_info, *split);

#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: parse split", key, split_parse, sw, size);
        }
#endif

        // everything is fine
        return true;
    }
    catch ( exception& exc ) {
        ERR_POST("CId1Cache:LoadSplitBlob: "
                 "Exception: "<<key<<": "<<exc.what());
        return false;
    }
    return false;
}


bool CCachedId1Reader::LoadWholeBlob(CTSE_Info& tse_info,
                                     const string& key, TBlobVersion version)
{
    size_t size;
    
#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif

    CID1server_back id1_reply;

    try {
        auto_ptr<IReader> reader(
            m_BlobCache->GetReadStream(key, version, GetSeqEntrySubkey()));
        if ( !reader.get() ) {
            return false;
        }

        CIRByteSourceReader rd(reader.get());

        {{
            CObjectIStreamAsnBinary in(rd);
            CReader::SetSeqEntryReadHooks(in);
            in >> id1_reply;
            size = in.GetStreamOffset();
        }}
        
#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: load blob", key, whole_load, sw, size);
        }
#endif

    }
    catch ( exception& exc ) {
        ERR_POST("CId1Cache:LoadWholeBlob: "
                 "Exception: " << key << ": " << exc.what());

#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: fail blob", key, whole_load, sw, 0);
        }
#endif

        return false;
    }

    CSeq_annot_SNP_Info_Reader::TSNP_InfoMap snps;
    TParent::SetSeq_entry(tse_info, id1_reply, snps);

    // everything is fine
    return true;
}


void CCachedId1Reader::x_SetBlobRequest(CID1server_request& request,
                                        const CBlob_id& blob_id)
{
    x_SetParams(request.SetGetsewithinfo(), blob_id);
/*
    if ( m_BlobCache ) {
        x_SetParams(request.SetGetsewithinfo(), blob_id);
    }
    else {
        x_SetParams(request.SetGetsefromgi(), blob_id);
    }
*/
}


void CCachedId1Reader::x_ReadBlobReply(CID1server_back& reply,
                                       CObjectIStream& stream,
                                       const CBlob_id& blob_id)
{
    if ( m_BlobCache ) {
        CStreamDelayBufferGuard guard(stream);
        TParent::x_ReadBlobReply(reply, stream, blob_id);
        string key = GetBlobKey(blob_id);
        TBlobVersion version = x_GetVersion(reply);
        StoreVersion(key, version);
        StoreBlob(key, version, guard.EndDelayBuffer());
    }
    else {
        TParent::x_ReadBlobReply(reply, stream, blob_id);
    }
}


void CCachedId1Reader::x_ReadBlobReply(CID1server_back& reply,
                                       TSNP_InfoMap& snps,
                                       CObjectIStream& stream,
                                       const CBlob_id& blob_id)
{
    if ( m_BlobCache ) {
        CStreamDelayBufferGuard guard(stream);
        TParent::x_ReadBlobReply(reply, snps, stream, blob_id);
        string key = GetBlobKey(blob_id);
        TBlobVersion version = x_GetVersion(reply);
        StoreVersion(key, version);
        if ( snps.empty() ) {
            StoreBlob(key, version, guard.EndDelayBuffer());
        }
        else {
            StoreSNPBlob(key, version, reply, snps);
        }
    }
    else {
        TParent::x_ReadBlobReply(reply, snps, stream, blob_id);
    }
}


void CCachedId1Reader::StoreBlob(const string& key, TBlobVersion version,
                                 CRef<CByteSource> bytes)
{

#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif

    try {
        auto_ptr<IWriter> writer(
            m_BlobCache->GetWriteStream(key, version, GetSeqEntrySubkey()));
        if ( !writer.get() ) {
            return;
        }

        size_t size = 0;

        CRef<CByteSourceReader> reader(bytes->Open());
        const size_t BUFFER_SIZE = 8*1024;
        char buffer[BUFFER_SIZE];
        for ( ;; ) {
            size_t cnt = reader->Read(buffer, BUFFER_SIZE);
            if ( cnt == 0 ) {
                if ( reader->EndOfData() ) {
                    break;
                }
                else {
                    NCBI_THROW(CLoaderException, eLoaderFailed,
                               "Cannot store loaded blob in cache");
                }
            }
            size_t written;
            writer->Write(buffer, cnt, &written);
            if ( written != cnt ) {
                NCBI_THROW(CLoaderException, eLoaderFailed,
                           "Cannot store loaded blob in cache");
            }
            size += cnt;
        }
        writer->Flush();
        writer.reset();
        
#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: store blob", key, whole_store, sw, size);
        }
#endif
        // everything is fine
        return;
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
#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: fail blob", key, whole_store, sw, 0);
        }
#endif
        // ignore cache write error - it doesn't affect application
    }
}


void CCachedId1Reader::StoreSNPBlob(const string& key, TBlobVersion version,
                                    const CID1server_back& reply,
                                    const TSNP_InfoMap& snps)
{

#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif

    try {
        auto_ptr<IWriter> writer(
            m_BlobCache->GetWriteStream(key, version,
                                        GetSeqEntryWithSNPSubkey()));
        if ( !writer.get() ) {
            return;
        }

        {{
            CWStream stream(writer.get());
            CSeq_annot_SNP_Info_Reader::Write(stream, ConstBegin(reply), snps);
        }}
        writer->Flush();
        writer.reset();
        
#ifdef ID1_COLLECT_STATS
        size_t size = m_BlobCache->GetSize(key, version,
                                           GetSeqEntryWithSNPSubkey());
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: store SNP blob", key, snp_store, sw, size);
        }
#endif
        // everything is fine
        return;
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
#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: fail SNP blob", key, snp_store, sw, 0);
        }
#endif
        // ignore cache write error - it doesn't affect application
    }
}


bool CCachedId1Reader::LoadSNPBlob(CTSE_Info& tse_info,
                                   const string& key, TBlobVersion version)
{
#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif

    CID1server_back id1_reply;
    CSeq_annot_SNP_Info_Reader::TSNP_InfoMap snps;

    try {
        auto_ptr<IReader> reader(
            m_BlobCache->GetReadStream(key, version,
                                       GetSeqEntryWithSNPSubkey()));
        if ( !reader.get() ) {
            return false;
        }

        {{
            CRStream stream(reader.get());
            CSeq_annot_SNP_Info_Reader::Read(stream, Begin(id1_reply), snps);
        }}
        
#ifdef ID1_COLLECT_STATS
        size_t size = m_BlobCache->GetSize(key, version,
                                           GetSeqEntryWithSNPSubkey());
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: load SNP blob", key, snp_load, sw, size);
        }
#endif

    }
    catch ( exception& exc ) {
        ERR_POST("CId1Cache:LoadSNPBlob: "
                 "Exception: " << key << ": " << exc.what());

#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: fail SNP blob", key, snp_load, sw, 0);
        }
#endif

        return false;
    }

    TParent::SetSeq_entry(tse_info, id1_reply, snps);

    // everything is fine
    return true;
}


CRef<CSeq_annot_SNP_Info>
CCachedId1Reader::LoadSNPTable(const string& key, TBlobVersion version)
{
    CRef<CSeq_annot_SNP_Info> snp_annot_info;
#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif

    try {
        auto_ptr<IReader> reader(
            m_BlobCache->GetReadStream(key, version, GetSNPTableSubkey()));
        if ( !reader.get() ) {
            return snp_annot_info;
        }
        CRStream stream(reader.get());

        snp_annot_info = new CSeq_annot_SNP_Info;
        // table
        CSeq_annot_SNP_Info_Reader::Read(stream, *snp_annot_info);

#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: load SNP", key, snp_load, sw,
                    m_BlobCache->GetSize(key, version, GetSNPTableSubkey()));
        }
#endif

        return snp_annot_info;
    }
    catch ( exception& exc ) {
        ERR_POST("CId1Cache:LoadSNPTable: "
                 "Exception: "<<key<<": "<< exc.what());
        snp_annot_info.Reset();

#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: fail SNP", key, snp_load, sw, 0);
        }
#endif

        return snp_annot_info;
    }
}


void CCachedId1Reader::StoreSNPTable(const CSeq_annot_SNP_Info& snp_info,
                                     const string& key, TBlobVersion version)
{
#ifdef ID1_COLLECT_STATS
    CStopWatch sw(CollectStatistics()>0);
#endif

    try {
        auto_ptr<IWriter> writer(
            m_BlobCache->GetWriteStream(key, version, GetSNPTableSubkey()));
        if ( !writer.get() ) {
            return;
        }
        {{
            CWStream stream(writer.get());
            CSeq_annot_SNP_Info_Reader::Write(stream, snp_info);
        }}
        writer->Flush();
        writer.reset();

#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: store SNP", key, snp_store, sw,
                    m_BlobCache->GetSize(key, version, GetSNPTableSubkey()));
        }
#endif

    }
    catch ( exception& exc ) {
        ERR_POST("CId1Cache:StoreSNPTable: "
                 "Exception: "<< key << ": " << exc.what());
        try {
            m_BlobCache->Remove(key);
        }
        catch ( exception& /*exc*/ ) {
            // ignored
        }

#ifdef ID1_COLLECT_STATS
        if ( CollectStatistics() ) {
            LogStat("CId1Cache: fail SNP", key, snp_store, sw, 0);
        }
#endif
    }
}


size_t CCachedId1Reader::LoadData(const string& key, TBlobVersion version,
                                  const string& suffix,
                                  CID2_Reply_Data& data, int data_type)
{
    size_t size = m_BlobCache->GetSize(key, version, suffix);
    if ( !size ) {
        return 0;
    }

    auto_ptr<IReader> reader(m_BlobCache->GetReadStream(key, version, suffix));
    if ( !reader.get() ) {
        return 0;
    }
    
    CIRByteSourceReader rd(reader.get());
    
    CObjectIStreamAsnBinary in(rd);
    
    in >> data;

#ifdef FIX_BAD_ID1S_REPLY_DATA
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
#endif

    if ( data.GetData_type() != data_type ) {
        return 0;
    }

    return size;
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
    COctetStringSequenceStream(const TOctetStringSequence& inp)
        {
            ITERATE( TOctetStringSequence, it, inp ) {
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
