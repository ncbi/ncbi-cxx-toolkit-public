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
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/bioseq_base_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/annot_type_index.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>

#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Packed_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/seqres/Seq_graph.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//
//  CAnnotObject_Info::
//


CAnnotObject_Info::CAnnotObject_Info(CSeq_annot_Info& annot,
                                     TIndex index,
                                     TFtable::iterator iter)
    : m_Seq_annot_Info(&annot),
      m_ObjectIndex(index),
      m_Type((*iter)->GetData().GetSubtype())
{
#ifdef NCBI_NON_POD_TYPE_STL_ITERATORS
    m_Iter.m_Feat.Construct();
#endif
    *m_Iter.m_Feat = iter;
    _ASSERT(IsRegular());
    _ASSERT(m_Iter.m_RawPtr != 0);
}


CAnnotObject_Info::CAnnotObject_Info(CSeq_annot_Info& annot,
                                     TIndex index,
                                     TAlign::iterator iter)
    : m_Seq_annot_Info(&annot),
      m_ObjectIndex(index),
      m_Type(C_Data::e_Align)
{
#ifdef NCBI_NON_POD_TYPE_STL_ITERATORS
    m_Iter.m_Align.Construct();
#endif
    *m_Iter.m_Align = iter;
    _ASSERT(IsRegular());
    _ASSERT(m_Iter.m_RawPtr != 0);
}


CAnnotObject_Info::CAnnotObject_Info(CSeq_annot_Info& annot,
                                     TIndex index,
                                     TGraph::iterator iter)
    : m_Seq_annot_Info(&annot),
      m_ObjectIndex(index),
      m_Type(C_Data::e_Graph)
{
#ifdef NCBI_NON_POD_TYPE_STL_ITERATORS
    m_Iter.m_Graph.Construct();
#endif
    *m_Iter.m_Graph = iter;
    _ASSERT(IsRegular());
    _ASSERT(m_Iter.m_RawPtr != 0);
}


CAnnotObject_Info::CAnnotObject_Info(CSeq_annot_Info& annot,
                                     TIndex index,
                                     TLocs::iterator iter)
    : m_Seq_annot_Info(&annot),
      m_ObjectIndex(index),
      m_Type(C_Data::e_Locs)
{
#ifdef NCBI_NON_POD_TYPE_STL_ITERATORS
    m_Iter.m_Locs.Construct();
#endif
    *m_Iter.m_Locs = iter;
    _ASSERT(IsRegular());
    _ASSERT(m_Iter.m_RawPtr != 0);
}


CAnnotObject_Info::CAnnotObject_Info(CSeq_annot_Info& annot,
                                     TIndex index,
                                     TFtable& cont,
                                     const CSeq_feat& obj)
    : m_Seq_annot_Info(&annot),
      m_ObjectIndex(index),
      m_Type(obj.GetData().GetSubtype())
{
#ifdef NCBI_NON_POD_TYPE_STL_ITERATORS
    m_Iter.m_Feat.Construct();
#endif
    *m_Iter.m_Feat = cont.insert(cont.end(),
                                 Ref(const_cast<CSeq_feat*>(&obj)));
    _ASSERT(IsRegular());
    _ASSERT(m_Iter.m_RawPtr != 0);
}


CAnnotObject_Info::CAnnotObject_Info(CSeq_annot_Info& annot,
                                     TIndex index,
                                     TAlign& cont,
                                     const CSeq_align& obj)
    : m_Seq_annot_Info(&annot),
      m_ObjectIndex(index),
      m_Type(C_Data::e_Align)
{
#ifdef NCBI_NON_POD_TYPE_STL_ITERATORS
    m_Iter.m_Align.Construct();
#endif
    *m_Iter.m_Align = cont.insert(cont.end(),
                                  Ref(const_cast<CSeq_align*>(&obj)));
    _ASSERT(IsRegular());
    _ASSERT(m_Iter.m_RawPtr != 0);
}


