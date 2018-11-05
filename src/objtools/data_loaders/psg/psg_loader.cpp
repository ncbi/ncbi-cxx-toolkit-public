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
 * Author: Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description: PSG data loader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqres/seqres__.hpp>

#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

#include <objtools/data_loaders/psg/psg_loader.hpp>
#include <objtools/data_loaders/psg/impl/psg_loader_impl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;


BEGIN_LOCAL_NAMESPACE;

class CLoaderFilter : public CObjectManager::IDataLoaderFilter {
public:
    bool IsDataLoaderMatches(CDataLoader& loader) const {
        return dynamic_cast<CPSGDataLoader*>(&loader) != 0;
    }
};


class CRevoker {
public:
    ~CRevoker() {
        CLoaderFilter filter;
        CObjectManager::GetInstance()->RevokeDataLoaders(filter);
    }
};
static CRevoker s_Revoker;

END_LOCAL_NAMESPACE;


/////////////////////////////////////////////////////////////////////////////
// CPSGDataLoader
/////////////////////////////////////////////////////////////////////////////


const char kDataLoader_PSG_DriverName[] = "psg";
const char kPSG_ServiceName[] = "service_name";
const char kPSG_NoSplit[] = "no_split";


NCBI_PARAM_DECL(string, PSG_LOADER, SERVICE_NAME);
NCBI_PARAM_DEF_EX(string, PSG_LOADER, SERVICE_NAME, "psg",
    eParam_NoThread, PSG_LOADER_SERVICE_NAME);
typedef NCBI_PARAM_TYPE(PSG_LOADER, SERVICE_NAME) TPSG_ServiceName;

NCBI_PARAM_DECL(bool, PSG_LOADER, NO_SPLIT);
NCBI_PARAM_DEF_EX(bool, PSG_LOADER, NO_SPLIT, false,
    eParam_NoThread, PSG_LOADER_NO_SPLIT);
typedef NCBI_PARAM_TYPE(PSG_LOADER, NO_SPLIT) TPSG_NoSplit;

SPSGLoaderParams::SPSGLoaderParams(void)
{
    m_ServiceName = TPSG_ServiceName::GetDefault();
    m_NoSplit = TPSG_NoSplit::GetDefault();
}


CPSGDataLoader::TRegisterLoaderInfo CPSGDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const SPSGLoaderParams& params,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CPSGDataLoader::TRegisterLoaderInfo CPSGDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SPSGLoaderParams params;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CPSGDataLoader::TRegisterLoaderInfo CPSGDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& service_name,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SPSGLoaderParams params;
    params.m_ServiceName = service_name;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CPSGDataLoader::GetLoaderNameFromArgs(void)
{
    return "PSGDataLoader";
}


string CPSGDataLoader::GetLoaderNameFromArgs(const SPSGLoaderParams& params)
{
    string ret = GetLoaderNameFromArgs();
    if ( !params.m_ServiceName.empty() ) {
        ret += "@" + params.m_ServiceName;
    }
    return ret;
}


string CPSGDataLoader::GetLoaderNameFromArgs(
    const string& service_name)
{
    SPSGLoaderParams params;
    params.m_ServiceName = service_name;
    return GetLoaderNameFromArgs(params);
}


CPSGDataLoader::CPSGDataLoader(const string& loader_name,
                               const SPSGLoaderParams& params)
    : CDataLoader(loader_name)
{
    m_Impl.Reset(new CPSGDataLoader_Impl(params));
}


CPSGDataLoader::~CPSGDataLoader(void)
{
}


CDataLoader::TBlobId CPSGDataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    return TBlobId(m_Impl->GetBlobId(idh).GetPointerOrNull());
}


CDataLoader::TBlobId
CPSGDataLoader::GetBlobIdFromString(const string& str) const
{
    return TBlobId(new CPsgBlobId(str));
}


bool CPSGDataLoader::CanGetBlobById(void) const
{
    return true;
}


