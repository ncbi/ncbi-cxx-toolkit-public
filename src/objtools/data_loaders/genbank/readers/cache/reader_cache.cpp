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
#include <objtools/data_loaders/genbank/readers/cache/reader_cache.hpp>
#include <objtools/data_loaders/genbank/readers/cache/reader_cache_entry.hpp>
#include <objtools/data_loaders/genbank/readers/cache/reader_cache_params.h>
#include <objtools/data_loaders/genbank/readers/readers.hpp> // for entry point
#include <objtools/data_loaders/genbank/dispatcher.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>

#include <corelib/ncbitime.hpp>

#include <util/cache/icache.hpp>

#include <util/rwstream.hpp>

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>

#include <corelib/plugin_manager_store.hpp>

#include <serial/objistrasnb.hpp>       // for reading Seq-ids
#include <serial/serial.hpp>            // for reading Seq-ids
#include <objects/seqloc/Seq_id.hpp>    // for reading Seq-ids

#define FIX_BAD_ID2S_REPLY_DATA 1

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

const int    SCacheInfo::IDS_MAGIC = 0x32fd0104;
const size_t SCacheInfo::IDS_HSIZE  = 2;
const size_t SCacheInfo::IDS_SIZE  = 4;

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


string SCacheInfo::GetIdKey(int gi)
{
    return NStr::IntToString(gi);
}


string SCacheInfo::GetIdKey(const CSeq_id& id)
{
    return id.IsGi()? GetIdKey(id.GetGi()): id.AsFastaString();
}


string SCacheInfo::GetIdKey(const CSeq_id_Handle& id)
{
    return id.IsGi()? GetIdKey(id.GetGi()): id.AsString();
}


const char* SCacheInfo::GetBlob_idsSubkey(void)
{
    return "blobs";
}


const char* SCacheInfo::GetGiSubkey(void)
{
    return "gi";
}


const char* SCacheInfo::GetSeq_idsSubkey(void)
{
    return "ids";
}


const char* SCacheInfo::GetBlobVersionSubkey(void)
{
    return "ver";
}


string SCacheInfo::GetBlobSubkey(int chunk_id)
{
    if ( chunk_id == kMain_ChunkId )
        return kEmptyStr;
    else
        return NStr::IntToString(chunk_id);
}


/////////////////////////////////////////////////////////////////////////////
// CCacheHolder
/////////////////////////////////////////////////////////////////////////////

CCacheHolder::CCacheHolder(ICache* blob_cache,
                           ICache* id_cache,
                           TOwnership own)
    : m_BlobCache(blob_cache), m_IdCache(id_cache), m_Own(own)
{
}


CCacheHolder::~CCacheHolder(void)
{
    SetIdCache(0);
    SetBlobCache(0);
}


void CCacheHolder::SetIdCache(ICache* id_cache, TOwnership own)
{
    if ( m_Own & fOwnIdCache ) {
        delete m_IdCache;
        m_Own &= ~fOwnIdCache;
    }

    m_IdCache = id_cache;
    m_Own |= (own & fOwnIdCache);
}


void CCacheHolder::SetBlobCache(ICache* blob_cache, TOwnership own)
{
    if ( m_Own & fOwnBlobCache ) {
        delete m_BlobCache;
        m_Own &= ~fOwnBlobCache;
    }

    m_BlobCache = blob_cache;
    m_Own |= (own & fOwnBlobCache);
}


/////////////////////////////////////////////////////////////////////////


CCacheReader::CCacheReader(ICache* blob_cache,
                           ICache* id_cache,
                           TOwnership own)
    : CCacheHolder(blob_cache, id_cache, own)
{
    SetMaximumConnections(1);
}


void CCacheReader::x_Connect(TConn /*conn*/)
{
}


void CCacheReader::x_Disconnect(TConn /*conn*/)
{
    SetIdCache(0);
    SetBlobCache(0);
}


