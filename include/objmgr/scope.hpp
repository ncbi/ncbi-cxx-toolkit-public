#ifndef SCOPE__HPP
#define SCOPE__HPP

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
* Authors:
*           Andrei Gourianov
*           Aleksey Grichenko
*           Michael Kimelman
*           Denis Vakatov
*           Eugene Vasilchenko
*
* File Description:
*           Scope is top-level object available to a client.
*           Its purpose is to define a scope of visibility and reference
*           resolution and provide access to the bio sequence data
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>

#include <objects/seq/Seq_inst.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_id_handle.hpp>
#include <objmgr/seq_id_mapper.hpp>

#include <objmgr/impl/priority.hpp>
#include <objmgr/impl/scope_info.hpp>
#include <objmgr/impl/mutex_pool.hpp>

#include <set>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// fwd decl
// objects
class CSeq_entry;
class CSeq_annot;
class CSeq_data;
class CSeq_id;
class CSeq_loc;
class CBioseq;

// objmgr
class CScope;
class CHeapScope;
class CObjectManager;
class CDataSource;
class CTSE_Info;
class CSeq_entry_Info;
class CSeq_annot_Info;
class CBioseq_Info;
class CSeq_id_Handle;
class CSeqMap;
class CSeqMatch_Info;
class CSynonymsSet;
class CBioseq_Handle;
class CHandleRangeMap;
struct CDataSource_ScopeInfo;
struct SAnnotTypeSelector;
struct SAnnotSelector;


// Base class for CBioseq_CI to make enums visible in CScope
class CBioseq_CI_Base {
public:
    enum EBioseqLevelFlag {
        eLevel_All,
        eLevel_Mains,
        eLevel_Parts
    };
};


/////////////////////////////////////////////////////////////////////////////
// CScope_Impl
/////////////////////////////////////////////////////////////////////////////

class NCBI_XOBJMGR_EXPORT CScope_Impl : public CObject
{
public:
    typedef CConstRef<CTSE_Info>                     TTSE_Lock;
    typedef set<TTSE_Lock>                           TTSE_LockSet;

    // History of requests
    typedef map<CSeq_id_Handle, SSeq_id_ScopeInfo>   TSeq_idMap;
    typedef TSeq_idMap::value_type                   TSeq_idMapValue;
    typedef CRef<CBioseq_ScopeInfo>                  TBioseqMapValue;
    typedef map<const CBioseq*, TBioseqMapValue>     TBioseqMap;
    typedef SSeq_id_ScopeInfo::TAnnotRefSet          TAnnotRefSet;
    typedef CPriorityNode::TPriority                 TPriority;

    enum EPriority {
        kPriority_NotSet = -1
    };

    // Add default data loaders from object manager
    void AddDefaults(TPriority priority = kPriority_NotSet);
    // Add data loader by name.
    // The loader (or its factory) must be known to Object Manager.
    void AddDataLoader(const string& loader_name,
                       TPriority priority = kPriority_NotSet);
    // Add seq_entry, default priority is higher than for defaults or loaders
    void AddTopLevelSeqEntry(CSeq_entry& top_entry,
                             TPriority priority = kPriority_NotSet);

    // Add the scope's datasources as a single group with the given priority
    void AddScope(CScope_Impl& scope,
                  TPriority priority = kPriority_NotSet);

    // Add bioseq, return bioseq handle. Try to use unresolved seq-id
    // from the bioseq, fail if all ids are already resolved to
    // other sequences.
    CBioseq_Handle AddBioseq(CBioseq& bioseq,
                             TPriority priority = kPriority_NotSet);

    // Add annotations to a seq-entry (seq or set)
    void AttachAnnot(CSeq_entry& entry, CSeq_annot& annot);
    void RemoveAnnot(CSeq_entry& entry, CSeq_annot& annot);
    void ReplaceAnnot(CSeq_entry& entry,
                      CSeq_annot& old_annot, CSeq_annot& new_annot);

