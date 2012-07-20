#ifndef OBJECTS_OBJMGR_IMPL___TSE_INFO__HPP
#define OBJECTS_OBJMGR_IMPL___TSE_INFO__HPP

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
*   TSE info -- entry for data source seq-id to TSE map
*
*/


#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/tse_lock.hpp>

#include <objmgr/edit_saver.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/blob_id.hpp>
#include <objmgr/bio_object_id.hpp>
#include <objects/seq/seq_id_handle.hpp>

#include <util/rangemap.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <objmgr/impl/annot_object_index.hpp>

#include <map>
#include <set>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope_Impl;
class CBioseq_Info;
class CSeq_entry_Info;
class CSeq_annot_Info;
class CSeq_annot_SNP_Info;
class CTSE_Chunk_Info;
class CTSE_Split_Info;
class CTSE_Info_Object;
class CTSE_Lock;
class CTSE_LoadLock;

class CDataSource;
class CHandleRange;
class CAnnotTypes_CI;
class CSeq_entry;

class CSeq_literal;
class CObject_id;

class ITSE_Assigner;
class CTSE_Default_Assigner;
class IEditSaver;

class CSeq_annot_Finder;
class CMasterSeqSegments;

////////////////////////////////////////////////////////////////////
//
//  CTSE_Info::
//
//    General information and indexes for top level seq-entries
//


struct NCBI_XOBJMGR_EXPORT SIdAnnotObjs
{
    SIdAnnotObjs(void);
    ~SIdAnnotObjs(void);
    SIdAnnotObjs(const SIdAnnotObjs& objs);
    
    typedef CRange<TSeqPos>                                  TRange;
    typedef CRangeMultimap<SAnnotObject_Index, TSeqPos>      TRangeMap;
    typedef vector<TRangeMap*>                               TAnnotSet;
    typedef vector<CConstRef<CSeq_annot_SNP_Info> >          TSNPSet;

    size_t x_GetRangeMapCount(void) const
        {
            return m_AnnotSet.size();
        }
    bool x_RangeMapIsEmpty(size_t index) const
        {
            _ASSERT(index < x_GetRangeMapCount());
            TRangeMap* slot = m_AnnotSet[index];
            return !slot || slot->empty();
        }
    const TRangeMap& x_GetRangeMap(size_t index) const
        {
            _ASSERT(!x_RangeMapIsEmpty(index));
            return *m_AnnotSet[index];
        }

    TRangeMap& x_GetRangeMap(size_t index);
    bool x_CleanRangeMaps(void);

    TAnnotSet m_AnnotSet;
    TSNPSet   m_SNPSet;

private:
    const SIdAnnotObjs& operator=(const SIdAnnotObjs& objs);
};


struct SSeqMatch_TSE
{
    CSeq_id_Handle          m_Seq_id;
    CConstRef<CBioseq_Info> m_Bioseq;

    DECLARE_OPERATOR_BOOL_REF(m_Bioseq);
};


class NCBI_XOBJMGR_EXPORT CTSE_SetObjectInfo : public CObject
{
public:
    CTSE_SetObjectInfo(void);
    ~CTSE_SetObjectInfo(void);

    struct SSeq_annot_Info {
        CRef<CSeq_annot_SNP_Info> m_SNP_annot_Info;
    };
    //    struct SBioseq_Info {
    //        vector<CSeq_id_Handle> m_Removed_ids;
    //    };
    struct SBioseq_set_Info {
        int m_LoadOrder;
    };
    typedef map<CConstRef<CSeq_annot>, SSeq_annot_Info> TSeq_annot_InfoMap;
    typedef map<CConstRef<CBioseq_set>, SBioseq_set_Info> TBioseq_set_InfoMap;
    //    typedef map<CConstRef<CBioseq>, SBioseq_Info> TBioseq_InfoMap;