CAnnotObject_Info::CAnnotObject_Info(CSeq_annot_Info& annot,
                                     TIndex index,
                                     TGraph& cont,
                                     const CSeq_graph& obj)
    : m_Seq_annot_Info(&annot),
      m_ObjectIndex(index),
      m_Type(C_Data::e_Graph)
{
#ifdef NCBI_NON_POD_TYPE_STL_ITERATORS
    m_Iter.m_Graph.Construct();
#endif
    *m_Iter.m_Graph = cont.insert(cont.end(),
                                  Ref(const_cast<CSeq_graph*>(&obj)));
    _ASSERT(IsRegular());
    _ASSERT(m_Iter.m_RawPtr != 0);
}


CAnnotObject_Info::CAnnotObject_Info(CSeq_annot_Info& annot,
                                     TIndex index,
                                     TLocs& cont,
                                     const CSeq_loc& obj)
    : m_Seq_annot_Info(&annot),
      m_ObjectIndex(index),
      m_Type(C_Data::e_Locs)
{
#ifdef NCBI_NON_POD_TYPE_STL_ITERATORS
    m_Iter.m_Locs.Construct();
#endif
    *m_Iter.m_Locs = cont.insert(cont.end(),
                                 Ref(const_cast<CSeq_loc*>(&obj)));
    _ASSERT(IsRegular());
    _ASSERT(m_Iter.m_RawPtr != 0);
}


CAnnotObject_Info::CAnnotObject_Info(CTSE_Chunk_Info& chunk_info,
                                     const SAnnotTypeSelector& sel)
    : m_Seq_annot_Info(0),
      m_ObjectIndex(eChunkStub),
      m_Type(sel)
{
    m_Iter.m_Chunk = &chunk_info;
    _ASSERT(IsChunkStub());
    _ASSERT(m_Iter.m_RawPtr != 0);
}


#ifdef NCBI_NON_POD_TYPE_STL_ITERATORS

CAnnotObject_Info::~CAnnotObject_Info()
{
    Reset();
}


CAnnotObject_Info::CAnnotObject_Info(const CAnnotObject_Info& info)
    : m_Seq_annot_Info(info.m_Seq_annot_Info),
      m_ObjectIndex(info.m_ObjectIndex),
      m_Type(info.m_Type)
{
    if ( IsRegular() ) {
        if ( IsFeat() ) {
            m_Iter.m_Feat.Construct();
            *m_Iter.m_Feat = *info.m_Iter.m_Feat;
        }
        else if ( IsAlign() ) {
            m_Iter.m_Align.Construct();
            *m_Iter.m_Align = *info.m_Iter.m_Align;
        }
        else if ( IsGraph() ) {
            m_Iter.m_Graph.Construct();
            *m_Iter.m_Graph = *info.m_Iter.m_Graph;
        }
        else if ( IsLocs() ) {
            m_Iter.m_Locs.Construct();
            *m_Iter.m_Locs = *info.m_Iter.m_Locs;
        }
    }
    else {
        m_Iter.m_RawPtr = info.m_Iter.m_RawPtr;
    }
}


CAnnotObject_Info& CAnnotObject_Info::operator=(const CAnnotObject_Info& info)
{
    if ( this != &info ) {
        Reset();
        m_Seq_annot_Info = info.m_Seq_annot_Info;
        m_ObjectIndex = info.m_ObjectIndex;
        m_Type = info.m_Type;
        if ( IsRegular() ) {
            if ( IsFeat() ) {
                m_Iter.m_Feat.Construct();
                *m_Iter.m_Feat = *info.m_Iter.m_Feat;
            }
            else if ( IsAlign() ) {
                m_Iter.m_Align.Construct();
                *m_Iter.m_Align = *info.m_Iter.m_Align;
            }
            else if ( IsGraph() ) {
                m_Iter.m_Graph.Construct();
                *m_Iter.m_Graph = *info.m_Iter.m_Graph;
            }
            else if ( IsLocs() ) {
                m_Iter.m_Locs.Construct();
                *m_Iter.m_Locs = *info.m_Iter.m_Locs;
            }
        }
        else {
            m_Iter.m_RawPtr = info.m_Iter.m_RawPtr;
        }
    }
    return *this;
}

