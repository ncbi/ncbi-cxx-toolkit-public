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

#include "blob_splitter_impl.hpp"

#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>

#include <objects/general/Object_id.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>

#include <objects/id2/ID2S_Split_Info.hpp>
#include <objects/id2/ID2S_Chunk_Id.hpp>
#include <objects/id2/ID2S_Chunk.hpp>
#include <objects/id2/ID2S_Chunk_Data.hpp>
#include <objects/id2/ID2S_Chunk_Info.hpp>
#include <objects/id2/ID2S_Chunk_Content.hpp>
#include <objects/id2/ID2S_Seq_annot_Info.hpp>
#include <objects/id2/ID2S_Feat_type_Info.hpp>
#include <objects/id2/ID2_Seq_loc.hpp>
#include <objects/id2/ID2_Id_Range.hpp>
#include <objects/id2/ID2_Seq_range.hpp>
#include <objects/id2/ID2_Interval.hpp>
#include <objects/id2/ID2_Packed_Seq_ints.hpp>

#include "blob_splitter.hpp"
#include "object_splitinfo.hpp"
#include "annot_piece.hpp"
#include "asn_sizer.hpp"
#include "chunk_info.hpp"

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(objects);

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
    m_SplittedBlob.Reset();
    m_Skeleton.Reset(new CSeq_entry);
    m_NextBioseq_set_Id = 1;
    m_Bioseqs.clear();
    m_Pieces.reset();
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
    m_SplittedBlob.Reset(*m_Skeleton, *m_Split_Info);
    ITERATE ( TID2Chunks, it, m_ID2_Chunks ) {
        m_SplittedBlob.AddChunk(it->first, *it->second);
    }
}


struct SAnnotTypeLocation {
    SAnnotTypeLocation(void)
        : m_Align(false), m_Graph(false)
        {
        }

    typedef CSeqsRange TTotalLocation;
    typedef set<CSeqFeatData::ESubtype> TFeatSubtypes;
    typedef map<CSeqFeatData::E_Choice, TFeatSubtypes> TFeatTypes;

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
            }
        }


    void Add(const CSeq_annot::C_Data::TGraph& objs)
        {
            ITERATE ( CSeq_annot::C_Data::TGraph, it, objs ) {
                m_Graph = true;
                m_TotalLocation.Add(**it);
            }
        }
    void Add(const CSeq_annot::C_Data::TAlign& objs)
        {
            ITERATE ( CSeq_annot::C_Data::TAlign, it, objs ) {
                m_Align = true;
                m_TotalLocation.Add(**it);
            }
        }
    void Add(const CSeq_annot::C_Data::TFtable& objs)
        {
            ITERATE ( CSeq_annot::C_Data::TFtable, it, objs ) {
                CSeqFeatData::E_Choice type = (*it)->GetData().Which();
                CSeqFeatData::ESubtype subtype = (*it)->GetData().GetSubtype();
                m_Feat[type].insert(subtype);
                m_TotalLocation.Add(**it);
            }
        }

    bool m_Align;
    bool m_Graph;
    TFeatTypes m_Feat;
    
    TTotalLocation m_TotalLocation;
};
typedef map<CAnnotName, SAnnotTypeLocation> TTotalAnnotLocation;


typedef set<int> TWhole_set;
TWhole_set seq_gis;
typedef set<CSeqsRange::TRange> TGiInt_set;
typedef map<int, TGiInt_set> TInt_set;
TGiInt_set seq_ints;


CRef<CID2_Seq_loc> MakeLoc(int gi, const TGiInt_set& int_set)
{
    CRef<CID2_Seq_loc> loc(new CID2_Seq_loc);
    if ( int_set.size() == 1 ) {
        CID2_Interval& interval = loc->SetInt();
        interval.SetGi(gi);
        const CSeqsRange::TRange& range = *int_set.begin();
        interval.SetStart(range.GetFrom());
        interval.SetLength(range.GetLength());
    }
    else {
        CID2_Packed_Seq_ints& seq_ints = loc->SetInt_set();
        seq_ints.SetGi(gi);
        ITERATE ( TGiInt_set, it, int_set ) {
            CRef<CID2_Seq_range> add(new CID2_Seq_range);
            const CSeqsRange::TRange& range = *it;
            add->SetStart(range.GetFrom());
            add->SetLength(range.GetLength());
            seq_ints.SetInts().push_back(add);
        }
    }
    return loc;
}


void AddLoc(CRef<CID2_Seq_loc>& loc, const CRef<CID2_Seq_loc>& add)
{
    if ( !loc ) {
        loc = add;
    }
    else {
        if ( !loc->IsLoc_set() ) {
            CRef<CID2_Seq_loc> loc_set(new CID2_Seq_loc);
            loc_set->SetLoc_set().push_back(loc);
            loc = loc_set;
        }
        loc->SetLoc_set().push_back(add);
    }
}


void AddLoc(CRef<CID2_Seq_loc>& loc, const TInt_set& int_set)
{
    ITERATE ( TInt_set, it, int_set ) {
        AddLoc(loc, MakeLoc(it->first, it->second));
    }
}


void AddLoc(CRef<CID2_Seq_loc>& loc, const TWhole_set& whole_set)
{
    ITERATE ( TWhole_set, it, whole_set ) {
        CRef<CID2_Seq_loc> add(new CID2_Seq_loc);
        add->SetWhole(*it);
        AddLoc(loc, add);
    }
}