    TSeq_annot_InfoMap  m_Seq_annot_InfoMap;
    TBioseq_set_InfoMap m_Bioseq_set_InfoMap;
    //    TBioseq_InfoMap m_Bioseq_InfoMap;
    CRef<CSeq_entry> m_TSE;

private:
    CTSE_SetObjectInfo(const CTSE_SetObjectInfo&);
    void operator=(const CTSE_SetObjectInfo&);
};

class CTSEAnnotObjectMapper;

class NCBI_XOBJMGR_EXPORT CTSE_Info : public CSeq_entry_Info
{
    typedef CSeq_entry_Info TParent;
public:
/*
    enum EBlobState {
        fState_none          = 0,
        fState_suppress_temp = 1 << 0,
        fState_suppress_perm = 1 << 1,
        fState_suppress      = fState_suppress_temp | fState_suppress_perm,
        fState_dead          = 1 << 2,
        fState_confidential  = 1 << 3,
        fState_withdrawn     = 1 << 4,
        fState_no_data       = 1 << 5,
        fState_conflict      = 1 << 6,
        fState_conn_failed   = 1 << 7,
        fState_other_error   = 1 << 8
    };
*/
    typedef CBlobIdKey                        TBlobId;
    typedef CBioseq_Handle::TBioseqStateFlags TBlobState;
    typedef int                               TBlobVersion;
    typedef pair<TBlobState, TBlobVersion>    TBlobOrder;

    // 'ctors
    // Argument tse will be parentized.
    explicit CTSE_Info(void);
    explicit CTSE_Info(const TBlobId& blob_id,
                       TBlobVersion blob_version = -1);
    explicit CTSE_Info(CSeq_entry& tse,
                       TBlobState state = CBioseq_Handle::fState_none);
    explicit CTSE_Info(CSeq_entry& tse,
                       TBlobState blob_state,
                       const TBlobId& blob_id,
                       TBlobVersion blob_version = -1);
    explicit CTSE_Info(const CTSE_Lock& tse);
    virtual ~CTSE_Info(void);

    bool HasDataSource(void) const;
    CDataSource& GetDataSource(void) const;

    bool IsLocked(void) const;

    CConstRef<CSeq_entry> GetCompleteTSE(void) const;
    CConstRef<CSeq_entry> GetTSECore(void) const;

    const CTSE_Info& GetTSE_Info(void) const;
    CTSE_Info& GetTSE_Info(void);

    CTSE_Info& Assign(const CTSE_Lock& tse);
    CTSE_Info& Assign(const CTSE_Lock& tse, 
                      CRef<CSeq_entry> entry); 

    // Additional TSE info not available in CSeq_entry
    const TBlobId& GetBlobId(void) const;
    TBlobVersion GetBlobVersion(void) const;
    void SetBlobVersion(TBlobVersion version);

    TBlobState GetBlobState(void) const;
    void ResetBlobState(TBlobState state = CBioseq_Handle::fState_none);
    void SetBlobState(TBlobState state);
    bool IsDead(void) const;
    bool IsUnavailable(void) const;

    // return bits used to determine best TSE, less is better
    TBlobState GetBlobStateOrder(void) const;

    // return full blob order object, less is better
    TBlobOrder GetBlobOrder(void) const;

    const CAnnotName& GetName(void) const;
    void SetName(const CAnnotName& name);

    void SetSeq_entry(CSeq_entry& entry, CTSE_SetObjectInfo* set_info = 0);

    size_t GetUsedMemory(void) const;
    void SetUsedMemory(size_t size);

    // Annot index access
    bool HasAnnot(const CAnnotName& name) const;
    bool HasUnnamedAnnot(void) const;
    bool HasNamedAnnot(const string& name) const;

    // Check types of IDs used in the annotations
    bool HasMatchingAnnotIds(void) const;
    bool OnlyGiAnnotIds(void) const;

    // indexes types
    typedef map<int, CBioseq_set_Info*>                      TBioseq_sets;
    typedef CBioseq_Info*                                    TBioseqInfo;
    typedef map<CSeq_id_Handle, TBioseqInfo>                 TBioseqs;

