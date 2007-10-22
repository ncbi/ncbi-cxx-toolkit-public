#ifndef ANNOT_OBJECT__HPP
#define ANNOT_OBJECT__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Annotation object wrapper
*
*/

#include <corelib/ncbiobj.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <objmgr/annot_type_selector.hpp>

#include <serial/serialbase.hpp> // for CUnionBuffer

#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;
class CHandleRangeMap;
struct SAnnotTypeSelector;
class CSeq_align;
class CSeq_graph;
class CSeq_feat;
class CSeq_entry;
class CSeq_entry_Info;
class CSeq_annot;
class CSeq_annot_Info;
class CTSE_Info;
class CTSE_Chunk_Info;

struct SAnnotObject_Key
{
    bool IsSingle(void) const
        {
            return m_Handle;
        }
    void SetMultiple(size_t from, size_t to)
        {
            m_Handle.Reset();
            m_Range.SetFrom(from);
            m_Range.SetToOpen(to);
        }
    size_t begin(void) const
        {
            _ASSERT(!IsSingle());
            return m_Range.GetFrom();
        }
    size_t end(void) const
        {
            _ASSERT(!IsSingle());
            return m_Range.GetToOpen();
        }
    void Reset()
        {
            m_Handle.Reset();
            m_Range.SetFrom(0);
            m_Range.SetToOpen(0);
        }
    CSeq_id_Handle          m_Handle;
    CRange<TSeqPos>         m_Range;
};

// General Seq-annot object
class NCBI_XOBJMGR_EXPORT CAnnotObject_Info
{
public:
    typedef CSeq_annot::C_Data           C_Data;
    typedef C_Data::TFtable              TFtable;
    typedef C_Data::TAlign               TAlign;
    typedef C_Data::TGraph               TGraph;
    typedef C_Data::TLocs                TLocs;
    typedef C_Data::E_Choice             TAnnotType;
    typedef CSeqFeatData::E_Choice       TFeatType;
    typedef CSeqFeatData::ESubtype       TFeatSubtype;
    typedef Int4                         TIndex;

    CAnnotObject_Info(void);
    CAnnotObject_Info(CSeq_annot_Info& annot, TIndex index,
                      TFtable::iterator iter);
    CAnnotObject_Info(CSeq_annot_Info& annot, TIndex index,
                      TAlign::iterator iter);
    CAnnotObject_Info(CSeq_annot_Info& annot, TIndex index,
                      TGraph::iterator iter);
    CAnnotObject_Info(CSeq_annot_Info& annot, TIndex index,
                      TLocs::iterator iter);
    CAnnotObject_Info(CSeq_annot_Info& annot, TIndex index,
                      TFtable& cont, const CSeq_feat& obj);
    CAnnotObject_Info(CSeq_annot_Info& annot, TIndex index,
                      TAlign& cont, const CSeq_align& obj);
    CAnnotObject_Info(CSeq_annot_Info& annot, TIndex index,
                      TGraph& cont, const CSeq_graph& obj);
    CAnnotObject_Info(CSeq_annot_Info& annot, TIndex index,
                      TLocs& cont, const CSeq_loc& obj);
    CAnnotObject_Info(CSeq_annot_Info& annot, TIndex index,
                      const SAnnotTypeSelector& type);
    CAnnotObject_Info(CTSE_Chunk_Info& chunk_info,
                      const SAnnotTypeSelector& sel);

#ifdef NCBI_NON_POD_TYPE_STL_ITERATORS
    ~CAnnotObject_Info();
    CAnnotObject_Info(const CAnnotObject_Info& info);
    CAnnotObject_Info& operator=(const CAnnotObject_Info& info);
#endif

    // state check
    bool IsEmpty(void) const;
    bool IsRemoved(void) const; // same as empty
    bool IsRegular(void) const;
    bool IsChunkStub(void) const;

    // reset to empty state
    void Reset(void);

    // Get Seq-annot, containing the element
    const CSeq_annot_Info& GetSeq_annot_Info(void) const;
    CSeq_annot_Info& GetSeq_annot_Info(void);

    // Get Seq-entry, containing the annotation
    const CSeq_entry_Info& GetSeq_entry_Info(void) const;

    // Get top level Seq-entry, containing the annotation
    const CTSE_Info& GetTSE_Info(void) const;
    CTSE_Info& GetTSE_Info(void);

    // Get CDataSource object
    CDataSource& GetDataSource(void) const;

    // Get index of this annotation within CSeq_annot_Info
    TIndex GetAnnotIndex(void) const;

    const SAnnotTypeSelector& GetTypeSelector(void) const;
    TAnnotType Which(void) const;
    TAnnotType GetAnnotType(void) const;
    TFeatType GetFeatType(void) const;
    TFeatSubtype GetFeatSubtype(void) const;

    CConstRef<CObject> GetObject(void) const;
    const CObject* GetObjectPointer(void) const;

