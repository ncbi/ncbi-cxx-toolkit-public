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
#include <objmgr/annot_selector.hpp>
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
    // 'ctors
    // Argument tse will be parentized.
    explicit CTSE_Info(CSeq_entry& tse,
                       bool dead = false,
                       const CObject* blob_id = 0);
    explicit CTSE_Info(const CTSE_Info& info);
    virtual ~CTSE_Info(void);

    bool HasDataSource(void) const;
    CDataSource& GetDataSource(void) const;

    CConstRef<CSeq_entry> GetCompleteTSE(void) const;
    CConstRef<CSeq_entry> GetTSECore(void) const;

    const CTSE_Info& GetTSE_Info(void) const;
    CTSE_Info& GetTSE_Info(void);

    bool IsDead(void) const;
    void SetDead(bool dead);
    bool Locked(void) const;

    const CAnnotName& GetName(void) const;
    void SetName(const CAnnotName& name);

    bool HasAnnot(const CAnnotName& name) const;
    bool HasUnnamedAnnot(void) const;
    bool HasNamedAnnot(const string& name) const;

    const CConstRef<CObject>& GetBlobId(void) const;
    void SetBlobId(const CObject* blob_id);

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

    typedef int                                              TChunkId;
    typedef map<TChunkId, CRef<CTSE_Chunk_Info> >            TChunks;

    bool ContainsSeqid(const CSeq_id_Handle& id) const;
    CConstRef<CBioseq_Info> FindBioseq(const CSeq_id_Handle& key) const;

    void UpdateAnnotIndex(void) const;
    void UpdateAnnotIndex(const CTSE_Info_Object& object) const;
    void UpdateAnnotIndex(void);
    void UpdateAnnotIndex(CTSE_Info_Object& object);

    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

    void x_DSAttachContents(CDataSource& ds);
    void x_DSDetachContents(CDataSource& ds);
    
    virtual void x_SetDirtyAnnotIndexNoParent(void);
    virtual void x_ResetDirtyAnnotIndexNoParent(void);

    CRef<CTSE_Chunk_Info> GetNotLoadedChunk(void);
    void LoadAllChunks(void);

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
    friend struct SAnnotSelector;

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

    // Parent data-source
    CDataSource*           m_DataSource;

    // Dead seq-entry flag
    bool                   m_Dead;

    // TSE has name
    CAnnotName             m_Name;

    // May be used by data loaders to store blob-id
    typedef CConstRef<CObject> TBlob_ID;
    TBlob_ID               m_Blob_ID;

    typedef CRWLock        TAnnotObjsLock;
    typedef TAnnotObjsLock::TReadLockGuard  TAnnotReadLockGuard;
    typedef TAnnotObjsLock::TWriteLockGuard TAnnotWriteLockGuard;

    // ID to bioseq-info
    TBioseq_sets           m_Bioseq_sets;
    TBioseqs               m_Bioseqs;

    // ID to annot-selector-map
    TNamedAnnotObjs        m_NamedAnnotObjs;
    TSeqIdToNames          m_SeqIdToNames;
    mutable TAnnotObjsLock m_AnnotObjsLock;

    TChunks                m_Chunks;

    // Hide copy methods
    CTSE_Info& operator= (const CTSE_Info&);

};


typedef CConstRef<CTSE_Info> TTSE_Lock;

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
bool CTSE_Info::IsDead(void) const
{
    return m_Dead;
}


inline
void CTSE_Info::SetDead(bool dead)
{
    m_Dead = dead;
}


inline
bool CTSE_Info::Locked(void) const
{
    return !ReferencedOnlyOnce();
}


inline
const CAnnotName& CTSE_Info::GetName(void) const
{
    return m_Name;
}


