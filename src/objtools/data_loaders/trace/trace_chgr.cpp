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
#include <objmgr/data_loader_factory.hpp>
#include <objtools/data_loaders/trace/trace_chgr.hpp>
#include <objects/id1/id1_client.hpp>
#include <objects/id1/ID1server_maxcomplex.hpp>
#include <objects/id1/ID1SeqEntry_info.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <corelib/plugin_manager_impl.hpp>


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


void CTraceChromatogramLoader::GetRecords(const CSeq_id_Handle& idh,
                                          EChoice choice)
{
    // we only handle a particular subset of seq-ids
    // we look for IDs for ids of the form 'gnl|ti|###' or 'gnl|TRACE|###'
    const CSeq_id* id = idh.GetSeqId();
    if ( !id  ||
         !id->IsGeneral() ||
         (id->GetGeneral().GetDb() != "ti"  &&
          id->GetGeneral().GetDb() != "TRACE")  ||
          !id->GetGeneral().GetTag().IsId()) {
        return;
    }

    int ti = id->GetGeneral().GetTag().GetId();
    CMutexGuard LOCK(m_Mutex);
    TTraceEntries::const_iterator iter = m_Entries.find(ti);
    if (iter != m_Entries.end()) {
        return;
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
    m_Entries[ti] = entry;
    if (entry) {
        GetDataSource()->AddTSE(*entry);
    }
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


extern "C"
{

void NCBI_EntryPoint_DataLoader_Trace(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CTrace_DataLoaderCF>::
        NCBI_EntryPointImpl(info_list, method);
}

}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/08/02 17:34:44  grichenk
 * Added data_loader_factory.cpp.
 * Renamed xloader_cdd to ncbi_xloader_cdd.
 * Implemented data loader factories for all loaders.
 *
 * Revision 1.5  2004/07/28 14:02:57  grichenk
 * Improved MT-safety of RegisterInObjectManager(), simplified the code.
 *
 * Revision 1.4  2004/07/26 14:13:32  grichenk
 * RegisterInObjectManager() return structure instead of pointer.
 * Added CObjectManager methods to manipuilate loaders.
 *
 * Revision 1.3  2004/07/21 15:51:26  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.2  2004/05/21 21:42:53  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.1  2004/03/25 14:20:25  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
