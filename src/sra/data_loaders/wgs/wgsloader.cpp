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
 * Author: Eugene Vasilchenko
 *
 * File Description: WGS file data loader
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

#include <sra/data_loaders/wgs/wgsloader.hpp>
#include <sra/data_loaders/wgs/impl/wgsloader_impl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;


/////////////////////////////////////////////////////////////////////////////
// CWGSDataLoader
/////////////////////////////////////////////////////////////////////////////

CWGSDataLoader::TRegisterLoaderInfo CWGSDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const SLoaderParams& params,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CWGSDataLoader::TRegisterLoaderInfo CWGSDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CWGSDataLoader::TRegisterLoaderInfo CWGSDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& dir_path,
    const vector<string>& wgs_files,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    params.m_WGSVolPath = dir_path;
    params.m_WGSFiles = wgs_files;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CWGSDataLoader::GetLoaderNameFromArgs(void)
{
    return "WGSDataLoader";
}


string CWGSDataLoader::GetLoaderNameFromArgs(const SLoaderParams& params)
{
    string ret = GetLoaderNameFromArgs();
    if ( params.m_WGSFiles.empty() ) {
        if ( !params.m_WGSVolPath.empty() ) {
            ret += "("+params.m_WGSVolPath+")";
        }
    }
    else {
        CNcbiOstrstream str;
        str << ret << ":" << params.m_WGSVolPath << "/";
        ITERATE ( vector<string>, it, params.m_WGSFiles ) {
            str << "+" << *it;
        }
        ret = CNcbiOstrstreamToString(str);
    }
    return ret;
}


string CWGSDataLoader::GetLoaderNameFromArgs(
    const string& dir_path,
    const vector<string>& wgs_files)
{
    SLoaderParams params;
    params.m_WGSVolPath = dir_path;
    params.m_WGSFiles = wgs_files;
    return GetLoaderNameFromArgs(params);
}


CWGSDataLoader::CWGSDataLoader(const string& loader_name,
                               const SLoaderParams& params)
    : CDataLoader(loader_name)
{
    m_Impl.Reset(new CWGSDataLoader_Impl(params));
}


CWGSDataLoader::~CWGSDataLoader(void)
{
}


CDataLoader::TBlobId CWGSDataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    return TBlobId(m_Impl->GetBlobId(idh).GetPointerOrNull());
}


CDataLoader::TBlobId
CWGSDataLoader::GetBlobIdFromString(const string& str) const
{
    return TBlobId(new CWGSBlobId(str));
}


bool CWGSDataLoader::CanGetBlobById(void) const
{
    return true;
}


CDataLoader::TTSE_LockSet
CWGSDataLoader::GetRecords(const CSeq_id_Handle& idh,
                           EChoice choice)
{
    return m_Impl->GetRecords(GetDataSource(), idh, choice);
}


CDataLoader::TTSE_LockSet
CWGSDataLoader::GetExternalRecords(const CBioseq_Info& /*seq*/)
{
    // WGS loader doesn't provide external annotations
    return TTSE_LockSet();
}


CDataLoader::TTSE_LockSet
CWGSDataLoader::GetExternalAnnotRecords(const CSeq_id_Handle& /*idh*/,
                                        const SAnnotSelector* /*sel*/)
{
    // WGS loader doesn't provide external annotations
    return TTSE_LockSet();
}


CDataLoader::TTSE_LockSet
CWGSDataLoader::GetExternalAnnotRecords(const CBioseq_Info& /*bioseq*/,
                                        const SAnnotSelector* /*sel*/)
{
    // WGS loader doesn't provide external annotations
    return TTSE_LockSet();
}


CDataLoader::TTSE_LockSet
CWGSDataLoader::GetOrphanAnnotRecords(const CSeq_id_Handle& /*idh*/,
                                      const SAnnotSelector* /*sel*/)
{
    // WGS loader doesn't provide orphan annotations
    return TTSE_LockSet();
}


void CWGSDataLoader::GetChunk(TChunk chunk)
{
    TBlobId blob_id = chunk->GetBlobId();
    const CWGSBlobId& wgs_id = dynamic_cast<const CWGSBlobId&>(*blob_id);
    m_Impl->LoadChunk(wgs_id, *chunk);
}


void CWGSDataLoader::GetChunks(const TChunkSet& chunks)
{
    ITERATE ( TChunkSet, it, chunks ) {
        GetChunk(*it);
    }
}


CDataLoader::TTSE_Lock
CWGSDataLoader::GetBlobById(const TBlobId& blob_id)
{
    return m_Impl->GetBlobById(GetDataSource(),
                               dynamic_cast<const CWGSBlobId&>(*blob_id));
}


void CWGSDataLoader::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    m_Impl->GetIds(idh, ids);
}


CSeq_id_Handle CWGSDataLoader::GetAccVer(const CSeq_id_Handle& idh)
{
    return m_Impl->GetAccVer(idh);
}


int CWGSDataLoader::GetGi(const CSeq_id_Handle& idh)
{
    return m_Impl->GetGi(idh);
}


int CWGSDataLoader::GetTaxId(const CSeq_id_Handle& idh)
{
    return m_Impl->GetTaxId(idh);
}


TSeqPos CWGSDataLoader::GetSequenceLength(const CSeq_id_Handle& idh)
{
    return m_Impl->GetSequenceLength(idh);
}


CSeq_inst::TMol CWGSDataLoader::GetSequenceType(const CSeq_id_Handle& idh)
{
    return m_Impl->GetSequenceType(idh);
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_WGS(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_DataLoader_WGS);
}


const char kDataLoader_WGS_DriverName[] = "wgs";

class CWGS_DataLoaderCF : public CDataLoaderFactory
{
public:
    CWGS_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_WGS_DriverName) {}
    virtual ~CWGS_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CWGS_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CWGSDataLoader::RegisterInObjectManager(om).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CWGSDataLoader::RegisterInObjectManager(
        om,
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_DataLoader_WGS(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CWGS_DataLoaderCF>::NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_wgs(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_WGS(info_list, method);
}


END_NCBI_SCOPE
