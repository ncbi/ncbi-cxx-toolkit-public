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


#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/handle_range.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


SAnnotObject_Index::SAnnotObject_Index(void)
    : m_AnnotObject_Info(0),
      m_AnnotLocationIndex(0)
{
}


SAnnotObject_Index::~SAnnotObject_Index(void)
{
}


SAnnotObject_Index::SAnnotObject_Index(const SAnnotObject_Index& ind)
    : m_AnnotObject_Info(ind.m_AnnotObject_Info),
      m_AnnotLocationIndex(ind.m_AnnotLocationIndex),
      m_HandleRange(ind.m_HandleRange)
{
}


SAnnotObject_Index&
SAnnotObject_Index::operator=(const SAnnotObject_Index& ind)
{
    m_AnnotObject_Info = ind.m_AnnotObject_Info;
    m_AnnotLocationIndex = ind.m_AnnotLocationIndex;
    m_HandleRange = ind.m_HandleRange;
    return *this;
}


CTSE_Info::SIdAnnotObjs::SIdAnnotObjs(void)
{
}


CTSE_Info::SIdAnnotObjs::~SIdAnnotObjs(void)
{
}


CTSE_Info::SIdAnnotObjs::SIdAnnotObjs(const SIdAnnotObjs& objs)
    : m_AnnotSet(objs.m_AnnotSet), m_SNPSet(objs.m_SNPSet)
{
}


const CTSE_Info::SIdAnnotObjs&
CTSE_Info::SIdAnnotObjs::operator=(const SIdAnnotObjs& objs)
{
    m_AnnotSet = objs.m_AnnotSet;
    m_SNPSet = objs.m_SNPSet;
    return *this;
}


////////////////////////////////////////////////////////////////////
//
//  CTSE_Info::
//
//    General information and indexes for top level seq-entries
//


CTSE_Info::CTSE_Info(CDataSource* source,
                     CSeq_entry& entry,
                     bool dead,
                     const CObject* blob_id)
    : CSeq_entry_Info(entry), m_DataSource(source), m_Dead(dead),
      m_Blob_ID(blob_id), m_DirtyAnnotIndex(true)
{
    m_Parent = this;
    m_TSE_Info = this;
}

CTSE_Info::~CTSE_Info(void)
{
}


// map annot/feat type into unique integer:
// align -> 0
// graph -> 1
// feat -> 2...
inline
size_t CTSE_Info::x_GetTypeIndex(int annot_type, int feat_type)
{
    // check enum values
    const int kFtable = CSeq_annot::C_Data::e_Ftable;
    const int kMinNonFtable = CSeq_annot::C_Data::e_Align;
    const int kMaxNonFtable = CSeq_annot::C_Data::e_Graph;
    const int kNonFtableCount = kMaxNonFtable - kMinNonFtable + 1;
    const int kShiftNonFtableDown = kMinNonFtable;
    const int kShiftFtableUp = kNonFtableCount;
    
    _ASSERT(CSeq_annot::C_Data::e_Ftable == kFtable);
    _ASSERT(CSeq_annot::C_Data::e_Align >= kMinNonFtable &&
            CSeq_annot::C_Data::e_Align <= kMaxNonFtable);
    _ASSERT(CSeq_annot::C_Data::e_Graph >= kMinNonFtable &&
            CSeq_annot::C_Data::e_Graph <= kMaxNonFtable);
    _ASSERT(annot_type == kFtable ||
            annot_type >= kMinNonFtable && annot_type <= kMaxNonFtable);
    _ASSERT(feat_type >= 0);
    return annot_type == kFtable?
        feat_type + kShiftFtableUp:
        annot_type - kShiftNonFtableDown;
}


inline
size_t CTSE_Info::x_GetTypeIndex(const SAnnotObject_Key& key)
{
    return x_GetTypeIndex(key.m_AnnotObject_Info->GetAnnotType(),
                          key.m_AnnotObject_Info->GetFeatType());
}


pair<size_t, size_t> CTSE_Info::x_GetIndexRange(int annot_type,
                                                int feat_type)
{
    if ( annot_type == CSeq_annot::C_Data::e_not_set ) {
        size_t last = x_GetTypeIndex(CSeq_annot::C_Data::e_Ftable,
                                     CSeqFeatData::e_Biosrc);
        return pair<size_t, size_t>(0, last+1);
    }
    if ( annot_type == CSeq_annot::C_Data::e_Ftable &&
         feat_type == CSeqFeatData::e_not_set ) {
        size_t first = x_GetTypeIndex(CSeq_annot::C_Data::e_Ftable,
                                      CSeqFeatData::e_not_set); // first
        size_t last = x_GetTypeIndex(CSeq_annot::C_Data::e_Ftable,
                                     CSeqFeatData::e_Biosrc);
        return pair<size_t, size_t>(first, last+1);
    }
    size_t index = x_GetTypeIndex(annot_type, feat_type);
    return pair<size_t, size_t>(index, index+1);
}