    typedef SIdAnnotObjs::TRange                             TRange;
    typedef SIdAnnotObjs::TRangeMap                          TRangeMap;
    typedef SIdAnnotObjs::TAnnotSet                          TAnnotSet;
    typedef SIdAnnotObjs::TSNPSet                            TSNPSet;

    typedef map<CSeq_id_Handle, SIdAnnotObjs>                TAnnotObjs;
    typedef map<CAnnotName, TAnnotObjs>                      TNamedAnnotObjs;
    typedef set<CAnnotName>                                  TNames;
    struct SIdAnnotInfo
    {
        TNames  m_Names;
        bool    m_Orphan;
    };
    typedef map<CSeq_id_Handle, SIdAnnotInfo>                TIdAnnotInfoMap;

    struct SFeatIdInfo
    {
        SFeatIdInfo(bool xref, CAnnotObject_Info* info)
            : m_Xref(xref), m_IsChunk(false)
            {
                m_Info = info;
            }
        SFeatIdInfo(bool xref, TChunkId chunk_id)
            : m_Xref(xref), m_IsChunk(true)
            {
                m_ChunkId = chunk_id;
            }

        bool               m_Xref;
        bool               m_IsChunk;
        union {
            CAnnotObject_Info* m_Info;
            TChunkId           m_ChunkId;
        };
    };
    typedef int TFeatIdInt;
    typedef string TFeatIdStr;
    typedef CObject_id TFeatId;
    struct SFeatIdIndex
    {
        typedef multimap<TFeatIdInt, SFeatIdInfo> TIndexInt;
        typedef multimap<TFeatIdStr, SFeatIdInfo> TIndexStr;

        TChunkIds m_Chunks;
        AutoPtr<TIndexInt> m_IndexInt;
        AutoPtr<TIndexStr> m_IndexStr;
    };
    typedef map<CSeqFeatData::ESubtype, SFeatIdIndex>        TFeatIdIndex;
    typedef pair<string, bool> TLocusKey;
    typedef multimap<TLocusKey, CAnnotObject_Info*> TLocusIndex;

    typedef vector<CSeq_id_Handle> TSeqIds;
    typedef TSeqIds TBioseqsIds;

    // find bioseq with exactly the same id
    bool ContainsBioseq(const CSeq_id_Handle& id) const;
    CConstRef<CBioseq_Info> FindBioseq(const CSeq_id_Handle& id) const;

    // find bioseq with matching id
    bool ContainsMatchingBioseq(const CSeq_id_Handle& id) const;
    CConstRef<CBioseq_Info> FindMatchingBioseq(const CSeq_id_Handle& id) const;
    SSeqMatch_TSE GetSeqMatch(const CSeq_id_Handle& id) const;

    CConstRef<CBioseq_Info> GetSegSetMaster(void) const;
    CConstRef<CMasterSeqSegments> GetMasterSeqSegments(void) const;

    // fill ids with all Bioseqs Seq-ids from this TSE
    // the result will be sorted and contain no duplicates
    virtual void GetBioseqsIds(TSeqIds& ids) const;
    // fill ids with all Annot Seq-ids from this TSE
    // the result will be sorted and contain no duplicates
    virtual void GetAnnotIds(TSeqIds& ids) const;
    
    void UpdateAnnotIndex(const CSeq_id_Handle& id) const;
    void UpdateAnnotIndex(void) const;
    void UpdateAnnotIndex(const CTSE_Info_Object& object) const;
    void UpdateAnnotIndex(void);
    void UpdateAnnotIndex(CTSE_Info_Object& object);

    void x_UpdateAnnotIndexContents(CTSE_Info& tse);

    void x_DSAttachContents(CDataSource& ds);
    void x_DSDetachContents(CDataSource& ds);
    
