#ifndef OBJMGR_SCOPE__HPP
#define OBJMGR_SCOPE__HPP

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
#include <objects/seq/seq_id_handle.hpp>
#include <objmgr/tse_handle.hpp>
#include <objmgr/scope_transaction.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/gc_assembly_parser.hpp>
#include <objects/genomecoll/GC_Assembly.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/** @addtogroup ObjectManagerCore
 *
 * @{
 */


// fwd decl
// objects
class CSeq_entry;
class CBioseq_set;
class CBioseq;
class CSeq_annot;
class CSeq_id;
class CSeq_loc;

// objmgr
class CSeq_id_Handle;
class CObjectManager;
class CScope_Impl;
class CSynonymsSet;


/////////////////////////////////////////////////////////////////////////////
///
///  CScope --
///
///  Scope of cache visibility and reference resolution
///

class NCBI_XOBJMGR_EXPORT CScope : public CObject
{
public:
    explicit CScope(CObjectManager& objmgr);
    virtual ~CScope(void);

    /// priority type and special value for added objects
    typedef int TPriority;
    enum EPriority {
        kPriority_Default = -1, ///< Use default priority for added data
        kPriority_NotSet = -1   ///< Deprecated: use kPriority_Default instead
    };

    /// Get object manager controlling this scope
    CObjectManager& GetObjectManager(void);

    // CBioseq_Handle methods:
    /// Get bioseq handle by seq-id
    CBioseq_Handle GetBioseqHandle(const CSeq_id& id);

    /// Get bioseq handle by seq-id handle
    CBioseq_Handle GetBioseqHandle(const CSeq_id_Handle& id);

    /// Get bioseq handle by seq-loc.
    /// If the seq-loc contains no seq-ids or there's a single seq-id which
    /// can not be resolved, returns empty handle.
    /// If the seq-loc references only parts of a segmented set, the method
    /// returns bioseq handle for the master bioseq.
    /// If the seq-loc contains a single seq-id, the bioseq handle for this
    /// id is returned.
    /// If there are multiple seq-ids not belonging to the same seg-set,
    /// the method throws CObjMgrException.
    CBioseq_Handle GetBioseqHandle(const CSeq_loc& loc);

    enum EGetBioseqFlag {
        eGetBioseq_Resolved, ///< Search only in already resolved ids
        eGetBioseq_Loaded,   ///< Search in all loaded TSEs in the scope
        eGetBioseq_All       ///< Search bioseq, load if not loaded yet
    };

    /// Get bioseq handle without loading new data
    CBioseq_Handle GetBioseqHandle(const CSeq_id& id,
                                   EGetBioseqFlag get_flag);
    /// Get bioseq handle without loading new data
    CBioseq_Handle GetBioseqHandle(const CSeq_id_Handle& id,
                                   EGetBioseqFlag get_flag);

    /// Check if two seq-ids are resolved to the same Bioseq
    bool IsSameBioseq(const CSeq_id_Handle& id1,
                      const CSeq_id_Handle& id2,
                      EGetBioseqFlag get_flag);

    typedef CBioseq_Handle::TId TIds;
    typedef vector<CBioseq_Handle> TBioseqHandles;
    /// Get bioseq handles for all ids. The returned vector contains
    /// bioseq handles for all requested ids in the same order.
    TBioseqHandles GetBioseqHandles(const TIds& ids);

    /// GetXxxHandle control values.
    enum EMissing {
        eMissing_Throw,
        eMissing_Null,
        eMissing_Default = eMissing_Throw
    };

    // Deprecated interface
    /// Find object in scope
    /// If object is not found GetXxxHandle() methods will either
    /// throw an exception or return null handle depending on argument.
    CTSE_Handle GetTSE_Handle(const CSeq_entry& tse,
                              EMissing action = eMissing_Default);
    CBioseq_Handle GetBioseqHandle(const CBioseq& bioseq,
                                   EMissing action = eMissing_Default);
    CBioseq_set_Handle GetBioseq_setHandle(const CBioseq_set& seqset,
                                           EMissing action = eMissing_Default);
    CSeq_entry_Handle GetSeq_entryHandle(const CSeq_entry& entry,
                                         EMissing action = eMissing_Default);
    CSeq_annot_Handle GetSeq_annotHandle(const CSeq_annot& annot,
                                         EMissing action = eMissing_Default);
    CSeq_feat_Handle GetSeq_featHandle(const CSeq_feat& feat,
                                       EMissing action = eMissing_Default);

    CBioseq_Handle GetObjectHandle(const CBioseq& bioseq,
                                   EMissing action = eMissing_Default);
    CBioseq_set_Handle GetObjectHandle(const CBioseq_set& seqset,
                                       EMissing action = eMissing_Default);
    CSeq_entry_Handle GetObjectHandle(const CSeq_entry& entry,
                                      EMissing action = eMissing_Default);
    CSeq_annot_Handle GetObjectHandle(const CSeq_annot& annot,
                                      EMissing action = eMissing_Default);
    CSeq_feat_Handle GetObjectHandle(const CSeq_feat& feat,
                                     EMissing action = eMissing_Default);

    /// Get edit handle for the specified object
    /// Throw an exception if object is not found, or non-editable
    CBioseq_EditHandle GetBioseqEditHandle(const CBioseq& bioseq);
    CSeq_entry_EditHandle GetSeq_entryEditHandle(const CSeq_entry& entry);
    CSeq_annot_EditHandle GetSeq_annotEditHandle(const CSeq_annot& annot);
    CBioseq_set_EditHandle GetBioseq_setEditHandle(const CBioseq_set& seqset);

    CBioseq_EditHandle GetObjectEditHandle(const CBioseq& bioseq);
    CBioseq_set_EditHandle GetObjectEditHandle(const CBioseq_set& seqset);
    CSeq_entry_EditHandle GetObjectEditHandle(const CSeq_entry& entry);
    CSeq_annot_EditHandle GetObjectEditHandle(const CSeq_annot& annot);

    /// Get bioseq handle for sequence withing one TSE
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id& id,
                                          const CTSE_Handle& tse);
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                          const CTSE_Handle& tse);

    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id& id,
                                          const CBioseq_Handle& bh);

    /// Get bioseq handle for sequence withing one TSE
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id& id,
                                          const CSeq_entry_Handle& seh);

    /// Get bioseq handle for sequence withing one TSE
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                          const CBioseq_Handle& bh);

    /// Get bioseq handle for sequence withing one TSE
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                          const CSeq_entry_Handle& seh);


    // CScope contents modification methods

    /// Add default data loaders from object manager
    void AddDefaults(TPriority pri = kPriority_Default);

    /// Add data loader by name.
    /// The loader (or its factory) must be known to Object Manager.
    void AddDataLoader(const string& loader_name,
                       TPriority pri = kPriority_Default);

    /// Add the scope's datasources as a single group with the given priority
    /// All data sources (data loaders and explicitly added data) have
    /// priorities. The scope scans data sources in order of increasing
    /// priorities to find the sequence you've requested. By default,
    /// explicitly added data have priority 9, and data loaders - priority
    /// 99, so the scope will first look in explicit data, then in data
    /// loaders. If you have conflicting data or loaders (e.g. GenBank and
    /// BLAST), you may need different priorities to make scope first look,
    /// for example, in BLAST, and then if sequence is not found - in GenBank.
    /// Note, that the priority you've specified for a data loader at
    /// registration time (RegisterInObjectManager()) is a new default for
    /// it, and can be overridden when you add the data loader to a scope.
    void AddScope(CScope& scope,
                  TPriority pri = kPriority_Default);


    /// AddXxx() control values
    enum EExist {
        eExist_Throw,
        eExist_Get,
        eExist_Default = eExist_Throw
    };
    /// Add seq_entry, default priority is higher than for defaults or loaders
    /// Add object to the score with possibility to edit it directly.
    /// If the object is already in the scope the AddXxx() methods will
    /// throw an exception or return handle to existent object depending
    /// on the action argument.
    CSeq_entry_Handle AddTopLevelSeqEntry(CSeq_entry& top_entry,
                                          TPriority pri = kPriority_Default,
                                          EExist action = eExist_Default);
    /// Add shared Seq-entry, scope will not modify it.
    /// If edit handle is requested, scope will create a copy object.
    /// If the object is already in the scope the AddXxx() methods will
    /// throw an exception or return handle to existent object depending
    /// on the action argument.
    CSeq_entry_Handle AddTopLevelSeqEntry(const CSeq_entry& top_entry,
                                          TPriority pri = kPriority_Default,
                                          EExist action = eExist_Throw);


    /// Add bioseq, return bioseq handle. Try to use unresolved seq-id
    /// from the bioseq, fail if all ids are already resolved to
    /// other sequences.
    /// Add object to the score with possibility to edit it directly.
    /// If the object is already in the scope the AddXxx() methods will
    /// throw an exception or return handle to existent object depending
    /// on the action argument.
    CBioseq_Handle AddBioseq(CBioseq& bioseq,
                             TPriority pri = kPriority_Default,
                             EExist action = eExist_Throw);

    /// Add shared Bioseq, scope will not modify it.
    /// If edit handle is requested, scope will create a copy object.
    /// If the object is already in the scope the AddXxx() methods will
    /// throw an exception or return handle to existent object depending
    /// on the action argument.
    CBioseq_Handle AddBioseq(const CBioseq& bioseq,
                             TPriority pri = kPriority_Default,
                             EExist action = eExist_Throw);

    /// Add GC-Assembly. Sequences from the assembly are converted
    /// to bioseqs and added to the scope as a new top level seq-entry.
    CSeq_entry_Handle AddGC_Assembly(const CGC_Assembly& gc_assembly,
        CGC_Assembly_Parser::FParserFlags flags = CGC_Assembly_Parser::fDefault,
        TPriority pri = kPriority_Default,
        EExist action = eExist_Throw);

    /// Add Seq-annot, return its CSeq_annot_Handle.
    /// Add object to the score with possibility to edit it directly.
    /// If the object is already in the scope the AddXxx() methods will
    /// throw an exception or return handle to existent object depending
    /// on the action argument.
    CSeq_annot_Handle AddSeq_annot(CSeq_annot& annot,
                                   TPriority pri = kPriority_Default,
                                   EExist action = eExist_Throw);
    /// Add shared Seq-annot, scope will not modify it.
    /// If edit handle is requested, scope will create a copy object.
    /// If the object is already in the scope the AddXxx() methods will
    /// throw an exception or return handle to existent object depending
    /// on the action argument.
    CSeq_annot_Handle AddSeq_annot(const CSeq_annot& annot,
                                   TPriority pri = kPriority_Default,
                                   EExist action = eExist_Throw);

    /// Get editable Biosec handle by regular one
    CBioseq_EditHandle     GetEditHandle(const CBioseq_Handle&     seq);

    /// Get editable SeqEntry handle by regular one
    CSeq_entry_EditHandle  GetEditHandle(const CSeq_entry_Handle&  entry);

    /// Get editable Seq-annot handle by regular one
    CSeq_annot_EditHandle  GetEditHandle(const CSeq_annot_Handle&  annot);

    /// Get editable Biosec-set handle by regular one
    CBioseq_set_EditHandle GetEditHandle(const CBioseq_set_Handle& seqset);

    enum EActionIfLocked {
        eKeepIfLocked,
        eThrowIfLocked,
        eRemoveIfLocked
    };
    /// Clean all unused TSEs from the scope's cache and release the memory.
    /// TSEs referenced by any handles are not removed.
    /// Explicitly added entries are not removed.
    /// Edited entries retrieved from a data loader are removed,
    /// so that their unmodified version will be visible.
    void ResetHistory(EActionIfLocked action = eKeepIfLocked);
    /// Clear all information in the scope except added data loaders.
    void ResetDataAndHistory(void);
    /// Clear all information in the scope including data loaders.
    enum ERemoveDataLoaders {
        eRemoveDataLoaders
    };
    void ResetDataAndHistory(ERemoveDataLoaders remove_data_loaders);

    /// Remove single TSE from the scope's history. If there are other
    /// live handles referencing the TSE, nothing is removed.
    /// @param tse
    ///  TSE to be removed from the cache.
    void RemoveFromHistory(const CTSE_Handle& tse);
    /// Remove the bioseq's TSE from the scope's history. If there are other
    /// live handles referencing the TSE, nothing is removed.
    /// @param bioseq
    ///  Bioseq, which TSE is to be removed from the cache.
    void RemoveFromHistory(const CBioseq_Handle& bioseq);

    /// Revoke data loader from the scope. Throw exception if the
    /// operation fails (e.g. data source is in use or not found).
    void RemoveDataLoader(const string& loader_name,
                          EActionIfLocked action = eThrowIfLocked);
    /// Revoke TSE previously added using AddTopLevelSeqEntry() or
    /// AddBioseq(). Throw exception if the TSE is still in use or
    /// not found in the scope.
    void RemoveTopLevelSeqEntry(const CTSE_Handle& entry);
    void RemoveTopLevelSeqEntry(const CSeq_entry_Handle& entry);

    /// Revoke Bioseq previously added using AddBioseq().
    /// Throw exception if the Bioseq is still in use or
    /// not found in the scope.
    void RemoveBioseq(const CBioseq_Handle& seq);

    /// Revoke Seq-annot previously added using AddSeq_annot().
    /// Throw exception if the Bioseq is still in use or
    /// not found in the scope.
    void RemoveSeq_annot(const CSeq_annot_Handle& annot);

    /// Get "native" bioseq ids without filtering and matching.
    TIds GetIds(const CSeq_id&        id );
    TIds GetIds(const CSeq_id_Handle& idh);

    CSeq_id_Handle GetAccVer(const CSeq_id_Handle& idh);
    TGi GetGi(const CSeq_id_Handle& idh);
    
    static CSeq_id_Handle x_GetAccVer(const TIds& ids);
    static TGi x_GetGi(const TIds& ids);

    /// Get short description of bioseq, usually "accession.version"
    enum EForceLabelLoad {
        eNoForceLabelLoad,
        eForceLabelLoad
    };
    string GetLabel(const CSeq_id& id,
                    EForceLabelLoad force_load = eNoForceLabelLoad);
    string GetLabel(const CSeq_id_Handle& idh,
                    EForceLabelLoad force_load = eNoForceLabelLoad);

    /// Get taxonomy id of bioseq
    /// -1 means failure to determine the taxonomy id
    /// 0 means absence of the taxonomy id for the sequence
    enum EForceLoad {
        eNoForceLoad,
        eForceLoad
    };
    int GetTaxId(const CSeq_id& id,
                 EForceLoad force_load = eNoForceLoad);
    int GetTaxId(const CSeq_id_Handle& idh,
                 EForceLoad force_load = eNoForceLoad);

    // returns kInvalidSeqPos if sequence is not known
    TSeqPos GetSequenceLength(const CSeq_id& id,
                              EForceLoad force_load = eNoForceLoad);
    TSeqPos GetSequenceLength(const CSeq_id_Handle& id,
                              EForceLoad force_load = eNoForceLoad);

    // returns CSeq_inst::eMol_not_set if sequence is not known
    CSeq_inst::TMol GetSequenceType(const CSeq_id& id,
                                    EForceLoad force_load = eNoForceLoad);
    CSeq_inst::TMol GetSequenceType(const CSeq_id_Handle& id,
                                    EForceLoad force_load = eNoForceLoad);

    /// Bulk retrieval methods
    typedef vector<CSeq_id_Handle> TSeq_id_Handles;
    TSeq_id_Handles GetAccVers(const TSeq_id_Handles& idhs,
                               EForceLoad force_load = eNoForceLoad);
    void GetAccVers(TSeq_id_Handles* results,
                    const TSeq_id_Handles& idhs,
                    EForceLoad force_load = eNoForceLoad);
    typedef vector<TGi> TGIs;
    TGIs GetGis(const TSeq_id_Handles& idhs,
                EForceLoad force_load = eNoForceLoad);
    void GetGis(TGIs* results,
                const TSeq_id_Handles& idhs,
                EForceLoad force_load = eNoForceLoad);
    typedef vector<string> TLabels;
    TLabels GetLabels(const TSeq_id_Handles& idhs,
                      EForceLoad force_load = eNoForceLoad);
    void GetLabels(TLabels* results,
                   const TSeq_id_Handles& idhs,
                   EForceLoad force_load = eNoForceLoad);
    typedef vector<int> TTaxIds;
    TTaxIds GetTaxIds(const TSeq_id_Handles& idhs,
                      EForceLoad force_load = eNoForceLoad);
    void GetTaxIds(TTaxIds* results,
                   const TSeq_id_Handles& idhs,
                   EForceLoad force_load = eNoForceLoad);

    typedef vector<TSeqPos> TSequenceLengths;
    TSequenceLengths GetSequenceLengths(const TSeq_id_Handles& idhs,
                                        EForceLoad force_load = eNoForceLoad);
    void GetSequenceLengths(TSequenceLengths* results,
                            const TSeq_id_Handles& idhs,
                            EForceLoad force_load = eNoForceLoad);
    typedef vector<CSeq_inst::TMol> TSequenceTypes;
    TSequenceTypes GetSequenceTypes(const TSeq_id_Handles& idhs,
                                    EForceLoad force_load = eNoForceLoad);
    void GetSequenceTypes(TSequenceTypes* results,
                          const TSeq_id_Handles& idhs,
                          EForceLoad force_load = eNoForceLoad);

    /// Get bioseq synonyms, resolving to the bioseq in this scope.
    CConstRef<CSynonymsSet> GetSynonyms(const CSeq_id&        id);

    /// Get bioseq synonyms, resolving to the bioseq in this scope.
    CConstRef<CSynonymsSet> GetSynonyms(const CSeq_id_Handle& id);

    /// Get bioseq synonyms, resolving to the bioseq in this scope.
    CConstRef<CSynonymsSet> GetSynonyms(const CBioseq_Handle& bh);

    // deprecated interface
    void AttachEntry(CSeq_entry& parent, CSeq_entry& entry);
    void RemoveEntry(CSeq_entry& entry);

    void AttachAnnot(CSeq_entry& parent, CSeq_annot& annot);
    void RemoveAnnot(CSeq_entry& parent, CSeq_annot& annot);
    void ReplaceAnnot(CSeq_entry& entry,
                      CSeq_annot& old_annot, CSeq_annot& new_annot);

    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id& id,
                                          const CSeq_entry& tse);
    CBioseq_Handle GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                          const CSeq_entry& tse);

    enum ETSEKind {
        eManualTSEs,
        eAllTSEs
    };
    typedef vector<CSeq_entry_Handle> TTSE_Handles;
    void GetAllTSEs(TTSE_Handles& tses, enum ETSEKind kind = eManualTSEs);

    CScopeTransaction GetTransaction();

    void UpdateAnnotIndex(void);