CTSE_Info::SIdAnnotObjs&
CTSE_Info::x_SetIdObjects(const CSeq_id_Handle& idh)
{
    // repeat for more generic types of selector
    TAnnotObjs::iterator it = m_AnnotObjs.lower_bound(idh);
    if ( it == m_AnnotObjs.end() || it->first != idh ) {
        // new id
        it = m_AnnotObjs.insert(it,
                                TAnnotObjs::value_type(idh, SIdAnnotObjs()));
        GetDataSource().x_AddTSE_ref(idh, this);
    }
    _ASSERT(it != m_AnnotObjs.end() && it->first == idh);
    return it->second;
}


const CTSE_Info::SIdAnnotObjs*
CTSE_Info::x_GetIdObjects(const CSeq_id_Handle& idh) const
{
    TAnnotObjs::const_iterator it = m_AnnotObjs.find(idh);
    return it == m_AnnotObjs.end()? 0: &it->second;
}


const CTSE_Info::TRangeMap*
CTSE_Info::x_GetRangeMap(const CSeq_id_Handle& id, size_t index) const
{
    TAnnotObjs::const_iterator it = m_AnnotObjs.find(id);
    return it == m_AnnotObjs.end()? 0: x_GetRangeMap(it->second, index);
}


inline
void CTSE_Info::x_MapAnnotObject(TRangeMap& rangeMap,
                                 const SAnnotObject_Key& key,
                                 const SAnnotObject_Index& annotRef)
{
    _ASSERT(annotRef.m_AnnotObject_Info == key.m_AnnotObject_Info);
    rangeMap.insert(TRangeMap::value_type(key.m_Range, annotRef));
}


inline
bool CTSE_Info::x_DropAnnotObject(TRangeMap& rangeMap,
                                 const SAnnotObject_Key& key)
{
    for ( TRangeMap::iterator it = rangeMap.find(key.m_Range);
          it && it->first == key.m_Range; ++it ) {
        if ( it->second.m_AnnotObject_Info == key.m_AnnotObject_Info ) {
            rangeMap.erase(it);
            return rangeMap.empty();
        }
    }
    _ASSERT(0);
    return rangeMap.empty();
}


void CTSE_Info::x_MapAnnotObject(SIdAnnotObjs& objs,
                                 const SAnnotObject_Key& key,
                                 const SAnnotObject_Index& annotRef)
{
    size_t index = x_GetTypeIndex(key);
    if ( index >= objs.m_AnnotSet.size() ) {
        objs.m_AnnotSet.resize(index+1);
    }
    x_MapAnnotObject(objs.m_AnnotSet[index], key, annotRef);
}


bool CTSE_Info::x_DropAnnotObject(SIdAnnotObjs& objs,
                                  const SAnnotObject_Key& key)
{
    size_t index = x_GetTypeIndex(key);
    _ASSERT(index < objs.m_AnnotSet.size());
    if ( x_DropAnnotObject(objs.m_AnnotSet[index], key) ) {
        while ( objs.m_AnnotSet.back().empty() ) {
            objs.m_AnnotSet.pop_back();
            if ( objs.m_AnnotSet.empty() ) {
                return objs.m_SNPSet.empty();
            }
        }
    }
    return false;
}


void CTSE_Info::x_DropAnnotObject(const SAnnotObject_Key& key)
{
    // repeat for more generic types of selector
    TAnnotObjs::iterator it = m_AnnotObjs.find(key.m_Handle);
    _ASSERT(it != m_AnnotObjs.end() && it->first == key.m_Handle);
    if ( x_DropAnnotObject(it->second, key) ) {
        GetDataSource().x_DropTSE_ref(key.m_Handle, this);
        m_AnnotObjs.erase(it);
    }
}


void CTSE_Info::AddSNP_annot_Info(CSeq_annot_SNP_Info& snp_info)
{
    CSeq_id id;
    id.SetGi(snp_info.GetGi());
    CSeq_id_Handle idh = GetDataSource().GetSeq_id_Mapper().GetHandle(id);
    x_SetIdObjects(idh).m_SNPSet
        .push_back(CRef<CSeq_annot_SNP_Info>(&snp_info));
}