    virtual void x_SetDirtyAnnotIndexNoParent(void);
    virtual void x_ResetDirtyAnnotIndexNoParent(void);

    void x_GetRecords(const CSeq_id_Handle& id, bool bioseq) const;
    void x_LoadChunk(TChunkId chunk_id) const;
    void x_LoadChunks(const TChunkIds& chunk_ids) const;

    CTSE_Split_Info& GetSplitInfo(void);
    bool x_NeedsDelayedMainChunk(void) const;

    const CSeq_id_Handle& GetRequestedId(void) const;
    void SetRequestedId(const CSeq_id_Handle& requested_id) const;

    // annot object map mutex
    typedef CMutex                      TAnnotLock;
    typedef TAnnotLock::TReadLockGuard  TAnnotLockReadGuard;
    typedef TAnnotLock::TWriteLockGuard TAnnotLockWriteGuard;
    TAnnotLock& GetAnnotLock(void) const;

    CRef<IEditSaver> GetEditSaver() const;

    CBioseq_set_Info& x_GetBioseq_set(int id);
    CBioseq_Info& x_GetBioseq(const CSeq_id_Handle& id);

    CTSE_Info_Object* x_FindBioObject(const CBioObjectId& uniq_id) const;

    SFeatIdIndex::TIndexInt& x_GetFeatIdIndexInt(CSeqFeatData::ESubtype type);
    SFeatIdIndex::TIndexStr& x_GetFeatIdIndexStr(CSeqFeatData::ESubtype type);

    bool x_HasFeaturesWithId(CSeqFeatData::ESubtype subtype) const;

    typedef vector<CAnnotObject_Info*> TAnnotObjects;
    void x_AddFeaturesById(TAnnotObjects& objects,
                           const SFeatIdIndex& index,
                           TFeatIdInt id,
                           bool xref) const;
    void x_AddFeaturesById(TAnnotObjects& objects,
                           CSeqFeatData::ESubtype subtype,
                           TFeatIdInt id,
                           bool xref) const;
    void x_AddAllFeaturesById(TAnnotObjects& objects,
                              TFeatIdInt id,
                              bool xref) const;
    TAnnotObjects x_GetFeaturesById(CSeqFeatData::E_Choice type,
                                    TFeatIdInt id,
                                    bool xref) const;
    TAnnotObjects x_GetFeaturesById(CSeqFeatData::ESubtype subtype,
                                    TFeatIdInt id,
                                    bool xref) const;
    void x_AddFeaturesById(TAnnotObjects& objects,
                           const SFeatIdIndex& index,
                           const TFeatIdStr& id,
                           bool xref) const;
    void x_AddFeaturesById(TAnnotObjects& objects,
                           CSeqFeatData::ESubtype subtype,
                           const TFeatIdStr& id,
                           bool xref) const;
    void x_AddAllFeaturesById(TAnnotObjects& objects,
                              const TFeatIdStr& id,
                              bool xref) const;
    TAnnotObjects x_GetFeaturesById(CSeqFeatData::E_Choice type,
                                    const TFeatIdStr& id,
                                    bool xref) const;
    TAnnotObjects x_GetFeaturesById(CSeqFeatData::ESubtype subtype,
                                    const TFeatIdStr& id,
                                    bool xref) const;
    TAnnotObjects x_GetFeaturesById(CSeqFeatData::E_Choice type,
                                    const TFeatId& id,
                                    bool xref) const;
    TAnnotObjects x_GetFeaturesById(CSeqFeatData::ESubtype subtype,
                                    const TFeatId& id,
                                    bool xref) const;
    TAnnotObjects x_GetFeaturesByLocus(const string& locus, bool tag) const;