protected:
    CScope_Impl& GetImpl(void);

private:
    // to prevent copying
    CScope(const CScope&);
    CScope& operator=(const CScope&);

    friend class CSeqMap_CI;
    friend class CSeq_annot_CI;
    friend class CAnnot_Collector;
    friend class CBioseq_CI;
    friend class CHeapScope;
    friend class CPrefetchTokenOld_Impl;
    friend class CScopeTransaction;

    CRef<CScope>      m_HeapScope;
    CRef<CScope_Impl> m_Impl;
};


/////////////////////////////////////////////////////////////////////////////
// CScope inline methods
/////////////////////////////////////////////////////////////////////////////


inline
CScope_Impl& CScope::GetImpl(void)
{
    return *m_Impl;
}


inline
CBioseq_Handle CScope::GetObjectHandle(const CBioseq& obj,
                                       EMissing action)
{
    return GetBioseqHandle(obj, action);
}


inline
CBioseq_set_Handle CScope::GetObjectHandle(const CBioseq_set& obj,
                                           EMissing action)
{
    return GetBioseq_setHandle(obj, action);
}


inline
CSeq_entry_Handle CScope::GetObjectHandle(const CSeq_entry& obj,
                                          EMissing action)
{
    return GetSeq_entryHandle(obj, action);
}


inline
CSeq_annot_Handle CScope::GetObjectHandle(const CSeq_annot& obj,
                                          EMissing action)
{
    return GetSeq_annotHandle(obj, action);
}


inline
CSeq_feat_Handle CScope::GetObjectHandle(const CSeq_feat& feat,
                                         EMissing action)
{
    return GetSeq_featHandle(feat, action);
}


inline
CBioseq_EditHandle CScope::GetObjectEditHandle(const CBioseq& obj)
{
    return GetBioseqEditHandle(obj);
}


inline
CBioseq_set_EditHandle CScope::GetObjectEditHandle(const CBioseq_set& obj)
{
    return GetBioseq_setEditHandle(obj);
}


inline
CSeq_entry_EditHandle CScope::GetObjectEditHandle(const CSeq_entry& obj)
{
    return GetSeq_entryEditHandle(obj);
}


inline
CSeq_annot_EditHandle CScope::GetObjectEditHandle(const CSeq_annot& obj)
{
    return GetSeq_annotEditHandle(obj);
}


inline
void CScope::RemoveTopLevelSeqEntry(const CSeq_entry_Handle& entry)
{
    RemoveTopLevelSeqEntry(entry.GetTSE_Handle());
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJMGR_SCOPE__HPP
