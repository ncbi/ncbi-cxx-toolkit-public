/*  $Id$
 * ===========================================================================
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
 *  Author:  Eugene Vasilchenko
 *
 *  File Description: Methods to create object manager structures from ID2 spec
 *
 */

#include <ncbi_pch.hpp>
#include <objmgr/impl/split_parser.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/error_codes.hpp>
#include <objects/seqsplit/seqsplit__.hpp>

#define NCBI_USE_ERRCODE_X   ObjMgr_Rd_Split

BEGIN_NCBI_SCOPE

NCBI_DEFINE_ERR_SUBCODE_X(1);

BEGIN_SCOPE(objects)

void CSplitParser::Attach(CTSE_Info& tse, const CID2S_Split_Info& split)
{
    if ( !tse.HasSeq_entry() ) {
        if ( split.IsSetSkeleton() ) {
            tse.SetSeq_entry(const_cast<CSeq_entry&>(split.GetSkeleton()));
        }
    }
    CTSE_Split_Info& sinfo = tse.GetSplitInfo();
    ITERATE ( CID2S_Split_Info::TChunks, it, split.GetChunks() ) {
        CRef<CTSE_Chunk_Info> chunk = Parse(**it);
        sinfo.AddChunk(*chunk);
    }
}


CRef<CTSE_Chunk_Info> CSplitParser::Parse(const CID2S_Chunk_Info& info)
{
    CRef<CTSE_Chunk_Info> ret(new CTSE_Chunk_Info(info.GetId()));
    ITERATE ( CID2S_Chunk_Info::TContent, it, info.GetContent() ) {
        const CID2S_Chunk_Content& content = **it;
        switch ( content.Which() ) {
        case CID2S_Chunk_Content::e_not_set:
            break;
        case CID2S_Chunk_Content::e_Seq_descr:
            x_Attach(*ret, content.GetSeq_descr());
            break;
        case CID2S_Chunk_Content::e_Seq_annot:
            x_Attach(*ret, content.GetSeq_annot());
            break;
        case CID2S_Chunk_Content::e_Seq_annot_place:
            x_Attach(*ret, content.GetSeq_annot_place());
            break;
        case CID2S_Chunk_Content::e_Seq_data:
            x_Attach(*ret, content.GetSeq_data());
            break;
        case CID2S_Chunk_Content::e_Bioseq_place:
            ITERATE ( CID2S_Chunk_Content::TBioseq_place, it2,
                      content.GetBioseq_place() ) {
                x_Attach(*ret, **it2);
            }
            break;
        case CID2S_Chunk_Content::e_Seq_assembly:
            x_Attach(*ret, content.GetSeq_assembly());
            break;
        case CID2S_Chunk_Content::e_Feat_ids:
            ITERATE ( CID2S_Chunk_Content::TFeat_ids, it2,
                      content.GetFeat_ids() ) {
                x_Attach(*ret, **it2);
            }
            break;
        default:
            ERR_POST_X_ONCE(1, "ID2 Split parser: Unexpected split data: "<<content.Which());
        }
    }
    return ret;
}


void CSplitParser::x_Attach(CTSE_Chunk_Info& chunk,
                            const CID2S_Seq_data_Info& data)
{
    TLocationSet loc;
    x_ParseLocation(loc, data);
    chunk.x_AddSeq_data(loc);
}


namespace {
    template<class Func>
    void ForEach(const CID2S_Bioseq_Ids& ids, Func func)
    {
        ITERATE ( CID2S_Bioseq_Ids::Tdata, it, ids.Get() ) {
            const CID2S_Bioseq_Ids::C_E& e = **it;
            switch ( e.Which() ) {
            case CID2S_Bioseq_Ids::C_E::e_Gi:
                func(CSeq_id_Handle::GetGiHandle(e.GetGi()));
                break;
            case CID2S_Bioseq_Ids::C_E::e_Seq_id:
                func(CSeq_id_Handle::GetHandle(e.GetSeq_id()));
                break;
            case CID2S_Bioseq_Ids::C_E::e_Gi_range:
            {
                const CID2S_Gi_Range& r = e.GetGi_range();
                int n = r.GetCount();
                for( TGi id = r.GetStart(); n--; ++id ) {
                    func(CSeq_id_Handle::GetGiHandle(id));
                }
                break;
            }
            default:
                NCBI_THROW(CLoaderException, eOtherError,
                           "unknown bioseq id type");
            }
        }
    }


    template<class Func>
    void ForEach(const CID2S_Bioseq_set_Ids& ids, Func func)
    {
        ITERATE ( CID2S_Bioseq_set_Ids::Tdata, it, ids.Get() ) {
            func(*it);
        }
    }