    bool IsFeat(void) const;
    const CSeq_feat& GetFeat(void) const;
    const CSeq_feat* GetFeatFast(void) const; // unchecked & unsafe

    bool IsAlign(void) const;
    const CSeq_align& GetAlign(void) const;
    const CSeq_align* GetAlignFast(void) const;

    bool IsGraph(void) const;
    const CSeq_graph& GetGraph(void) const;
    const CSeq_graph* GetGraphFast(void) const; // unchecked & unsafe

    typedef pair<size_t, size_t> TIndexRange;
    typedef vector<TIndexRange> TTypeIndexSet;

    bool IsLocs(void) const;
    const CSeq_loc& GetLocs(void) const;
    void GetLocsTypes(TTypeIndexSet& idx_set) const;

    void GetMaps(vector<CHandleRangeMap>& hrmaps) const;

    // split support
    const CTSE_Chunk_Info& GetChunk_Info(void) const;

    void x_SetObject(const CSeq_feat& new_obj);
    void x_SetObject(const CSeq_align& new_obj);
    void x_SetObject(const CSeq_graph& new_obj);

    const TFtable::iterator& x_GetFeatIter(void) const;
    const TAlign::iterator& x_GetAlignIter(void) const;
    const TGraph::iterator& x_GetGraphIter(void) const;
    const TLocs::iterator& x_GetLocsIter(void) const;

    static void x_ProcessAlign(vector<CHandleRangeMap>& hrmaps,
                               const CSeq_align& align);
    static void x_ProcessFeat(vector<CHandleRangeMap>& hrmaps,
                              const CSeq_feat& feat);
    static void x_ProcessGraph(vector<CHandleRangeMap>& hrmaps,
                               const CSeq_graph& graph);

    bool HasSingleKey(void) const
        {
            return m_Key.IsSingle();
        }
    const SAnnotObject_Key& GetKey(void) const
        {
            _ASSERT(m_Key.IsSingle());
            return m_Key;
        }
    size_t GetKeysBegin(void) const
        {
            return m_Key.begin();
        }
    size_t GetKeysEnd(void) const
        {
            return m_Key.end();
        }
    void SetKey(const SAnnotObject_Key& key)
        {
            _ASSERT(key.IsSingle());
            m_Key = key;
        }
    void SetKeys(size_t begin, size_t end)
        {
            m_Key.SetMultiple(begin, end);
        }
    void ResetKey(void)
        {
            m_Key.Reset();
        }

private:
    friend class CSeq_annot_Info;

    // Constructors used by CAnnotTypes_CI only to create fake annotations
    // for sequence segments. The annot object points to the seq-annot
    // containing the original annotation object.

    void x_Locs_AddFeatSubtype(int ftype,
                               int subtype,
                               TTypeIndexSet& idx_set) const;

    // Special values for m_ObjectIndex
    // regular indexes start from 0 so all special values are negative
    enum {
        eEmpty          = -1,
        eChunkStub      = -2
    };

    // Possible states:
    // 0. empty
    //   all fields are null
    //   m_ObjectIndex == eEmpty
    //   m_Iter.m_RawPtr == 0
    // A. regular annotation (feat, align, graph):
    //   m_Seq_annot_Info points to containing Seq-annot
    //   m_Type contains type of the annotation
    //   m_ObjectIndex contains index of CAnnotObject_Info within CSeq_annot_Info
    //   m_ObjectIndex >= 0
    //   m_Iter.m_(Feat|Align|Graph) contains iterator in CSeq_annot's container
    //   m_Iter.m_RawPtr != 0
    // B. Seq-locs type of annotation
    //   m_Seq_annot_Info points to containing Seq-annot
    //   m_Type == e_Locs
    //   m_ObjectIndex contains index of CAnnotObject_Info within CSeq_annot_Info
    //   m_ObjectIndex >= 0
    //   m_Iter.m_Locs contains iterator in CSeq_annot's container
    //   m_Iter.m_RawPtr != 0
    // C. Split chunk annotation info:
    //   m_Seq_annot_Info == 0
    //   m_Type contains type of split annotations
    //   m_ObjectIndex == eChunkStub
    //   m_Iter.m_RawPtr == 0
    // D. Removed regular annotation:
    //   same as empty
    //   m_ObjectIndex == eEmpty
    //   m_Iter.m_RawPtr == 0

