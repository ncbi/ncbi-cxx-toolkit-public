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
*  Author: Michael Kimelman, Eugene Vasilchenko
*
*  File Description: GenBank Data loader
*
*/

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/reader.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>

// TODO: remove the following two includes
# include <objtools/data_loaders/genbank/readers/id1/reader_id1.hpp>
# include <objtools/data_loaders/genbank/readers/id2/reader_id2.hpp>
#if defined(HAVE_PUBSEQ_OS)
# include <objtools/data_loaders/genbank/readers/pubseqos/reader_pubseq.hpp>
#endif

#include <objtools/data_loaders/genbank/seqref.hpp>

#include <objmgr/objmgr_exception.hpp>

#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/data_loader_factory.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/interfaces.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <util/plugin_manager_store.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//=======================================================================
//   GBLoader sub classes 
//

//=======================================================================
// GBLoader Public interface 
// 

static const char* const DRV_ENV_VAR = "GENBANK_LOADER_METHOD";

#if defined(HAVE_PUBSEQ_OS)
static const char* const DEFAULT_DRV_ORDER = "PUBSEQOS:ID1";
#else
static const char* const DEFAULT_DRV_ORDER = "ID1";
#endif
//static const char* const DRV_PUBSEQOS = "PUBSEQOS";
//static const char* const DRV_ID1 = "ID1";
//static const char* const DRV_ID2 = "ID2";

class CGBReaderRequestResult : public CReaderRequestResult
{
    typedef CReaderRequestResult TParent;
public:
    CGBReaderRequestResult(CGBDataLoader* loader);
    ~CGBReaderRequestResult(void);

    CGBDataLoader& GetLoader(void)
        {
            return *m_Loader;
        }
    
    virtual TConn GetConn(void);
    virtual void ReleaseConn(void);
    virtual CRef<CLoadInfoSeq_ids> GetInfoSeq_ids(const string& id);
    virtual CRef<CLoadInfoSeq_ids> GetInfoSeq_ids(const CSeq_id_Handle& id);
    virtual CRef<CLoadInfoBlob_ids> GetInfoBlob_ids(const CSeq_id_Handle& id);
    virtual CTSE_LoadLock GetTSE_LoadLock(const TKeyBlob& blob_id);
    virtual operator CInitMutexPool&(void);

    CInitMutexPool& GetMutexPool(void);

