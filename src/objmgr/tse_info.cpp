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


#include <ncbi_pch.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/annot_type_index.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/handle_range.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/objmgr_exception.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


SIdAnnotObjs::SIdAnnotObjs(void)
{
}


SIdAnnotObjs::~SIdAnnotObjs(void)
{
}


SIdAnnotObjs::SIdAnnotObjs(const SIdAnnotObjs& _DEBUG_ARG(objs))
{
    _ASSERT(objs.m_AnnotSet.empty());
    _ASSERT(objs.m_SNPSet.empty());
}


////////////////////////////////////////////////////////////////////
//
//  CTSE_Info::
//
//    General information and indexes for top level seq-entries
//


CTSE_Info::CTSE_Info(CSeq_entry& entry,
                     bool dead,
                     const CObject* blob_id)
    : m_DataSource(0),
      m_Dead(dead),
      m_Blob_ID(blob_id)
{
    entry.Parentize();
    x_SetObject(entry);
    x_TSEAttach(*this);
}


CTSE_Info::CTSE_Info(const CTSE_Info& info)
    : TParent(info),
      m_DataSource(0),
      m_Dead(false),
      m_Name(info.m_Name)
{
    x_TSEAttach(*this);
}


CTSE_Info::~CTSE_Info(void)
{
}


void CTSE_Info::SetBlobId(const CObject* blob_id)
{
    m_Blob_ID.Reset(blob_id);
}


void CTSE_Info::SetName(const CAnnotName& name)
{
    m_Name = name;
}


bool CTSE_Info::HasAnnot(const CAnnotName& name) const
{
    _ASSERT(!x_DirtyAnnotIndex());
    return m_NamedAnnotObjs.find(name) != m_NamedAnnotObjs.end();
}


bool CTSE_Info::HasNamedAnnot(const string& name) const
{
    return HasAnnot(CAnnotName(name));
}


bool CTSE_Info::HasUnnamedAnnot(void) const
{
    return HasAnnot(CAnnotName());
}


CConstRef<CSeq_entry> CTSE_Info::GetCompleteTSE(void) const
{
    return GetCompleteSeq_entry();
}


CConstRef<CSeq_entry> CTSE_Info::GetTSECore(void) const
{
    return GetSeq_entryCore();
}


void CTSE_Info::x_DSAttachContents(CDataSource& ds)
{
    _ASSERT(!m_DataSource);

    m_DataSource = &ds;
    TParent::x_DSAttachContents(ds);
    ITERATE ( TBioseqs, it, m_Bioseqs ) {
        ds.x_IndexSeqTSE(it->first, this);
    }
    ds.x_IndexAnnotTSEs(this);
}


void CTSE_Info::x_DSDetachContents(CDataSource& ds)
{
    _ASSERT(m_DataSource == &ds);

    ITERATE ( TBioseqs, it, m_Bioseqs ) {
        ds.x_UnindexSeqTSE(it->first, this);
    }
    ds.x_UnindexAnnotTSEs(this);
    TParent::x_DSDetachContents(ds);
    m_DataSource = 0;
}


void CTSE_Info::x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds)
{
    ds.x_Map(obj, this);
    TParent::x_DSMapObject(obj, ds);
}


void CTSE_Info::x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds)
{
    ds.x_Unmap(obj, this);
    TParent::x_DSUnmapObject(obj, ds);
}


inline
void CTSE_Info::x_IndexSeqTSE(const CSeq_id_Handle& id)
{
    if ( HasDataSource() ) {
        GetDataSource().x_IndexSeqTSE(id, this);
    }
}


inline
void CTSE_Info::x_UnindexSeqTSE(const CSeq_id_Handle& id)
{
    if ( HasDataSource() ) {
        GetDataSource().x_UnindexSeqTSE(id, this);
    }
}


void CTSE_Info::x_IndexAnnotTSE(const CAnnotName& name,
                                const CSeq_id_Handle& id)
{
    TSeqIdToNames::iterator iter = m_SeqIdToNames.lower_bound(id);
    if ( iter == m_SeqIdToNames.end() || iter->first != id ) {
        iter = m_SeqIdToNames.insert(iter,
                                     TSeqIdToNames::value_type(id, TNames()));
        if ( HasDataSource() ) {
            GetDataSource().x_IndexAnnotTSE(id, this);
        }
    }
    _VERIFY(iter->second.insert(name).second);
}