    // Add new sub-entry to the existing tree if it is in this scope
    void AttachEntry(CSeq_entry& parent, CSeq_entry& entry);
    // Add sequence map for a bioseq if it is in this scope
    void AttachMap(CSeq_entry& bioseq, CSeqMap& seqmap);

    // Get bioseq handle by seq-id
    // Declared "virtual" to avoid circular dependencies with seqloc
    CBioseq_Handle GetBioseqHandle(const CSeq_id& id);
    CBioseq_Handle GetBioseqHandle(const CSeq_id_Handle& id);

    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id& id,
                                          const CBioseq_Handle& bh);
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                          const CBioseq_Handle& bh);
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id& id,
                                          const CSeq_entry& tse);

    // Get bioseq handle by seqloc
    CBioseq_Handle GetBioseqHandle(const CSeq_loc& loc);

    // Get bioseq handle by bioseq
    CBioseq_Handle GetBioseqHandle(const CBioseq& seq);


    // Find set of CSeq_id by a string identifier
    // The latter could be name, accession, something else
    // which could be found in CSeq_id
    void FindSeqid(set< CConstRef<CSeq_id> >& setId,
                   const string& searchBy);

    void ResetHistory(void);

    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

    CConstRef<CSynonymsSet> GetSynonyms(const CSeq_id& id);
    CConstRef<CSynonymsSet> GetSynonyms(const CSeq_id_Handle& id);
    CConstRef<CSynonymsSet> GetSynonyms(const CBioseq_Handle& bh);

private:
    // constructor/destructor visible from CScope
    CScope_Impl(CObjectManager& objmgr);
    virtual ~CScope_Impl(void);

    // to prevent copying
    CScope_Impl(const CScope_Impl&);
    CScope_Impl& operator=(const CScope_Impl&);

    void UpdateAnnotIndex(const CSeq_annot& limit_annot);

    CConstRef<TAnnotRefSet>
    GetTSESetWithAnnots(const CSeq_id_Handle& idh_type);
    CConstRef<TAnnotRefSet>
    GetTSESetWithAnnots(const CBioseq_Handle& bh);

    void x_AttachToOM(CObjectManager& objmgr);
    void x_DetachFromOM(void);
    void x_ResetHistory(void);

    // clean some cache entries when new data source is added
    void x_ClearCacheOnNewData(void);
    void x_ClearAnnotCache(void);

    // Find the best possible resolution for the Seq-id
    void x_ResolveSeq_id(TSeq_idMapValue& id);
    // Iterate over priorities, find all possible data sources
    CDataSource_ScopeInfo* x_FindBioseqInfo(const CPriorityTree& tree,
                                            const CSeq_id_Handle& idh,
                                            CSeqMatch_Info& match_info);
    CDataSource_ScopeInfo* x_FindBioseqInfo(const CPriorityNode& node,
                                            const CSeq_id_Handle& idh,
                                            CSeqMatch_Info& match_info);
    CDataSource_ScopeInfo* x_FindBioseqInfo(CDataSource_ScopeInfo& ds_info,
                                            const CSeq_id_Handle& idh,
                                            CSeqMatch_Info& match_info);

    CBioseq_Handle x_GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                            const CTSE_Info& tse);

public:
    // GetTSE_Info corresponding to the TSE
    TTSE_Lock GetTSEInfo(const CSeq_entry* tse);

    // Get seq-entry corresponding to the TSE_Info, load the TSE
    // if necessary.
    const CSeq_entry& x_GetTSEFromInfo(const TTSE_Lock& tse);

    TTSE_Lock GetTSE_Info(const CSeq_entry& tse);
    TTSE_Lock GetTSE_Info_WithSeq_entry(const CSeq_entry& entry);
    TTSE_Lock GetTSE_Info_WithSeq_annot(const CSeq_annot& annot);
    CConstRef<CSeq_entry_Info> GetSeq_entry_Info(const CSeq_entry& entry);
    CConstRef<CSeq_annot_Info> GetSeq_annot_Info(const CSeq_annot& annot);
    // Get Seq-entry info for the seq-entry
    CConstRef<CTSE_Info> x_GetTSE_Info(const CSeq_entry& tse);
    CRef<CTSE_Info> x_GetTSE_Info(CSeq_entry& tse);
    CConstRef<CSeq_entry_Info> x_GetSeq_entry_Info(const CSeq_entry& entry);
    CRef<CSeq_entry_Info> x_GetSeq_entry_Info(CSeq_entry& entry);
    CConstRef<CSeq_annot_Info> x_GetSeq_annot_Info(const CSeq_annot& annot);
    CRef<CSeq_annot_Info> x_GetSeq_annot_Info(CSeq_annot& annot);

