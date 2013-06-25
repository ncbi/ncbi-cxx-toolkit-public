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
 * File Description: CSRA file data loader
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

#include <sra/data_loaders/csra/csraloader.hpp>
#include <sra/data_loaders/csra/impl/csraloader_impl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;

/*
NCBI_PARAM_DECL(string, CSRA, DIR_PATH);
NCBI_PARAM_DEF_EX(string, CSRA, DIR_PATH, "",
                  eParam_NoThread, CSRA_DIR_PATH);
*/

/////////////////////////////////////////////////////////////////////////////
// CCSRADataLoader
/////////////////////////////////////////////////////////////////////////////

CCSRADataLoader::TRegisterLoaderInfo CCSRADataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const SLoaderParams& params,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CCSRADataLoader::TRegisterLoaderInfo CCSRADataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CCSRADataLoader::TRegisterLoaderInfo CCSRADataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& srz_acc,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    params.m_DirPath = srz_acc;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CCSRADataLoader::TRegisterLoaderInfo CCSRADataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& dir_path,
    const string& csra_name,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    params.m_DirPath = dir_path;
    params.m_CSRAFiles.push_back(csra_name);
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CCSRADataLoader::TRegisterLoaderInfo CCSRADataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& dir_path,
    const vector<string>& csra_files,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    params.m_DirPath = dir_path;
    params.m_CSRAFiles = csra_files;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CCSRADataLoader::GetLoaderNameFromArgs(void)
{
    return "CCSRADataLoader";
}


string CCSRADataLoader::GetLoaderNameFromArgs(const SLoaderParams& params)
{
    if ( params.m_CSRAFiles.empty() ) {
        return "CCSRADataLoader:"+params.m_DirPath;
    }
    else {
        CNcbiOstrstream str;
        str << "CCSRADataLoader:" << params.m_DirPath;
        if ( !params.m_CSRAFiles.empty() ) {
            str << "/";
            ITERATE ( vector<string>, it, params.m_CSRAFiles ) {
                str << "+" << *it;
            }
        }
        return CNcbiOstrstreamToString(str);
    }
}


string CCSRADataLoader::GetLoaderNameFromArgs(const string& srz_acc)
{
    SLoaderParams params;
    params.m_DirPath = srz_acc;
    return GetLoaderNameFromArgs(params);
}


string CCSRADataLoader::GetLoaderNameFromArgs(const string& dir_path,
                                              const string& csra_name)
{
    SLoaderParams params;
    params.m_DirPath = dir_path;
    params.m_CSRAFiles.push_back(csra_name);
    return GetLoaderNameFromArgs(params);
}


string CCSRADataLoader::GetLoaderNameFromArgs(
    const string& dir_path,
    const vector<string>& csra_files)
{
    SLoaderParams params;
    params.m_DirPath = dir_path;
    params.m_CSRAFiles = csra_files;
    return GetLoaderNameFromArgs(params);
}


CCSRADataLoader::CCSRADataLoader(const string& loader_name,
                                 const SLoaderParams& params)
    : CDataLoader(loader_name)
{
    string dir_path = params.m_DirPath;
/*
    if ( dir_path.empty() ) {
        dir_path = NCBI_PARAM_TYPE(CSRA, DIR_PATH)::GetDefault();
    }
*/
    m_Impl.Reset(new CCSRADataLoader_Impl(params));
}


CCSRADataLoader::~CCSRADataLoader(void)
{
}


CDataLoader::TBlobId CCSRADataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    return TBlobId(m_Impl->GetBlobId(idh).GetPointerOrNull());
}


CDataLoader::TBlobId
CCSRADataLoader::GetBlobIdFromString(const string& str) const
{
    return TBlobId(new CCSRABlobId(str));
}


bool CCSRADataLoader::CanGetBlobById(void) const
{
    return true;
}


CDataLoader::TTSE_LockSet
CCSRADataLoader::GetRecords(const CSeq_id_Handle& idh,
                           EChoice choice)
{
    return m_Impl->GetRecords(GetDataSource(), idh, choice);
}


void CCSRADataLoader::GetChunk(TChunk chunk)
{
    TBlobId blob_id = chunk->GetBlobId();
    const CCSRABlobId& csra_id = dynamic_cast<const CCSRABlobId&>(*blob_id);
    m_Impl->LoadChunk(csra_id, *chunk);
}


void CCSRADataLoader::GetChunks(const TChunkSet& chunks)
{
    ITERATE ( TChunkSet, it, chunks ) {
        GetChunk(*it);
    }
}


CDataLoader::TTSE_Lock
CCSRADataLoader::GetBlobById(const TBlobId& blob_id)
{
    return m_Impl->GetBlobById(GetDataSource(),
                               dynamic_cast<const CCSRABlobId&>(*blob_id));
}


CCSRADataLoader::TAnnotNames CCSRADataLoader::GetPossibleAnnotNames(void) const
{
    return m_Impl->GetPossibleAnnotNames();
}


void CCSRADataLoader::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    m_Impl->GetIds(idh, ids);
}


CSeq_id_Handle CCSRADataLoader::GetAccVer(const CSeq_id_Handle& idh)
{
    return m_Impl->GetAccVer(idh);
}


int CCSRADataLoader::GetGi(const CSeq_id_Handle& idh)
{
    return m_Impl->GetGi(idh);
}


string CCSRADataLoader::GetLabel(const CSeq_id_Handle& idh)
{
    return m_Impl->GetLabel(idh);
}


int CCSRADataLoader::GetTaxId(const CSeq_id_Handle& idh)
{
    return m_Impl->GetTaxId(idh);
}


TSeqPos CCSRADataLoader::GetSequenceLength(const CSeq_id_Handle& idh)
{
    return m_Impl->GetSequenceLength(idh);
}


CSeq_inst::TMol CCSRADataLoader::GetSequenceType(const CSeq_id_Handle& idh)
{
    return m_Impl->GetSequenceType(idh);
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_CSRA(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_DataLoader_CSRA);
}


const char kDataLoader_CSRA_DriverName[] = "csra";

class CCSRA_DataLoaderCF : public CDataLoaderFactory
{
public:
    CCSRA_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_CSRA_DriverName) {}
    virtual ~CCSRA_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CCSRA_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CCSRADataLoader::RegisterInObjectManager(om).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CCSRADataLoader::RegisterInObjectManager(
        om,
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_DataLoader_CSRA(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CCSRA_DataLoaderCF>::NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_csra(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_CSRA(info_list, method);
}


END_NCBI_SCOPE
