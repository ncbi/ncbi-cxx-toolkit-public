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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Application for splitting blobs withing ID1 cache
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objmgr/split/blob_splitter_impl.hpp>

#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>

#include <objects/general/Object_id.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seq/seq__.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>

#include <objects/seqsplit/seqsplit__.hpp>

#include <objmgr/split/blob_splitter.hpp>
#include <objmgr/split/object_splitinfo.hpp>
#include <objmgr/split/annot_piece.hpp>
#include <objmgr/split/asn_sizer.hpp>
#include <objmgr/split/chunk_info.hpp>
#include <objmgr/split/place_id.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/annot_type_selector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

template<class C>
inline
C& NonConst(const C& c)
{
    return const_cast<C&>(c);
}


/////////////////////////////////////////////////////////////////////////////
// CBlobSplitterImpl
/////////////////////////////////////////////////////////////////////////////


CBlobSplitterImpl::CBlobSplitterImpl(const SSplitterParams& params)
    : m_Params(params)
{
}


CBlobSplitterImpl::~CBlobSplitterImpl(void)
{
}


void CBlobSplitterImpl::Reset(void)
{
    m_SplitBlob.Reset();
    m_Skeleton.Reset(new CSeq_entry);
    m_NextBioseq_set_Id = 1;
    m_Entries.clear();
    m_Pieces.clear();
    m_Chunks.clear();
}


void CBlobSplitterImpl::MakeID2SObjects(void)
{
    m_Split_Info.Reset(new CID2S_Split_Info);
    ITERATE ( TChunks, it, m_Chunks ) {
        if ( it->first == 0 ) {
            AttachToSkeleton(it->second);
        }
        else {
            MakeID2Chunk(it->first, it->second);
        }
    }
    m_SplitBlob.Reset(*m_Skeleton, *m_Split_Info);
    ITERATE ( TID2Chunks, it, m_ID2_Chunks ) {
        m_SplitBlob.AddChunk(it->first, *it->second);
    }
}


namespace {

    struct SOneSeqAnnots
    {
        typedef set<SAnnotTypeSelector> TTypeSet;
        typedef COneSeqRange TTotalLocation;

        void Add(const SAnnotTypeSelector& type, const COneSeqRange& loc)
            {
                m_TotalType.insert(type);
                m_TotalLocation.Add(loc);
            }

        TTypeSet m_TotalType;
        TTotalLocation m_TotalLocation;
    };


    struct SSplitAnnotInfo
    {
        typedef vector<SAnnotTypeSelector> TTypeSet;
        typedef CSeqsRange TLocation;

        TTypeSet m_TypeSet;
        TLocation m_Location;
    };


    struct SAllAnnots
    {
        typedef map<CSeq_id_Handle, SOneSeqAnnots> TAllAnnots;
        typedef vector<SAnnotTypeSelector> TTypeSet;
        typedef map<TTypeSet, CSeqsRange> TSplitAnnots;

        void Add(const CSeq_annot& annot)
            {
                switch ( annot.GetData().Which() ) {
                case CSeq_annot::C_Data::e_Ftable:
                    Add(annot.GetData().GetFtable());
                    break;
                case CSeq_annot::C_Data::e_Align:
                    Add(annot.GetData().GetAlign());
                    break;
                case CSeq_annot::C_Data::e_Graph:
                    Add(annot.GetData().GetGraph());
                    break;
                default:
                    _ASSERT("bad annot type" && 0);
                }
            }


        void Add(const CSeq_annot::C_Data::TGraph& objs)
            {
                SAnnotTypeSelector type(CSeq_annot::C_Data::e_Graph);
                ITERATE ( CSeq_annot::C_Data::TGraph, it, objs ) {
                    CSeqsRange loc;
                    loc.Add(**it);
                    Add(type, loc);
                }
            }
        void Add(const CSeq_annot::C_Data::TAlign& objs)
            {
                SAnnotTypeSelector type(CSeq_annot::C_Data::e_Align);
                ITERATE ( CSeq_annot::C_Data::TAlign, it, objs ) {
                    CSeqsRange loc;
                    loc.Add(**it);
                    Add(type, loc);
                }
            }
        void Add(const CSeq_annot::C_Data::TFtable& objs)
            {
                ITERATE ( CSeq_annot::C_Data::TFtable, it, objs ) {
                    SAnnotTypeSelector type((*it)->GetData().GetSubtype());
                    CSeqsRange loc;
                    loc.Add(**it);
                    Add(type, loc);
                }
            }

        void Add(const SAnnotTypeSelector& sel, const CSeqsRange& loc)
            {
                ITERATE ( CSeqsRange, it, loc ) {
                    m_AllAnnots[it->first].Add(sel, it->second);
                }
            }