inline
const CConstRef<CObject>& CTSE_Info::GetBlobId(void) const
{
    return m_Blob_ID;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.50  2004/07/12 15:05:31  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.49  2004/04/05 15:56:13  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.48  2004/03/24 18:30:29  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.47  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.46  2004/02/19 17:17:00  vasilche
* Formatting.
*
* Revision 1.45  2004/02/04 18:05:33  grichenk
* Added annotation filtering by set of types/subtypes.
* Renamed *Choice to *Type in SAnnotSelector.
*
* Revision 1.44  2004/01/22 20:10:39  vasilche
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
* Revision 1.43  2003/11/26 17:55:55  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.42  2003/10/27 16:47:12  vasilche
* Fixed error:
* src/objmgr/data_source.cpp", line 913: Fatal: Assertion failed: (it != tse_map.end() && it->first == id)
*
* Revision 1.41  2003/10/09 20:20:59  vasilche
* Added possibility to include and exclude Seq-annot names to annot iterator.
* Fixed adaptive search. It looked only on selected set of annot names before.
*
* Revision 1.40  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.39  2003/09/30 16:22:01  vasilche
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
* Revision 1.38  2003/09/16 14:21:46  grichenk
* Added feature indexing and searching by subtype.
*
* Revision 1.37  2003/08/14 20:05:19  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.36  2003/07/18 16:57:52  rsmith
* Do not leave redundant class qualifiers
*
* Revision 1.35  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.34  2003/07/14 21:13:24  grichenk
* Added possibility to resolve seq-map iterator withing a single TSE
* and to skip intermediate references during this resolving.
*
* Revision 1.33  2003/06/24 14:25:18  vasilche
* Removed obsolete CTSE_Guard class.
* Used separate mutexes for bioseq and annot maps.
*
* Revision 1.32  2003/06/19 18:23:45  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.31  2003/06/02 16:01:37  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.30  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.29  2003/05/06 18:54:08  grichenk
* Moved TSE filtering from CDataSource to CScope, changed
* some filtering rules (e.g. priority is now more important
* than scope history). Added more caches to CScope.
*
* Revision 1.28  2003/04/25 14:23:46  vasilche
* Added explicit constructors, destructor and assignment operator to make it compilable on MSVC DLL.
*
* Revision 1.27  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.26  2003/03/21 19:22:50  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.25  2003/03/18 14:50:08  grichenk
* Made CounterOverflow() and CounterUnderflow() private
*
* Revision 1.24  2003/03/14 19:10:39  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.23  2003/03/12 20:09:31  grichenk
* Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
*
* Revision 1.22  2003/03/05 20:56:43  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.21  2003/02/25 20:10:38  grichenk
* Reverted to single total-range index for annotations
*
* Revision 1.20  2003/02/25 14:48:07  vasilche
* Added Win32 export modifier to object manager classes.
*
* Revision 1.19  2003/02/24 18:57:21  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.18  2003/02/13 14:34:32  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.17  2003/02/05 17:57:41  dicuccio
* Moved into include/objects/objmgr/impl to permit data loaders to be defined
* outside of xobjmgr.
*
* Revision 1.16  2003/02/04 21:46:31  grichenk
* Added map of annotations by intervals (the old one was
* by total ranges)
*
* Revision 1.15  2003/01/29 22:02:22  grichenk
* Fixed includes for SAnnotSelector
*
* Revision 1.14  2003/01/29 17:45:05  vasilche
* Annotaions index is split by annotation/feature type.
*
* Revision 1.13  2002/12/26 21:03:33  dicuccio
* Added Win32 export specifier.  Moved file from src/objects/objmgr to
* include/objects/objmgr.
*
* Revision 1.12  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.11  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.10  2002/10/18 19:12:41  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.9  2002/07/10 16:50:33  grichenk
* Fixed bug with duplicate and uninitialized atomic counters
*
* Revision 1.8  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.7  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.6  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.5  2002/05/03 21:28:11  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.4  2002/05/02 20:42:38  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.3  2002/03/14 18:39:14  gouriano
* added mutex for MT protection
*
* Revision 1.2  2002/02/21 19:27:07  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/02/07 21:25:06  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_OBJMGR_IMPL___TSE_INFO__HPP */
