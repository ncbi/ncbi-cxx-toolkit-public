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
#include <objtools/data_loaders/genbank/cache/reader_cache.hpp>
#include <objtools/data_loaders/genbank/cache/reader_cache_entry.hpp>
#include <objtools/data_loaders/genbank/cache/reader_cache_params.h>
#include <objtools/data_loaders/genbank/readers.hpp> // for entry point
#include <objtools/data_loaders/genbank/dispatcher.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <corelib/ncbitime.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/plugin_manager_store.hpp>

#include <util/cache/icache.hpp>
#include <connect/ncbi_conn_stream.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/annot_selector.hpp>

#include <serial/objistrasnb.hpp>       // for reading Seq-ids
#include <serial/serial.hpp>            // for reading Seq-ids
#include <objects/seqloc/Seq_id.hpp>    // for reading Seq-ids

#define FIX_BAD_ID2S_REPLY_DATA 1

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

NCBI_PARAM_DECL(int, GENBANK, CACHE_DEBUG);

NCBI_PARAM_DEF_EX(int, GENBANK, CACHE_DEBUG, 0,
                  eParam_NoThread, GENBANK_CACHE_DEBUG);

int SCacheInfo::GetDebugLevel(void)
{
    static const int s_Value =
        NCBI_PARAM_TYPE(GENBANK, CACHE_DEBUG)::GetDefault();
    return s_Value;
}

const int    SCacheInfo::BLOB_IDS_MAGIC = 0x32fd0108;
const char*  SCacheInfo::BLOB_IDS_SUBKEY = "blobs8";

string SCacheInfo::GetBlobKey(const CBlob_id& blob_id)
{
    CNcbiOstrstream oss;
    oss << blob_id.GetSat();
    if ( blob_id.GetSubSat() != 0 ) {
        oss << '.' << blob_id.GetSubSat();
    }
    oss << '-' << blob_id.GetSatKey();
    return CNcbiOstrstreamToString(oss);
}


string SCacheInfo::GetIdKey(TGi gi)
{
    return NStr::NumericToString(gi);
}


string SCacheInfo::GetIdKey(const CSeq_id& id)
{
    return id.IsGi()? GetIdKey(id.GetGi()): id.AsFastaString();
}


string SCacheInfo::GetIdKey(const CSeq_id_Handle& id)
{
    return id.IsGi()? GetIdKey(id.GetGi()): id.AsString();
}


static const size_t kHashLimit = 100;


void SCacheInfo::GetBlob_idsSubkey(const SAnnotSelector* sel,
                                   string& subkey,
                                   string& true_subkey)
{
    if ( !sel ) {
        subkey = BLOB_IDS_SUBKEY;
        return;
    }
    const SAnnotSelector::TNamedAnnotAccessions& accs =
        sel->GetNamedAnnotAccessions();
    if ( accs.empty() ) {
        subkey = BLOB_IDS_SUBKEY;
        return;
    }

    CNcbiOstrstream str;
    str << BLOB_IDS_SUBKEY;
    size_t total_size = 0;
    ITERATE ( SAnnotSelector::TNamedAnnotAccessions, it, accs ) {
        total_size += 1+it->first.size();
    }
    bool add_hash = total_size > kHashLimit;
    if ( add_hash ) {
        size_t hash = 5381;
        ITERATE ( SAnnotSelector::TNamedAnnotAccessions, it, accs ) {
            hash = hash*17 + it->first.size();
            ITERATE ( string, i, it->first ) {
                hash = hash*17 + (*i & 0xff);
            }
        }
        str << ";#" << hex << hash << dec;
    }
    ITERATE ( SAnnotSelector::TNamedAnnotAccessions, it,
              sel->GetNamedAnnotAccessions() ) {
        str << ';';
        str << it->first;
    }
    if ( add_hash ) {
        true_subkey = CNcbiOstrstreamToString(str);
        subkey = true_subkey.substr(0, kHashLimit);
    }
    else {
        subkey = CNcbiOstrstreamToString(str);
    }
}


const char* SCacheInfo::GetGiSubkey(void)
{
    return "gi4";
}


const char* SCacheInfo::GetAccVerSubkey(void)
{
    return "accver";
}


const char* SCacheInfo::GetLabelSubkey(void)
{
    return "label";
}


const char* SCacheInfo::GetTaxIdSubkey(void)
{
    return "taxid";
}


const char* SCacheInfo::GetSeq_idsSubkey(void)
{
    return "ids4";
}


const char* SCacheInfo::GetBlobVersionSubkey(void)
{
    return "ver4";
}


