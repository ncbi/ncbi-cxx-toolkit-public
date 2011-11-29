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
#include <objmgr/split/split_exceptions.hpp>

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

#include <objects/seqsplit/ID2S_Chunk_Id.hpp>
#include <objects/seqsplit/ID2S_Chunk_Id.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>

#include <objmgr/split/blob_splitter.hpp>
#include <objmgr/split/object_splitinfo.hpp>
#include <objmgr/split/annot_piece.hpp>
#include <objmgr/split/asn_sizer.hpp>
#include <objmgr/split/chunk_info.hpp>
#include <objmgr/split/place_id.hpp>
#include <objmgr/impl/seq_table_info.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   ObjMgr_BlobSplit

BEGIN_NCBI_SCOPE

NCBI_DEFINE_ERR_SUBCODE_X(12);

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


static CAsnSizer s_Sizer;

static CSize small_annot;

void CBlobSplitterImpl::CopySkeleton(CSeq_entry& dst, const CSeq_entry& src)
{
    small_annot.clear();

    if ( src.IsSeq() ) {
        CopySkeleton(dst.SetSeq(), src.GetSeq());
    }
    else {
        CopySkeleton(dst.SetSet(), src.GetSet());
    }

    if ( m_Params.m_Verbose ) {
        // annot statistics
        if ( small_annot ) {
            NcbiCout << "Small Seq-annots: " << small_annot << NcbiEndl;
        }
    }

    if ( m_Params.m_Verbose && m_Skeleton == &dst ) {
        // skeleton statistics
        s_Sizer.Set(*m_Skeleton, m_Params);
        CSize size(s_Sizer);
        NcbiCout <<
            "\nSkeleton: " << size << NcbiEndl;
    }
}


TSeqPos CBlobSplitterImpl::GetLength(const CSeq_data& src) const
{
    switch ( src.Which() ) {
    case CSeq_data::e_Iupacna:
        return src.GetIupacna().Get().size();
    case CSeq_data::e_Iupacaa:
        return src.GetIupacaa().Get().size();
    case CSeq_data::e_Ncbi2na:
        return src.GetNcbi2na().Get().size()*4;
    case CSeq_data::e_Ncbi4na:
        return src.GetNcbi4na().Get().size()*2;
    case CSeq_data::e_Ncbi8na:
        return src.GetNcbi8na().Get().size();
    case CSeq_data::e_Ncbi8aa:
        return src.GetNcbi8aa().Get().size();
    case CSeq_data::e_Ncbieaa:
        return src.GetNcbieaa().Get().size();
    default:
        NCBI_THROW(CSplitException, eInvalidBlob,
                   "Invalid Seq-data");
    }
}


TSeqPos CBlobSplitterImpl::GetLength(const CDelta_seq& src) const
{
    switch ( src.Which() ) {
    case CDelta_seq::e_Literal:
        return src.GetLiteral().GetLength();
    case CDelta_seq::e_Loc:
        return src.GetLoc().GetInt().GetLength();
    default:
        NCBI_THROW(CSplitException, eInvalidBlob,
                   "Delta-seq is unset");
    }
}


TSeqPos CBlobSplitterImpl::GetLength(const CDelta_ext& src) const
{
    TSeqPos ret = 0;
    ITERATE ( CDelta_ext::Tdata, it, src.Get() ) {
        ret += GetLength(**it);
    }
    return ret;
}


TSeqPos CBlobSplitterImpl::GetLength(const CSeq_ext& src) const
{
    return GetLength(src.GetDelta());
}


TSeqPos CBlobSplitterImpl::GetLength(const CSeq_inst& src) const
{
    try {
        if ( src.IsSetLength() ) {
            return src.GetLength();
        }
        else if ( src.IsSetSeq_data() ) {
            return GetLength(src.GetSeq_data());
        }
        else if ( src.IsSetExt() ) {
            return GetLength(src.GetExt());
        }
    }
    catch ( CException& exc ) {
        ERR_POST_X(1, "GetLength(CSeq_inst): exception: " << exc.GetMsg());
    }
    return kInvalidSeqPos;
}


