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
 *  File Description: Cached writer for GenBank data loader
 *
 */
#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/readers/cache/writer_cache.hpp>
#include <objtools/data_loaders/genbank/readers/cache/writer_cache_entry.hpp>
#include <objtools/data_loaders/genbank/readers/cache/reader_cache_params.h>
#include <objtools/data_loaders/genbank/readers/readers.hpp> // for entry point
#include <objtools/data_loaders/genbank/request_result.hpp>

#include <util/cache/icache.hpp>
#include <util/rwstream.hpp>

#include <serial/objostrasnb.hpp>

#include <objmgr/objmgr_exception.hpp>

#include <corelib/plugin_manager_store.hpp>
#include <serial/serial.hpp>
#include <memory>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CCacheWriter::CCacheWriter(ICache* blob_cache,
                           ICache* id_cache,
                           TOwnership own)
    : CCacheHolder(blob_cache, id_cache, own)
{
}


void CCacheWriter::SaveStringSeq_ids(CReaderRequestResult& result,
                                     const string& seq_id)
{
    if ( !m_IdCache) {
        return;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    WriteSeq_ids(seq_id, ids);
}


void CCacheWriter::SaveStringGi(CReaderRequestResult& result,
                                const string& seq_id)
{
    if( !m_IdCache) {
        return;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids->IsLoadedGi() ) {
        int gi = ids->GetGi();
        m_IdCache->Store(seq_id,
                         0,
                         GetGiSubkey(),
                         &gi,
                         sizeof(gi));
    }
}


void CCacheWriter::SaveSeq_idSeq_ids(CReaderRequestResult& result,
                                     const CSeq_id_Handle& seq_id)
{
    if( !m_IdCache) {
        return;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    WriteSeq_ids(GetIdKey(seq_id), ids);
}


void CCacheWriter::SaveSeq_idGi(CReaderRequestResult& result,
                                const CSeq_id_Handle& seq_id)
{
    if( !m_IdCache) {
        return;
    }

    CLoadLockSeq_ids ids(result, seq_id);
    if ( ids->IsLoadedGi() ) {
        int gi = ids->GetGi();
        m_IdCache->Store(GetIdKey(seq_id),
                         0,
                         GetGiSubkey(),
                         &gi,
                         sizeof(gi));
    }
}


void CCacheWriter::WriteSeq_ids(const string& key,
                                const CLoadLockSeq_ids& ids)
{
    if( !m_IdCache) {
        return;
    }

    if ( !ids.IsLoaded() ) {
        return;
    }

    try {
        auto_ptr<IWriter> writer
            (m_IdCache->GetWriteStream(key, 0, GetSeq_idsSubkey()));
        if ( !writer.get() ) {
            return;
        }

        {{
            CWStream w_stream(writer.get());
            CObjectOStreamAsnBinary obj_stream(w_stream);
            ITERATE ( CLoadInfoSeq_ids, it, *ids ) {
                obj_stream << *it->GetSeqId();
            }
        }}

        writer.reset();
    }
    catch ( ... ) {
        // In case of an error we need to remove incomplete data
        // from the cache.
        try {
            m_BlobCache->Remove(key);
        }
        catch ( exception& /*exc*/ ) {
            // ignored
        }
        // ignore cache write error - it doesn't affect application
    }
}


void CCacheWriter::SaveSeq_idBlob_ids(CReaderRequestResult& result,
                                      const CSeq_id_Handle& seq_id)
{
    if ( !m_IdCache) {
        return;
    }

    CLoadLockBlob_ids ids(result, seq_id);
    if ( !ids.IsLoaded() ) {
        return;
    }

    TIdCacheData data;
    data.push_back(IDS_MAGIC);
    data.push_back(ids->GetState());
    ITERATE ( CLoadInfoBlob_ids, it, *ids ) {
        const CBlob_id& id = it->first;
        const CBlob_Info& info = it->second;
        data.push_back(id.GetSat());
        data.push_back(id.GetSubSat());
        data.push_back(id.GetSatKey());
        data.push_back(info.GetContentsMask());
    }
    _ASSERT(data.size() % IDS_SIZE == IDS_HSIZE && data.front() == IDS_MAGIC);
    m_IdCache->Store(GetIdKey(seq_id),
                     0,
                     GetBlob_idsSubkey(),
                     &data.front(),
                     data.size() * sizeof(int));
}


void CCacheWriter::SaveBlobVersion(CReaderRequestResult& /*result*/,
                                   const TBlobId& blob_id,
                                   TBlobVersion version)
{
    if( !m_IdCache ) {
        return;
    }

    m_IdCache->Store(GetBlobKey(blob_id),
                     0,
                     GetBlobVersionSubkey(),
                     &version,
                     sizeof(version));
}


class CCacheBlobStream : public CWriter::CBlobStream
{
public:
    typedef int TVersion;

    CCacheBlobStream(ICache* cache, const string& key,
                     TVersion version, const string& subkey)
        : m_Cache(cache), m_Key(key), m_Version(version), m_Subkey(subkey),
          m_Writer(cache->GetWriteStream(key, version, subkey))
        {
            if ( m_Writer.get() ) {
                m_Stream.reset(new CWStream(m_Writer.get()));
            }
        }
    ~CCacheBlobStream(void)
        {
            if ( m_Stream.get() ) {
                Abort();
            }
        }

    bool CanWrite(void) const
        {
            return m_Stream.get() != 0;
        }
    
    CNcbiOstream& operator*(void)
        {
            _ASSERT(m_Stream.get());
            return *m_Stream;
        }

    void Close(void)
        {
            *m_Stream << flush;
            if ( !*m_Stream ) {
                Abort();
                NCBI_THROW(CLoaderException, eLoaderFailed,
                           "cannot write into cache");
            }
            m_Stream.reset();
            m_Writer.reset();
        }

    void Abort(void)
        {
            m_Stream.reset();
            m_Writer.reset();
            Remove();
        }

    void Remove(void)
        {
            m_Cache->Remove(m_Key, m_Version, m_Subkey);
        }
    
private:
    ICache*             m_Cache;
    string              m_Key;
    TVersion            m_Version;
    string              m_Subkey;
    auto_ptr<IWriter>   m_Writer;
    auto_ptr<CWStream>  m_Stream;
};


CRef<CWriter::CBlobStream>
CCacheWriter::OpenBlobStream(CReaderRequestResult& result,
                             const TBlobId& blob_id,
                             TChunkId chunk_id, 
                             const CProcessor& processor)
{
    if( !m_BlobCache ) {
        return null;
    }

    CLoadLockBlob blob(result, blob_id);
    CRef<CBlobStream> stream
        (new CCacheBlobStream(m_BlobCache, GetBlobKey(blob_id),
                              blob.GetBlobVersion(),
                              GetBlobSubkey(chunk_id)));
    if ( !stream->CanWrite() ) {
        return null;
    }

    WriteProcessorTag(**stream, processor);
    return stream;
}


bool CCacheWriter::CanWrite(EType type) const
{
    return (type == eIdWriter ? m_IdCache : m_BlobCache) != 0;
}


END_SCOPE(objects)


using namespace objects;


/// Class factory for Cache writer
///
/// @internal
///
class CCacheWriterCF :
    public CSimpleClassFactoryImpl<CWriter, CCacheWriter>
{
private:
    typedef CSimpleClassFactoryImpl<CWriter, CCacheWriter> TParent;
public:
    CCacheWriterCF()
        : TParent(NCBI_GBLOADER_WRITER_CACHE_DRIVER_NAME, 0) {}
    ~CCacheWriterCF() {}


    CWriter* 
    CreateInstance(const string& driver  = kEmptyStr,
                   CVersionInfo version = NCBI_INTERFACE_VERSION(CWriter),
                   const TPluginManagerParamTree* params = 0) const
    {
        if ( !driver.empty()  &&  driver != m_DriverName ) 
            return 0;
        
        if ( !version.Match(NCBI_INTERFACE_VERSION(CWriter)) ) {
            return 0;
        }
        auto_ptr<ICache> id_cache(SCacheInfo::CreateCache(params, true));
        auto_ptr<ICache> blob_cache(SCacheInfo::CreateCache(params, false));
        if ( blob_cache.get()  ||  id_cache.get() ) {
            return new CCacheWriter(blob_cache.release(), id_cache.release(),
                                    CCacheWriter::fOwnAll);
        }
        return 0;
    }
};


void NCBI_EntryPoint_CacheWriter(
     CPluginManager<CWriter>::TDriverInfoList&   info_list,
     CPluginManager<CWriter>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CCacheWriterCF>::NCBI_EntryPointImpl(info_list,
                                                             method);
}


void NCBI_EntryPoint_xwriter_cache(
     CPluginManager<CWriter>::TDriverInfoList&   info_list,
     CPluginManager<CWriter>::EEntryPointRequest method)
{
    NCBI_EntryPoint_CacheWriter(info_list, method);
}


void GenBankWriters_Register_Cache(void)
{
    RegisterEntryPoint<CWriter>(NCBI_EntryPoint_CacheWriter);
}


END_NCBI_SCOPE