string SCacheInfo::GetBlobSubkey(CLoadLockBlob& blob, int chunk_id)
{
    if ( chunk_id == kMain_ChunkId )
        return string();
    else if ( chunk_id == kDelayedMain_ChunkId )
        return "ext";
    else {
        CNcbiOstrstream oss;
        oss << chunk_id << '-' << blob->GetSplitInfo().GetSplitVersion();
        return CNcbiOstrstreamToString(oss);;
    }
}


/////////////////////////////////////////////////////////////////////////////
// CCacheHolder
/////////////////////////////////////////////////////////////////////////////

CCacheHolder::CCacheHolder(void)
    : m_BlobCache(0), m_IdCache(0)
{
}


CCacheHolder::~CCacheHolder(void)
{
    SetIdCache(0);
    SetBlobCache(0);
}


void CCacheHolder::SetIdCache(ICache* id_cache)
{
    m_IdCache = id_cache;
}


void CCacheHolder::SetBlobCache(ICache* blob_cache)
{
    m_BlobCache = blob_cache;
}


/////////////////////////////////////////////////////////////////////////


CCacheReader::CCacheReader(void)
    : m_JoinedBlobVersion(eDefault)
{
    SetMaximumConnections(1);
}


CCacheReader::CCacheReader(const TPluginManagerParamTree* params,
                           const string& driver_name)
    : m_JoinedBlobVersion(eDefault)
{
    CConfig conf(params);
    bool joined_blob_version = conf.GetBool(
        driver_name,
        NCBI_GBLOADER_READER_CACHE_PARAM_JOINED_BLOB_VERSION,
        CConfig::eErr_NoThrow,
        true);
    m_JoinedBlobVersion = joined_blob_version? eDefault: eOff;
    SetMaximumConnections(1);
}


void CCacheReader::x_AddConnectionSlot(TConn /*conn*/)
{
}


void CCacheReader::x_RemoveConnectionSlot(TConn /*conn*/)
{
    SetIdCache(0);
    SetBlobCache(0);
}


void CCacheReader::x_DisconnectAtSlot(TConn /*conn*/, bool /*failed*/)
{
}


void CCacheReader::x_ConnectAtSlot(TConn /*conn*/)
{
}


int CCacheReader::GetRetryCount(void) const
{
    return 2;
}


bool CCacheReader::MayBeSkippedOnErrors(void) const
{
    return true;
}


int CCacheReader::GetMaximumConnectionsLimit(void) const
{
    return 1;
}


//////////////////////////////////////////////////////////////////


namespace {
    class CParseBuffer {
    public:
        CParseBuffer(ICache* cache,
                     const string& key, int version, const string& subkey);

        bool Found(void) const
            {
                return m_Descr.blob_found;
            }
        bool Done(void) const
            {
                if ( m_Ptr ) {
                    return m_Size == 0;
                }
                else {
                    return x_Eof();
                }
            }

        Uint4 ParseUint4(void)
            {
                const char* ptr = x_NextBytes(4);
                return ((ptr[0]&0xff)<<24)|((ptr[1]&0xff)<<16)|
                    ((ptr[2]&0xff)<<8)|(ptr[3]&0xff);
            }
        Int4 ParseInt4(void)
            {
                return ParseUint4();
            }
        string ParseString(void);
        string FullString(void);
        
    protected:
        bool x_Eof(void) const;
        const char* x_NextBytes(size_t size);
        
    private:
        CParseBuffer(const CParseBuffer&);
        void operator=(const CParseBuffer&);

        char m_Buffer[4096];
        ICache::SBlobAccessDescr m_Descr;
        const char* m_Ptr;
        size_t m_Size;
    };

    CParseBuffer::CParseBuffer(ICache* cache,
                               const string& key,
                               int version,
                               const string& subkey)
        : m_Descr(m_Buffer, sizeof(m_Buffer)), m_Ptr(0), m_Size(0)
    {
        if ( SCacheInfo::GetDebugLevel() ) {
            LOG_POST(Info<<"CCache:Read: "<<key<<","<<subkey<<","<<version);
        }
        cache->GetBlobAccess(key, version, subkey, &m_Descr);
        if ( !m_Descr.reader.get() ) {
            m_Ptr = m_Descr.buf;
            m_Size = m_Descr.blob_size;
        }
    }
    
    string CParseBuffer::ParseString(void)
    {
        string ret;
        size_t size = ParseUint4();
        if ( m_Ptr ) {
            ret.assign(x_NextBytes(size), size);
        }
        else {
            ret.reserve(size);
            while ( size ) {
                size_t count = min(size, sizeof(m_Buffer));
                ret.assign(x_NextBytes(count), count);
                size -= count;
            }
        }
        return ret;
    }

