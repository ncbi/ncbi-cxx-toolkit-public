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
*           Eugene Vasilchenko
*
* File Description:
*   Sequence map for the Object Manager. Describes sequence as a set of
*   segments of different types (data, reference, gap or end).
*
*/

#include <objects/objmgr/seq_map_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//  CSeqMap

CSeqMap_Delta_seqs::CSeqMap_Delta_seqs(TObject& obj, CDataSource* source)
    : CSeqMap(source),
      m_Object(&obj),
      m_List(&obj.Set())
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap: cannot store CDelta_seq iterator: too big");
    }
    x_IndexAll(obj.Set());
}

CSeqMap_Delta_seqs::~CSeqMap_Delta_seqs(void)
{
}

void CSeqMap_Delta_seqs::x_Index(TList& seq)
{
    non_const_iterate ( TList, iter, seq ) {
        x_SetSegmentList_I(x_Add(**iter), iter);
    }
}


void CSeqMap_Delta_seqs::x_IndexAll(TList& seq)
{
    x_Index(seq);
    x_AddEnd();
}


void CSeqMap_Delta_seqs::x_IndexUnloadedSubMap(TSeqPos len)
{
    x_SetSegmentList_I(x_AddUnloadedSubMap(len), m_List->end());
}


CSeqMap_Delta_seqs::TList_I
CSeqMap_Delta_seqs::x_FindInsertList_I(size_t index) const
{
    // find insertion point in sequence
    TList_I ins = m_List->end();
    _ASSERT(x_GetSegmentList_I(index) == ins);
    for ( size_t i = index+1; i < x_GetSegmentsCount(); ++i ) {
        TList_I it = x_GetSegmentList_I(i);
        if ( it != ins ) {
            ins = it;
            break;
        }
    }
    return ins;
}


void CSeqMap_Delta_seqs::x_SetSeq_data(size_t index, CSeq_data& data)
{
    // check segment type
    CSegment& segment = x_SetSegment(index);
    if ( segment.m_SegType != eSeqData ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap::x_SetSeq_data: invalid segment type");
    }

    // lock for object modification
    CFastMutexGuard guard(m_SeqMap_Mtx);

    // check for object
    if ( segment.m_RefObject ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap::x_SetSeq_data: CSeq_data already set");
    }

    // do insertion
    // set object
    segment.m_RefObject.Reset(&data);
    // update sequence
    (*x_GetSegmentList_I(index))->SetLiteral().SetSeq_data(data);
}


void CSeqMap_Delta_seqs::x_SetSubSeqMap(size_t index,
                                        CSeqMap_Delta_seqs* subMap)
{
    // check segment type
    CSegment& segment = x_SetSegment(index);
    if ( segment.m_SegType != eSeqSubMap ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap::x_SetSubSeqMap: invalid segment type");
    }

    // lock for object modification
    CFastMutexGuard guard(m_SeqMap_Mtx);

    // check for object
    if ( segment.m_RefObject ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap::x_SetSubSeqMap: submap already set");
    }

    // do insertion
    // set object
    segment.m_RefObject.Reset(subMap);
    // set index
    x_SetSegmentList_I(segment, subMap->m_List->begin());
    // insert sequence
    m_List->splice(x_FindInsertList_I(index), *subMap->m_List);
    // update submap
    subMap->m_List = m_List;
    subMap->m_Object = m_Object;
}


CSeqMap_Seq_locs::CSeqMap_Seq_locs(CSeg_ext& obj, TList& seq,
                                   CDataSource* source)
    : CSeqMap(source),
      m_Object(&obj),
      m_List(&seq)
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap: cannot store CSeq_loc list iterator: too big");
    }
    x_IndexAll();
}


CSeqMap_Seq_locs::CSeqMap_Seq_locs(CSeq_loc_mix& obj, TList& seq,
                                   CDataSource* source)
    : CSeqMap(source),
      m_Object(&obj),
      m_List(&seq)
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap: cannot store CSeq_loc list iterator: too big");
    }
    x_IndexAll();
}


CSeqMap_Seq_locs::CSeqMap_Seq_locs(CSeq_loc_equiv& obj, TList& seq,
                                   CDataSource* source)
    : CSeqMap(source),
      m_Object(&obj),
      m_List(&seq)
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap: cannot store CSeq_loc list iterator: too big");
    }
    x_IndexAll();
}


CSeqMap_Seq_locs::~CSeqMap_Seq_locs(void)
{
}


void CSeqMap_Seq_locs::x_IndexAll(void)
{
    TList& seq = *m_List;
    non_const_iterate ( TList, iter, seq ) {
        x_SetSegmentList_I(x_Add(**iter), iter);
    }
    x_AddEnd();
}


CSeqMap_Seq_intervals::CSeqMap_Seq_intervals(TObject& obj, CDataSource* source)
    : CSeqMap(source),
      m_Object(&obj),
      m_List(&obj.Set())
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap: cannot store CSeq_interval list iterator: "
                     "too big");
    }
    x_IndexAll();
}


CSeqMap_Seq_intervals::CSeqMap_Seq_intervals(TObject& obj,
                                             CSeqMap* parent, size_t index)
    : CSeqMap(parent, index),
      m_Object(&obj),
      m_List(&obj.Set())
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap: cannot store CSeq_interval list iterator: "
                     "too big");
    }
    x_IndexAll();
}


CSeqMap_Seq_intervals::~CSeqMap_Seq_intervals(void)
{
}


void CSeqMap_Seq_intervals::x_IndexAll(void)
{
    TList& seq = *m_List;
    non_const_iterate ( TList, iter, seq ) {
        x_SetSegmentList_I(x_Add(**iter), iter);
    }
    x_AddEnd();
}


CSeqMap_SeqPoss::CSeqMap_SeqPoss(TObject& obj, CDataSource* source)
    : CSeqMap(source),
      m_Object(&obj),
      m_List(&obj.SetPoints())
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap: cannot store TSeqPos list iterator: "
                     "too big");
    }
    x_IndexAll();
}


CSeqMap_SeqPoss::CSeqMap_SeqPoss(TObject& obj, CSeqMap* parent, size_t index)
    : CSeqMap(parent, index),
      m_Object(&obj),
      m_List(&obj.SetPoints())
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        THROW1_TRACE(runtime_error,
                     "CSeqMap: cannot store TSeqPos list iterator: "
                     "too big");
    }
    x_IndexAll();
}


CSeqMap_SeqPoss::~CSeqMap_SeqPoss(void)
{
}


void CSeqMap_SeqPoss::x_IndexAll(void)
{
    TList& seq = *m_List;
    non_const_iterate ( TList, iter, seq ) {
        x_SetSegmentList_I(x_AddPos(*iter), iter);
    }
    x_AddEnd();
}


CSeqMap::CSegment& CSeqMap_SeqPoss::x_AddPos(TSeqPos pos)
{
    return x_AddSegment(eSeqRef, &m_Object->SetId(),
                        pos, 1, m_Object->GetStrand());
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.1  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
*
* ===========================================================================
*/
