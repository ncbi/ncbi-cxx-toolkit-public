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

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>

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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//
//  CAnnotObject_Info::
//


CAnnotObject_Info::CAnnotObject_Info(void)
    : m_Annot_Info(0),
      m_Object(0),
      m_FeatSubtype(CSeqFeatData::eSubtype_any),
      m_FeatType(CSeqFeatData::e_not_set),
      m_AnnotType(CSeq_annot::C_Data::e_not_set),
      m_MultiId(0)
{
}


CAnnotObject_Info::CAnnotObject_Info(const CSeq_feat& feat,
                                     CSeq_annot_Info& annot)
    : m_Annot_Info(&annot),
      m_Object(&feat),
      m_FeatSubtype(feat.GetData().GetSubtype()),
      m_FeatType(feat.GetData().Which()),
      m_AnnotType(CSeq_annot::C_Data::e_Ftable),
      m_MultiId(0)
{
    _ASSERT(!IsChunkStub());
}


CAnnotObject_Info::CAnnotObject_Info(const CSeq_align& align,
                                     CSeq_annot_Info& annot)
    : m_Annot_Info(&annot),
      m_Object(&align),
      m_FeatSubtype(CSeqFeatData::eSubtype_any),
      m_FeatType(CSeqFeatData::e_not_set),
      m_AnnotType(CSeq_annot::C_Data::e_Align),
      m_MultiId(0)
{
    _ASSERT(!IsChunkStub());
}


CAnnotObject_Info::CAnnotObject_Info(const CSeq_graph& graph,
                                     CSeq_annot_Info& annot)
    : m_Annot_Info(&annot),
      m_Object(&graph),
      m_FeatSubtype(CSeqFeatData::eSubtype_any),
      m_FeatType(CSeqFeatData::e_not_set),
      m_AnnotType(CSeq_annot::C_Data::e_Graph),
      m_MultiId(0)
{
    _ASSERT(!IsChunkStub());
}


CAnnotObject_Info::CAnnotObject_Info(CTSE_Chunk_Info& chunk_info,
                                     const SAnnotTypeSelector& sel)
    : m_Annot_Info(0),
      m_Object(&chunk_info),
      m_FeatSubtype(sel.GetFeatSubtype()),
      m_FeatType(sel.GetFeatType()),
      m_AnnotType(sel.GetAnnotType()),
      m_MultiId(0)
{
    _ASSERT(IsChunkStub());
}


CAnnotObject_Info::~CAnnotObject_Info(void)
{
}


const CTSE_Chunk_Info& CAnnotObject_Info::GetChunk_Info(void) const
{
    _ASSERT(IsChunkStub());
    return *static_cast<const CTSE_Chunk_Info*>(m_Object.GetPointer());
}