    string CParseBuffer::FullString(void)
    {
        string ret;
        if ( m_Ptr ) {
            ret.assign(m_Ptr, m_Size);
            m_Ptr += m_Size;
            m_Size = 0;
        }
        else {
            for ( ;; ) {
                size_t count = 0;
                if ( m_Descr.reader->Read(m_Buffer, sizeof(m_Buffer), &count) != eRW_Success ) {
                    break;
                }
                ret.append(m_Buffer, count);
            }
        }
        return ret;
    }
    
    const char* CParseBuffer::x_NextBytes(size_t size)
    {
        const char* ret = m_Ptr;
        if ( ret ) {
            if ( m_Size >= size ) {
                m_Ptr = ret + size;
                m_Size -= size;
                return ret;
            }
        }
        else if ( size <= sizeof(m_Buffer) ) {
            char* buf = m_Buffer;
            while ( size ) {
                size_t count = 0;
                if ( m_Descr.reader->Read(buf, size, &count) != eRW_Success ) {
                    break;
                }
                buf += count;
                size -= count;
            }
            if ( size == 0 ) {
                return m_Buffer;
            }
        }
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "parse buffer overflow");
    }

    bool CParseBuffer::x_Eof(void) const
    {
        char buffer[1];
        size_t count;
        return m_Descr.reader->Read(buffer, 1, &count) == eRW_Eof;
    }
}



bool CCacheReader::LoadStringSeq_ids(CReaderRequestResult& result,
                                     const string& seq_id)
{
    if ( !m_IdCache ) {
        return false;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    return ReadSeq_ids(result, seq_id, ids);
}


bool CCacheReader::LoadSeq_idSeq_ids(CReaderRequestResult& result,
                                     const CSeq_id_Handle& seq_id)
{
    if ( !m_IdCache ) {
        return false;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    return ReadSeq_ids(result, GetIdKey(seq_id), ids);
}


bool CCacheReader::LoadSeq_idGi(CReaderRequestResult& result,
                                const CSeq_id_Handle& seq_id)
{
    if ( !m_IdCache ) {
        return false;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoadedGi() ) {
        return true;
    }
    
    CParseBuffer str(m_IdCache, GetIdKey(seq_id), 0, GetGiSubkey());
    if ( str.Found() ) {
        TGi gi = GI_FROM(Int4, str.ParseInt4());
        if ( str.Done() ) {
            ids.SetLoadedGi(gi);
            return true;
        }
    }
    
    ReadSeq_ids(result, GetIdKey(seq_id), ids);
    
    return ids.IsLoadedGi();
}


bool CCacheReader::LoadSeq_idAccVer(CReaderRequestResult& result,
                                    const CSeq_id_Handle& seq_id)
{
    if ( !m_IdCache ) {
        return false;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoadedAccVer() ) {
        return true;
    }

    CParseBuffer str(m_IdCache, GetIdKey(seq_id), 0, GetAccVerSubkey());
    if ( str.Found() ) {
        string data = str.FullString();
        CSeq_id_Handle acch;
        if ( !data.empty() ) {
            CSeq_id id(data);
            acch = CSeq_id_Handle::GetHandle(id);
        }
        ids.SetLoadedAccVer(acch);
        return true;
    }

    ReadSeq_ids(result, GetIdKey(seq_id), ids);
    
    return ids.IsLoadedAccVer();
}


bool CCacheReader::LoadSeq_idLabel(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id)
{
    if ( !m_IdCache ) {
        return false;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoadedLabel() ) {
        return true;
    }
    
    CParseBuffer str(m_IdCache, GetIdKey(seq_id), 0, GetLabelSubkey());
    if ( str.Found() ) {
        ids.SetLoadedLabel(str.FullString());
        return true;
    }

    ReadSeq_ids(result, GetIdKey(seq_id), ids);
    
    return ids.IsLoadedLabel();
}


bool CCacheReader::LoadSeq_idTaxId(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id)
{
    if ( !m_IdCache ) {
        return false;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids.IsLoadedTaxId() ) {
        return true;
    }
    
    CParseBuffer str(m_IdCache, GetIdKey(seq_id), 0, GetTaxIdSubkey());
    if ( str.Found() ) {
        int taxid = str.ParseInt4();
        if ( str.Done() ) {
            ids.SetLoadedTaxId(taxid);
            return true;
        }
    }

    return false;
}


bool CCacheReader::LoadAccVers(CReaderRequestResult& result,
                               const TIds& ids, TLoaded& loaded, TIds& ret)
{
    if ( !m_IdCache ) {
        return false;
    }
    size_t count = ids.size();
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        CLoadLockSeq_ids lock(result, ids[i]);
        if ( !lock.IsLoadedAccVer() ) {
            LoadSeq_idAccVer(result, ids[i]);
        }
        if ( !lock.IsLoadedAccVer() ) {
            LoadSeq_idSeq_ids(result, ids[i]);
        }
        if ( lock.IsLoadedAccVer() ) {
            ret[i] = lock->GetAccVer();
            loaded[i] = true;
            continue;
        }
    }
    return false;
}


bool CCacheReader::LoadGis(CReaderRequestResult& result,
                           const TIds& ids, TLoaded& loaded, TGis& ret)
{
    if ( !m_IdCache ) {
        return false;
    }
    size_t count = ids.size();
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        CLoadLockSeq_ids lock(result, ids[i]);
        if ( !lock.IsLoadedGi() ) {
            LoadSeq_idGi(result, ids[i]);
        }
        if ( lock.IsLoadedGi() ) {
            ret[i] = lock->GetGi();
            loaded[i] = true;
            continue;
        }
    }
    return false;
}


