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

// TODO: remove the following two includes
# include <objtools/data_loaders/genbank/readers/id1/reader_id1.hpp>
#if defined(HAVE_PUBSEQ_OS)
# include <objtools/data_loaders/genbank/readers/pubseqos/reader_pubseq.hpp>
#endif

#include <objmgr/objmgr_exception.hpp>
#include <objmgr/annot_selector.hpp>

#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/annot_object.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/interfaces.hpp>
#include <corelib/ncbithr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static int x_Request2SeqrefMask(CGBDataLoader::EChoice choice);

//=======================================================================
//   GBLoader sub classes 
//

//=======================================================================
// GBLoader Public interface 
// 

static const char* const DRV_ENV_VAR = "GENBANK_LOADER_METHOD";
static const char* const DEFAULT_DRV_ORDER = "PUBSEQOS:ID1";
static const char* const DRV_PUBSEQOS = "PUBSEQOS";
static const char* const DRV_ID1 = "ID1";


SSeqrefs::SSeqrefs(const CSeq_id_Handle& h)
    : m_Handle(h)
{
}


SSeqrefs::~SSeqrefs(void)
{
}


CGBDataLoader::STSEinfo::STSEinfo()
    : next(0), prev(0), locked(0), tseinfop(0), m_LoadState(eLoadStateNone)
{
}


CGBDataLoader::STSEinfo::~STSEinfo()
{
}


#define TSE(stse) CSeqref::printTSE((stse).key)
#define DUMP(stse) CSeqref::printTSE((stse).key) << \
    " stse=" << &(stse) << \
    " tsei=" << (stse).tseinfop << \
    " tse=" << ((stse).tseinfop? (stse).tseinfop->GetSeq_entryCore().GetPointer(): 0)


// Create driver specified in "env"
/*
CReader* s_CreateReader(string env)
{
#if defined(HAVE_PUBSEQ_OS)
    if (env == DRV_PUBSEQOS) {
        try {
            return new CPubseqReader;
        }
        catch ( exception& e ) {
            GBLOG_POST("CPubseqReader is not available ::" << e.what());
            return 0;
        }
        catch ( ... ) {
            LOG_POST("CPubseqReader:: unable to init ");
            return 0;
        }
    }
#endif
    if (env == DRV_ID1) {
        return new CId1Reader;
    }
    return 0;
}
*/

CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CReader*        driver,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    TRegisterLoaderInfo info;
    string name = GetLoaderNameFromArgs(driver);
    CDataLoader* loader = om.FindDataLoader(name);
    if ( loader ) {
        info.Set(loader, false);
        return info;
    }
    loader = new CGBDataLoader(name, driver);
    CObjectManager::TRegisterLoaderInfo base_info =
        CDataLoader::RegisterInObjectManager(om, name, *loader,
                                             is_default, priority);
    info.Set(base_info.GetLoader(), base_info.IsCreated());
    return info;
}


string CGBDataLoader::GetLoaderNameFromArgs(CReader* /*driver*/)
{
    return "GBLOADER";
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager&        om,
    TReader_PluginManager* plugin_manager,
    EOwnership             take_plugin_manager,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TRegisterLoaderInfo info;
    string name = GetLoaderNameFromArgs(
        plugin_manager,
        take_plugin_manager);
    CDataLoader* loader = om.FindDataLoader(name);
    if ( loader ) {
        info.Set(loader, false);
        return info;
    }
    loader = new CGBDataLoader(name,
                               plugin_manager,
                               take_plugin_manager);
    CObjectManager::TRegisterLoaderInfo base_info =
        CDataLoader::RegisterInObjectManager(om, name, *loader,
                                             is_default, priority);
    info.Set(base_info.GetLoader(), base_info.IsCreated());
    return info;
}


string CGBDataLoader::GetLoaderNameFromArgs(
    TReader_PluginManager* /*plugin_manager*/,
    EOwnership /*take_plugin_manager*/)
{
    return "GBLOADER";
}


CGBDataLoader::CGBDataLoader(const string& loader_name, CReader *driver)
  : CDataLoader(loader_name),
    m_Driver(driver),
    m_ReaderPluginManager(0),
    m_UseListHead(0),
    m_UseListTail(0)
{
    GBLOG_POST( "CGBDataLoader");
    if ( !m_Driver ) {
        x_CreateDriver();
// Commented out by kuznets Dec 03, 2003
/*
        const char* env = ::getenv(DRV_ENV_VAR);
        if (!env) {
            env = DEFAULT_DRV_ORDER; // default drivers' order
        }
        list<string> drivers;
        NStr::Split(env, ":", drivers);
        ITERATE ( list<string>, drv, drivers ) {
            m_Driver = s_CreateReader(*drv);
            if ( m_Driver )
                break;
        }
        if(!m_Driver) {
            NCBI_THROW(CLoaderException, eNoConnection,
                       "Could not create driver: " + string(env));
        }
*/
    }
  
    size_t i = m_Driver->GetParallelLevel();
    m_Locks.m_Pool.SetSize(i<=0?10:i);
    m_Locks.m_SlowTraverseMode=0;
  
    m_TseCount=0;
    m_TseGC_Threshhold = 100;
    m_InvokeGC=false;
    //GBLOG_POST( "CGBDataLoader("<<loader_name<<"::" <<gc_threshold << ")" );
}

CGBDataLoader::CGBDataLoader(const string& loader_name,
                             TReader_PluginManager *plugin_manager,
                             EOwnership  take_plugin_manager)
  : CDataLoader(loader_name),
    m_Driver(0),
    m_ReaderPluginManager(plugin_manager),
    m_OwnReaderPluginManager(take_plugin_manager),
    m_UseListHead(0),
    m_UseListTail(0)
{
    GBLOG_POST( "CGBDataLoader");
    if (!m_ReaderPluginManager) {
        x_CreateReaderPluginManager();
    }
    x_CreateDriver();

    size_t i = m_Driver->GetParallelLevel();
    m_Locks.m_Pool.SetSize(i<=0?10:i);
    m_Locks.m_SlowTraverseMode=0;
  
    m_TseCount=0;
    m_TseGC_Threshhold = 100;
    m_InvokeGC=false;
}

