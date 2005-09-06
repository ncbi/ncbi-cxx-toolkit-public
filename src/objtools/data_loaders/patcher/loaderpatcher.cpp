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
*  Author: Maxim Didenko
*
*  File Description:
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <objtools/data_loaders/patcher/loaderpatcher.hpp>
#include <objtools/data_loaders/patcher/datapatcher_iface.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_split_info.hpp>

//=======================================================================
// CDataLoaderPatcher Public interface 
//

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CDataLoaderPatcher::TRegisterLoaderInfo 
CDataLoaderPatcher::RegisterInObjectManager(
    CObjectManager& om,
    CRef<CDataLoader> data_loader,
    CRef<IDataPatcher> patcher,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SParam param(data_loader, patcher);
    TMaker maker(param);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}



string CDataLoaderPatcher::GetLoaderNameFromArgs(const SParam& param)
{
    return "PATCHER_" + param.m_DataLoader->GetName();
}


CDataLoaderPatcher::CDataLoaderPatcher(const string& loader_name,
                                       const SParam& param)
    : CDataLoader (loader_name),
      m_DataLoader(param.m_DataLoader),
      m_Patcher   (param.m_Patcher)
{
}

CDataLoaderPatcher::~CDataLoaderPatcher(void)
{
}

CDataLoader::TTSE_LockSet 
CDataLoaderPatcher::GetRecords(const CSeq_id_Handle& idh,
                               EChoice choice)
{
    TTSE_LockSet locks;
    CRef<ISeq_id_Translator> translator = m_Patcher->GetSeqIdTranslator();
    if (translator) {
        CSeq_id_Handle pid = translator->TranslateToPatched(idh);
        if (!pid)
            return locks;
        locks = m_DataLoader->GetRecords(pid, choice);
    } else {
        locks = m_DataLoader->GetRecords(idh, choice);
    }
    TTSE_LockSet nlocks;
    PatchLockSet(locks, nlocks);
    return nlocks;
}

CDataLoader::TTSE_LockSet 
CDataLoaderPatcher::GetDetailedRecords(const CSeq_id_Handle& idh,
                                       const SRequestDetails& details)
{
    TTSE_LockSet locks;
    CRef<ISeq_id_Translator> translator = m_Patcher->GetSeqIdTranslator();
    if (translator) {
        CSeq_id_Handle pid = translator->TranslateToPatched(idh);
        if (!pid)
            return locks;
        locks = m_DataLoader->GetDetailedRecords(pid, details);
    } else {
        locks = m_DataLoader->GetDetailedRecords(idh, details);
    }
    TTSE_LockSet nlocks;
    PatchLockSet(locks, nlocks);
    return nlocks;
}


CDataLoader::TTSE_LockSet 
CDataLoaderPatcher::GetExternalRecords(const CBioseq_Info& bioseq)
{
    TTSE_LockSet locks = m_DataLoader->GetExternalRecords(bioseq);
    TTSE_LockSet nlocks;
    PatchLockSet(locks, nlocks);
    return nlocks;
}

void CDataLoaderPatcher::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    CRef<ISeq_id_Translator> translator = m_Patcher->GetSeqIdTranslator();
    if (translator) {
        CSeq_id_Handle pid = translator->TranslateToPatched(idh);
        if (!pid)
            return ;
        m_DataLoader->GetIds(pid, ids);
    } else
        m_DataLoader->GetIds(idh, ids);
}

CDataLoader::TBlobId 
CDataLoaderPatcher::GetBlobId(const CSeq_id_Handle& idh)
{
    CRef<ISeq_id_Translator> translator = m_Patcher->GetSeqIdTranslator();
    if (translator) {
        CSeq_id_Handle pid = translator->TranslateToPatched(idh);
        return m_DataLoader->GetBlobId(pid);
    } else
        return m_DataLoader->GetBlobId(idh);

}

CDataLoader::TBlobVersion 
CDataLoaderPatcher::GetBlobVersion(const TBlobId& id)
{
    return m_DataLoader->GetBlobVersion(id);
}

bool CDataLoaderPatcher::CanGetBlobById(void) const
{
    return m_DataLoader->CanGetBlobById();
}
CDataLoader::TTSE_Lock 
CDataLoaderPatcher::GetBlobById(const TBlobId& blob_id)
{
    return PatchLock(m_DataLoader->GetBlobById(blob_id));
}

bool CDataLoaderPatcher::LessBlobId(const TBlobId& id1, const TBlobId& id2) const
{
    return m_DataLoader->LessBlobId(id1, id2);
    //return id1 < id2;
}
void CDataLoaderPatcher::GetChunk(TChunk chunk_info)
{
    m_DataLoader->GetChunk(chunk_info);
}
void CDataLoaderPatcher::GetChunks(const TChunkSet& chunks)
{
    m_DataLoader->GetChunks(chunks);
}

/*
string CDataLoaderPatcher::BlobIdToString(const TBlobId& id) const
{
    return m_DataLoader->BlobIdToString(id);
}
*/