    friend class CGBDataLoader;

private:
    CRef<CGBDataLoader> m_Loader;
    TConn               m_Conn;
};


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CReader*        driver,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    TReaderPtrMaker maker(driver);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CGBDataLoader::GetLoaderNameFromArgs(CReader* /*driver*/)
{
    return "GBLOADER";
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string&   reader_name,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    TReaderNameMaker maker(reader_name);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CGBDataLoader::GetLoaderNameFromArgs(const string& /*reader_name*/)
{
    return "GBLOADER";
}


CGBDataLoader::CGBDataLoader(const string& loader_name, CReader *driver)
  : CDataLoader(loader_name),
    m_Driver(driver)
{
    GBLOG_POST( "CGBDataLoader");
    if ( !m_Driver ) {
        x_CreateDriver();
    }
    if ( m_Driver ) {
        m_ConnMutexes.SetSize(m_Driver->GetParallelLevel());
    }
}


CGBDataLoader::CGBDataLoader(const string& loader_name,
                             const string& reader_name)
  : CDataLoader(loader_name),
    m_Driver(0)
{
    GBLOG_POST( "CGBDataLoader");
    x_CreateDriver(reader_name);
    if ( m_Driver ) {
        m_ConnMutexes.SetSize(m_Driver->GetParallelLevel());
    }
}


void CGBDataLoader::x_CreateDriver(const string& driver_name)
{
    if ( !driver_name.empty() ) {
        m_Driver = x_CreateReader(driver_name);
        if ( m_Driver ) {
            return;
        }
    }

    const char* env = ::getenv(DRV_ENV_VAR);
    if (!env) {
        env = DEFAULT_DRV_ORDER; // default drivers' order
    }
    list<string> drivers;
    NStr::Split(env, ":", drivers);
    ITERATE ( list<string>, drv, drivers ) {
        m_Driver = x_CreateReader(*drv);
        if ( m_Driver )
            break;
    }

    if (!m_Driver) {
        NCBI_THROW(CLoaderException, eNoConnection,
                   "Could not create driver: " + string(env));
    }
}


CReader* CGBDataLoader::x_CreateReader(const string& env)
{
    typedef CPluginManager<CReader> TReader_PluginManager;
    // TReader_PluginManager ReaderPluginManager;
    CRef<TReader_PluginManager> ReaderPluginManager;
    string rpm_name = CInterfaceVersion<CReader>::GetName();

    if ( CPluginManagerStore::HasObject(rpm_name) ) {
        ReaderPluginManager.Reset(
            dynamic_cast<TReader_PluginManager*>(
            CPluginManagerStore::GetObject(rpm_name)));
    }
    else {
        ReaderPluginManager.Reset(new TReader_PluginManager);
        CPluginManagerStore::PutObject(rpm_name, &*ReaderPluginManager);

#define REGISTER_READER_ENTRY_POINTS
#if defined(REGISTER_READER_ENTRY_POINTS)
        ReaderPluginManager->RegisterWithEntryPoint(NCBI_EntryPoint_Id1Reader);
        ReaderPluginManager->RegisterWithEntryPoint(NCBI_EntryPoint_Id2Reader);

#if defined(HAVE_PUBSEQ_OS)
        ReaderPluginManager->RegisterWithEntryPoint(NCBI_EntryPoint_ReaderPubseqos);
#endif // HAVE_PUBSEQ_OS

#endif // REGISTER_READER_ENTRY_POINTS
    }

    string reader_name = env;
    NStr::ToLower(reader_name);
    try {
        return ReaderPluginManager->CreateInstance(reader_name);
    }
    catch ( exception& e ) {
        LOG_POST(env << " reader is not available ::" << e.what());
    }
    catch ( ... ) {
        LOG_POST(env << " reader unable to init ");
    }

    return 0;
}


CGBDataLoader::~CGBDataLoader(void)
{
    GBLOG_POST( "~CGBDataLoader");
    m_Driver.Reset();
    m_ConnMutexes.Reset();

    //m_LoadMapBlob.clear();
    m_LoadMapBlob_ids.clear();
    m_LoadMapSeq_ids.clear();
}


CConstRef<CSeqref> CGBDataLoader::GetSatSatkey(const CSeq_id_Handle& sih)
{
    TBlobId id = GetBlobId(sih);
    if ( id ) {
        CBlob_id blob_id = GetBlobId(id);
        return ConstRef(new CSeqref(0,
                                    blob_id.GetSat(),
                                    blob_id.GetSatKey()));
    }
    return CConstRef<CSeqref>();
}


CConstRef<CSeqref> CGBDataLoader::GetSatSatkey(const CSeq_id& id)
{
    return GetSatSatkey(CSeq_id_Handle::GetHandle(id));
}


CDataLoader::TBlobId CGBDataLoader::GetBlobId(const CSeq_id_Handle& sih)
{
    for ( int attempt_count = 0; attempt_count < 3; ++attempt_count ) {
        CGBReaderRequestResult result(this);
        CLoadLockBlob_ids blobs(result, sih);
        if ( !blobs ) {
            m_Driver->ResolveSeq_id(result, sih);
            if ( !blobs ) {
                continue; // retry
            }
        }
        ITERATE ( CLoadInfoBlob_ids, it, *blobs ) {
            const CBlob_Info& info = it->second;
            if ( info.GetContentsMask() & fBlobHasCore ) {
                return TBlobId(&it->first);
            }
        }
        return TBlobId();
    }
    NCBI_THROW(CLoaderException, eOtherError,
               "cannot resolve Seq-id "+sih.AsString()+": too many attempts");
}


CDataLoader::TBlobVersion CGBDataLoader::GetBlobVersion(const TBlobId& id)
{
    const CBlob_id& blob_id = dynamic_cast<const CBlob_id&>(*id);
    for ( int attempt_count = 0; attempt_count < 3; ++attempt_count ) {
        CGBReaderRequestResult result(this);
        return m_Driver->GetBlobVersion(result, blob_id);
    }
    NCBI_THROW(CLoaderException, eOtherError,
               "cannot get blob version "+blob_id.ToString()+
               ": too many attempts");
}


TTSE_Lock CGBDataLoader::ResolveConflict(const CSeq_id_Handle& handle,
                                         const TTSE_LockSet& tse_set)
{
    TTSE_Lock best;
    bool         conflict=false;

    CGBReaderRequestResult result(this);

    ITERATE(TTSE_LockSet, sit, tse_set) {
        const CTSE_Info& tse = **sit;

        CLoadLockBlob blob(result, GetBlobId(tse));
        _ASSERT(blob);

        /*
        if ( tse.m_SeqIds.find(handle) == tse->m_SeqIds.end() ) {
            continue;
        }
        */

        // listed for given TSE
        if ( !best ) {
            best = *sit; conflict=false;
        }
        else if( !tse.IsDead() && best->IsDead() ) {
            best = *sit; conflict=false;
        }
        else if( tse.IsDead() && best->IsDead() ) {
            conflict=true;
        }
        else if( tse.IsDead() && !best->IsDead() ) {
        }
        else {
            conflict=true;
            //_ASSERT(tse.IsDead() || best->IsDead());
        }
    }

/*
    if ( !best || conflict ) {
        // try harder
        best.Reset();
        conflict=false;

        CReaderRequestResultBlob_ids blobs = result.GetResultBlob_ids(handle);
        _ASSERT(blobs);

        ITERATE ( CLoadInfoBlob_ids, it, *blobs ) {
            TBlob_InfoMap::iterator tsep =
                m_Blob_InfoMap.find((*srp)->GetKeyByTSE());
            if (tsep == m_Blob_InfoMap.end()) continue;
            ITERATE(TTSE_LockSet, sit, tse_set) {
                CConstRef<CTSE_Info> ti = *sit;
                //TTse2TSEinfo::iterator it =
                //    m_Tse2TseInfo.find(&ti->GetSeq_entry());
                //if(it==m_Tse2TseInfo.end()) continue;
                CRef<STSEinfo> tinfo = GetTSEinfo(*ti);
                if ( !tinfo )
                    continue;

                if(tinfo==tsep->second) {
                    if ( !best )
                        best=ti;
                    else if (ti != best)
                        conflict=true;
                }
            }
        }
        if(conflict)
            best.Reset();
    }
*/
    if ( !best ) {
        _TRACE("CGBDataLoader::ResolveConflict("<<handle.AsString()<<"): "
               "conflict");
    }
    return best;
}


//=======================================================================
// GBLoader private interface
// 
void CGBDataLoader::GC(void)
{
}


TBlobContentsMask CGBDataLoader::x_MakeContentMask(EChoice choice) const
{
    switch(choice) {
    case CGBDataLoader::eBlob:
    case CGBDataLoader::eBioseq:
        // whole bioseq
        return fBlobHasAllLocal;
    case CGBDataLoader::eCore:
    case CGBDataLoader::eBioseqCore:
        // everything except bioseqs & annotations
        return fBlobHasCore;
    case CGBDataLoader::eSequence:
        // seq data
        return fBlobHasSeqMap | fBlobHasSeqData;
    case CGBDataLoader::eFeatures:
        // SeqFeatures
        return fBlobHasFeatures;
    case CGBDataLoader::eGraph:
        // SeqGraph
        return fBlobHasGraph;
    case CGBDataLoader::eAlign:
        // SeqGraph
        return fBlobHasAlign;
    case CGBDataLoader::eAnnot:
        // all internal annotations
        return fBlobHasAlign | fBlobHasGraph | fBlobHasFeatures;
    case CGBDataLoader::eExtFeatures:
    case CGBDataLoader::eExtGraph:
    case CGBDataLoader::eExtAlign:
    case CGBDataLoader::eExtAnnot:
        // external annotations
        return fBlobHasExternal;
    case CGBDataLoader::eAll:
        // whole bioseq
        return fBlobHasAllLocal | fBlobHasExternal;
    default:
        return 0;
    }
}


TBlobContentsMask
CGBDataLoader::x_MakeContentMask(const SRequestDetails& details) const
{
    TBlobContentsMask mask = 0;
    if ( details.m_NeedSeqMap.NotEmpty() ) {
        mask |= fBlobHasSeqMap;
    }
    if ( details.m_NeedSeqData.NotEmpty() ) {
        mask |= fBlobHasSeqData;
    }
    switch ( DetailsToChoice(details.m_NeedInternalAnnots) ) {
    case eFeatures:
        mask |= fBlobHasFeatures;
        break;
    case eAlign:
        mask |= fBlobHasAlign;
        break;
    case eGraph:
        mask |= fBlobHasGraph;
        break;
    case eAnnot:
        mask |= fBlobHasAlign | fBlobHasGraph | fBlobHasFeatures;
        break;
    default:
        break;
    }
    if ( !details.m_NeedExternalAnnots.empty() ) {
        mask |= fBlobHasExternal;
    }
    return mask;
}


CDataLoader::TTSE_LockSet
CGBDataLoader::GetRecords(const CSeq_id_Handle& sih, const EChoice choice)
{
    return x_GetRecords(sih, x_MakeContentMask(choice));
}


CDataLoader::TTSE_LockSet
CGBDataLoader::GetRecords(const CSeq_id_Handle& sih,
                          const SRequestDetails& details)
{
    return x_GetRecords(sih, x_MakeContentMask(details));
}


CDataLoader::TTSE_LockSet
CGBDataLoader::x_GetRecords(const CSeq_id_Handle& sih, TBlobContentsMask mask)
{
    TTSE_LockSet locks;
    
    if ( mask == 0 ) {
        return locks;
    }
    
    if ( (mask & fBlobHasAllLocal) == 0 && !sih.IsGi() ) {
        // no external features on non-gi
        return locks;
    }
    
    GC();

    for ( int attempt_count = 0; attempt_count < 3; ++attempt_count ) {
        CGBReaderRequestResult result(this);
        CLoadLockBlob_ids blobs(result, sih);
        if ( !blobs ) {
            m_Driver->LoadBlobs(result, sih, mask);
            if ( !blobs ) {
                continue; // retry
            }
        }
        bool done = true;
        ITERATE ( CLoadInfoBlob_ids, it, *blobs ) {
            const CBlob_Info& info = it->second;
            if( (info.GetContentsMask() & mask) == 0 ) {
                continue;
            }
            CLoadLockBlob blob(result, it->first);
            if ( !blob.IsLoaded() ) {
                m_Driver->LoadBlob(result, blobs, it);
                if ( !blob.IsLoaded() ) {
                    done = false;
                }
            }
        }
        result.SaveLocksTo(locks);
        if ( done ) {
            return locks;
        }
    }
    NCBI_THROW(CLoaderException, eOtherError,
               "cannot get records for Seq-id "+sih.AsString()+
               ": too many attempts");
}


void CGBDataLoader::GetChunk(TChunk chunk)
{
    for ( int retry = 0; chunk->NotLoaded() && retry < 3; ++retry ) {
        CGBReaderRequestResult result(this);
        m_Driver->LoadChunk(result,
                            GetBlobId(chunk->GetTSE_Info()),
                            chunk->GetChunkId());
    }
    if ( chunk->NotLoaded() ) {
        NCBI_THROW(CLoaderException, eOtherError,
                   "cannot load chunk : too many attempts");
    }
}


void CGBDataLoader::GetChunks(const TChunkSet& chunks)
{
    CDataLoader::GetChunks(chunks);
}


class CTimerGuard
{
    CTimer *t;
    bool    calibrating;
public:
    CTimerGuard(CTimer& x)
        : t(&x), calibrating(x.NeedCalibration())
        {
            if ( calibrating ) {
                t->Start();
            }
        }
    ~CTimerGuard(void)
        {
            if ( calibrating ) {
                t->Stop();
            }
        }
};


CBlob_id CGBDataLoader::GetBlobId(const TBlobId& blob_id) const
{
    return dynamic_cast<const CBlob_id&>(*blob_id);
}


CBlob_id CGBDataLoader::GetBlobId(const CTSE_Info& tse_info) const
{
    if ( &tse_info.GetDataSource() != GetDataSource() ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "not mine TSE");
    }
    return GetBlobId(tse_info.GetBlobId());
}


