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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Data loader base class for object manager
*
*/


#include <objects/objmgr/data_loader.hpp>
#include <objects/objmgr/seq_id_mapper.hpp>
#include <objects/objmgr/impl/tse_info.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CDataLoader::CDataLoader(void)
{
    m_Name = NStr::PtrToString(this);
    return;
}


CDataLoader::CDataLoader(const string& loader_name)
    : m_Name(loader_name)
{
    if (loader_name.empty())
    {
        m_Name = NStr::PtrToString(this);
    }
}


CDataLoader::~CDataLoader(void)
{
    return;
}


void CDataLoader::SetTargetDataSource(CDataSource& data_source)
{
    m_DataSource = &data_source;
}


CDataSource* CDataLoader::GetDataSource(void)
{
    return m_DataSource;
}


void CDataLoader::SetName(const string& loader_name)
{
    m_Name = loader_name;
}


string CDataLoader::GetName(void) const
{
    return m_Name;
}


bool CDataLoader::IsLive(const CTSE_Info& tse)
{
    return !tse.m_Dead;
}


bool CDataLoader::DropTSE(const CTSE_Info& /*tse_info*/)
{
    return true;
}


void CDataLoader::GC(void)
{
}


CConstRef<CTSE_Info> CDataLoader::ResolveConflict(const CSeq_id_Handle&,
                                                  const TTSE_LockSet&)
{
    return CConstRef<CTSE_Info>();
}


void CDataLoader::DebugDump(CDebugDumpContext, unsigned int) const
{
    return;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.8  2003/05/06 18:54:09  grichenk
* Moved TSE filtering from CDataSource to CScope, changed
* some filtering rules (e.g. priority is now more important
* than scope history). Added more caches to CScope.
*
* Revision 1.7  2003/04/29 19:51:13  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.6  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.5  2002/12/26 20:55:17  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.4  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.3  2002/05/06 03:28:46  vakatov
* OM/OM1 renaming
*
* Revision 1.2  2002/03/18 17:26:35  grichenk
* +CDataLoader::x_GetSeq_id(), x_GetSeq_id_Key(), x_GetSeq_id_Handle()
*
* Revision 1.1  2002/01/11 19:06:17  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
