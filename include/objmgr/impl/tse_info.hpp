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
#include <objmgr/annot_name.hpp>
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
class CTSE_Info_Object;
class CTSE_Lock;
class CTSE_LoadLock;

class CDataSource;
class CHandleRange;
class CAnnotObject_Info;
class CAnnotTypes_CI;
class CSeq_entry;

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
    typedef vector<TRangeMap>                                TAnnotSet;
    typedef vector<CConstRef<CSeq_annot_SNP_Info> >          TSNPSet;

    TAnnotSet m_AnnotSet;
    TSNPSet   m_SNPSet;

private:
    const SIdAnnotObjs& operator=(const SIdAnnotObjs& objs);
};


class NCBI_XOBJMGR_EXPORT CTSE_Info : public CSeq_entry_Info
{
    typedef CSeq_entry_Info TParent;
public:
    enum ESuppression_Level {
        eSuppression_none,
        eSuppression_temporary,
        eSuppression_permanent,
        eSuppression_dead,
        eSuppression_private,
        eSuppression_withdrawn
    };
    typedef CConstRef<CObject> TBlobId;
    typedef int                TBlobVersion;

    // 'ctors
    // Argument tse will be parentized.
    explicit CTSE_Info(void);
    explicit CTSE_Info(const TBlobId& blob_id,
                       TBlobVersion blob_version = -1);
    explicit CTSE_Info(CSeq_entry& tse,
                       ESuppression_Level level = eSuppression_none);
    explicit CTSE_Info(CSeq_entry& tse,
                       ESuppression_Level level,
                       const TBlobId& blob_id,
                       TBlobVersion blob_version = -1);
    explicit CTSE_Info(const CTSE_Info& info);
    virtual ~CTSE_Info(void);

    bool HasDataSource(void) const;
    CDataSource& GetDataSource(void) const;

    CConstRef<CSeq_entry> GetCompleteTSE(void) const;
    CConstRef<CSeq_entry> GetTSECore(void) const;

    const CTSE_Info& GetTSE_Info(void) const;
    CTSE_Info& GetTSE_Info(void);

    // Additional TSE info not available in CSeq_entry
    const TBlobId& GetBlobId(void) const;
    TBlobVersion GetBlobVersion(void) const;
    void SetBlobVersion(TBlobVersion version);

    ESuppression_Level GetSuppressionLevel(void) const;
    void SetSuppressionLevel(ESuppression_Level level);
    bool IsDead(void) const;

    const CAnnotName& GetName(void) const;
    void SetName(const CAnnotName& name);
    void SetSeq_entry(CSeq_entry& entry);

    size_t GetUsedMemory(void) const;
    void SetUsedMemory(size_t size);

    // Annot index access
    bool HasAnnot(const CAnnotName& name) const;
    bool HasUnnamedAnnot(void) const;
    bool HasNamedAnnot(const string& name) const;

    // indexes types
    typedef map<int, CBioseq_set_Info*>                      TBioseq_sets;
    typedef map<CSeq_id_Handle, CBioseq_Info*>               TBioseqs;

    typedef SIdAnnotObjs::TRange                             TRange;
    typedef SIdAnnotObjs::TRangeMap                          TRangeMap;
    typedef SIdAnnotObjs::TAnnotSet                          TAnnotSet;
    typedef SIdAnnotObjs::TSNPSet                            TSNPSet;

    typedef map<CSeq_id_Handle, SIdAnnotObjs>                TAnnotObjs;
    typedef map<CAnnotName, TAnnotObjs>                      TNamedAnnotObjs;
    typedef set<CAnnotName>                                  TNames;
    typedef map<CSeq_id_Handle, TNames>                      TSeqIdToNames;

    typedef map<TChunkId, CRef<CTSE_Chunk_Info> >            TChunks;

    bool ContainsSeqid(const CSeq_id_Handle& id) const;
    CConstRef<CBioseq_Info> FindBioseq(const CSeq_id_Handle& key) const;

    void UpdateAnnotIndex(void) const;
    void UpdateAnnotIndex(const CTSE_Info_Object& object) const;
    void UpdateAnnotIndex(void);
    void UpdateAnnotIndex(CTSE_Info_Object& object);

    void x_DSAttachContents(CDataSource& ds);
    void x_DSDetachContents(CDataSource& ds);
    
    virtual void x_SetDirtyAnnotIndexNoParent(void);
    virtual void x_ResetDirtyAnnotIndexNoParent(void);