void CGBDataLoader::AllocateConn(CGBReaderRequestResult& result)
{
    TConn conn;
#ifdef NCBI_NO_THREADS
    conn = 0;
#else
    static bool single_conn =
        CReader::s_GetEnvFlag("GENBANK_SINGLE_CONN", false);
    if ( single_conn ) {
        conn = 0;
    }
    else {
        TThreadSystemID thread_id = 0;
        CThread::GetSystemID(&thread_id);
        conn = TConn((size_t)thread_id % m_ConnMutexes.GetSize());
    }
    //ERR_POST("AllocateConn() thread: "<<thread_id<<" conn: "<<conn);
    m_ConnMutexes[conn].Lock();

#endif
    result.m_Conn = conn;
}


void CGBDataLoader::FreeConn(CGBReaderRequestResult& result)
{
    TConn conn = result.m_Conn;
    result.m_Conn = -1;
#ifdef NCBI_NO_THREADS
#else
    m_ConnMutexes[conn].Unlock();
#endif
}


void CGBDataLoader::DropTSE(CRef<CTSE_Info> tse_info)
{
    //TWriteLockGuard guard(m_LoadMap_Lock);
    //m_LoadMapBlob.erase(GetBlob_id(*tse_info));
}


CRef<CLoadInfoSeq_ids>
CGBDataLoader::GetLoadInfoSeq_ids(const string& key)
{
    {{
        TReadLockGuard guard(m_LoadMap_Lock);
        TLoadMapSeq_ids::iterator iter = m_LoadMapSeq_ids.find(key);
        if ( iter != m_LoadMapSeq_ids.end() ) {
            return iter->second;
        }
    }}
    {{
        TWriteLockGuard guard(m_LoadMap_Lock);
        CRef<CLoadInfoSeq_ids>& ret = m_LoadMapSeq_ids[key];
        if ( !ret ) {
            ret = new CLoadInfoSeq_ids();
        }
        return ret;
    }}
}