bool CCacheReader::LoadLabels(CReaderRequestResult& result,
                              const TIds& ids, TLoaded& loaded, TLabels& ret)
{
    if ( !m_IdCache ) {
        return false;
    }
    size_t count = ids.size();
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        CLoadLockSeq_ids lock(result, ids[i]);
        if ( !lock.IsLoadedLabel() ) {
            LoadSeq_idLabel(result, ids[i]);
        }
        if ( lock.IsLoadedLabel() ) {
            ret[i] = lock->GetLabel();
            loaded[i] = true;
            continue;
        }
    }
    return false;
}


bool CCacheReader::LoadTaxIds(CReaderRequestResult& result,
                              const TIds& ids, TLoaded& loaded, TTaxIds& ret)
{
    if ( !m_IdCache ) {
        return false;
    }
    size_t count = ids.size();
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        CLoadLockSeq_ids lock(result, ids[i]);
        if ( !lock.IsLoadedTaxId() ) {
            LoadSeq_idTaxId(result, ids[i]);
        }
        if ( lock.IsLoadedTaxId() ) {
            ret[i] = lock->GetTaxId();
            loaded[i] = true;
            continue;
        }
    }
    return false;
}


bool CCacheReader::ReadSeq_ids(CReaderRequestResult& result,
                               const string& key,
                               CLoadLockSeq_ids& ids)
{
    if ( !m_IdCache ) {
        return false;
    }

    if ( ids.IsLoaded() ) {
        return true;
    }

    TSeqIds seq_ids;
    {{
        CConn conn(result, this);
        if ( GetDebugLevel() ) {
            LOG_POST(Info<<"CCache:Read: "<<key<<","<<GetSeq_idsSubkey());
        }
        auto_ptr<IReader> reader
            (m_IdCache->GetReadStream(key, 0, GetSeq_idsSubkey()));
        if ( !reader.get() ) {
            conn.Release();
            return false;
        }
        CRStream r_stream(reader.release(), 0, 0, CRWStreambuf::fOwnAll);
        CObjectIStreamAsnBinary obj_stream(r_stream);
        size_t count = static_cast<CObjectIStream&>(obj_stream).ReadUint4();
        for ( size_t i = 0; i < count; ++i ) {
            CSeq_id id;
            obj_stream >> id;
            seq_ids.push_back(CSeq_id_Handle::GetHandle(id));
        }
        conn.Release();
    }}
    ids.SetLoadedSeq_ids(0, CFixedSeq_ids(eTakeOwnership, seq_ids));
    return true;
}