private:
    // Get bioseq handles for sequences from the given TSE using the filter
    typedef vector<CBioseq_Handle> TBioseq_HandleSet;
    void x_PopulateBioseq_HandleSet(const CSeq_entry& tse,
                                    TBioseq_HandleSet& handles,
                                    CSeq_inst::EMol filter,
                                    CBioseq_CI_Base::EBioseqLevelFlag level);

    CConstRef<CSynonymsSet> x_GetSynonyms(CRef<CBioseq_ScopeInfo> info);

    // Conflict reporting function
    enum EConflict {
        eConflict_History,
        eConflict_Live
    };
    void x_ThrowConflict(EConflict conflict_type,
                         const CSeqMatch_Info& info1,
                         const CSeqMatch_Info& info2) const;

    TSeq_idMapValue& x_GetSeq_id_Info(const CSeq_id_Handle& id);
    TSeq_idMapValue& x_GetSeq_id_Info(const CBioseq_Handle& bh);
    TSeq_idMapValue* x_FindSeq_id_Info(const CSeq_id_Handle& id);

    CRef<CBioseq_ScopeInfo> x_InitBioseq_Info(TSeq_idMapValue& info);
    CRef<CBioseq_ScopeInfo> x_GetBioseq_Info(const CSeq_id_Handle& id);
    CRef<CBioseq_ScopeInfo> x_FindBioseq_Info(const CSeq_id_Handle& id);


    CScope*         m_HeapScope;

    CObjectManager* m_pObjMgr;
    CPriorityTree   m_setDataSrc; // Data sources ordered by priority

    CInitMutexPool  m_MutexPool;

    typedef CRWLock                  TRWLock;
    typedef TRWLock::TReadLockGuard  TReadLockGuard;
    typedef TRWLock::TWriteLockGuard TWriteLockGuard;

    mutable TRWLock m_Scope_Conf_RWLock;

    TSeq_idMap      m_Seq_idMap;
    mutable TRWLock m_Seq_idMapLock;
    TBioseqMap      m_BioseqMap;
    mutable TRWLock m_BioseqMapLock;

    friend class CScope;
    friend class CHeapScope;
    friend class CObjectManager;
    friend class CSeqVector;
    friend class CDataSource;
    friend class CBioseq_CI;
    friend class CAnnotTypes_CI;
    friend class CBioseq_Handle;
    friend class CTSE_CI;
    friend class CSeq_annot_CI;
    friend class CSeqMap_CI;
};


/////////////////////////////////////////////////////////////////////////////
// CScope
/////////////////////////////////////////////////////////////////////////////

class NCBI_XOBJMGR_EXPORT CScope : public CObject
{
public:
    CScope(CObjectManager& objmgr);
    virtual ~CScope(void);

    // CBioseq_Handle methods:
    // Get bioseq handle by seq-id
    CBioseq_Handle GetBioseqHandle(const CSeq_id& id);
    CBioseq_Handle GetBioseqHandle(const CSeq_id_Handle& id);
    CBioseq_Handle GetBioseqHandle(const CSeq_loc& loc);

    // Get bioseq handle by bioseq
    CBioseq_Handle GetBioseqHandle(const CBioseq& seq);