CRef<CLoadInfoSeq_ids>
CGBDataLoader::GetLoadInfoSeq_ids(const CSeq_id_Handle& key)
{
    {{
        TReadLockGuard guard(m_LoadMap_Lock);
        TLoadMapSeq_ids2::iterator iter = m_LoadMapSeq_ids2.find(key);
        if ( iter != m_LoadMapSeq_ids2.end() ) {
            return iter->second;
        }
    }}
    {{
        TWriteLockGuard guard(m_LoadMap_Lock);
        CRef<CLoadInfoSeq_ids>& ret = m_LoadMapSeq_ids2[key];
        if ( !ret ) {
            ret = new CLoadInfoSeq_ids();
        }
        return ret;
    }}
}


CRef<CLoadInfoBlob_ids>
CGBDataLoader::GetLoadInfoBlob_ids(const CSeq_id_Handle& key)
{
    {{
        TReadLockGuard guard(m_LoadMap_Lock);
        TLoadMapBlob_ids::iterator iter = m_LoadMapBlob_ids.find(key);
        if ( iter != m_LoadMapBlob_ids.end() ) {
            return iter->second;
        }
    }}
    {{
        TWriteLockGuard guard(m_LoadMap_Lock);
        CRef<CLoadInfoBlob_ids>& ret = m_LoadMapBlob_ids[key];
        if ( !ret ) {
            ret = new CLoadInfoBlob_ids(key);
        }
        return ret;
    }}
}