bool CCacheReader::LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id,
                                      const SAnnotSelector* sel)
{
    if ( !m_IdCache ) {
        return false;
    }

    CLoadLockBlob_ids ids(result, seq_id, sel);
    if( ids.IsLoaded() ) {
        return true;
    }
    
    string subkey, true_subkey;
    GetBlob_idsSubkey(sel, subkey, true_subkey);
    CParseBuffer str(m_IdCache, GetIdKey(seq_id), 0, subkey);
    if ( !str.Found() ) {
        return false;
    }
    if ( str.ParseInt4() != BLOB_IDS_MAGIC ) {
        return false;
    }
    
    int state = str.ParseUint4();
    TBlobIds blob_ids;
    size_t blob_count = str.ParseUint4();
    blob_ids.reserve(blob_count);
    
    for ( size_t i = 0; i < blob_count; ++i ) {
        CRef<CBlob_id> id(new CBlob_id);
        id->SetSat(str.ParseUint4());
        id->SetSubSat(str.ParseUint4());
        id->SetSatKey(str.ParseUint4());
        CBlob_Info info(id, str.ParseUint4());
        CRef<CBlob_Annot_Info> annots_info;
        size_t name_count = str.ParseUint4();
        if ( name_count ) {
            annots_info = new CBlob_Annot_Info;
            for ( size_t j = 0; j < name_count; ++j ) {
                annots_info->AddNamedAnnotName(str.ParseString());
            }
        }
        string s = str.ParseString();
        if ( !s.empty() ) {
            if ( !annots_info ) {
                annots_info = new CBlob_Annot_Info;
            }
            CObjectIStreamAsnBinary stream(s.data(), s.size());
            CRef<CID2S_Seq_annot_Info> annot_info;
            while ( stream.HaveMoreData() ) {
                annot_info.Reset(new CID2S_Seq_annot_Info);
                stream >> *annot_info;
                annots_info->AddAnnotInfo(*annot_info);
            }
        }
        if ( annots_info ) {
            info.SetAnnotInfo(annots_info);
        }
        blob_ids.push_back(info);
    }
    if ( !true_subkey.empty() && str.ParseString() != true_subkey ) {
        return false;
    }
    if ( !str.Done() ) {
        return false;
    }
    ids.SetLoadedBlob_ids(state, CFixedBlob_ids(eTakeOwnership, blob_ids));

    return true;
}


bool CCacheReader::LoadBlobVersion(CReaderRequestResult& result,
                                   const TBlobId& blob_id)
{
    if ( !m_IdCache ) {
        return false;
    }
    
    CParseBuffer str(m_IdCache, GetBlobKey(blob_id), 0, GetBlobVersionSubkey());
    if ( str.Found() ) {
        int version = str.ParseInt4();
        if ( str.Done() ) {
            SetAndSaveBlobVersion(result, blob_id, version);
            return true;
        }
    }
    return false;
}


bool CCacheReader::LoadBlob(CReaderRequestResult& result,
                            const TBlobId& blob_id)
{
    return LoadChunk(result, blob_id, CProcessor::kMain_ChunkId);
}


bool CCacheReader::LoadChunk(CReaderRequestResult& result,
                             const TBlobId& blob_id,
                             TChunkId chunk_id)
{
    if ( !m_BlobCache ) {
        return false;
    }

    CLoadLockBlob blob(result, blob_id);
    if ( CProcessor::IsLoaded(result, blob_id, chunk_id, blob) ) {
        return true;
    }

    string key = GetBlobKey(blob_id);
    string subkey = GetBlobSubkey(blob, chunk_id);
    if ( !blob.IsSetBlobVersion() ) {
        if ( m_JoinedBlobVersion != eOff ) {
            do { // artificial one-time cycle to allow breaking
                CConn conn(result, this);
                TBlobVersion version;
                ICache::EBlobVersionValidity validity;
                auto_ptr<IReader> reader;
                try {
                    if ( GetDebugLevel() ) {
                        LOG_POST(Info<<"CCache:ReadV: "<<key<<","<<subkey);
                    }
                    reader.reset(m_BlobCache->GetReadStream(key, subkey,
                                                            &version,
                                                            &validity));
                    if ( m_JoinedBlobVersion == eDefault ) {
                        // joined blob version is supported by ICache
                        m_JoinedBlobVersion = eOn;
                    }
                }
                catch ( CException& /*ignored*/ ) {
                    if ( m_JoinedBlobVersion == eOn ) {
                        // we know already that joined blob version
                        // is supported by ICache -> it's a transient exception
                        throw;
                    }
                    // joined blob version is not supported by ICache
                    m_JoinedBlobVersion = eOff;
                    conn.Release();
                    break; // continue with separate blob version
                }
                if ( !reader.get() ) {
                    // no any blob data
                    conn.Release();
                    return false;
                }
                if ( validity != ICache::eCurrent ) {
                    // check if blob version is still the same

                    // read the blob to allow next ICache command
                    CConn_MemoryStream data;
                    {{
                        CRStream stream(reader.release(),
                                        0, 0, CRWStreambuf::fOwnAll);
                        data << stream.rdbuf();
                    }}
                    conn.Release();

                    // get blob version from other readers (e.g. ID2)
                    m_Dispatcher->LoadBlobVersion(result, blob_id, this);
                    if ( !blob.IsSetBlobVersion() ||
                         blob.GetBlobVersion() != version ) {
                        // Cannot determine the blob verion or
                        // cached blob version is outdated ->
                        // pass the request to the next reader.
                        return false;
                    }
                    // current blob version is still valid
                    if ( GetDebugLevel() ) {
                        LOG_POST(Info<<"SetBlobVersionAsCurrent("<<key<<", "<<subkey<<", "<<version<<")");
                    }
                    m_BlobCache->SetBlobVersionAsCurrent(key, subkey, version);
                    x_ProcessBlob(result, blob_id, chunk_id, data);
                    return true;
                }
                else {
                    // current blob version is valid
                    blob.SetBlobVersion(version);
                    {{
                        CRStream stream(reader.release(),
                                        0, 0, CRWStreambuf::fOwnAll);
                        x_ProcessBlob(result, blob_id, chunk_id, stream);
                    }}
                    conn.Release();
                    return true;
                }
            } while ( false );
            // failed, continue with old-style version
        }
        {{
            CConn conn(result, this);
            if ( !m_BlobCache->HasBlobs(key, subkey) ) {
                conn.Release();
                return false;
            }
        }}
        m_Dispatcher->LoadBlobVersion(result, blob_id);
        if ( !blob.IsSetBlobVersion() ) {
            return false;
        }
    }
    TBlobVersion version = blob.GetBlobVersion();
    _ASSERT(version);

    CConn conn(result, this);
    if ( GetDebugLevel() ) {
        LOG_POST(Info<<"CCache:Read: "<<key<<","<<subkey<<","<<version);
    }
    auto_ptr<IReader> reader(m_BlobCache->GetReadStream(key, version, subkey));
    if ( !reader.get() ) {
        conn.Release();
        return false;
    }

    CRStream stream(reader.release(), 0, 0, CRWStreambuf::fOwnAll);
    x_ProcessBlob(result, blob_id, chunk_id, stream);
    conn.Release();
    return true;
}


