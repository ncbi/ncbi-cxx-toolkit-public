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
#include <corelib/ncbi_config_value.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/gbloader_params.h>
#include <objtools/data_loaders/genbank/dispatcher.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>

#include <objtools/data_loaders/genbank/reader_interface.hpp>
#include <objtools/data_loaders/genbank/writer_interface.hpp>

// TODO: remove the following includes
#define REGISTER_READER_ENTRY_POINTS 1
#ifdef REGISTER_READER_ENTRY_POINTS
# include <objtools/data_loaders/genbank/readers/readers.hpp>
#endif

#include <objtools/data_loaders/genbank/seqref.hpp>

#include <objmgr/objmgr_exception.hpp>

#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/data_loader_factory.hpp>

#include <objects/seqloc/Seq_id.hpp>

#include <corelib/ncbithr.hpp>
#include <corelib/ncbiapp.hpp>

#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//=======================================================================
//   GBLoader sub classes 
//

//=======================================================================
// GBLoader Public interface 
// 

#if defined(HAVE_PUBSEQ_OS)
static const char* const DEFAULT_DRV_ORDER = "PUBSEQOS:ID1";
#else
static const char* const DEFAULT_DRV_ORDER = "ID1";
#endif

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
    
    //virtual TConn GetConn(void);
    //virtual void ReleaseConn(void);
    virtual CRef<CLoadInfoSeq_ids> GetInfoSeq_ids(const string& id);
    virtual CRef<CLoadInfoSeq_ids> GetInfoSeq_ids(const CSeq_id_Handle& id);
    virtual CRef<CLoadInfoBlob_ids> GetInfoBlob_ids(const CSeq_id_Handle& id);
    virtual CTSE_LoadLock GetTSE_LoadLock(const TKeyBlob& blob_id);
    virtual operator CInitMutexPool&(void) { return GetMutexPool(); }

    CInitMutexPool& GetMutexPool(void) { return m_Loader->m_MutexPool; }

    friend class CGBDataLoader;

private:
    CRef<CGBDataLoader> m_Loader;
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


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const TParamTree& params,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    TParamMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CGBDataLoader::GetLoaderNameFromArgs(const TParamTree& /* params */)
{
    return "GBLOADER";
}


CGBDataLoader::CGBDataLoader(const string& loader_name, CReader *reader)
  : CDataLoader(loader_name)
{
    GBLOG_POST( "CGBDataLoader");
    x_CreateDriver(reader);
}


CGBDataLoader::CGBDataLoader(const string& loader_name,
                             const string& reader_name)
  : CDataLoader(loader_name)
{
    GBLOG_POST( "CGBDataLoader");
    x_CreateDriver(reader_name);
}


CGBDataLoader::CGBDataLoader(const string&     loader_name,
                             const TParamTree& params)
  : CDataLoader(loader_name)
{
    GBLOG_POST( "CGBDataLoader");
    x_CreateDriver(GetLoaderParams(&params));
}


CGBDataLoader::~CGBDataLoader(void)
{
    GBLOG_POST( "~CGBDataLoader");

    m_LoadMapBlob_ids.clear();
    m_LoadMapSeq_ids.clear();
}


const CGBDataLoader::TParamTree*
CGBDataLoader::GetParamsSubnode(const TParamTree* params,
                                const string& subnode_name)
{
    const TParamTree* subnode = 0;
    if ( params ) {
        if ( NStr::CompareNocase(params->GetId(), subnode_name) == 0 ) {
            subnode = params;
        }
        else {
            subnode = params->FindSubNode(subnode_name);
        }
    }
    return subnode;
}


CGBDataLoader::TParamTree*
CGBDataLoader::GetParamsSubnode(TParamTree* params,
                                const string& subnode_name)
{
    _ASSERT(params);
    TParamTree* subnode = 0;
    if ( NStr::CompareNocase(params->GetId(), subnode_name) == 0 ) {
        subnode = params;
    }
    else {
        subnode = const_cast<TParamTree*>(params->FindSubNode(subnode_name));
        if ( !subnode ) {
            subnode = params->AddNode(subnode_name, kEmptyStr);
        }
    }
    return subnode;
}


const CGBDataLoader::TParamTree*
CGBDataLoader::GetLoaderParams(const TParamTree* params)
{
    return GetParamsSubnode(params, NCBI_GBLOADER_DRIVER_NAME);
}


