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
#include <objects/seqtable/Seq_table.hpp>
#include <objmgr/feat_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_annot_Handle::CSeq_annot_Handle(const CSeq_annot_Info& info,
                                     const CTSE_Handle& tse)
    : m_Info(tse.x_GetScopeInfo().GetScopeLock(tse, info))
{
}


void CSeq_annot_Handle::x_Set(const CSeq_annot_Info& info,
                              const CTSE_Handle& tse)
{
    m_Info = tse.x_GetScopeInfo().GetScopeLock(tse, info);
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


bool CSeq_annot_Handle::OrderedBefore(const CSeq_annot_Handle& h) const
{
    if ( *this == h ) {
        return false;
    }
    if ( GetTSE_Handle() != h.GetTSE_Handle() ) {
        _ASSERT(GetTSE_Handle().OrderedBefore(h.GetTSE_Handle())||
                h.GetTSE_Handle().OrderedBefore(GetTSE_Handle()));
        return GetTSE_Handle().OrderedBefore(h.GetTSE_Handle());
    }
    if ( x_GetInfo().GetChunkId() != h.x_GetInfo().GetChunkId() ) {
        return x_GetInfo().GetChunkId() < h.x_GetInfo().GetChunkId();
    }
    if ( x_GetInfo().GetBioObjectId() != h.x_GetInfo().GetBioObjectId() ) {
        _ASSERT(x_GetInfo().GetBioObjectId()<h.x_GetInfo().GetBioObjectId()||
                h.x_GetInfo().GetBioObjectId()<x_GetInfo().GetBioObjectId());
        return x_GetInfo().GetBioObjectId() < h.x_GetInfo().GetBioObjectId();
    }
    return *this < h;
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


bool CSeq_annot_Handle::IsSeq_table(void) const
{
    return x_GetSeq_annotCore().GetData().IsSeq_table();
}


size_t CSeq_annot_Handle::GetSeq_tableNumRows(void) const
{
    return x_GetSeq_annotCore().GetData().GetSeq_table().GetNum_rows();
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
    return CSeq_feat_EditHandle(*this,
                                //CSeq_annot_Info::eNonTable,
                                x_GetInfo().Add(new_obj));
}


CSeq_align_Handle 
CSeq_annot_EditHandle::x_RealAdd(const CSeq_align& new_obj) const
{
    return CSeq_align_Handle(*this, x_GetInfo().Add(new_obj));
}


CSeq_graph_Handle 
CSeq_annot_EditHandle::x_RealAdd(const CSeq_graph& new_obj) const
{
    return CSeq_graph_Handle(*this, x_GetInfo().Add(new_obj));
}


void CSeq_annot_EditHandle::ReorderFtable(CFeat_CI& feat_ci) const
{
    vector<CSeq_feat_Handle> feats;
    feats.reserve(feat_ci.GetSize());
    for ( feat_ci.Rewind(); feat_ci; ++feat_ci ) {
        CSeq_feat_Handle feat = feat_ci->GetSeq_feat_Handle();
        if ( feat.GetAnnot() == *this ) {
            feats.push_back(feat);
        }
    }
    ReorderFtable(feats);
}


void CSeq_annot_EditHandle::ReorderFtable(const vector<CSeq_feat_Handle>& feats) const
{
    x_GetInfo().ReorderFtable(feats);
}


END_SCOPE(objects)
END_NCBI_SCOPE