void CCacheReader::x_ProcessBlob(CReaderRequestResult& result,
                                 const TBlobId& blob_id,
                                 TChunkId chunk_id,
                                 CNcbiIstream& stream)
{
    int processor_type = ReadInt(stream);
    const CProcessor& processor =
        m_Dispatcher->GetProcessor(CProcessor::EType(processor_type));
    if ( processor_type != processor.GetType() ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "CCacheReader::LoadChunk: "
                       "invalid processor type: "<<processor_type);
    }
    int processor_magic = ReadInt(stream);
    if ( processor_magic != int(processor.GetMagic()) ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "CCacheReader::LoadChunk: "
                       "invalid processor magic number: "<<processor_magic);
    }
    processor.ProcessStream(result, blob_id, chunk_id, stream);
}


struct SPluginParams
{
    typedef SCacheInfo::TParams TParams;

    struct SDefaultValue {
        const char* name;
        const char* value;
    };


    static
    TParams* FindSubNode(TParams* params, const string& name)
    {
        if ( params ) {
            for ( TParams::TNodeList_I it = params->SubNodeBegin();
                  it != params->SubNodeEnd(); ++it ) {
                if ( NStr::CompareNocase((*it)->GetKey(), name) == 0 ) {
                    return static_cast<TParams*>(*it);
                }
            }
        }
        return 0;
    }


    static
    const TParams* FindSubNode(const TParams* params,
                               const string& name)
    {
        if ( params ) {
            for ( TParams::TNodeList_CI it = params->SubNodeBegin();
                  it != params->SubNodeEnd(); ++it ) {
                if ( NStr::CompareNocase((*it)->GetKey(), name) == 0 ) {
                    return static_cast<const TParams*>(*it);
                }
            }
        }
        return 0;
    }


    static
    TParams* SetSubNode(TParams* params,
                        const string& name,
                        const char* default_value = "")
    {
        _ASSERT(!name.empty());
        TParams* node = FindSubNode(params, name);
        if ( !node ) {
            node = params->AddNode(TParams::TValueType(name, default_value));
        }
        return node;
    }


    static
    TParams* SetSubSection(TParams* params, const string& name)
    {
        return SetSubNode(params, name, "");
    }


    static
    const string& SetDefaultValue(TParams* params,
                                  const string& name,
                                  const char* value)
    {
        return SetSubNode(params, name, value)->GetValue().value;
    }


    static
    const string& SetDefaultValue(TParams* params, const SDefaultValue& value)
    {
        return SetDefaultValue(params, value.name, value.value);
    }