void CGBDataLoader::x_CreateReaderPluginManager(void)
{
    if (m_OwnReaderPluginManager == eTakeOwnership) {
        delete m_ReaderPluginManager;
        m_ReaderPluginManager = 0;
    }

    m_ReaderPluginManager = new TReader_PluginManager;
    m_OwnReaderPluginManager = eTakeOwnership;

    TReader_PluginManager::FNCBI_EntryPoint ep1 = NCBI_EntryPoint_Id1Reader;
    m_ReaderPluginManager->RegisterWithEntryPoint(ep1);

#if defined(HAVE_PUBSEQ_OS)
    TReader_PluginManager::FNCBI_EntryPoint ep2 = NCBI_EntryPoint_Reader_Pubseqos;
    m_ReaderPluginManager->RegisterWithEntryPoint(ep2);
#endif

}


void CGBDataLoader::x_CreateDriver(void)
{
    if (!m_ReaderPluginManager) {
        x_CreateReaderPluginManager();
        _ASSERT(m_ReaderPluginManager);
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
    _ASSERT(m_ReaderPluginManager);

#if defined(HAVE_PUBSEQ_OS)
    if (env == DRV_PUBSEQOS) {
        try {
            return m_ReaderPluginManager->CreateInstance("pubseq_reader");
        }
        catch ( exception& e ) {
            GBLOG_POST("CPubseqReader is not available ::" << e.what());
            return 0;
        }
        catch ( ... ) {
            LOG_POST("CPubseqReader:: unable to init ");
            return 0;
        }
    }
#endif
    if (env == DRV_ID1) {
        try {
            return m_ReaderPluginManager->CreateInstance("id1_reader");
        }
        catch ( exception& e ) {
            LOG_POST("CId1Reader is not available ::" << e.what());
            return 0;
        }
        catch ( ... ) {
            LOG_POST("CId1Reader:: unable to init ");
            return 0;
        }
    }
    return 0;

}


CGBDataLoader::~CGBDataLoader(void)
{
    GBLOG_POST( "~CGBDataLoader");
    CGBLGuard g(m_Locks,"~CGBDataLoader");
    while ( m_UseListHead ) {
        if ( m_UseListHead->tseinfop ) {
            ERR_POST("CGBDataLoader::~CGBDataLoader: TSE not dropped: "<<
                     TSE(*m_UseListHead));
        }
        x_DropTSEinfo(m_UseListHead);
    }
    m_Bs2Sr.clear();
    if (m_OwnReaderPluginManager == eTakeOwnership) {
        delete m_ReaderPluginManager;
    }
}


static const char* const s_ChoiceName[] = {
    "eBlob",
    "eBioseq",
    "eCore",
    "eBioseqCore",
    "eSequence",
    "eFeatures",
    "eGraph",
    "eAlign",
    "eAll",
    "eAnnot",
    "???"
};

static
const size_t kChoiceNameCount = sizeof(s_ChoiceName)/sizeof(s_ChoiceName[0]);


void CGBDataLoader::GetRecords(const CSeq_id_Handle& idh,
                               const EChoice choice)
{
    if ( choice == eAnnot && !idh.IsGi() ) {
        // no external annots available on non-gi ids
        return;
    }
    CMutexGuard guard(m_Locks.m_Lookup);
    x_GetRecords(s_ChoiceName[min(size_t(choice), kChoiceNameCount-1)],
                 idh, x_Request2SeqrefMask(choice));
}


void CGBDataLoader::GetChunk(CTSE_Chunk_Info& chunk_info)
{
    CMutexGuard guard(m_Locks.m_Lookup);
    x_GetChunk(GetTSEinfo(chunk_info.GetTSE_Info()), chunk_info);
}


CConstRef<CSeqref> CGBDataLoader::GetSatSatkey(const CSeq_id& id)
{
    return GetSatSatkey(CSeq_id_Handle::GetHandle(id));
}


CConstRef<CSeqref> CGBDataLoader::GetSatSatkey(const CSeq_id_Handle& idh)
{
    CConstRef<CSeqref> ret;
    {{
        CMutexGuard guard(m_Locks.m_Lookup);
        CRef<SSeqrefs> srs = x_ResolveHandle(idh);
        ITERATE ( SSeqrefs::TSeqrefs, it, srs->m_Sr ) {
            const CSeqref& sr = **it;
            if ( sr.GetFlags() & CSeqref::fHasCore ) {
                ret = &sr;
                break;
            }
        }
    }}
    return ret;
}


void CGBDataLoader::x_GetRecords(const char*
#ifdef DEBUG_SYNC
                                 type_name
#endif
                                 ,
                                 const CSeq_id_Handle& idh,
                                 TMask sr_mask)
{
    GC();

    char s[100];
#ifdef DEBUG_SYNC
    memset(s,0,sizeof(s));
    {
        strstream ss(s,sizeof(s));
        ss << "GetRecords(" << type_name <<")";
    }
#else
    s[0] = 0;
#endif

    CGBLGuard g(m_Locks,s);
    x_GetRecords(idh, sr_mask);
}


#if !defined(_DEBUG)
inline
#endif
void
CGBDataLoader::x_Check(const STSEinfo* _DEBUG_ARG(me))
{
#if defined(_DEBUG)
    unsigned c = 0;
    bool tse_found=false;
    const STSEinfo* tse2 = m_UseListHead;
    const STSEinfo* t1 = 0;
    while ( tse2 ) {
        c++;
        if( tse2 == me )
            tse_found = true;
        t1 = tse2;
        tse2 = tse2->next;
    }
    _ASSERT(t1 == m_UseListTail);
    _ASSERT(m_Sr2TseInfo.size() == m_TseCount ||
            m_Sr2TseInfo.size() == m_TseCount + 1);
    _ASSERT(c  <= m_TseCount);
    //_ASSERT(m_Tse2TseInfo.size() <= m_TseCount);
    if(me) {
        //GBLOG_POST("check tse " << me << " by " << CThread::GetSelf() );
        _ASSERT(tse_found);
    }
#endif
}


CRef<CGBDataLoader::STSEinfo>
CGBDataLoader::GetTSEinfo(const CTSE_Info& tse_info)
{
    return CRef<STSEinfo>(const_cast<STSEinfo*>
                          (dynamic_cast<const STSEinfo*>
                           (tse_info.GetBlobId().GetPointerOrNull())));
}


void CGBDataLoader::DropTSE(const CTSE_Info& tse_info)
{
    CGBLGuard g(m_Locks,"drop_tse");
    //TTse2TSEinfo::iterator it = m_Tse2TseInfo.find(sep);
    //if (it == m_Tse2TseInfo.end()) // oops - apprently already done;
    //    return true;
    CRef<STSEinfo> tse = GetTSEinfo(tse_info);
    if ( !tse )
        return;
    g.Lock(&*tse);
    _ASSERT(tse);
    x_Check(tse);
  
    //m_Tse2TseInfo.erase(it);
    x_DropTSEinfo(tse);
}


CConstRef<CTSE_Info>
CGBDataLoader::ResolveConflict(const CSeq_id_Handle& handle,
                               const TTSE_LockSet& tse_set)
{
    bool         conflict=false;

#ifdef _DEBUG  
    {{
        CNcbiOstrstream s;
        s << "CGBDataLoader::ResolveConflict("<<handle.AsString();
        ITERATE ( TTSE_LockSet, it, tse_set ) {
            CRef<STSEinfo> ti = GetTSEinfo(**it);
            s << ", " << DUMP(*ti);
        }
        _TRACE(string(CNcbiOstrstreamToString(s)));
    }}
#endif

    GBLOG_POST( "ResolveConflict" );
    CGBLGuard g(m_Locks,"ResolveConflict");
    CRef<SSeqrefs> sr;
    CConstRef<CTSE_Info> best;
    {
        int cnt = 5;
        while ( !(sr = x_ResolveHandle(handle))  && cnt>0 )
            cnt --;
        if ( !sr )
            return best;
    }

    ITERATE(TTSE_LockSet, sit, tse_set) {
        CConstRef<CTSE_Info> ti = *sit;
        CRef<STSEinfo> tse = GetTSEinfo(*ti);
        if ( !tse )
            continue;
      
        x_Check(tse);

        g.Lock(&*tse);

        if(tse->mode.test(STSEinfo::eDead) && !ti->IsDead()) {
            GetDataSource()->x_UpdateTSEStatus(const_cast<CTSE_Info&>(*ti),
                                               true);
        }
        if(tse->m_SeqIds.find(handle)!=tse->m_SeqIds.end()) {
            // listed for given TSE
            if(!best) {
                best=ti; conflict=false;
            }
            else if(!ti->IsDead() && best->IsDead()) {
                best=ti; conflict=false;
            }
            else if(ti->IsDead() && best->IsDead()) {
                conflict=true;
            }
            else if(ti->IsDead() && !best->IsDead()) {
            }
            else {
                conflict=true;
                //_ASSERT(ti->IsDead() || best->IsDead());
            }
        }
        g.Unlock(&*tse);
    }

    if ( !best || conflict ) {
        // try harder
        
        best.Reset();conflict=false;
        ITERATE (SSeqrefs::TSeqrefs, srp, sr->m_Sr) {
            TSr2TSEinfo::iterator tsep =
                m_Sr2TseInfo.find((*srp)->GetKeyByTSE());
            if (tsep == m_Sr2TseInfo.end()) continue;
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
    if ( !best ) {
        _TRACE("CGBDataLoader::ResolveConflict("<<handle.AsString()<<
               ") - conflict");
    }
    return best;
}

//=======================================================================
// GBLoader private interface
// 

void CGBDataLoader::x_ExcludeFromDropList(STSEinfo* tse)
{
    _ASSERT(tse);
    _TRACE("x_ExcludeFromDropList("<<DUMP(*tse)<<")");
    STSEinfo* next = tse->next;
    STSEinfo* prev = tse->prev;
    if ( next || prev ) {
        x_Check(tse);
        if ( next ) {
            _ASSERT(tse != m_UseListTail);
            _ASSERT(tse == next->prev);
            next->prev = prev;
        }
        else {
            _ASSERT(tse == m_UseListTail);
            m_UseListTail = prev;
        }
        if ( prev ) {
            _ASSERT(tse != m_UseListHead);
            _ASSERT(tse == prev->next);
            prev->next = next;
        }
        else {
            _ASSERT(tse == m_UseListHead);
            m_UseListHead = next;
        }
        tse->prev = tse->next = 0;
        --m_TseCount;
    }
    else if ( tse == m_UseListHead ) {
        _ASSERT(tse == m_UseListTail);
        x_Check(tse);
        m_UseListHead = m_UseListTail = 0;
        --m_TseCount;
    }
    else {
        _ASSERT(tse != m_UseListTail);
    }
    x_Check();
}


void CGBDataLoader::x_AppendToDropList(STSEinfo* tse)
{
    _ASSERT(tse);
    _TRACE("x_AppendToDropList("<<DUMP(*tse)<<")");
    _ASSERT(m_Sr2TseInfo[tse->key] == tse);
    x_Check();
    _ASSERT(!tse->next && !tse->prev);
    if ( m_UseListTail ) {
        tse->prev = m_UseListTail;
        m_UseListTail->next = tse;
        m_UseListTail = tse;
    }
    else {
        _ASSERT(!m_UseListHead);
        m_UseListHead = m_UseListTail = tse;
    }
    ++m_TseCount;
    x_Check(tse);
}


void CGBDataLoader::x_UpdateDropList(STSEinfo* tse)
{
    _ASSERT(tse);
    // reset LRU links
    if(tse == m_UseListTail) // already the last one
        return;
  
    // Unlink from current place
    x_ExcludeFromDropList(tse);
    x_AppendToDropList(tse);
}


void CGBDataLoader::x_DropTSEinfo(STSEinfo* tse)
{
    if(!tse) return;
    _TRACE( "DropBlob(" << DUMP(*tse) << ")" );
  
    ITERATE(STSEinfo::TSeqids,sih_it,tse->m_SeqIds) {
        m_Bs2Sr.erase(*sih_it);
    }

    x_ExcludeFromDropList(tse);
    m_Sr2TseInfo.erase(tse->key);
}


void CGBDataLoader::GC(void)
{
    //LOG_POST("X_GC "<<m_TseCount<<","<<m_TseGC_Threshhold<<","<< m_InvokeGC);
    // dirty read - but that ok for garbage collector
    if(!m_InvokeGC || m_TseCount==0) return ;
    if(m_TseCount < m_TseGC_Threshhold) {
        if(m_TseCount < 0.5*m_TseGC_Threshhold)
            m_TseGC_Threshhold = (m_TseCount + 3*m_TseGC_Threshhold)/4;
        return;
    }
    GBLOG_POST( "X_GC " << m_TseCount);
    //GetDataSource()->x_CleanupUnusedEntries();

    CGBLGuard g(m_Locks,"GC");
    x_Check();

    unsigned skip=0;
    // scan 10% of least recently used pile before giving up:
    unsigned skip_max = (int)(0.1*m_TseCount + 1);
    STSEinfo* cur_tse = m_UseListHead;
    while ( cur_tse && skip<skip_max) {
        STSEinfo* tse_to_drop = cur_tse;
        cur_tse = cur_tse->next;
        ++skip;
        // fast checks
        if (tse_to_drop->locked) continue;
        if (tse_to_drop->tseinfop && tse_to_drop->tseinfop->Locked()) continue;
        //CRef<CTSE_Info> tse_info(tse_to_drop->tseinfop);

        if ( !tse_to_drop->tseinfop ) {
            // not loaded yet
            if ( tse_to_drop->m_LoadState != STSEinfo::eLoadStateNone ) {
                // no data
                g.Lock(tse_to_drop);
                GBLOG_POST("X_GC:: drop nonexistent tse " << tse_to_drop);
                CRef<STSEinfo> tse_ref(tse_to_drop);
                x_DropTSEinfo(tse_to_drop);
                g.Unlock(tse_to_drop);
                --skip;
            }
            continue;
        }
        //if(m_Tse2TseInfo.find(sep) == m_Tse2TseInfo.end()) continue;
        
        GBLOG_POST("X_GC::DropTSE("<<TSE(*tse_to_drop)<<")");
        //g.Unlock();
        g.Lock(tse_to_drop);
        if( GetDataSource()->DropTSE(*tse_to_drop->tseinfop) ) {
            --skip;
            m_InvokeGC=false;
        }
        g.Unlock(tse_to_drop);
        //g.Lock();
#if defined(NCBI_THREADS)
        unsigned i=0;
        for(cur_tse = m_UseListHead; cur_tse && i<skip; ++i) {
            cur_tse = cur_tse->next;
        }
#endif
    }
    if(m_InvokeGC) { // nothing has been cleaned up
        //assert(m_TseGC_Threshhold<=m_TseCount); // GC entrance condition
        m_TseGC_Threshhold = m_TseCount+2; // do not even try until next load
    } else if(m_TseCount < 0.5*m_TseGC_Threshhold) {
        m_TseGC_Threshhold = (m_TseCount + m_TseGC_Threshhold)/2;
    }
}


CSeqref::TFlags x_Request2SeqrefMask(CGBDataLoader::EChoice choice)
{
    switch(choice) {
    case CGBDataLoader::eBlob:
    case CGBDataLoader::eBioseq:
    case CGBDataLoader::eAll:
        // whole bioseq
        return CSeqref::fHasAllLocal;
    case CGBDataLoader::eCore:
    case CGBDataLoader::eBioseqCore:
        // everything except bioseqs & annotations
        return CSeqref::fHasCore;
    case CGBDataLoader::eSequence:
        // seq data
        return CSeqref::fHasSeqMap|CSeqref::fHasSeqData;
    case CGBDataLoader::eFeatures:
        // SeqFeatures
        return CSeqref::fHasFeatures;
    case CGBDataLoader::eGraph:
        // SeqGraph
        return CSeqref::fHasGraph;
    case CGBDataLoader::eAlign:
        // SeqGraph
        return CSeqref::fHasAlign;
    case CGBDataLoader::eAnnot:
        // all annotations
        return CSeqref::fHasAlign | CSeqref::fHasGraph |
            CSeqref::fHasFeatures | CSeqref::fHasExternal;
    default:
        return 0;
    }
}


void CGBDataLoader::x_GetRecords(const CSeq_id_Handle& sih,
                                 TMask sr_mask)
{
    int attempt_count = 3, attempt;
    for ( attempt = 0; attempt < attempt_count; ++attempt ) {
        CRef<SSeqrefs> sr = x_ResolveHandle(sih);
        _ASSERT(sr);
        try {
            ITERATE ( SSeqrefs::TSeqrefs, srp, sr->m_Sr ) {
                // skip TSE which doesn't contain requested type of info
                if( ((*srp)->GetFlags() & sr_mask) == 0 )
                    continue;
                
                // find TSE info for each seqref
                TSr2TSEinfo::iterator tsep =
                    m_Sr2TseInfo.find((*srp)->GetKeyByTSE());
                CRef<STSEinfo> tse;
                if (tsep != m_Sr2TseInfo.end()) {
                    tse = tsep->second;
                }
                else {
                    tse.Reset(new STSEinfo());
                    tse->seqref = *srp;
                    tse->key = tse->seqref->GetKeyByTSE();
                    m_Sr2TseInfo[tse->key] = tse;
                    x_AppendToDropList(tse.GetPointer());
                    GBLOG_POST("x_GetRecords-newTSE(" << tse << ")");
                }
                
                CGBLGuard g(m_Locks,CGBLGuard::eMain,"x_GetRecords");
                g.Lock(&*tse);
                {{ // make sure we have reverse reference to handle
                    STSEinfo::TSeqids &sid = tse->m_SeqIds;
                    if (sid.find(sih) == sid.end())
                        sid.insert(sih);
                }}
                
                if( !x_NeedMoreData(*tse) )
                    continue;
                // need update
                _ASSERT(tse->m_LoadState != STSEinfo::eLoadStateDone);
                tse->locked++;
                g.Local();

                CReader::TConn conn = m_Locks.m_Pool.Select(&*tse);
                try {
                    x_GetData(tse, conn);
                }
                catch ( CLoaderException& e ) {
                    g.Lock();
                    g.Lock(&*tse);
                    tse->locked--;
                    x_UpdateDropList(&*tse); // move up as just checked
                    switch ( e.GetErrCode() ) {
                    case CLoaderException::eNoConnection:
                        throw;
                    case CLoaderException::ePrivateData:
                        // no need to reconnect
                        // no need to wait more
                        break;
                    case CLoaderException::eNoData:
                        // no need to reconnect
                        throw;
                    default:
                        ERR_POST("GenBank connection failed: Reconnecting...");
                        m_Driver->Reconnect(conn);
                        throw;
                    }
                }
                catch ( ... ) {
                    g.Lock();
                    g.Lock(&*tse);
                    tse->locked--;
                    x_UpdateDropList(&*tse); // move up as just checked
                    ERR_POST("GenBank connection failed: Reconnecting...");
                    m_Driver->Reconnect(conn);
                    throw;
                }
                _ASSERT(!x_NeedMoreData(*tse));
                g.Lock();
                g.Lock(&*tse);
                tse->locked--;
                x_UpdateDropList(&*tse); // move up as just checked

                x_Check(&*tse);
                x_Check();
            }
            // everything is loaded, break the attempt loop
            break;
        }
        catch ( CLoaderException& e ) {
            ERR_POST(e.what());
            if ( e.GetErrCode() == e.eNoConnection ) {
                throw;
            }
        }
        catch ( exception& e ) {
            ERR_POST(e.what());
        }
        catch ( ... ) {
            ERR_POST(CThread::GetSelf()<<":: Data request failed....");
            throw;
        }
        // something is not loaded
        // in case of any error we'll force reloading seqrefs
        
        CGBLGuard g(m_Locks,CGBLGuard::eMain,"x_ResolveHandle");
        g.Lock();
        sr->m_Timer.Reset();
        m_Driver->PurgeSeqrefs(sr->m_Sr, *sih.GetSeqId());
    }
    if ( attempt >= attempt_count ) {
        ERR_POST("CGBLoader:GetData: data request failed: "
                 "exceeded maximum attempts count");
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "Multiple attempts to retrieve data failed");
    }
}


void CGBDataLoader::x_GetChunk(CRef<STSEinfo> tse,
                               CTSE_Chunk_Info& chunk_info)
{
    CGBLGuard g(m_Locks,"x_GetChunk");
    g.Lock(&*tse);
    tse->locked++;
    g.Local();

    bool done = false;
    int try_cnt = 3;
    while( !done && try_cnt-- > 0 ) {
        CReader::TConn conn = m_Locks.m_Pool.Select(&*tse);
        try {
            x_GetChunk(tse, conn, chunk_info);
            done = true;
            break;
        }
        catch ( CLoaderException& e ) {
            if ( e.GetErrCode() == CLoaderException::eNoConnection ) {
                g.Lock();
                g.Lock(&*tse);
                tse->locked--;
                x_UpdateDropList(&*tse); // move up as just checked
                throw;
            }
            LOG_POST(e.what());
            LOG_POST("GenBank connection failed: Reconnecting....");
            m_Driver->Reconnect(conn);
        }
        catch ( const exception &e ) {
            LOG_POST(e.what());
            LOG_POST("GenBank connection failed: Reconnecting....");
            m_Driver->Reconnect(conn);
        }
        catch ( ... ) {
            LOG_POST("GenBank connection failed: Reconnecting....");
            m_Driver->Reconnect(conn);
            g.Lock();
            g.Lock(&*tse);
            tse->locked--;
            x_UpdateDropList(&*tse); // move up as just checked
            throw;
        }
    }
    if ( !done ) {
        ERR_POST("CGBLoader:GetChunk: data request failed: "
                 "exceeded maximum attempts count");
        g.Lock();
        g.Lock(&*tse);
        tse->locked--;
        x_UpdateDropList(&*tse); // move up as just checked
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "Multiple attempts to retrieve data failed");
    }
    g.Lock();
    g.Lock(&*tse);
    x_UpdateDropList(&*tse);
    if( done ) {
        x_Check();
        _ASSERT(tse->tseinfop);
    }
    tse->locked--;
    x_Check(&*tse);
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


CRef<SSeqrefs> CGBDataLoader::x_ResolveHandle(const CSeq_id_Handle& h)
{
    CGBLGuard g(m_Locks,CGBLGuard::eMain,"x_ResolveHandle");

    CRef<SSeqrefs> sr;
    TSeqId2Seqrefs::iterator bsit = m_Bs2Sr.find(h);
    if (bsit == m_Bs2Sr.end() ) {
        sr.Reset(new SSeqrefs(h));
        m_Bs2Sr[h] = sr;
    }
    else {
        sr = bsit->second;
    }

    int key = sr->m_Handle.GetHash();
    g.Lock(key);
    if( !sr->m_Timer.NeedRefresh(m_Timer) )
        return sr;
    g.Local();

    SSeqrefs::TSeqrefs osr;
    bool got = false;

    CConstRef<CSeq_id> seq_id = h.GetSeqId();
    for ( int try_cnt = 3; !got && try_cnt > 0; --try_cnt ) {
        osr.clear();
        CTimerGuard tg(m_Timer);
        CReader::TConn conn = m_Locks.m_Pool.Select(key);
        try {
            m_Driver->ResolveSeq_id(osr, *seq_id, conn);
            got = true;
            break;
        }
        catch ( CLoaderException& e ) {
            if ( e.GetErrCode() == CLoaderException::eNoConnection ) {
                throw;
            }
            LOG_POST(e.what());
        }
        catch ( const exception &e ) {
            LOG_POST(e.what());
        }
        LOG_POST("GenBank connection failed: Reconnecting....");
        m_Driver->Reconnect(conn);
    }
    if ( !got ) {
        ERR_POST("CGBLoader:x_ResolveHandle: Seq-id resolve failed: "
                 "exceeded maximum attempts count");
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "Multiple attempts to resolve Seq-id failed");
    }

    g.Lock(); // will unlock everything and lock lookupMutex again 

    swap(sr->m_Sr, osr);
    sr->m_Timer.Reset(m_Timer);
  
    GBLOG_POST( "ResolveHandle(" << h << ") " << sr->m_Sr.size() );
    ITERATE(SSeqrefs::TSeqrefs, srp, sr->m_Sr) {
        GBLOG_POST( (*srp)->print());
    }
    
    if ( !osr.empty() ) {
        bsit = m_Bs2Sr.find(h);
        // make sure we are not deleted in the unlocked time 
        if (bsit != m_Bs2Sr.end()) {
            SSeqrefs::TSeqrefs& nsr=bsit->second->m_Sr;
          
            // catch dissolving TSE and mark them dead
            //GBLOG_POST( "old seqrefs");
            ITERATE ( SSeqrefs::TSeqrefs, srp, osr ) {
                //(*srp)->print(); cout);
                bool found=false;
                ITERATE ( SSeqrefs::TSeqrefs, nsrp, nsr ) {
                    if( (*srp)->SameTSE(**nsrp) ) {
                        found=true;
                        break;
                    }
                }
                if ( found ) {
                    continue;
                }
                TSr2TSEinfo::iterator tsep =
                    m_Sr2TseInfo.find((*srp)->GetKeyByTSE());
                if (tsep == m_Sr2TseInfo.end()) {
                    continue;
                }
              
                // update TSE info 
                CRef<STSEinfo> tse = tsep->second;
                g.Lock(&*tse);
                bool mark_dead  = tse->mode.test(STSEinfo::eDead);
                if ( mark_dead ) {
                    tse->mode.set(STSEinfo::eDead);
                }
                tse->m_SeqIds.erase(h); // drop h as refewrenced seqid
                g.Unlock(&*tse);
                if ( mark_dead && tse->tseinfop ) {
                    // inform data_source :: make sure to avoid deadlocks
                    tse->tseinfop->SetDead(true);
                }
            }
        }
    }
    return sr;
}

bool CGBDataLoader::x_NeedMoreData(const STSEinfo& tse)
{
    bool need_data=true;
  
    if (tse.m_LoadState==STSEinfo::eLoadStateDone)
        need_data=false;
    if (tse.m_LoadState==STSEinfo::eLoadStatePartial) {
        // split code : check tree for presence of data and
        // return from routine if all data already loaded
        // present;
    }
    return need_data;
}


void CGBDataLoader::x_GetData(CRef<STSEinfo> tse,
                              CReader::TConn conn)
{
    try {
        if ( x_NeedMoreData(*tse) ) {
            GBLOG_POST("GetBlob("<<TSE(*tse)<<") "<<":="<<tse->m_LoadState);
            
            _ASSERT(tse->m_LoadState != STSEinfo::eLoadStateDone);
            if (tse->m_LoadState == STSEinfo::eLoadStateNone) {
                CRef<CTSE_Info> tse_info =
                    m_Driver->GetBlob(*tse->seqref, conn);
                GBLOG_POST("GetBlob("<<TSE(*tse)<<")");
                m_InvokeGC=true;
                _ASSERT(tse_info);
                tse->m_LoadState   = STSEinfo::eLoadStateDone;
                tse_info->SetBlobId(tse);
                GetDataSource()->AddTSE(tse_info);
                tse->tseinfop = tse_info.GetPointer();
                _TRACE("GetBlob("<<DUMP(*tse)<<") - whole blob retrieved");
            }
        }
    }
    catch ( CLoaderException& e ) {
        if ( e.GetErrCode() == e.ePrivateData ) {
            LOG_POST("GI("<<tse->seqref->GetGi()<<") is private");
            tse->m_LoadState = STSEinfo::eLoadStateDone;
        }
        else if ( e.GetErrCode() == e.eNoData ) {
            if ( (tse->seqref->GetFlags() & CSeqref::fPossible) ) {
                tse->m_LoadState   = STSEinfo::eLoadStateDone;
            }
            else {
                LOG_POST("ERROR: can not retrive sequence: "<<TSE(*tse));
                throw;
            }
        }
        else {
            throw;
        }
    }
    _ASSERT(!x_NeedMoreData(*tse));
}


void CGBDataLoader::x_GetChunk(CRef<STSEinfo> tse,
                               CReader::TConn conn,
                               CTSE_Chunk_Info& chunk_info)
{
    GBLOG_POST("GetChunk("<<TSE(*tse)<<") "<<":="<<chunk_info.GetChunkId());
  
    _ASSERT(tse->m_LoadState == STSEinfo::eLoadStateDone);
    
    CRef<CTSE_Info> tse_info =
        m_Driver->GetBlob(*tse->seqref, conn, &chunk_info);
    GBLOG_POST("GetChunk("<<TSE(*tse)<<")");
    m_InvokeGC=true;
    _ASSERT(!tse_info);
    _TRACE("GetChunk("<<DUMP(*tse)<<") - chunk retrieved");
}


const CSeqref& CGBDataLoader::GetSeqref(const CTSE_Info& tse_info)
{
    if ( &tse_info.GetDataSource() != GetDataSource() ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "not mine TSE");
    }
    CRef<CGBDataLoader::STSEinfo> info(GetTSEinfo(tse_info));
    return *info->seqref;
}


