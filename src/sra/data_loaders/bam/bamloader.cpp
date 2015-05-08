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
 * File Description: BAM file data loader
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

#include <sra/data_loaders/bam/bamloader.hpp>
#include <sra/data_loaders/bam/impl/bamloader_impl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;


BEGIN_LOCAL_NAMESPACE;

class CLoaderFilter : public CObjectManager::IDataLoaderFilter {
public:
    bool IsDataLoaderMatches(CDataLoader& loader) const {
        return dynamic_cast<CBAMDataLoader*>(&loader) != 0;
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


NCBI_PARAM_DECL(string, BAM, DIR_PATH);
NCBI_PARAM_DEF_EX(string, BAM, DIR_PATH, "",
                  eParam_NoThread, BAM_DIR_PATH);

NCBI_PARAM_DECL(string, BAM, BAM_NAME);
NCBI_PARAM_DEF_EX(string, BAM, BAM_NAME, "",
                  eParam_NoThread, BAM_BAM_NAME);

NCBI_PARAM_DECL(string, BAM, INDEX_NAME);
NCBI_PARAM_DEF_EX(string, BAM, INDEX_NAME, "",
                  eParam_NoThread, BAM_INDEX_NAME);


/////////////////////////////////////////////////////////////////////////////
// CBAMDataLoader
/////////////////////////////////////////////////////////////////////////////

CBAMDataLoader::TRegisterLoaderInfo CBAMDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const SLoaderParams& params,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CBAMDataLoader::TRegisterLoaderInfo CBAMDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CBAMDataLoader::TRegisterLoaderInfo CBAMDataLoader::RegisterInObjectManager(
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


CBAMDataLoader::TRegisterLoaderInfo CBAMDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& dir_path,
    const string& bam_name,
    const string& index_name,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    params.m_DirPath = dir_path;
    params.m_BamFiles.push_back(SBamFileName(bam_name, index_name));
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CBAMDataLoader::GetLoaderNameFromArgs(void)
{
    return "BAMDataLoader";
}


string CBAMDataLoader::GetLoaderNameFromArgs(const SLoaderParams& params)
{
    CNcbiOstrstream str;
    str << "CCSRADataLoader:" << params.m_DirPath;
    if ( !params.m_BamFiles.empty() ) {
        str << "/files=";
        ITERATE ( vector<SBamFileName>, it, params.m_BamFiles ) {
            str << "+" << it->m_BamName;
        }
    }
    if ( params.m_IdMapper ) {
        str << "/mapper=" << params.m_IdMapper.get();
    }
    return CNcbiOstrstreamToString(str);
}


string CBAMDataLoader::GetLoaderNameFromArgs(const string& srz_acc)
{
    SLoaderParams params;
    params.m_DirPath = srz_acc;
    return GetLoaderNameFromArgs(params);
}


string CBAMDataLoader::GetLoaderNameFromArgs(const string& dir_path,
                                             const string& bam_name,
                                             const string& index_name)
{
    SLoaderParams params;
    params.m_DirPath = dir_path;
    params.m_BamFiles.push_back(SBamFileName(bam_name, index_name));
    return GetLoaderNameFromArgs(params);
}


string CBAMDataLoader::GetLoaderNameFromArgs(
    const string& dir_path,
    const vector<SBamFileName>& bam_files)
{
    SLoaderParams params;
    params.m_DirPath = dir_path;
    params.m_BamFiles = bam_files;
    return GetLoaderNameFromArgs(params);
}


CBAMDataLoader::CBAMDataLoader(const string& loader_name,
                               const SLoaderParams& params)
    : CDataLoader(loader_name)
{
    string dir_path = params.m_DirPath;
    if ( dir_path.empty() ) {
        dir_path = NCBI_PARAM_TYPE(BAM, DIR_PATH)::GetDefault();
    }
    m_Impl.Reset(new CBAMDataLoader_Impl(params));
}


CBAMDataLoader::~CBAMDataLoader(void)
{
}


CDataLoader::TBlobId CBAMDataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    return TBlobId(m_Impl->GetShortSeqBlobId(idh).GetPointerOrNull());
}


CDataLoader::TBlobId
CBAMDataLoader::GetBlobIdFromString(const string& str) const
{
    return TBlobId(new CBAMBlobId(str));
}


bool CBAMDataLoader::CanGetBlobById(void) const
{
    return true;
}


CDataLoader::TTSE_LockSet
CBAMDataLoader::GetRecords(const CSeq_id_Handle& idh,
                           EChoice choice)
{
    TTSE_LockSet locks;
    if ( choice == eOrphanAnnot ) {
        // alignment by refseqid
        TBlobId blob_id(m_Impl->GetRefSeqBlobId(idh).GetPointerOrNull());
        if ( blob_id ) {
            locks.insert(GetBlobById(blob_id));
        }
    }
    else {
        // shortseqid || alignment by shortseqid
        // look in the already loaded TSEs
        TBlobId blob_id = GetBlobId(idh);
        if ( blob_id ) {
            locks.insert(GetBlobById(blob_id));
        }
    }
    return locks;
}


void CBAMDataLoader::GetChunk(TChunk chunk)
{
    TBlobId blob_id = chunk->GetBlobId();
    const CBAMBlobId& bam_id = dynamic_cast<const CBAMBlobId&>(*blob_id);
    m_Impl->LoadChunk(bam_id, *chunk);
}


void CBAMDataLoader::GetChunks(const TChunkSet& chunks)
{
    ITERATE ( TChunkSet, it, chunks ) {
        GetChunk(*it);
    }
}


CDataLoader::TTSE_Lock
CBAMDataLoader::GetBlobById(const TBlobId& blob_id)
{
    CTSE_LoadLock load_lock = GetDataSource()->GetTSE_LoadLock(blob_id);
    if ( !load_lock.IsLoaded() ) {
        const CBAMBlobId& bam_id = dynamic_cast<const CBAMBlobId&>(*blob_id);
        m_Impl->LoadBAMEntry(bam_id, load_lock);
        load_lock.SetLoaded();
    }
    return load_lock;
}


CBAMDataLoader::TAnnotNames CBAMDataLoader::GetPossibleAnnotNames(void) const
{
    return m_Impl->GetPossibleAnnotNames();
}


void CBAMDataLoader::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    m_Impl->GetIds(idh, ids);
}


CSeq_id_Handle CBAMDataLoader::GetAccVer(const CSeq_id_Handle& idh)
{
    return m_Impl->GetAccVer(idh);
}


TGi CBAMDataLoader::GetGi(const CSeq_id_Handle& idh)
{
    return m_Impl->GetGi(idh);
}


string CBAMDataLoader::GetLabel(const CSeq_id_Handle& idh)
{
    return m_Impl->GetLabel(idh);
}


int CBAMDataLoader::GetTaxId(const CSeq_id_Handle& idh)
{
    return m_Impl->GetTaxId(idh);
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_BAM(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_DataLoader_Bam);
}


const string kDataLoader_Bam_DriverName("bam");

class CBAM_DataLoaderCF : public CDataLoaderFactory
{
public:
    CBAM_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_Bam_DriverName) {}
    virtual ~CBAM_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CBAM_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CBAMDataLoader::RegisterInObjectManager(om).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CBAMDataLoader::RegisterInObjectManager(
        om,
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_DataLoader_Bam(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CBAM_DataLoaderCF>::NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_bam(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_Bam(info_list, method);
}


END_NCBI_SCOPE