    static
    void SetDefaultValues(TParams* params, const SDefaultValue* values)
    {
        for ( ; values->name; ++values ) {
            SetDefaultValue(params, *values);
        }
    }

};


static
bool IsDisabledCache(const SCacheInfo::TParams* params)
{
    const SCacheInfo::TParams* driver =
        SPluginParams::FindSubNode(params,
            NCBI_GBLOADER_READER_CACHE_PARAM_DRIVER);
    if ( driver ) {
        if ( driver->GetValue().value.empty() ) {
            // driver is set empty, it means no cache
            return true;
        }
    }
    return false;
}


static
SCacheInfo::TParams* GetDriverParams(SCacheInfo::TParams* params)
{
    const string& driver_name =
        SPluginParams::SetDefaultValue(params,
                        NCBI_GBLOADER_READER_CACHE_PARAM_DRIVER,
                        "bdb");
    return SPluginParams::SetSubSection(params, driver_name);
}


static
SCacheInfo::TParams*
GetCacheParamsCopy(const SCacheInfo::TParams* src_params,
                   const char* section_name)
{
    const SCacheInfo::TParams* src_section =
        SPluginParams::FindSubNode(src_params, section_name);
    if ( IsDisabledCache(src_section) ) {
        // no cache
        return 0;
    }
    if ( src_section ) {
        // make a copy of params
        return new SCacheInfo::TParams(*src_section);
    }
    else {
        // new param tree, if section is absent
        return new SCacheInfo::TParams();
    }
}

static const SPluginParams::SDefaultValue s_DefaultParams[] = {
    { "path", ".genbank_cache" },
    { "keep_versions", "all" },
    { "write_sync", "no" },
    { "mem_size", "20M" },
    { "log_file_max", "20M" },
    { "purge_batch_sleep", "500" }, // .5 sec
    { "purge_thread_delay", "3600" }, // 1 hour
    { "purge_clean_log", "16" },
    { 0, 0 }
};
static const SPluginParams::SDefaultValue s_DefaultIdParams[] = {
    { "name", "ids" },
    { "timeout", "172800" }, // 2 days
    { "timestamp", "subkey check_expiration" /* purge_on_startup"*/ },
    { "page_size", "small" },
    { 0, 0}
};
static const SPluginParams::SDefaultValue s_DefaultBlobParams[] = {
    { "name", "blobs" },
    { "timeout", "432000" }, // 5 days
    { "timestamp", "onread expire_not_used" /* purge_on_startup"*/ },
    { 0, 0 }
};
static const SPluginParams::SDefaultValue s_DefaultReaderParams[] = {
    { "purge_thread", "yes" },
    { 0, 0 }
};
static const SPluginParams::SDefaultValue s_DefaultWriterParams[] = {
    { "purge_thread", "no" },
    { 0, 0 }
};

SCacheInfo::TParams*
GetCacheParams(const SCacheInfo::TParams* src_params,
                SCacheInfo::EReaderOrWriter reader_or_writer,
                SCacheInfo::EIdOrBlob id_or_blob)
{
    const char* section_name;
    if ( id_or_blob == SCacheInfo::eIdCache ) {
        section_name = NCBI_GBLOADER_READER_CACHE_PARAM_ID_SECTION;
    }
    else {
        section_name = NCBI_GBLOADER_READER_CACHE_PARAM_BLOB_SECTION;
    }
    auto_ptr<SCacheInfo::TParams> section
        (GetCacheParamsCopy(src_params, section_name));
    if ( !section.get() ) {
        // disabled
        return 0;
    }
    // fill driver section with default values
    SCacheInfo::TParams* driver_params = GetDriverParams(section.get());
    SPluginParams::SetDefaultValues(driver_params, s_DefaultParams);
    if ( id_or_blob == SCacheInfo::eIdCache ) {
        SPluginParams::SetDefaultValues(driver_params, s_DefaultIdParams);
    }
    else {
        SPluginParams::SetDefaultValues(driver_params, s_DefaultBlobParams);
    }
    if ( reader_or_writer == SCacheInfo::eCacheReader ) {
        SPluginParams::SetDefaultValues(driver_params, s_DefaultReaderParams);
    }
    else {
        SPluginParams::SetDefaultValues(driver_params, s_DefaultWriterParams);
    }
    return section.release();
}


