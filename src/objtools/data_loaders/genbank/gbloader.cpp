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
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/gbloader_params.h>
#include <objtools/data_loaders/genbank/impl/dispatcher.hpp>
#include <objtools/data_loaders/genbank/impl/request_result.hpp>

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


NCBI_PARAM_DEF_EX(string, GENBANK, LOADER_METHOD, kEmptyStr,
                  eParam_NoThread, GENBANK_LOADER_METHOD);
typedef NCBI_PARAM_TYPE(GENBANK, LOADER_METHOD) TGenbankLoaderMethod;

#if defined(HAVE_PUBSEQ_OS)
static const char* const DEFAULT_DRV_ORDER = "ID2:PUBSEQOS:ID1";
#else
static const char* const DEFAULT_DRV_ORDER = "ID2:ID1";
#endif

#define GBLOADER_NAME "GBLOADER"

#define DEFAULT_ID_GC_SIZE 10000
#define DEFAULT_ID_EXPIRATION_TIMEOUT 2*3600 // 2 hours

class CGBReaderRequestResult : public CReaderRequestResult
{
    typedef CReaderRequestResult TParent;
public:
    CGBReaderRequestResult(CGBDataLoader* loader,
                           const CSeq_id_Handle& requested_id);
    ~CGBReaderRequestResult(void);

    CGBDataLoader& GetLoader(void)
        {
            return *m_Loader;
        }
    virtual CGBDataLoader* GetLoaderPtr(void);

    //virtual TConn GetConn(void);
    //virtual void ReleaseConn(void);
    virtual CTSE_LoadLock GetTSE_LoadLock(const TKeyBlob& blob_id);
    virtual CTSE_LoadLock GetTSE_LoadLockIfLoaded(const TKeyBlob& blob_id);
    virtual void GetLoadedBlob_ids(const CSeq_id_Handle& idh,
                                   TLoadedBlob_ids& blob_ids) const;

    virtual operator CInitMutexPool&(void) { return GetMutexPool(); }

    CInitMutexPool& GetMutexPool(void) { return m_Loader->m_MutexPool; }

    virtual TExpirationTime GetIdExpirationTimeout(void) const;

    virtual bool GetAddWGSMasterDescr(void) const;

    friend class CGBDataLoader;

private:
    CRef<CGBDataLoader> m_Loader;
};


CGBLoaderParams::CGBLoaderParams(void)
    : m_ReaderName(),
      m_ReaderPtr(0),
      m_ParamTree(0),
      m_Preopen(ePreopenByConfig)
{
}


CGBLoaderParams::CGBLoaderParams(const string& reader_name)
    : m_ReaderName(reader_name),
      m_ReaderPtr(0),
      m_ParamTree(0),
      m_Preopen(ePreopenByConfig)
{
}


CGBLoaderParams::CGBLoaderParams(CReader* reader_ptr)
    : m_ReaderName(),
      m_ReaderPtr(reader_ptr),
      m_ParamTree(0),
      m_Preopen(ePreopenByConfig)
{
}


CGBLoaderParams::CGBLoaderParams(const TParamTree* param_tree)
    : m_ReaderName(),
      m_ReaderPtr(0),
      m_ParamTree(param_tree),
      m_Preopen(ePreopenByConfig)
{
}


CGBLoaderParams::CGBLoaderParams(EPreopenConnection preopen)
    : m_ReaderName(),
      m_ReaderPtr(0),
      m_ParamTree(0),
      m_Preopen(preopen)
{
}


CGBLoaderParams::~CGBLoaderParams(void)
{
}


CGBLoaderParams::CGBLoaderParams(const CGBLoaderParams& params)
    : m_ReaderName(params.m_ReaderName),
      m_ReaderPtr(params.m_ReaderPtr),
      m_ParamTree(params.m_ParamTree),
      m_Preopen(params.m_Preopen)
{
}


CGBLoaderParams& CGBLoaderParams::operator=(const CGBLoaderParams& params)
{
    if ( this != &params ) {
        m_ReaderName = params.m_ReaderName;
        m_ReaderPtr = params.m_ReaderPtr;
        m_ParamTree = params.m_ParamTree;
        m_Preopen = params.m_Preopen;
    }
    return *this;
}


void CGBLoaderParams::SetReaderPtr(CReader* reader_ptr)
{
    m_ReaderPtr = reader_ptr;
}