CRef<CID2_Seq_loc> MakeLoc(const CSeqsRange& range)
{
    TWhole_set whole_set;
    TInt_set int_set;

    ITERATE ( CSeqsRange, it, range ) {
        CConstRef<CSeq_id> id = it->first.GetSeqId();
        _ASSERT(id->IsGi());
        int gi = id->GetGi();
        CSeqsRange::TRange range = it->second.GetTotalRange();
        if ( range == range.GetWhole() ) {
            whole_set.insert(gi);
            _ASSERT(int_set.count(gi) == 0);
        }
        else {
            int_set[gi].insert(range);
            _ASSERT(whole_set.count(gi) == 0);
        }
    }

    CRef<CID2_Seq_loc> loc;
    AddLoc(loc, int_set);
    AddLoc(loc, whole_set);
    _ASSERT(loc);
    return loc;
}


void CBlobSplitterImpl::MakeID2Chunk(int id, const SChunkInfo& info)
{
    CRef<CID2S_Chunk> chunk(new CID2S_Chunk);
    CRef<CID2S_Chunk_Info> chunk_info(new CID2S_Chunk_Info);
    chunk_info->SetId(CID2S_Chunk_Id(id));

    TTotalAnnotLocation annot_loc;

    ITERATE ( SChunkInfo::TChunkAnnots, it, info.m_Annots ) {
        CRef<CID2S_Chunk_Data> data(new CID2S_Chunk_Data);
        chunk->SetData().push_back(data);
        CID2S_Chunk_Data::TId& id = data->SetId();
        if ( it->first > 0 ) {
            id.SetGi(it->first);
        }
        else {
            id.SetBioseq_set(-it->first);
        }
        ITERATE ( SChunkInfo::TIdAnnots, annot_it, it->second ) {
            CRef<CSeq_annot> annot = MakeSeq_annot(*annot_it->first,
                                                   annot_it->second);
            data->SetAnnots().push_back(annot);

            // collect locations
            CAnnotName name = CSeq_annot_SplitInfo::GetName(*annot_it->first);
            annot_loc[name].Add(*annot);
        }
    }

    ITERATE ( TTotalAnnotLocation, it, annot_loc ) {
        CRef<CID2S_Chunk_Content> content(new CID2S_Chunk_Content);
        CID2S_Seq_annot_Info& annot_info = content->SetSeq_annot();
        if ( it->first.IsNamed() ) {
            annot_info.SetName(it->first.GetName());
        }
        if ( it->second.m_Graph ) {
            annot_info.SetGraph() = true;
        }
        if ( it->second.m_Align ) {
            annot_info.SetAlign() = true;
        }
        ITERATE ( SAnnotTypeLocation::TFeatTypes, ftit, it->second.m_Feat ) {
            CSeqFeatData::E_Choice type = ftit->first;
            CRef<CID2S_Feat_type_Info> type_info(new CID2S_Feat_type_Info);
            type_info->SetType(type);
            bool all_subtypes = true;
            const SAnnotTypeLocation::TFeatSubtypes& subtypes = ftit->second;
            for ( CSeqFeatData::ESubtype subtype = CSeqFeatData::eSubtype_bad;
                  subtype <= CSeqFeatData::eSubtype_max;
                  subtype = CSeqFeatData::ESubtype(subtype+1) ) {
                if ( CSeqFeatData::GetTypeFromSubtype(subtype) == type &&
                     subtypes.find(subtype) == subtypes.end() ) {
                    all_subtypes = false;
                    break;
                }
            }
            if ( !all_subtypes ) {
                ITERATE ( SAnnotTypeLocation::TFeatSubtypes, stit, subtypes ) {
                    type_info->SetSubtypes().push_back(*stit);
                }
            }
            annot_info.SetFeat().push_back(type_info);
        }
        
        annot_info.SetSeq_loc(*MakeLoc(it->second.m_TotalLocation));
        chunk_info->SetContent().push_back(content);
    }

#if 0
    NcbiCout << "Objects: in SChunkInfo: " << info.CountAnnotObjects() <<
        " in CID2S_Chunk: " << CountAnnotObjects(*chunk) << '\n';
#endif

    m_ID2_Chunks[CID2S_Chunk_Id(id)] = chunk;
    m_Split_Info->SetChunks().push_back(chunk_info);
}


void CBlobSplitterImpl::AttachToSkeleton(const SChunkInfo& info)
{
    ITERATE ( SChunkInfo::TChunkAnnots, it, info.m_Annots ) {
        TBioseqs::iterator seq_it = m_Bioseqs.find(it->first);
        _ASSERT(seq_it != m_Bioseqs.end());
        _ASSERT(seq_it->second.m_Bioseq || seq_it->second.m_Bioseq_set);
        ITERATE ( SChunkInfo::TIdAnnots, annot_it, it->second ) {
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
            annot->SetData().SetFtable()
                .push_back(Ref(&dynamic_cast<CSeq_feat&>
                               (NonConst(*it->m_Object))));
        }
        break;
    case CSeq_annot::C_Data::e_Align:
        ITERATE ( CLocObjects_SplitInfo, it, objs ) {
            annot->SetData().SetAlign()
                .push_back(Ref(&dynamic_cast<CSeq_align&>
                               (NonConst(*it->m_Object))));
        }
        break;
    case CSeq_annot::C_Data::e_Graph:
        ITERATE ( CLocObjects_SplitInfo, it, objs ) {
            annot->SetData().SetGraph()
                .push_back(Ref(&dynamic_cast<CSeq_graph&>
                               (NonConst(*it->m_Object))));
        }
        break;
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


END_SCOPE(objects);
END_NCBI_SCOPE;

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/11/12 16:18:28  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