ICache* SCacheInfo::CreateCache(const TParams* params,
                                EReaderOrWriter reader_or_writer,
                                EIdOrBlob id_or_blob)
{
    auto_ptr<TParams> cache_params
        (GetCacheParams(params, reader_or_writer, id_or_blob));
    if ( !cache_params.get() ) {
        return 0;
    }
    typedef CPluginManager<ICache> TCacheManager;
    CRef<TCacheManager> manager(CPluginManagerGetter<ICache>::Get());
    _ASSERT(manager);
    return manager->CreateInstanceFromKey
        (cache_params.get(), NCBI_GBLOADER_READER_CACHE_PARAM_DRIVER);
}


void CCacheReader::InitializeCache(CReaderCacheManager& cache_manager,
                                   const TPluginManagerParamTree* params)
{
    const TPluginManagerParamTree* reader_params = params ?
        params->FindNode(NCBI_GBLOADER_READER_CACHE_DRIVER_NAME) : 0;
    ICache* id_cache = 0;
    ICache* blob_cache = 0;
    auto_ptr<TParams> id_params
        (GetCacheParams(reader_params, eCacheReader, eIdCache));
    auto_ptr<TParams> blob_params
        (GetCacheParams(reader_params, eCacheReader, eBlobCache));
    _ASSERT(id_params.get());
    _ASSERT(blob_params.get());
    const TParams* share_id_param =
        id_params->FindNode(NCBI_GBLOADER_WRITER_CACHE_PARAM_SHARE);
    bool share_id = !share_id_param  ||
        NStr::StringToBool(share_id_param->GetValue().value);
    const TParams* share_blob_param =
        blob_params->FindNode(NCBI_GBLOADER_WRITER_CACHE_PARAM_SHARE);
    bool share_blob = !share_blob_param  ||
        NStr::StringToBool(share_blob_param->GetValue().value);
    if (share_id  ||  share_blob) {
        if ( share_id ) {
            ICache* cache = cache_manager.
                FindCache(CReaderCacheManager::fCache_Id,
                          id_params.get());
            if ( cache ) {
                _ASSERT(!id_cache);
                id_cache = cache;
            }
        }
        if ( share_blob ) {
            ICache* cache = cache_manager.
                FindCache(CReaderCacheManager::fCache_Blob,
                          blob_params.get());
            if ( cache ) {
                _ASSERT(!blob_cache);
                blob_cache = cache;
            }
        }
    }
    if ( !id_cache ) {
        id_cache = CreateCache(reader_params, eCacheReader, eIdCache);
        if ( id_cache ) {
            cache_manager.RegisterCache(*id_cache,
                CReaderCacheManager::fCache_Id);
        }
    }
    if ( !blob_cache ) {
        blob_cache = CreateCache(reader_params, eCacheReader, eBlobCache);
        if ( blob_cache ) {
            cache_manager.RegisterCache(*blob_cache,
                CReaderCacheManager::fCache_Blob);
        }
    }
    SetIdCache(id_cache);
    SetBlobCache(blob_cache);
}


void CCacheReader::ResetCache(void)
{
    SetIdCache(0);
    SetBlobCache(0);
}


END_SCOPE(objects)


/// Class factory for Cache reader
///
/// @internal
///
using namespace objects;

class CCacheReaderCF :
    public CSimpleClassFactoryImpl<CReader, CCacheReader>
{
    typedef CSimpleClassFactoryImpl<CReader, CCacheReader> TParent;
public:
    CCacheReaderCF()
        : TParent(NCBI_GBLOADER_READER_CACHE_DRIVER_NAME, 0)
        {
        }
    ~CCacheReaderCF()
        {
        }


    CReader*
    CreateInstance(const string& driver  = kEmptyStr,
                   CVersionInfo version = NCBI_INTERFACE_VERSION(CReader),
                   const TPluginManagerParamTree* params = 0) const
    {
        if ( !driver.empty()  &&  driver != m_DriverName ) {
            return 0;
        }
        if ( !version.Match(NCBI_INTERFACE_VERSION(CReader)) ) {
            return 0;
        }
        return new CCacheReader(params, driver);
    }
};


void NCBI_EntryPoint_CacheReader(
     CPluginManager<CReader>::TDriverInfoList&   info_list,
     CPluginManager<CReader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CCacheReaderCF>::NCBI_EntryPointImpl(info_list,
                                                             method);
}


void NCBI_EntryPoint_xreader_cache(
     CPluginManager<CReader>::TDriverInfoList&   info_list,
     CPluginManager<CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_CacheReader(info_list, method);
}


void GenBankReaders_Register_Cache(void)
{
    RegisterEntryPoint<CReader>(NCBI_EntryPoint_CacheReader);
}


END_NCBI_SCOPE
