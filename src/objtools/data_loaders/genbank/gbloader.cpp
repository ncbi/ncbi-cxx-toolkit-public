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
#include <common/ncbi_source_ver.h>
#include <corelib/ncbi_param.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/gbnative.hpp>
#include <objtools/data_loaders/genbank/psg_loader.hpp>
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

#include <util/md5.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objtools_GBLoader

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


NCBI_PARAM_DECL(bool, GENBANK, LOADER_PSG);
NCBI_PARAM_DEF_EX(bool, GENBANK, LOADER_PSG, false,
    eParam_NoThread, GENBANK_LOADER_PSG);
typedef NCBI_PARAM_TYPE(GENBANK, LOADER_PSG) TGenbankLoaderPsg;

#define GBLOADER_NAME "GBLOADER"
#define GBLOADER_HUP_NAME "GBLOADER-HUP"

CGBLoaderParams::CGBLoaderParams(void)
    : m_ReaderPtr(0),
      m_ParamTree(0),
      m_Preopen(ePreopenByConfig),
      m_UsePSGInitialized(false),
      m_UsePSG(false),
      m_PSGNoSplit(false),
      m_HasHUPIncluded(false)
{
}


CGBLoaderParams::CGBLoaderParams(const string& reader_name)
    : CGBLoaderParams()
{
    m_ReaderName = reader_name;
}


CGBLoaderParams::CGBLoaderParams(CReader* reader_ptr)
    : CGBLoaderParams()
{
    m_ReaderPtr = reader_ptr;
}


CGBLoaderParams::CGBLoaderParams(const TParamTree* param_tree)
    : CGBLoaderParams()
{
    m_ParamTree = param_tree;
}


CGBLoaderParams::CGBLoaderParams(EPreopenConnection preopen)
    : CGBLoaderParams()
{
    m_Preopen = preopen;
}


CGBLoaderParams::~CGBLoaderParams(void) = default;

CGBLoaderParams::CGBLoaderParams(const CGBLoaderParams&) = default;
CGBLoaderParams& CGBLoaderParams::operator=(const CGBLoaderParams&) = default;


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


bool CGBLoaderParams::IsSetEnableSNP(void) const
{
    return !m_EnableSNP.IsNull();
}

bool CGBLoaderParams::GetEnableSNP(void) const
{
    return m_EnableSNP.GetValue();
}

void CGBLoaderParams::SetEnableSNP(bool enable)
{
    m_EnableSNP = enable;
}

bool CGBLoaderParams::IsSetEnableWGS(void) const
{
    return !m_EnableWGS.IsNull();
}

bool CGBLoaderParams::GetEnableWGS(void) const
{
    return m_EnableWGS.GetValue();
}

void CGBLoaderParams::SetEnableWGS(bool enable)
{
    m_EnableWGS = enable;
}

bool CGBLoaderParams::IsSetEnableCDD(void) const
{
    return !m_EnableCDD.IsNull();
}

bool CGBLoaderParams::GetEnableCDD(void) const
{
    return m_EnableCDD.GetValue();
}

void CGBLoaderParams::SetEnableCDD(bool enable)
{
    m_EnableCDD = enable;
}


NCBI_PARAM_DEF_EX(string, GENBANK, LOADER_METHOD, "",
                  eParam_NoThread, GENBANK_LOADER_METHOD);
typedef NCBI_PARAM_TYPE(GENBANK, LOADER_METHOD) TGenbankLoaderMethod;

#define NCBI_GBLOADER_PARAM_LOADER_PSG = "loader_psg"
#define NCBI_PSG_READER_NAME "psg"


string CGBDataLoader::x_GetLoaderMethod(const TParamTree* params)
{
    string method = GetParam(params, NCBI_GBLOADER_PARAM_LOADER_METHOD);
    if ( method.empty() ) {
        // try config first
        method = TGenbankLoaderMethod::GetDefault();
    }
    return method;
}


static bool s_CheckPSGMethod(const string& loader_method)
{
    bool use_psg = false;
    if ( NStr::FindNoCase(loader_method, NCBI_PSG_READER_NAME) != NPOS ) {
        vector<string> str_list;
        NStr::Split(loader_method, ";", str_list);
        for (auto s : str_list) {
            if ( NStr::EqualNocase(s, NCBI_PSG_READER_NAME) ) {
#ifdef HAVE_PSG_LOADER
                if (str_list.size() == 1) {
                    use_psg = true;
                    break;
                }
                // PSG method can not be combined with any other methods.
                NCBI_THROW(CLoaderException, eBadConfig,
                           "'PSG' loader method can not be combined with other methods: '" + loader_method + "'");
#else
                // PSG method is not available
                NCBI_THROW(CLoaderException, eBadConfig,
                           "'PSG' loader method is not available: '" + loader_method + "'");
#endif
            }
        }
    }
    return use_psg;
}    