const CGBDataLoader::TParamTree*
CGBDataLoader::GetReaderParams(const TParamTree* params,
                               const string& reader_name)
{
    return GetParamsSubnode(GetLoaderParams(params), reader_name);
}


CGBDataLoader::TParamTree*
CGBDataLoader::GetLoaderParams(TParamTree* params)
{
    return GetParamsSubnode(params, NCBI_GBLOADER_DRIVER_NAME);
}


CGBDataLoader::TParamTree*
CGBDataLoader::GetReaderParams(TParamTree* params,
                               const string& reader_name)
{
    return GetParamsSubnode(GetLoaderParams(params), reader_name);
}


void CGBDataLoader::SetParam(TParamTree* params,
                             const string& param_name,
                             const string& param_value)
{
    TParamTree* subnode =
        const_cast<TParamTree*>(params->FindSubNode(param_name));
    if ( !subnode ) {
        params->AddNode(param_name, param_value);
    }
    else {
        subnode->SetValue(param_value);
    }
}


string CGBDataLoader::GetParam(const TParamTree* params,
                               const string& param_name)
{
    if ( params ) {
        TParamTree* subnode =
            const_cast<TParamTree*>(params->FindSubNode(param_name));
        if ( subnode ) {
            return subnode->GetValue();
        }
    }
    return kEmptyStr;
}


void CGBDataLoader::x_CreateDriver(CReader* reader)
{
    if ( reader ) {
        m_Dispatcher = new CReadDispatcher;
        m_Dispatcher->InsertReader(1, Ref(reader));
    }
    else {
        x_CreateDriver(kEmptyStr);
    }
}


void CGBDataLoader::x_CreateDriver(const string& reader_name)
{
    auto_ptr<TParamTree> app_params;
    CNcbiApplication* app = CNcbiApplication::Instance();
    if ( app ) {
        app_params.reset(CConfig::ConvertRegToTree(app->GetConfig()));
    }
    else if ( !reader_name.empty() ) {
        app_params.reset(new TParamTree);
    }
    if ( !reader_name.empty() ) {
        SetParam(GetLoaderParams(app_params.get()),
                 NCBI_GBLOADER_PARAM_READER_NAME,
                 reader_name);
    }
    x_CreateDriver(app_params.get());
}


void CGBDataLoader::x_CreateDriver(const TParamTree* params)
{
    params = GetLoaderParams(params);
    m_Dispatcher = new CReadDispatcher;

    if ( x_CreateReaders(params) ) {
        x_CreateWriters(params);
    }
}


bool CGBDataLoader::x_CreateReaders(const TParamTree* params)
{
    string str;
    if ( str.empty() ) {
        // try config first
        static string env_reader = GetConfigString("GENBANK",
                                                   "LOADER_METHOD",
                                                   0);
        str = env_reader;
    }
    if ( str.empty() ) {
        str = GetParam(params, NCBI_GBLOADER_PARAM_READER_NAME);
    }
    if ( str.empty() ) {
        // fall back default reader list
        str = DEFAULT_DRV_ORDER;
    }
    NStr::ToLower(str);
    return x_CreateReaders(str, params);
}


void CGBDataLoader::x_CreateWriters(const TParamTree* params)
{
    string str = GetParam(params, NCBI_GBLOADER_PARAM_WRITER_NAME);
    NStr::ToLower(str);
    x_CreateWriters(str, params);
}


bool CGBDataLoader::x_CreateReaders(const string& str,
                                    const TParamTree* params)
{
    vector<string> str_list;
    NStr::Tokenize(str, ";", str_list, NStr::eNoMergeDelims);
    bool need_writer = false;
    for ( size_t i = 0; i < str_list.size(); ++i ) {
        CRef<CReader> reader(x_CreateReader(str_list[i], params));
        if( reader ) {
            if ( i > 0 ) {
                need_writer = true;
            }
            m_Dispatcher->InsertReader(i, reader);
        }
    }
    return need_writer;
}