    CTSE_Chunk_Info& GetChunk(TChunkId chunk_id);

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
    friend class CTSE_Chunk_Info;
    friend class CSeq_annot_SNP_Info;
    friend class CSeqMatch_Info;

    CBioseq_set_Info& GetBioseq_set(int id);
    CBioseq_Info& GetBioseq(int gi);
    
    void x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds);
    void x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds);

    void x_SetBioseqId(const CSeq_id_Handle& key, CBioseq_Info* info);
    void x_ResetBioseqId(const CSeq_id_Handle& key, CBioseq_Info* info);
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
    SIdAnnotObjs& x_SetIdObjects(TAnnotObjs& objs,
                                 const CAnnotName& name,
                                 const CSeq_id_Handle& id);
    SIdAnnotObjs& x_SetIdObjects(const CAnnotName& name,
                                 const CSeq_id_Handle& idh);

    void x_MapSNP_Table(const CAnnotName& name,
                        const CSeq_id_Handle& key,
                        const CSeq_annot_SNP_Info& snp_info);
    void x_UnmapSNP_Table(const CAnnotName& name,
                          const CSeq_id_Handle& key,
                          const CSeq_annot_SNP_Info& snp_info);

    void x_MapAnnotObject(TRangeMap& rangeMap,
                          const SAnnotObject_Key& key,
                          const SAnnotObject_Index& annotRef);
    bool x_UnmapAnnotObject(TRangeMap& rangeMap,
                            const SAnnotObject_Key& key);
    void x_MapAnnotObject(SIdAnnotObjs& objs,
                          const SAnnotObject_Key& key,
                          const SAnnotObject_Index& annotRef);
    bool x_UnmapAnnotObject(SIdAnnotObjs& objs,
                            const SAnnotObject_Key& key);
    void x_MapAnnotObject(TAnnotObjs& objs,
                          const CAnnotName& name,
                          const SAnnotObject_Key& key,
                          const SAnnotObject_Index& annotRef);
    void x_MapAnnotObject(TAnnotObjs& index,
                          const SAnnotObject_Key& key,
                          const SAnnotObject_Index& annotRef,
                          SAnnotObjects_Info& infos);
    void x_MapAnnotObject(const SAnnotObject_Key& key,
                          const SAnnotObject_Index& annotRef,
                          SAnnotObjects_Info& infos);

    bool x_UnmapAnnotObject(TAnnotObjs& objs,
                            const CAnnotName& name,
                            const SAnnotObject_Key& key);
    void x_UnmapAnnotObjects(SAnnotObjects_Info& infos);

    void x_IndexSeqTSE(const CSeq_id_Handle& id);
    void x_UnindexSeqTSE(const CSeq_id_Handle& id);
    void x_IndexAnnotTSE(const CAnnotName& name, const CSeq_id_Handle& id);
    void x_UnindexAnnotTSE(const CAnnotName& name, const CSeq_id_Handle& id);

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
    ESuppression_Level     m_SuppressionLevel;

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
    mutable CAtomicCounter m_LockCounter;

    class CLoadMutex : public CObject, public CMutex
    {
    };

    CRef<CLoadMutex>       m_LoadMutex;

    // ID to bioseq-info
    TBioseq_sets           m_Bioseq_sets;
    TBioseqs               m_Bioseqs;
    // Split chunks
    TChunks                m_Chunks;

    // Annot objects maps: ID to annot-selector-map
    TNamedAnnotObjs        m_NamedAnnotObjs;
    TSeqIdToNames          m_SeqIdToNames;

    // annot object map mutex
    typedef CRWLock        TAnnotObjsLock;
    typedef TAnnotObjsLock::TReadLockGuard  TAnnotReadLockGuard;
    typedef TAnnotObjsLock::TWriteLockGuard TAnnotWriteLockGuard;
    mutable TAnnotObjsLock m_AnnotObjsLock;

private:
    // Hide copy methods
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
CTSE_Info::ESuppression_Level CTSE_Info::GetSuppressionLevel(void) const
{
    return m_SuppressionLevel;
}


inline
void CTSE_Info::SetSuppressionLevel(ESuppression_Level level)
{
    m_SuppressionLevel = level;
}


inline
bool CTSE_Info::IsDead(void) const
{
    return GetSuppressionLevel() >= eSuppression_dead;
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


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJECTS_OBJMGR_IMPL___TSE_INFO__HPP */
