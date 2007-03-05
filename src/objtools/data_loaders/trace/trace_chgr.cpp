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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <objtools/data_loaders/trace/trace_chgr.hpp>
#include <objects/id1/id1_client.hpp>
#include <objects/id1/ID1server_maxcomplex.hpp>
#include <objects/id1/ID1SeqEntry_info.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CTraceChromatogramLoader::TRegisterLoaderInfo
CTraceChromatogramLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    TMaker maker;
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CTraceChromatogramLoader::GetLoaderNameFromArgs(void)
{
    return "TRACE_CHGR_LOADER";
}


CTraceChromatogramLoader::CTraceChromatogramLoader()
    : CDataLoader(GetLoaderNameFromArgs())
{
    return;
}


CTraceChromatogramLoader::CTraceChromatogramLoader(const string& loader_name)
    : CDataLoader(loader_name)
{
    return;
}


CTraceChromatogramLoader::~CTraceChromatogramLoader()
{
    return;
}


CDataLoader::TTSE_LockSet
CTraceChromatogramLoader::GetRecords(const CSeq_id_Handle& idh,
                                     EChoice /* choice */)
{
    TTSE_LockSet locks;
    // we only handle a particular subset of seq-ids
    // we look for IDs for ids of the form 'gnl|ti|###' or 'gnl|TRACE|###'
    const CSeq_id* id = idh.GetSeqId();
    if ( !id  ||
         !id->IsGeneral() ||
         (id->GetGeneral().GetDb() != "ti"  &&
          id->GetGeneral().GetDb() != "TRACE")  ||
          !id->GetGeneral().GetTag().IsId()) {
        return locks;
    }

    int ti = id->GetGeneral().GetTag().GetId();
    TBlobId blob_id = new CBlobIdInt(ti);
    CTSE_LoadLock load_lock = GetDataSource()->GetTSE_LoadLock(blob_id);
    if ( load_lock.IsLoaded() ) {
        if ( load_lock->HasSeq_entry() ) {
            locks.insert(load_lock);
        }
        return locks;
    }

    CID1server_maxcomplex maxplex;
    maxplex.SetMaxplex(16);
    maxplex.SetGi(0);
    maxplex.SetSat("TRACE_CHGR");
    maxplex.SetEnt(ti);

    CRef<CSeq_entry> entry;
    CRef<CID1SeqEntry_info> info = x_GetClient().AskGetsewithinfo(maxplex);
    if (info) {
        entry.Reset(&info->SetBlob());
    }
    if (!entry) {
        load_lock.SetLoaded();
        return locks;
    }
    load_lock->SetSeq_entry(*entry);
    load_lock.SetLoaded();
    locks.insert(load_lock);
    return locks;
}


CID1Client& CTraceChromatogramLoader::x_GetClient()
{
    if ( !m_Client ) {
        m_Client.Reset(new CID1Client());
    }
    return *m_Client;
}



END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_Trace(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_DataLoader_Trace);
}


const string kDataLoader_Trace_DriverName("trace");

class CTrace_DataLoaderCF : public CDataLoaderFactory
{
public:
    CTrace_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_Trace_DriverName) {}
    virtual ~CTrace_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CTrace_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CTraceChromatogramLoader::
            RegisterInObjectManager(om).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CTraceChromatogramLoader::RegisterInObjectManager(
        om,
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_DataLoader_Trace(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CTrace_DataLoaderCF>::
        NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_trace(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_Trace(info_list, method);
}


END_NCBI_SCOPE