void CBlobSplitterImpl::CopySkeleton(CBioseq& dst, const CBioseq& src)
{
    dst.Reset();
    const CBioseq::TId& ids = src.GetId();
    CPlaceId place_id;
    ITERATE ( CBioseq::TId, it, ids ) {
        const CSeq_id& id = **it;
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
        if ( !place_id.IsBioseq() ||
             (!place_id.GetBioseqId().IsGi() &&
              (idh.IsGi() || idh.IsBetter(place_id.GetBioseqId()))) ) {
            place_id = CPlaceId(idh);
        }
        dst.SetId().push_back(Ref(&NonConst(id)));
    }

    const CSeq_inst& inst = src.GetInst();
    TSeqPos seq_length = GetLength(inst);

    bool need_split_descr;
    if ( m_Params.m_DisableSplitDescriptions ) {
        need_split_descr = false;
    }
    else {
        need_split_descr = src.IsSetDescr() && !src.GetDescr().Get().empty();
    }

    bool need_split_inst;
    if ( m_Params.m_DisableSplitSequence ) {
        need_split_inst = false;
    }
    else {
        need_split_inst = false;
        if ( inst.IsSetSeq_data() && inst.IsSetExt() ) {
            // both data and ext
            need_split_inst = false;
        }
        else {
            if ( inst.IsSetSeq_data() ) {
                need_split_inst = true;
            }
            if ( inst.IsSetExt() ) {
                const CSeq_ext& ext = inst.GetExt();
                if ( ext.Which() == CSeq_ext::e_Delta ) {
                    need_split_inst = true;
                    // check delta segments' lengths
                    try {
                        if ( seq_length != GetLength(ext) ) {
                            need_split_inst = false;
                        }
                    }
                    catch ( CException& /*exc*/ ) {
                        need_split_inst = false;
                    }
                }
            }
        }
    }

    bool need_split_annot;
    if ( m_Params.m_DisableSplitAnnotations ) {
        need_split_annot = false;
    }
    else {
        need_split_annot = !src.GetAnnot().empty();
    }

    bool need_split = need_split_descr || need_split_inst || need_split_annot;
    CPlace_SplitInfo* info = 0;

    if ( need_split ) {
        if ( !place_id.IsBioseq() ) {
            ERR_POST_X(2, "Bioseq doesn't have Seq-id");
        }
        else {
            info = &m_Entries[place_id];
            
            if ( info->m_PlaceId.IsBioseq() ) {
                ERR_POST_X(3, "Several Bioseqs with the same id: " <<
                              place_id.GetBioseqId().AsString());
                info = 0;
            }
            else {
                _ASSERT(info->m_PlaceId.IsNull());
                _ASSERT(!info->m_Bioseq);
                _ASSERT(!info->m_Bioseq_set);
                info->m_PlaceId = place_id;
                info->m_Bioseq.Reset(&dst);
            }
        }
        
        if ( !info ) {
            need_split_descr = need_split_inst = need_split_annot = false;
        }
    }
    
    if ( need_split_descr ) {
        if ( !CopyDescr(*info, seq_length, src.GetDescr()) ) {
            dst.SetDescr().Set() = src.GetDescr().Get();
        }
    }
    else {
        if ( src.IsSetDescr() ) {
            dst.SetDescr().Set() = src.GetDescr().Get();
        }
    }

    if ( need_split_inst ) {
        CopySequence(*info, seq_length, dst.SetInst(), inst);
    }
    else {
        dst.SetInst(NonConst(inst));
    }
    
    if ( need_split_annot ) {
        ITERATE ( CBioseq::TAnnot, it, src.GetAnnot() ) {
            if ( !CopyAnnot(*info, **it) ) {
                dst.SetAnnot().push_back(*it);
            }
        }
    }
}


