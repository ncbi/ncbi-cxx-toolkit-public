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
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/handle_range_map.hpp>

#include <objects/seqsplit/seqsplit__.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CSplitParser::Attach(CTSE_Info& tse, const CID2S_Split_Info& split)
{
    if ( !tse.HasSeq_entry() ) {
        if ( split.IsSetSkeleton() ) {
            tse.SetSeq_entry(const_cast<CSeq_entry&>(split.GetSkeleton()));
        }
    }
    ITERATE ( CID2S_Split_Info::TChunks, it, split.GetChunks() ) {
        CRef<CTSE_Chunk_Info> chunk = Parse(**it);
        tse.GetSplitInfo().AddChunk(*chunk);
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
        case CID2S_Chunk_Content::e_Bioseq_place:
            ITERATE ( CID2S_Chunk_Content::TBioseq_place, it2,
                      content.GetBioseq_place() ) {
                x_Attach(*ret, **it2);
            }
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
                for( int id = r.GetStart(), n = r.GetCount(); n--; ++id ) {
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
        for ( int gi = wr.GetStart(), end = gi+wr.GetCount(); gi < end; ++gi )
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
        int gi = ints.GetGi();
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
            chunk.x_LoadAnnot(place, Ref(new CSeq_annot_Info(**it)));
        }

        if ( data.IsSetAssembly() ) {
            chunk.x_LoadAssembly(place, data.GetAssembly());
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

/*
 * $Log$
 * Revision 1.15  2005/03/14 17:20:32  vasilche
 * Removed variable name clash.
 *
 * Revision 1.14  2005/03/10 20:55:07  vasilche
 * New CReader/CWriter schema of CGBDataLoader.
 *
 * Revision 1.13  2004/10/18 14:01:28  vasilche
 * Updated split parser for new SeqSplit specs.
 *
 * Revision 1.12  2004/10/07 14:08:10  vasilche
 * Use static GetConfigXxx() functions.
 * Try to deal with withdrawn and private blobs without exceptions.
 * Use correct desc mask in split data.
 *
 * Revision 1.11  2004/08/31 14:40:10  vasilche
 * Postpone indexing of split blobs.
 *
 * Revision 1.10  2004/08/19 14:18:36  vasilche
 * Added splitting of whole Bioseqs.
 *
 * Revision 1.9  2004/08/04 14:55:18  vasilche
 * Changed TSE locking scheme.
 * TSE cache is maintained by CDataSource.
 * Added ID2 reader.
 * CSeqref is replaced by CBlobId.
 *
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
