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
 * Author: Philip Johnson, Mike DiCuccio
 *
 * File Description: CDD consensus sequence data loader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

#include <objtools/data_loaders/cdd/cdd.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CCddDataLoader::TRegisterLoaderInfo CCddDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TMaker maker;
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CCddDataLoader::GetLoaderNameFromArgs(void)
{
    return "CDD_DataLoader";
}


CCddDataLoader::CCddDataLoader(const string& loader_name)
    : CDataLoader(loader_name)
{
}


/*---------------------------------------------------------------------------*/
// PRE : ??
// POST: ??
CDataLoader::TTSE_LockSet
CCddDataLoader::GetRecords(const CSeq_id_Handle& idh,
                           EChoice /* choice */)
{
    TTSE_LockSet locks;
    CConstRef<CSeq_id> id = idh.GetSeqId();
    if ( !id  ||  !id->IsGeneral()  ||  id->GetGeneral().GetDb() != "CDD") {
        return locks;
    }

    CMutexGuard LOCK(m_Mutex);

    int cdd_id = id->GetGeneral().GetTag().GetId();
    TCddEntries::iterator iter = m_Entries.find(cdd_id);
    if (iter != m_Entries.end()) {
        // found, already added
        CConstRef<CObject> blob_id(&*iter->second);
        CTSE_LoadLock load_lock =
            GetDataSource()->GetTSE_LoadLock(blob_id);
        if ( !load_lock.IsLoaded() ) {
            locks.insert(GetDataSource()->AddTSE(*iter->second));
            load_lock.SetLoaded();
        }
        return locks;
    }

    // retrieve the seq-entry, if possible
    _TRACE("CCddDataLoader(): loading id = " << id->AsFastaString());

    // create HTTP connection to CDD server
    string url("http://web.ncbi.nlm.nih.gov/Structure/cdd/cddsrv.cgi");
    string params("uid=");
    params += NStr::IntToString(cdd_id);
    params += "&getcseq";
    CConn_HttpStream inHttp(url);
    inHttp << params;


    CRef<CSeq_entry> entry(new CSeq_entry());
    try {
        auto_ptr<CObjectIStream> is
            (CObjectIStream::Open(eSerial_AsnText, inHttp));
        *is >> *entry;
    }
    catch (...) {
        _TRACE("CCddDataLoader(): failed to parse data");
    }

    // save our entry in all relevant places
    m_Entries[cdd_id] = entry;
    CConstRef<CObject> blob_id(&*entry);
    CTSE_LoadLock load_lock = GetDataSource()->GetTSE_LoadLock(blob_id);
    _ASSERT(!load_lock.IsLoaded());
    locks.insert(GetDataSource()->AddTSE(*entry));
    load_lock.SetLoaded();
    return locks;
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_CDD(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_DataLoader_Cdd);
}


const string kDataLoader_Cdd_DriverName("cdd");

class CCdd_DataLoaderCF : public CDataLoaderFactory
{
public:
    CCdd_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_Cdd_DriverName) {}
    virtual ~CCdd_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CCdd_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CCddDataLoader::RegisterInObjectManager(om).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CCddDataLoader::RegisterInObjectManager(
        om,
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_DataLoader_Cdd(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CCdd_DataLoaderCF>::NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_cdd(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_Cdd(info_list, method);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2005/02/02 19:49:55  grichenk
 * Fixed more warnings
 *
 * Revision 1.10  2004/12/22 20:42:53  grichenk
 * Added entry points registration funcitons
 *
 * Revision 1.9  2004/08/10 16:56:11  grichenk
 * Fixed dll export declarations, moved entry points to cpp.
 *
 * Revision 1.8  2004/08/04 14:56:35  vasilche
 * Updated to changes in TSE locking scheme.
 *
 * Revision 1.7  2004/08/02 17:34:43  grichenk
 * Added data_loader_factory.cpp.
 * Renamed xloader_cdd to ncbi_xloader_cdd.
 * Implemented data loader factories for all loaders.
 *
 * Revision 1.6  2004/07/28 14:02:57  grichenk
 * Improved MT-safety of RegisterInObjectManager(), simplified the code.
 *
 * Revision 1.5  2004/07/26 14:13:32  grichenk
 * RegisterInObjectManager() return structure instead of pointer.
 * Added CObjectManager methods to manipuilate loaders.
 *
 * Revision 1.4  2004/07/21 15:51:26  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.3  2004/05/21 21:42:52  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.2  2003/11/28 13:41:09  dicuccio
 * Fixed to match new API in CDataLoader
 *
 * Revision 1.1  2003/10/20 17:47:49  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
