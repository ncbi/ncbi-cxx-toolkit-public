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

    /// Eemove current annot
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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.27  2006/09/19 19:22:02  vasilche
* Implemented TakeFeat() like methods.
*
* Revision 1.26  2006/08/07 15:25:05  vasilche
* CSeq_annot_EditHandle(CSeq_annot_Handle) made public and explicit.
* Introduced CSeq_feat_EditHandle.
*
* Revision 1.25  2006/02/02 14:28:19  vasilche
* Added TObject, GetCompleteObject(), GetObjectCore() for templates.
*
* Revision 1.24  2005/11/15 19:22:06  didenko
* Added transactions and edit commands support
*
* Revision 1.23  2005/10/18 15:38:12  vasilche
* Restore handles to inner objects when adding removed objects.
*
* Revision 1.22  2005/09/20 15:45:35  vasilche
* Feature editing API.
* Annotation handles remember annotations by index.
*
* Revision 1.21  2005/07/14 17:04:14  vasilche
* Fixed detaching from data loader.
* Implemented 'Removed' handles.
* Use 'Removed' handles when transferring object from one place to another.
* Fixed MT locking when removing/unlocking handles, clearing scope's history.
*
* Revision 1.20  2005/06/22 14:27:31  vasilche
* Implemented copying of shared Seq-entries at edit request.
* Added invalidation of handles to removed objects.
*
* Revision 1.19  2005/04/07 16:30:42  vasilche
* Inlined handles' constructors and destructors.
* Optimized handles' assignment operators.
*
* Revision 1.18  2005/03/17 17:52:27  grichenk
* Added flag to SAnnotSelector for skipping multiple SNPs from the same
* seq-annot. Optimized CAnnotCollector::GetAnnot().
*
* Revision 1.17  2005/01/24 17:09:36  vasilche
* Safe boolean operators.
*
* Revision 1.16  2005/01/12 17:16:14  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.15  2005/01/06 16:41:30  grichenk
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
* Revision 1.11  2004/09/29 16:52:34  kononenk
* Added doxygen formatting
*
* Revision 1.10  2004/08/05 18:28:17  vasilche
* Fixed order of CRef<> release in destruction and assignment of handles.
*
* Revision 1.9  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.8  2004/04/29 15:44:30  grichenk
* Added GetTopLevelEntry()
*
* Revision 1.7  2004/03/31 19:54:07  vasilche
* Fixed removal of bioseqs and bioseq-sets.
*
* Revision 1.6  2004/03/24 18:30:28  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.5  2004/03/16 21:01:32  vasilche
* Added methods to move Bioseq withing Seq-entry
*
* Revision 1.4  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.3  2003/10/08 14:14:54  vasilche
* Use CHeapScope instead of CRef<CScope> internally.
*
* Revision 1.2  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.1  2003/09/30 16:21:59  vasilche
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

#endif//SEQ_ANNOT_HANDLE__HPP