#endif


void CAnnotObject_Info::Reset(void)
{
#ifdef NCBI_NON_POD_TYPE_STL_ITERATORS
    if ( IsRegular() ) {
        if ( IsFeat() ) {
            m_Iter.m_Feat.Destruct();
        }
        else if ( IsAlign() ) {
            m_Iter.m_Align.Destruct();
        }
        else if ( IsGraph() ) {
            m_Iter.m_Graph.Destruct();
        }
        else if ( IsLocs() ) {
            m_Iter.m_Locs.Destruct();
        }
    }
#endif
    m_Type.SetAnnotType(C_Data::e_not_set);
    m_Iter.m_RawPtr = 0;
    m_ObjectIndex = eEmpty;
    m_Seq_annot_Info = 0;
}


CConstRef<CObject> CAnnotObject_Info::GetObject(void) const
{
    return ConstRef(GetObjectPointer());
}


const CObject* CAnnotObject_Info::GetObjectPointer(void) const
{
    switch ( Which() ) {
    case C_Data::e_Ftable:
        return GetFeatFast();
    case C_Data::e_Graph:
        return GetGraphFast();
    case C_Data::e_Align:
        return &GetAlign();
    case C_Data::e_Locs:
        return &GetLocs();
    default:
        return 0;
    }
}


void CAnnotObject_Info::GetMaps(vector<CHandleRangeMap>& hrmaps) const
{
    _ASSERT(IsRegular());
    switch ( Which() ) {
    case C_Data::e_Ftable:
        x_ProcessFeat(hrmaps, *GetFeatFast());
        break;
    case C_Data::e_Graph:
        x_ProcessGraph(hrmaps, *GetGraphFast());
        break;
    case C_Data::e_Align:
    {
        const CSeq_align& align = GetAlign();
        // TODO: separate alignment locations
        hrmaps.clear();
        x_ProcessAlign(hrmaps, align);
        break;
    }
    case C_Data::e_Locs:
    {
        _ASSERT(!IsRemoved());
        // Index by location in region descriptor, not by referenced one
        const CSeq_annot& annot = *GetSeq_annot_Info().GetCompleteSeq_annot();
        if ( !annot.IsSetDesc() ) {
            break;
        }
        CConstRef<CSeq_loc> region;
        ITERATE(CSeq_annot::TDesc::Tdata, desc_it, annot.GetDesc().Get()) {
            if ( (*desc_it)->IsRegion() ) {
                region.Reset(&(*desc_it)->GetRegion());
                break;
            }
        }
        if ( region ) {
            hrmaps.resize(1);
            hrmaps[0].clear();
            hrmaps[0].AddLocation(*region);
        }
        break;
    }
    default:
        break;
    }
}

/* static */
void CAnnotObject_Info::x_ProcessFeat(vector<CHandleRangeMap>& hrmaps,
                                      const CSeq_feat& feat) 
{
    hrmaps.resize(feat.IsSetProduct()? 2: 1);
    hrmaps[0].clear();
    hrmaps[0].AddLocation(feat.GetLocation());
    if ( feat.IsSetProduct() ) {
        hrmaps[1].clear();
        hrmaps[1].AddLocation(feat.GetProduct());
    }
}
/* static */
void CAnnotObject_Info::x_ProcessGraph(vector<CHandleRangeMap>& hrmaps,
                                      const CSeq_graph& graph) 
{
    hrmaps.resize(1);
    hrmaps[0].clear();
    hrmaps[0].AddLocation(graph.GetLoc());
}

const CSeq_entry_Info& CAnnotObject_Info::GetSeq_entry_Info(void) const
{
    return GetSeq_annot_Info().GetParentSeq_entry_Info();
}