        void SplitInfo(void)
            {
                ITERATE ( TAllAnnots, it, m_AllAnnots ) {
                    TTypeSet type_set;
                    ITERATE ( SOneSeqAnnots::TTypeSet, tit,
                              it->second.m_TotalType) {
                        type_set.push_back(*tit);
                    }
                    m_SplitAnnots[type_set].Add(it->first,
                                                it->second.m_TotalLocation);
                }
            }

        TAllAnnots m_AllAnnots;
        TSplitAnnots m_SplitAnnots;
    };


    typedef set<int> TGiSet;
    typedef set<CSeq_id_Handle> TIdSet;

    template<class Func>
    void ForEachGiRange(const TGiSet& gis, Func func)
    {
        int gi_start = 0, gi_count = 0;
        ITERATE ( TGiSet, it, gis ) {
            if ( gi_count == 0 || *it != gi_start + gi_count ) {
                if ( gi_count > 0 ) {
                    func(gi_start, gi_count);
                }
                gi_start = *it;
                gi_count = 0;
            }
            ++gi_count;
        }
        if ( gi_count > 0 ) {
            func(gi_start, gi_count);
        }
    }

    typedef CBlobSplitterImpl::TRange TRange;
    typedef set<TRange> TRangeSet;
    typedef map<CSeq_id_Handle, TRangeSet> TIntervalSet;


    template<class C>
    inline
    void SetRange(C& obj, const TRange& range)
    {
        obj.SetStart(range.GetFrom());
        obj.SetLength(range.GetLength());
    }


    void AddIntervals(CID2S_Gi_Ints::TInts& ints,
                      const TRangeSet& range_set)
    {
        ITERATE ( TRangeSet, it, range_set ) {
            CRef<CID2S_Interval> add(new CID2S_Interval);
            SetRange(*add, *it);
            ints.push_back(add);
        }
    }


    CRef<CID2S_Seq_loc> MakeLoc(const CSeq_id_Handle& id,
                                const TRangeSet& range_set)
    {
        CRef<CID2S_Seq_loc> loc(new CID2S_Seq_loc);
        if ( id.IsGi() ) {
            int gi = id.GetGi();
            if ( range_set.size() == 1 ) {
                CID2S_Gi_Interval& interval = loc->SetGi_interval();
                interval.SetGi(gi);
                SetRange(interval, *range_set.begin());
            }
            else {
                CID2S_Gi_Ints& seq_ints = loc->SetGi_ints();
                seq_ints.SetGi(gi);
                AddIntervals(seq_ints.SetInts(), range_set);
            }
        }
        else {
            if ( range_set.size() == 1 ) {
                CID2S_Seq_id_Interval& interval = loc->SetSeq_id_interval();
                interval.SetSeq_id(const_cast<CSeq_id&>(*id.GetSeqId()));
                SetRange(interval, *range_set.begin());
            }
            else {
                CID2S_Seq_id_Ints& seq_ints = loc->SetSeq_id_ints();
                seq_ints.SetSeq_id(const_cast<CSeq_id&>(*id.GetSeqId()));
                AddIntervals(seq_ints.SetInts(), range_set);
            }
        }
        return loc;
    }


    CRef<CID2S_Seq_loc> MakeLoc(const CSeq_id_Handle& id)
    {
        CRef<CID2S_Seq_loc> loc(new CID2S_Seq_loc);
        if ( id.IsGi() ) {
            loc->SetWhole_gi(id.GetGi());
        }
        else {
            loc->SetWhole_seq_id(const_cast<CSeq_id&>(*id.GetSeqId()));
        }
        return loc;
    }


    void AddLoc(CID2S_Seq_loc& loc, CRef<CID2S_Seq_loc> add)
    {
        if ( loc.Which() == CID2S_Seq_loc::e_not_set ) {
            loc.Assign(*add);
        }
        else {
            if ( loc.Which() != CID2S_Seq_loc::e_Loc_set &&
                 loc.Which() != CID2S_Seq_loc::e_not_set ) {
                CRef<CID2S_Seq_loc> copy(new CID2S_Seq_loc);
                AddLoc(*copy, Ref(&loc));
                loc.SetLoc_set().push_back(copy);
            }
            loc.SetLoc_set().push_back(add);
        }
    }


    struct FAddGiRangeToSeq_loc
    {
        FAddGiRangeToSeq_loc(CID2S_Seq_loc& loc)
            : m_Loc(loc)
            {
            }

        enum { SEPARATE = 3 };