    typedef pair<CConstRef<CSeq_annot_Info>, CTSE_Lock> TSeq_annot_Lock;
    typedef pair<TSeq_annot_Lock, int> TSeq_feat_Lock;
    TSeq_feat_Lock x_FindSeq_feat(const CSeq_id_Handle& loc_id,
                                  TSeqPos loc_pos,
                                  const CSeq_feat& feat) const;

private:
    friend class CTSE_Guard;
    friend class CDataSource;
    friend class CScope_Impl;
    friend class CDataLoader;
    friend class CAnnot_Collector;
    friend class CSeq_entry_Info;
    friend class CSeq_annot_Info;
    friend class CBioseq_Info;
    friend class CBioseq_set_Info;
    friend class CTSE_Info_Object;
    friend class CTSE_Chunk_Info;
    friend class CTSE_Split_Info;
    friend class CSeq_annot_SNP_Info;

    friend class ITSE_Assigner;
    friend class CTSE_Default_Assigner;
    friend class CSeq_annot_Finder;


    void x_Initialize(void);
   
    void x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds);
    void x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds);

    void x_SetBioseqId(const CSeq_id_Handle& id, CBioseq_Info* info);
    void x_SetBioseqIds(CBioseq_Info* info);
    void x_ResetBioseqId(const CSeq_id_Handle& id, CBioseq_Info* info);

    void x_SetBioseq_setId(int key, CBioseq_set_Info* info);
    void x_ResetBioseq_setId(int key, CBioseq_set_Info* info);

    // index access methods
    TAnnotObjs& x_SetAnnotObjs(const CAnnotName& name);
    const TAnnotObjs* x_GetAnnotObjs(const CAnnotName& name) const;
    const TAnnotObjs* x_GetUnnamedAnnotObjs(void) const;
    void x_RemoveAnnotObjs(const CAnnotName& name);
    const SIdAnnotObjs* x_GetIdObjects(const TAnnotObjs& objs,
                                       const CSeq_id_Handle& idh) const;
    const SIdAnnotObjs* x_GetIdObjects(const CAnnotName& name,
                                       const CSeq_id_Handle& id) const;
    const SIdAnnotObjs* x_GetUnnamedIdObjects(const CSeq_id_Handle& id) const;

    // tse annot index should be locked by TAnnotLockReadGuard
    bool x_HasIdObjects(const CSeq_id_Handle& id) const;

    SIdAnnotObjs& x_SetIdObjects(TAnnotObjs& objs,
                                 const CAnnotName& name,
                                 const CSeq_id_Handle& id);
    SIdAnnotObjs& x_SetIdObjects(const CAnnotName& name,
                                 const CSeq_id_Handle& idh);

    CRef<CSeq_annot_SNP_Info> x_GetSNP_Info(const CConstRef<CSeq_annot>& annot);

    void x_MapSNP_Table(const CAnnotName& name,
                        const CSeq_id_Handle& key,
                        const CSeq_annot_SNP_Info& snp_info);
    void x_UnmapSNP_Table(const CAnnotName& name,
                          const CSeq_id_Handle& key,
                          const CSeq_annot_SNP_Info& snp_info);

    void x_MapAnnotObject(TRangeMap& rangeMap,
                          const SAnnotObject_Key& key,
                          const SAnnotObject_Index& index);
    bool x_UnmapAnnotObject(TRangeMap& rangeMap,
                            const CAnnotObject_Info& info,
                            const SAnnotObject_Key& key);
    void x_MapAnnotObject(SIdAnnotObjs& objs,
                          const SAnnotObject_Key& key,
                          const SAnnotObject_Index& index);
    bool x_UnmapAnnotObject(SIdAnnotObjs& objs,
                            const CAnnotObject_Info& info,
                            const SAnnotObject_Key& key);
    void x_MapAnnotObject(TAnnotObjs& objs,
                          const CAnnotName& name,
                          const SAnnotObject_Key& key,
                          const SAnnotObject_Index& index);
    void x_MapAnnotObject(const CAnnotName& name,
                          const SAnnotObject_Key& key,
                          const SAnnotObject_Index& index);

    bool x_UnmapAnnotObject(TAnnotObjs& objs,
                            const CAnnotName& name,
                            const CAnnotObject_Info& info,
                            const SAnnotObject_Key& key);
    void x_UnmapAnnotObject(const CAnnotName& name,
                            const CAnnotObject_Info& info,
                            const SAnnotObject_Key& key);
    void x_UnmapAnnotObjects(const SAnnotObjectsIndex& infos);

    void x_MapFeatById(TFeatIdInt id,
                       CAnnotObject_Info& info,
                       bool xref);
    void x_UnmapFeatById(TFeatIdInt id,
                         CAnnotObject_Info& info,
                         bool xref);
    void x_MapFeatById(const TFeatIdStr& id,
                       CAnnotObject_Info& info,
                       bool xref);
    void x_UnmapFeatById(const TFeatIdStr& id,
                         CAnnotObject_Info& info,
                         bool xref);
    void x_MapFeatById(const TFeatId& id,
                       CAnnotObject_Info& info,
                       bool xref);
    void x_UnmapFeatById(const TFeatId& id,
                         CAnnotObject_Info& info,
                         bool xref);
    void x_MapFeatByLocus(const string& locus, bool tag,
                          CAnnotObject_Info& info);
    void x_UnmapFeatByLocus(const string& locus, bool tag,
                            CAnnotObject_Info& info);

    void x_MapChunkByFeatType(CSeqFeatData::ESubtype subtype,
                              TChunkId chunk_id);
    void x_MapChunkByFeatType(CSeqFeatData::E_Choice type,
                              TChunkId chunk_id);
    void x_MapChunkByFeatType(const SAnnotTypeSelector& type,
                              TChunkId chunk_id);

    void x_MapChunkByFeatId(TFeatIdInt feat_id,
                            CSeqFeatData::ESubtype subtype,
                            TChunkId chunk_id,
                            bool xref);
    void x_MapChunkByFeatId(TFeatIdInt feat_id,
                            CSeqFeatData::E_Choice type,
                            TChunkId chunk_id,
                            bool xref);
    void x_MapChunkByFeatId(TFeatIdInt feat_id,
                            const SAnnotTypeSelector& type,
                            TChunkId chunk_id,
                            bool xref);
    void x_MapChunkByFeatId(const TFeatIdStr& feat_id,
                            CSeqFeatData::ESubtype subtype,
                            TChunkId chunk_id,
                            bool xref);
    void x_MapChunkByFeatId(const TFeatIdStr& feat_id,
                            CSeqFeatData::E_Choice type,
                            TChunkId chunk_id,
                            bool xref);
    void x_MapChunkByFeatId(const TFeatIdStr& feat_id,
                            const SAnnotTypeSelector& type,
                            TChunkId chunk_id,
                            bool xref);
    void x_MapChunkByFeatId(const TFeatId& feat_id,
                            CSeqFeatData::ESubtype subtype,
                            TChunkId chunk_id,
                            bool xref);
    void x_MapChunkByFeatId(const TFeatId& feat_id,
                            CSeqFeatData::E_Choice type,
                            TChunkId chunk_id,
                            bool xref);
    void x_MapChunkByFeatId(const TFeatId& feat_id,
                            const SAnnotTypeSelector& type,
                            TChunkId chunk_id,
                            bool xref);

    friend class CTSEAnnotObjectMapper;

    void x_IndexSeqTSE(const CSeq_id_Handle& id);
    void x_UnindexSeqTSE(const CSeq_id_Handle& id);
    void x_IndexAnnotTSE(const CAnnotName& name, const CSeq_id_Handle& id);
    void x_UnindexAnnotTSE(const CAnnotName& name, const CSeq_id_Handle& id);

    void x_DoUpdate(TNeedUpdateFlags flags);

    //void x_RegisterRemovedIds(const CConstRef<CBioseq>&, CBioseq_Info*);
    CBioObjectId x_IndexBioseq(CBioseq_Info*);
    CBioObjectId x_IndexBioseq_set(CBioseq_set_Info*);

    CBioObjectId x_RegisterBioObject(CTSE_Info_Object& info);
    void x_UnregisterBioObject(CTSE_Info_Object& info);

    friend class CTSE_Lock;
    friend class CTSE_LoadLock;

    // Owner data-source
    CDataSource*           m_DataSource;

    //////////////////////////////////////////////////////////////////
    // TSE information in addition to CSeq_entry

    // Unique blob-id within data-source and corresponding data-loader
    TBlobId                m_BlobId;
    TBlobVersion           m_BlobVersion;

    // Suppression level
    TBlobState             m_BlobState;

    // TSE has name
    CAnnotName             m_Name;

    // estimations of memory useage, 0 - means unknown
    size_t                 m_UsedMemory;

    //////////////////////////////////////////////////////////////////
    // Runtime state within object manager

    enum ELoadState {
        eNotLoaded,
        eLoaded,
        eDropped
    };
    enum ECacheState {
        eNotInCache,
        eInCache
    };
    ELoadState              m_LoadState;
    mutable ECacheState     m_CacheState;
    
    typedef list< CRef<CTSE_Info> > TTSE_Cache;
    mutable TTSE_Cache::iterator   m_CachePosition;

    // lock counter for garbage collector
    mutable CAtomicCounter_WithAutoInit m_LockCounter;

    class CLoadMutex : public CObject, public CMutex
    {
    };

    CRef<CLoadMutex>       m_LoadMutex;

    // ID to bioseq-info
    TBioseq_sets           m_Bioseq_sets;
    TBioseqs               m_Bioseqs;
    TBioseq_sets           m_Removed_Bioseq_sets;
    TBioseqs               m_Removed_Bioseqs;
    mutable CFastMutex     m_BioseqsMutex;

    // Split chunks
    CRef<CTSE_Split_Info>  m_Split;

    // SNP info set: temporarily used in SetSeq_entry()
    CRef<CTSE_SetObjectInfo> m_SetObjectInfo;

    // Annot objects maps: ID to annot-selector-map
    TNamedAnnotObjs        m_NamedAnnotObjs;
    TIdAnnotInfoMap        m_IdAnnotInfoMap;
    TFeatIdIndex           m_FeatIdIndex;
    TLocusIndex            m_LocusIndex;

    mutable TAnnotLock     m_AnnotLock;
    mutable CSeq_id_Handle m_RequestedId;

    enum EAnnotIdsFlags {
        fAnnotIds_NonGi    = 1 << 0,  // Have annots with non-gi ids
        fAnnotIds_Matching = 1 << 1   // Have annots with multiple matching ids
    };
    typedef int TAnnotIdsFlags;

    // Do not use ID matching for annotations
    TAnnotIdsFlags m_AnnotIdsFlags;

    // information about original TSE for its copy
    struct SBaseTSE
    {
        SBaseTSE(const CTSE_Lock& tse)
            : m_BaseTSE(tse)
            {
            }

        CTSE_Lock      m_BaseTSE;
        TObjectCopyMap m_ObjectCopyMap;
    };
    auto_ptr<SBaseTSE> m_BaseTSE;

    CRef<IEditSaver>              m_EditSaver;
    int                           m_InternalBioObjNumber;
    typedef map<CBioObjectId, CTSE_Info_Object*> TBioObjects;
    TBioObjects m_BioObjects;

    mutable bool m_MasterSeqSegmentsLoaded;
    mutable CConstRef<CMasterSeqSegments> m_MasterSeqSegments;