const CTSE_Info& CAnnotObject_Info::GetTSE_Info(void) const
{
    return GetSeq_annot_Info().GetTSE_Info();
}


CTSE_Info& CAnnotObject_Info::GetTSE_Info(void)
{
    return GetSeq_annot_Info().GetTSE_Info();
}


CDataSource& CAnnotObject_Info::GetDataSource(void) const
{
    return GetSeq_annot_Info().GetDataSource();
}


const string kAnnotTypePrefix = "Seq-annot.data.";

void CAnnotObject_Info::GetLocsTypes(TTypeIndexSet& idx_set) const
{
    const CSeq_annot& annot = *GetSeq_annot_Info().GetCompleteSeq_annot();
    _ASSERT(annot.IsSetDesc());
    ITERATE(CSeq_annot::TDesc::Tdata, desc_it, annot.GetDesc().Get()) {
        if ( !(*desc_it)->IsUser() ) {
            continue;
        }
        const CUser_object& obj = (*desc_it)->GetUser();
        if ( !obj.GetType().IsStr() ) {
            continue;
        }
        string type = obj.GetType().GetStr();
        if (type.substr(0, kAnnotTypePrefix.size()) != kAnnotTypePrefix) {
            continue;
        }
        type.erase(0, kAnnotTypePrefix.size());
        if (type == "align") {
            idx_set.push_back(CAnnotType_Index::GetAnnotTypeRange(
                C_Data::e_Align));
        }
        else if (type == "graph") {
            idx_set.push_back(CAnnotType_Index::GetAnnotTypeRange(
                C_Data::e_Graph));
        }
        else if (type == "ftable") {
            if ( obj.GetData().size() == 0 ) {
                // Feature type/subtype not set
                idx_set.push_back(CAnnotType_Index::GetAnnotTypeRange(
                    C_Data::e_Ftable));
                continue;
            }
            // Parse feature types and subtypes
            ITERATE(CUser_object::TData, data_it, obj.GetData()) {
                const CUser_field& field = **data_it;
                if ( !field.GetLabel().IsId() ) {
                    continue;
                }
                int ftype = field.GetLabel().GetId();
                switch (field.GetData().Which()) {
                case CUser_field::C_Data::e_Int:
                    x_Locs_AddFeatSubtype(ftype,
                        field.GetData().GetInt(), idx_set);
                    break;
                case CUser_field::C_Data::e_Ints:
                    {
                        ITERATE(CUser_field::C_Data::TInts, it,
                            field.GetData().GetInts()) {
                            x_Locs_AddFeatSubtype(ftype, *it, idx_set);
                        }
                        break;
                    }
                default:
                    break;
                }
            }
        }
    }
}


void CAnnotObject_Info::x_Locs_AddFeatSubtype(int ftype,
                                              int subtype,
                                              TTypeIndexSet& idx_set) const
{
    if (subtype != CSeqFeatData::eSubtype_any) {
        size_t idx =
            CAnnotType_Index::GetSubtypeIndex(CSeqFeatData::ESubtype(subtype));
        idx_set.push_back(TIndexRange(idx, idx+1));
    }
    else {
        idx_set.push_back(
            CAnnotType_Index::GetFeatTypeRange(CSeqFeatData::E_Choice(ftype)));
    }
}