void CBlobSplitterImpl::CopySkeleton(CBioseq_set& dst, const CBioseq_set& src)
{
    dst.Reset();
    CPlaceId place_id;
    if ( src.IsSetId() ) {
        dst.SetId(NonConst(src.GetId()));
        if ( src.GetId().IsId() ) {
            place_id = CPlaceId(src.GetId().GetId());
        }
    }
    else {
        int id = m_NextBioseq_set_Id++;
        dst.SetId().SetId(id);
        place_id = CPlaceId(id);
    }
    if ( src.IsSetColl() ) {
        dst.SetColl(NonConst(src.GetColl()));
    }
    if ( src.IsSetLevel() ) {
        dst.SetLevel(src.GetLevel());
    }
    if ( src.IsSetClass() ) {
        dst.SetClass(src.GetClass());
    }
    if ( src.IsSetRelease() ) {
        dst.SetRelease(src.GetRelease());
    }
    if ( src.IsSetDate() ) {
        dst.SetDate(NonConst(src.GetDate()));
    }

    // Try to split all descriptors, the most important of them will get
    // skeleton priority anyway.
    bool need_split_descr = !m_Params.m_DisableSplitDescriptions
        &&  src.IsSetDescr();

    bool need_split_annot;
    if ( m_Params.m_DisableSplitAnnotations ) {
        need_split_annot = false;
    }
    else {
        need_split_annot = !src.GetAnnot().empty();
    }

    bool need_split_bioseq = false;
    if ( m_Params.m_SplitWholeBioseqs ) {
        ITERATE ( CBioseq_set::TSeq_set, it, src.GetSeq_set() ) {
            const CSeq_entry& entry = **it;
            if ( entry.Which() == CSeq_entry::e_Seq ) {
                const CBioseq& bioseq = entry.GetSeq();
                if ( CanSplitBioseq(bioseq) ) {
                    need_split_bioseq = true;
                    break;
                }
            }
        }
    }

    bool need_split =need_split_descr || need_split_annot || need_split_bioseq;
    CPlace_SplitInfo* info = 0;

    if ( need_split ) {
        if ( !place_id.IsBioseq_set() ) {
            ERR_POST_X(4, "Bioseq_set doesn't have integer id");
        }
        else {
            info = &m_Entries[place_id];
            if ( info->m_PlaceId.IsBioseq_set() ) {
                ERR_POST_X(5, "Several Bioseq-sets with the same id: " <<
                              place_id.GetBioseq_setId());
                info = 0;
            }
            else {
                _ASSERT(info->m_PlaceId.IsNull());
                _ASSERT(!info->m_Bioseq);
                _ASSERT(!info->m_Bioseq_set);
                info->m_PlaceId = place_id;
                info->m_Bioseq_set.Reset(&dst);
            }
        }

        if ( !info ) {
            need_split_descr = need_split_annot = need_split_bioseq = 0;
        }
    }


    if ( need_split_descr ) {
        if ( !CopyDescr(*info, kInvalidSeqPos, src.GetDescr()) ) {
            dst.SetDescr().Set() = src.GetDescr().Get();
        }
    }
    else {
        if ( src.IsSetDescr() ) {
            dst.SetDescr().Set() = src.GetDescr().Get();
        }
    }

    if ( need_split_annot ) {
        ITERATE ( CBioseq_set::TAnnot, it, src.GetAnnot() ) {
            if ( !CopyAnnot(*info, **it) ) {
                dst.SetAnnot().push_back(*it);
            }
        }
    }

    dst.SetSeq_set();
    ITERATE ( CBioseq_set::TSeq_set, it, src.GetSeq_set() ) {
        if ( need_split_bioseq ) {
            const CSeq_entry& entry = **it;
            if ( entry.Which() == CSeq_entry::e_Seq ) {
                const CBioseq& seq = entry.GetSeq();
                if ( SplitBioseq(*info, seq) ) {
                    continue;
                }
            }
        }
        dst.SetSeq_set().push_back(Ref(new CSeq_entry));
        CopySkeleton(*dst.SetSeq_set().back(), **it);
    }
    
    if ( src.IsSetClass() &&
         src.GetClass() == CBioseq_set::eClass_segset &&
         !src.GetSeq_set().empty() ) {
        CConstRef<CSeq_entry> first = src.GetSeq_set().front();
        if ( first->IsSeq() ) {
            m_Master = new CMasterSeqSegments();
            CBioseq_Handle bh = m_Scope->GetBioseqHandle(first->GetSeq());
            m_Master->AddSegments(bh.GetSeqMap());
            ITERATE ( CBioseq_set::TSeq_set, it, src.GetSeq_set() ) {
                if ( *it != first && (*it)->IsSeq() ) {
                    m_Master->AddSegmentIds((*it)->GetSeq().GetId());
                }
            }
        }
    }
}