private:
    // Hide copy methods
    CTSE_Info(const CTSE_Info&);
    CTSE_Info& operator= (const CTSE_Info&);
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////

inline
bool CTSE_Info::HasDataSource(void) const
{
    return m_DataSource != 0;
}


inline
CDataSource& CTSE_Info::GetDataSource(void) const
{
    _ASSERT(m_DataSource);
    return *m_DataSource;
}


inline
bool CTSE_Info::IsLocked(void) const
{
    return m_LockCounter.Get() != 0;
}


inline
const CTSE_Info& CTSE_Info::GetTSE_Info(void) const
{
    return *this;
}


inline
CTSE_Info& CTSE_Info::GetTSE_Info(void)
{
    return *this;
}


inline
CTSE_Info::TBlobState CTSE_Info::GetBlobState(void) const
{
    return m_BlobState;
}


inline
void CTSE_Info::ResetBlobState(TBlobState state)
{
    m_BlobState = state;
}


inline
void CTSE_Info::SetBlobState(TBlobState state)
{
    m_BlobState |= state;
}


inline
bool CTSE_Info::IsDead(void) const
{
    return (m_BlobState & CBioseq_Handle::fState_dead) != 0;
}


inline
bool CTSE_Info::IsUnavailable(void) const
{
    return (m_BlobState & CBioseq_Handle::fState_no_data) != 0;
}