    CSeq_annot_Info*             m_Seq_annot_Info; // owner Seq-annot
    union {
        const void*                        m_RawPtr;
        CUnionBuffer<TFtable::iterator>    m_Feat;
        CUnionBuffer<TAlign::iterator>     m_Align;
        CUnionBuffer<TGraph::iterator>     m_Graph;
        CUnionBuffer<TLocs::iterator>      m_Locs;
        CTSE_Chunk_Info*                   m_Chunk;
    }                            m_Iter;
    TIndex                       m_ObjectIndex;
    SAnnotTypeSelector           m_Type;     // annot type
    SAnnotObject_Key             m_Key;      // single key or range of keys
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CAnnotObject_Info::CAnnotObject_Info(void)
    : m_Seq_annot_Info(0),
      m_ObjectIndex(eEmpty)
{
    m_Iter.m_RawPtr = 0;
}


inline
const SAnnotTypeSelector& CAnnotObject_Info::GetTypeSelector(void) const
{
    return m_Type;
}


inline
CAnnotObject_Info::TAnnotType CAnnotObject_Info::Which(void) const
{
    return GetTypeSelector().GetAnnotType();
}


inline
CAnnotObject_Info::TAnnotType CAnnotObject_Info::GetAnnotType(void) const
{
    return GetTypeSelector().GetAnnotType();
}


inline
CAnnotObject_Info::TFeatType CAnnotObject_Info::GetFeatType(void) const
{
    return GetTypeSelector().GetFeatType();
}


inline
CAnnotObject_Info::TFeatSubtype CAnnotObject_Info::GetFeatSubtype(void) const
{
    return GetTypeSelector().GetFeatSubtype();
}


inline
bool CAnnotObject_Info::IsEmpty(void) const
{
    return m_ObjectIndex == eEmpty;
}


inline
bool CAnnotObject_Info::IsRemoved(void) const
{
    return m_ObjectIndex == eEmpty;
}


inline
bool CAnnotObject_Info::IsRegular(void) const
{
    return m_ObjectIndex >= 0 && m_Iter.m_RawPtr;
}


inline
bool CAnnotObject_Info::IsChunkStub(void) const
{
    return m_ObjectIndex == eChunkStub;
}


inline
CAnnotObject_Info::TIndex CAnnotObject_Info::GetAnnotIndex(void) const
{
    return m_ObjectIndex;
}


inline
bool CAnnotObject_Info::IsFeat(void) const
{
    return Which() == C_Data::e_Ftable;
}


inline
bool CAnnotObject_Info::IsAlign(void) const
{
    return Which() == C_Data::e_Align;
}


inline
bool CAnnotObject_Info::IsGraph(void) const
{
    return Which() == C_Data::e_Graph;
}


inline
bool CAnnotObject_Info::IsLocs(void) const
{
    return Which() == C_Data::e_Locs;
}


inline
const CSeq_annot::C_Data::TFtable::iterator&
CAnnotObject_Info::x_GetFeatIter(void) const
{
    _ASSERT(IsFeat() && IsRegular() && m_Iter.m_RawPtr);
    return *m_Iter.m_Feat;
}


inline
const CSeq_annot::C_Data::TAlign::iterator&
CAnnotObject_Info::x_GetAlignIter(void) const
{
    _ASSERT(IsAlign() && IsRegular() && m_Iter.m_RawPtr);
    return *m_Iter.m_Align;
}


inline
const CSeq_annot::C_Data::TGraph::iterator&
CAnnotObject_Info::x_GetGraphIter(void) const
{
    _ASSERT(IsGraph() && IsRegular() && m_Iter.m_RawPtr);
    return *m_Iter.m_Graph;
}


inline
const CSeq_annot::C_Data::TLocs::iterator&
CAnnotObject_Info::x_GetLocsIter(void) const
{
    _ASSERT(IsLocs() && IsRegular() && m_Iter.m_RawPtr);
    return *m_Iter.m_Locs;
}


inline
const CSeq_feat* CAnnotObject_Info::GetFeatFast(void) const
{
    return *x_GetFeatIter();
}


inline
const CSeq_align* CAnnotObject_Info::GetAlignFast(void) const
{
    return *x_GetAlignIter();
}


inline
const CSeq_graph* CAnnotObject_Info::GetGraphFast(void) const
{
    return *x_GetGraphIter();
}


inline
const CSeq_feat& CAnnotObject_Info::GetFeat(void) const
{
    return *GetFeatFast();
}


inline
const CSeq_align& CAnnotObject_Info::GetAlign(void) const
{
    return *GetAlignFast();
}


inline
const CSeq_graph& CAnnotObject_Info::GetGraph(void) const
{
    return *GetGraphFast();
}


inline
const CSeq_loc& CAnnotObject_Info::GetLocs(void) const
{
    return **x_GetLocsIter();
}


inline
const CTSE_Chunk_Info& CAnnotObject_Info::GetChunk_Info(void) const
{
    _ASSERT(IsChunkStub() && m_Iter.m_Chunk && !m_Seq_annot_Info);
    return *m_Iter.m_Chunk;
}


inline
const CSeq_annot_Info& CAnnotObject_Info::GetSeq_annot_Info(void) const
{
    _ASSERT(m_Seq_annot_Info);
    return *m_Seq_annot_Info;
}


inline
CSeq_annot_Info& CAnnotObject_Info::GetSeq_annot_Info(void)
{
    _ASSERT(m_Seq_annot_Info);
    return *m_Seq_annot_Info;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ANNOT_OBJECT__HPP