    // Get bioseq handle for sequence withing one TSE
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id& id,
                                          const CBioseq_Handle& bh);
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                          const CBioseq_Handle& bh);


    // CScope contents modification methods
    typedef CPriorityNode::TPriority TPriority;
    enum EPriority {
        kPriority_NotSet = -1
    };

    // Add default data loaders from object manager
    void AddDefaults(TPriority priority = kPriority_NotSet);
    // Add data loader by name.
    // The loader (or its factory) must be known to Object Manager.
    void AddDataLoader(const string& loader_name,
                       TPriority priority = kPriority_NotSet);
    // Add seq_entry, default priority is higher than for defaults or loaders
    void AddTopLevelSeqEntry(CSeq_entry& top_entry,
                             TPriority priority = kPriority_NotSet);
    // Add the scope's datasources as a single group with the given priority
    void AddScope(CScope& scope,
                  TPriority priority = kPriority_NotSet);

    // Add bioseq, return bioseq handle. Try to use unresolved seq-id
    // from the bioseq, fail if all ids are already resolved to
    // other sequences.
    CBioseq_Handle AddBioseq(CBioseq& bioseq,
                             TPriority priority = kPriority_NotSet);

    // Add annotations to a seq-entry (seq or set)
    void AttachAnnot(CSeq_entry& entry, CSeq_annot& annot);
    void RemoveAnnot(CSeq_entry& entry, CSeq_annot& annot);
    void ReplaceAnnot(CSeq_entry& entry,
                      CSeq_annot& old_annot, CSeq_annot& new_annot);
    // Add new sub-entry to the existing tree if it is in this scope
    void AttachEntry(CSeq_entry& parent, CSeq_entry& entry);
    // Add sequence map for a bioseq if it is in this scope
    void AttachMap(CSeq_entry& bioseq, CSeqMap& seqmap);

    void ResetHistory(void);

    CConstRef<CSynonymsSet> GetSynonyms(const CSeq_id& id);
    CConstRef<CSynonymsSet> GetSynonyms(const CSeq_id_Handle& id);
    CConstRef<CSynonymsSet> GetSynonyms(const CBioseq_Handle& bh);

    CScope_Impl& GetImpl(void);

private:
    // to prevent copying
    CScope(const CScope&);
    CScope& operator=(const CScope&);

    friend class CSeqMap_CI;
    friend class CSeq_annot_CI;
    friend class CAnnotTypes_CI;
    friend class CBioseq_CI;
    friend class CHeapScope;

    CRef<CScope_Impl> m_Impl;
    CRef<CScope>      m_HeapScope;
};


/////////////////////////////////////////////////////////////////////////////
// CHeapScope
//    Holds reference on heap scope object
//    used internally in interface classes (iterators, handles etc)
/////////////////////////////////////////////////////////////////////////////

class CHeapScope
{
public:
    CHeapScope(void)
        {
        }
    CHeapScope(CScope& scope)
        : m_Scope(scope.m_Impl->m_HeapScope)
        {
            _ASSERT(m_Scope);
        }
    CHeapScope(CScope* scope)
        : m_Scope(scope->m_Impl->m_HeapScope)
        {
            _ASSERT(m_Scope);
        }

    
    operator bool(void) const
        {
            return m_Scope;
        }
    bool operator!(void) const
        {
            return !m_Scope;
        }

    operator CScope&(void) const
        {
            return *m_Scope;
        }
    operator CScope*(void) const
        {
            return m_Scope.GetPointer();
        }
    operator CScope_Impl*(void) const
        {
            return m_Scope->m_Impl.GetPointer();
        }

    CScope& operator*(void) const
        {
            return *m_Scope;
        }
    CScope_Impl* operator->(void) const
        {
            return m_Scope->m_Impl.GetPointer();
        }

private:
    mutable CRef<CScope> m_Scope;
};


/////////////////////////////////////////////////////////////////////////////
// CScope_Impl inline methods
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CScope inline methods
/////////////////////////////////////////////////////////////////////////////


inline
CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id& id)
{
    return m_Impl->GetBioseqHandle(id);
}


inline
CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id_Handle& id)
{
    return m_Impl->GetBioseqHandle(id);
}


inline
CBioseq_Handle CScope::GetBioseqHandle(const CSeq_loc& loc)
{
    return m_Impl->GetBioseqHandle(loc);
}