inline
CTSE_Info::TBlobState CTSE_Info::GetBlobStateOrder(void) const
{
    return m_BlobState & (CBioseq_Handle::fState_dead |
                          CBioseq_Handle::fState_no_data);
}


inline
CTSE_Info::TBlobVersion CTSE_Info::GetBlobVersion(void) const
{
    return m_BlobVersion;
}


inline
CTSE_Info::TBlobOrder CTSE_Info::GetBlobOrder(void) const
{
    // less blob state order first,
    // higher blob version second
    return TBlobOrder(GetBlobStateOrder(), -GetBlobVersion());
}


inline
const CAnnotName& CTSE_Info::GetName(void) const
{
    return m_Name;
}


inline
size_t CTSE_Info::GetUsedMemory(void) const
{
    return m_UsedMemory;
}


inline
const CTSE_Info::TBlobId& CTSE_Info::GetBlobId(void) const
{
    return m_BlobId;
}


inline
CTSE_Info::TAnnotLock& CTSE_Info::GetAnnotLock(void) const
{
    return m_AnnotLock;
}


inline
const CSeq_id_Handle& CTSE_Info::GetRequestedId(void) const
{
    return m_RequestedId;
}


inline
void CTSE_Info::SetRequestedId(const CSeq_id_Handle& requested_id) const
{
    m_RequestedId = requested_id;
}