static bool s_GetDefaultUsePSG()
{
    static atomic<bool> initialized;
    static atomic<bool> loader_psg;
    if ( !initialized.load(memory_order_acquire) ) {
        bool new_value = false;
#ifdef HAVE_PSG_LOADER
        if ( TGenbankLoaderPsg::GetDefault() ) {
            new_value = true;
        }
#endif
        if ( !new_value ) {
            new_value = s_CheckPSGMethod(TGenbankLoaderMethod::GetDefault());
        }
        loader_psg.store(new_value, memory_order_relaxed);
        initialized.store(true, memory_order_release);
    }
    return loader_psg.load(memory_order_relaxed);
}


static bool s_GetDefaultUsePSG(const CGBLoaderParams::TParamTree* param_tree)
{
    if ( param_tree ) {
        if ( const CGBLoaderParams::TParamTree* gb_params = CGBDataLoader::GetLoaderParams(param_tree) ) {
#ifdef HAVE_LOADER_PSG
            auto loader_psg_param = CGBDataLoader::GetParam(gb_params, NCBI_GBLOADER_PARAM_LOADER_PSG);
            if ( !loader_psg_param.empty() ) {
                try {
                    return NStr::StringToBool(loader_psg_param);
                }
                catch (CStringException& ex) {
                    NCBI_THROW_FMT(CConfigException, eInvalidParameter,
                                   "Cannot init GenBank loader, incorrect parameter format: "
                                   << NCBI_GBLOADER_PARAM_LOADER_PSG << " : " << loader_psg_param
                                   << ". " << ex.what());
                }
            }
#endif
            string loader_method = CGBDataLoader::GetParam(gb_params, NCBI_GBLOADER_PARAM_LOADER_METHOD);
            if ( !loader_method.empty() ) {
                return s_CheckPSGMethod(loader_method);
            }
        }
    }
    return s_GetDefaultUsePSG();
}


bool CGBLoaderParams::GetUsePSG() const
{
    if ( m_UsePSGInitialized ) {
        return m_UsePSG;
    }
    
    string loader_method = GetLoaderMethod();
    if (loader_method.empty()) {
        loader_method = GetReaderName();
    }
    if (loader_method.empty()) {
        // use default settings from config
        m_UsePSG = s_GetDefaultUsePSG(GetParamTree());
    }
    else {
        m_UsePSG = s_CheckPSGMethod(loader_method);
    }
    m_UsePSGInitialized = true;
    return m_UsePSG;
}

/*
void CGBDataLoader::SetLoaderMethod(CGBLoaderParams& params)
{
    string loader_method = params.GetLoaderMethod();
    if (loader_method.empty()) {
        loader_method = params.GetReaderName();
    }
    if (loader_method.empty()) {
        unique_ptr<TParamTree> app_params;
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

        if ( !gb_params ) {
            app_params.reset(new TParamTree);
            gb_params = GetLoaderParams(app_params.get());
        }

        loader_method = GetParam(gb_params, NCBI_GBLOADER_PARAM_LOADER_METHOD);
        if ( loader_method.empty() ) {
            // try config first
            loader_method = TGenbankLoaderMethod::GetDefault();
        }
        params.SetLoaderMethod(loader_method);
    }

    vector<string> str_list;
    NStr::Split(loader_method, ";", str_list);
    for (auto s : str_list) {
        if (!NStr::EqualNocase(s, "psg")) continue;
        if (str_list.size() == 1) {
            TGenbankLoaderPsg::SetDefault(true);
            break;
        }
        // PSG method can not be combined with any other methods.
        NCBI_THROW(CLoaderException, eBadConfig,
            "'PSG' loader method can not be combined with other methods: '" + loader_method + "'");
    }
}
*/

CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CReader*        reader_ptr,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    CGBLoaderParams params(reader_ptr);
    return RegisterInObjectManager(om, params, is_default, priority);
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
    return RegisterInObjectManager(om, params, is_default, priority);
}


string CGBDataLoader::GetLoaderNameFromArgs(const string& /*reader_name*/)
{
    return GBLOADER_NAME;
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    EIncludeHUP     include_hup,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    return RegisterInObjectManager(om, include_hup, NcbiEmptyString, is_default,
                                   priority);
}

string CGBDataLoader::GetLoaderNameFromArgs(EIncludeHUP include_hup)
{
    return GBLOADER_HUP_NAME;
}

CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    EIncludeHUP     /*include_hup*/,
    const string& web_cookie,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    CGBLoaderParams params;
    params.SetHUPIncluded(true, web_cookie);
    return RegisterInObjectManager(om, params, is_default, priority);
}


string CGBDataLoader::GetLoaderNameFromArgs(EIncludeHUP include_hup,
                                            const string& web_cookie)
{
    CGBLoaderParams params;
    params.SetHUPIncluded(true, web_cookie);
    return GetLoaderNameFromArgs(params);
}

CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string&   reader_name,
    EIncludeHUP     include_hup,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    return RegisterInObjectManager(om, reader_name, include_hup, NcbiEmptyString,
                                   is_default, priority);
}

string CGBDataLoader::GetLoaderNameFromArgs(const string& reader_name,
                                            EIncludeHUP include_hup)
{
    return GBLOADER_HUP_NAME;
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string&   reader_name,
    EIncludeHUP     include_hup,
    const string& web_cookie,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    CGBLoaderParams params(reader_name);
    params.SetHUPIncluded(true, web_cookie);
    return RegisterInObjectManager(om, params, is_default, priority);
}


string CGBDataLoader::GetLoaderNameFromArgs(const string& reader_name,
                                            EIncludeHUP include_hup,
                                            const string& web_cookie)
{
    CGBLoaderParams params(reader_name);
    params.SetHUPIncluded(true, web_cookie);
    return GetLoaderNameFromArgs(params);
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const TParamTree& param_tree,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    CGBLoaderParams params(&param_tree);
    return RegisterInObjectManager(om, params, is_default, priority);
}


string CGBDataLoader::GetLoaderNameFromArgs(const TParamTree& param_tree)
{
    return GBLOADER_NAME;
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const CGBLoaderParams& params,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    if ( params.GetUsePSG() ) {
#if defined(HAVE_PSG_LOADER)
        return CPSGDataLoader::RegisterInObjectManager(om, params, is_default, priority);
#else
        ERR_POST_X(3, Critical << "PSG Loader is requested but not available");
        TRegisterLoaderInfo info;
        info.Set(nullptr, false);
        return info;
#endif
    }
    return CGBDataLoader_Native::RegisterInObjectManager(om, params, is_default, priority);
}


string CGBDataLoader::GetLoaderNameFromArgs(const CGBLoaderParams& params)
{
    if ( !params.GetLoaderName().empty() ) {
        return params.GetLoaderName();
    }
    if (params.HasHUPIncluded()) {
        const string& web_cookie = params.GetWebCookie();
        if (web_cookie.empty()) {
            return GBLOADER_HUP_NAME;
        }
        else {
            CMD5 md5;
            md5.Update(web_cookie.data(), web_cookie.size());
            return GBLOADER_HUP_NAME + string("-") + md5.GetHexSum();
        }
    } else {
        return GBLOADER_NAME;
    }
}


CGBDataLoader::CGBDataLoader(const string& loader_name,
                             const CGBLoaderParams& params)
    : CDataLoader(loader_name),
      m_HasHUPIncluded(params.HasHUPIncluded()),
      m_WebCookie(params.GetWebCookie())
{
}


CGBDataLoader::~CGBDataLoader(void)
{
}


CObjectManager::TPriority CGBDataLoader::GetDefaultPriority(void) const
{
    CObjectManager::TPriority priority = CDataLoader::GetDefaultPriority();
    if ( HasHUPIncluded() ) {
        // HUP data loader has lower priority by default
        priority += 1;
    }
    return priority;
}


bool CGBDataLoader::GetTrackSplitSeq() const
{
    return true;
}


namespace {
    typedef CGBDataLoader::TParamTree TParams;

    TParams* FindSubNode(TParams* params,
                         const string& name)
    {
        return params? params->FindSubNode(name): 0;
    }

    const TParams* FindSubNode(const TParams* params,
                               const string& name)
    {
        return params? params->FindSubNode(name): 0;
    }
}


const CGBDataLoader::TParamTree*
CGBDataLoader::GetParamsSubnode(const TParamTree* params,
                                const string& subnode_name)
{
    const TParamTree* subnode = 0;
    if ( params ) {
        if ( params->KeyEqual(subnode_name) ) {
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
    if ( params->KeyEqual(subnode_name) ) {
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


CConstRef<CSeqref> CGBDataLoader::GetSatSatkey(const CSeq_id_Handle& sih)
{
    TBlobId id = GetBlobId(sih);
    if (id) {
        TRealBlobId blob_id = GetRealBlobId(id);
        return ConstRef(new CSeqref(ZERO_GI, blob_id.GetSat(), blob_id.GetSatKey()));
    }
    return CConstRef<CSeqref>();
}


CConstRef<CSeqref> CGBDataLoader::GetSatSatkey(const CSeq_id& id)
{
    return GetSatSatkey(CSeq_id_Handle::GetHandle(id));
}


CGBDataLoader::TRealBlobId
CGBDataLoader::GetRealBlobId(const TBlobId& blob_id) const
{
    return x_GetRealBlobId(blob_id);
}


CGBDataLoader::TRealBlobId
CGBDataLoader::GetRealBlobId(const CTSE_Info& tse_info) const
{
    if (&tse_info.GetDataSource() != GetDataSource()) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
            "not mine TSE");
    }
    return GetRealBlobId(tse_info.GetBlobId());
}


bool CGBDataLoader::IsUsingPSGLoader(void)
{
    return s_GetDefaultUsePSG();
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