void CAnnotObject_Info::GetMaps(vector<CHandleRangeMap>& hrmaps) const
{
    switch ( Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
    {
        const CSeq_feat& feat = *GetFeatFast();
        hrmaps.resize(feat.IsSetProduct()? 2: 1);
        hrmaps[0].clear();
        hrmaps[0].AddLocation(feat.GetLocation());
        if (hrmaps[0].GetMap().size() > 1) {
            m_MultiId |= fMultiId_Location;
        }
        if ( feat.IsSetProduct() ) {
            hrmaps[1].clear();
            hrmaps[1].AddLocation(feat.GetProduct());
            if (hrmaps[1].GetMap().size() > 1) {
                m_MultiId |= fMultiId_Product;
            }
        }
        break;
    }
    case CSeq_annot::C_Data::e_Graph:
    {
        const CSeq_graph& graph = *GetGraphFast();
        hrmaps.resize(1);
        hrmaps[0].clear();
        hrmaps[0].AddLocation(graph.GetLoc());
        if (hrmaps[0].GetMap().size() > 1) {
            m_MultiId |= fMultiId_Location;
        }
        break;
    }
    case CSeq_annot::C_Data::e_Align:
    {
        const CSeq_align& align = GetAlign();
        // TODO: separate alignment locations
        hrmaps.resize(1);
        hrmaps[0].clear();
        x_ProcessAlign(hrmaps[0], align);
        break;
    }
    default:
        break;
    }
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


const CSeq_feat& CAnnotObject_Info::GetFeat(void) const
{
    _ASSERT(!IsChunkStub() && IsFeat());
    const CObject& obj = *m_Object;
    return dynamic_cast<const CSeq_feat&>(obj);
}


const CSeq_align& CAnnotObject_Info::GetAlign(void) const
{
    _ASSERT(!IsChunkStub() && IsAlign());
    const CObject& obj = *m_Object;
    return dynamic_cast<const CSeq_align&>(obj);
}


const CSeq_graph& CAnnotObject_Info::GetGraph(void) const
{
    _ASSERT(!IsChunkStub() && IsGraph());
    const CObject& obj = *m_Object;
    return dynamic_cast<const CSeq_graph&>(obj);
}


void CAnnotObject_Info::x_ProcessAlign(CHandleRangeMap& hrmap,
                                       const CSeq_align& align) const
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
                CDense_diag::TIds::const_iterator it_id =
                    (*it)->GetIds().begin();
                CDense_diag::TStarts::const_iterator it_start =
                    (*it)->GetStarts().begin();
                TSeqPos len = (*it)->GetLen();
                CDense_diag::TStrands::const_iterator it_strand;
                if ( (*it)->IsSetStrands() ) {
                    it_strand = (*it)->GetStrands().begin();
                }
                while (it_id != (*it)->GetIds().end()) {
                    // Create CHandleRange from an align element
                    CSeq_loc loc;
                    loc.SetInt().SetId().Assign(**it_id);
                    loc.SetInt().SetFrom(*it_start);
                    loc.SetInt().SetTo(*it_start + len - 1);
                    if ( (*it)->IsSetStrands() ) {
                        loc.SetInt().SetStrand(*it_strand);
                        ++it_strand;
                    }
                    hrmap.AddLocation(loc);
                    ++it_id;
                    ++it_start;
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
            if (denseg.IsSetScores()
                && numseg != (int)denseg.GetScores().size()) {
                ERR_POST(Warning << "Invalid 'scores' size in denseg");
                numseg = min(numseg, (int)denseg.GetScores().size());
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
            CDense_seg::TStarts::const_iterator it_start =
                denseg.GetStarts().begin();
            CDense_seg::TLens::const_iterator it_len =
                denseg.GetLens().begin();
            CDense_seg::TStrands::const_iterator it_strand;
            if ( denseg.IsSetStrands() ) {
                it_strand = denseg.GetStrands().begin();
            }
            for (int seg = 0;  seg < numseg;  seg++, ++it_len) {
                CDense_seg::TIds::const_iterator it_id =
                    denseg.GetIds().begin();
                for (int seq = 0;  seq < dim;  seq++, ++it_start, ++it_id) {
                    ENa_strand strand = eNa_strand_unknown;
                    if ( denseg.IsSetStrands() ) {
                        strand = *it_strand;
                    }
                    ++it_strand;
                    if ( *it_start < 0 )
                        continue;
                    CSeq_loc loc;
                    loc.SetInt().SetId().Assign(**it_id);
                    loc.SetInt().SetFrom(*it_start);
                    loc.SetInt().SetTo(*it_start + *it_len - 1);
                    if ( denseg.IsSetStrands() ) {
                        loc.SetInt().SetStrand(strand);
                    }
                    hrmap.AddLocation(loc);
                }
            }
            break;
        }
    case CSeq_align::C_Segs::e_Std:
        {
            const CSeq_align::C_Segs::TStd& std =
                align.GetSegs().GetStd();
            ITERATE ( CSeq_align::C_Segs::TStd, it, std ) {
                //### Ignore Seq-id, assuming it is also set in Seq-loc?
                ITERATE ( CStd_seg::TLoc, it_loc, (*it)->GetLoc() ) {
                    // Create CHandleRange from an align element
                    hrmap.AddLocation(**it_loc);
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
            CPacked_seg::TStarts::const_iterator it_start =
                packed.GetStarts().begin();
            CPacked_seg::TLens::const_iterator it_len =
                packed.GetLens().begin();
            CPacked_seg::TPresent::const_iterator it_pres =
                packed.GetPresent().begin();
            CPacked_seg::TStrands::const_iterator it_strand;
            if ( packed.IsSetStrands() ) {
                it_strand = packed.GetStrands().begin();
                if (dim * numseg > (int)packed.GetStrands().size()) {
                    dim = packed.GetStrands().size() / numseg;
                }
            }
            for (int seg = 0;  seg < numseg;  seg++, ++it_len) {
                CPacked_seg::TIds::const_iterator it_id =
                    packed.GetIds().begin();
                for (int seq = 0;  seq < dim;  seq++, ++it_pres) {
                    if ( *it_pres ) {
                        CSeq_loc loc;
                        loc.SetInt().SetId().Assign(**it_id);
                        loc.SetInt().SetFrom(*it_start);
                        loc.SetInt().SetTo(*it_start + *it_len - 1);
                        if ( packed.IsSetStrands() ) {
                            loc.SetInt().SetStrand(*it_strand);
                            ++it_strand;
                        }
                        ++it_id;
                        ++it_start;
                        hrmap.AddLocation(loc);
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
                x_ProcessAlign(hrmap, **it);
            }
            break;
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