inline
bool CTSE_Info::HasMatchingAnnotIds(void) const
{
    return (m_AnnotIdsFlags & fAnnotIds_Matching) != 0;
}


inline
bool CTSE_Info::OnlyGiAnnotIds(void) const
{
    return (m_AnnotIdsFlags & fAnnotIds_NonGi) == 0;
}


inline
CRef<IEditSaver> CTSE_Info::GetEditSaver() const
{
    return m_EditSaver;
}


class CTSEAnnotObjectMapper
{
public:
    CTSEAnnotObjectMapper(CTSE_Info& tse, const CAnnotName& name)
        : m_TSE(tse), m_Name(name),
          m_AnnotObjs(tse.x_SetAnnotObjs(name))
        {
        }

    void Map(const SAnnotObject_Key& key,
             const SAnnotObject_Index& index) const
        {
            m_TSE.x_MapAnnotObject(m_AnnotObjs, m_Name, key, index);
        }

    void Unmap(const SAnnotObject_Key& key,
               const CAnnotObject_Info& info) const
        {
            m_TSE.x_UnmapAnnotObject(m_AnnotObjs, m_Name, info, key);
            if ( m_AnnotObjs.empty() ) {
                m_TSE.x_RemoveAnnotObjs(m_Name);
            }
        }

private:
    CTSE_Info& m_TSE;
    const CAnnotName& m_Name;
    CTSE_Info::TAnnotObjs& m_AnnotObjs;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJECTS_OBJMGR_IMPL___TSE_INFO__HPP */