/*
CRef<CLoadInfoBlob>
CGBDataLoader::GetLoadInfoBlob(const CBlob_id& key)
{
    {{
        TReadLockGuard guard(m_LoadMap_Lock);
        TLoadMapBlob::iterator iter = m_LoadMapBlob.find(key);
        if ( iter != m_LoadMapBlob.end() ) {
            return iter->second;
        }
    }}
    {{
        TWriteLockGuard guard(m_LoadMap_Lock);
        CRef<CLoadInfoBlob>& ret = m_LoadMapBlob[key];
        if ( !ret ) {
            ret = new CLoadInfoBlob(key);
        }
        return ret;
    }}
}
*/

CGBReaderRequestResult::CGBReaderRequestResult(CGBDataLoader* loader)
    : m_Loader(loader), m_Conn(-1)
{
}


CGBReaderRequestResult::~CGBReaderRequestResult(void)
{
    ReleaseConn();
}


void CGBReaderRequestResult::ReleaseConn(void)
{
    if ( m_Conn >= 0 ) {
        GetLoader().FreeConn(*this);
        _ASSERT(m_Conn < 0);
    }
}


CReaderRequestResult::TConn CGBReaderRequestResult::GetConn(void)
{
    if ( m_Conn < 0 ) {
        GetLoader().AllocateConn(*this);
        _ASSERT(m_Conn >= 0);
    }
    return m_Conn;
}


