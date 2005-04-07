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
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_annot_Handle::CSeq_annot_Handle(const CSeq_annot_Info& info,
                                     const CTSE_Handle& tse)
    : m_TSE(tse), m_Info(&info)
{
}


void CSeq_annot_Handle::Reset(void)
{
    // order is significant
    m_Info.Reset();
    m_TSE.Reset();
}


CSeq_annot_Handle& CSeq_annot_Handle::operator=(const CSeq_annot_Handle& sah)
{
    // order is significant
    if ( this != &sah ) {
        m_Info = sah.m_Info;
        m_TSE = sah.m_TSE;
    }
    return *this;
}


CConstRef<CSeq_annot> CSeq_annot_Handle::GetCompleteSeq_annot(void) const
{
    return x_GetInfo().GetCompleteSeq_annot();
}


CSeq_entry_Handle CSeq_annot_Handle::GetParentEntry(void) const
{
    return CSeq_entry_Handle(x_GetInfo().GetParentSeq_entry_Info(),
                             GetTSE_Handle());
}


CSeq_entry_Handle CSeq_annot_Handle::GetTopLevelEntry(void) const
{
    return GetTSE_Handle();
}


CSeq_annot_EditHandle CSeq_annot_Handle::GetEditHandle(void) const
{
    return x_GetScopeImpl().GetEditHandle(*this);
}


bool CSeq_annot_Handle::IsNamed(void) const
{
    return x_GetInfo().GetName().IsNamed();
}


const string& CSeq_annot_Handle::GetName(void) const
{
    return x_GetInfo().GetName().GetName();
}


const CSeq_annot& CSeq_annot_Handle::x_GetSeq_annotCore(void) const
{
    return *x_GetInfo().GetSeq_annotCore();
}


CSeq_annot::C_Data::E_Choice CSeq_annot_Handle::Which(void) const
{
    return x_GetSeq_annotCore().GetData().Which();
}


bool CSeq_annot_Handle::IsFtable(void) const
{
    return x_GetSeq_annotCore().GetData().IsFtable();
}


bool CSeq_annot_Handle::IsAlign(void) const
{
    return x_GetSeq_annotCore().GetData().IsAlign();
}


bool CSeq_annot_Handle::IsGraph(void) const
{
    return x_GetSeq_annotCore().GetData().IsGraph();
}


bool CSeq_annot_Handle::IsIds(void) const
{
    return x_GetSeq_annotCore().GetData().IsIds();
}


bool CSeq_annot_Handle::IsLocs(void) const
{
    return x_GetSeq_annotCore().GetData().IsLocs();
}


bool CSeq_annot_Handle::Seq_annot_IsSetId(void) const
{
    return x_GetSeq_annotCore().IsSetId();
}


bool CSeq_annot_Handle::Seq_annot_CanGetId(void) const
{
    return x_GetSeq_annotCore().CanGetId();
}


const CSeq_annot::TId& CSeq_annot_Handle::Seq_annot_GetId(void) const
{
    return x_GetSeq_annotCore().GetId();
}


bool CSeq_annot_Handle::Seq_annot_IsSetDb(void) const
{
    return x_GetSeq_annotCore().IsSetDb();
}


bool CSeq_annot_Handle::Seq_annot_CanGetDb(void) const
{
    return x_GetSeq_annotCore().CanGetDb();
}


CSeq_annot::TDb CSeq_annot_Handle::Seq_annot_GetDb(void) const
{
    return x_GetSeq_annotCore().GetDb();
}


bool CSeq_annot_Handle::Seq_annot_IsSetName(void) const
{
    return x_GetSeq_annotCore().IsSetName();
}


bool CSeq_annot_Handle::Seq_annot_CanGetName(void) const
{
    return x_GetSeq_annotCore().CanGetName();
}


const CSeq_annot::TName& CSeq_annot_Handle::Seq_annot_GetName(void) const
{
    return x_GetSeq_annotCore().GetName();
}


bool CSeq_annot_Handle::Seq_annot_IsSetDesc(void) const
{
    return x_GetSeq_annotCore().IsSetDesc();
}


bool CSeq_annot_Handle::Seq_annot_CanGetDesc(void) const
{
    return x_GetSeq_annotCore().CanGetDesc();
}


const CSeq_annot::TDesc& CSeq_annot_Handle::Seq_annot_GetDesc(void) const
{
    return x_GetSeq_annotCore().GetDesc();
}


CSeq_annot_Info& CSeq_annot_EditHandle::x_GetInfo(void) const
{
    return const_cast<CSeq_annot_Info&>(CSeq_annot_Handle::x_GetInfo());
}


CSeq_entry_EditHandle CSeq_annot_EditHandle::GetParentEntry(void) const
{
    return CSeq_entry_EditHandle(x_GetInfo().GetParentSeq_entry_Info(),
                                 GetTSE_Handle());
}


void CSeq_annot_EditHandle::Remove(void) const
{
    x_GetScopeImpl().RemoveAnnot(*this);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.16  2005/04/07 16:30:42  vasilche
* Inlined handles' constructors and destructors.
* Optimized handles' assignment operators.
*
* Revision 1.15  2005/01/06 16:41:31  grichenk
* Removed deprecated methods
*
* Revision 1.14  2005/01/03 21:51:58  grichenk
* Added proxy methods for CSeq_annot getters.
*
* Revision 1.13  2004/12/22 15:56:04  vasilche
* Introduced CTSE_Handle.
*
* Revision 1.12  2004/10/29 16:29:47  grichenk
* Prepared to remove deprecated methods, added new constructors.
*
* Revision 1.11  2004/08/05 18:28:17  vasilche
* Fixed order of CRef<> release in destruction and assignment of handles.
*
* Revision 1.10  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.9  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.8  2004/04/29 15:44:30  grichenk
* Added GetTopLevelEntry()
*
* Revision 1.7  2004/03/24 18:30:30  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.6  2004/03/16 21:01:32  vasilche
* Added methods to move Bioseq withing Seq-entry
*
* Revision 1.5  2004/03/16 15:47:28  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.4  2003/10/08 17:55:53  vasilche
* Fixed null initialization of CHeapScope.
*
* Revision 1.3  2003/10/08 14:14:27  vasilche
* Use CHeapScope instead of CRef<CScope> internally.
*
* Revision 1.2  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.1  2003/09/30 16:22:03  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* ===========================================================================
*/