CDataLoader::TTSE_LockSet
CPSGDataLoader::GetRecords(const CSeq_id_Handle& idh,
                           EChoice choice)
{
    return m_Impl->GetRecords(GetDataSource(), idh, choice);
}


CDataLoader::TTSE_LockSet
CPSGDataLoader::GetExternalRecords(const CBioseq_Info& /*seq*/)
{
    // PSG loader doesn't provide external annotations ???
    return TTSE_LockSet();
}


CDataLoader::TTSE_LockSet
CPSGDataLoader::GetExternalAnnotRecords(const CSeq_id_Handle& /*idh*/,
                                        const SAnnotSelector* /*sel*/)
{
    // PSG loader doesn't provide external annotations ???
    return TTSE_LockSet();
}


CDataLoader::TTSE_LockSet
CPSGDataLoader::GetExternalAnnotRecords(const CBioseq_Info& /*bioseq*/,
                                        const SAnnotSelector* /*sel*/)
{
    // PSG loader doesn't provide external annotations ???
    return TTSE_LockSet();
}


CDataLoader::TTSE_LockSet
CPSGDataLoader::GetOrphanAnnotRecords(const CSeq_id_Handle& /*idh*/,
                                      const SAnnotSelector* /*sel*/)
{
    // PSG loader doesn't provide orphan annotations ???
    return TTSE_LockSet();
}


void CPSGDataLoader::GetChunk(TChunk chunk)
{
    TBlobId blob_id = chunk->GetBlobId();
    const CPsgBlobId& psg_id = dynamic_cast<const CPsgBlobId&>(*blob_id);
    m_Impl->LoadChunk(psg_id, *chunk);
}


void CPSGDataLoader::GetChunks(const TChunkSet& chunks)
{
    ITERATE ( TChunkSet, it, chunks ) {
        GetChunk(*it);
    }
}


CDataLoader::TTSE_Lock
CPSGDataLoader::GetBlobById(const TBlobId& blob_id)
{
    return m_Impl->GetBlobById(GetDataSource(),
                               dynamic_cast<const CPsgBlobId&>(*blob_id));
}


void CPSGDataLoader::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    m_Impl->GetIds(idh, ids);
}


int CPSGDataLoader::GetTaxId(const CSeq_id_Handle& idh)
{
    return m_Impl->GetTaxId(idh);
}


TSeqPos CPSGDataLoader::GetSequenceLength(const CSeq_id_Handle& idh)
{
    return m_Impl->GetSequenceLength(idh);
}


CDataLoader::SHashFound
CPSGDataLoader::GetSequenceHashFound(const CSeq_id_Handle& idh)
{
    return m_Impl->GetSequenceHash(idh);
}


CDataLoader::STypeFound
CPSGDataLoader::GetSequenceTypeFound(const CSeq_id_Handle& idh)
{
    return m_Impl->GetSequenceType(idh);
}


CObjectManager::TPriority CPSGDataLoader::GetDefaultPriority(void) const
{
    return CObjectManager::kPriority_Replace;
}


void CPSGDataLoader::DropTSE(CRef<CTSE_Info> tse_info)
{
    m_Impl->DropTSE(dynamic_cast<const CPsgBlobId&>(*tse_info->GetBlobId()));
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_PSG(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_DataLoader_PSG);
}


class CPSG_DataLoaderCF : public CDataLoaderFactory
{
public:
    CPSG_DataLoaderCF(void)
        : CDataLoaderFactory(objects::kDataLoader_PSG_DriverName) {}
    virtual ~CPSG_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CPSG_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CPSGDataLoader::RegisterInObjectManager(om).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CPSGDataLoader::RegisterInObjectManager(
        om,
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_DataLoader_PSG(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CPSG_DataLoaderCF>::NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_psg(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_PSG(info_list, method);
}


END_NCBI_SCOPE