    struct FAddDescInfo
    {
        FAddDescInfo(CTSE_Chunk_Info& chunk, unsigned type_mask)
            : m_Chunk(chunk), m_TypeMask(type_mask)
            {
            }
        void operator()(const CSeq_id_Handle& id) const
            {
                m_Chunk.x_AddDescInfo(m_TypeMask, id);
            }
        void operator()(int id) const
            {
                m_Chunk.x_AddDescInfo(m_TypeMask, id);
            }
        CTSE_Chunk_Info& m_Chunk;
        unsigned m_TypeMask;
    };
    struct FAddAnnotPlace
    {
        FAddAnnotPlace(CTSE_Chunk_Info& chunk)
            : m_Chunk(chunk)
            {
            }
        void operator()(const CSeq_id_Handle& id) const
            {
                m_Chunk.x_AddAnnotPlace(id);
            }
        void operator()(int id) const
            {
                m_Chunk.x_AddAnnotPlace(id);
            }
        CTSE_Chunk_Info& m_Chunk;
    };
    struct FAddBioseqId
    {
        FAddBioseqId(CTSE_Chunk_Info& chunk)
            : m_Chunk(chunk)
            {
            }
        void operator()(const CSeq_id_Handle& id) const
            {
                m_Chunk.x_AddBioseqId(id);
            }
        CTSE_Chunk_Info& m_Chunk;
    };
    struct FAddAssemblyInfo
    {
        FAddAssemblyInfo(CTSE_Chunk_Info& chunk)
            : m_Chunk(chunk)
            {
            }
        void operator()(const CSeq_id_Handle& id) const
            {
                m_Chunk.x_AddAssemblyInfo(id);
            }
        CTSE_Chunk_Info& m_Chunk;
    };
}


void CSplitParser::x_Attach(CTSE_Chunk_Info& chunk,
                            const CID2S_Seq_descr_Info& place)
{
    CID2S_Seq_descr_Info::TType_mask type_mask = place.GetType_mask();
    if ( place.IsSetBioseqs() ) {
        ForEach(place.GetBioseqs(), FAddDescInfo(chunk, type_mask));
    }
    if ( place.IsSetBioseq_sets() ) {
        ForEach(place.GetBioseq_sets(), FAddDescInfo(chunk, type_mask));
    }
}

void CSplitParser::x_Attach(CTSE_Chunk_Info& chunk,
                            const CID2S_Seq_annot_place_Info& place)
{
    if ( place.IsSetBioseqs() ) {
        ForEach(place.GetBioseqs(), FAddAnnotPlace(chunk));
    }
    if ( place.IsSetBioseq_sets() ) {
        ForEach(place.GetBioseq_sets(), FAddAnnotPlace(chunk));
    }
}


void CSplitParser::x_Attach(CTSE_Chunk_Info& chunk,
                            const CID2S_Bioseq_place_Info& place)
{
    chunk.x_AddBioseqPlace(place.GetBioseq_set());
    ForEach(place.GetSeq_ids(), FAddBioseqId(chunk));
}


void CSplitParser::x_Attach(CTSE_Chunk_Info& chunk,
                            const CID2S_Seq_annot_Info& annot)
{
    CAnnotName name;
    if ( annot.IsSetName() ) {
        const string& n = annot.GetName();
        if ( !n.empty() ) {
            name.SetNamed(annot.GetName());
        }
    }
    
    TLocationSet loc;
    x_ParseLocation(loc, annot.GetSeq_loc());

    if ( annot.IsSetAlign() ) {
        SAnnotTypeSelector sel(CSeq_annot::TData::e_Align);
        chunk.x_AddAnnotType(name, sel, loc);
    }
    if ( annot.IsSetGraph() ) {
        SAnnotTypeSelector sel(CSeq_annot::TData::e_Graph);
        chunk.x_AddAnnotType(name, sel, loc);
    }
        
    ITERATE ( CID2S_Seq_annot_Info::TFeat, it, annot.GetFeat() ) {
        const CID2S_Feat_type_Info& type = **it;
        if ( type.IsSetSubtypes() ) {
            ITERATE ( CID2S_Feat_type_Info::TSubtypes, sit,
                      type.GetSubtypes() ) {
                SAnnotTypeSelector sel(CSeqFeatData::ESubtype(+*sit));
                chunk.x_AddAnnotType(name, sel, loc);
            }
        }
        else if ( type.GetType() ) {
            SAnnotTypeSelector sel(CSeqFeatData::E_Choice(type.GetType()));
            chunk.x_AddAnnotType(name, sel, loc);
        }
        else {
            SAnnotTypeSelector sel(CSeq_annot::C_Data::e_Seq_table);
            chunk.x_AddAnnotType(name, sel, loc);
        }
    }
}


