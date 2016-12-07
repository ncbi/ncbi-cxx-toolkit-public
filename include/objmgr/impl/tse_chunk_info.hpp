#ifndef OBJECTS_OBJMGR_IMPL___TSE_CHUNK_INFO__HPP
#define OBJECTS_OBJMGR_IMPL___TSE_CHUNK_INFO__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   Split TSE chunk info
*
*/


#include <corelib/ncbiobj.hpp>

#include <objmgr/annot_name.hpp>
#include <objmgr/annot_type_selector.hpp>
#include <objmgr/impl/annot_object_index.hpp>
#include <util/mutex_pool.hpp>
#include <objmgr/blob_id.hpp>

#include <vector>
#include <list>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CTSE_Info;
class CTSE_Split_Info;
class CSeq_entry_Info;
class CSeq_annot_Info;
class CSeq_literal;
class CSeq_descr;
class CSeq_annot;
class CBioseq_Base_Info;
class CBioseq_Info;
class CBioseq_set_Info;
class CDataLoader;
class CTSE_SetObjectInfo;
class ITSE_Assigner;
class CTSE_Default_Assigner;


class NCBI_XOBJMGR_EXPORT CTSE_Chunk_Info : public CObject
{
public:
    //////////////////////////////////////////////////////////////////
    // types used
    //////////////////////////////////////////////////////////////////

    // chunk identification
    typedef CBlobIdKey TBlobId;
    typedef int TBlobVersion;
    typedef int TChunkId;

    // contents place identification
    typedef int TBioseq_setId;
    typedef CSeq_id_Handle TBioseqId;
    typedef pair<TBioseqId, TBioseq_setId> TPlace;
    typedef unsigned TDescTypeMask;
    typedef pair<TDescTypeMask, TPlace> TDescInfo;
    typedef vector<TPlace> TPlaces;
    typedef vector<TDescInfo> TDescInfos;
    typedef vector<TBioseq_setId> TBioseqPlaces;
    typedef vector<TBioseqId> TBioseqIds;
    typedef TBioseqId TAssemblyInfo;
    typedef vector<TAssemblyInfo> TAssemblyInfos;

    // annot contents identification
    typedef CSeq_id_Handle TLocationId;
    typedef CRange<TSeqPos> TLocationRange;
    typedef pair<TLocationId, TLocationRange> TLocation;
    typedef vector<TLocation> TLocationSet;
    typedef map<SAnnotTypeSelector, TLocationSet> TAnnotTypes;
    typedef map<CAnnotName, TAnnotTypes> TAnnotContents;

    // annot contents indexing
    typedef SAnnotObjectsIndex TObjectIndex;
    typedef list<TObjectIndex> TObjectIndexList;

    // attached data types
    typedef list< CRef<CSeq_literal> > TSequence;
    typedef list< CRef<CSeq_align> > TAssembly;

    //////////////////////////////////////////////////////////////////
    // constructor & destructor
    //////////////////////////////////////////////////////////////////
    CTSE_Chunk_Info(TChunkId id);
    virtual ~CTSE_Chunk_Info(void);

    //////////////////////////////////////////////////////////////////
    // chunk identification getters
    //////////////////////////////////////////////////////////////////
    TBlobId GetBlobId(void) const;
    TBlobVersion GetBlobVersion(void) const;
    TChunkId GetChunkId(void) const;
    const CTSE_Split_Info& GetSplitInfo(void) const;

    //////////////////////////////////////////////////////////////////
    // loading control
    //////////////////////////////////////////////////////////////////
    bool NotLoaded(void) const;
    bool IsLoaded(void) const;
    void Load(void) const;
    CInitGuard* GetLoadInitGuard(void);

    //////////////////////////////////////////////////////////////////
    // chunk content identification
    // should be set before attaching to CTSE_Info
    //////////////////////////////////////////////////////////////////
    void x_AddDescInfo(TDescTypeMask type_mask, const TBioseqId& id);
    void x_AddDescInfo(TDescTypeMask type_mask, TBioseq_setId id);
    void x_AddDescInfo(const TDescInfo& info);

    void x_AddAssemblyInfo(const TBioseqId& id);

    void x_AddAnnotPlace(const TBioseqId& id);
    void x_AddAnnotPlace(TBioseq_setId id);
    void x_AddAnnotPlace(const TPlace& place);

    // The bioseq-set contains some bioseq(s)
    void x_AddBioseqPlace(TBioseq_setId id);
    // The chunk contains the whole bioseq and its annotations,
    // the annotations can not refer other bioseqs.
    void x_AddBioseqId(const TBioseqId& id);

    void x_AddAnnotType(const CAnnotName& annot_name,
                        const SAnnotTypeSelector& annot_type,
                        const TLocationId& location_id);
    void x_AddAnnotType(const CAnnotName& annot_name,
                        const SAnnotTypeSelector& annot_type,
                        const TLocationId& location_id,
                        const TLocationRange& location_range);
    void x_AddAnnotType(const CAnnotName& annot_name,
                        const SAnnotTypeSelector& annot_type,
                        const TLocationSet& location);

    // The chunk contains features with ids
    void x_AddFeat_ids(void);
    typedef int TFeatIdInt;
    typedef string TFeatIdStr;
    typedef vector<TFeatIdInt> TFeatIdIntList;
    typedef list<TFeatIdStr> TFeatIdStrList;
    struct SFeatIds {
        TFeatIdIntList m_IntList;
        TFeatIdStrList m_StrList;
    };
    typedef map<SAnnotTypeSelector, SFeatIds> TFeatIdsMap;

