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
    TRegisterLoaderInfo info;
    string name = GetLoaderNameFromArgs();
    CDataLoader* loader = om.FindDataLoader(name);
    if ( loader ) {
        info.Set(loader, false);
        return info;
    }
    loader = new CCddDataLoader(name);
    CObjectManager::TRegisterLoaderInfo base_info =
        CDataLoader::RegisterInObjectManager(om, name, *loader,
                                             is_default, priority);
    info.Set(base_info.GetLoader(), base_info.IsCreated());
    return info;
}


string CCddDataLoader::GetLoaderNameFromArgs(void)
{
    return "CDD_DataLoader";
}


CCddDataLoader::CCddDataLoader(void)
    : CDataLoader(GetLoaderNameFromArgs())
{
}


CCddDataLoader::CCddDataLoader(const string& loader_name)
    : CDataLoader(loader_name)
{
}


/*---------------------------------------------------------------------------*/
// PRE : ??
// POST: ??
void CCddDataLoader::GetRecords(const CSeq_id_Handle& idh,
                                EChoice choice)
{
    CConstRef<CSeq_id> id = idh.GetSeqId();
    if ( !id  ||  !id->IsGeneral()  ||  id->GetGeneral().GetDb() != "CDD") {
        return;
    }

    CMutexGuard LOCK(m_Mutex);

    int cdd_id = id->GetGeneral().GetTag().GetId();
    TCddEntries::iterator iter = m_Entries.find(cdd_id);
    if (iter != m_Entries.end()) {
        // found, already added
        return;
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
    GetDataSource()->AddTSE(*entry);
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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