        void operator()(int start, int count) const
            {
                _ASSERT(count > 0);
                if ( count <= SEPARATE ) {
                    for ( ; count > 0; --count, ++start ) {
                        CRef<CID2S_Seq_loc> add(new CID2S_Seq_loc);
                        add->SetWhole_gi(start);
                        AddLoc(m_Loc, add);
                    }
                }
                else {
                    CRef<CID2S_Seq_loc> add(new CID2S_Seq_loc);
                    add->SetWhole_gi_range().SetStart(start);
                    add->SetWhole_gi_range().SetCount(count);
                    AddLoc(m_Loc, add);
                }
            }
        CID2S_Seq_loc& m_Loc;
    };
    
    
    struct FAddGiRangeToBioseqIds
    {
        FAddGiRangeToBioseqIds(CID2S_Bioseq_Ids& ids)
            : m_Ids(ids)
            {
            }

        enum { SEPARATE = 2 };

        void operator()(int start, int count) const
            {
                _ASSERT(count > 0);
                if ( count <= SEPARATE ) {
                    // up to SEPARATE consequent gis will be encoded separately
                    for ( ; count > 0; --count, ++start ) {
                        CRef<CID2S_Bioseq_Ids::C_E> elem
                            (new CID2S_Bioseq_Ids::C_E);
                        elem->SetGi(start);
                        m_Ids.Set().push_back(elem);
                    }
                }
                else {
                    CRef<CID2S_Bioseq_Ids::C_E> elem
                        (new CID2S_Bioseq_Ids::C_E);
                    elem->SetGi_range().SetStart(start);
                    elem->SetGi_range().SetCount(count);
                    m_Ids.Set().push_back(elem);
                }
            }
        CID2S_Bioseq_Ids& m_Ids;
    };
    
    
    void AddLoc(CID2S_Seq_loc& loc, const TGiSet& whole_gi_set)
    {
        ForEachGiRange(whole_gi_set, FAddGiRangeToSeq_loc(loc));
    }


    void AddLoc(CID2S_Seq_loc& loc, const TIdSet& whole_id_set)
    {
        ITERATE ( TIdSet, it, whole_id_set ) {
            AddLoc(loc, MakeLoc(*it));
        }
    }


    void AddLoc(CID2S_Seq_loc& loc, const TIntervalSet& interval_set)
    {
        ITERATE ( TIntervalSet, it, interval_set ) {
            AddLoc(loc, MakeLoc(it->first, it->second));
        }
    }

    struct SLessSeq_id
    {
        bool operator()(const CConstRef<CSeq_id>& id1,
                        const CConstRef<CSeq_id>& id2)
            {
                if ( id1->Which() != id2->Which() ) {
                    return id1->Which() < id2->Which();
                }
                return id1->AsFastaString() < id2->AsFastaString();
            }
    };


    void AddBioseqIds(CID2S_Bioseq_Ids& ret, const set<CSeq_id_Handle>& ids)
    {
        TGiSet gi_set;
        typedef set<CConstRef<CSeq_id>, SLessSeq_id> TIdSet;
        TIdSet id_set;
        ITERATE ( set<CSeq_id_Handle>, it, ids ) {
            if ( it->IsGi() ) {
                gi_set.insert(it->GetGi());
            }
            else {
                id_set.insert(it->GetSeqId());
            }
        }
        
        ForEachGiRange(gi_set, FAddGiRangeToBioseqIds(ret));
        ITERATE ( TIdSet, it, id_set ) {
            CRef<CID2S_Bioseq_Ids::C_E> elem(new CID2S_Bioseq_Ids::C_E);
            elem->SetSeq_id(const_cast<CSeq_id&>(**it));
            ret.Set().push_back(elem);
        }
    }


    void AddBioseq_setIds(CID2S_Bioseq_set_Ids& ret, const set<int>& ids)
    {
        ITERATE ( set<int>, it, ids ) {
            ret.Set().push_back(*it);
        }
    }


    typedef set<CSeq_id_Handle> TBioseqs;
    typedef set<int> TBioseq_sets;
    typedef pair<TBioseqs, TBioseq_sets> TPlaces;

    void AddPlace(TPlaces& places, const CPlaceId& place)
    {
        if ( place.IsBioseq() ) {
            places.first.insert(place.GetBioseqId());
        }
        else {
            places.second.insert(place.GetBioseq_setId());
        }
    }
}


TSeqPos CBlobSplitterImpl::GetLength(const CSeq_id_Handle& id) const
{
    TEntries::const_iterator iter = m_Entries.find(CPlaceId(id));
    if ( iter != m_Entries.end() ) {
        const CSeq_inst& inst = iter->second.m_Bioseq->GetInst();
        if ( inst.IsSetLength() )
            return inst.GetLength();
    }
    return kInvalidSeqPos;
}


