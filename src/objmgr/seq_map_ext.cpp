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

#include <ncbi_pch.hpp>
#include <objmgr/seq_map_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//  CSeqMap

CSeqMap_Delta_seqs::CSeqMap_Delta_seqs(const TObject& obj)
    : CSeqMap(),
      m_Object(&obj),
      m_List(&obj.Get())
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        NCBI_THROW(CSeqMapException, eIteratorTooBig,
                   "Cannot store CDelta_seq iterator: too big");
    }
    x_IndexAll(obj.Get());
}

CSeqMap_Delta_seqs::~CSeqMap_Delta_seqs(void)
{
}

void CSeqMap_Delta_seqs::x_Index(const TList& seq)
{
    ITERATE ( TList, iter, seq ) {
        x_SetSegmentList_I(x_Add(**iter), iter);
    }
}


void CSeqMap_Delta_seqs::x_IndexAll(const TList& seq)
{
    x_AddEnd();
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
        NCBI_THROW(CSeqMapException, eSegmentTypeError,
                   "Invalid segment type");
    }

    // lock for object modification
    CFastMutexGuard guard(m_SeqMap_Mtx);

    // check for object
    if ( bool(segment.m_RefObject) && segment.m_ObjType == eSeqData ) {
        NCBI_THROW(CSeqMapException, eDataError, "object already set");
    }

    // do insertion
    // set object
    segment.m_ObjType = eSeqData;
    segment.m_RefObject.Reset(&data);
    // update sequence
    const_cast<CDelta_seq&>(**x_GetSegmentList_I(index)).SetLiteral().SetSeq_data(data);
}


void CSeqMap_Delta_seqs::x_SetSubSeqMap(size_t /*index*/,
                                        CSeqMap_Delta_seqs* /*subMap*/)
{
/*
    // check segment type
    CSegment& segment = x_SetSegment(index);
    if ( segment.m_SegType != eSeqSubMap ) {
        NCBI_THROW(CSeqMapException, eSegmentTypeError,
                   "Invalid segment type");
    }

    // lock for object modification
    CFastMutexGuard guard(m_SeqMap_Mtx);

    // check for object
    if ( segment.m_RefObject ) {
        NCBI_THROW(CSeqMapException, eDataError,
                   "Submap already set");
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
*/
}


////////////////////////////////////////////////////////////////////
//  CSeqMap_Seq_data

CSeqMap_Seq_data::CSeqMap_Seq_data(const TObject& obj)
    : CSeqMap(),
      m_Object(&obj)
{
    _ASSERT(obj.IsSetLength());
    x_AddEnd();
    if ( obj.IsSetSeq_data() ) {
        x_Add(obj.GetSeq_data(), obj.GetLength());
    }
    else {
        // split Seq-data
        x_AddGap(obj.GetLength());
    }
    x_AddEnd();
}

CSeqMap_Seq_data::~CSeqMap_Seq_data(void)
{
}

void CSeqMap_Seq_data::x_SetSeq_data(size_t index, CSeq_data& data)
{
    // check segment type
    CSegment& segment = x_SetSegment(index);
    if ( segment.m_SegType != eSeqData ) {
        NCBI_THROW(CSeqMapException, eSegmentTypeError,
                   "Invalid segment type");
    }

    x_SetObject(segment, data);

    // update sequence
    const_cast<CSeq_inst&>(*m_Object).SetSeq_data(data);
}


CSeqMap_Seq_locs::CSeqMap_Seq_locs(const CSeg_ext& obj, const TList& seq)
    : CSeqMap(),
      m_Object(&obj),
      m_List(&seq)
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        NCBI_THROW(CSeqMapException, eIteratorTooBig,
                   "Cannot store CSeq_loc list iterator: too big");
    }
    x_IndexAll();
}


CSeqMap_Seq_locs::CSeqMap_Seq_locs(const CSeq_loc_mix& obj, const TList& seq)
    : CSeqMap(),
      m_Object(&obj),
      m_List(&seq)
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        NCBI_THROW(CSeqMapException, eIteratorTooBig,
                   "Cannot store CSeq_loc list iterator: too big");
    }
    x_IndexAll();
}


