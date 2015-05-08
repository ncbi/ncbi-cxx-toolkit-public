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
 * File Description: data loader for VDB graph data
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

#include <sra/data_loaders/vdbgraph/vdbgraphloader.hpp>
#include <sra/data_loaders/vdbgraph/impl/vdbgraphloader_impl.hpp>

BEGIN_NCBI_SCOPE

class CObject;

BEGIN_SCOPE(objects)

class CDataLoader;

BEGIN_LOCAL_NAMESPACE;

class CLoaderFilter : public CObjectManager::IDataLoaderFilter {
public:
    bool IsDataLoaderMatches(CDataLoader& loader) const {
        return dynamic_cast<CVDBGraphDataLoader*>(&loader) != 0;
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
// CVDBGraphDataLoader
/////////////////////////////////////////////////////////////////////////////

CVDBGraphDataLoader::SLoaderParams::SLoaderParams(void)
{
}


CVDBGraphDataLoader::SLoaderParams::SLoaderParams(const string& vdb_file)
    : m_VDBFiles(1, vdb_file)
{
}


CVDBGraphDataLoader::SLoaderParams::SLoaderParams(const TVDBFiles& vdb_files)
    : m_VDBFiles(vdb_files)
{
}


CVDBGraphDataLoader::SLoaderParams::~SLoaderParams(void)
{
}


CVDBGraphDataLoader::TRegisterLoaderInfo
CVDBGraphDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CVDBGraphDataLoader::TRegisterLoaderInfo
CVDBGraphDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& vdb_file,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params(vdb_file);
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CVDBGraphDataLoader::TRegisterLoaderInfo
CVDBGraphDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const TVDBFiles& vdb_files,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params(vdb_files);
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CVDBGraphDataLoader::GetLoaderNameFromArgs(void)
{
    SLoaderParams params;
    return GetLoaderNameFromArgs(params);
}


string CVDBGraphDataLoader::GetLoaderNameFromArgs(const string& vdb_file)
{
    SLoaderParams params(vdb_file);
    return GetLoaderNameFromArgs(params);
}


string CVDBGraphDataLoader::GetLoaderNameFromArgs(const TVDBFiles& vdb_files)
{
    SLoaderParams params(vdb_files);
    return GetLoaderNameFromArgs(params);
}


string CVDBGraphDataLoader::GetLoaderNameFromArgs(const SLoaderParams& params)
{
    string ret = "VDBGraphDataLoader(";
    ITERATE ( TVDBFiles, it, params.m_VDBFiles ) {
        if ( it != params.m_VDBFiles.begin() ) {
            ret += ',';
        }
        ret += *it;
    }
    ret += ')';
    return ret;
}


CVDBGraphDataLoader::CVDBGraphDataLoader(const string& loader_name,
                                         const SLoaderParams& params)
    : CDataLoader(loader_name)
{
    m_Impl = new CVDBGraphDataLoader_Impl(params.m_VDBFiles);
}


CVDBGraphDataLoader::~CVDBGraphDataLoader(void)
{
}


CVDBGraphDataLoader::TAnnotNames
CVDBGraphDataLoader::GetPossibleAnnotNames(void) const
{
    return m_Impl->GetPossibleAnnotNames();
}


CDataLoader::TBlobId CVDBGraphDataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    return m_Impl->GetBlobId(idh);
}


CDataLoader::TBlobId
CVDBGraphDataLoader::GetBlobIdFromString(const string& str) const
{
    return m_Impl->GetBlobIdFromString(str);
}


bool CVDBGraphDataLoader::CanGetBlobById(void) const
{
    return true;
}


CDataLoader::TTSE_LockSet
CVDBGraphDataLoader::GetRecords(const CSeq_id_Handle& idh,
                                EChoice choice)
{
    return m_Impl->GetRecords(GetDataSource(), idh, choice);
}


CDataLoader::TTSE_LockSet
CVDBGraphDataLoader::GetOrphanAnnotRecords(const CSeq_id_Handle& idh,
                                           const SAnnotSelector* sel)
{
    return m_Impl->GetOrphanAnnotRecords(GetDataSource(), idh, sel);
}


CDataLoader::TTSE_Lock
CVDBGraphDataLoader::GetBlobById(const TBlobId& blob_id)
{
    return m_Impl->GetBlobById(GetDataSource(), blob_id);
}


void CVDBGraphDataLoader::GetChunk(TChunk chunk)
{
    m_Impl->GetChunk(*chunk);
}


void CVDBGraphDataLoader::GetChunks(const TChunkSet& chunks)
{
    ITERATE ( TChunkSet, it, chunks ) {
        GetChunk(*it);
    }
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_VDBGraph(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_xloader_vdbgraph);
}


const char kDataLoader_VDBGraph_DriverName[] = "vdbgraph";

class CVDBGraph_DataLoaderCF : public CDataLoaderFactory
{
public:
    CVDBGraph_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_VDBGraph_DriverName) {}
    virtual ~CVDBGraph_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CVDBGraph_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CVDBGraphDataLoader::RegisterInObjectManager(om).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CVDBGraphDataLoader::RegisterInObjectManager(
        om,
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_xloader_vdbgraph(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CVDBGraph_DataLoaderCF>::NCBI_EntryPointImpl(info_list, method);
}


END_NCBI_SCOPE
