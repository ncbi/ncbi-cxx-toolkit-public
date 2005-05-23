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
#include <objmgr/impl/mutex_pool.hpp>

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

class NCBI_XOBJMGR_EXPORT CTSE_Chunk_Info : public CObject
{
public:
    //////////////////////////////////////////////////////////////////
    // types used
    //////////////////////////////////////////////////////////////////

    // chunk identification
    typedef CConstRef<CObject> TBlobId;
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

    //////////////////////////////////////////////////////////////////
    // loading control
    //////////////////////////////////////////////////////////////////
    bool NotLoaded(void) const;
    bool IsLoaded(void) const;
    void Load(void) const;

    //////////////////////////////////////////////////////////////////
    // chunk content identification
    // should be set before attaching to CTSE_Info
    //////////////////////////////////////////////////////////////////
    void x_AddDescInfo(TDescTypeMask type_mask, const TBioseqId& id);
    void x_AddDescInfo(TDescTypeMask type_mask, TBioseq_setId id);
    void x_AddDescInfo(const TDescInfo& info);

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
    void x_LoadAnnot(const TPlace& place, CRef<CSeq_annot_Info> annot);
    void x_LoadBioseq(const TPlace& place, const CBioseq& bioseq);
    void x_LoadSequence(const TPlace& place, TSeqPos pos,
                        const TSequence& seq);
    void x_LoadAssembly(const TPlace& place, const TAssembly& assembly);

protected:
    //////////////////////////////////////////////////////////////////
    // interaction with CTSE_Info
    //////////////////////////////////////////////////////////////////

    // attach to CTSE_Info
    void x_SplitAttach(CTSE_Split_Info& split_info);
    void x_TSEAttach(CTSE_Info& tse_info);
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
    void x_UnmapAnnotObjects(CTSE_Info& tse);
    void x_DropAnnotObjects(CTSE_Info& tse);
    void x_DropAnnotObjects(void);

    void x_InitObjectIndexList(void);

private:
    friend class CTSE_Info;
    friend class CTSE_Split_Info;

    CTSE_Chunk_Info(const CTSE_Chunk_Info&);
    CTSE_Chunk_Info& operator=(const CTSE_Chunk_Info&);

    CTSE_Split_Info*m_SplitInfo;
    TChunkId        m_ChunkId;

    bool            m_AnnotIndexEnabled;

    TDescInfos      m_DescInfos;
    TPlaces         m_AnnotPlaces;
    TBioseqPlaces   m_BioseqPlaces;
    TBioseqIds      m_BioseqIds;
    TAnnotContents  m_AnnotContents;
    TLocationSet    m_Seq_data;

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


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  2005/05/23 14:10:20  grichenk
* Added comments
*
* Revision 1.17  2005/04/20 15:07:41  ucko
* Revert previous change -- fixed properly in mutex_pool.hpp.
*
* Revision 1.16  2005/04/19 21:00:42  ucko
* Tweak to compile properly under WorkShop.
*
* Revision 1.15  2005/03/15 19:14:27  vasilche
* Correctly update and check  bioseq ids in split blobs.
*
* Revision 1.14  2004/12/22 15:56:27  vasilche
* Use SAnnotObjectsIndex.
*
* Revision 1.13  2004/10/18 13:59:22  vasilche
* Added support for split history assembly.
* Added support for split non-gi sequences.
*
* Revision 1.12  2004/10/07 14:03:32  vasilche
* Use shared among TSEs CTSE_Split_Info.
* Use typedefs and methods for TSE and DataSource locking.
* Load split CSeqdesc on the fly in CSeqdesc_CI.
*
* Revision 1.11  2004/08/31 14:22:56  vasilche
* Postpone indexing of split blobs.
*
* Revision 1.10  2004/08/19 14:20:58  vasilche
* Added splitting of whole Bioseqs.
*
* Revision 1.9  2004/08/05 18:25:18  vasilche
* CAnnotName and CAnnotTypeSelector are moved in separate headers.
*
* Revision 1.8  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.7  2004/07/12 16:57:32  vasilche
* Fixed loading of split Seq-descr and Seq-data objects.
* They are loaded correctly now when GetCompleteXxx() method is called.
*
* Revision 1.6  2004/06/15 14:06:49  vasilche
* Added support to load split sequences.
*
* Revision 1.5  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.4  2004/01/22 20:10:39  vasilche
* 1. Splitted ID2 specs to two parts.
* ID2 now specifies only protocol.
* Specification of ID2 split data is moved to seqsplit ASN module.
* For now they are still reside in one resulting library as before - libid2.
* As the result split specific headers are now in objects/seqsplit.
* 2. Moved ID2 and ID1 specific code out of object manager.
* Protocol is processed by corresponding readers.
* ID2 split parsing is processed by ncbi_xreader library - used by all readers.
* 3. Updated OBJMGR_LIBS correspondingly.
*
* Revision 1.3  2003/11/26 17:55:55  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
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
* Revision 1.1  2003/09/30 16:22:01  vasilche
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

#endif//OBJECTS_OBJMGR_IMPL___TSE_CHUNK_INFO__HPP