bool CBlobSplitterImpl::IsWhole(const CSeq_id_Handle& id,
                                const TRange& range) const
{
    return range == range.GetWhole() ||
        range.GetFrom() <= 0 && range.GetLength() >= GetLength(id);
}


void CBlobSplitterImpl::SetLoc(CID2S_Seq_loc& loc,
                               const CSeqsRange& ranges) const
{
    TGiSet whole_gi_set;
    TIdSet whole_id_set;
    TIntervalSet interval_set;

    ITERATE ( CSeqsRange, it, ranges ) {
        const TRange& range = it->second.GetTotalRange();
        if ( IsWhole(it->first, range) ) {
            if ( it->first.IsGi() ) {
                whole_gi_set.insert(it->first.GetGi());
            }
            else {
                whole_id_set.insert(it->first);
            }
        }
        else {
            interval_set[it->first].insert(range);
        }
    }

    AddLoc(loc, whole_gi_set);
    AddLoc(loc, whole_id_set);
    AddLoc(loc, interval_set);
    _ASSERT(loc.Which() != loc.e_not_set);
}


void CBlobSplitterImpl::SetLoc(CID2S_Seq_loc& loc,
                               const CHandleRangeMap& ranges) const
{
    TGiSet whole_gi_set;
    TIdSet whole_id_set;
    TIntervalSet interval_set;

    ITERATE ( CHandleRangeMap, id_it, ranges ) {
        ITERATE ( CHandleRange, it, id_it->second ) {
            const TRange& range = it->first;
            if ( IsWhole(id_it->first, range) ) {
                if ( id_it->first.IsGi() ) {
                    whole_gi_set.insert(id_it->first.GetGi());
                }
                else {
                    whole_id_set.insert(id_it->first);
                }
            }
            else {
                interval_set[id_it->first].insert(range);
            }
        }
    }

    AddLoc(loc, whole_gi_set);
    AddLoc(loc, whole_id_set);
    AddLoc(loc, interval_set);
    _ASSERT(loc.Which() != loc.e_not_set);
}


void CBlobSplitterImpl::SetLoc(CID2S_Seq_loc& loc,
                               const CSeq_id_Handle& id,
                               const TRange& range) const
{
    if ( IsWhole(id, range) ) {
        if ( id.IsGi() ) {
            loc.SetWhole_gi(id.GetGi());
        }
        else {
            loc.SetWhole_seq_id(const_cast<CSeq_id&>(*id.GetSeqId()));
        }
    }
    else {
        if ( id.IsGi() ) {
            CID2S_Gi_Interval& interval = loc.SetGi_interval();
            interval.SetGi(id.GetGi());
            SetRange(interval, range);
        }
        else {
            CID2S_Seq_id_Interval& interval = loc.SetSeq_id_interval();
            interval.SetSeq_id(const_cast<CSeq_id&>(*id.GetSeqId()));
            SetRange(interval, range);
        }
    }
}


CRef<CID2S_Seq_loc> CBlobSplitterImpl::MakeLoc(const CSeqsRange& range) const
{
    CRef<CID2S_Seq_loc> loc(new CID2S_Seq_loc);
    SetLoc(*loc, range);
    return loc;
}


CRef<CID2S_Seq_loc>
CBlobSplitterImpl::MakeLoc(const CSeq_id_Handle& id, const TRange& range) const
{
    CRef<CID2S_Seq_loc> loc(new CID2S_Seq_loc);
    SetLoc(*loc, id, range);
    return loc;
}


CID2S_Chunk_Data& CBlobSplitterImpl::GetChunkData(TChunkData& chunk_data,
                                                  const CPlaceId& place_id)
{
    CRef<CID2S_Chunk_Data>& data = chunk_data[place_id];
    if ( !data ) {
        data.Reset(new CID2S_Chunk_Data);
        if ( place_id.IsBioseq_set() ) {
            data->SetId().SetBioseq_set(place_id.GetBioseq_setId());
        }
        else if ( place_id.GetBioseqId().IsGi() ) {
            data->SetId().SetGi(place_id.GetBioseqId().GetGi());
        }
        else {
            CConstRef<CSeq_id> id = place_id.GetBioseqId().GetSeqId();
            data->SetId().SetSeq_id(const_cast<CSeq_id&>(*id));
        }
    }
    return *data;
}


CRef<CID2S_Bioseq_Ids>
CBlobSplitterImpl::MakeBioseqIds(const set<CSeq_id_Handle>& ids) const
{
    CRef<CID2S_Bioseq_Ids> ret(new CID2S_Bioseq_Ids);
    AddBioseqIds(*ret, ids);
    return ret;
}