void CSplitParser::x_Attach(CTSE_Chunk_Info& chunk,
                            const CID2S_Seq_assembly_Info& place)
{
    ForEach(place.GetBioseqs(), FAddAssemblyInfo(chunk));
}

inline
void CSplitParser::x_AddWhole(TLocationSet& vec,
                              const TLocationId& id)
{
    vec.push_back(TLocation(id, TLocationRange::GetWhole()));
}


inline
void CSplitParser::x_AddInterval(TLocationSet& vec,
                                 const TLocationId& id,
                                 TSeqPos start, TSeqPos length)
{
    vec.push_back(TLocation(id, TLocationRange(start, start+length-1)));
}


inline
void CSplitParser::x_AddGiWhole(TLocationSet& vec, TGi gi)
{
    x_AddWhole(vec, CSeq_id_Handle::GetGiHandle(gi));
}


inline
void CSplitParser::x_AddGiInterval(TLocationSet& vec, TGi gi,
                                   TSeqPos start, TSeqPos length)
{
    x_AddInterval(vec, CSeq_id_Handle::GetGiHandle(gi), start, length);
}


void CSplitParser::x_Attach(CTSE_Chunk_Info& chunk,
                            const CID2S_Seq_feat_Ids_Info& ids)
{
    chunk.x_AddFeat_ids();
    ITERATE ( CID2S_Seq_feat_Ids_Info::TFeat_types, it, ids.GetFeat_types() ) {
        const CID2S_Feat_type_Info& type = **it;
        if ( type.IsSetSubtypes() ) {
            ITERATE ( CID2S_Feat_type_Info::TSubtypes, sit,
                      type.GetSubtypes() ) {
                SAnnotTypeSelector sel(CSeqFeatData::ESubtype(+*sit));
                if ( ids.IsSetLocal_ids() ) {
                    chunk.x_AddFeat_ids(sel, ids.GetLocal_ids());
                }
                if ( ids.IsSetLocal_str_ids() ) {
                    chunk.x_AddFeat_ids(sel, ids.GetLocal_str_ids());
                }
            }
        }
        else {
            SAnnotTypeSelector sel(CSeqFeatData::E_Choice(type.GetType()));
            if ( ids.IsSetLocal_ids() ) {
                chunk.x_AddFeat_ids(sel, ids.GetLocal_ids());
            }
            if ( ids.IsSetLocal_str_ids() ) {
                chunk.x_AddFeat_ids(sel, ids.GetLocal_str_ids());
            }
        }
    }
    ITERATE ( CID2S_Seq_feat_Ids_Info::TXref_types, it, ids.GetXref_types() ) {
        const CID2S_Feat_type_Info& type = **it;
        if ( type.IsSetSubtypes() ) {
            ITERATE ( CID2S_Feat_type_Info::TSubtypes, sit,
                      type.GetSubtypes() ) {
                SAnnotTypeSelector sel(CSeqFeatData::ESubtype(+*sit));
                if ( ids.IsSetLocal_ids() ) {
                    chunk.x_AddXref_ids(sel, ids.GetLocal_ids());
                }
                if ( ids.IsSetLocal_str_ids() ) {
                    chunk.x_AddXref_ids(sel, ids.GetLocal_str_ids());
                }
            }
        }
        else {
            SAnnotTypeSelector sel(CSeqFeatData::E_Choice(type.GetType()));
            if ( ids.IsSetLocal_ids() ) {
                chunk.x_AddXref_ids(sel, ids.GetLocal_ids());
            }
            if ( ids.IsSetLocal_str_ids() ) {
                chunk.x_AddXref_ids(sel, ids.GetLocal_str_ids());
            }
        }
    }
}