bool CBlobSplitterImpl::CopyDescr(CPlace_SplitInfo& place_info,
                                  TSeqPos seq_length,
                                  const CSeq_descr& descr)
{
    _ASSERT(!place_info.m_Descr);
    place_info.m_Descr = new CSeq_descr_SplitInfo(place_info.m_PlaceId,
                                                  seq_length,
                                                  descr, m_Params);
    if ( !place_info.m_Bioseq ) {
        // try not to split descriptors of Bioseq-sets
        place_info.m_Descr->m_Priority = eAnnotPriority_skeleton;
    }
    if ( seq_length != kInvalidSeqPos && seq_length > 100000 ) {
        // try not to split descriptors of very long sequences
        place_info.m_Descr->m_Priority = eAnnotPriority_skeleton;
    }
    return true;
}


bool CBlobSplitterImpl::CopyHist(CPlace_SplitInfo& place_info,
                                 const CSeq_hist& hist)
{
    if ( m_Params.m_DisableSplitAssembly ) {
        return false;
    }
    _ASSERT( place_info.m_Bioseq );
    _ASSERT(!place_info.m_Hist);
    // Split history with big assembly only
    if ( !hist.IsSetAssembly() ) {
        return false;
    }
    place_info.m_Hist = new CSeq_hist_SplitInfo(place_info.m_PlaceId,
                                                hist, m_Params);
    if (place_info.m_Hist->m_Size.GetZipSize() < m_Params.m_MinChunkSize) {
        place_info.m_Hist.Reset();
        return false;
    }
    return true;
}


bool CBlobSplitterImpl::CopySequence(CPlace_SplitInfo& place_info,
                                     TSeqPos seq_length,
                                     CSeq_inst& dst,
                                     const CSeq_inst& src)
{
    if ( !place_info.m_Bioseq ) {
        // we will not split descriptors of Bioseq-sets
        return false;
    }
    _ASSERT(!place_info.m_Inst);
    place_info.m_Inst.Reset(new CSeq_inst_SplitInfo);
    CSeq_inst_SplitInfo& info = *place_info.m_Inst;
    info.m_Seq_inst.Reset(&src);

    dst.SetRepr(src.GetRepr());
    dst.SetMol(src.GetMol());

    if ( seq_length != kInvalidSeqPos )
        dst.SetLength(seq_length);
    if ( src.IsSetFuzz() )
        dst.SetFuzz(const_cast<CInt_fuzz&>(src.GetFuzz()));
    if ( src.IsSetTopology() )
        dst.SetTopology(src.GetTopology());
    if ( src.IsSetStrand() )
        dst.SetStrand(src.GetStrand());
    if ( src.IsSetHist() ) {
        if ( !CopyHist(place_info, src.GetHist()) ) {
            dst.SetHist(const_cast<CSeq_hist&>(src.GetHist()));
        }
        else {
            // Create history, but do not create assembly
            dst.SetHist();
            if ( src.GetHist().IsSetReplaces() ) {
                dst.SetHist().SetReplaces(const_cast<CSeq_hist_rec&>(
                    src.GetHist().GetReplaces()));
            }
            if ( src.GetHist().IsSetReplaced_by() ) {
                dst.SetHist().SetReplaced_by(const_cast<CSeq_hist_rec&>(
                    src.GetHist().GetReplaced_by()));
            }
            if ( src.GetHist().IsSetDeleted() ) {
                dst.SetHist().SetDeleted(const_cast<CSeq_hist::TDeleted&>(
                    src.GetHist().GetDeleted()));
            }
        }
    }

    if ( src.IsSetSeq_data() ) {
        CSeq_data_SplitInfo data;
        CRange<TSeqPos> range;
        range.SetFrom(0).SetLength(seq_length);
        data.SetSeq_data(place_info.m_PlaceId, range, seq_length,
                         src.GetSeq_data(), m_Params);
        info.Add(data);
    }
    else {
        if ( !src.IsSetExt() ) {
            return false;
        }
        _ASSERT(src.IsSetExt());
        const CSeq_ext& src_ext = src.GetExt();
        _ASSERT(src_ext.Which() == CSeq_ext::e_Delta);
        const CDelta_ext& src_delta = src_ext.GetDelta();
        CDelta_ext& dst_delta = dst.SetExt().SetDelta();
        TSeqPos pos = 0;
        ITERATE ( CDelta_ext::Tdata, it, src_delta.Get() ) {
            const CDelta_seq& src_seq = **it;
            TSeqPos length = GetLength(src_seq);
            CRef<CDelta_seq> new_seq;
            switch ( src_seq.Which() ) {
            case CDelta_seq::e_Loc:
                new_seq = *it;
                break;
            case CDelta_seq::e_Literal:
            {{
                const CSeq_literal& src_lit = src_seq.GetLiteral();
                new_seq.Reset(new CDelta_seq);
                CSeq_literal& dst_lit = new_seq->SetLiteral();
                dst_lit.SetLength(length);
                if ( src_lit.IsSetFuzz() )
                    dst_lit.SetFuzz(const_cast<CInt_fuzz&>(src_lit.GetFuzz()));
                if ( src_lit.IsSetSeq_data() ) {
                    const CSeq_data& src_data = src_lit.GetSeq_data();
                    if ( src_data.IsGap() ) {
                        dst_lit.SetSeq_data(const_cast<CSeq_data&>(src_data));
                    }
                    else {
                        CSeq_data_SplitInfo data;
                        CRange<TSeqPos> range;
                        range.SetFrom(pos).SetLength(length);
                        data.SetSeq_data(place_info.m_PlaceId,
                                         range, seq_length,
                                         src_data, m_Params);
                        info.Add(data);
                    }
                }
                break;
            }}
            default:
                new_seq.Reset(new CDelta_seq);
                break;
            }
            dst_delta.Set().push_back(new_seq);
            pos += length;
        }
    }
    return false;
}


