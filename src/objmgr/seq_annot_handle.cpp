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
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/seq_align_handle.hpp>
#include <objmgr/seq_graph_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>

#include <objmgr/impl/seq_annot_edit_commands.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_annot_Handle::CSeq_annot_Handle(const CSeq_annot_Info& info,
                                     const CTSE_Handle& tse)
    : m_Info(tse.x_GetScopeInfo().GetScopeLock(tse, info))
{
}


void CSeq_annot_Handle::Reset(void)
{
    m_Info.Reset();
}


const CSeq_annot_Info& CSeq_annot_Handle::x_GetInfo(void) const
{
    return m_Info->GetObjectInfo();
}


CConstRef<CSeq_annot> CSeq_annot_Handle::GetCompleteSeq_annot(void) const
{
    return x_GetInfo().GetCompleteSeq_annot();
}


CConstRef<CSeq_annot> CSeq_annot_Handle::GetSeq_annotCore(void) const
{
    return GetCompleteSeq_annot();
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


CSeq_annot_EditHandle::CSeq_annot_EditHandle(const CSeq_annot_Handle& h)
    : CSeq_annot_Handle(h)
{
    if ( !h.GetTSE_Handle().CanBeEdited() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "object is not in editing mode");
    }
}


CSeq_annot_EditHandle::CSeq_annot_EditHandle(CSeq_annot_Info& info,
                                             const CTSE_Handle& tse)
    : CSeq_annot_Handle(info, tse)
{
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
    typedef CRemoveAnnot_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, x_GetScopeImpl()));   
    //    x_GetScopeImpl().RemoveAnnot(*this);
}


CSeq_feat_EditHandle
CSeq_annot_EditHandle::AddFeat(const CSeq_feat& new_obj) const
{

    typedef CSeq_annot_Add_EditCommand<CSeq_feat_EditHandle> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, new_obj));
    //    return CSeq_feat_Handle(*this, x_GetInfo().Add(new_obj));
}


CSeq_align_Handle CSeq_annot_EditHandle::AddAlign(const CSeq_align& new_obj) const
{
    typedef CSeq_annot_Add_EditCommand<CSeq_align_Handle> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, new_obj));
    //    return CSeq_align_Handle(*this, x_GetInfo().Add(new_obj));
}


CSeq_graph_Handle CSeq_annot_EditHandle::AddGraph(const CSeq_graph& new_obj) const
{
    typedef CSeq_annot_Add_EditCommand<CSeq_graph_Handle> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, new_obj));
}


CSeq_feat_EditHandle
CSeq_annot_EditHandle::TakeFeat(const CSeq_feat_EditHandle& handle) const
{
    CScopeTransaction guard = handle.GetScope().GetTransaction();
    CConstRef<CSeq_feat> obj = handle.GetSeq_feat();
    handle.Remove();
    CSeq_feat_EditHandle ret = AddFeat(*obj);
    guard.Commit();
    return ret;
}


CSeq_graph_Handle
CSeq_annot_EditHandle::TakeGraph(const CSeq_graph_Handle& handle) const
{
    CScopeTransaction guard = handle.GetScope().GetTransaction();
    CConstRef<CSeq_graph> obj = handle.GetSeq_graph();
    handle.Remove();
    CSeq_graph_Handle ret = AddGraph(*obj);
    guard.Commit();
    return ret;
}


CSeq_align_Handle
CSeq_annot_EditHandle::TakeAlign(const CSeq_align_Handle& handle) const
{
    CScopeTransaction guard = handle.GetScope().GetTransaction();
    CConstRef<CSeq_align> obj = handle.GetSeq_align();
    handle.Remove();
    CSeq_align_Handle ret = AddAlign(*obj);
    guard.Commit();
    return ret;
}


void
CSeq_annot_EditHandle::TakeAllAnnots(const CSeq_annot_EditHandle& annot) const
{
    if ( Which() != annot.Which() ) {
        NCBI_THROW(CAnnotException, eIncomatibleType,
                   "different Seq-annot types");
    }
    CScopeTransaction guard = annot.GetScope().GetTransaction();
    switch ( annot.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
        for ( CSeq_annot_ftable_I it(annot); it; ++it ) {
            TakeFeat(*it);
        }
        break;
    case CSeq_annot::C_Data::e_Graph:
        NCBI_THROW(CObjMgrException, eNotImplemented,
                   "taking graphs is not implemented yet");
        break;
    case CSeq_annot::C_Data::e_Align:
        NCBI_THROW(CObjMgrException, eNotImplemented,
                   "taking aligns is not implemented yet");
        break;
    case CSeq_annot::C_Data::e_Locs:
        NCBI_THROW(CObjMgrException, eNotImplemented,
                   "taking locs is not implemented yet");
        break;
    default:
        break;
    }
    guard.Commit();
}


CSeq_feat_EditHandle 
CSeq_annot_EditHandle::x_RealAdd(const CSeq_feat& new_obj) const
{

    CSeq_feat_EditHandle handle(*this, x_GetInfo().Add(new_obj));
    x_GetScopeImpl().x_ClearAnnotCache();
    return handle;
}


CSeq_align_Handle 
CSeq_annot_EditHandle::x_RealAdd(const CSeq_align& new_obj) const
{
    CSeq_align_Handle handle(*this, x_GetInfo().Add(new_obj));
    x_GetScopeImpl().x_ClearAnnotCache();
    return handle;
}


CSeq_graph_Handle 
CSeq_annot_EditHandle::x_RealAdd(const CSeq_graph& new_obj) const
{
    CSeq_graph_Handle handle(*this, x_GetInfo().Add(new_obj));
    x_GetScopeImpl().x_ClearAnnotCache();
    return handle;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.25  2006/09/26 18:03:32  vasilche
* Use transactions for complex operations.
*
* Revision 1.24  2006/09/19 19:22:02  vasilche
* Implemented TakeFeat() like methods.
*
* Revision 1.23  2006/08/07 15:25:05  vasilche
* CSeq_annot_EditHandle(CSeq_annot_Handle) made public and explicit.
* Introduced CSeq_feat_EditHandle.
*
* Revision 1.22  2006/02/02 14:28:19  vasilche
* Added TObject, GetCompleteObject(), GetObjectCore() for templates.
*
* Revision 1.21  2006/01/25 18:59:04  didenko
* Redisgned bio objects edit facility
*
* Revision 1.20  2005/11/15 22:00:57  ucko
* Don't return expressions from functions declared void.
*
* Revision 1.19  2005/11/15 19:22:08  didenko
* Added transactions and edit commands support
*
* Revision 1.18  2005/09/20 15:45:36  vasilche
* Feature editing API.
* Annotation handles remember annotations by index.
*
* Revision 1.17  2005/06/22 14:27:31  vasilche
* Implemented copying of shared Seq-entries at edit request.
* Added invalidation of handles to removed objects.
*
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
