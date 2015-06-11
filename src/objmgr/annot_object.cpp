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
#include <objmgr/error_codes.hpp>

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
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/seqalign/Sparse_seg.hpp>
#include <objects/seqalign/Sparse_align.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/seqres/Seq_graph.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>


#define NCBI_USE_ERRCODE_X   ObjMgr_AnnotObject

BEGIN_NCBI_SCOPE

NCBI_DEFINE_ERR_SUBCODE_X(12);

BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//
//  CAnnotObject_Info::
//


CAnnotObject_Info::CAnnotObject_Info(CSeq_annot_Info& annot,
                                     TIndex index,
                                     const SAnnotTypeSelector& type)
    : m_Seq_annot_Info(&annot),
      m_ObjectIndex(index),
      m_Type(type)
{
    m_Iter.m_RawPtr = 0;
}


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
    if ( info.IsRegular() ) {
        if ( info.IsFeat() ) {
            m_Iter.m_Feat.Construct();
            *m_Iter.m_Feat = *info.m_Iter.m_Feat;
            _ASSERT(IsFeat());
        }
        else if ( info.IsAlign() ) {
            m_Iter.m_Align.Construct();
            *m_Iter.m_Align = *info.m_Iter.m_Align;
            _ASSERT(IsAlign());
        }
        else if ( info.IsGraph() ) {
            m_Iter.m_Graph.Construct();
            *m_Iter.m_Graph = *info.m_Iter.m_Graph;
            _ASSERT(IsGraph());
        }
        else if ( info.IsLocs() ) {
            m_Iter.m_Locs.Construct();
            *m_Iter.m_Locs = *info.m_Iter.m_Locs;
            _ASSERT(IsLocs());
        }
        _ASSERT(IsRegular());
    }
    else {
        m_Iter.m_RawPtr = info.m_Iter.m_RawPtr;
        _ASSERT(!IsRegular());
    }
}