bool CBlobSplitterImpl::CopyAnnot(CPlace_SplitInfo& place_info,
                                  const CSeq_annot& annot)
{
    if ( m_Params.m_DisableSplitAnnotations ) {
        return false;
    }

    switch ( annot.GetData().Which() ) {
    case CSeq_annot::TData::e_Ftable:
    case CSeq_annot::TData::e_Align:
    case CSeq_annot::TData::e_Graph:
        break;
    case CSeq_annot::TData::e_Seq_table:
        if ( m_Params.m_SplitNonFeatureSeqTables ||
             CSeqTableInfo::IsGoodFeatTable(annot.GetData().GetSeq_table()) ) {
            break;
        }
        // splitting non-feature Seq-tables is disabled
        return false;
    default:
        // we don't split other types of Seq-annot
        return false;
    }

    CSeq_annot_SplitInfo& info = place_info.m_Annots[ConstRef(&annot)];
    info.SetSeq_annot(annot, m_Params, *this);

    if ( info.m_Size.GetAsnSize() > 1024 ) {
        if ( m_Params.m_Verbose ) {
            NcbiCout << info;
        }
    }
    else {
        small_annot += info.m_Size;
    }

    return true;
}


// quick check if Bioseq is suitable for whole splitting
bool CBlobSplitterImpl::CanSplitBioseq(const CBioseq& seq) const
{
    return GetLength(seq.GetInst()) < m_Params.m_ChunkSize*2 &&
        seq.GetId().size() <= 4;
}


bool CBlobSplitterImpl::SplitBioseq(CPlace_SplitInfo& place_info,
                                    const CBioseq& seq)
{
    _ASSERT(place_info.m_Bioseq_set);
    if ( !CanSplitBioseq(seq) ) {
        return false;
    }
    const CBioseq::TId& ids = seq.GetId();
    
    for ( CTypeConstIterator<CSeq_id> i(ConstBegin(seq)); i; ++i ) {
        bool same_id = false;
        ITERATE ( CBioseq::TId, j, ids ) {
            if ( i->Equals(**j) ) {
                same_id = true;
                break;
            }
        }
        if ( !same_id ) {
            // extra Seq-id in Bioseq
            return false;
        }
    }
    // check compressed size also
    CBioseq_SplitInfo info(seq, m_Params);
    if ( info.m_Size.GetZipSize() > m_Params.m_ChunkSize ) {
        // too big
        return false;
    }

    // ok, split it
    place_info.m_Bioseqs.push_back(info);

    return true;
}


size_t CBlobSplitterImpl::CountAnnotObjects(const CSeq_entry& entry)
{
    size_t count = 0;
    for ( CTypeConstIterator<CSeq_annot> it(ConstBegin(entry)); it; ++it ) {
        count += CSeq_annot_SplitInfo::CountAnnotObjects(*it);
    }
    return count;
}


END_SCOPE(objects)
END_NCBI_SCOPE