inline
CBioseq_Handle CScope::GetBioseqHandle(const CBioseq& seq)
{
    return m_Impl->GetBioseqHandle(seq);
}


inline
CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id& id,
                                              const CBioseq_Handle& bh)
{
    return m_Impl->GetBioseqHandleFromTSE(id, bh);
}


inline
CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                              const CBioseq_Handle& bh)
{
    return m_Impl->GetBioseqHandleFromTSE(id, bh);
}


inline
void CScope::AddDefaults(TPriority priority)
{
    m_Impl->AddDefaults(priority);
}


inline
void CScope::AddDataLoader(const string& loader_name, TPriority priority)
{
    m_Impl->AddDataLoader(loader_name, priority);
}


inline
void CScope::AddTopLevelSeqEntry(CSeq_entry& top_entry, TPriority priority)
{
    m_Impl->AddTopLevelSeqEntry(top_entry, priority);
}


inline
void CScope::AddScope(CScope& scope, TPriority priority)
{
    m_Impl->AddScope(*scope.m_Impl, priority);
}


inline
CBioseq_Handle CScope::AddBioseq(CBioseq& bioseq, TPriority priority)
{
    return m_Impl->AddBioseq(bioseq, priority);
}


inline
void CScope::AttachAnnot(CSeq_entry& entry, CSeq_annot& annot)
{
    m_Impl->AttachAnnot(entry, annot);
}


inline
void CScope::RemoveAnnot(CSeq_entry& entry, CSeq_annot& annot)
{
    m_Impl->RemoveAnnot(entry, annot);
}


inline
void CScope::ReplaceAnnot(CSeq_entry& entry,
                          CSeq_annot& old_annot, CSeq_annot& new_annot)
{
    m_Impl->ReplaceAnnot(entry, old_annot, new_annot);
}


inline
void CScope::AttachEntry(CSeq_entry& parent, CSeq_entry& entry)
{
    m_Impl->AttachEntry(parent, entry);
}


inline
void CScope::AttachMap(CSeq_entry& bioseq, CSeqMap& seqmap)
{
    m_Impl->AttachMap(bioseq, seqmap);
}


inline
void CScope::ResetHistory(void)
{
    m_Impl->ResetHistory();
}