CRef<CID2S_Bioseq_set_Ids>
CBlobSplitterImpl::MakeBioseq_setIds(const set<int>& ids) const
{
    CRef<CID2S_Bioseq_set_Ids> ret(new CID2S_Bioseq_set_Ids);
    AddBioseq_setIds(*ret, ids);
    return ret;
}


void CBlobSplitterImpl::MakeID2Chunk(TChunkId chunk_id, const SChunkInfo& info)
{
    TChunkData chunk_data;
    TChunkContent chunk_content;

    typedef unsigned TDescTypeMask;
    _ASSERT(CSeqdesc::e_MaxChoice < 32);
    typedef map<TDescTypeMask, TPlaces> TDescPlaces;
    TDescPlaces all_descrs;
    TPlaces all_annot_places;
    typedef map<CAnnotName, SAllAnnots> TAllAnnots;
    TAllAnnots all_annots;
    CHandleRangeMap all_data;
    typedef set<CSeq_id_Handle> TBioseqIds;
    typedef map<CPlaceId, TBioseqIds> TBioseqPlaces;
    TBioseqPlaces all_bioseqs;

    ITERATE ( SChunkInfo::TChunkSeq_descr, it, info.m_Seq_descr ) {
        const CPlaceId& place_id = it->first;
        TDescTypeMask desc_type_mask = 0;
        CID2S_Chunk_Data::TDescr& dst =
            GetChunkData(chunk_data, place_id).SetDescr();
        ITERATE ( SChunkInfo::TPlaceSeq_descr, dit, it->second ) {
            CSeq_descr& descr = const_cast<CSeq_descr&>(*dit->m_Descr);
            ITERATE ( CSeq_descr::Tdata, i, descr.Get() ) {
                dst.Set().push_back(*i);
                desc_type_mask |= (1<<(**i).Which());
            }
        }
        AddPlace(all_descrs[desc_type_mask], place_id);
    }

    ITERATE ( SChunkInfo::TChunkAnnots, it, info.m_Annots ) {
        const CPlaceId& place_id = it->first;
        AddPlace(all_annot_places, place_id);
        CID2S_Chunk_Data::TAnnots& dst =
            GetChunkData(chunk_data, place_id).SetAnnots();
        ITERATE ( SChunkInfo::TPlaceAnnots, ait, it->second ) {
            CRef<CSeq_annot> annot = MakeSeq_annot(*ait->first, ait->second);
            dst.push_back(annot);

            // collect locations
            CAnnotName name = CSeq_annot_SplitInfo::GetName(*ait->first);
            all_annots[name].Add(*annot);
        }
    }

    ITERATE ( SChunkInfo::TChunkSeq_data, it, info.m_Seq_data ) {
        const CPlaceId& place_id = it->first;
        _ASSERT(place_id.IsBioseq());
        CID2S_Chunk_Data::TSeq_data& dst =
            GetChunkData(chunk_data, place_id).SetSeq_data();
        CRef<CID2S_Sequence_Piece> piece;
        TSeqPos p_start = kInvalidSeqPos;
        TSeqPos p_end = kInvalidSeqPos;
        ITERATE ( SChunkInfo::TPlaceSeq_data, data_it, it->second ) {
            const CSeq_data_SplitInfo& data = *data_it;
            const TRange& range = data.GetRange();
            TSeqPos start = range.GetFrom(), length = range.GetLength();
            if ( !piece || start != p_end ) {
                if ( piece ) {
                    all_data.AddRange(place_id.GetBioseqId(),
                                      TRange(p_start, p_end-1),
                                      eNa_strand_unknown);
                    dst.push_back(piece);
                }
                piece.Reset(new CID2S_Sequence_Piece);
                p_start = p_end = start;
                piece->SetStart(p_start);
            }
            CRef<CSeq_literal> literal(new CSeq_literal);
            literal->SetLength(length);
            literal->SetSeq_data(const_cast<CSeq_data&>(*data.m_Data));
            piece->SetData().push_back(literal);
            p_end += length;
        }
        if ( piece ) {
            all_data.AddRange(place_id.GetBioseqId(),
                              TRange(p_start, p_end-1),
                              eNa_strand_unknown);
            dst.push_back(piece);
        }
    }

    ITERATE ( SChunkInfo::TChunkBioseq, it, info.m_Bioseq ) {
        const CPlaceId& place_id = it->first;
        _ASSERT(place_id.IsBioseq_set());
        TBioseqIds& ids = all_bioseqs[place_id];
        CID2S_Chunk_Data::TBioseqs& dst =
            GetChunkData(chunk_data, place_id).SetBioseqs();
        ITERATE ( SChunkInfo::TPlaceBioseq, bit, it->second ) {
            const CBioseq& seq = *bit->m_Bioseq;
            dst.push_back(Ref(const_cast<CBioseq*>(&seq)));
            ITERATE ( CBioseq::TId, idit, seq.GetId() ) {
                ids.insert(CSeq_id_Handle::GetHandle(**idit));
            }
        }
    }

    NON_CONST_ITERATE ( TAllAnnots, nit, all_annots ) {
        nit->second.SplitInfo();
        const CAnnotName& annot_name = nit->first;
        ITERATE ( SAllAnnots::TSplitAnnots, it, nit->second.m_SplitAnnots ) {
            const SAllAnnots::TTypeSet& type_set = it->first;
            const CSeqsRange& location = it->second;
            CRef<CID2S_Chunk_Content> content(new CID2S_Chunk_Content);
            CID2S_Seq_annot_Info& annot_info = content->SetSeq_annot();
            if ( annot_name.IsNamed() ) {
                annot_info.SetName(annot_name.GetName());
            }
            typedef CSeqFeatData::ESubtype TSubtype;
            typedef CSeqFeatData::E_Choice TFeatType;
            typedef set<TSubtype> TSubtypes;
            typedef map<TFeatType, TSubtypes> TFeatTypes;
            TFeatTypes feat_types;
            ITERATE ( SAllAnnots::TTypeSet, tit, type_set ) {
                const SAnnotTypeSelector& t = *tit;
                switch ( t.GetAnnotType() ) {
                case CSeq_annot::C_Data::e_Align:
                    annot_info.SetAlign();
                    break;
                case CSeq_annot::C_Data::e_Graph:
                    annot_info.SetGraph();
                    break;
                case CSeq_annot::C_Data::e_Ftable:
                    feat_types[t.GetFeatType()].insert(t.GetFeatSubtype());
                    break;
                default:
                    _ASSERT("bad annot type" && 0);
                }
            }
            ITERATE ( TFeatTypes, tit, feat_types ) {
                TFeatType t = tit->first;
                const TSubtypes& subtypes = tit->second;
                bool all_subtypes =
                    subtypes.find(CSeqFeatData::eSubtype_any) !=
                    subtypes.end();
                if ( !all_subtypes ) {
                    all_subtypes = true;
                    for ( TSubtype st = CSeqFeatData::eSubtype_bad;
                          st <= CSeqFeatData::eSubtype_max;
                          st = TSubtype(st+1) ) {
                        if ( CSeqFeatData::GetTypeFromSubtype(st) == t &&
                             subtypes.find(st) == subtypes.end() ) {
                            all_subtypes = false;
                            break;
                        }
                    }
                }
                CRef<CID2S_Feat_type_Info> type_info(new CID2S_Feat_type_Info);
                type_info->SetType(t);
                if ( !all_subtypes ) {
                    ITERATE ( TSubtypes, stit, subtypes ) {
                        type_info->SetSubtypes().push_back(*stit);
                    }
                }
                annot_info.SetFeat().push_back(type_info);
            }
            annot_info.SetSeq_loc(*MakeLoc(location));
            chunk_content.push_back(content);
        }
    }

    if ( !all_descrs.empty() ) {
        ITERATE ( TDescPlaces, tmit, all_descrs ) {
            CRef<CID2S_Chunk_Content> content(new CID2S_Chunk_Content);
            CID2S_Seq_descr_Info& inf = content->SetSeq_descr();
            inf.SetType_mask(tmit->first);
            if ( !tmit->second.first.empty() ) {
                inf.SetBioseqs(*MakeBioseqIds(tmit->second.first));
            }
            if ( !tmit->second.second.empty() ) {
                inf.SetBioseq_sets(*MakeBioseq_setIds(tmit->second.second));
            }
            chunk_content.push_back(content);
        }
    }

    if ( !all_annot_places.first.empty() ||
         !all_annot_places.second.empty() ) {
        CRef<CID2S_Chunk_Content> content(new CID2S_Chunk_Content);
        CID2S_Seq_annot_place_Info& inf = content->SetSeq_annot_place();
        if ( !all_annot_places.first.empty() ) {
            inf.SetBioseqs(*MakeBioseqIds(all_annot_places.first));
        }
        if ( !all_annot_places.second.empty() ) {
            inf.SetBioseq_sets(*MakeBioseq_setIds(all_annot_places.second));
        }
        chunk_content.push_back(content);
    }

    if ( !all_data.empty() ) {
        CRef<CID2S_Chunk_Content> content(new CID2S_Chunk_Content);
        SetLoc(content->SetSeq_data(), all_data);
        chunk_content.push_back(content);
    }

    if ( !all_bioseqs.empty() ) {
        CRef<CID2S_Chunk_Content> content(new CID2S_Chunk_Content);
        CID2S_Chunk_Content::TBioseq_place& places =content->SetBioseq_place();
        ITERATE ( TBioseqPlaces, it, all_bioseqs ) {
            CRef<CID2S_Bioseq_place_Info> place(new CID2S_Bioseq_place_Info);
            place->SetBioseq_set(it->first.GetBioseq_setId());
            place->SetSeq_ids(*MakeBioseqIds(it->second));
            places.push_back(place);
        }
        chunk_content.push_back(content);
    }

    CRef<CID2S_Chunk> chunk(new CID2S_Chunk);
    CID2S_Chunk::TData& dst_chunk_data = chunk->SetData();
    ITERATE ( TChunkData, it, chunk_data ) {
        dst_chunk_data.push_back(it->second);
    }
    m_ID2_Chunks[CID2S_Chunk_Id(chunk_id)] = chunk;

    CRef<CID2S_Chunk_Info> chunk_info(new CID2S_Chunk_Info);
    chunk_info->SetId(CID2S_Chunk_Id(chunk_id));
    CID2S_Chunk_Info::TContent& dst_chunk_content = chunk_info->SetContent();
    ITERATE ( TChunkContent, it, chunk_content ) {
        dst_chunk_content.push_back(*it);
    }
    m_Split_Info->SetChunks().push_back(chunk_info);
}


