#ifndef SEQ_ANNOT_HANDLE__HPP
#define SEQ_ANNOT_HANDLE__HPP

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
*    Handle to Seq-annot object
*
*/

#include <corelib/ncbiobj.hpp>

#include <objmgr/tse_handle.hpp>
#include <objects/seq/Seq_annot.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerHandles
 *
 * @{
 */

class CSeq_annot;

class CScope;

class CSeq_annot_CI;
class CAnnotTypes_CI;
class CAnnot_CI;
class CSeq_annot_Handle;
class CSeq_annot_EditHandle;
class CSeq_entry_Handle;
class CSeq_entry_EditHandle;
class CSeq_feat_Handle;
class CSeq_align_Handle;
class CSeq_graph_Handle;
class CSeq_feat_EditHandle;
class CSeq_align_EditHandle;
class CSeq_graph_EditHandle;
class CSeq_annot_Info;


class CSeq_annot_ScopeInfo : public CScopeInfo_Base
{
public:
    typedef CSeq_annot_Info TObjectInfo;

    CSeq_annot_ScopeInfo(const CTSE_Handle& tse, const TObjectInfo& info)
        : CScopeInfo_Base(tse, reinterpret_cast<const CTSE_Info_Object&>(info))
        {
        }

    const TObjectInfo& GetObjectInfo(void) const
        {
            return reinterpret_cast<const TObjectInfo&>(GetObjectInfo_Base());
        }
    TObjectInfo& GetNCObjectInfo(void)
        {
            return const_cast<TObjectInfo&>(GetObjectInfo());
        }
};



/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_annot_Handle --
///
///  Proxy to access seq-annot objects
///

class NCBI_XOBJMGR_EXPORT CSeq_annot_Handle
{
public:
    CSeq_annot_Handle(void);


    DECLARE_OPERATOR_BOOL(m_Info.IsValid());

    bool IsRemoved(void) const;


    // Get CTSE_Handle of containing TSE
    const CTSE_Handle& GetTSE_Handle(void) const;


    // Reset handle and make it not to point to any seq-annot
    void Reset(void);

    /// Check if handles point to the same seq-annot
    ///
    /// @sa
    ///     operator!=()
    bool operator==(const CSeq_annot_Handle& annot) const;

    // Check if handles point to different seq-annot
    ///
    /// @sa
    ///     operator==()
    bool operator!=(const CSeq_annot_Handle& annot) const;

    /// For usage in containers
    bool operator<(const CSeq_annot_Handle& annot) const;

    /// Get scope this handle belongs to
    CScope& GetScope(void) const;

    /// Complete and return const reference to the current seq-annot
    CConstRef<CSeq_annot> GetCompleteSeq_annot(void) const;
    CConstRef<CSeq_annot> GetSeq_annotCore(void) const;

    /// Unified interface for templates
    typedef CSeq_annot TObject;
    CConstRef<TObject> GetCompleteObject(void) const;
    CConstRef<TObject> GetObjectCore(void) const;

    /// Get parent Seq-entry handle
    ///
    /// @sa 
    ///     GetSeq_entry_Handle()
    CSeq_entry_Handle GetParentEntry(void) const;

    /// Get top level Seq-entry handle
    CSeq_entry_Handle GetTopLevelEntry(void) const;

    /// Get 'edit' version of handle
    CSeq_annot_EditHandle GetEditHandle(void) const;

    // Seq-annot accessors
    bool IsNamed(void) const;
    const string& GetName(void) const;

    // Mappings for CSeq_annot::C_Data methods
    CSeq_annot::C_Data::E_Choice Which(void) const;
    bool IsFtable(void) const;
    bool IsAlign(void) const;
    bool IsGraph(void) const;
    bool IsIds(void) const;
    bool IsLocs(void) const;

    bool Seq_annot_IsSetId(void) const;
    bool Seq_annot_CanGetId(void) const;
    const CSeq_annot::TId& Seq_annot_GetId(void) const;

    bool Seq_annot_IsSetDb(void) const;
    bool Seq_annot_CanGetDb(void) const;
    CSeq_annot::TDb Seq_annot_GetDb(void) const;

    bool Seq_annot_IsSetName(void) const;
    bool Seq_annot_CanGetName(void) const;
    const CSeq_annot::TName& Seq_annot_GetName(void) const;

    bool Seq_annot_IsSetDesc(void) const;
    bool Seq_annot_CanGetDesc(void) const;
    const CSeq_annot::TDesc& Seq_annot_GetDesc(void) const;

protected:
    friend class CScope_Impl;
    friend class CSeq_annot_CI;
    friend class CAnnot_Collector;
    friend class CMappedFeat;

    typedef CSeq_annot_ScopeInfo TScopeInfo;
    CSeq_annot_Handle(const CSeq_annot_Info& annot, const CTSE_Handle& tse);

    CScopeInfo_Ref<TScopeInfo>  m_Info;

public: // non-public section
    const TScopeInfo& x_GetScopeInfo(void) const;
    const CSeq_annot_Info& x_GetInfo(void) const;
    const CSeq_annot& x_GetSeq_annotCore(void) const;

    CScope_Impl& x_GetScopeImpl(void) const;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_annot_EditHandle --
///
///  Proxy to access and edit seq-annot objects
///

template<typename Handle>
class CSeq_annot_Add_EditCommand;

class NCBI_XOBJMGR_EXPORT CSeq_annot_EditHandle : public CSeq_annot_Handle
{
public:
    CSeq_annot_EditHandle(void);
    /// create edit interface class to the object which already allows editing
    /// throw an exception if the argument is not in editing mode
    explicit CSeq_annot_EditHandle(const CSeq_annot_Handle& h);

    /// Navigate object tree
    CSeq_entry_EditHandle GetParentEntry(void) const;

    /// Remove current annot
    void Remove(void) const;

    // Individual annotations modifications.
    // For all AddXxx and ReplaceXxx methods the ownership
    // of new_obj argument will be taken by CSeq_annot_Handle,
    // and the object should not be modified after the call.

    CSeq_feat_EditHandle AddFeat(const CSeq_feat& new_obj) const;
    CSeq_align_Handle AddAlign(const CSeq_align& new_obj) const;
    CSeq_graph_Handle AddGraph(const CSeq_graph& new_obj) const;

    // Remove handle from its current Seq-annot and add it here
    CSeq_feat_EditHandle TakeFeat(const CSeq_feat_EditHandle& handle) const;
    CSeq_align_Handle TakeAlign(const CSeq_align_Handle& handle) const;
    CSeq_graph_Handle TakeGraph(const CSeq_graph_Handle& handle) const;

    void TakeAllAnnots(const CSeq_annot_EditHandle& annot) const;

    /// Update index after manual modification of the object
    void Update(void) const;

protected:
    friend class CScope_Impl;
    friend class CBioseq_EditHandle;
    friend class CBioseq_set_EditHandle;
    friend class CSeq_entry_EditHandle;

    CSeq_annot_EditHandle(CSeq_annot_Info& info, const CTSE_Handle& tse);

public: // non-public section
    TScopeInfo& x_GetScopeInfo(void) const;
    CSeq_annot_Info& x_GetInfo(void) const;

public:
    friend class CSeq_annot_Add_EditCommand<CSeq_feat_EditHandle>;
    friend class CSeq_annot_Add_EditCommand<CSeq_align_Handle>;
    friend class CSeq_annot_Add_EditCommand<CSeq_graph_Handle>;

    CSeq_feat_EditHandle x_RealAdd(const CSeq_feat& new_obj) const;
    CSeq_align_Handle x_RealAdd(const CSeq_align& new_obj) const;
    CSeq_graph_Handle x_RealAdd(const CSeq_graph& new_obj) const;

};


/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_Handle inline methods
/////////////////////////////////////////////////////////////////////////////


inline
CSeq_annot_Handle::CSeq_annot_Handle(void)
{
}


inline
const CTSE_Handle& CSeq_annot_Handle::GetTSE_Handle(void) const
{
    return m_Info->GetTSE_Handle();
}


inline
CScope& CSeq_annot_Handle::GetScope(void) const
{
    return GetTSE_Handle().GetScope();
}


inline
CScope_Impl& CSeq_annot_Handle::x_GetScopeImpl(void) const
{
    return GetTSE_Handle().x_GetScopeImpl();
}


inline
const CSeq_annot_ScopeInfo& CSeq_annot_Handle::x_GetScopeInfo(void) const
{
    return *m_Info;
}


inline
bool CSeq_annot_Handle::IsRemoved(void) const
{
    return m_Info.IsRemoved();
}


inline
bool CSeq_annot_Handle::operator==(const CSeq_annot_Handle& handle) const
{
    return m_Info == handle.m_Info;
}


inline
bool CSeq_annot_Handle::operator!=(const CSeq_annot_Handle& handle) const
{
    return m_Info != handle.m_Info;
}


inline
bool CSeq_annot_Handle::operator<(const CSeq_annot_Handle& handle) const
{
    return m_Info < handle.m_Info;
}


inline
CConstRef<CSeq_annot> CSeq_annot_Handle::GetCompleteObject(void) const
{
    return GetCompleteSeq_annot();
}


inline
CConstRef<CSeq_annot> CSeq_annot_Handle::GetObjectCore(void) const
{
    return GetSeq_annotCore();
}


inline
CSeq_annot_EditHandle::CSeq_annot_EditHandle(void)
{
}


inline
CSeq_annot_ScopeInfo& CSeq_annot_EditHandle::x_GetScopeInfo(void) const
{
    return m_Info.GetNCObject();
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//SEQ_ANNOT_HANDLE__HPP