inline
CScope_Impl& CScope::GetImpl(void)
{
    return *m_Impl;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.64  2003/11/21 20:33:03  grichenk
* Added GetBioseqHandleFromTSE(CSeq_id, CSeq_entry)
*
* Revision 1.63  2003/11/12 15:49:48  vasilche
* Added loading annotations on non gi Seq-id.
*
* Revision 1.62  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.61  2003/09/30 16:21:59  vasilche
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
* Revision 1.60  2003/09/05 20:50:24  grichenk
* +AddBioseq(CBioseq&)
*
* Revision 1.59  2003/09/03 19:59:59  grichenk
* Added sequence filtering by level (mains/parts/all)
*
* Revision 1.58  2003/08/12 18:25:39  grichenk
* Fixed default priorities
*
* Revision 1.57  2003/08/06 20:51:14  grichenk
* Removed declarations of non-existent methods
*
* Revision 1.56  2003/08/04 17:02:57  grichenk
* Added constructors to iterate all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.55  2003/07/25 15:25:22  grichenk
* Added CSeq_annot_CI class
*
* Revision 1.54  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.53  2003/06/30 18:42:09  vasilche
* CPriority_I made to use less memory allocations/deallocations.
*
* Revision 1.52  2003/06/19 18:34:07  vasilche
* Fixed compilation on Windows.
*
* Revision 1.51  2003/06/19 18:23:44  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.49  2003/05/27 19:44:04  grichenk
* Added CSeqVector_CI class
*
* Revision 1.48  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.47  2003/05/14 18:39:25  grichenk
* Simplified TSE caching and filtering in CScope, removed
* some obsolete members and functions.
*
* Revision 1.46  2003/05/12 19:18:28  vasilche
* Fixed locking of object manager classes in multi-threaded application.
*
* Revision 1.45  2003/05/06 18:54:06  grichenk
* Moved TSE filtering from CDataSource to CScope, changed
* some filtering rules (e.g. priority is now more important
* than scope history). Added more caches to CScope.
*
* Revision 1.44  2003/04/29 19:51:12  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.43  2003/04/24 17:24:57  vasilche
* SAnnotSelector is struct.
* Added include required by MS VC.
*
* Revision 1.42  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.41  2003/04/09 16:04:29  grichenk
* SDataSourceRec replaced with CPriorityNode
* Added CScope::AddScope(scope, priority) to allow scope nesting
*
* Revision 1.40  2003/04/03 14:18:08  vasilche
* Added public GetSynonyms() method.
*
* Revision 1.39  2003/03/26 21:00:07  grichenk
* Added seq-id -> tse with annotation cache to CScope
*
* Revision 1.38  2003/03/24 21:26:42  grichenk
* Added support for CTSE_CI
*
* Revision 1.37  2003/03/21 19:22:48  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.36  2003/03/18 14:46:36  grichenk
* Set limit object type to "none" if the object is null.
*
* Revision 1.35  2003/03/12 20:09:30  grichenk
* Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
*
* Revision 1.34  2003/03/11 14:15:49  grichenk
* +Data-source priority
*
* Revision 1.33  2003/03/10 16:55:16  vasilche
* Cleaned SAnnotSelector structure.
* Added shortcut when features are limited to one TSE.
*
* Revision 1.32  2003/03/03 20:32:47  vasilche
* Removed obsolete method GetSynonyms().
*
* Revision 1.31  2003/02/28 20:02:35  grichenk
* Added synonyms cache and x_GetSynonyms()
*
* Revision 1.30  2003/02/27 14:35:32  vasilche
* Splitted PopulateTSESet() by logically independent parts.
*
* Revision 1.29  2003/02/24 18:57:21  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.28  2003/01/22 20:11:53  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.27  2002/12/26 20:51:36  dicuccio
* Added Win32 export specifier
*
* Revision 1.26  2002/12/26 16:39:21  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.25  2002/11/08 22:15:50  grichenk
* Added methods for removing/replacing annotations
*
* Revision 1.24  2002/11/01 05:34:06  kans
* added GetBioseqHandle taking CBioseq parameter
*
* Revision 1.23  2002/10/31 22:25:09  kans
* added GetBioseqHandle taking CSeq_loc parameter
*
* Revision 1.22  2002/10/18 19:12:39  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.21  2002/10/02 17:58:21  grichenk
* Added sequence type filter to CBioseq_CI
*
* Revision 1.20  2002/09/30 20:01:17  grichenk
* Added methods to support CBioseq_CI
*
* Revision 1.19  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.18  2002/06/04 17:18:32  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.17  2002/05/28 18:01:11  gouriano
* DebugDump added
*
* Revision 1.16  2002/05/14 20:06:23  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.15  2002/05/06 03:30:36  vakatov
* OM/OM1 renaming
*
* Revision 1.14  2002/05/03 21:28:02  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.13  2002/04/17 21:09:38  grichenk
* Fixed annotations loading
*
* Revision 1.12  2002/04/08 19:14:15  gouriano
* corrected comment to AddDefaults()
*
* Revision 1.11  2002/03/27 18:46:26  gouriano
* three functions made public
*
* Revision 1.10  2002/03/20 21:20:38  grichenk
* +CScope::ResetHistory()
*
* Revision 1.9  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.8  2002/02/07 21:27:33  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.7  2002/02/06 21:46:43  gouriano
* *** empty log message ***
*
* Revision 1.6  2002/02/05 21:47:21  gouriano
* added FindSeqid function, minor tuneup in CSeq_id_mapper
*
* Revision 1.5  2002/01/28 19:45:33  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.4  2002/01/23 21:59:29  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.3  2002/01/18 17:07:12  gouriano
* renamed GetSequence to GetSeqVector
*
* Revision 1.2  2002/01/16 16:26:35  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:03  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // SCOPE__HPP
