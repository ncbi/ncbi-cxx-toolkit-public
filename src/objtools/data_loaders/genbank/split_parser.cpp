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
#include <objtools/data_loaders/genbank/split_parser.hpp>

#include <objmgr/objmgr_exception.hpp>

#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/handle_range_map.hpp>

#include <objects/seqsplit/seqsplit__.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CSplitParser::Attach(CTSE_Info& tse, const CID2S_Split_Info& split)
{
    ITERATE ( CID2S_Split_Info::TChunks, it, split.GetChunks() ) {
        CRef<CTSE_Chunk_Info> chunk = Parse(**it);
        chunk->x_TSEAttach(tse);
    }
}


CRef<CTSE_Chunk_Info> CSplitParser::Parse(const CID2S_Chunk_Info& info)
{
    CRef<CTSE_Chunk_Info> ret(new CTSE_Chunk_Info(info.GetId()));
    ITERATE ( CID2S_Chunk_Info::TContent, it, info.GetContent() ) {
        const CID2S_Chunk_Content& content = **it;
        switch ( content.Which() ) {
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
        default:
            NCBI_THROW(CLoaderException, eOtherError,
                       "Unexpected split data");
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


void CSplitParser::x_Attach(CTSE_Chunk_Info& chunk,
                            const CID2S_Seq_descr_Info& place)
{
    ITERATE ( CID2S_Seq_descr_Info::TBioseqs, it, place.GetBioseqs() ) {
        const CID2_Id_Range& range = **it;
        for( int id = range.GetStart(), cnt = range.GetCount(); cnt--; ++id ) {
            chunk.x_AddDescrPlace(CTSE_Chunk_Info::eBioseq, id);
        }
    }
    ITERATE( CID2S_Seq_descr_Info::TBioseq_sets, it, place.GetBioseq_sets() ) {
        const CID2_Id_Range& range = **it;
        for( int id = range.GetStart(), cnt = range.GetCount(); cnt--; ++id ) {
            chunk.x_AddDescrPlace(CTSE_Chunk_Info::eBioseq_set, id);
        }
    }
}


void CSplitParser::x_Attach(CTSE_Chunk_Info& chunk,
                            const CID2S_Seq_annot_place_Info& place)
{
    ITERATE ( CID2S_Seq_annot_place_Info::TBioseqs, it, place.GetBioseqs() ) {
        const CID2_Id_Range& range = **it;
        for( int id = range.GetStart(), cnt = range.GetCount(); cnt--; ++id ) {
            chunk.x_AddAnnotPlace(CTSE_Chunk_Info::eBioseq, id);
        }
    }
    ITERATE ( CID2S_Seq_annot_place_Info::TBioseq_sets,
              it, place.GetBioseq_sets() ) {
        const CID2_Id_Range& range = **it;
        for( int id = range.GetStart(), cnt = range.GetCount(); cnt--; ++id ) {
            chunk.x_AddAnnotPlace(CTSE_Chunk_Info::eBioseq_set, id);
        }
    }
}


void CSplitParser::x_Attach(CTSE_Chunk_Info& chunk,
                            const CID2S_Seq_annot_Info& annot)
{
    CAnnotName name;
    if ( annot.IsSetName() ) {
        name.SetNamed(annot.GetName());
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
        else {
            SAnnotTypeSelector sel(CSeqFeatData::E_Choice(type.GetType()));
            chunk.x_AddAnnotType(name, sel, loc);
        }
    }
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
void CSplitParser::x_AddGiWhole(TLocationSet& vec, int gi)
{
    x_AddWhole(vec, CSeq_id_Handle::GetGiHandle(gi));
}


inline
void CSplitParser::x_AddGiInterval(TLocationSet& vec, int gi,
                                   TSeqPos start, TSeqPos length)
{
    x_AddInterval(vec, CSeq_id_Handle::GetGiHandle(gi), start, length);
}


void CSplitParser::x_ParseLocation(TLocationSet& vec,
                                   const CID2_Seq_loc& loc)
{
    switch ( loc.Which() ) {
    case CID2_Seq_loc::e_Gi_whole:
    {
        x_AddGiWhole(vec, loc.GetGi_whole());
        break;
    }
    
    case CID2_Seq_loc::e_Gi_whole_range:
    {
        const CID2_Id_Range& wr = loc.GetGi_whole_range();
        for ( int gi = wr.GetStart(), end = gi+wr.GetCount(); gi < end; ++gi )
            x_AddGiWhole(vec, gi);
        break;
    }

    case CID2_Seq_loc::e_Interval:
    {
        const CID2_Interval& interval = loc.GetInterval();
        x_AddGiInterval(vec,
                        interval.GetGi(),
                        interval.GetStart(),
                        interval.GetLength());
        break;
    }

    case CID2_Seq_loc::e_Packed_ints:
    {
        const CID2_Packed_Seq_ints& ints = loc.GetPacked_ints();
        ITERATE ( CID2_Packed_Seq_ints::TIntervals, it, ints.GetIntervals() ) {
            const CID2_Seq_range& interval = **it;
            x_AddGiInterval(vec,
                            ints.GetGi(),
                            interval.GetStart(),
                            interval.GetLength());
        }
        break;
    }

    case CID2_Seq_loc::e_Loc_set:
    {
        const CID2_Seq_loc::TLoc_set& loc_set = loc.GetLoc_set();
        ITERATE ( CID2_Seq_loc::TLoc_set, it, loc_set ) {
            x_ParseLocation(vec, **it);
        }
        break;
    }


    case CID2_Seq_loc::e_Seq_loc:
    {
        CHandleRangeMap hrm;
        hrm.AddLocation(loc.GetSeq_loc());
        ITERATE ( CHandleRangeMap, i, hrm ) {
            ITERATE ( CHandleRange, j, i->second ) {
                const CHandleRange::TRange& range = j->first;
                x_AddInterval(vec,
                              i->first,
                              range.GetFrom(),
                              range.GetLength());
            }
        }
        break;
    }

    case CID2_Seq_loc::e_not_set:
        break;
    }
}


void CSplitParser::Load(CTSE_Chunk_Info& chunk,
                        const CID2S_Chunk& id2_chunk)
{
    ITERATE ( CID2S_Chunk::TData, dit, id2_chunk.GetData() ) {
        const CID2S_Chunk_Data& data = **dit;

        CTSE_Chunk_Info::TPlace place;
        if ( data.GetId().IsGi() ) {
            place.first = CTSE_Chunk_Info::eBioseq;
            place.second = data.GetId().GetGi();
        }
        else {
            place.first = CTSE_Chunk_Info::eBioseq_set;
            place.second = data.GetId().GetBioseq_set();
        }

        ITERATE ( CID2S_Chunk_Data::TDescrs, it, data.GetDescrs() ) {
            chunk.x_LoadDescr(place, **it);
        }

        ITERATE ( CID2S_Chunk_Data::TAnnots, it, data.GetAnnots() ) {
            chunk.x_LoadAnnot(place, Ref(new CSeq_annot_Info(**it)));
        }

        ITERATE ( CID2S_Chunk_Data::TAssembly, it, data.GetAssembly() ) {
            NCBI_THROW(CLoaderException, eOtherError,
                       "split assembly is not supported");
        }

        ITERATE ( CID2S_Chunk_Data::TSeq_map, it, data.GetSeq_map() ) {
            NCBI_THROW(CLoaderException, eOtherError,
                       "split seq-map is not supported");
        }

        ITERATE ( CID2S_Chunk_Data::TSeq_data, it, data.GetSeq_data() ) {
            const CID2S_Sequence_Piece& piece = **it;
            chunk.x_LoadSequence(place, piece.GetStart(), piece.GetData());
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * $Log$
 * Revision 1.8  2004/07/12 16:59:53  vasilche
 * Added parsing of information of where split data is to be inserted.
 *
 * Revision 1.7  2004/06/15 14:08:22  vasilche
 * Added parsing split info with split sequences.
 *
 * Revision 1.6  2004/05/21 21:42:52  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.5  2004/03/16 15:47:29  vasilche
 * Added CBioseq_set_Handle and set of EditHandles
 *
 * Revision 1.4  2004/02/17 21:19:35  vasilche
 * Fixed 'non-const reference to temporary' warnings.
 *
 * Revision 1.3  2004/01/28 20:53:42  vasilche
 * Added CSplitParser::Attach().
 *
 * Revision 1.2  2004/01/22 20:36:43  ucko
 * Correct path to seqsplit__.hpp.
 *
 * Revision 1.1  2004/01/22 20:10:35  vasilche
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
 */