CSeqMap_Seq_locs::CSeqMap_Seq_locs(const CSeq_loc_equiv& obj, const TList& seq)
    : CSeqMap(),
      m_Object(&obj),
      m_List(&seq)
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        NCBI_THROW(CSeqMapException, eIteratorTooBig,
                   "Cannot store CSeq_loc list iterator: too big");
    }
    x_IndexAll();
}


CSeqMap_Seq_locs::~CSeqMap_Seq_locs(void)
{
}


void CSeqMap_Seq_locs::x_IndexAll(void)
{
    x_AddEnd();
    const TList& seq = *m_List;
    ITERATE ( TList, iter, seq ) {
        x_SetSegmentList_I(x_Add(**iter), iter);
    }
    x_AddEnd();
}


CSeqMap_Seq_intervals::CSeqMap_Seq_intervals(const TObject& obj)
    : CSeqMap(),
      m_Object(&obj),
      m_List(&obj.Get())
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        NCBI_THROW(CSeqMapException, eIteratorTooBig,
                   "Cannot store CSeq_interval list iterator: too big");
    }
    x_IndexAll();
}


CSeqMap_Seq_intervals::CSeqMap_Seq_intervals(const TObject& obj,
                                             CSeqMap* parent, size_t index)
    : CSeqMap(parent, index),
      m_Object(&obj),
      m_List(&obj.Get())
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        NCBI_THROW(CSeqMapException, eIteratorTooBig,
                   "Cannot store CSeq_interval list iterator: too big");
    }
    x_IndexAll();
}


CSeqMap_Seq_intervals::~CSeqMap_Seq_intervals(void)
{
}


void CSeqMap_Seq_intervals::x_IndexAll(void)
{
    x_AddEnd();
    const TList& seq = *m_List;
    ITERATE ( TList, iter, seq ) {
        x_SetSegmentList_I(x_Add(**iter), iter);
    }
    x_AddEnd();
}


CSeqMap_SeqPoss::CSeqMap_SeqPoss(const TObject& obj)
    : CSeqMap(),
      m_Object(&obj),
      m_List(&obj.GetPoints())
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        NCBI_THROW(CSeqMapException, eIteratorTooBig,
                   "Cannot store TSeqPos list iterator: too big");
    }
    x_IndexAll();
}


CSeqMap_SeqPoss::CSeqMap_SeqPoss(const TObject& obj, CSeqMap* parent, size_t index)
    : CSeqMap(parent, index),
      m_Object(&obj),
      m_List(&obj.GetPoints())
{
    if ( sizeof(TList_I) > sizeof(CSegment::TList0_I) ) {
        NCBI_THROW(CSeqMapException, eIteratorTooBig,
                   "Cannot store TSeqPos list iterator: too big");
    }
    x_IndexAll();
}


CSeqMap_SeqPoss::~CSeqMap_SeqPoss(void)
{
}


inline
CSeqMap::CSegment& CSeqMap_SeqPoss::x_AddPos(const CSeq_id* id, TSeqPos pos, ENa_strand strand)
{
    return x_AddSegment(eSeqRef, id, pos, 1, strand);
}


void CSeqMap_SeqPoss::x_IndexAll(void)
{
    x_AddEnd();
    const TList& seq = *m_List;
    const CSeq_id* id = &m_Object->GetId();
    ENa_strand strand =
        m_Object->IsSetStrand()? m_Object->GetStrand(): eNa_strand_unknown;
    ITERATE ( TList, iter, seq ) {
        x_SetSegmentList_I(x_AddPos(id, *iter, strand), iter);
    }
    x_AddEnd();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2004/07/12 16:53:28  vasilche
* Fixed loading of split Seq-data when sequence is not delta.
*
* Revision 1.12  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.11  2004/02/19 17:19:35  vasilche
* Removed 'unused argument' warning.
*
* Revision 1.10  2003/11/12 16:53:17  grichenk
* Modified CSeqMap to work with const objects (CBioseq, CSeq_loc etc.)
*
* Revision 1.9  2003/09/30 16:22:04  vasilche
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
* Revision 1.8  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.7  2003/06/02 16:06:38  dicuccio
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
* Revision 1.6  2003/05/21 16:03:08  vasilche
* Fixed access to uninitialized optional members.
* Added initialization of mandatory members.
*
* Revision 1.5  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.4  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.3  2003/02/05 20:59:12  vasilche
* Added eSeqEnd segment at the beginning of seq map.
* Added flags to CSeqMap_CI to stop on data, gap, or references.
*
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
