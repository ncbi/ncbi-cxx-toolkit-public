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

#define DEFAULT_ID_GC_SIZE 10000
#define DEFAULT_ID_EXPIRATION_TIMEOUT 2*3600 // 2 hours

CGBLoaderParams::CGBLoaderParams(void)
    : m_ReaderName(),
      m_ReaderPtr(0),
      m_ParamTree(0),
      m_Preopen(ePreopenByConfig),
      m_HasHUPIncluded(false),
      m_PSGNoSplit(false)
{
}


CGBLoaderParams::CGBLoaderParams(const string& reader_name)
    : m_ReaderName(reader_name),
      m_ReaderPtr(0),
      m_ParamTree(0),
      m_Preopen(ePreopenByConfig),
      m_HasHUPIncluded(false),
      m_PSGNoSplit(false)
{
}


CGBLoaderParams::CGBLoaderParams(CReader* reader_ptr)
    : m_ReaderName(),
      m_ReaderPtr(reader_ptr),
      m_ParamTree(0),
      m_Preopen(ePreopenByConfig),
      m_HasHUPIncluded(false),
      m_PSGNoSplit(false)
{
}


CGBLoaderParams::CGBLoaderParams(const TParamTree* param_tree)
    : m_ReaderName(),
      m_ReaderPtr(0),
      m_ParamTree(param_tree),
      m_Preopen(ePreopenByConfig),
      m_HasHUPIncluded(false),
      m_PSGNoSplit(false)
{
}


CGBLoaderParams::CGBLoaderParams(EPreopenConnection preopen)
    : m_ReaderName(),
      m_ReaderPtr(0),
      m_ParamTree(0),
      m_Preopen(preopen),
      m_HasHUPIncluded(false),
      m_PSGNoSplit(false)
{
}


CGBLoaderParams::~CGBLoaderParams(void)
{
}


CGBLoaderParams::CGBLoaderParams(const CGBLoaderParams& params)
    : m_ReaderName(params.m_ReaderName),
      m_ReaderPtr(params.m_ReaderPtr),
      m_ParamTree(params.m_ParamTree),
      m_Preopen(params.m_Preopen),
      m_HasHUPIncluded(params.m_HasHUPIncluded),
      m_WebCookie(params.m_WebCookie),
      m_LoaderName(params.m_LoaderName),
      m_PSGServiceName(params.m_PSGServiceName),
      m_PSGNoSplit(params.m_PSGNoSplit)
{
}


CGBLoaderParams& CGBLoaderParams::operator=(const CGBLoaderParams& params)
{
    if ( this != &params ) {
        m_ReaderName = params.m_ReaderName;
        m_ReaderPtr = params.m_ReaderPtr;
        m_ParamTree = params.m_ParamTree;
        m_Preopen = params.m_Preopen;
        m_HasHUPIncluded = params.m_HasHUPIncluded;
        m_WebCookie = params.m_WebCookie;
        m_LoaderName = params.m_LoaderName;
        m_PSGServiceName = params.m_PSGServiceName;
        m_PSGNoSplit = params.m_PSGNoSplit;
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
    if (TGenbankLoaderPsg::GetDefault()) {
#if defined(HAVE_PSG_LOADER)
        return CPSGDataLoader::RegisterInObjectManager(om, is_default, priority);
#else
        ERR_POST_X(3, Critical << "PSG Loader is requested but not available");
        TRegisterLoaderInfo info;
        info.Set(nullptr, false);
        return info;
#endif
    }
    return CGBDataLoader_Native::RegisterInObjectManager(om, reader_ptr, is_default, priority);
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
    if (TGenbankLoaderPsg::GetDefault()) {
#if defined(HAVE_PSG_LOADER)
        return CPSGDataLoader::RegisterInObjectManager(om, is_default, priority);
#else
        ERR_POST_X(3, Critical << "PSG Loader is requested but not available");
        TRegisterLoaderInfo info;
        info.Set(nullptr, false);
        return info;
#endif
    }
    return CGBDataLoader_Native::RegisterInObjectManager(om, reader_name, is_default, priority);
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


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    EIncludeHUP     include_hup,
    const string& web_cookie,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    if (TGenbankLoaderPsg::GetDefault()) {
#if defined(HAVE_PSG_LOADER)
        return CPSGDataLoader::RegisterInObjectManager(om, is_default, priority);
#else
        ERR_POST_X(3, Critical << "PSG Loader is requested but not available");
        TRegisterLoaderInfo info;
        info.Set(nullptr, false);
        return info;
#endif
    }
    return CGBDataLoader_Native::RegisterInObjectManager(om, include_hup, web_cookie, is_default, priority);
}


string CGBDataLoader::GetLoaderNameFromArgs(EIncludeHUP /*include_hup*/)
{
    return GBLOADER_HUP_NAME;
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

CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string&   reader_name,
    EIncludeHUP     include_hup,
    const string& web_cookie,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    if (TGenbankLoaderPsg::GetDefault()) {
#if defined(HAVE_PSG_LOADER)
        return CPSGDataLoader::RegisterInObjectManager(om, is_default, priority);
#else
        ERR_POST_X(3, Critical << "PSG Loader is requested but not available");
        TRegisterLoaderInfo info;
        info.Set(nullptr, false);
        return info;
#endif
    }
    return CGBDataLoader_Native::RegisterInObjectManager(om, reader_name, include_hup, web_cookie, is_default, priority);
}


string CGBDataLoader::GetLoaderNameFromArgs(const string& /*reader_name*/,
                                            EIncludeHUP /*include_hup*/)
{
    return GBLOADER_HUP_NAME;
}


CGBDataLoader::TRegisterLoaderInfo CGBDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const TParamTree& param_tree,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    if (TGenbankLoaderPsg::GetDefault()) {
#if defined(HAVE_PSG_LOADER)
        return CPSGDataLoader::RegisterInObjectManager(om, param_tree, is_default, priority);
#else
        ERR_POST_X(3, Critical << "PSG Loader is requested but not available");
        TRegisterLoaderInfo info;
        info.Set(nullptr, false);
        return info;
#endif
    }
    return CGBDataLoader_Native::RegisterInObjectManager(om, param_tree, is_default, priority);
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
    if (TGenbankLoaderPsg::GetDefault()) {
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
        if (web_cookie.empty())
            return GBLOADER_HUP_NAME;
        else
            return GBLOADER_HUP_NAME + string("-") + web_cookie;
    } else {
        return GBLOADER_NAME;
    }
}


CGBDataLoader::CGBDataLoader(const string& loader_name,
                             const CGBLoaderParams& params)
    : CDataLoader(loader_name),
      m_HasHUPIncluded(params.HasHUPIncluded())
{
}


CGBDataLoader::~CGBDataLoader(void)
{
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
#if defined(HAVE_PSG_LOADER)
    return TGenbankLoaderPsg::GetDefault();
#else
    return false;
#endif
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
