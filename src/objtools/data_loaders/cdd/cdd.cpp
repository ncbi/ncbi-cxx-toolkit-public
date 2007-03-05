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
    TBlobId blob_id = new CBlobIdInt(cdd_id);
    CTSE_LoadLock load_lock = GetDataSource()->GetTSE_LoadLock(blob_id);
    if ( load_lock.IsLoaded() ) {
        if ( load_lock->HasSeq_entry() ) {
            locks.insert(load_lock);
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
    params += "&notag";  // tell server not to wrap ASN.1 in html tags
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
        load_lock.SetLoaded();
        return locks;
    }

    // save our entry in all relevant places
    load_lock->SetSeq_entry(*entry);
    load_lock.SetLoaded();
    locks.insert(load_lock);
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