void CSplitParser::x_ParseLocation(TLocationSet& vec,
                                   const CID2S_Seq_loc& loc)
{
    switch ( loc.Which() ) {
    case CID2S_Seq_loc::e_Whole_gi:
    {
        x_AddGiWhole(vec, loc.GetWhole_gi());
        break;
    }
    
    case CID2S_Seq_loc::e_Whole_seq_id:
    {
        x_AddWhole(vec, CSeq_id_Handle::GetHandle(loc.GetWhole_seq_id()));
        break;
    }
    
    case CID2S_Seq_loc::e_Whole_gi_range:
    {
        const CID2S_Gi_Range& wr = loc.GetWhole_gi_range();
        int n = wr.GetCount();
        for ( TGi gi = wr.GetStart(); n--; ++gi )
            x_AddGiWhole(vec, gi);
        break;
    }

    case CID2S_Seq_loc::e_Gi_interval:
    {
        const CID2S_Gi_Interval& interval = loc.GetGi_interval();
        x_AddGiInterval(vec,
                        interval.GetGi(),
                        interval.GetStart(),
                        interval.GetLength());
        break;
    }

    case CID2S_Seq_loc::e_Seq_id_interval:
    {
        const CID2S_Seq_id_Interval& interval = loc.GetSeq_id_interval();
        x_AddInterval(vec,
                      CSeq_id_Handle::GetHandle(interval.GetSeq_id()),
                      interval.GetStart(),
                      interval.GetLength());
        break;
    }

    case CID2S_Seq_loc::e_Gi_ints:
    {
        const CID2S_Gi_Ints& ints = loc.GetGi_ints();
        TGi gi = ints.GetGi();
        ITERATE ( CID2S_Gi_Ints::TInts, it, ints.GetInts() ) {
            const CID2S_Interval& interval = **it;
            x_AddGiInterval(vec, gi,
                            interval.GetStart(), interval.GetLength());
        }
        break;
    }

    case CID2S_Seq_loc::e_Seq_id_ints:
    {
        const CID2S_Seq_id_Ints& ints = loc.GetSeq_id_ints();
        CSeq_id_Handle id = CSeq_id_Handle::GetHandle(ints.GetSeq_id());
        ITERATE ( CID2S_Seq_id_Ints::TInts, it, ints.GetInts() ) {
            const CID2S_Interval& interval = **it;
            x_AddInterval(vec, id,
                          interval.GetStart(), interval.GetLength());
        }
        break;
    }

    case CID2S_Seq_loc::e_Loc_set:
    {
        const CID2S_Seq_loc::TLoc_set& loc_set = loc.GetLoc_set();
        ITERATE ( CID2S_Seq_loc::TLoc_set, it, loc_set ) {
            x_ParseLocation(vec, **it);
        }
        break;
    }

    case CID2S_Seq_loc::e_not_set:
        break;
    }
}


void CSplitParser::Load(CTSE_Chunk_Info& chunk,
                        const CID2S_Chunk& id2_chunk)
{
    ITERATE ( CID2S_Chunk::TData, dit, id2_chunk.GetData() ) {
        const CID2S_Chunk_Data& data = **dit;

        CTSE_Chunk_Info::TPlace place;
        switch ( data.GetId().Which() ) {
        case CID2S_Chunk_Data::TId::e_Gi:
            place.first = CSeq_id_Handle::GetGiHandle(data.GetId().GetGi());
            break;
        case CID2S_Chunk_Data::TId::e_Seq_id:
            place.first = CSeq_id_Handle::GetHandle(data.GetId().GetSeq_id());
            break;
        case CID2S_Chunk_Data::TId::e_Bioseq_set:
            place.second = data.GetId().GetBioseq_set();
            break;
        default:
            NCBI_THROW(CLoaderException, eOtherError,
                       "Unexpected place type");
        }

        if ( data.IsSetDescr() ) {
            chunk.x_LoadDescr(place, data.GetDescr());
        }

        ITERATE ( CID2S_Chunk_Data::TAnnots, it, data.GetAnnots() ) {
            chunk.x_LoadAnnot(place, **it/*Ref(new CSeq_annot_Info(**it))*/);
        }

        if ( data.IsSetAssembly() ) {
            if ( !place.first ) {
                NCBI_THROW(CLoaderException, eOtherError,
                        "assembly not allowed in bioseq-sets");
            }
            chunk.x_LoadAssembly(place.first, data.GetAssembly());
        }

        ITERATE ( CID2S_Chunk_Data::TSeq_map, it, data.GetSeq_map() ) {
            NCBI_THROW(CLoaderException, eOtherError,
                       "split seq-map is not supported");
        }

        ITERATE ( CID2S_Chunk_Data::TSeq_data, it, data.GetSeq_data() ) {
            const CID2S_Sequence_Piece& piece = **it;
            chunk.x_LoadSequence(place, piece.GetStart(), piece.GetData());
        }

        ITERATE ( CID2S_Chunk_Data::TBioseqs, it, data.GetBioseqs() ) {
            const CBioseq& bioseq = **it;
            chunk.x_LoadBioseq(place, bioseq);
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