void CTSE_Info::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CTSE_Info");
    CObject::DebugDump( ddc, depth);

    ddc.Log("m_TSE", m_Seq_entry.GetPointer(),0);
    ddc.Log("m_Dead", m_Dead);
    if (depth == 0) {
        DebugDumpValue(ddc, "m_Bioseqs.size()", m_Bioseqs.size());
        DebugDumpValue(ddc, "m_AnnotObjs.size()",  m_AnnotObjs.size());
    } else {
        unsigned int depth2 = depth-1;
        { //--- m_Bioseqs
            DebugDumpValue(ddc, "m_Bioseqs.type",
                "map<CSeq_id_Handle, CRef<CBioseq_Info>>");
            CDebugDumpContext ddc2(ddc,"m_Bioseqs");
            TBioseqs::const_iterator it;
            for (it=m_Bioseqs.begin(); it!=m_Bioseqs.end(); ++it) {
                string member_name = "m_Bioseqs[ " +
                    (it->first).AsString() +" ]";
                ddc2.Log(member_name, (it->second).GetPointer(),depth2);
            }
        }
        { //--- m_AnnotObjs_ByInt
            DebugDumpValue(ddc, "m_AnnotObjs_ByInt.type",
                "map<CSeq_id_Handle, CRangeMultimap<CRef<CAnnotObject>,"
                "CRange<TSeqPos>::position_type>>");

            CDebugDumpContext ddc2(ddc,"m_AnnotObjs_ByInt");
            TAnnotObjs::const_iterator it;
            for (it=m_AnnotObjs.begin(); it!=m_AnnotObjs.end(); ++it) {
                string member_name = "m_AnnotObjs[ " +
                    (it->first).AsString() +" ]";
                if (depth2 == 0) {
/*
                    member_name += "size()";
                    DebugDumpValue(ddc2, member_name, (it->second).size());
*/
                } else {
/*
                    // CRangeMultimap
                    CDebugDumpContext ddc3(ddc2, member_name);
                    ITERATE( TRangeMap, itrm, it->second ) {
                        // CRange as string
                        string rg;
                        if (itrm->first.Empty()) {
                            rg += "empty";
                        } else if (itrm->first.IsWhole()) {
                            rg += "whole";
                        } else if (itrm->first.IsWholeTo()) {
                            rg += "unknown";
                        } else {
                            rg +=
                                NStr::UIntToString(itrm->first.GetFrom()) +
                                "..." +
                                NStr::UIntToString(itrm->first.GetTo());
                        }
                        string rm_name = member_name + "[ " + rg + " ]";
                        // CAnnotObject
                        ddc3.Log(rm_name, itrm->second, depth2-1);
                    }
*/
                }
            }
        }
    }
    // DebugDumpValue(ddc, "CMutableAtomicCounter::Get()", Get());
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.29  2003/08/14 20:05:19  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.28  2003/07/17 20:07:56  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.27  2003/06/24 14:25:18  vasilche
* Removed obsolete CTSE_Guard class.
* Used separate mutexes for bioseq and annot maps.
*
* Revision 1.26  2003/06/02 16:06:38  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.25  2003/05/20 15:44:38  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.24  2003/04/25 14:23:26  vasilche
* Added explicit constructors, destructor and assignment operator to make it compilable on MSVC DLL.
*
* Revision 1.23  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.22  2003/03/21 19:22:51  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.21  2003/03/12 20:09:34  grichenk
* Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
*
* Revision 1.20  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.19  2003/03/10 16:55:17  vasilche
* Cleaned SAnnotSelector structure.
* Added shortcut when features are limited to one TSE.
*
* Revision 1.18  2003/03/05 20:56:43  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.17  2003/02/25 20:10:40  grichenk
* Reverted to single total-range index for annotations
*
* Revision 1.16  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.15  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.14  2003/02/04 21:46:32  grichenk
* Added map of annotations by intervals (the old one was
* by total ranges)
*
* Revision 1.13  2003/01/29 17:45:03  vasilche
* Annotaions index is split by annotation/feature type.
*
* Revision 1.12  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.11  2002/12/26 20:55:18  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.10  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.9  2002/10/18 19:12:40  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.8  2002/07/10 16:50:33  grichenk
* Fixed bug with duplicate and uninitialized atomic counters
*
* Revision 1.7  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.6  2002/07/01 15:32:30  grichenk
* Fixed 'unused variable depth3' warning
*
* Revision 1.5  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.4  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.3  2002/03/14 18:39:13  gouriano
* added mutex for MT protection
*
* Revision 1.2  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/02/07 21:25:05  grichenk
* Initial revision
*
*
* ===========================================================================
*/