void CTSE_Info::x_UnindexAnnotTSE(const CAnnotName& name,
                                  const CSeq_id_Handle& id)
{
    TSeqIdToNames::iterator iter = m_SeqIdToNames.lower_bound(id);
    _ASSERT(iter != m_SeqIdToNames.end() && iter->first == id);
    _VERIFY(iter->second.erase(name) == 1);
    if ( iter->second.empty() ) {
        m_SeqIdToNames.erase(iter);
        if ( HasDataSource() ) {
            GetDataSource().x_UnindexAnnotTSE(id, this);
        }
    }
}


CConstRef<CBioseq_Info> CTSE_Info::FindBioseq(const CSeq_id_Handle& id) const
{
    CConstRef<CBioseq_Info> ret;
    //CFastMutexGuard guard(m_BioseqsLock);
    TBioseqs::const_iterator it = m_Bioseqs.find(id);
    if ( it != m_Bioseqs.end() ) {
        ret = it->second;
    }
    return ret;
}


bool CTSE_Info::ContainsSeqid(const CSeq_id_Handle& id) const
{
    //CFastMutexGuard guard(m_BioseqsLock);
    return m_Bioseqs.find(id) != m_Bioseqs.end();
}


void CTSE_Info::x_SetBioseqId(const CSeq_id_Handle& key,
                              CBioseq_Info* info)
{
    //CFastMutexGuard guard(m_BioseqsLock);
    pair<TBioseqs::iterator, bool> ins =
        m_Bioseqs.insert(TBioseqs::value_type(key, info));
    if ( ins.second ) {
        // register this TSE in data source as containing the sequence
        x_IndexSeqTSE(key);
    }
    else {
        // No duplicate bioseqs in the same TSE
        NCBI_THROW(CObjMgrException, eAddDataError,
                   " duplicate Bioseq id '"+key.AsString()+"' present in"+
                   "\n  seq1: " + ins.first->second->IdString() +
                   "\n  seq2: " + info->IdString());
    }
}


void CTSE_Info::x_ResetBioseqId(const CSeq_id_Handle& key,
                                CBioseq_Info* _DEBUG_ARG(info))
{
    //CFastMutexGuard guard(m_BioseqsLock);
    TBioseqs::iterator iter = m_Bioseqs.lower_bound(key);
    if ( iter != m_Bioseqs.end() && iter->first == key ) {
        _ASSERT(iter->second == info);
        m_Bioseqs.erase(iter);
        x_UnindexSeqTSE(key);
    }
}


void CTSE_Info::x_SetBioseq_setId(int key,
                                  CBioseq_set_Info* info)
{
    //CFastMutexGuard guard(m_BioseqsLock);
    pair<TBioseq_sets::iterator, bool> ins =
        m_Bioseq_sets.insert(TBioseq_sets::value_type(key, info));
    if ( ins.second ) {
        // everything is fine
    }
    else {
        // No duplicate bioseqs in the same TSE
        NCBI_THROW(CObjMgrException, eAddDataError,
                   " duplicate Bioseq_set id '"+NStr::IntToString(key));
    }
}


void CTSE_Info::x_ResetBioseq_setId(int key,
                                    CBioseq_set_Info* _DEBUG_ARG(info))
{
    //CFastMutexGuard guard(m_BioseqsLock);
    TBioseq_sets::iterator iter = m_Bioseq_sets.lower_bound(key);
    if ( iter != m_Bioseq_sets.end() && iter->first == key ) {
        _ASSERT(iter->second == info);
        m_Bioseq_sets.erase(iter);
    }
}


void CTSE_Info::x_SetDirtyAnnotIndexNoParent(void)
{
    if ( HasDataSource() ) {
        GetDataSource().x_SetDirtyAnnotIndex(*this);
    }
}


void CTSE_Info::x_ResetDirtyAnnotIndexNoParent(void)
{
    if ( HasDataSource() ) {
        GetDataSource().x_ResetDirtyAnnotIndex(*this);
    }
}