/*
SRequestDetails CDataLoaderPatcher::ChoiceToDetails(EChoice choice) const
{
    return m_DataLoader->ChoiceToDetails(choice);
}
CDataLoaderPatcher::EChoice 
CDataLoaderPatcher::DetailsToChoice(const SRequestDetails::TAnnotSet& annots) const
{
    return m_DataLoader->DetailsToChoice(annots);
}

CDataLoaderPatcher::EChoice 
CDataLoaderPatcher::DetailsToChoice(const SRequestDetails& details) const
{
    return m_DataLoader->DetailsToChoice(details);
}
*/
 
CDataLoaderPatcher::TTSE_Lock
CDataLoaderPatcher::PatchLock(const TTSE_Lock& lock)
{
    const TBlobId& blob_id = (*lock).GetBlobId();
    CTSE_LoadLock nlock = GetDataSource()->GetTSE_LoadLock(blob_id);

    if (!nlock.IsLoaded()) {
        if (!m_Patcher->IsPatchNeeded(*lock) ) 
            nlock->Assign(lock);
        else {
            CRef<CSeq_entry> entry;
            CConstRef<CSeq_entry> orig_entry = lock->GetSeq_entrySkeleton();
            if( orig_entry ) {
                entry.Reset(new CSeq_entry);
                entry->Assign(*orig_entry);
                m_Patcher->Patch(*entry);
            }
            nlock->Assign(lock, entry, m_Patcher->GetAssigner());
            nlock->SetSeqIdTranslator(m_Patcher->GetSeqIdTranslator());
        }

        nlock.SetLoaded();
    }
    return TTSE_Lock(nlock);
}   
void CDataLoaderPatcher::PatchLockSet(const TTSE_LockSet& orig_locks, 
                                      TTSE_LockSet& new_locks)
{
    ITERATE(TTSE_LockSet, it, orig_locks) {
        const TTSE_Lock& lock = *it;
        new_locks.insert(PatchLock(lock)); 
    }
}   

END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_Patcher(void)
{
    // Typedef to silence compiler warning.  A better solution to this
    // problem is probably possible.
    
    typedef void(*TArgFuncType)(list<CPluginManager<CDataLoader>
                                ::SDriverInfo> &,
                                CPluginManager<CDataLoader>
                                ::EEntryPointRequest);
    
    RegisterEntryPoint<CDataLoader>((TArgFuncType)
                                    NCBI_EntryPoint_DataLoader_Patcher);
}

const string kDataLoader_Patcher_DriverName("dlpatcher");

/// Data Loader Factory for BlastDbDataLoader
///
/// This class provides an interface which builds an instance of the
/// BlastDbDataLoader and registers it with the object manager.

class CDLPatcher_DataLoaderCF : public CDataLoaderFactory
{
public:
    /// Constructor
    CDLPatcher_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_Patcher_DriverName) {}
    
    /// Destructor
    virtual ~CDLPatcher_DataLoaderCF(void) {}
    
protected:
    /// Create and register a data loader
    /// @param om
    ///   A reference to the object manager
    /// @param params
    ///   Arguments for the data loader constructor
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CDLPatcher_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        return NULL;
    }
    //Parse params, select constructor
    const string& data_loader =
        GetParam(GetDriverName(), params,
                 kCFParam_DLP_DataLoader, false, kEmptyStr);
    const string& data_patcher =
        GetParam(GetDriverName(), params,
                 kCFParam_DLP_DataPatcher, false, kEmptyStr);
    if ( !data_loader.empty() && !data_patcher.empty() ) {
        const TPluginManagerParamTree* dl_tree = 
            params->FindNode(data_loader);
        
        typedef CPluginManager<CDataLoader> TDLManager;
        TDLManager dl_manager;
        CRef<CDataLoader> dl(dl_manager.CreateInstance(data_loader,
                                                      TDLManager::GetDefaultDrvVers(),
                                                      dl_tree));
        const TPluginManagerParamTree* dp_tree = 
            params->FindNode(data_patcher);
        
        typedef CPluginManager<IDataPatcher> TDPManager;
        TDPManager dp_manager;
        CRef<IDataPatcher> dp(dp_manager.CreateInstance(data_patcher,
                                                        TDPManager::GetDefaultDrvVers(),
                                                        dp_tree));

        if (dl && dp) {
            return CDataLoaderPatcher::RegisterInObjectManager(
                             om,
                             dl,
                             dp,
                             GetIsDefault(params),
                             GetPriority(params)).GetLoader();
        }
    }
    return NULL;
}


void NCBI_EntryPoint_DataLoader_Patcher(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDLPatcher_DataLoaderCF>::
        NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_Patcher(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_Patcher(info_list, method);
}


END_NCBI_SCOPE


/* ========================================================================== 
 * $Log$
 * Revision 1.3  2005/09/06 13:22:11  didenko
 * IDataPatcher interface moved to a separate file
 *
 * Revision 1.2  2005/08/31 19:36:44  didenko
 * Reduced the number of objects copies which are being created while doing PatchSeqIds
 *
 * Revision 1.1  2005/08/25 14:06:44  didenko
 * Added data loader patcher
 *
 * ========================================================================== */
