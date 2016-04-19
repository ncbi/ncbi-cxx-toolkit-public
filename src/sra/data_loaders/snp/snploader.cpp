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
 * File Description: SNP file data loader
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

#include <sra/data_loaders/snp/snploader.hpp>
#include <sra/data_loaders/snp/impl/snploader_impl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;

BEGIN_LOCAL_NAMESPACE;

class CLoaderFilter : public CObjectManager::IDataLoaderFilter {
public:
    bool IsDataLoaderMatches(CDataLoader& loader) const {
        return dynamic_cast<CSNPDataLoader*>(&loader) != 0;
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
// CSNPDataLoader params
/////////////////////////////////////////////////////////////////////////////


NCBI_PARAM_DECL(string, SNP, ACCESSIONS);
NCBI_PARAM_DEF(string, SNP, ACCESSIONS, "");


string CSNPDataLoader::SLoaderParams::GetLoaderName(void) const
{
    CNcbiOstrstream str;
    str << "CSNPDataLoader:" << m_DirPath;
    if ( !m_VDBFiles.empty() ) {
        str << "/files=";
        for ( auto& file : m_VDBFiles ) {
            str << "+" << file;
        }
    }
    if ( !m_AnnotName.empty() ) {
        str << "/name=" << m_AnnotName;
    }
    return CNcbiOstrstreamToString(str);
}


/////////////////////////////////////////////////////////////////////////////
// CSNPDataLoader
/////////////////////////////////////////////////////////////////////////////

CSNPDataLoader::TRegisterLoaderInfo CSNPDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const SLoaderParams& params,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CSNPDataLoader::TRegisterLoaderInfo CSNPDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    NStr::Split(NCBI_PARAM_TYPE(SNP, ACCESSIONS)::GetDefault(), ",",
                   params.m_VDBFiles);
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CSNPDataLoader::TRegisterLoaderInfo CSNPDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& path,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    params.m_DirPath = path;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CSNPDataLoader::TRegisterLoaderInfo CSNPDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const vector<string>& files,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    params.m_VDBFiles = files;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CSNPDataLoader::TRegisterLoaderInfo CSNPDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& dir_path,
    const string& file,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    params.m_DirPath = dir_path;
    params.m_VDBFiles.push_back(file);
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CSNPDataLoader::TRegisterLoaderInfo CSNPDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& dir_path,
    const vector<string>& files,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    params.m_DirPath = dir_path;
    params.m_VDBFiles = files;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CSNPDataLoader::GetLoaderNameFromArgs(void)
{
    return "CSNPDataLoader";
}


string CSNPDataLoader::GetLoaderNameFromArgs(const SLoaderParams& params)
{
    return params.GetLoaderName();
}


string CSNPDataLoader::GetLoaderNameFromArgs(const string& path)
{
    SLoaderParams params;
    params.m_DirPath = path;
    return GetLoaderNameFromArgs(params);
}


string CSNPDataLoader::GetLoaderNameFromArgs(const vector<string>& files)
{
    SLoaderParams params;
    params.m_VDBFiles = files;
    return GetLoaderNameFromArgs(params);
}


string CSNPDataLoader::GetLoaderNameFromArgs(const string& dir_path,
                                             const string& file)
{
    SLoaderParams params;
    params.m_DirPath = dir_path;
    params.m_VDBFiles.push_back(file);
    return GetLoaderNameFromArgs(params);
}


string CSNPDataLoader::GetLoaderNameFromArgs(const string& dir_path,
                                             const vector<string>& files)
{
    SLoaderParams params;
    params.m_DirPath = dir_path;
    params.m_VDBFiles = files;
    return GetLoaderNameFromArgs(params);
}


CSNPDataLoader::CSNPDataLoader(const string& loader_name,
                               const SLoaderParams& params)
    : CDataLoader(loader_name)
{
    string dir_path = params.m_DirPath;
/*
    if ( dir_path.empty() ) {
        dir_path = NCBI_PARAM_TYPE(SNP, DIR_PATH)::GetDefault();
    }
*/
    m_Impl.Reset(new CSNPDataLoader_Impl(params));
}


CSNPDataLoader::~CSNPDataLoader(void)
{
}


CDataLoader::TBlobId
CSNPDataLoader::GetBlobIdFromString(const string& str) const
{
    return TBlobId(new CSNPBlobId(str));
}


bool CSNPDataLoader::CanGetBlobById(void) const
{
    return true;
}


CDataLoader::TTSE_LockSet
CSNPDataLoader::GetRecords(const CSeq_id_Handle& idh,
                           EChoice choice)
{
    return m_Impl->GetRecords(GetDataSource(), idh, choice);
}


CDataLoader::TTSE_LockSet
CSNPDataLoader::GetOrphanAnnotRecords(const CSeq_id_Handle& idh,
                                      const SAnnotSelector* sel)
{
    return m_Impl->GetOrphanAnnotRecords(GetDataSource(), idh, sel);
}


void CSNPDataLoader::GetChunk(TChunk chunk)
{
    TBlobId blob_id = chunk->GetBlobId();
    const CSNPBlobId& id = dynamic_cast<const CSNPBlobId&>(*blob_id);
    m_Impl->LoadChunk(id, *chunk);
}


void CSNPDataLoader::GetChunks(const TChunkSet& chunks)
{
    ITERATE ( TChunkSet, it, chunks ) {
        GetChunk(*it);
    }
}


CDataLoader::TTSE_Lock
CSNPDataLoader::GetBlobById(const TBlobId& blob_id)
{
    return m_Impl->GetBlobById(GetDataSource(),
                               dynamic_cast<const CSNPBlobId&>(*blob_id));
}


CSNPDataLoader::TAnnotNames CSNPDataLoader::GetPossibleAnnotNames(void) const
{
    return m_Impl->GetPossibleAnnotNames();
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_SNP(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_xloader_snp);
}


const char kDataLoader_SNP_DriverName[] = "snp";

class CSNP_DataLoaderCF : public CDataLoaderFactory
{
public:
    CSNP_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_SNP_DriverName) {}
    virtual ~CSNP_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CSNP_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CSNPDataLoader::RegisterInObjectManager(om).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CSNPDataLoader::RegisterInObjectManager(
        om,
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_xloader_snp(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CSNP_DataLoaderCF>::NCBI_EntryPointImpl(info_list, method);
}


END_NCBI_SCOPE