void CTSE_Info::UpdateAnnotIndex(void) const
{
    const_cast<CTSE_Info*>(this)->UpdateAnnotIndex();
    _ASSERT(!x_DirtyAnnotIndex());
}


void CTSE_Info::UpdateAnnotIndex(const CTSE_Info_Object& object) const
{
    const_cast<CTSE_Info*>(this)->
        UpdateAnnotIndex(const_cast<CTSE_Info_Object&>(object));
    _ASSERT(!object.x_DirtyAnnotIndex());
}


void CTSE_Info::UpdateAnnotIndex(void)
{
    UpdateAnnotIndex(*this);
}


void CTSE_Info::UpdateAnnotIndex(CTSE_Info_Object& object)
{
    _ASSERT(&object.GetTSE_Info() == this);
    CTSE_Info::TAnnotWriteLockGuard guard(m_AnnotObjsLock);
    NON_CONST_ITERATE ( TChunks, it, m_Chunks ) {
        it->second->x_UpdateAnnotIndex(*this);
    }
    object.x_UpdateAnnotIndex(*this);
    _ASSERT(!object.x_DirtyAnnotIndex());
}


CTSE_Chunk_Info& CTSE_Info::GetChunk(TChunkId chunk_id)
{
    TChunks::iterator iter = m_Chunks.find(chunk_id);
    if ( iter == m_Chunks.end() ) {
        NCBI_THROW(CObjMgrException, eAddDataError,
                   "invalid chunk id: "+NStr::IntToString(chunk_id));
    }
    return *iter->second;
}


CRef<CTSE_Chunk_Info> CTSE_Info::GetNotLoadedChunk(void)
{
    NON_CONST_ITERATE ( TChunks, it, m_Chunks ) {
        if ( it->second->NotLoaded() ) {
            return it->second;
        }
    }
    return CRef<CTSE_Chunk_Info>();
}


CTSE_Info::TAnnotObjs& CTSE_Info::x_SetAnnotObjs(const CAnnotName& name)
{
    TNamedAnnotObjs::iterator iter = m_NamedAnnotObjs.lower_bound(name);
    if ( iter == m_NamedAnnotObjs.end() || iter->first != name ) {
        typedef TNamedAnnotObjs::value_type value_type;
        iter = m_NamedAnnotObjs.insert(iter, value_type(name, TAnnotObjs()));
    }
    return iter->second;
}


void CTSE_Info::x_RemoveAnnotObjs(const CAnnotName& name)
{
    m_NamedAnnotObjs.erase(name);
}


const CTSE_Info::TAnnotObjs*
CTSE_Info::x_GetAnnotObjs(const CAnnotName& name) const
{
    TNamedAnnotObjs::const_iterator iter = m_NamedAnnotObjs.lower_bound(name);
    if ( iter == m_NamedAnnotObjs.end() || iter->first != name ) {
        return 0;
    }
    return &iter->second;
}


const CTSE_Info::TAnnotObjs*
CTSE_Info::x_GetUnnamedAnnotObjs(void) const
{
    TNamedAnnotObjs::const_iterator iter = m_NamedAnnotObjs.begin();
    if ( iter == m_NamedAnnotObjs.end() || iter->first.IsNamed() ) {
        return 0;
    }
    return &iter->second;
}


SIdAnnotObjs& CTSE_Info::x_SetIdObjects(TAnnotObjs& objs,
                                        const CAnnotName& name,
                                        const CSeq_id_Handle& id)
{
    // repeat for more generic types of selector
    TAnnotObjs::iterator it = objs.lower_bound(id);
    if ( it == objs.end() || it->first != id ) {
        // new id
        it = objs.insert(it, TAnnotObjs::value_type(id, SIdAnnotObjs()));
        x_IndexAnnotTSE(name, id);
    }
    _ASSERT(it != objs.end() && it->first == id);
    return it->second;
}


SIdAnnotObjs& CTSE_Info::x_SetIdObjects(const CAnnotName& name,
                                        const CSeq_id_Handle& id)
{
    return x_SetIdObjects(x_SetAnnotObjs(name), name, id);
}


