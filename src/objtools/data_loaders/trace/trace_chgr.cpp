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
#include <objtools/data_loaders/trace/trace_chgr.hpp>
#include <objects/id1/id1_client.hpp>
#include <objects/id1/ID1server_maxcomplex.hpp>
#include <objects/id1/ID1SeqEntry_info.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CTraceChromatogramLoader* CTraceChromatogramLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority  priority)
{
    string name = GetLoaderNameFromArgs();
    if ( om.FindDataLoader(name) ) {
        return 0;
    }
    CTraceChromatogramLoader* loader = new CTraceChromatogramLoader(name);
    return CDataLoader::RegisterInObjectManager(om, name, *loader,
                                                is_default, priority) ?
        loader : 0;
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
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
