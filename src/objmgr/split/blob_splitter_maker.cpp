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

#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk_Id.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>
#include <objects/seqsplit/ID2S_Chunk_Data.hpp>
#include <objects/seqsplit/ID2S_Chunk_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk_Content.hpp>
#include <objects/seqsplit/ID2S_Seq_annot_Info.hpp>
#include <objects/seqsplit/ID2S_Feat_type_Info.hpp>
#include <objects/seqsplit/ID2_Seq_loc.hpp>
#include <objects/seqsplit/ID2_Id_Range.hpp>
#include <objects/seqsplit/ID2_Seq_range.hpp>
#include <objects/seqsplit/ID2_Interval.hpp>
#include <objects/seqsplit/ID2_Packed_Seq_ints.hpp>

#include <objmgr/split/blob_splitter.hpp>
#include <objmgr/split/object_splitinfo.hpp>
#include <objmgr/split/annot_piece.hpp>
#include <objmgr/split/asn_sizer.hpp>
#include <objmgr/split/chunk_info.hpp>

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
    m_SplitBlob.Reset(*m_Skeleton, *m_Split_Info);
    ITERATE ( TID2Chunks, it, m_ID2_Chunks ) {
        m_SplitBlob.AddChunk(it->first, *it->second);
    }
}


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
                ITERATE(SOneSeqAnnots::TTypeSet, tit, it->second.m_TotalType) {
                    type_set.push_back(*tit);
                }
                m_SplitAnnots[type_set].Add(it->first,
                                            it->second.m_TotalLocation);
            }
        }

    TAllAnnots m_AllAnnots;
    TSplitAnnots m_SplitAnnots;
};


typedef set<int> TWhole_set;
TWhole_set seq_gis;
typedef set<CSeqsRange::TRange> TGiInt_set;
typedef map<int, TGiInt_set> TInt_set;
TGiInt_set seq_ints;


CRef<CID2_Seq_loc> MakeLoc(int gi, const TGiInt_set& int_set)
{
    CRef<CID2_Seq_loc> loc(new CID2_Seq_loc);
    if ( int_set.size() == 1 ) {
        CID2_Interval& interval = loc->SetInterval();
        interval.SetGi(gi);
        const CSeqsRange::TRange& range = *int_set.begin();
        interval.SetStart(range.GetFrom());
        interval.SetLength(range.GetLength());
    }
    else {
        CID2_Packed_Seq_ints& seq_ints = loc->SetPacked_ints();
        seq_ints.SetGi(gi);
        ITERATE ( TGiInt_set, it, int_set ) {
            CRef<CID2_Seq_range> add(new CID2_Seq_range);
            const CSeqsRange::TRange& range = *it;
            add->SetStart(range.GetFrom());
            add->SetLength(range.GetLength());
            seq_ints.SetIntervals().push_back(add);
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


void AddLoc(CRef<CID2_Seq_loc>& loc, int gi_start, int gi_count)
{
    if ( gi_count < 4 ) {
        for ( int i = 0; i < gi_count; ++i ) {
            CRef<CID2_Seq_loc> add(new CID2_Seq_loc);
            add->SetGi_whole(gi_start + i);
            AddLoc(loc, add);
        }
    }
    else {
        CRef<CID2_Seq_loc> add(new CID2_Seq_loc);
        add->SetGi_whole_range().SetStart(gi_start);
        add->SetGi_whole_range().SetCount(gi_count);
        AddLoc(loc, add);
    }
}

void AddLoc(CRef<CID2_Seq_loc>& loc, const TWhole_set& whole_set)
{
    int gi_start = 0, gi_count = 0;
    ITERATE ( TWhole_set, it, whole_set ) {
        if ( gi_count == 0 || *it != gi_start + gi_count ) {
            AddLoc(loc, gi_start, gi_count);
            gi_start = *it;
            gi_count = 0;
        }
        ++gi_count;
    }
    AddLoc(loc, gi_start, gi_count);
}


CRef<CID2_Seq_loc> MakeLoc(const CSeqsRange& range)
{
    TWhole_set whole_set;
    TInt_set int_set;

    ITERATE ( CSeqsRange, it, range ) {
        int gi = it->first.GetGi();
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

    typedef map<CAnnotName, SAllAnnots> TAllAnnots;
    TAllAnnots all_annots;

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
            all_annots[name].Add(*annot);
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
            chunk_info->SetContent().push_back(content);
        }
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
        _ASSERT(bool(seq_it->second.m_Bioseq) || bool(seq_it->second.m_Bioseq_set));
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