void CGBDataLoader::x_CreateWriters(const string& str,
                                    const TParamTree* params)
{
    vector<string> str_list;
    NStr::Tokenize(str, ";", str_list, NStr::eNoMergeDelims);
    for ( size_t i = 0; i < str_list.size(); ++i ) {
        CRef<CWriter> writer(x_CreateWriter(str_list[i], params));
        if( writer ) {
            m_Dispatcher->InsertWriter(i, writer);
        }
    }
}


CRef<CPluginManager<CReader> > CGBDataLoader::x_GetReaderManager(void)
{
    typedef CPluginManagerStore::CPMMaker<CReader> TManagerStore;
    
    bool created = false;
    CRef<TReaderManager> manager(TManagerStore::Get(&created));
    _ASSERT(manager);

    if ( created ) {
#ifdef REGISTER_READER_ENTRY_POINTS
        GenBankReaders_Register_Id1();
        GenBankReaders_Register_Id2();
        GenBankReaders_Register_Cache();
# ifdef HAVE_PUBSEQ_OS
        GenBankReaders_Register_Pubseq();
# endif
#endif
    }

    return manager;
}


CRef<CPluginManager<CWriter> > CGBDataLoader::x_GetWriterManager(void)
{
    typedef CPluginManagerStore::CPMMaker<CWriter> TManagerStore;

    bool created = false;
    CRef<TWriterManager> manager(TManagerStore::Get(&created));
    _ASSERT(manager);

    if ( created ) {
#ifdef REGISTER_READER_ENTRY_POINTS
        GenBankWriters_Register_Cache();
#endif
    }
    
    return manager;
}


CReader* CGBDataLoader::x_CreateReader(const string& names,
                                       const TParamTree* params)
{
    try {
        return x_GetReaderManager()->CreateInstanceFromList(params, names);
    }
    catch ( exception& e ) {
        LOG_POST(names << " readers are not available ::" << e.what());
    }
    catch ( ... ) {
        LOG_POST(names << " reader unable to init ");
    }
    return 0;
}


CWriter* CGBDataLoader::x_CreateWriter(const string& names,
                                       const TParamTree* params)
{
    try {
        return x_GetWriterManager()->CreateInstanceFromList(params, names);
    }
    catch ( exception& e ) {
        LOG_POST(names << " writers are not available ::" << e.what());
    }
    catch ( ... ) {
        LOG_POST(names << " writer unable to init ");
    }
    return 0;
}


CConstRef<CSeqref> CGBDataLoader::GetSatSatkey(const CSeq_id_Handle& sih)
{
    TBlobId id = GetBlobId(sih);
    if ( id ) {
        CBlob_id blob_id = GetBlobId(id);
        return ConstRef(new CSeqref(0, blob_id.GetSat(), blob_id.GetSatKey()));
    }
    return CConstRef<CSeqref>();
}


CConstRef<CSeqref> CGBDataLoader::GetSatSatkey(const CSeq_id& id)
{
    return GetSatSatkey(CSeq_id_Handle::GetHandle(id));
}