void
CGBDataLoader::DebugDump(CDebugDumpContext ddc, unsigned int /*depth*/) const
{
    ddc.SetFrame("CGBLoader");
    // CObject::DebugDump( ddc, depth);
    DebugDumpValue(ddc,"m_TseCount", m_TseCount);
    DebugDumpValue(ddc,"m_TseGC_Threshhold", m_TseGC_Threshhold);
}


END_SCOPE(objects)
END_NCBI_SCOPE



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.109  2004/07/26 14:13:32  grichenk
* RegisterInObjectManager() return structure instead of pointer.
* Added CObjectManager methods to manipuilate loaders.
*
* Revision 1.108  2004/07/21 15:51:26  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.107  2004/06/08 19:18:40  grichenk
* Removed CSafeStaticRef from seq_id_mapper.hpp
*
* Revision 1.106  2004/05/21 21:42:52  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.105  2004/03/16 15:47:29  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.104  2004/02/17 21:17:53  vasilche
* Fixed 'non-const reference to temporary' warning.
*
* Revision 1.103  2004/02/04 20:59:46  ucko
* Fix the PubSeq entry point name to something that actually exists....
*
* Revision 1.102  2004/02/04 17:47:40  kuznets
* Fixed naming of entry points
*
* Revision 1.101  2004/01/22 20:10:35  vasilche
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
* Revision 1.100  2004/01/13 21:52:06  vasilche
* Resurrected new version.
*
* Revision 1.4  2004/01/13 16:55:55  vasilche
* CReader, CSeqref and some more classes moved from xobjmgr to separate lib.
* Headers moved from include/objmgr to include/objtools/data_loaders/genbank.
*
* Revision 1.3  2003/12/30 22:14:41  vasilche
* Updated genbank loader and readers plugins.
*
* Revision 1.98  2003/12/30 19:51:24  vasilche
* Implemented CGBDataLoader::GetSatSatkey() method.
*
* Revision 1.97  2003/12/30 17:43:17  vasilche
* Fixed warning about unused variable.
*
* Revision 1.96  2003/12/30 16:41:19  vasilche
* Removed warning about unused variable.
*
* Revision 1.95  2003/12/19 19:49:21  vasilche
* Use direct Seq-id -> sat/satkey resolution without intermediate gi.
*
* Revision 1.94  2003/12/03 15:14:05  kuznets
* CReader management re-written to use plugin manager
*
* Revision 1.93  2003/12/02 23:17:34  vasilche
* Fixed exception in ID1 reader when invalid Seq-id is supplied.
*
* Revision 1.92  2003/12/01 23:42:29  vasilche
* Temporary fix for segfault in genbank data loader in multithreaded applications.
*
* Revision 1.91  2003/11/26 17:55:58  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.90  2003/10/27 19:28:02  vasilche
* Removed debug message.
*
* Revision 1.89  2003/10/27 18:50:49  vasilche
* Detect 'private' blobs in ID1 reader.
* Avoid reconnecting after ID1 server replied with error packet.
*
* Revision 1.88  2003/10/27 15:05:41  vasilche
* Added correct recovery of cached ID1 loader if gi->sat/satkey cache is invalid.
* Added recognition of ID1 error codes: private, etc.
* Some formatting of old code.
*
* Revision 1.87  2003/10/22 16:12:37  vasilche
* Added CLoaderException::eNoConnection.
* Added check for 'fail' state of ID1 connection stream.
* CLoaderException::eNoConnection will be rethrown from CGBLoader.
*
* Revision 1.86  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.85  2003/09/30 16:22:02  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.84  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.83  2003/08/27 14:25:22  vasilche
* Simplified CCmpTSE class.
*
* Revision 1.82  2003/08/14 20:05:19  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.81  2003/07/24 19:28:09  vasilche
* Implemented SNP split for ID1 loader.
*
* Revision 1.80  2003/07/22 22:01:43  vasilche
* Removed use of HAVE_LIBDL.
*
* Revision 1.79  2003/07/17 22:51:31  vasilche
* Fixed unused variables warnings.
*
* Revision 1.78  2003/07/17 20:07:56  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.77  2003/06/24 14:25:18  vasilche
* Removed obsolete CTSE_Guard class.
* Used separate mutexes for bioseq and annot maps.
*
* Revision 1.76  2003/06/11 14:54:06  vasilche
* Fixed wrong error message in CGBDataLoader destructor when
* some data requests were failed.
*
* Revision 1.75  2003/06/10 19:01:07  vasilche
* Fixed loader methods string.
*
* Revision 1.74  2003/06/10 15:25:33  vasilche
* Changed wrong _ASSERT to _VERIFY
*
* Revision 1.73  2003/06/02 16:06:37  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.72  2003/05/20 21:13:02  vasilche
* Fixed ambiguity on MSVC.
*
* Revision 1.71  2003/05/20 18:27:29  vasilche
* Fixed ambiguity on MSVC.
*
* Revision 1.70  2003/05/20 16:18:42  vasilche
* Fixed compilation errors on GCC.
*
* Revision 1.69  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.68  2003/05/13 20:27:05  vasilche
* Added lost SAT SATKEY info in error message.
*
* Revision 1.67  2003/05/13 20:21:10  vasilche
* *** empty log message ***
*
* Revision 1.66  2003/05/13 20:14:40  vasilche
* Catching exceptions and reconnection were moved from readers to genbank loader.
*
* Revision 1.65  2003/05/13 18:32:29  vasilche
* Fixed use of GBLOG_POST() macro.
*
* Revision 1.64  2003/05/12 19:18:29  vasilche
* Fixed locking of object manager classes in multi-threaded application.
*
* Revision 1.63  2003/05/12 18:26:08  vasilche
* Removed buggy _ASSERT() on reconnection.
*
* Revision 1.62  2003/05/06 16:52:28  vasilche
* Try to reconnect to genbank on any exception.
*
* Revision 1.61  2003/04/29 19:51:13  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.60  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.59  2003/04/18 17:38:01  grichenk
* Use GENBANK_LOADER_METHOD env. variable to specify GB readers.
* Default is "PUBSEQOS:ID1".
*
* Revision 1.58  2003/04/15 16:32:29  dicuccio
* Added include for I_DriverContext from DBAPI library - avoids concerning
* warning about deletion of unknwon type.
*
* Revision 1.57  2003/04/15 15:30:15  vasilche
* Added include <memory> when needed.
* Removed buggy buffer in printing methods.
* Removed unnecessary include of stream_util.hpp.
*
* Revision 1.56  2003/04/15 14:24:08  vasilche
* Changed CReader interface to not to use fake streams.
*
* Revision 1.55  2003/03/21 19:22:51  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.54  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.53  2003/03/05 20:54:41  vasilche
* Commented out wrong assert().
*
* Revision 1.52  2003/03/03 20:34:51  vasilche
* Added NCBI_THREADS macro - it's opposite to NCBI_NO_THREADS.
* Avoid using _REENTRANT macro - use NCBI_THREADS instead.
*
* Revision 1.51  2003/03/01 23:07:42  kimelman
* bugfix: MTsafe
*
* Revision 1.50  2003/03/01 22:27:57  kimelman
* performance fixes
*
* Revision 1.49  2003/02/27 21:58:26  vasilche
* Fixed performance of Object Manager's garbage collector.
*
* Revision 1.48  2003/02/26 18:03:31  vasilche
* Added some error check.
* Fixed formatting.
*
* Revision 1.47  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.46  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.45  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.44  2002/12/26 20:53:02  dicuccio
* Moved tse_info.hpp -> include/ tree.  Minor tweaks to relieve compiler
* warnings in MSVC.
*
* Revision 1.43  2002/11/08 19:43:35  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.42  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.41  2002/09/09 16:12:43  dicuccio
* Fixed minor typo ("conneciton" -> "connection").
*
* Revision 1.40  2002/07/24 20:30:27  ucko
* Move CTimerGuard out to file scope to fix a MIPSpro compiler core dump.
*
* Revision 1.39  2002/07/22 22:53:24  kimelman
* exception handling fixed: 2level mutexing moved to Guard class + added
* handling of confidential data.
*
* Revision 1.38  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.37  2002/05/14 20:06:26  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.36  2002/05/10 16:44:32  kimelman
* tuning to allow pubseq enable build
*
* Revision 1.35  2002/05/08 22:23:48  kimelman
* MT fixes
*
* Revision 1.34  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.33  2002/05/03 21:28:10  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.32  2002/04/30 18:56:54  gouriano
* added multithreading-related initialization
*
* Revision 1.31  2002/04/28 03:36:47  vakatov
* Temporarily turn off R1.30(b) unconditionally -- until it is buildable
*
* Revision 1.30  2002/04/26 16:32:23  kimelman
* a) turn on GC b) turn on PubSeq where sybase is available
*
* Revision 1.29  2002/04/12 22:54:28  kimelman
* pubseq_reader auto call commented per Denis request
*
* Revision 1.28  2002/04/12 21:10:33  kimelman
* traps for coredumps
*
* Revision 1.27  2002/04/11 18:45:35  ucko
* Pull in extra headers to make KCC happy.
*
* Revision 1.26  2002/04/10 22:47:56  kimelman
* added pubseq_reader as default one
*
* Revision 1.25  2002/04/09 19:04:23  kimelman
* make gcc happy
*
* Revision 1.24  2002/04/09 18:48:15  kimelman
* portability bugfixes: to compile on IRIX, sparc gcc
*
* Revision 1.23  2002/04/05 23:47:18  kimelman
* playing around tests
*
* Revision 1.22  2002/04/04 01:35:35  kimelman
* more MT tests
*
* Revision 1.21  2002/04/02 17:27:00  gouriano
* bugfix: skip test for yet unregistered data
*
* Revision 1.20  2002/04/02 16:27:20  gouriano
* memory leak
*
* Revision 1.19  2002/04/02 16:02:31  kimelman
* MT testing
*
* Revision 1.18  2002/03/30 19:37:06  kimelman
* gbloader MT test
*
* Revision 1.17  2002/03/29 02:47:04  kimelman
* gbloader: MT scalability fixes
*
* Revision 1.16  2002/03/27 20:23:50  butanaev
* Added connection pool.
*
* Revision 1.15  2002/03/26 23:31:08  gouriano
* memory leaks and garbage collector fix
*
* Revision 1.14  2002/03/26 15:39:24  kimelman
* GC fixes
*
* Revision 1.13  2002/03/25 17:49:12  kimelman
* ID1 failure handling
*
* Revision 1.12  2002/03/25 15:44:46  kimelman
* proper logging and exception handling
*
* Revision 1.11  2002/03/22 18:56:05  kimelman
* GC list fix
*
* Revision 1.10  2002/03/22 18:51:18  kimelman
* stream WS skipping fix
*
* Revision 1.9  2002/03/22 18:15:47  grichenk
* Unset "skipws" flag in binary stream
*
* Revision 1.8  2002/03/21 23:16:32  kimelman
* GC bugfixes
*
* Revision 1.7  2002/03/21 21:39:48  grichenk
* garbage collector bugfix
*
* Revision 1.6  2002/03/21 19:14:53  kimelman
* GB related bugfixes
*
* Revision 1.5  2002/03/21 01:34:53  kimelman
* gbloader related bugfixes
*
* Revision 1.4  2002/03/20 21:24:59  gouriano
* *** empty log message ***
*
* Revision 1.3  2002/03/20 19:06:30  kimelman
* bugfixes
*
* Revision 1.2  2002/03/20 17:03:24  gouriano
* minor changes to make it compilable on MS Windows
*
* Revision 1.1  2002/03/20 04:50:13  kimelman
* GB loader added
*
* ===========================================================================
*/
