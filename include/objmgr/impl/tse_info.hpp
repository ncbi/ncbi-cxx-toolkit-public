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
#include <objmgr/seq_id_handle.hpp>

#include <util/rangemap.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>

#include <map>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBioseq_Info;
class CSeq_entry_Info;
class CHandleRange;

////////////////////////////////////////////////////////////////////
//
//  CTSE_Info::
//
//    General information and indexes for top level seq-entries
//


// forward declaration
class CSeq_entry;
class CBioseq;
class CDataSource;
class CAnnotObject_Info;
class CSeq_annot_Info;
class CSeq_annot_SNP_Info;
class CAnnotTypes_CI;

struct NCBI_XOBJMGR_EXPORT SAnnotObject_Key
{
    CAnnotObject_Info*      m_AnnotObject_Info;
    CSeq_id_Handle          m_Handle;
    CRange<TSeqPos>         m_Range;
};

struct NCBI_XOBJMGR_EXPORT SAnnotObject_Index
{
    SAnnotObject_Index(void);
    ~SAnnotObject_Index(void);
    SAnnotObject_Index(const SAnnotObject_Index&);
    SAnnotObject_Index& operator=(const SAnnotObject_Index&);

    CAnnotObject_Info*                  m_AnnotObject_Info;
    int                                 m_AnnotLocationIndex;
    CRef< CObjectFor<CHandleRange> >    m_HandleRange;
};

class NCBI_XOBJMGR_EXPORT CTSE_Info : public CSeq_entry_Info
{
public:
    // 'ctors
    CTSE_Info(CDataSource* data_source,
              CSeq_entry& tse,
              bool dead,
              const CObject* blob_id);
    virtual ~CTSE_Info(void);

    CDataSource& GetDataSource(void) const;

    const CSeq_entry& GetTSE(void) const;
    CSeq_entry& GetTSE(void);

    const CTSE_Info& GetTSE_Info(void) const;
    CTSE_Info& GetTSE_Info(void);

    bool IsDead(void) const;
    bool Locked(void) const;

    const CConstRef<CObject>& GetBlobId(void) const;

    // indexes types
    typedef map<CSeq_id_Handle, CRef<CBioseq_Info> >         TBioseqs;

    typedef CRange<TSeqPos>                                  TRange;
    typedef CRangeMultimap<SAnnotObject_Index,
                           TRange::position_type>            TRangeMap;
    typedef vector<TRangeMap>                                TAnnotSet;
    typedef vector<CRef<CSeq_annot_SNP_Info> >               TSNPSet;

    struct NCBI_XOBJMGR_EXPORT SIdAnnotObjs
    {
        SIdAnnotObjs(void);
        ~SIdAnnotObjs(void);
        SIdAnnotObjs(const SIdAnnotObjs& objs);
        const SIdAnnotObjs& operator=(const SIdAnnotObjs& objs);

        TAnnotSet m_AnnotSet;
        TSNPSet   m_SNPSet;
    };

    typedef map<CSeq_id_Handle, SIdAnnotObjs>                TAnnotObjs;

    bool ContainsSeqid(CSeq_id_Handle id) const;

    void AddSNP_annot_Info(CSeq_annot_SNP_Info& snp_info);

    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

private:
    friend class CTSE_Guard;
    friend class CDataSource;
    friend class CScope;
    friend class CDataLoader;
    friend class CAnnotTypes_CI;
    friend class CSeq_annot_Info;

    static pair<size_t, size_t> x_GetIndexRange(int annot_type,
                                                           int feat_type);
    static size_t x_GetTypeIndex(int annot_type, int feat_type);
    static size_t x_GetTypeIndex(const SAnnotObject_Key& key);

    // index access methods
    const SIdAnnotObjs* x_GetIdObjects(const CSeq_id_Handle& id) const;
    SIdAnnotObjs& x_SetIdObjects(const CSeq_id_Handle& idh);

    const TRangeMap* x_GetRangeMap(const SIdAnnotObjs& objs,
                                   size_t index) const;
    const TRangeMap* x_GetRangeMap(const CSeq_id_Handle& id,
                                   size_t index) const;
    const TRangeMap* x_GetRangeMap(const CSeq_id_Handle& id,
                                   int annot_type,
                                   int feat_type) const;

    void x_MapAnnotObject(TRangeMap& rangeMap,
                          const SAnnotObject_Key& key,
                          const SAnnotObject_Index& annotRef);
    bool x_DropAnnotObject(TRangeMap& rangeMap,
                           const SAnnotObject_Key& key);
    void x_MapAnnotObject(SIdAnnotObjs& objs,
                          const SAnnotObject_Key& key,
                          const SAnnotObject_Index& annotRef);
    bool x_DropAnnotObject(SIdAnnotObjs& objs,
                           const SAnnotObject_Key& key);
    void x_MapAnnotObject(const SAnnotObject_Key& key,
                          const SAnnotObject_Index& annotRef);
    void x_DropAnnotObject(const SAnnotObject_Key& key);


    // Parent data-source
    CDataSource* m_DataSource;

    // Dead seq-entry flag
    bool m_Dead;

    typedef CFastMutex      TBioseqsLock;
    typedef CRWLock         TAnnotObjsLock;

    // ID to bioseq-info
    TBioseqs               m_Bioseqs;
    mutable TBioseqsLock   m_BioseqsLock;

    // ID to annot-selector-map
    TAnnotObjs             m_AnnotObjs;
    mutable TAnnotObjsLock m_AnnotObjsLock;

    // May be used by data loaders to store blob-id
    typedef CConstRef<CObject> TBlob_ID;
    TBlob_ID   m_Blob_ID;

    // Hide copy methods
    CTSE_Info(const CTSE_Info&);
    CTSE_Info& operator= (const CTSE_Info&);

    bool m_DirtyAnnotIndex;
};


typedef CConstRef<CTSE_Info> TTSE_Lock;

/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////

inline
CDataSource& CTSE_Info::GetDataSource(void) const
{
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
const CSeq_entry& CTSE_Info::GetTSE(void) const
{
    return GetSeq_entry();
}


inline
CSeq_entry& CTSE_Info::GetTSE(void)
{
    return GetSeq_entry();
}


inline
bool CTSE_Info::IsDead(void) const
{
    return m_Dead;
}


inline
bool CTSE_Info::Locked(void) const
{
    return !ReferencedOnlyOnce();
}


inline
const CConstRef<CObject>& CTSE_Info::GetBlobId(void) const
{
    return m_Blob_ID;
}

inline
bool CTSE_Info::ContainsSeqid(CSeq_id_Handle id) const
{
    CFastMutexGuard guard(m_BioseqsLock);
    return m_Bioseqs.find(id) != m_Bioseqs.end();
}


inline
const CTSE_Info::TRangeMap*
CTSE_Info::x_GetRangeMap(const SIdAnnotObjs& objs, size_t index) const
{
    return index >= objs.m_AnnotSet.size()? 0: &objs.m_AnnotSet[index];
}


inline
const CTSE_Info::TRangeMap*
CTSE_Info::x_GetRangeMap(const CSeq_id_Handle& id,
                         int annot_type, int feat_type) const
{
    return x_GetRangeMap(id, x_GetTypeIndex(annot_type, feat_type));
}


inline
void CTSE_Info::x_MapAnnotObject(const SAnnotObject_Key& key,
                                 const SAnnotObject_Index& annotRef)
{
    x_MapAnnotObject(x_SetIdObjects(key.m_Handle), key, annotRef);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