void CBlobSplitterImpl::AttachToSkeleton(const SChunkInfo& info)
{
    ITERATE ( SChunkInfo::TChunkSeq_descr, it, info.m_Seq_descr ) {
        TEntries::iterator seq_it = m_Entries.find(it->first);
        _ASSERT(seq_it != m_Entries.end());
        ITERATE ( SChunkInfo::TPlaceSeq_descr, dit, it->second ) {
            const CSeq_descr& src = const_cast<CSeq_descr&>(*dit->m_Descr);
            CSeq_descr* dst;
            if ( seq_it->second.m_Bioseq ) {
                dst = &seq_it->second.m_Bioseq->SetDescr();
            }
            else {
                dst = &seq_it->second.m_Bioseq_set->SetDescr();
            }
            dst->Set().insert(dst->Set().end(),
                              src.Get().begin(), src.Get().end());
        }
    }

    ITERATE ( SChunkInfo::TChunkAnnots, it, info.m_Annots ) {
        TEntries::iterator seq_it = m_Entries.find(it->first);
        _ASSERT(seq_it != m_Entries.end());
        ITERATE ( SChunkInfo::TPlaceAnnots, annot_it, it->second ) {
            CRef<CSeq_annot> annot = MakeSeq_annot(*annot_it->first,
                                                   annot_it->second);
            if ( seq_it->second.m_Bioseq ) {
                seq_it->second.m_Bioseq->SetAnnot().push_back(annot);
            }
            else {
                seq_it->second.m_Bioseq_set->SetAnnot().push_back(annot);
            }
        }
    }

    ITERATE ( SChunkInfo::TChunkSeq_data, i, info.m_Seq_data ) {
        TEntries::iterator seq_it = m_Entries.find(i->first);
        _ASSERT(seq_it != m_Entries.end());
        _ASSERT(seq_it->second.m_Bioseq);
        CSeq_inst& inst = seq_it->second.m_Bioseq->SetInst();

        TSeqPos seq_length = GetLength(inst);
        typedef map<TSeqPos, CRef<CSeq_literal> > TLiterals;
        TLiterals literals;
        if ( inst.IsSetExt() ) {
            _ASSERT(inst.GetExt().Which() == CSeq_ext::e_Delta);
            CDelta_ext& delta = inst.SetExt().SetDelta();
            TSeqPos pos = 0;
            NON_CONST_ITERATE ( CDelta_ext::Tdata, j, delta.Set() ) {
                CDelta_seq& seg = **j;
                if ( seg.Which() == CDelta_seq::e_Literal &&
                     !seg.GetLiteral().IsSetSeq_data() ){
                    literals[pos] = &seg.SetLiteral();
                }
                pos += GetLength(seg);
            }
            _ASSERT(pos == seq_length);
        }
        else {
            _ASSERT(!inst.IsSetSeq_data());
        }

        ITERATE ( SChunkInfo::TPlaceSeq_data, j, i->second ) {
            TRange range = j->GetRange();
            CSeq_data& data = const_cast<CSeq_data&>(*j->m_Data);
            if ( range.GetFrom() == 0 && range.GetLength() == seq_length ) {
                _ASSERT(!inst.IsSetSeq_data());
                inst.SetSeq_data(data);
            }
            else {
                TLiterals::iterator iter = literals.find(range.GetFrom());
                _ASSERT(iter != literals.end());
                CSeq_literal& literal = *iter->second;
                _ASSERT(!literal.IsSetSeq_data());
                literal.SetSeq_data(data);
            }
        }
    }
}


