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
    SetInitialConnections(1);
}


void CCacheReader::x_AddConnectionSlot(TConn /*conn*/)
{
}


void CCacheReader::x_RemoveConnectionSlot(TConn /*conn*/)
{
    SetIdCache(0);
    SetBlobCache(0);
}


void CCacheReader::x_DisconnectAtSlot(TConn /*conn*/)
{
}


void CCacheReader::x_ConnectAtSlot(TConn /*conn*/)
{
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

struct SPluginParams
{
    //typedef NCBI_NS_NCBI::CTreePairNode<string, string> TParams;
    typedef ncbi::CTreePairNode<std::string, std::string> TParams;

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
                if ( NStr::CompareNocase((*it)->GetValue().id, name) == 0 ) {
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
                if ( NStr::CompareNocase((*it)->GetValue().id, name) == 0 ) {
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
            node = params->AddNode(name, default_value);
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
        return SetSubNode(params, name, value)->GetValue();
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
bool IsDisabledCache(const SPluginParams::TParams* params)
{
    const SPluginParams::TParams* driver =
        SPluginParams::FindSubNode(params,
            NCBI_GBLOADER_READER_CACHE_PARAM_DRIVER);
    if ( driver ) {
        if ( driver->GetValue().empty() ) {
            // driver is set empty, it means no cache
            return true;
        }
    }
    return false;
}
    

static
SPluginParams::TParams* GetDriverParams(SPluginParams::TParams* params)
{
    const string& driver_name =
        SPluginParams::SetDefaultValue(params,
                        NCBI_GBLOADER_READER_CACHE_PARAM_DRIVER,
                        "bdb");
    return SPluginParams::SetSubSection(params, driver_name);
}


static
SPluginParams::TParams*
GetCacheParamsCopy(const SPluginParams::TParams* src_params,
                   const char* section_name)
{
    const SPluginParams::TParams* src_section =
        SPluginParams::FindSubNode(src_params, section_name);
    if ( IsDisabledCache(src_section) ) {
        // no cache
        return 0;
    }
    if ( src_section ) {
        // make a copy of params
        return new SPluginParams::TParams(*src_section);
    }
    else {
        // new param tree, if section is absent
        return new SPluginParams::TParams();
    }
}

static const SPluginParams::SDefaultValue s_DefaultParams[] = {
    { "path", ".genbank_cache" },
    { "keep_versions", "all" },
    { "write_sync", "no" },
    { "mem_size", "20M" },
    { "log_file_max", "20M" },
    { "purge_batch_sleep", "500" }, // .5 sec
    { "purge_batch_delay", "3600" }, // 1 hour
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

SPluginParams::TParams*
GetCacheParams(const SPluginParams::TParams* src_params,
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
    auto_ptr<SPluginParams::TParams> section
        (GetCacheParamsCopy(src_params, section_name));
    if ( !section.get() ) {
        // disabled
        return 0;
    }
    // fill driver section with default values
    SPluginParams::TParams* driver_params = GetDriverParams(section.get());
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
        auto_ptr<ICache> id_cache
            (SCacheInfo::CreateCache(params,
                                     SCacheInfo::eCacheReader,
                                     SCacheInfo::eIdCache));
        auto_ptr<ICache> blob_cache
            (SCacheInfo::CreateCache(params,
                                     SCacheInfo::eCacheReader,
                                     SCacheInfo::eBlobCache));
        if ( blob_cache.get()  ||  id_cache.get() ) {
            return new CCacheReader(blob_cache.release(), id_cache.release(),
                                    CCacheReader::fOwnAll);
        }
        return 0;
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
