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

#include <objects/seqsplit/ID2S_Chunk_Id.hpp>
#include <objects/seqsplit/ID2S_Chunk_Id.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>

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


static CAsnSizer s_Sizer;

static CSize small_annot;

static size_t landmark_annot_count;
static size_t complex_annot_count;
static size_t simple_annot_count;

void CBlobSplitterImpl::CopySkeleton(CSeq_entry& dst, const CSeq_entry& src)
{
    small_annot.clear();
    landmark_annot_count = complex_annot_count = simple_annot_count = 0;

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
#if 0
    NcbiCout << "Total: after: " <<
        (landmark_annot_count + complex_annot_count + simple_annot_count) <<
        NcbiEndl;
#endif

    if ( m_Params.m_Verbose && m_Skeleton == &dst ) {
        // skeleton statistics
        s_Sizer.Set(*m_Skeleton, m_Params);
        CSize size(s_Sizer);
        NcbiCout <<
            "\nSkeleton: " << size << NcbiEndl;
    }
}


void CBlobSplitterImpl::CopySkeleton(CBioseq& dst, const CBioseq& src)
{
    dst.Reset();
    int gi = 0;
    ITERATE ( CBioseq::TId, it, src.GetId() ) {
        const CSeq_id& id = **it;
        if ( id.IsGi() ) {
            gi = id.GetGi();
        }
        dst.SetId().push_back(Ref(&NonConst(id)));
    }
    if ( src.IsSetDescr() ) {
        dst.SetDescr(NonConst(src.GetDescr()));
    }
    dst.SetInst(NonConst(src.GetInst()));

    if ( src.GetAnnot().empty() ) {
        return;
    }

    if ( gi <= 0 ) {
        ERR_POST("Bioseq doesn't have gi");
        dst.SetAnnot() = src.GetAnnot();
        return;
    }
    
    CBioseq_SplitInfo& info = m_Bioseqs[gi];
    if ( info.m_Id != 0 ) {
        ERR_POST("Several Bioseqs with the same gi: " << gi);
        dst.SetAnnot() = src.GetAnnot();
        return;
    }
    
    _ASSERT(info.m_Id == 0);
    _ASSERT(!info.m_Bioseq);
    _ASSERT(!info.m_Bioseq_set);
    info.m_Id = gi;
    info.m_Bioseq.Reset(&dst);

    ITERATE ( CBioseq::TAnnot, it, src.GetAnnot() ) {
        if ( !CopyAnnot(info, **it) ) {
            dst.SetAnnot().push_back(Ref(&NonConst(**it)));
        }
    }
}


void CBlobSplitterImpl::CopySkeleton(CBioseq_set& dst, const CBioseq_set& src)
{
    dst.Reset();
    int id = 0;
    if ( src.IsSetId() ) {
        dst.SetId(NonConst(dst.GetId()));
        if ( src.GetId().IsId() ) {
            id = src.GetId().GetId();
        }
    }
    else {
        id = m_NextBioseq_set_Id++;
        dst.SetId().SetId(id);
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
    if ( src.IsSetDescr() ) {
        dst.SetDescr(NonConst(src.GetDescr()));
    }

    ITERATE ( CBioseq_set::TSeq_set, it, src.GetSeq_set() ) {
        dst.SetSeq_set().push_back(Ref(new CSeq_entry));
        CopySkeleton(*dst.SetSeq_set().back(), **it);
    }

    if ( src.GetAnnot().empty() ) {
        return;
    }

    if ( id <= 0 ) {
        ERR_POST("Bioseq_set doesn't have integer id");
        dst.SetAnnot() = src.GetAnnot();
        return;
    }

    CBioseq_SplitInfo& info = m_Bioseqs[-id];
    if ( info.m_Id != 0 ) {
        ERR_POST("Several Bioseqs with the same gi: " << id);
        dst.SetAnnot() = src.GetAnnot();
        return;
    }
    
    _ASSERT(info.m_Id == 0);
    _ASSERT(!info.m_Bioseq);
    _ASSERT(!info.m_Bioseq_set);
    info.m_Id = -id;
    info.m_Bioseq_set.Reset(&dst);

    ITERATE ( CBioseq_set::TAnnot, it, src.GetAnnot() ) {
        if ( !CopyAnnot(info, **it) ) {
            dst.SetAnnot().push_back(Ref(&NonConst(**it)));
        }
    }
}


bool CBlobSplitterImpl::CopyAnnot(CBioseq_SplitInfo& bioseq_info,
                                  const CSeq_annot& annot)
{
    switch ( annot.GetData().Which() ) {
    case CSeq_annot::TData::e_Ftable:
    case CSeq_annot::TData::e_Align:
    case CSeq_annot::TData::e_Graph:
        break;
    default:
        ERR_POST(Info << "Bad type of Seq-annot: skipping.");
        return false;
    }

    CSeq_annot_SplitInfo& info = bioseq_info.m_Seq_annots[ConstRef(&annot)];
    info.SetSeq_annot(bioseq_info.m_Id, annot, m_Params);

    landmark_annot_count += info.m_LandmarkObjects.size();
    complex_annot_count += info.m_ComplexLocObjects.size();
    ITERATE ( TSimpleLocObjects, it, info.m_SimpleLocObjects ) {
        simple_annot_count += it->second.size();
    }

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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2004/05/21 21:42:14  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.7  2004/03/24 18:29:19  vasilche
* Fixed performance problem in verbose output.
*
* Revision 1.6  2004/03/05 17:40:34  vasilche
* Added 'verbose' option to splitter parameters.
*
* Revision 1.5  2004/01/22 20:10:42  vasilche
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
* Revision 1.4  2004/01/07 17:36:25  vasilche
* Moved id2_split headers to include/objmgr/split.
* Fixed include path to genbank.
*
* Revision 1.3  2003/12/17 15:19:37  vasilche
* Added missing return if Seq-annot cannot be split.
*
* Revision 1.2  2003/11/26 23:04:58  vasilche
* Removed extra semicolons after BEGIN_SCOPE and END_SCOPE.
*
* Revision 1.1  2003/11/12 16:18:29  vasilche
* First implementation of ID2 blob splitter withing cache.
*
* ===========================================================================
*/