CDataLoader::TBlobId CGBDataLoader::GetBlobId(const CSeq_id_Handle& sih)
{
    CGBReaderRequestResult result(this);
    CLoadLockBlob_ids blobs(result, sih);
    if ( !blobs.IsLoaded() ) {
        CLoadLockSeq_ids ids(result, sih);
        if ( ids.IsLoaded() &&
             (ids->GetState() & CBioseq_Handle::fState_no_data) ) {
            blobs->SetState(ids->GetState());
            blobs.SetLoaded();
        }
        else {
            m_Dispatcher->LoadSeq_idBlob_ids(result, sih);
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


void CGBDataLoader::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    CGBReaderRequestResult result(this);
    CLoadLockSeq_ids seq_ids(result, idh);
    if ( !seq_ids.IsLoaded() ) {
        m_Dispatcher->LoadSeq_idSeq_ids(result, idh);
    }
    ids = seq_ids->m_Seq_ids;
}


CDataLoader::TBlobVersion CGBDataLoader::GetBlobVersion(const TBlobId& id)
{
    const CBlob_id& blob_id = dynamic_cast<const CBlob_id&>(*id);
    CGBReaderRequestResult result(this);
    CLoadLockBlob blob(result, blob_id);
    if ( !blob.IsSetBlobVersion() ) {
        m_Dispatcher->LoadBlobVersion(result, blob_id);
    }
    return blob.GetBlobVersion();
}


CDataLoader::TTSE_Lock
CGBDataLoader::ResolveConflict(const CSeq_id_Handle& handle,
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
        return fBlobHasIntFeat;
    case CGBDataLoader::eGraph:
        // SeqGraph
        return fBlobHasIntGraph;
    case CGBDataLoader::eAlign:
        // SeqGraph
        return fBlobHasIntAlign;
    case CGBDataLoader::eAnnot:
        // all internal annotations
        return fBlobHasIntAnnot;
    case CGBDataLoader::eExtFeatures:
        return fBlobHasExtFeat;
    case CGBDataLoader::eExtGraph:
        return fBlobHasExtGraph;
    case CGBDataLoader::eExtAlign:
        return fBlobHasExtAlign;
    case CGBDataLoader::eExtAnnot:
        // external annotations
        return fBlobHasExtAnnot;
    case CGBDataLoader::eOrphanAnnot:
        // orphan annotations
        return fBlobHasOrphanAnnot;
    case CGBDataLoader::eAll:
        // everything
        return fBlobHasAll;
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
    if ( details.m_AnnotBlobType != SRequestDetails::fAnnotBlobNone ) {
        TBlobContentsMask annots = 0;
        switch ( DetailsToChoice(details.m_NeedAnnots) ) {
        case eFeatures:
            annots |= fBlobHasIntFeat;
            break;
        case eAlign:
            annots |= fBlobHasIntAlign;
            break;
        case eGraph:
            annots |= fBlobHasIntGraph;
            break;
        case eAnnot:
            annots |= fBlobHasIntAnnot;
            break;
        default:
            break;
        }
        if ( details.m_AnnotBlobType & SRequestDetails::fAnnotBlobInternal ) {
            mask |= annots;
        }
        if ( details.m_AnnotBlobType & SRequestDetails::fAnnotBlobExternal ) {
            mask |= (annots << 1);
        }
        if ( details.m_AnnotBlobType & SRequestDetails::fAnnotBlobOrphan ) {
            mask |= (annots << 2);
        }
    }
    return mask;
}


CDataLoader::TTSE_LockSet
CGBDataLoader::GetRecords(const CSeq_id_Handle& sih, const EChoice choice)
{
    return x_GetRecords(sih, x_MakeContentMask(choice));
}


CDataLoader::TTSE_LockSet
CGBDataLoader::GetDetailedRecords(const CSeq_id_Handle& sih,
                                  const SRequestDetails& details)
{
    return x_GetRecords(sih, x_MakeContentMask(details));
}


bool CGBDataLoader::CanGetBlobById(void) const
{
    return true;
}


CDataLoader::TTSE_Lock
CGBDataLoader::GetBlobById(const TBlobId& id)
{
    CBlob_id blob_id = GetBlobId(id);

    CGBReaderRequestResult result(this);
    CLoadLockBlob blob(result, blob_id);
    if ( !blob.IsLoaded() ) 
        m_Dispatcher->LoadBlob(result, blob_id);

    return blob;
}


namespace {
    struct SBetterId
    {
        int GetScore(const CSeq_id_Handle& id1) const
            {
                if ( id1.IsGi() ) {
                    return 100;
                }
                if ( !id1 ) {
                    return -1;
                }
                CConstRef<CSeq_id> seq_id = id1.GetSeqId();
                const CTextseq_id* text_id = seq_id->GetTextseq_Id();
                if ( text_id ) {
                    int score;
                    if ( text_id->IsSetAccession() ) {
                        if ( text_id->IsSetVersion() ) {
                            score = 99;
                        }
                        else {
                            score = 50;
                        }
                    }
                    else {
                        score = 0;
                    }
                    return score;
                }
                if ( seq_id->IsGeneral() ) {
                    return 10;
                }
                if ( seq_id->IsLocal() ) {
                    return 0;
                }
                return 1;
            }
        bool operator()(const CSeq_id_Handle& id1,
                        const CSeq_id_Handle& id2) const
            {
                int score1 = GetScore(id1);
                int score2 = GetScore(id2);
                if ( score1 != score2 ) {
                    return score1 > score2;
                }
                return id1 < id2;
            }
    };
}


CDataLoader::TTSE_LockSet
CGBDataLoader::GetExternalRecords(const CBioseq_Info& bioseq)
{
    TTSE_LockSet ret;
    TIds ids = bioseq.GetId();
    sort(ids.begin(), ids.end(), SBetterId());
    ITERATE ( TIds, it, ids ) {
        if ( GetBlobId(*it) ) {
            // correct id is found
            TTSE_LockSet ret2 = GetRecords(*it, eExtAnnot);
            ret.swap(ret2);
            break;
        }
    }
    return ret;
}


CDataLoader::TTSE_LockSet
CGBDataLoader::x_GetRecords(const CSeq_id_Handle& sih, TBlobContentsMask mask)
{
    TTSE_LockSet locks;
    
    if ( mask == 0 ) {
        return locks;
    }
    
    if ( (mask & ~fBlobHasOrphanAnnot) == 0 ) {
        // no orphan annotations in GenBank
        return locks;
    }
    
    GC();

    CGBReaderRequestResult result(this);
    CLoadLockBlob_ids blobs(result, sih);
    if ( !blobs.IsLoaded() ) {
        CLoadLockSeq_ids ids(result, sih);
        if ( ids.IsLoaded() &&
             (ids->GetState() & CBioseq_Handle::fState_no_data) ) {
            blobs->SetState(ids->GetState());
            blobs.SetLoaded();
        }
        else {
            m_Dispatcher->LoadBlobs(result, sih, mask);
        }
    }
    _ASSERT(blobs.IsLoaded());
    
    if ((blobs->GetState() & CBioseq_Handle::fState_no_data) != 0) {
        NCBI_THROW2(CBlobStateException, eBlobStateError,
                    "blob state error", blobs->GetState());
    }
    
    ITERATE ( CLoadInfoBlob_ids, it, *blobs ) {
        const CBlob_Info& info = it->second;
        if ( info.GetContentsMask() & mask ) {
            CLoadLockBlob blob(result, it->first);
            if ( !blob.IsLoaded() ) {
                m_Dispatcher->LoadBlob(result, it->first);
            }
            _ASSERT(blob.IsLoaded());
            if ((blob.GetBlobState() & CBioseq_Handle::fState_no_data) != 0) {
                NCBI_THROW2(CBlobStateException, eBlobStateError,
                    "blob state error", blob.GetBlobState());
            }
        }
        result.SaveLocksTo(locks);
    }
    result.SaveLocksTo(locks);
    return locks;
}


void CGBDataLoader::GetChunk(TChunk chunk)
{
    CGBReaderRequestResult result(this);
    m_Dispatcher->LoadChunk(result,
                            GetBlobId(chunk->GetBlobId()),
                            chunk->GetChunkId());
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



void CGBDataLoader::DropTSE(CRef<CTSE_Info> /* tse_info */)
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


CGBReaderRequestResult::CGBReaderRequestResult(CGBDataLoader* loader)
    : m_Loader(loader)
{
}


CGBReaderRequestResult::~CGBReaderRequestResult(void)
{
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


bool CGBDataLoader::LessBlobId(const TBlobId& id1, const TBlobId& id2) const
{
    const CBlob_id& bid1 = dynamic_cast<const CBlob_id&>(*id1);
    const CBlob_id& bid2 = dynamic_cast<const CBlob_id&>(*id2);
    return bid1 < bid2;
}


string CGBDataLoader::BlobIdToString(const TBlobId& id) const
{
    const CBlob_id& bid = dynamic_cast<const CBlob_id&>(*id);
    return bid.ToString();
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_GenBank(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_DataLoader_GB);
}


class CGB_DataLoaderCF : public CDataLoaderFactory
{
public:
    CGB_DataLoaderCF(void)
        : CDataLoaderFactory(NCBI_GBLOADER_DRIVER_NAME) {}
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
    if ( params ) {
        // Let the loader detect driver from params
        return CGBDataLoader::RegisterInObjectManager(
            om,
            *params,
            GetIsDefault(params),
            GetPriority(params)).GetLoader();
    }
    return CGBDataLoader::RegisterInObjectManager(
        om,
        0, // no driver - try to autodetect
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_DataLoader_GB(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CGB_DataLoaderCF>::NCBI_EntryPointImpl(info_list,
                                                               method);
}

void NCBI_EntryPoint_xloader_genbank(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_GB(info_list, method);
}


END_NCBI_SCOPE