CReader* CGBLoaderParams::GetReaderPtr(void) const
{
    return m_ReaderPtr.GetNCPointerOrNull();
}


void CGBLoaderParams::SetParamTree(const TPluginManagerParamTree* param_tree)
{
    m_ParamTree = param_tree;
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CReader*        reader_ptr,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    CGBLoaderParams params(reader_ptr);
    TGBMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CGBDataLoader::GetLoaderNameFromArgs(CReader* /*driver*/)
{
    return GBLOADER_NAME;
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string&   reader_name,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    CGBLoaderParams params(reader_name);
    TGBMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CGBDataLoader::GetLoaderNameFromArgs(const string& /*reader_name*/)
{
    return GBLOADER_NAME;
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const TParamTree& param_tree,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    CGBLoaderParams params(&param_tree);
    TGBMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CGBDataLoader::GetLoaderNameFromArgs(const TParamTree& /* params */)
{
    return GBLOADER_NAME;
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const CGBLoaderParams& params,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    TGBMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CGBDataLoader::GetLoaderNameFromArgs(const CGBLoaderParams& /*params*/)
{
    return GBLOADER_NAME;
}


CGBDataLoader::CGBDataLoader(const string& loader_name,
                             const CGBLoaderParams& params)
  : CDataLoader(loader_name)
{
    GBLOG_POST_X(1, "CGBDataLoader");
    x_CreateDriver(params);
}


CGBDataLoader::~CGBDataLoader(void)
{
    GBLOG_POST_X(2, "~CGBDataLoader");
    CloseCache();
}


namespace {
    typedef CGBDataLoader::TParamTree TParams;

    TParams* FindSubNode(TParams* params,
                         const string& name)
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
}


const CGBDataLoader::TParamTree*
CGBDataLoader::GetParamsSubnode(const TParamTree* params,
                                const string& subnode_name)
{
    const TParamTree* subnode = 0;
    if ( params ) {
        if ( NStr::CompareNocase(params->GetKey(), subnode_name) == 0 ) {
            subnode = params;
        }
        else {
            subnode = FindSubNode(params, subnode_name);
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
    if ( NStr::CompareNocase(params->GetKey(), subnode_name) == 0 ) {
        subnode = params;
    }
    else {
        subnode = FindSubNode(params, subnode_name);
        if ( !subnode ) {
            subnode = params->AddNode(
                TParamTree::TValueType(subnode_name, kEmptyStr));
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
    TParamTree* subnode = FindSubNode(params, param_name);
    if ( !subnode ) {
        params->AddNode(TParamTree::TValueType(param_name, param_value));
    }
    else {
        subnode->GetValue().value = param_value;
    }
}


string CGBDataLoader::GetParam(const TParamTree* params,
                               const string& param_name)
{
    if ( params ) {
        const TParamTree* subnode = FindSubNode(params, param_name);
        if ( subnode ) {
            return subnode->GetValue().value;
        }
    }
    return kEmptyStr;
}


void CGBDataLoader::x_CreateDriver(const CGBLoaderParams& params)
{
    auto_ptr<TParamTree> app_params;
    const TParamTree* gb_params = 0;
    if ( params.GetParamTree() ) {
        gb_params = GetLoaderParams(params.GetParamTree());
    }
    else {
        CMutexGuard guard(CNcbiApplication::GetInstanceMutex());
        CNcbiApplication* app = CNcbiApplication::Instance();
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
        if ( x_CreateReaders(reader_name, gb_params, preopen) ) {
            if ( reader_name == "cache" ||
                 NStr::StartsWith(reader_name, "cache;") ) {
                x_CreateWriters("cache", gb_params);
            }
        }
    }
    else {
        if ( x_CreateReaders(GetReaderName(gb_params), gb_params, preopen) ) {
            x_CreateWriters(GetWriterName(gb_params), gb_params);
        }
    }
}


string CGBDataLoader::GetReaderName(const TParamTree* params) const
{
    string str;
    if ( str.empty() ) {
        str = GetParam(params, NCBI_GBLOADER_PARAM_READER_NAME);
    }
    if ( str.empty() ) {
        str = GetParam(params, NCBI_GBLOADER_PARAM_LOADER_METHOD);
    }
    if ( str.empty() ) {
        // try config first
        str = TGenbankLoaderMethod::GetDefault();
    }
    if ( str.empty() ) {
        // fall back default reader list
        str = DEFAULT_DRV_ORDER;
    }
    NStr::ToLower(str);
    return str;
}


string CGBDataLoader::GetWriterName(const TParamTree* params) const
{
    string str = GetParam(params, NCBI_GBLOADER_PARAM_WRITER_NAME);
    if ( str.empty() ) {
        // try config first
        string env_reader = GetParam(params, NCBI_GBLOADER_PARAM_LOADER_METHOD);
        if ( env_reader.empty() ) {
            env_reader = TGenbankLoaderMethod::GetDefault();
        }
        NStr::ToLower(env_reader);
        if ( NStr::StartsWith(env_reader, "cache;") ) {
            str = "cache";
        }
    }
    NStr::ToLower(str);
    return str;
}


bool CGBDataLoader::x_CreateReaders(const string& str,
                                    const TParamTree* params,
                                    CGBLoaderParams::EPreopenConnection preopen)
{
    vector<string> str_list;
    NStr::Tokenize(str, ";", str_list, NStr::eNoMergeDelims);
    size_t reader_count = 0;
    for ( size_t i = 0; i < str_list.size(); ++i ) {
        CRef<CReader> reader(x_CreateReader(str_list[i], params));
        if( reader ) {
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


#ifdef REGISTER_READER_ENTRY_POINTS
NCBI_PARAM_DECL(bool, GENBANK, REGISTER_READERS);
NCBI_PARAM_DEF_EX(bool, GENBANK, REGISTER_READERS, true,
                  eParam_NoThread, GENBANK_REGISTER_READERS);
#endif

CRef<CPluginManager<CReader> > CGBDataLoader::x_GetReaderManager(void)
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


CRef<CPluginManager<CWriter> > CGBDataLoader::x_GetWriterManager(void)
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


CReader* CGBDataLoader::x_CreateReader(const string& name,
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


CWriter* CGBDataLoader::x_CreateWriter(const string& name,
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


CConstRef<CSeqref> CGBDataLoader::GetSatSatkey(const CSeq_id_Handle& sih)
{
    TBlobId id = GetBlobId(sih);
    if ( id ) {
        const TRealBlobId& blob_id = GetRealBlobId(id);
        return ConstRef(new CSeqref(ZERO_GI, blob_id.GetSat(), blob_id.GetSatKey()));
    }
    return CConstRef<CSeqref>();
}


CConstRef<CSeqref> CGBDataLoader::GetSatSatkey(const CSeq_id& id)
{
    return GetSatSatkey(CSeq_id_Handle::GetHandle(id));
}


CDataLoader::TBlobId CGBDataLoader::GetBlobId(const CSeq_id_Handle& sih)
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

CDataLoader::TBlobId CGBDataLoader::GetBlobIdFromString(const string& str) const
{
    return TBlobId(CBlob_id::CreateFromString(str));
}


CDataLoader::TBlobId CGBDataLoader::GetBlobIdFromSatSatKey(int sat,
                                                           int sat_key,
                                                           int sub_sat) const
{
    CRef<CBlob_id> blob_id(new CBlob_id);
    blob_id->SetSat(sat);
    blob_id->SetSatKey(sat_key);
    blob_id->SetSubSat(sub_sat);
    return TBlobId(blob_id);
}


void CGBDataLoader::GetIds(const CSeq_id_Handle& idh, TIds& ids)
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


CSeq_id_Handle CGBDataLoader::GetAccVer(const CSeq_id_Handle& idh)
{
    if ( CReadDispatcher::CannotProcess(idh) ) {
        return null;
    }
    CGBReaderRequestResult result(this, idh);
    CLoadLockAcc lock(result, idh);
    if ( !lock.IsLoadedAccVer() ) {
        m_Dispatcher->LoadSeq_idAccVer(result, idh);
    }
    return lock.GetAccVer();
}


TGi CGBDataLoader::GetGi(const CSeq_id_Handle& idh)
{
    if ( CReadDispatcher::CannotProcess(idh) ) {
        return ZERO_GI;
    }
    CGBReaderRequestResult result(this, idh);
    CLoadLockGi lock(result, idh);
    if ( !lock.IsLoadedGi() ) {
        m_Dispatcher->LoadSeq_idGi(result, idh);
    }
    return lock.GetGi();
}


string CGBDataLoader::GetLabel(const CSeq_id_Handle& idh)
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


int CGBDataLoader::GetTaxId(const CSeq_id_Handle& idh)
{
    if ( CReadDispatcher::CannotProcess(idh) ) {
        return -1;
    }
    CGBReaderRequestResult result(this, idh);
    CLoadLockTaxId lock(result, idh);
    if ( !lock.IsLoadedTaxId() ) {
        m_Dispatcher->LoadSeq_idTaxId(result, idh);
    }
    int taxid = lock.IsLoadedTaxId()? lock.GetTaxId(): -1;
    if ( taxid == -1 ) {
        return CDataLoader::GetTaxId(idh);
    }
    return taxid;
}


int CGBDataLoader::GetSequenceState(const CSeq_id_Handle& sih)
{
    const int kNotFound = (CBioseq_Handle::fState_not_found |
                           CBioseq_Handle::fState_no_data);

    if ( CReadDispatcher::CannotProcess(sih) ) {
        return kNotFound;
    }
    CGBReaderRequestResult result(this, sih);
    CLoadLockBlobIds blobs(result, sih, 0);
    m_Dispatcher->LoadSeq_idBlob_ids(result, sih, 0);
    if ( blobs.GetState() & CBioseq_Handle::fState_no_data ) {
        return blobs.GetState();
    }

    CFixedBlob_ids blob_ids = blobs.GetBlob_ids();
    ITERATE ( CFixedBlob_ids, it, blob_ids ) {
        if ( it->Matches(fBlobHasCore, 0) ) {
            CFixedBlob_ids::TState state = blob_ids.GetState();
            if ( state != CFixedBlob_ids::kUnknownState ) {
                return blob_ids.GetState();
            }
        }
    }
    return kNotFound;
}


void CGBDataLoader::GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret)
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


void CGBDataLoader::GetGis(const TIds& ids, TLoaded& loaded, TGis& ret)
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


void CGBDataLoader::GetLabels(const TIds& ids, TLoaded& loaded, TLabels& ret)
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


void CGBDataLoader::GetTaxIds(const TIds& ids, TLoaded& loaded, TTaxIds& ret)
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
            if ( loaded[i] && ret[i] == -1 ) {
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


void CGBDataLoader::GetSequenceLengths(const TIds& ids, TLoaded& loaded,
                                       TSequenceLengths& ret)
{
    if ( !s_LoadBulkBlobs ) {
        CDataLoader::GetSequenceLengths(ids, loaded, ret);
        return;
    }
    if ( ids.empty() ) {
        return;
    }
    CGBReaderRequestResult result(this, ids[0]);
    vector<CSeq_id_Handle> load_ids;
    size_t count = ids.size();
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        // add into loading set
        load_ids.push_back(ids[i]);
    }
    if ( load_ids.empty() ) {
        // nothing to load
        return;
    }
    sort(load_ids.begin(), load_ids.end());
    m_Dispatcher->LoadBlobSet(result, load_ids);
    // update sequence types
    for ( size_t i = 0; i < count; ++i ) {
        const CSeq_id_Handle& id = ids[i];
        if ( loaded[i] || CReadDispatcher::CannotProcess(id) ) {
            continue;
        }
        CLoadLockBlobIds blob_ids_lock(result, id, 0);
        CFixedBlob_ids blob_ids = blob_ids_lock.GetBlob_ids();
        ITERATE ( CFixedBlob_ids, it, blob_ids ) {
            const CBlob_Info& info = *it;
            const CBlob_id& blob_id = *info.GetBlob_id();
            if ( !info.Matches(fBlobHasCore, 0) ) {
                continue;
            }
            CLoadLockBlob blob(result, blob_id);
            _ASSERT(blob.IsLoadedBlob());
            CTSE_LoadLock& lock = blob.GetTSE_LoadLock();
            _ASSERT(lock);
            if ( CConstRef<CBioseq_Info> seq = lock->FindMatchingBioseq(id) ) {
                ret[i] = seq->GetBioseqLength();
                loaded[i] = true;
                break;
            }
        }
    }
}


void CGBDataLoader::GetSequenceTypes(const TIds& ids, TLoaded& loaded,
                                     TSequenceTypes& ret)
{
    if ( !s_LoadBulkBlobs ) {
        CDataLoader::GetSequenceTypes(ids, loaded, ret);
        return;
    }
    TTSE_LockSets lock_sets;
    size_t count = ids.size();
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        // add into loading set
        lock_sets[ids[i]];
    }
    if ( lock_sets.empty() ) {
        // nothing to load
        return;
    }
    GetBlobs(lock_sets);
    // update sequence types
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        TTSE_LockSet& locks = lock_sets[ids[i]];
        ITERATE(TTSE_LockSet, it, locks) {
            CConstRef<CBioseq_Info> bs_info =
                (*it)->FindMatchingBioseq(ids[i]);
            if ( bs_info ) {
                ret[i] = bs_info->GetInst_Mol();
                loaded[i] = true;
                break;
            }
        }
    }
}


void CGBDataLoader::GetSequenceStates(const TIds& ids, TLoaded& loaded,
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


int CGBDataLoader::GetSequenceHash(const CSeq_id_Handle& sih)
{
    if ( CReadDispatcher::CannotProcess(sih) ) {
        return 0;
    }
    CGBReaderRequestResult result(this, sih);
    CLoadLockHash lock(result, sih);
    if ( !lock.IsLoadedHash() ) {
        m_Dispatcher->LoadSequenceHash(result, sih);
    }
    return lock.IsLoaded()? lock.GetHash(): 0;
}


void CGBDataLoader::GetSequenceHashes(const TIds& ids, TLoaded& loaded,
                                      TSequenceHashes& ret)
{
    for ( size_t i = 0; i < ids.size(); ++i ) {
        if ( loaded[i] || CReadDispatcher::CannotProcess(ids[i]) ) {
            continue;
        }
        CGBReaderRequestResult result(this, ids[i]);
        m_Dispatcher->LoadHashes(result, ids, loaded, ret);
        return;
    }
}


CDataLoader::TBlobVersion CGBDataLoader::GetBlobVersion(const TBlobId& id)
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
CGBDataLoader::ResolveConflict(const CSeq_id_Handle& handle,
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


bool CGBDataLoader::HaveCache(TCacheType cache_type)
{
    ITERATE(CReaderCacheManager::TCaches, it, m_CacheManager.GetCaches()) {
        if ((it->m_Type & cache_type) != 0) {
            return true;
        }
    }
    return false;
}


void CGBDataLoader::PurgeCache(TCacheType            cache_type,
                               time_t                access_timeout,
                               ICache::EKeepVersions keep_last_ver)
{
    ITERATE(CReaderCacheManager::TCaches, it, m_CacheManager.GetCaches()) {
        if ((it->m_Type & cache_type) != 0) {
            it->m_Cache->Purge(access_timeout, keep_last_ver);
        }
    }
}


void CGBDataLoader::CloseCache(void)
{
    // Reset cache for each reader/writer
    m_Dispatcher->ResetCaches();
    m_CacheManager.GetCaches().clear();
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
        return fBlobHasExtFeat|fBlobHasNamedFeat;
    case CGBDataLoader::eExtGraph:
        return fBlobHasExtGraph|fBlobHasNamedGraph;
    case CGBDataLoader::eExtAlign:
        return fBlobHasExtAlign|fBlobHasNamedAlign;
    case CGBDataLoader::eExtAnnot:
        // external annotations
        return fBlobHasExtAnnot|fBlobHasNamedAnnot;
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
    return x_GetRecords(sih, x_MakeContentMask(choice), 0);
}


CDataLoader::TTSE_LockSet
CGBDataLoader::GetDetailedRecords(const CSeq_id_Handle& sih,
                                  const SRequestDetails& details)
{
    return x_GetRecords(sih, x_MakeContentMask(details), 0);
}


bool CGBDataLoader::CanGetBlobById(void) const
{
    return true;
}


CDataLoader::TTSE_Lock
CGBDataLoader::GetBlobById(const TBlobId& id)
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
        else if ( it->Which() == CSeq_id::e_Gi ) {
            // gi is not found, do not try any other Seq-id
            break;
        }
    }
    return ret;
}


CDataLoader::TTSE_LockSet
CGBDataLoader::GetExternalAnnotRecords(const CSeq_id_Handle& idh,
                                       const SAnnotSelector* sel)
{
    return x_GetRecords(idh, fBlobHasExtAnnot|fBlobHasNamedAnnot, sel);
}


CDataLoader::TTSE_LockSet
CGBDataLoader::GetExternalAnnotRecords(const CBioseq_Info& bioseq,
                                       const SAnnotSelector* sel)
{
    TTSE_LockSet ret;
    TIds ids = bioseq.GetId();
    sort(ids.begin(), ids.end(), SBetterId());
    ITERATE ( TIds, it, ids ) {
        if ( GetBlobId(*it) ) {
            // correct id is found
            TTSE_LockSet ret2 = GetExternalAnnotRecords(*it, sel);
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
CGBDataLoader::GetOrphanAnnotRecords(const CSeq_id_Handle& idh,
                                     const SAnnotSelector* sel)
{
    bool load_external = m_AlwaysLoadExternal;
    bool load_namedacc = m_AlwaysLoadNamedAcc &&
        sel && sel->IsIncludedAnyNamedAnnotAccession();
    if ( load_external || load_namedacc ) {
        TBlobContentsMask mask = 0;
        if ( load_external ) {
            mask |= fBlobHasExtAnnot;
        }
        if ( load_namedacc ) {
            mask |= fBlobHasNamedAnnot;
        }
        return x_GetRecords(idh, mask, sel);
    }
    else {
        return CDataLoader::GetOrphanAnnotRecords(idh, sel);
    }
}


CDataLoader::TTSE_LockSet
CGBDataLoader::x_GetRecords(const CSeq_id_Handle& sih,
                            TBlobContentsMask mask,
                            const SAnnotSelector* sel)
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
    _ASSERT(blobs.IsLoaded());

    CFixedBlob_ids blob_ids = blobs.GetBlob_ids();
    if ( (blob_ids.GetState() & CBioseq_Handle::fState_no_data) != 0 ) {
        if ( blob_ids.GetState() == CBioseq_Handle::fState_no_data ) {
            // default state - return empty lock set
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
            _ASSERT(blob.IsLoadedBlob());
            CTSE_LoadLock& lock = blob.GetTSE_LoadLock();
            _ASSERT(lock);
            if ( lock->GetBlobState() & CBioseq_Handle::fState_no_data ) {
                NCBI_THROW2(CBlobStateException, eBlobStateError,
                            "blob state error for "+blob_id.ToString(),
                            lock->GetBlobState());
            }
            locks.insert(lock);
        }
    }
    result.SaveLocksTo(locks);
    return locks;
}


CGBDataLoader::TNamedAnnotNames
CGBDataLoader::GetNamedAnnotAccessions(const CSeq_id_Handle& sih)
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
CGBDataLoader::GetNamedAnnotAccessions(const CSeq_id_Handle& sih,
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


void CGBDataLoader::GetChunk(TChunk chunk)
{
    CReader::TChunkId id = chunk->GetChunkId();
    if ( id == kMasterWGS_ChunkId ) {
        CProcessor::LoadWGSMaster(this, chunk);
    }
    else {
        CGBReaderRequestResult result(this, CSeq_id_Handle());
        m_Dispatcher->LoadChunk(result,
                                GetRealBlobId(chunk->GetBlobId()),
                                id);
    }
}


void CGBDataLoader::GetChunks(const TChunkSet& chunks)
{
    typedef map<TBlobId, CReader::TChunkIds> TChunkIdMap;
    TChunkIdMap chunk_ids;
    ITERATE(TChunkSet, it, chunks) {
        CReader::TChunkId id = (*it)->GetChunkId();
        if ( id == kMasterWGS_ChunkId ) {
            CProcessor::LoadWGSMaster(this, *it);
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


void CGBDataLoader::GetBlobs(TTSE_LockSets& tse_sets)
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

const CGBDataLoader::TRealBlobId&
CGBDataLoader::GetRealBlobId(const TBlobId& blob_id) const
{
    return dynamic_cast<const CBlob_id&>(*blob_id);
}


const CGBDataLoader::TRealBlobId&
CGBDataLoader::GetRealBlobId(const CTSE_Info& tse_info) const
{
    if ( &tse_info.GetDataSource() != GetDataSource() ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "not mine TSE");
    }
    return GetRealBlobId(tse_info.GetBlobId());
}



void CGBDataLoader::DropTSE(CRef<CTSE_Info> /* tse_info */)
{
    //TWriteLockGuard guard(m_LoadMap_Lock);
    //m_LoadMapBlob.erase(GetBlob_id(*tse_info));
}


CGBReaderRequestResult::
CGBReaderRequestResult(CGBDataLoader* loader,
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


CGBDataLoader* CGBReaderRequestResult::GetLoaderPtr(void)
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
CGBReaderRequestResult::GetIdExpirationTimeout(void) const
{
    return m_Loader->GetIdExpirationTimeout();
}


bool
CGBReaderRequestResult::GetAddWGSMasterDescr(void) const
{
    return m_Loader->GetAddWGSMasterDescr();
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
