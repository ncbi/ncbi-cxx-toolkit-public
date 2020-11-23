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
#include <corelib/ncbi_param.hpp>
#include <objtools/data_loaders/genbank/gbnative.hpp>
#include <objtools/data_loaders/genbank/gbloader_params.h>
#include <objtools/data_loaders/genbank/impl/dispatcher.hpp>
#include <objtools/data_loaders/genbank/impl/request_result.hpp>
#include <objtools/data_loaders/genbank/impl/wgsmaster.hpp>

#include <objtools/data_loaders/genbank/reader_interface.hpp>
#include <objtools/data_loaders/genbank/writer_interface.hpp>

// TODO: remove the following includes
#define REGISTER_READER_ENTRY_POINTS 1
#ifdef REGISTER_READER_ENTRY_POINTS
# include <objtools/data_loaders/genbank/readers.hpp>
#endif

#include <objtools/data_loaders/genbank/seqref.hpp>
#include <objtools/error_codes.hpp>

#include <objmgr/objmgr_exception.hpp>

#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <objmgr/annot_selector.hpp>

#include <objects/seqloc/Seq_id.hpp>

#include <corelib/ncbithr.hpp>
#include <corelib/ncbiapp.hpp>

#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_GBLoader

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//=======================================================================
//   GBLoader sub classes
//

//=======================================================================
// GBLoader Public interface
//


NCBI_PARAM_DEF_EX(string, GENBANK, LOADER_METHOD, "",
                  eParam_NoThread, GENBANK_LOADER_METHOD);
typedef NCBI_PARAM_TYPE(GENBANK, LOADER_METHOD) TGenbankLoaderMethod;

NCBI_PARAM_DECL(string, GENBANK, READER_NAME);
NCBI_PARAM_DEF_EX(string, GENBANK, READER_NAME, "",
                  eParam_NoThread, GENBANK_READER_NAME);
typedef NCBI_PARAM_TYPE(GENBANK, READER_NAME) TGenbankReaderName;

NCBI_PARAM_DECL(string, GENBANK, WRITER_NAME);
NCBI_PARAM_DEF_EX(string, GENBANK, WRITER_NAME, "",
                  eParam_NoThread, GENBANK_WRITER_NAME);
typedef NCBI_PARAM_TYPE(GENBANK, WRITER_NAME) TGenbankWriterName;

#if defined(HAVE_PUBSEQ_OS)
static const char* const DEFAULT_DRV_ORDER = "ID2:PUBSEQOS:ID1";
#else
static const char* const DEFAULT_DRV_ORDER = "ID2:ID1";
#endif

#define GBLOADER_NAME "GBLOADER"
#define GBLOADER_HUP_NAME "GBLOADER-HUP"

#define DEFAULT_ID_GC_SIZE 10000
#define DEFAULT_ID_EXPIRATION_TIMEOUT 2*3600 // 2 hours

class CGBReaderRequestResult : public CReaderRequestResult
{
    typedef CReaderRequestResult TParent;
public:
    CGBReaderRequestResult(CGBDataLoader_Native* loader,
                           const CSeq_id_Handle& requested_id);
    ~CGBReaderRequestResult(void);

    CGBDataLoader_Native& GetLoader(void)
        {
            return *m_Loader;
        }
    virtual CGBDataLoader_Native* GetLoaderPtr(void);

    //virtual TConn GetConn(void);
    //virtual void ReleaseConn(void);
    virtual CTSE_LoadLock GetTSE_LoadLock(const TKeyBlob& blob_id);
    virtual CTSE_LoadLock GetTSE_LoadLockIfLoaded(const TKeyBlob& blob_id);
    virtual void GetLoadedBlob_ids(const CSeq_id_Handle& idh,
                                   TLoadedBlob_ids& blob_ids) const;

    virtual operator CInitMutexPool&(void) { return GetMutexPool(); }

    CInitMutexPool& GetMutexPool(void) { return m_Loader->m_MutexPool; }

    virtual TExpirationTime GetIdExpirationTimeout(GBL::EExpirationType type) const;

    virtual bool GetAddWGSMasterDescr(void) const;

    virtual EGBErrorAction GetPTISErrorAction(void) const;
    