CRef<CSeq_annot>
CBlobSplitterImpl::MakeSeq_annot(const CSeq_annot& src,
                                 const TAnnotObjects& objs)
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    if ( src.IsSetId() ) {
        CSeq_annot::TId& id = annot->SetId();
        ITERATE ( CSeq_annot::TId, it, src.GetId() ) {
            id.push_back(Ref(&NonConst(**it)));
        }
    }
    if ( src.IsSetDb() ) {
        annot->SetDb(src.GetDb());
    }
    if ( src.IsSetName() ) {
        annot->SetName(src.GetName());
    }
    if ( src.IsSetDesc() ) {
        annot->SetDesc(NonConst(src.GetDesc()));
    }
    switch ( src.GetData().Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
        ITERATE ( CLocObjects_SplitInfo, it, objs ) {
            CObject& obj = NonConst(*it->m_Object);
            annot->SetData().SetFtable()
                .push_back(Ref(&dynamic_cast<CSeq_feat&>(obj)));
        }
        break;
    case CSeq_annot::C_Data::e_Align:
        ITERATE ( CLocObjects_SplitInfo, it, objs ) {
            CObject& obj = NonConst(*it->m_Object);
            annot->SetData().SetAlign()
                .push_back(Ref(&dynamic_cast<CSeq_align&>(obj)));
        }
        break;
    case CSeq_annot::C_Data::e_Graph:
        ITERATE ( CLocObjects_SplitInfo, it, objs ) {
            CObject& obj = NonConst(*it->m_Object);
            annot->SetData().SetGraph()
                .push_back(Ref(&dynamic_cast<CSeq_graph&>(obj)));
        }
        break;
    default:
        _ASSERT("bad annot type" && 0);
    }
    return annot;
}