CRef<CLoadInfoSeq_ids>
CGBReaderRequestResult::GetInfoSeq_ids(const string& key)
{
    return GetLoader().GetLoadInfoSeq_ids(key);
}


CRef<CLoadInfoSeq_ids>
CGBReaderRequestResult::GetInfoSeq_ids(const CSeq_id_Handle& key)
{
    return GetLoader().GetLoadInfoSeq_ids(key);
}


CRef<CLoadInfoBlob_ids>
CGBReaderRequestResult::GetInfoBlob_ids(const CSeq_id_Handle& key)
{
    return GetLoader().GetLoadInfoBlob_ids(key);
}


CTSE_LoadLock CGBReaderRequestResult::GetTSE_LoadLock(const TKeyBlob& blob_id)
{
    CConstRef<CObject> id(new TKeyBlob(blob_id));
    return GetLoader().GetDataSource()->GetTSE_LoadLock(id);
}


CInitMutexPool& CGBReaderRequestResult::GetMutexPool(void)
{
    //GetConn(); // allocate connection
    return GetLoader().m_MutexPool;
}


CGBReaderRequestResult::operator CInitMutexPool&(void)
{
    return GetMutexPool();
}


bool CGBDataLoader::LessBlobId(const TBlobId& id1,
                                const TBlobId& id2) const
{
    const CBlob_id& bid1 = dynamic_cast<const CBlob_id&>(*id1);
    const CBlob_id& bid2 = dynamic_cast<const CBlob_id&>(*id2);
    return bid1 < bid2;
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

const string kDataLoader_GB_DriverName("genbank");

class CGB_DataLoaderCF : public CDataLoaderFactory
{
public:
    CGB_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_GB_DriverName) {}
    virtual ~CGB_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CGB_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CGBDataLoader::RegisterInObjectManager(om).GetLoader();
    }
    // Parse params, select constructor
    const string& reader_ptr_str =
        GetParam(GetDriverName(), params,
                 kCFParam_GB_ReaderPtr, false, "0");
    CReader* reader = dynamic_cast<CReader*>(
        static_cast<CObject*>(
            const_cast<void*>(NStr::StringToPtr(reader_ptr_str))));
    if ( reader ) {
        return CGBDataLoader::RegisterInObjectManager(
            om,
            reader,
            GetIsDefault(params),
            GetPriority(params)).GetLoader();
    }
    // Try to use reader name
    const string& reader_name =
        GetParam(GetDriverName(), params,
                 kCFParam_GB_ReaderName, false, kEmptyStr);
    if ( !reader_name.empty() ) {
        return CGBDataLoader::RegisterInObjectManager(
            om,
            reader_name,
            GetIsDefault(params),
            GetPriority(params)).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CGBDataLoader::RegisterInObjectManager(
        om,
        0, // no reader
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_DataLoader_GB(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CGB_DataLoaderCF>::NCBI_EntryPointImpl(info_list, method);
}

void NCBI_EntryPoint_xloader_genbank(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_GB(info_list, method);
}


END_NCBI_SCOPE