    friend class CGBDataLoader_Native;

private:
    CRef<CGBDataLoader_Native> m_Loader;
};


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader_Native::ConvertRegInfo(const TGBMaker::TRegisterInfo& info)
{
    TRegisterLoaderInfo ret;
    ret.Set(info.GetLoader(), info.IsCreated());
    return ret;
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader_Native::RegisterInObjectManager(
    CObjectManager& om,
    CReader*        reader_ptr,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    CGBLoaderParams params(reader_ptr);
    TGBMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return ConvertRegInfo(maker.GetRegisterInfo());
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader_Native::RegisterInObjectManager(
    CObjectManager& om,
    const string&   reader_name,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    CGBLoaderParams params(reader_name);
    TGBMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return ConvertRegInfo(maker.GetRegisterInfo());
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader_Native::RegisterInObjectManager(
    CObjectManager& om,
    EIncludeHUP     include_hup,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    return RegisterInObjectManager(om, include_hup, NcbiEmptyString, is_default,
                                   priority);
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader_Native::RegisterInObjectManager(
    CObjectManager& om,
    EIncludeHUP     /*include_hup*/,
    const string& web_cookie,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    CGBLoaderParams params("PUBSEQOS2:PUBSEQOS");
    params.SetHUPIncluded(true, web_cookie);
    TGBMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return ConvertRegInfo(maker.GetRegisterInfo());
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader_Native::RegisterInObjectManager(
    CObjectManager& om,
    const string&   reader_name,
    EIncludeHUP     include_hup,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    return RegisterInObjectManager(om, reader_name, include_hup, NcbiEmptyString,
                                   is_default, priority);
}

CGBDataLoader::TRegisterLoaderInfo CGBDataLoader_Native::RegisterInObjectManager(
    CObjectManager& om,
    const string&   reader_name,
    EIncludeHUP     /*include_hup*/,
    const string& web_cookie,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    CGBLoaderParams params(reader_name);
    params.SetHUPIncluded(true, web_cookie);
    TGBMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return ConvertRegInfo(maker.GetRegisterInfo());
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader_Native::RegisterInObjectManager(
    CObjectManager& om,
    const TParamTree& param_tree,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    CGBLoaderParams params(&param_tree);
    TGBMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return ConvertRegInfo(maker.GetRegisterInfo());
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader_Native::RegisterInObjectManager(
    CObjectManager& om,
    const CGBLoaderParams& params,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    TGBMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return ConvertRegInfo(maker.GetRegisterInfo());
}


CGBDataLoader_Native::CGBDataLoader_Native(const string& loader_name,
                             const CGBLoaderParams& params)
    : CGBDataLoader(loader_name, params)
{
    GBLOG_POST_X(1, "CGBDataLoader_Native");
    x_CreateDriver(params);
}


CGBDataLoader_Native::~CGBDataLoader_Native(void)
{
    GBLOG_POST_X(2, "~CGBDataLoader_Native");
    CloseCache();
}


CObjectManager::TPriority CGBDataLoader_Native::GetDefaultPriority(void) const
{
    CObjectManager::TPriority priority = CDataLoader::GetDefaultPriority();
    if ( HasHUPIncluded() ) {
        // HUP data loader has lower priority by default
        priority += 1;
    }
    return priority;
}


void CGBDataLoader_Native::x_CreateDriver(const CGBLoaderParams& params)
{
    auto_ptr<TParamTree> app_params;
    const TParamTree* gb_params = 0;
    if ( params.GetParamTree() ) {
        gb_params = GetLoaderParams(params.GetParamTree());
    }
    else {
        CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
        if ( app ) {
            app_params.reset(CConfig::ConvertRegToTree(app->GetConfig()));
            gb_params = GetLoaderParams(app_params.get());
        }
    }
    
    size_t queue_size = DEFAULT_ID_GC_SIZE;
    if ( gb_params ) {
        try {
            string param = GetParam(gb_params, NCBI_GBLOADER_PARAM_ID_GC_SIZE);
            if ( !param.empty() ) {
                queue_size = NStr::StringToUInt(param);
            }
        }
        catch ( CException& /*ignored*/ ) {
        }
    }

    m_IdExpirationTimeout = DEFAULT_ID_EXPIRATION_TIMEOUT;
    if ( gb_params ) {
        string param =
            GetParam(gb_params, NCBI_GBLOADER_PARAM_ID_EXPIRATION_TIMEOUT);
        if ( !param.empty() ) {
            try {
                Uint4 timeout = NStr::StringToNumeric<Uint4>(param);
                if ( timeout > 0 ) {
                    m_IdExpirationTimeout = timeout;
                }
            }
            catch ( CException& exc ) {
                NCBI_RETHROW_FMT(exc, CLoaderException, eBadConfig,
                                 "Bad value of parameter "
                                 NCBI_GBLOADER_PARAM_ID_EXPIRATION_TIMEOUT
                                 ": \""<<param<<"\"");
            }
        }
    }
    m_AlwaysLoadExternal = false;
    if ( gb_params ) {
        string param =
            GetParam(gb_params, NCBI_GBLOADER_PARAM_ALWAYS_LOAD_EXTERNAL);
        if ( !param.empty() ) {
            try {
                m_AlwaysLoadExternal = NStr::StringToBool(param);
            }
            catch ( CException& exc ) {
                NCBI_RETHROW_FMT(exc, CLoaderException, eBadConfig,
                                 "Bad value of parameter "
                                 NCBI_GBLOADER_PARAM_ALWAYS_LOAD_EXTERNAL
                                 ": \""<<param<<"\"");
            }
        }
    }
    m_AlwaysLoadNamedAcc = true;
    if ( gb_params ) {
        string param =
            GetParam(gb_params, NCBI_GBLOADER_PARAM_ALWAYS_LOAD_NAMED_ACC);
        if ( !param.empty() ) {
            try {
                m_AlwaysLoadNamedAcc = NStr::StringToBool(param);
            }
            catch ( CException& exc ) {
                NCBI_RETHROW_FMT(exc, CLoaderException, eBadConfig,
                                 "Bad value of parameter "
                                 NCBI_GBLOADER_PARAM_ALWAYS_LOAD_NAMED_ACC
                                 ": \""<<param<<"\"");
            }
        }
    }
    m_AddWGSMasterDescr = true;
    if ( gb_params ) {
        string param =
            GetParam(gb_params, NCBI_GBLOADER_PARAM_ADD_WGS_MASTER);
        if ( !param.empty() ) {
            try {
                m_AddWGSMasterDescr = NStr::StringToBool(param);
            }
            catch ( CException& exc ) {
                NCBI_RETHROW_FMT(exc, CLoaderException, eBadConfig,
                                 "Bad value of parameter "
                                 NCBI_GBLOADER_PARAM_ADD_WGS_MASTER
                                 ": \""<<param<<"\"");
            }
        }
    }
    m_PTISErrorAction = eGBErrorAction_report;
    if ( gb_params ) {
        string param =
            GetParam(gb_params, NCBI_GBLOADER_PARAM_PTIS_ERROR_ACTION);
        if ( !param.empty() ) {
            if ( NStr::EqualNocase(param, NCBI_GBLOADER_PARAM_PTIS_ERROR_ACTION_IGNORE) ) {
                m_PTISErrorAction = eGBErrorAction_ignore;
            }
            else if ( NStr::EqualNocase(param, NCBI_GBLOADER_PARAM_PTIS_ERROR_ACTION_REPORT) ) {
                m_PTISErrorAction = eGBErrorAction_report;
            }
            else if ( NStr::EqualNocase(param, NCBI_GBLOADER_PARAM_PTIS_ERROR_ACTION_THROW) ) {
                m_PTISErrorAction = eGBErrorAction_throw;
            }
            else {
                NCBI_THROW_FMT(CLoaderException, eBadConfig,
                               "Bad value of parameter "
                               NCBI_GBLOADER_PARAM_PTIS_ERROR_ACTION
                               ": \""<<param<<"\"");
            }
        }
    }
    
    m_Dispatcher = new CReadDispatcher;
    m_InfoManager = new CGBInfoManager(queue_size);
    
    // now we create readers & writers
    if ( params.GetReaderPtr() ) {
        // explicit reader specified
        CRef<CReader> reader(params.GetReaderPtr());
        reader->OpenInitialConnection(false);
        m_Dispatcher->InsertReader(1, reader);
        return;
    }

    CGBLoaderParams::EPreopenConnection preopen =
        params.GetPreopenConnection();
    if ( preopen == CGBLoaderParams::ePreopenByConfig && gb_params ) {
        try {
            string param = GetParam(gb_params, NCBI_GBLOADER_PARAM_PREOPEN);
            if ( !param.empty() ) {
                if ( NStr::StringToBool(param) )
                    preopen = CGBLoaderParams::ePreopenAlways;
                else
                    preopen = CGBLoaderParams::ePreopenNever;
            }
        }
        catch ( CException& /*ignored*/ ) {
        }
    }
    
    if ( !gb_params ) {
        app_params.reset(new TParamTree);
        gb_params = GetLoaderParams(app_params.get());
    }

    if ( !params.GetReaderName().empty() ) {
        string reader_name = params.GetReaderName();
        NStr::ToLower(reader_name);
        if ( NStr::StartsWith(reader_name, "pubseqos") )
            m_WebCookie = params.GetWebCookie();
    
        if ( x_CreateReaders(reader_name, gb_params, preopen) ) {
            if ( reader_name == "cache" ||
                 NStr::StartsWith(reader_name, "cache;") ) {
                x_CreateWriters("cache", gb_params);
            }
        }
    }
    else {
        pair<string, string> rw_name = GetReaderWriterName(gb_params);
        if ( x_CreateReaders(rw_name.first, gb_params, preopen) ) {
            x_CreateWriters(rw_name.second, gb_params);
        }
    }
}


pair<string, string>
CGBDataLoader_Native::GetReaderWriterName(const TParamTree* params) const
{
    pair<string, string> ret;
    ret.first = GetParam(params, NCBI_GBLOADER_PARAM_READER_NAME);
    if ( ret.first.empty() ) {
        ret.first = TGenbankReaderName::GetDefault();
    }
    ret.second = GetParam(params, NCBI_GBLOADER_PARAM_WRITER_NAME);
    if ( ret.first.empty() ) {
        ret.first = TGenbankWriterName::GetDefault();
    }
    if ( ret.first.empty() || ret.second.empty() ) {
        string method = GetParam(params, NCBI_GBLOADER_PARAM_LOADER_METHOD);
        if ( method.empty() ) {
            // try config first
            method = TGenbankLoaderMethod::GetDefault();
        }
        if ( method.empty() ) {
            // fall back default reader list
            method = DEFAULT_DRV_ORDER;
        }
        NStr::ToLower(method);
        if ( ret.first.empty() ) {
            ret.first = method;
        }
        if ( ret.second.empty() && NStr::StartsWith(method, "cache;") ) {
            ret.second = "cache";
        }
    }
    NStr::ToLower(ret.first);
    NStr::ToLower(ret.second);
    return ret;
}


bool CGBDataLoader_Native::x_CreateReaders(const string& str,
                                    const TParamTree* params,
                                    CGBLoaderParams::EPreopenConnection preopen)
{
    vector<string> str_list;
    NStr::Split(str, ";", str_list);
    size_t reader_count = 0;
    for ( size_t i = 0; i < str_list.size(); ++i ) {
        CRef<CReader> reader(x_CreateReader(str_list[i], params));
        if( reader ) {
            if ( HasHUPIncluded() ) {
                reader->SetIncludeHUP(true, m_WebCookie);
            }
            if ( preopen != CGBLoaderParams::ePreopenNever ) {
                reader->OpenInitialConnection(preopen == CGBLoaderParams::ePreopenAlways);
            }
            m_Dispatcher->InsertReader(i, reader);
            ++reader_count;
        }
    }
    if ( !reader_count ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "no reader available from "+str);
    }
    return reader_count > 1 || str_list.size() > 1;
}


void CGBDataLoader_Native::x_CreateWriters(const string& str,
                                    const TParamTree* params)
{
    vector<string> str_list;
    NStr::Split(str, ";", str_list);
    for ( size_t i = 0; i < str_list.size(); ++i ) {
        if ( HasHUPIncluded() ) {
            NCBI_THROW(CObjMgrException, eRegisterError,
                       "HUP GBLoader cannot have cache");
        }
        CRef<CWriter> writer(x_CreateWriter(str_list[i], params));
        if( writer ) {
            m_Dispatcher->InsertWriter(i, writer);
        }
    }
}


#ifdef REGISTER_READER_ENTRY_POINTS
NCBI_PARAM_DECL(bool, GENBANK, REGISTER_READERS);
NCBI_PARAM_DEF_EX(bool, GENBANK, REGISTER_READERS, true,
                  eParam_NoThread, GENBANK_REGISTER_READERS);
#endif

CRef<CPluginManager<CReader> > CGBDataLoader_Native::x_GetReaderManager(void)
{
    CRef<TReaderManager> manager(CPluginManagerGetter<CReader>::Get());
    _ASSERT(manager);

#ifdef REGISTER_READER_ENTRY_POINTS
    if ( NCBI_PARAM_TYPE(GENBANK, REGISTER_READERS)::GetDefault() ) {
        GenBankReaders_Register_Id1();
        GenBankReaders_Register_Id2();
        GenBankReaders_Register_Cache();
# ifdef HAVE_PUBSEQ_OS
        //GenBankReaders_Register_Pubseq();
# endif
    }
#endif

    return manager;
}


CRef<CPluginManager<CWriter> > CGBDataLoader_Native::x_GetWriterManager(void)
{
    CRef<TWriterManager> manager(CPluginManagerGetter<CWriter>::Get());
    _ASSERT(manager);

#ifdef REGISTER_READER_ENTRY_POINTS
    if ( NCBI_PARAM_TYPE(GENBANK, REGISTER_READERS)::GetDefault() ) {
        GenBankWriters_Register_Cache();
    }
#endif

    return manager;
}


static bool s_ForceDriver(const string& name)
{
    return !name.empty() && name[name.size()-1] != ':';
}


CReader* CGBDataLoader_Native::x_CreateReader(const string& name,
                                       const TParamTree* params)
{
    CRef<TReaderManager> manager = x_GetReaderManager();
    CReader* ret = manager->CreateInstanceFromList(params, name);
    if ( !ret ) {
        if ( s_ForceDriver(name) ) {
            // reader is required at this slot
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "no reader available from "+name);
        }
    }
    else {
        ret->InitializeCache(m_CacheManager, params);
    }
    return ret;
}


CWriter* CGBDataLoader_Native::x_CreateWriter(const string& name,
                                       const TParamTree* params)
{
    CRef<TWriterManager> manager = x_GetWriterManager();
    CWriter* ret = manager->CreateInstanceFromList(params, name);
    if ( !ret ) {
        if ( s_ForceDriver(name) ) {
            // writer is required at this slot
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "no writer available from "+name);
        }
    }
    else {
        ret->InitializeCache(m_CacheManager, params);
    }
    return ret;
}


CDataLoader::TBlobId CGBDataLoader_Native::GetBlobId(const CSeq_id_Handle& sih)
{
    if ( CReadDispatcher::CannotProcess(sih) ) {
        return TBlobId();
    }
    CGBReaderRequestResult result(this, sih);
    CLoadLockBlobIds blobs(result, sih, 0);
    m_Dispatcher->LoadSeq_idBlob_ids(result, sih, 0);
    CFixedBlob_ids blob_ids = blobs.GetBlob_ids();
    ITERATE ( CFixedBlob_ids, it, blob_ids ) {
        if ( it->Matches(fBlobHasCore, 0) ) {
            return TBlobId(it->GetBlob_id().GetPointer());
        }
    }
    return TBlobId();
}

CDataLoader::TBlobId CGBDataLoader_Native::GetBlobIdFromString(const string& str) const
{
    return TBlobId(CBlob_id::CreateFromString(str));
}


void CGBDataLoader_Native::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    if ( CReadDispatcher::CannotProcess(idh) ) {
        return;
    }
    CGBReaderRequestResult result(this, idh);
    CLoadLockSeqIds lock(result, idh);
    if ( !lock.IsLoaded() ) {
        m_Dispatcher->LoadSeq_idSeq_ids(result, idh);
    }
    ids = lock.GetSeq_ids();
}


CDataLoader::SAccVerFound
CGBDataLoader_Native::GetAccVerFound(const CSeq_id_Handle& idh)
{
    SAccVerFound ret;
    if ( CReadDispatcher::CannotProcess(idh) ) {
        // no such sequence
        return ret;
    }
    CGBReaderRequestResult result(this, idh);
    CLoadLockAcc lock(result, idh);
    if ( !lock.IsLoadedAccVer() ) {
        m_Dispatcher->LoadSeq_idAccVer(result, idh);
    }
    if ( lock.IsLoadedAccVer() ) {
        ret = lock.GetAccVer();
    }
    return ret;
}


CDataLoader::SGiFound
CGBDataLoader_Native::GetGiFound(const CSeq_id_Handle& idh)
{
    SGiFound ret;
    if ( CReadDispatcher::CannotProcess(idh) ) {
        return ret;
    }
    CGBReaderRequestResult result(this, idh);
    CLoadLockGi lock(result, idh);
    if ( !lock.IsLoadedGi() ) {
        m_Dispatcher->LoadSeq_idGi(result, idh);
    }
    if ( lock.IsLoadedGi() ) {
        ret = lock.GetGi();
    }
    return ret;
}


string CGBDataLoader_Native::GetLabel(const CSeq_id_Handle& idh)
{
    if ( CReadDispatcher::CannotProcess(idh) ) {
        return string();
    }
    CGBReaderRequestResult result(this, idh);
    CLoadLockLabel lock(result, idh);
    if ( !lock.IsLoadedLabel() ) {
        m_Dispatcher->LoadSeq_idLabel(result, idh);
    }
    return lock.GetLabel();
}


TTaxId CGBDataLoader_Native::GetTaxId(const CSeq_id_Handle& idh)
{
    if ( CReadDispatcher::CannotProcess(idh) ) {
        return INVALID_TAX_ID;
    }
    CGBReaderRequestResult result(this, idh);
    CLoadLockTaxId lock(result, idh);
    if ( !lock.IsLoadedTaxId() ) {
        m_Dispatcher->LoadSeq_idTaxId(result, idh);
    }
    TTaxId taxid = lock.IsLoadedTaxId()? lock.GetTaxId(): INVALID_TAX_ID;
    if ( taxid == INVALID_TAX_ID ) {
        return CDataLoader::GetTaxId(idh);
    }
    return taxid;
}


int CGBDataLoader_Native::GetSequenceState(const CSeq_id_Handle& sih)
{
    const int kNotFound = (CBioseq_Handle::fState_not_found |
                           CBioseq_Handle::fState_no_data);

    if ( CReadDispatcher::CannotProcess(sih) ) {
        return kNotFound;
    }
    TIds ids(1, sih);
    TLoaded loaded(1);
    TSequenceStates states(1);
    GetSequenceStates(ids, loaded, states);
    return loaded[0]? states[0]: kNotFound;
}


void CGBDataLoader_Native::GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret)
{
    for ( size_t i = 0; i < ids.size(); ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CGBReaderRequestResult result(this, ids[i]);
        m_Dispatcher->LoadAccVers(result, ids, loaded, ret);
        return;
    }
}


void CGBDataLoader_Native::GetGis(const TIds& ids, TLoaded& loaded, TGis& ret)
{
    for ( size_t i = 0; i < ids.size(); ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CGBReaderRequestResult result(this, ids[i]);
        m_Dispatcher->LoadGis(result, ids, loaded, ret);
        return;
    }
}


void CGBDataLoader_Native::GetLabels(const TIds& ids, TLoaded& loaded, TLabels& ret)
{
    for ( size_t i = 0; i < ids.size(); ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CGBReaderRequestResult result(this, ids[i]);
        m_Dispatcher->LoadLabels(result, ids, loaded, ret);
        return;
    }
}


void CGBDataLoader_Native::GetTaxIds(const TIds& ids, TLoaded& loaded, TTaxIds& ret)
{
    for ( size_t i = 0; i < ids.size(); ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CGBReaderRequestResult result(this, ids[i]);
        m_Dispatcher->LoadTaxIds(result, ids, loaded, ret);

        // the ID2 may accidentally return no taxid for newly loaded sequences
        // we have to fall back to full sequence retrieval in such cases
        bool retry = false;
        for ( size_t i = 0; i < ids.size(); ++i ) {
            if ( loaded[i] && ret[i] == INVALID_TAX_ID ) {
                loaded[i] = false;
                retry = true;
            }
        }
        if ( retry ) {
            // full sequence retrieval is implemented in base CDataLoader class
            CDataLoader::GetTaxIds(ids, loaded, ret);
        }

        return;
    }
}


static const bool s_LoadBulkBlobs = true;


TSeqPos CGBDataLoader_Native::GetSequenceLength(const CSeq_id_Handle& sih)
{
    if ( CReadDispatcher::CannotProcess(sih) ) {
        return kInvalidSeqPos;
    }
    CGBReaderRequestResult result(this, sih);
    CLoadLockLength lock(result, sih);
    if ( !lock.IsLoadedLength() ) {
        m_Dispatcher->LoadSequenceLength(result, sih);
    }
    return lock.IsLoaded()? lock.GetLength(): 0;
}


void CGBDataLoader_Native::GetSequenceLengths(const TIds& ids, TLoaded& loaded,
                                       TSequenceLengths& ret)
{
    for ( size_t i = 0; i < ids.size(); ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CGBReaderRequestResult result(this, ids[i]);
        m_Dispatcher->LoadLengths(result, ids, loaded, ret);
        return;
    }
}


CDataLoader::STypeFound
CGBDataLoader_Native::GetSequenceTypeFound(const CSeq_id_Handle& sih)
{
    STypeFound ret;
    if ( CReadDispatcher::CannotProcess(sih) ) {
        return ret;
    }
    CGBReaderRequestResult result(this, sih);
    CLoadLockType lock(result, sih);
    if ( !lock.IsLoadedType() ) {
        m_Dispatcher->LoadSequenceType(result, sih);
    }
    if ( lock.IsLoadedType() ) {
        ret = lock.GetType();
    }
    return ret;
}


void CGBDataLoader_Native::GetSequenceTypes(const TIds& ids, TLoaded& loaded,
                                     TSequenceTypes& ret)
{
    for ( size_t i = 0; i < ids.size(); ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CGBReaderRequestResult result(this, ids[i]);
        m_Dispatcher->LoadTypes(result, ids, loaded, ret);
        return;
    }
}


void CGBDataLoader_Native::GetSequenceStates(const TIds& ids, TLoaded& loaded,
                                      TSequenceStates& ret)
{
    if ( !s_LoadBulkBlobs ) {
        CDataLoader::GetSequenceStates(ids, loaded, ret);
        return;
    }
    for ( size_t i = 0; i < ids.size(); ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CGBReaderRequestResult result(this, ids[i]);
        m_Dispatcher->LoadStates(result, ids, loaded, ret);
        return;
    }
    /*
    set<CSeq_id_Handle> load_set;
    size_t count = ids.size();
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    CGBReaderRequestResult result(this, ids[0]);
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CLoadLockBlobIds blobs(result, ids[i], 0);
        if ( blobs.IsLoaded() ) {
            continue;
        }
        // add into loading set
        load_set.insert(ids[i]);
    }
    if ( !load_set.empty() ) {
        result.SetRequestedId(*load_set.begin());
        m_Dispatcher->LoadSeq_idsBlob_ids(result, load_set);
    }
    // update sequence states
    for ( size_t i = 0; i < count; ++i ) {
        CReadDispatcher::SetBlobState(i, result, ids, loaded, ret);
    }
    */
}


CDataLoader::SHashFound
CGBDataLoader_Native::GetSequenceHashFound(const CSeq_id_Handle& sih)
{
    SHashFound ret;
    if ( CReadDispatcher::CannotProcess(sih) ) {
        return ret;
    }
    CGBReaderRequestResult result(this, sih);
    CLoadLockHash lock(result, sih);
    if ( !lock.IsLoadedHash() ) {
        m_Dispatcher->LoadSequenceHash(result, sih);
    }
    if ( lock.IsLoadedHash() ) {
        ret = lock.GetHash();
    }
    return ret;
}


void CGBDataLoader_Native::GetSequenceHashes(const TIds& ids, TLoaded& loaded,
                                      TSequenceHashes& ret, THashKnown& known)
{
    for ( size_t i = 0; i < ids.size(); ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CGBReaderRequestResult result(this, ids[i]);
        m_Dispatcher->LoadHashes(result, ids, loaded, ret, known);
        return;
    }
}


CDataLoader::TBlobVersion CGBDataLoader_Native::GetBlobVersion(const TBlobId& id)
{
    const TRealBlobId& blob_id = GetRealBlobId(id);
    CGBReaderRequestResult result(this, CSeq_id_Handle());
    CLoadLockBlobVersion lock(result, blob_id);
    if ( !lock.IsLoadedBlobVersion() ) {
        m_Dispatcher->LoadBlobVersion(result, blob_id);
    }
    return lock.GetBlobVersion();
}


CDataLoader::TTSE_Lock
CGBDataLoader_Native::ResolveConflict(const CSeq_id_Handle& handle,
                               const TTSE_LockSet& tse_set)
{
    TTSE_Lock best;
    bool         conflict=false;

    CGBReaderRequestResult result(this, handle);

    ITERATE(TTSE_LockSet, sit, tse_set) {
        const CTSE_Info& tse = **sit;

        CLoadLockBlob blob(result, GetRealBlobId(tse));
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
    if ( !best || conflict ) {
        _TRACE("CGBDataLoader::ResolveConflict("<<handle.AsString()<<"): "
               "conflict");
    }
    return best;
}


bool CGBDataLoader_Native::HaveCache(TCacheType cache_type)
{
    ITERATE(CReaderCacheManager::TCaches, it, m_CacheManager.GetCaches()) {
        if ((it->m_Type & cache_type) != 0) {
            return true;
        }
    }
    return false;
}


void CGBDataLoader_Native::PurgeCache(TCacheType            cache_type,
                               time_t                access_timeout)
{
    ITERATE(CReaderCacheManager::TCaches, it, m_CacheManager.GetCaches()) {
        if ((it->m_Type & cache_type) != 0) {
            it->m_Cache->Purge(access_timeout);
        }
    }
}


void CGBDataLoader_Native::CloseCache(void)
{
    // Reset cache for each reader/writer
    m_Dispatcher->ResetCaches();
    m_CacheManager.GetCaches().clear();
}


//=======================================================================
// GBLoader private interface
//
void CGBDataLoader_Native::GC(void)
{
}


TBlobContentsMask CGBDataLoader_Native::x_MakeContentMask(EChoice choice) const
{
    switch(choice) {
    case CGBDataLoader_Native::eBlob:
    case CGBDataLoader_Native::eBioseq:
        // whole bioseq
        return fBlobHasAllLocal;
    case CGBDataLoader_Native::eCore:
    case CGBDataLoader_Native::eBioseqCore:
        // everything except bioseqs & annotations
        return fBlobHasCore;
    case CGBDataLoader_Native::eSequence:
        // seq data
        return fBlobHasSeqMap | fBlobHasSeqData;
    case CGBDataLoader_Native::eFeatures:
        // SeqFeatures
        return fBlobHasIntFeat;
    case CGBDataLoader_Native::eGraph:
        // SeqGraph
        return fBlobHasIntGraph;
    case CGBDataLoader_Native::eAlign:
        // SeqGraph
        return fBlobHasIntAlign;
    case CGBDataLoader_Native::eAnnot:
        // all internal annotations
        return fBlobHasIntAnnot;
    case CGBDataLoader_Native::eExtFeatures:
        return fBlobHasExtFeat|fBlobHasNamedFeat;
    case CGBDataLoader_Native::eExtGraph:
        return fBlobHasExtGraph|fBlobHasNamedGraph;
    case CGBDataLoader_Native::eExtAlign:
        return fBlobHasExtAlign|fBlobHasNamedAlign;
    case CGBDataLoader_Native::eExtAnnot:
        // external annotations
        return fBlobHasExtAnnot|fBlobHasNamedAnnot;
    case CGBDataLoader_Native::eOrphanAnnot:
        // orphan annotations
        return fBlobHasOrphanAnnot;
    case CGBDataLoader_Native::eAll:
        // everything
        return fBlobHasAll;
    default:
        return 0;
    }
}


TBlobContentsMask
CGBDataLoader_Native::x_MakeContentMask(const SRequestDetails& details) const
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
CGBDataLoader_Native::GetRecords(const CSeq_id_Handle& sih, const EChoice choice)
{
    return x_GetRecords(sih, x_MakeContentMask(choice), 0);
}


CDataLoader::TTSE_LockSet
CGBDataLoader_Native::GetDetailedRecords(const CSeq_id_Handle& sih,
                                  const SRequestDetails& details)
{
    return x_GetRecords(sih, x_MakeContentMask(details), 0);
}


bool CGBDataLoader_Native::CanGetBlobById(void) const
{
    return true;
}


CDataLoader::TTSE_Lock
CGBDataLoader_Native::GetBlobById(const TBlobId& id)
{
    const TRealBlobId& blob_id = GetRealBlobId(id);

    CGBReaderRequestResult result(this, CSeq_id_Handle());
    CLoadLockBlob blob(result, blob_id);
    if ( !blob.IsLoadedBlob() ) {
        m_Dispatcher->LoadBlob(result, blob_id);
    }
    _ASSERT(blob.IsLoadedBlob());
    return blob.GetTSE_LoadLock();
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
CGBDataLoader_Native::GetExternalRecords(const CBioseq_Info& bioseq)
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
        else if ( it->Which() == CSeq_id::e_Gi ) {
            // gi is not found, do not try any other Seq-id
            break;
        }
    }
    return ret;
}


CDataLoader::TTSE_LockSet
CGBDataLoader_Native::GetExternalAnnotRecordsNA(const CSeq_id_Handle& idh,
                                         const SAnnotSelector* sel,
                                         TProcessedNAs* processed_nas)
{
    return x_GetRecords(idh, fBlobHasExtAnnot|fBlobHasNamedAnnot, sel, processed_nas);
}


CDataLoader::TTSE_LockSet
CGBDataLoader_Native::GetExternalAnnotRecordsNA(const CBioseq_Info& bioseq,
                                         const SAnnotSelector* sel,
                                         TProcessedNAs* processed_nas)
{
    TTSE_LockSet ret;
    TIds ids = bioseq.GetId();
    sort(ids.begin(), ids.end(), SBetterId());
    ITERATE ( TIds, it, ids ) {
        if ( GetBlobId(*it) ) {
            // correct id is found
            TTSE_LockSet ret2 = GetExternalAnnotRecordsNA(*it, sel, processed_nas);
            ret.swap(ret2);
            break;
        }
        else if ( it->Which() == CSeq_id::e_Gi ) {
            // gi is not found, do not try any other Seq-id
            break;
        }
    }
    return ret;
}


CDataLoader::TTSE_LockSet
CGBDataLoader_Native::GetOrphanAnnotRecordsNA(const CSeq_id_Handle& idh,
                                       const SAnnotSelector* sel,
                                       TProcessedNAs* processed_nas)
{
    bool load_external = m_AlwaysLoadExternal;
    bool load_namedacc =
        m_AlwaysLoadNamedAcc && IsRequestedAnyNA(sel);
    if ( load_external || load_namedacc ) {
        TBlobContentsMask mask = 0;
        if ( load_external ) {
            mask |= fBlobHasExtAnnot;
        }
        if ( load_namedacc ) {
            mask |= fBlobHasNamedAnnot;
        }
        return x_GetRecords(idh, mask, sel, processed_nas);
    }
    else {
        return CDataLoader::GetOrphanAnnotRecordsNA(idh, sel, processed_nas);
    }
}


CDataLoader::TTSE_LockSet
CGBDataLoader_Native::x_GetRecords(const CSeq_id_Handle& sih,
                            TBlobContentsMask mask,
                            const SAnnotSelector* sel,
                            TProcessedNAs* processed_nas)
{
    TTSE_LockSet locks;

    if ( mask == 0 || CReadDispatcher::CannotProcess(sih) ) {
        return locks;
    }

    if ( (mask & ~fBlobHasOrphanAnnot) == 0 ) {
        // no orphan annotations in GenBank
        return locks;
    }

    CGBReaderRequestResult result(this, sih);
    m_Dispatcher->LoadBlobs(result, sih, mask, sel);
    CLoadLockBlobIds blobs(result, sih, sel);
    if ( !blobs.IsLoaded() ) {
        return locks;
    }
    _ASSERT(blobs.IsLoaded());

    CFixedBlob_ids blob_ids = blobs.GetBlob_ids();
    if ( (blob_ids.GetState() & CBioseq_Handle::fState_no_data) != 0 ) {
        if ( (mask & fBlobHasAllLocal) == 0 ||
             blob_ids.GetState() == CBioseq_Handle::fState_no_data ) {
            // only external annotatsions are requested,
            // or default state - return empty lock set
            return locks;
        }
        NCBI_THROW2(CBlobStateException, eBlobStateError,
                    "blob state error for "+sih.AsString(),
                    blob_ids.GetState());
    }

    ITERATE ( CFixedBlob_ids, it, blob_ids ) {
        const CBlob_Info& info = *it;
        const CBlob_id& blob_id = *info.GetBlob_id();
        if ( info.Matches(mask, sel) ) {
            CLoadLockBlob blob(result, blob_id);
            if ( !blob.IsLoadedBlob() ) {
                continue;
            }
            CTSE_LoadLock& lock = blob.GetTSE_LoadLock();
            _ASSERT(lock);
            if ( lock->GetBlobState() & CBioseq_Handle::fState_no_data ) {
                NCBI_THROW2(CBlobStateException, eBlobStateError,
                            "blob state error for "+blob_id.ToString(),
                            lock->GetBlobState());
            }
            if ( processed_nas ) {
                if ( auto annot_info = info.GetAnnotInfo() ) {
                    for ( auto& acc : annot_info->GetNamedAnnotNames() ) {
                        CDataLoader::SetProcessedNA(acc, processed_nas);
                    }
                }
            }
            locks.insert(lock);
        }
    }
    result.SaveLocksTo(locks);
    return locks;
}


CGBDataLoader::TNamedAnnotNames
CGBDataLoader_Native::GetNamedAnnotAccessions(const CSeq_id_Handle& sih)
{
    TNamedAnnotNames names;

    CGBReaderRequestResult result(this, sih);
    SAnnotSelector sel;
    sel.IncludeNamedAnnotAccession("NA*");
    CLoadLockBlobIds blobs(result, sih, &sel);
    m_Dispatcher->LoadSeq_idBlob_ids(result, sih, &sel);
    _ASSERT(blobs.IsLoaded());

    CFixedBlob_ids blob_ids = blobs.GetBlob_ids();
    if ( (blob_ids.GetState() & CBioseq_Handle::fState_no_data) != 0) {
        if ( blob_ids.GetState() == CBioseq_Handle::fState_no_data ) {
            // default state - return empty name set
            return names;
        }
        NCBI_THROW2(CBlobStateException, eBlobStateError,
                    "blob state error for "+sih.AsString(),
                    blob_ids.GetState());
    }

    ITERATE ( CFixedBlob_ids, it, blob_ids ) {
        const CBlob_Info& info = *it;
        if ( !info.IsSetAnnotInfo() ) {
            continue;
        }
        CConstRef<CBlob_Annot_Info> annot_info = info.GetAnnotInfo();
        ITERATE( CBlob_Annot_Info::TNamedAnnotNames, jt,
                 annot_info->GetNamedAnnotNames()) {
            names.insert(*jt);
        }
    }
    return names;
}


CGBDataLoader::TNamedAnnotNames
CGBDataLoader_Native::GetNamedAnnotAccessions(const CSeq_id_Handle& sih,
                                       const string& named_acc)
{
    TNamedAnnotNames names;

    CGBReaderRequestResult result(this, sih);
    SAnnotSelector sel;
    if ( !ExtractZoomLevel(named_acc, 0, 0) ) {
        sel.IncludeNamedAnnotAccession(CombineWithZoomLevel(named_acc, -1));
    }
    else {
        sel.IncludeNamedAnnotAccession(named_acc);
    }
    CLoadLockBlobIds blobs(result, sih, &sel);
    m_Dispatcher->LoadSeq_idBlob_ids(result, sih, &sel);
    _ASSERT(blobs.IsLoaded());

    CFixedBlob_ids blob_ids = blobs.GetBlob_ids();
    if ( (blob_ids.GetState() & CBioseq_Handle::fState_no_data) != 0 ) {
        if ( blob_ids.GetState() == CBioseq_Handle::fState_no_data ) {
            // default state - return empty name set
            return names;
        }
        NCBI_THROW2(CBlobStateException, eBlobStateError,
                    "blob state error for "+sih.AsString(),
                    blob_ids.GetState());
    }

    ITERATE ( CFixedBlob_ids, it, blob_ids ) {
        const CBlob_Info& info = *it;
        if ( !info.IsSetAnnotInfo() ) {
            continue;
        }
        CConstRef<CBlob_Annot_Info> annot_info = info.GetAnnotInfo();
        ITERATE ( CBlob_Annot_Info::TNamedAnnotNames, jt,
                  annot_info->GetNamedAnnotNames() ) {
            names.insert(*jt);
        }
    }
    return names;
}


void CGBDataLoader_Native::GetChunk(TChunk chunk)
{
    CReader::TChunkId id = chunk->GetChunkId();
    if ( id == kMasterWGS_ChunkId ) {
        CWGSMasterSupport::LoadWGSMaster(this, chunk);
    }
    else {
        CGBReaderRequestResult result(this, CSeq_id_Handle());
        m_Dispatcher->LoadChunk(result,
                                GetRealBlobId(chunk->GetBlobId()),
                                id);
    }
}


void CGBDataLoader_Native::GetChunks(const TChunkSet& chunks)
{
    typedef map<TBlobId, CReader::TChunkIds> TChunkIdMap;
    TChunkIdMap chunk_ids;
    ITERATE(TChunkSet, it, chunks) {
        CReader::TChunkId id = (*it)->GetChunkId();
        if ( id == kMasterWGS_ChunkId ) {
            CWGSMasterSupport::LoadWGSMaster(this, *it);
        }
        else {
            chunk_ids[(*it)->GetBlobId()].push_back(id);
        }
    }
    ITERATE(TChunkIdMap, it, chunk_ids) {
        CGBReaderRequestResult result(this, CSeq_id_Handle());
        m_Dispatcher->LoadChunks(result,
                                 GetRealBlobId(it->first),
                                 it->second);
    }
}


void CGBDataLoader_Native::GetBlobs(TTSE_LockSets& tse_sets)
{
    CGBReaderRequestResult result(this, CSeq_id_Handle());
    TBlobContentsMask mask = fBlobHasCore;
    CReadDispatcher::TIds ids;
    ITERATE(TTSE_LockSets, tse_set, tse_sets) {
        const CSeq_id_Handle& id = tse_set->first;
        if ( CReadDispatcher::CannotProcess(id) ) {
            continue;
        }
        ids.push_back(id);
    }
    m_Dispatcher->LoadBlobSet(result, ids);

    NON_CONST_ITERATE(TTSE_LockSets, tse_set, tse_sets) {
        const CSeq_id_Handle& id = tse_set->first;
        if ( CReadDispatcher::CannotProcess(id) ) {
            continue;
        }
        CLoadLockBlobIds blob_ids_lock(result, id, 0);
        CFixedBlob_ids blob_ids = blob_ids_lock.GetBlob_ids();
        ITERATE ( CFixedBlob_ids, it, blob_ids ) {
            const CBlob_Info& info = *it;
            const CBlob_id& blob_id = *info.GetBlob_id();
            if ( info.Matches(mask, 0) ) {
                CLoadLockBlob blob(result, blob_id);
                _ASSERT(blob.IsLoadedBlob());
                /*
                if ((blob.GetBlobState() & CBioseq_Handle::fState_no_data) != 0) {
                    // Ignore bad blobs
                    continue;
                }
                */
                tse_set->second.insert(blob.GetTSE_LoadLock());
            }
        }
    }
}

#if 0
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
#endif


void CGBDataLoader_Native::DropTSE(CRef<CTSE_Info> /* tse_info */)
{
    //TWriteLockGuard guard(m_LoadMap_Lock);
    //m_LoadMapBlob.erase(GetBlob_id(*tse_info));
}


CGBReaderRequestResult::
CGBReaderRequestResult(CGBDataLoader_Native* loader,
                       const CSeq_id_Handle& requested_id)
    : CReaderRequestResult(requested_id,
                           loader->GetDispatcher(),
                           loader->GetInfoManager()),
      m_Loader(loader)
{
}


CGBReaderRequestResult::~CGBReaderRequestResult(void)
{
}


CGBDataLoader_Native* CGBReaderRequestResult::GetLoaderPtr(void)
{
    return m_Loader;
}


CTSE_LoadLock CGBReaderRequestResult::GetTSE_LoadLock(const TKeyBlob& blob_id)
{
    CGBDataLoader::TBlobId id(new TKeyBlob(blob_id));
    return GetLoader().GetDataSource()->GetTSE_LoadLock(id);
}


CTSE_LoadLock CGBReaderRequestResult::GetTSE_LoadLockIfLoaded(const TKeyBlob& blob_id)
{
    CGBDataLoader::TBlobId id(new TKeyBlob(blob_id));
    return GetLoader().GetDataSource()->GetTSE_LoadLockIfLoaded(id);
}


void CGBReaderRequestResult::GetLoadedBlob_ids(const CSeq_id_Handle& idh,
                                               TLoadedBlob_ids& blob_ids) const
{
    CDataSource::TLoadedBlob_ids blob_ids2;
    m_Loader->GetDataSource()->GetLoadedBlob_ids(idh,
                                                 CDataSource::fLoaded_bioseqs,
                                                 blob_ids2);
    ITERATE(CDataSource::TLoadedBlob_ids, id, blob_ids2) {
        blob_ids.push_back(m_Loader->GetRealBlobId(*id));
    }
}


CGBDataLoader::TExpirationTimeout
CGBReaderRequestResult::GetIdExpirationTimeout(GBL::EExpirationType type) const
{
    if ( type == GBL::eExpire_normal ) {
        return m_Loader->GetIdExpirationTimeout();
    }
    else {
        return CReaderRequestResult::GetIdExpirationTimeout(type);
    }
}


bool
CGBReaderRequestResult::GetAddWGSMasterDescr(void) const
{
    return m_Loader->GetAddWGSMasterDescr();
}


EGBErrorAction CGBReaderRequestResult::GetPTISErrorAction(void) const
{
    return m_Loader->GetPTISErrorAction();
}


/*
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
*/

void CGBReaderCacheManager::RegisterCache(ICache& cache,
                                          ECacheType cache_type)
{
    SReaderCacheInfo info(cache, cache_type);
    //!!! Make sure the cache is not registered yet!
    m_Caches.push_back(info);
}


ICache* CGBReaderCacheManager::FindCache(ECacheType cache_type,
                                         const TCacheParams* params)
{
    NON_CONST_ITERATE(TCaches, it, m_Caches) {
        if ((it->m_Type & cache_type) == 0) {
            continue;
        }
        if ( it->m_Cache->SameCacheParams(params) ) {
            return it->m_Cache.get();
        }
    }
    return 0;
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

END_NCBI_SCOPE