CAnnotObject_Info& CAnnotObject_Info::operator=(const CAnnotObject_Info& info)
{
    if ( this != &info ) {
        Reset();
        m_Seq_annot_Info = info.m_Seq_annot_Info;
        m_ObjectIndex = info.m_ObjectIndex;
        m_Type = info.m_Type;
        if ( info.IsRegular() ) {
            if ( info.IsFeat() ) {
                m_Iter.m_Feat.Construct();
                *m_Iter.m_Feat = *info.m_Iter.m_Feat;
                _ASSERT(IsFeat());
            }
            else if ( info.IsAlign() ) {
                m_Iter.m_Align.Construct();
                *m_Iter.m_Align = *info.m_Iter.m_Align;
                _ASSERT(IsAlign());
            }
            else if ( info.IsGraph() ) {
                m_Iter.m_Graph.Construct();
                *m_Iter.m_Graph = *info.m_Iter.m_Graph;
                _ASSERT(IsGraph());
            }
            else if ( info.IsLocs() ) {
                m_Iter.m_Locs.Construct();
                *m_Iter.m_Locs = *info.m_Iter.m_Locs;
                _ASSERT(IsLocs());
            }
            _ASSERT(IsRegular());
        }
        else {
            m_Iter.m_RawPtr = info.m_Iter.m_RawPtr;
            _ASSERT(!IsRegular());
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


void CAnnotObject_Info::GetMaps(vector<CHandleRangeMap>& hrmaps,
                                const CMasterSeqSegments* master) const
{
    _ASSERT(IsRegular());
    switch ( Which() ) {
    case C_Data::e_Ftable:
        x_ProcessFeat(hrmaps, *GetFeatFast(), master);
        break;
    case C_Data::e_Graph:
        x_ProcessGraph(hrmaps, *GetGraphFast(), master);
        break;
    case C_Data::e_Align:
    {
        const CSeq_align& align = GetAlign();
        // TODO: separate alignment locations
        hrmaps.clear();
        x_ProcessAlign(hrmaps, align, master);
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
            hrmaps[0].SetMasterSeq(master);
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
                                      const CSeq_feat& feat,
                                      const CMasterSeqSegments* master) 
{
    hrmaps.resize(feat.IsSetProduct()? 2: 1);
    hrmaps[0].clear();
    hrmaps[0].SetMasterSeq(master);
    CHandleRangeMap::ETransSplicing mode = CHandleRangeMap::eNoTransSplicing;
    if ( feat.IsSetExcept_text() &&
         feat.GetExcept_text().find("trans-splicing") != NPOS ) {
        mode = CHandleRangeMap::eTransSplicing;
    }
    hrmaps[0].AddLocation(feat.GetLocation(), mode);
    if ( feat.IsSetProduct() ) {
        hrmaps[1].clear();
        hrmaps[1].SetMasterSeq(master);
        hrmaps[1].AddLocation(feat.GetProduct(), mode);
    }
}
/* static */
void CAnnotObject_Info::x_ProcessGraph(vector<CHandleRangeMap>& hrmaps,
                                       const CSeq_graph& graph,
                                       const CMasterSeqSegments* master) 
{
    hrmaps.resize(1);
    hrmaps[0].clear();
    hrmaps[0].SetMasterSeq(master);
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


const CTempString kAnnotTypePrefix = "Seq-annot.data.";

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
        CTempString type = obj.GetType().GetStr();
        if (type.substr(0, kAnnotTypePrefix.size()) != kAnnotTypePrefix) {
            continue;
        }
        type = type.substr(kAnnotTypePrefix.size());
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
                                       const CSeq_align& align,
                                       const CMasterSeqSegments* master)
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
                    ERR_POST_X(1, Warning << "Invalid 'ids' size in dendiag");
                    dim = min(dim, (int)diag.GetIds().size());
                }
                if (dim != (int)diag.GetStarts().size()) {
                    ERR_POST_X(2, Warning << "Invalid 'starts' size in dendiag");
                    dim = min(dim, (int)diag.GetStarts().size());
                }
                if (diag.IsSetStrands()
                    && dim != (int)diag.GetStrands().size()) {
                    ERR_POST_X(3, Warning << "Invalid 'strands' size in dendiag");
                    dim = min(dim, (int)diag.GetStrands().size());
                }
                if ((int)hrmaps.size() < dim) {
                    hrmaps.resize(dim);
                }
                TSeqPos len = (*it)->GetLen();
                for (int row = 0; row < dim; ++row) {
                    const CSeq_id& id = *(*it)->GetIds()[row];
                    TSeqPos from = (*it)->GetStarts()[row];
                    TSeqPos to = from + len - 1;
                    ENa_strand strand = eNa_strand_unknown;
                    if ( (*it)->IsSetStrands() ) {
                        strand = (*it)->GetStrands()[row];
                    }
                    hrmaps[row].SetMasterSeq(master);
                    hrmaps[row].AddRange(id, from, to, strand);
                }
            }
            break;
        }
    case CSeq_align::C_Segs::e_Denseg:
        {
            const CSeq_align::C_Segs::TDenseg& denseg =
                align.GetSegs().GetDenseg();
            size_t dim    = size_t(denseg.GetDim());
            size_t numseg = size_t(denseg.GetNumseg());
            // claimed dimension may not be accurate :-/
            if (numseg != denseg.GetLens().size()) {
                ERR_POST_X(4, Warning << "Invalid 'lens' size in denseg");
                numseg = min(numseg, denseg.GetLens().size());
            }
            if (dim != denseg.GetIds().size()) {
                ERR_POST_X(5, Warning << "Invalid 'ids' size in denseg");
                dim = min(dim, denseg.GetIds().size());
            }
            if (dim*numseg != denseg.GetStarts().size()) {
                ERR_POST_X(6, Warning << "Invalid 'starts' size in denseg");
                dim = min(dim*numseg, denseg.GetStarts().size()) / numseg;
            }
            if (denseg.IsSetStrands()
                && dim*numseg != denseg.GetStrands().size()) {
                ERR_POST_X(7, Warning << "Invalid 'strands' size in denseg");
                dim = min(dim*numseg, denseg.GetStrands().size()) / numseg;
            }
            if (hrmaps.size() < dim) {
                hrmaps.resize(dim);
            }
            for (size_t seg = 0;  seg < numseg;  seg++) {
                for (size_t row = 0;  row < dim;  row++) {
                    if (denseg.GetStarts()[seg*dim + row] < 0 ) {
                        continue;
                    }
                    const CSeq_id& id = *denseg.GetIds()[row];
                    TSeqPos from = denseg.GetStarts()[seg*dim + row];
                    TSeqPos to = from + denseg.GetLens()[seg] - 1;
                    ENa_strand strand = eNa_strand_unknown;
                    if ( denseg.IsSetStrands() ) {
                        strand = denseg.GetStrands()[seg*dim + row];
                    }
                    hrmaps[row].SetMasterSeq(master);
                    hrmaps[row].AddRange(id, from, to, strand);
                }
            }
            break;
        }
    case CSeq_align::C_Segs::e_Std:
        {
            const CSeq_align::C_Segs::TStd& std =
                align.GetSegs().GetStd();
            ITERATE ( CSeq_align::C_Segs::TStd, it, std ) {
                size_t dim = size_t((*it)->GetDim());
                if (hrmaps.size() < dim) {
                    hrmaps.resize((*it)->GetDim());
                }
                ITERATE ( CStd_seg::TLoc, it_loc, (*it)->GetLoc() ) {
                    CSeq_loc_CI row_it(**it_loc);
                    for (size_t row = 0; row_it; ++row_it, ++row) {
                        if (row >= hrmaps.size()) {
                            hrmaps.resize(row + 1);
                        }
                        const CSeq_id& id = row_it.GetSeq_id();
                        TSeqPos from = row_it.GetRange().GetFrom();
                        TSeqPos to = row_it.GetRange().GetTo();
                        ENa_strand strand = row_it.GetStrand();
                        hrmaps[row].SetMasterSeq(master);
                        hrmaps[row].AddRange(id, from, to, strand);
                    }
                }
            }
            break;
        }
    case CSeq_align::C_Segs::e_Packed:
        {
            const CSeq_align::C_Segs::TPacked& packed =
                align.GetSegs().GetPacked();
            size_t dim    = size_t(packed.GetDim());
            size_t numseg = size_t(packed.GetNumseg());
            // claimed dimension may not be accurate :-/
            if (dim * numseg > packed.GetStarts().size()) {
                dim = packed.GetStarts().size() / numseg;
            }
            if (dim * numseg > packed.GetPresent().size()) {
                dim = packed.GetPresent().size() / numseg;
            }
            if (dim > packed.GetLens().size()) {
                dim = packed.GetLens().size();
            }
            if (hrmaps.size() < dim) {
                hrmaps.resize(dim);
            }
            for (size_t seg = 0;  seg < numseg;  seg++) {
                for (size_t row = 0;  row < dim;  row++) {
                    if ( packed.GetPresent()[seg*dim + row] ) {
                        hrmaps[row].SetMasterSeq(master);
                        const CSeq_id& id = *packed.GetIds()[row];
                        TSeqPos from = packed.GetStarts()[seg*dim + row];
                        TSeqPos to = from + packed.GetLens()[seg] - 1;
                        ENa_strand strand = eNa_strand_unknown;
                        if ( packed.IsSetStrands() ) {
                            strand = packed.GetStrands()[seg*dim + row];
                        }
                        hrmaps[row].AddRange(id, from, to, strand);
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
                x_ProcessAlign(hrmaps, **it, master);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Spliced:
        {
            const CSeq_align::C_Segs::TSpliced& spliced =
                align.GetSegs().GetSpliced();
            const CSeq_id* gen_id = spliced.IsSetGenomic_id() ?
                &spliced.GetGenomic_id() : 0;
            const CSeq_id* prod_id = spliced.IsSetProduct_id() ?
                &spliced.GetProduct_id() : 0;
            hrmaps.resize(2);
            ITERATE ( CSpliced_seg::TExons, it, spliced.GetExons() ) {
                const CSpliced_exon& ex = **it;
                const CSeq_id* ex_gen_id = ex.IsSetGenomic_id() ?
                    &ex.GetGenomic_id() : gen_id;
                if ( ex_gen_id ) {
                    const CSeq_id& id = *ex_gen_id;
                    TSeqPos from = ex.GetGenomic_start();
                    TSeqPos to = ex.GetGenomic_end();
                    ENa_strand strand = eNa_strand_unknown;
                    if ( ex.IsSetGenomic_strand() ) {
                        strand = ex.GetGenomic_strand();
                    }
                    else if ( spliced.IsSetGenomic_strand() ) {
                        strand = spliced.GetGenomic_strand();
                    }
                    hrmaps[1].SetMasterSeq(master);
                    hrmaps[1].AddRange(id, from, to, strand);
                }
                const CSeq_id* ex_prod_id = ex.IsSetProduct_id() ?
                    &ex.GetProduct_id() : prod_id;
                if ( ex_prod_id ) {
                    const CSeq_id& id = *ex_prod_id;
                    TSeqPos from =
                        (ex.GetProduct_start().IsNucpos() ?
                         ex.GetProduct_start().GetNucpos() :
                         ex.GetProduct_start().GetProtpos().GetAmin());
                    TSeqPos to =
                        (ex.GetProduct_end().IsNucpos() ?
                         ex.GetProduct_end().GetNucpos() :
                         ex.GetProduct_end().GetProtpos().GetAmin());
                    ENa_strand strand = eNa_strand_unknown;
                    if ( ex.IsSetProduct_strand() ) {
                        strand = ex.GetProduct_strand();
                    }
                    else if ( spliced.IsSetProduct_strand() ) {
                        strand = spliced.GetProduct_strand();
                    }
                    hrmaps[0].SetMasterSeq(master);
                    hrmaps[0].AddRange(id, from, to, strand);
                }
            }
            break;
        }
    case CSeq_align::C_Segs::e_Sparse:
        {
            const CSeq_align::C_Segs::TSparse& sparse =
                align.GetSegs().GetSparse();
            // consensus sequence row + one row for each sub-alignment
            size_t dim = sparse.GetRows().size() + 1;
            if (hrmaps.size() < dim) {
                hrmaps.resize(dim);
            }
            size_t row = 0;
            hrmaps[0].SetMasterSeq(master);
            ITERATE ( CSparse_seg::TRows, it, sparse.GetRows() ) {
                const CSparse_align& aln_row = **it;
                size_t numseg = aln_row.GetNumseg();
                if (numseg != aln_row.GetFirst_starts().size()) {
                    ERR_POST_X(9, Warning <<
                        "Invalid size of 'first-starts' in sparse-align");
                    numseg = min(numseg, aln_row.GetFirst_starts().size());
                }
                if (numseg != aln_row.GetSecond_starts().size()) {
                    ERR_POST_X(10, Warning <<
                        "Invalid size of 'second-starts' in sparse-align");
                    numseg = min(numseg, aln_row.GetSecond_starts().size());
                }
                if (numseg != aln_row.GetLens().size()) {
                    ERR_POST_X(11, Warning <<
                        "Invalid size of 'lens' in sparse-align");
                    numseg = min(numseg, aln_row.GetLens().size());
                }
                if (aln_row.IsSetSecond_strands()  &&
                    numseg != aln_row.GetSecond_strands().size()) {
                    ERR_POST_X(12, Warning <<
                        "Invalid size of 'second-strands' in sparse-align");
                    numseg = min(numseg, aln_row.GetSecond_strands().size());
                }

                hrmaps[row+1].SetMasterSeq(master);
                for (size_t seg = 0; seg < numseg; ++seg) {
                    TSeqPos len = aln_row.GetLens()[seg];
                    {
                        const CSeq_id& id = aln_row.GetFirst_id();
                        TSeqPos from = aln_row.GetFirst_starts()[seg];
                        TSeqPos to = from + len - 1;
                        // consensus sequence goes to the first row
                        hrmaps[0].AddRange(id, from, to);
                    }
                    {
                        const CSeq_id& id = aln_row.GetSecond_id();
                        TSeqPos from = aln_row.GetSecond_starts()[seg];
                        TSeqPos to = from + len - 1;
                        ENa_strand strand = eNa_strand_unknown;
                        if ( aln_row.IsSetSecond_strands() ) {
                            strand = aln_row.GetSecond_strands()[row];
                        }
                        hrmaps[row+1].AddRange(id, from, to, strand);
                    }
                }
                row++;
            }
            break;
        }
    default:
        {
            LOG_POST_X(8, Warning << "Unknown type of Seq-align: "<<
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


void CAnnotObject_Info::x_MoveToBack(TFtable& cont)
{
    _ASSERT(IsFeat() && IsRegular() && m_Iter.m_RawPtr);
    TFtable::iterator old_iter = *m_Iter.m_Feat;
    *m_Iter.m_Feat = cont.insert(cont.end(), *old_iter);
    cont.erase(old_iter);
}


END_SCOPE(objects)
END_NCBI_SCOPE