size_t CBlobSplitterImpl::CountAnnotObjects(const TID2Chunks& chunks)
{
    size_t count = 0;
    ITERATE ( TID2Chunks, it, chunks ) {
        count += CountAnnotObjects(*it->second);
    }
    return count;
}


size_t CBlobSplitterImpl::CountAnnotObjects(const CID2S_Chunk& chunk)
{
    size_t count = 0;
    for ( CTypeConstIterator<CSeq_annot> it(ConstBegin(chunk)); it; ++it ) {
        count += CSeq_annot_SplitInfo::CountAnnotObjects(*it);
    }
    return count;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.19  2005/02/02 19:49:55  grichenk
* Fixed more warnings
*
* Revision 1.18  2004/10/18 14:00:22  vasilche
* Updated splitter for new SeqSplit specs.
*
* Revision 1.17  2004/10/07 14:04:06  vasilche
* Generate correct desc mask in split info.
*
* Revision 1.16  2004/08/19 14:18:54  vasilche
* Added splitting of whole Bioseqs.
*
* Revision 1.15  2004/08/05 18:26:37  vasilche
* CAnnotName and CAnnotTypeSelector are moved in separate headers.
*
* Revision 1.14  2004/08/04 14:48:21  vasilche
* Added joining of very small chunks with skeleton.
*
* Revision 1.13  2004/07/12 16:55:47  vasilche
* Fixed split info for descriptions and Seq-data to load them properly.
*
* Revision 1.12  2004/06/30 20:56:32  vasilche
* Added splitting of Seqdesr objects (disabled yet).
*
* Revision 1.11  2004/06/15 14:05:50  vasilche
* Added splitting of sequence.
*
* Revision 1.10  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.9  2004/02/04 18:05:40  grichenk
* Added annotation filtering by set of types/subtypes.
* Renamed *Choice to *Type in SAnnotSelector.
*
* Revision 1.8  2004/01/22 20:10:42  vasilche
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
* Revision 1.7  2004/01/07 17:36:24  vasilche
* Moved id2_split headers to include/objmgr/split.
* Fixed include path to genbank.
*
* Revision 1.6  2003/12/03 19:30:45  kuznets
* Misprint fixed
*
* Revision 1.5  2003/12/02 19:12:24  vasilche
* Fixed compilation on MSVC.
*
* Revision 1.4  2003/12/01 18:37:10  vasilche
* Separate different annotation types in split info to reduce memory usage.
*
* Revision 1.3  2003/11/26 23:04:58  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.2  2003/11/26 17:56:02  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.1  2003/11/12 16:18:28  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