int CCacheReader::GetRetryCount(void) const
{
    return 0;
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



bool CCacheReader::x_LoadIdCache(const string& key,
                                 const string& subkey,
                                 TIdCacheData& data)
{
    size_t size = m_IdCache->GetSize(key, 0, subkey);
    data.resize(size / sizeof(int));
    if ( size == 0 ) {
        return false;
    }
    if ( size % sizeof(int) != 0 ||
         !m_IdCache->Read(key, 0, subkey, &data[0], size) ) {
        return false;
    }
    return true;
}


bool CCacheReader::LoadStringSeq_ids(CReaderRequestResult& result,
                                     const string& seq_id)
{
    if ( !m_IdCache ) {
        return false;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    return ReadSeq_ids(seq_id, ids);
}


bool CCacheReader::LoadSeq_idSeq_ids(CReaderRequestResult& result,
                                     const CSeq_id_Handle& seq_id)
{
    if ( !m_IdCache ) {
        return false;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    return ReadSeq_ids(GetIdKey(seq_id), ids);
}


bool CCacheReader::LoadSeq_idGi(CReaderRequestResult& result,
                                const CSeq_id_Handle& seq_id)
{
    if ( !m_IdCache ) {
        return false;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    if ( !ids->IsLoadedGi() ) {
        TIdCacheData data;
        if ( x_LoadIdCache(GetIdKey(seq_id), GetGiSubkey(), data) &&
             data.size() == 1  ) {
            ids->SetLoadedGi(data.front());
            return true;
        }
    }
    return false;
}


bool CCacheReader::ReadSeq_ids(const string& key,
                               CLoadLockSeq_ids& ids)
{
    if ( !m_IdCache ) {
        return false;
    }

    if ( ids.IsLoaded() ) {
        return true;
    }

    auto_ptr<IReader> reader
        (m_IdCache->GetReadStream(key, 0, GetSeq_idsSubkey()));
    if ( !reader.get() ) {
        return false;
    }
    
    CLoadInfoSeq_ids::TSeq_ids seq_ids;
    {{
        CRStream r_stream(reader.get());
        CObjectIStreamAsnBinary obj_stream(r_stream);
        CSeq_id id;
        while ( obj_stream.HaveMoreData() ) {
            obj_stream >> id;
            seq_ids.push_back(CSeq_id_Handle::GetHandle(id));
        }
    }}
    ids->m_Seq_ids.swap(seq_ids);
    ids.SetLoaded();
    return true;
}


bool CCacheReader::LoadSeq_idBlob_ids(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id)
{
    if ( !m_IdCache ) {
        return false;
    }

    CLoadLockBlob_ids ids(result, seq_id);
    if( ids.IsLoaded() ) {
        return true;
    }

    string key = GetIdKey(seq_id);

    TIdCacheData data;
    if ( !x_LoadIdCache(key, GetBlob_idsSubkey(), data) ||
         data.size() % IDS_SIZE != IDS_HSIZE|| data.front() != IDS_MAGIC ) {
        return false;
    }

    ids->SetState(data[1]);
    for ( size_t i = IDS_HSIZE; i < data.size(); i += IDS_SIZE ) {
        CBlob_id id;
        id.SetSat(data[i+0]);
        id.SetSubSat(data[i+1]);
        id.SetSatKey(data[i+2]);
        ids.AddBlob_id(id, data[i+3]);
    }
    ids.SetLoaded();
    return true;
}


bool CCacheReader::LoadBlobVersion(CReaderRequestResult& result,
                                   const TBlobId& blob_id)
{
    if ( !m_IdCache ) {
        return false;
    }

    TIdCacheData data;
    if ( x_LoadIdCache(GetBlobKey(blob_id), GetBlobVersionSubkey(), data) &&
         data.size() == 1  ) {
        SetAndSaveBlobVersion(result, blob_id, data.front());
        return true;
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
    if ( CProcessor::IsLoaded(blob_id, chunk_id, blob) ) {
        return true;
    }

    string key = GetBlobKey(blob_id);
    string subkey = GetBlobSubkey(chunk_id);
    if ( !blob.IsSetBlobVersion() ) {
        if ( !m_BlobCache->HasBlobs(key, subkey) ) {
            return false;
        }
        m_Dispatcher->LoadBlobVersion(result, blob_id);
        if ( !blob.IsSetBlobVersion() ) {
            return false;
        }
    }
    TBlobVersion version = blob.GetBlobVersion();

    auto_ptr<IReader> reader(m_BlobCache->GetReadStream(key, version, subkey));
    if ( !reader.get() ) {
        return false;
    }

    CRStream stream(reader.get());
    int processor_type = ReadInt(stream);
    const CProcessor& processor =
        m_Dispatcher->GetProcessor(CProcessor::EType(processor_type));
    if ( processor_type != processor.GetType() ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "invalid processor type: "+
                   NStr::IntToString(processor_type));
    }
    int processor_magic = ReadInt(stream);
    if ( processor_magic != int(processor.GetMagic()) ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "invalid processor magic number: "+
                   NStr::IntToString(processor_magic));
    }
    processor.ProcessStream(result, blob_id, chunk_id, stream);
    return true;
}

END_SCOPE(objects)


void GenBankReaders_Register_Cache(void)
{
    RegisterEntryPoint<objects::CReader>(NCBI_EntryPoint_CacheReader);
}


/// Class factory for Cache reader
///
/// @internal
///
class CCacheReaderCF :
    public CSimpleClassFactoryImpl<objects::CReader, objects::CCacheReader>
{
public:
    typedef objects::CCacheReader TReader;
    typedef CSimpleClassFactoryImpl<objects::CReader, TReader> TParent;
public:
    CCacheReaderCF()
        : TParent(NCBI_GBLOADER_READER_CACHE_DRIVER_NAME, 0) {}
    ~CCacheReaderCF() {}

    ICache* CreateCache(const TPluginManagerParamTree* params,
                        const string& section) const
        {
            typedef CPluginManager<ICache> TCacheManager;
            typedef CPluginManagerStore::CPMMaker<ICache> TCacheManagerStore;
            CRef<TCacheManager> CacheManager(TCacheManagerStore::Get());
            _ASSERT(CacheManager);
            const TPluginManagerParamTree* cache_params = params ?
                params->FindSubNode(section) : 0;
            return CacheManager->CreateInstanceFromKey
                (cache_params,
                 NCBI_GBLOADER_READER_CACHE_PARAM_DRIVER);
        }
    
    objects::CReader* 
    CreateInstance(const string& driver  = kEmptyStr,
                   CVersionInfo version =
                   NCBI_INTERFACE_VERSION(objects::CReader),
                   const TPluginManagerParamTree* params = 0) const
    {
        if ( !driver.empty()  &&  driver != m_DriverName ) {
            return 0;
        }
        if (version.Match(NCBI_INTERFACE_VERSION(objects::CReader)) 
                            != CVersionInfo::eNonCompatible) {
            auto_ptr<ICache> id_cache
                (CreateCache(params,
                             NCBI_GBLOADER_READER_CACHE_PARAM_ID_SECTION));
            auto_ptr<ICache> blob_cache
                (CreateCache(params,
                             NCBI_GBLOADER_READER_CACHE_PARAM_BLOB_SECTION));
            if ( blob_cache.get()  ||  id_cache.get() ) 
                return new TReader(blob_cache.release(),
                                   id_cache.release(),
                                   TReader::fOwnAll);
        }
        return 0;
    }
};


void NCBI_EntryPoint_CacheReader(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CCacheReaderCF>::NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xreader_cache(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_CacheReader(info_list, method);
}


END_NCBI_SCOPE