const SIdAnnotObjs* CTSE_Info::x_GetIdObjects(const TAnnotObjs& objs,
                                              const CSeq_id_Handle& idh) const
{
    TAnnotObjs::const_iterator it = objs.lower_bound(idh);
    if ( it == objs.end() || it->first != idh ) {
        return 0;
    }
    return &it->second;
}


const SIdAnnotObjs* CTSE_Info::x_GetIdObjects(const CAnnotName& name,
                                              const CSeq_id_Handle& idh) const
{
    const TAnnotObjs* objs = x_GetAnnotObjs(name);
    if ( !objs ) {
        return 0;
    }
    return x_GetIdObjects(*objs, idh);
}


const SIdAnnotObjs*
CTSE_Info::x_GetUnnamedIdObjects(const CSeq_id_Handle& idh) const
{
    const TAnnotObjs* objs = x_GetUnnamedAnnotObjs();
    if ( !objs ) {
        return 0;
    }
    return x_GetIdObjects(*objs, idh);
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
bool CTSE_Info::x_UnmapAnnotObject(TRangeMap& rangeMap,
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
    if ( key.m_AnnotObject_Info->IsLocs() ) {
        // Locs may contain multiple indexes
        CAnnotObject_Info::TTypeIndexSet idx_set;
        key.m_AnnotObject_Info->GetLocsTypes(idx_set);
        ITERATE(CAnnotObject_Info::TTypeIndexSet, idx_rg, idx_set) {
            for (size_t idx = idx_rg->first; idx < idx_rg->second; ++idx) {
                if ( idx >= objs.m_AnnotSet.size() ) {
                    objs.m_AnnotSet.resize(idx+1);
                }
                x_MapAnnotObject(objs.m_AnnotSet[idx], key, annotRef);
            }
        }
    }
    else {
        size_t index = CAnnotType_Index::GetTypeIndex(key);
        if ( index >= objs.m_AnnotSet.size() ) {
            objs.m_AnnotSet.resize(index+1);
        }
        x_MapAnnotObject(objs.m_AnnotSet[index], key, annotRef);
    }
}


bool CTSE_Info::x_UnmapAnnotObject(SIdAnnotObjs& objs,
                                   const SAnnotObject_Key& key)
{
    size_t index = CAnnotType_Index::GetTypeIndex(key);
    _ASSERT(index < objs.m_AnnotSet.size());
    if ( x_UnmapAnnotObject(objs.m_AnnotSet[index], key) ) {
        while ( objs.m_AnnotSet.back().empty() ) {
            objs.m_AnnotSet.pop_back();
            if ( objs.m_AnnotSet.empty() ) {
                return objs.m_SNPSet.empty();
            }
        }
    }
    return false;
}


void CTSE_Info::x_MapAnnotObject(TAnnotObjs& objs,
                                 const CAnnotName& name,
                                 const SAnnotObject_Key& key,
                                 const SAnnotObject_Index& annotRef)
{
    x_MapAnnotObject(x_SetIdObjects(objs, name, key.m_Handle), key, annotRef);
}


bool CTSE_Info::x_UnmapAnnotObject(TAnnotObjs& objs,
                                   const CAnnotName& name,
                                   const SAnnotObject_Key& key)
{
    TAnnotObjs::iterator it = objs.find(key.m_Handle);
    _ASSERT(it != objs.end() && it->first == key.m_Handle);
    if ( x_UnmapAnnotObject(it->second, key) ) {
        x_UnindexAnnotTSE(name, key.m_Handle);
        objs.erase(it);
        return objs.empty();
    }
    return false;
}


void CTSE_Info::x_MapSNP_Table(const CAnnotName& name,
                               const CSeq_id_Handle& key,
                               const CSeq_annot_SNP_Info& snp_info)
{
    SIdAnnotObjs& objs = x_SetIdObjects(name, key);
    objs.m_SNPSet.push_back(ConstRef(&snp_info));
}


void CTSE_Info::x_UnmapSNP_Table(const CAnnotName& name,
                                 const CSeq_id_Handle& key,
                                 const CSeq_annot_SNP_Info& snp_info)
{
    SIdAnnotObjs& objs = x_SetIdObjects(name, key);
    TSNPSet::iterator iter = find(objs.m_SNPSet.begin(),
                                  objs.m_SNPSet.end(),
                                  ConstRef(&snp_info));
    if ( iter != objs.m_SNPSet.end() ) {
        objs.m_SNPSet.erase(iter);
    }
}


void CTSE_Info::x_MapAnnotObject(TAnnotObjs& index,
                                 const SAnnotObject_Key& key,
                                 const SAnnotObject_Index& annotRef,
                                 SAnnotObjects_Info& infos)
{
    _ASSERT(&index == x_GetAnnotObjs(infos.GetName()));
    infos.AddKey(key);
    x_MapAnnotObject(index, infos.GetName(), key, annotRef);
}


void CTSE_Info::x_MapAnnotObject(const SAnnotObject_Key& key,
                                 const SAnnotObject_Index& annotRef,
                                 SAnnotObjects_Info& infos)
{
    x_MapAnnotObject(x_SetAnnotObjs(infos.GetName()), key, annotRef, infos);
}


void CTSE_Info::x_UnmapAnnotObjects(SAnnotObjects_Info& infos)
{
    TAnnotObjs& index = x_SetAnnotObjs(infos.GetName());

    ITERATE( SAnnotObjects_Info::TObjectKeys, it, infos.GetKeys() ) {
        x_UnmapAnnotObject(index, infos.GetName(), *it);
    }

    if ( index.empty() ) {
        x_RemoveAnnotObjs(infos.GetName());
    }

    infos.Clear();
}


CBioseq_set_Info& CTSE_Info::GetBioseq_set(int id)
{
    TBioseq_sets::iterator iter = m_Bioseq_sets.find(id);
    if ( iter == m_Bioseq_sets.end() ) {
        NCBI_THROW(CObjMgrException, eRegisterError,
                   "cannot find Bioseq-set by local id");
    }
    return *iter->second;
}


CBioseq_Info& CTSE_Info::GetBioseq(int gi)
{
    TBioseqs::iterator iter = m_Bioseqs.find(CSeq_id_Handle::GetGiHandle(gi));
    if ( iter == m_Bioseqs.end() ) {
        NCBI_THROW(CObjMgrException, eRegisterError,
                   "cannot find Bioseq by gi");
    }
    return *iter->second;
}


void CTSE_Info::DebugDump(CDebugDumpContext /*ddc*/, unsigned int /*depth*/) const
{
#if 0
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
#endif
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.46  2004/07/12 16:57:32  vasilche
* Fixed loading of split Seq-descr and Seq-data objects.
* They are loaded correctly now when GetCompleteXxx() method is called.
*
* Revision 1.45  2004/06/07 17:01:17  grichenk
* Implemented referencing through locs annotations
*
* Revision 1.44  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.43  2004/03/24 18:30:30  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.42  2004/03/23 15:14:00  vasilche
* Restored automatic call to Parentize() for Seq-entries in object manager.
*
* Revision 1.41  2004/03/16 15:47:28  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.40  2004/02/19 17:13:42  vasilche
* Removed 'unused argument' warnings.
*
* Revision 1.39  2004/02/04 18:05:39  grichenk
* Added annotation filtering by set of types/subtypes.
* Renamed *Choice to *Type in SAnnotSelector.
*
* Revision 1.38  2004/02/03 19:02:19  vasilche
* Fixed broken 'dirty annot index' state after RemoveEntry().
*
* Revision 1.37  2004/01/22 20:10:41  vasilche
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
* Revision 1.36  2003/11/26 17:56:00  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.35  2003/11/19 22:18:04  grichenk
* All exceptions are now CException-derived. Catch "exception" rather
* than "runtime_error".
*
* Revision 1.34  2003/10/27 16:47:12  vasilche
* Fixed error:
* src/objmgr/data_source.cpp", line 913: Fatal: Assertion failed: (it != tse_map.end() && it->first == id)
*
* Revision 1.33  2003/10/09 20:20:58  vasilche
* Added possibility to include and exclude Seq-annot names to annot iterator.
* Fixed adaptive search. It looked only on selected set of annot names before.
*
* Revision 1.32  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.31  2003/09/30 16:22:04  vasilche
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
* Revision 1.30  2003/09/16 14:21:48  grichenk
* Added feature indexing and searching by subtype.
*
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