/* static */
void CAnnotObject_Info::x_ProcessAlign(vector<CHandleRangeMap>& hrmaps,
                                       const CSeq_align& align)
{
    //### Check the implementation.
    switch ( align.GetSegs().Which() ) {
    case CSeq_align::C_Segs::e_not_set:
        {
            break;
        }
    case CSeq_align::C_Segs::e_Dendiag:
        {
            const CSeq_align::C_Segs::TDendiag& dendiag =
                align.GetSegs().GetDendiag();
            ITERATE ( CSeq_align::C_Segs::TDendiag, it, dendiag ) {
                const CDense_diag& diag = **it;
                int dim = diag.GetDim();
                if (dim != (int)diag.GetIds().size()) {
                    ERR_POST(Warning << "Invalid 'ids' size in dendiag");
                    dim = min(dim, (int)diag.GetIds().size());
                }
                if (dim != (int)diag.GetStarts().size()) {
                    ERR_POST(Warning << "Invalid 'starts' size in dendiag");
                    dim = min(dim, (int)diag.GetStarts().size());
                }
                if (diag.IsSetStrands()
                    && dim != (int)diag.GetStrands().size()) {
                    ERR_POST(Warning << "Invalid 'strands' size in dendiag");
                    dim = min(dim, (int)diag.GetStrands().size());
                }
                if ((int)hrmaps.size() < dim) {
                    hrmaps.resize(dim);
                }
                TSeqPos len = (*it)->GetLen();
                for (int row = 0; row < dim; ++row) {
                    CSeq_loc loc;
                    loc.SetInt().SetId().Assign(*(*it)->GetIds()[row]);
                    loc.SetInt().SetFrom((*it)->GetStarts()[row]);
                    loc.SetInt().SetTo((*it)->GetStarts()[row] + len - 1);
                    if ( (*it)->IsSetStrands() ) {
                        loc.SetInt().SetStrand((*it)->GetStrands()[row]);
                    }
                    hrmaps[row].AddLocation(loc);
                }
            }
            break;
        }
    case CSeq_align::C_Segs::e_Denseg:
        {
            const CSeq_align::C_Segs::TDenseg& denseg =
                align.GetSegs().GetDenseg();
            int dim    = denseg.GetDim();
            int numseg = denseg.GetNumseg();
            // claimed dimension may not be accurate :-/
            if (numseg != (int)denseg.GetLens().size()) {
                ERR_POST(Warning << "Invalid 'lens' size in denseg");
                numseg = min(numseg, (int)denseg.GetLens().size());
            }
            if (dim != (int)denseg.GetIds().size()) {
                ERR_POST(Warning << "Invalid 'ids' size in denseg");
                dim = min(dim, (int)denseg.GetIds().size());
            }
            if (dim*numseg != (int)denseg.GetStarts().size()) {
                ERR_POST(Warning << "Invalid 'starts' size in denseg");
                dim = min(dim*numseg, (int)denseg.GetStarts().size()) / numseg;
            }
            if (denseg.IsSetStrands()
                && dim*numseg != (int)denseg.GetStrands().size()) {
                ERR_POST(Warning << "Invalid 'strands' size in denseg");
                dim = min(dim*numseg, (int)denseg.GetStrands().size()) / numseg;
            }
            if ((int)hrmaps.size() < dim) {
                hrmaps.resize(dim);
            }
            for (int seg = 0;  seg < numseg;  seg++) {
                for (int row = 0;  row < dim;  row++) {
                    if (denseg.GetStarts()[seg*dim + row] < 0 ) {
                        continue;
                    }
                    CSeq_loc loc;
                    loc.SetInt().SetId().Assign(*denseg.GetIds()[row]);
                    loc.SetInt().SetFrom(denseg.GetStarts()[seg*dim + row]);
                    loc.SetInt().SetTo(denseg.GetStarts()[seg*dim + row]
                        + denseg.GetLens()[seg] - 1);
                    if ( denseg.IsSetStrands() ) {
                        loc.SetInt().SetStrand(denseg.GetStrands()[seg*dim + row]);
                    }
                    hrmaps[row].AddLocation(loc);
                }
            }
            break;
        }
    case CSeq_align::C_Segs::e_Std:
        {
            const CSeq_align::C_Segs::TStd& std =
                align.GetSegs().GetStd();
            ITERATE ( CSeq_align::C_Segs::TStd, it, std ) {
                if ((int)hrmaps.size() < (*it)->GetDim()) {
                    hrmaps.resize((*it)->GetDim());
                }
                ITERATE ( CStd_seg::TLoc, it_loc, (*it)->GetLoc() ) {
                    CSeq_loc_CI row_it(**it_loc);
                    for (int row = 0; row_it; ++row_it, ++row) {
                        if (row >= (int)hrmaps.size()) {
                            hrmaps.resize(row + 1);
                        }
                        CSeq_loc loc;
                        loc.SetInt().SetId().Assign(row_it.GetSeq_id());
                        loc.SetInt().SetFrom(row_it.GetRange().GetFrom());
                        loc.SetInt().SetTo(row_it.GetRange().GetTo());
                        if ( row_it.GetStrand() != eNa_strand_unknown ) {
                            loc.SetInt().SetStrand(row_it.GetStrand());
                        }
                        hrmaps[row].AddLocation(loc);
                    }
                }
            }
            break;
        }
    case CSeq_align::C_Segs::e_Packed:
        {
            const CSeq_align::C_Segs::TPacked& packed =
                align.GetSegs().GetPacked();
            int dim    = packed.GetDim();
            int numseg = packed.GetNumseg();
            // claimed dimension may not be accurate :-/
            if (dim * numseg > (int)packed.GetStarts().size()) {
                dim = packed.GetStarts().size() / numseg;
            }
            if (dim * numseg > (int)packed.GetPresent().size()) {
                dim = packed.GetPresent().size() / numseg;
            }
            if (dim > (int)packed.GetLens().size()) {
                dim = packed.GetLens().size();
            }
            if ((int)hrmaps.size() < dim) {
                hrmaps.resize(dim);
            }
            for (int seg = 0;  seg < numseg;  seg++) {
                for (int row = 0;  row < dim;  row++) {
                    if ( packed.GetPresent()[seg*dim + row] ) {
                        CSeq_loc loc;
                        loc.SetInt().SetId().Assign(*packed.GetIds()[row]);
                        loc.SetInt().SetFrom(packed.GetStarts()[seg*dim + row]);
                        loc.SetInt().SetTo(packed.GetStarts()[seg*dim + row]
                            + packed.GetLens()[seg] - 1);
                        if ( packed.IsSetStrands() ) {
                            loc.SetInt().SetStrand(packed.GetStrands()[seg*dim + row]);
                        }
                        hrmaps[row].AddLocation(loc);
                    }
                }
            }
            break;
        }
    case CSeq_align::C_Segs::e_Disc:
        {
            const CSeq_align::C_Segs::TDisc& disc =
                align.GetSegs().GetDisc();
            ITERATE ( CSeq_align_set::Tdata, it, disc.Get() ) {
                x_ProcessAlign(hrmaps, **it);
            }
            break;
        }
    default:
        {
            LOG_POST(Warning << "Unknown type of Seq-align: "<<
                     align.GetSegs().Which());
            break;
        }
    }
}