    void x_AddFeat_ids(const SAnnotTypeSelector& type,
                       const TFeatIdIntList& ids);
    void x_AddXref_ids(const SAnnotTypeSelector& type,
                       const TFeatIdIntList& ids);
    void x_AddFeat_ids(const SAnnotTypeSelector& type,
                       const TFeatIdStrList& ids);
    void x_AddXref_ids(const SAnnotTypeSelector& type,
                       const TFeatIdStrList& ids);

    // The chunk contains seq-data. The corresponding bioseq's
    // data should be not set or set to delta with empty literal(s)
    void x_AddSeq_data(const TLocationSet& location);

    //////////////////////////////////////////////////////////////////
    // chunk data loading interface
    // is called from CDataLoader
    //////////////////////////////////////////////////////////////////

    // synchronization
    operator CInitMutex_Base&(void)
        {
            return m_LoadLock;
        }
    void SetLoaded(CObject* obj = 0);

    // data attachment
    void x_LoadDescr(const TPlace& place, const CSeq_descr& descr);
    void x_LoadAnnot(const TPlace& place, const CSeq_annot& annot);
    NCBI_DEPRECATED
    void x_LoadBioseq(const TPlace& place, const CBioseq& bioseq);
    void x_LoadBioseqs(const TPlace& place, const list< CRef<CBioseq> >& bioseqs);
    void x_LoadSequence(const TPlace& place, TSeqPos pos,
                        const TSequence& seq);
    void x_LoadAssembly(const TBioseqId& seq_id, const TAssembly& assembly);

    void x_LoadSeq_entry(CSeq_entry& entry, CTSE_SetObjectInfo* set_info = 0);

    //////////////////////////////////////////////////////////////////
    // methods to find out what information is needed to be loaded
    //////////////////////////////////////////////////////////////////
    const TDescInfos& GetDescInfos(void) const
        {
            return m_DescInfos;
        }
    const TPlaces GetAnnotPlaces(void) const
        {
            return m_AnnotPlaces;
        }
    const TBioseqPlaces GetBioseqPlaces(void) const
        {
            return m_BioseqPlaces;
        }
    const TBioseqIds GetBioseqIds(void) const
        {
            return m_BioseqIds;
        }
    const TAnnotContents GetAnnotContents(void) const
        {
            return m_AnnotContents;
        }
    const TLocationSet& GetSeq_dataInfos(void) const
        {
            return m_Seq_data;
        }
    const TAssemblyInfos& GetAssemblyInfos(void) const
        {
            return m_AssemblyInfos;
        }

protected:
    //////////////////////////////////////////////////////////////////
    // interaction with CTSE_Info
    //////////////////////////////////////////////////////////////////

    // attach to CTSE_Info
    void x_SplitAttach(CTSE_Split_Info& split_info);
    void x_TSEAttach(CTSE_Info& tse, ITSE_Assigner& tse_info);
    bool x_Attached(void) const;

    // return true if chunk is loaded
    bool x_GetRecords(const CSeq_id_Handle& id, bool bioseq) const;

    // append ids with all Bioseqs Seq-ids from this Split-Info
    void GetBioseqsIds(TBioseqIds& ids) const;

    // biose lookup
    bool ContainsBioseq(const CSeq_id_Handle& id) const;

    // annot index maintainance
    void x_EnableAnnotIndex(void);
    void x_DisableAnnotIndexWhenLoaded(void);
    void x_UpdateAnnotIndex(CTSE_Info& tse);
    void x_UpdateAnnotIndexContents(CTSE_Info& tse);
    bool x_ContainsFeatType(CSeqFeatData::E_Choice type) const;
    bool x_ContainsFeatType(CSeqFeatData::ESubtype subtype) const;
    bool x_ContainsFeatIds(CSeqFeatData::E_Choice type,
                           EFeatIdType id_type) const;
    bool x_ContainsFeatIds(CSeqFeatData::ESubtype subtype,
                           EFeatIdType id_type) const;

    //void x_UnmapAnnotObjects(CTSE_Info& tse);
    //void x_DropAnnotObjects(CTSE_Info& tse);
    void x_DropAnnotObjects(void);

    void x_InitObjectIndexList(void);

private:
    friend class CTSE_Info;
    friend class CTSE_Split_Info;
    
    friend class CTSE_Default_Assigner;


    CTSE_Chunk_Info(const CTSE_Chunk_Info&);
    CTSE_Chunk_Info& operator=(const CTSE_Chunk_Info&);

    CTSE_Split_Info* m_SplitInfo;
    TChunkId         m_ChunkId;

    bool             m_AnnotIndexEnabled;
    bool             m_ExplicitFeatIds;

    TDescInfos       m_DescInfos;
    TPlaces          m_AnnotPlaces;
    TBioseqPlaces    m_BioseqPlaces;
    TBioseqIds       m_BioseqIds;
    TAnnotContents   m_AnnotContents;
    TLocationSet     m_Seq_data;
    TAssemblyInfos   m_AssemblyInfos;

    TFeatIdsMap      m_FeatIds;
    TFeatIdsMap      m_XrefIds;

    CInitMutex<CObject> m_LoadLock;
    TObjectIndexList m_ObjectIndexList;
};


inline
CTSE_Chunk_Info::TChunkId CTSE_Chunk_Info::GetChunkId(void) const
{
    return m_ChunkId;
}


inline
bool CTSE_Chunk_Info::NotLoaded(void) const
{
    return !m_LoadLock;
}


inline
bool CTSE_Chunk_Info::IsLoaded(void) const
{
    return m_LoadLock;
}


inline
const CTSE_Split_Info& CTSE_Chunk_Info::GetSplitInfo(void) const
{
    _ASSERT(m_SplitInfo);
    return *m_SplitInfo;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJECTS_OBJMGR_IMPL___TSE_CHUNK_INFO__HPP