void CAnnotObject_Info::x_SetObject(const CSeq_feat& new_obj)
{
    x_GetFeatIter()->Reset(&const_cast<CSeq_feat&>(new_obj));
    m_Type.SetFeatSubtype(new_obj.GetData().GetSubtype());
}


void CAnnotObject_Info::x_SetObject(const CSeq_align& new_obj)
{
    x_GetAlignIter()->Reset(&const_cast<CSeq_align&>(new_obj));
    m_Type.SetAnnotType(C_Data::e_Align);
}


void CAnnotObject_Info::x_SetObject(const CSeq_graph& new_obj)
{
    x_GetGraphIter()->Reset(&const_cast<CSeq_graph&>(new_obj));
    m_Type.SetAnnotType(C_Data::e_Graph);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.50  2006/10/25 18:28:22  jcherry
* Removed unfounded requirement that number of scores in Dense-seg
* equals number of segments
*
* Revision 1.49  2006/09/26 18:05:48  vasilche
* Replace compilation warning with runtime warning.
*
* Revision 1.48  2006/05/05 15:05:19  vasilche
* Implemented forgotten methods GetObject*().
*
* Revision 1.47  2006/03/17 18:11:28  vasilche
* Renamed NCBI_NON_POD_STL_ITERATORS -> NCBI_NON_POD_TYPE_STL_ITERATORS.
*
* Revision 1.46  2006/03/16 21:42:14  vasilche
* Always construct STL iterators on MSVC 2005.
*
* Revision 1.45  2006/01/25 18:59:04  didenko
* Redisgned bio objects edit facility
*
* Revision 1.44  2005/09/20 15:45:36  vasilche
* Feature editing API.
* Annotation handles remember annotations by index.
*
* Revision 1.43  2005/08/23 17:02:38  vasilche
* Used SAnnotTypeSelector for storing annotation type in CAnnotObject_Info.
* Moved multi id flags from CAnnotObject_Info to SAnnotObject_Index.
*
* Revision 1.42  2005/02/16 15:18:58  grichenk
* Ignore e_Locs annotations with unknown format
*
* Revision 1.41  2004/06/23 19:51:56  vasilche
* Fixed duplication of discontiguous alignments.
*
* Revision 1.40  2004/06/07 17:01:17  grichenk
* Implemented referencing through locs annotations
*
* Revision 1.39  2004/05/26 14:29:20  grichenk
* Redesigned CSeq_align_Mapper: preserve non-mapping intervals,
* fixed strands handling, improved performance.
*
* Revision 1.38  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.37  2004/04/01 20:33:58  grichenk
* One more m_MultiId initialization
*
* Revision 1.36  2004/04/01 20:18:12  grichenk
* Added initialization of m_MultiId member.
*
* Revision 1.35  2004/03/31 20:43:29  grichenk
* Fixed mapping of seq-locs containing both master sequence
* and its segments.
*
* Revision 1.34  2004/03/26 19:42:04  vasilche
* Fixed premature deletion of SNP annot info object.
* Removed obsolete references to chunk info.
*
* Revision 1.33  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.32  2004/02/04 18:05:38  grichenk
* Added annotation filtering by set of types/subtypes.
* Renamed *Choice to *Type in SAnnotSelector.
*
* Revision 1.31  2004/01/23 16:14:47  grichenk
* Implemented alignment mapping
*
* Revision 1.30  2004/01/22 20:10:40  vasilche
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
* Revision 1.29  2003/11/26 17:55:56  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.28  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.27  2003/09/30 16:22:02  vasilche
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
* Revision 1.26  2003/08/27 21:24:51  vasilche
* Reordered member initializers.
*
* Revision 1.25  2003/08/27 14:29:52  vasilche
* Reduce object allocations in feature iterator.
*
* Revision 1.24  2003/08/26 21:28:47  grichenk
* Added seq-align verification
*
* Revision 1.23  2003/07/18 16:58:23  grichenk
* Fixed alignment coordinates
*
* Revision 1.22  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.21  2003/06/02 16:06:37  dicuccio
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
* Revision 1.20  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.19  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.18  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.17  2003/02/13 14:34:34  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.16  2003/02/04 18:53:36  dicuccio
* Include file clean-up
*
* Revision 1.15  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.14  2002/09/13 15:20:30  dicuccio
* Fix memory leak (static deleted before termination).
*
* Revision 1.13  2002/09/11 16:08:26  dicuccio
* Fixed memory leak in x_PrepareAlign().
*
* Revision 1.12  2002/09/03 17:45:45  ucko
* Avoid overrunning alignment data when the claimed dimensions are too high.
*
* Revision 1.11  2002/07/25 15:01:51  grichenk
* Replaced non-const GetXXX() with SetXXX()
*
* Revision 1.10  2002/07/08 20:51:00  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.9  2002/07/01 15:31:57  grichenk
* Fixed 'enumeration value e_not_set...' warning
*
* Revision 1.8  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.7  2002/05/24 14:57:12  grichenk
* SerialAssign<>() -> CSerialObject::Assign()
*
* Revision 1.6  2002/05/03 21:28:08  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.5  2002/04/05 21:26:19  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.4  2002/02/21 19:27:04  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.3  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:56  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:16  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
