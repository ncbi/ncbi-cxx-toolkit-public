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

#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>

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
//  SAnnotSelector
//

SAnnotSelector& SAnnotSelector::SetLimitNone(void)
{
    m_LimitObjectType = eLimit_None;
    m_LimitObject.Reset();
    return *this;
}


SAnnotSelector& SAnnotSelector::SetLimitTSE(const CSeq_entry* tse)
{
    m_LimitObjectType = tse ? eLimit_TSE : eLimit_None;
    m_LimitObject.Reset(tse);
    return *this;
}


SAnnotSelector& SAnnotSelector::SetLimitSeqEntry(const CSeq_entry* entry)
{
    m_LimitObjectType = entry ? eLimit_Entry : eLimit_None;
    m_LimitObject.Reset(entry);
    return *this;
}


SAnnotSelector& SAnnotSelector::SetLimitSeqAnnot(const CSeq_annot* annot)
{
    m_LimitObjectType = annot ? eLimit_Annot : eLimit_None;
    m_LimitObject.Reset(annot);
    return *this;
}


SAnnotSelector::SSources::SSources(void)
{
}


SAnnotSelector::SSources::~SSources(void)
{
}


SAnnotSelector& SAnnotSelector::SetDataSource(const string& source)
{
    if ( !m_Sources ) {
        m_Sources.Reset(new SSources);
    }
    m_Sources->m_Sources.insert(source);
    return *this;
}


////////////////////////////////////////////////////////////////////
//
//  CAnnotObject_Info::
//


CAnnotObject_Info::CAnnotObject_Info(void)
    : m_AnnotType(CSeq_annot::C_Data::e_not_set),
      m_FeatType(CSeqFeatData::e_not_set),
      m_FeatSubtype(CSeqFeatData::eSubtype_bad),
      m_Object(0),
      m_Seq_annot_Info(0)
{
}


CAnnotObject_Info::CAnnotObject_Info(const CAnnotObject_Info& info)
    : m_AnnotType(info.m_AnnotType),
      m_FeatType(info.m_FeatType),
      m_FeatSubtype(info.m_FeatSubtype),
      m_Object(info.m_Object),
      m_Seq_annot_Info(info.m_Seq_annot_Info)
{
}


const CAnnotObject_Info&
CAnnotObject_Info::operator=(const CAnnotObject_Info& info)
{
    m_AnnotType = info.m_AnnotType;
    m_FeatType = info.m_FeatType;
    m_FeatSubtype = info.m_FeatSubtype;
    m_Object = info.m_Object;
    m_Seq_annot_Info = info.m_Seq_annot_Info;
    return *this;
}


CAnnotObject_Info::CAnnotObject_Info(CSeq_feat& feat,
                                     CSeq_annot_Info& annot)
    : m_AnnotType(CSeq_annot::C_Data::e_Ftable),
      m_FeatType(feat.GetData().Which()),
      m_FeatSubtype(feat.GetData().GetSubtype()),
      m_Object(&feat),
      m_Seq_annot_Info(&annot)
{
}


CAnnotObject_Info::CAnnotObject_Info(CSeq_align& align,
                                     CSeq_annot_Info& annot)
    : m_AnnotType(CSeq_annot::C_Data::e_Align),
      m_FeatType(CSeqFeatData::e_not_set),
      m_FeatSubtype(CSeqFeatData::eSubtype_bad),
      m_Object(&align),
      m_Seq_annot_Info(&annot)
{
}


CAnnotObject_Info::CAnnotObject_Info(CSeq_graph& graph,
                                     CSeq_annot_Info& annot)
    : m_AnnotType(CSeq_annot::C_Data::e_Graph),
      m_FeatType(CSeqFeatData::e_not_set),
      m_FeatSubtype(CSeqFeatData::eSubtype_bad),
      m_Object(&graph),
      m_Seq_annot_Info(&annot)
{
}


CAnnotObject_Info::~CAnnotObject_Info(void)
{
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
        if ( feat.IsSetProduct() ) {
            hrmaps[1].clear();
            hrmaps[1].AddLocation(feat.GetProduct());
        }
        break;
    }
    case CSeq_annot::C_Data::e_Graph:
    {
        const CSeq_graph& graph = *GetGraphFast();
        hrmaps.resize(1);
        hrmaps[0].clear();
        hrmaps[0].AddLocation(graph.GetLoc());
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
    }
}


const CSeq_annot& CAnnotObject_Info::GetSeq_annot(void) const
{
    return GetSeq_annot_Info().GetSeq_annot();
}


const CSeq_entry& CAnnotObject_Info::GetSeq_entry(void) const
{
    return GetSeq_annot_Info().GetSeq_entry();
}


const CSeq_entry_Info& CAnnotObject_Info::GetSeq_entry_Info(void) const
{
    return GetSeq_annot_Info().GetSeq_entry_Info();
}


const CSeq_entry& CAnnotObject_Info::GetTSE(void) const
{
    return GetSeq_annot_Info().GetTSE();
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
#if defined(NCBI_COMPILER_ICC)
    return *dynamic_cast<const CSeq_feat*>(m_Object.GetPointer());
#else
    return dynamic_cast<const CSeq_feat&>(*m_Object);
#endif
}


const CSeq_align& CAnnotObject_Info::GetAlign(void) const
{
#if defined(NCBI_COMPILER_ICC)
    return *dynamic_cast<const CSeq_align*>(m_Object.GetPointer());
#else
    return dynamic_cast<const CSeq_align&>(*m_Object);
#endif
}


const CSeq_graph& CAnnotObject_Info::GetGraph(void) const
{
#if defined(NCBI_COMPILER_ICC)
    return *dynamic_cast<const CSeq_graph*>(m_Object.GetPointer());
#else
    return dynamic_cast<const CSeq_graph&>(*m_Object);
#endif
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
                    loc.SetInt().SetTo(*it_start + len);
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
                    if ( *it_start < 0 )
                        continue;
                    CSeq_loc loc;
                    loc.SetInt().SetId().Assign(**it_id);
                    loc.SetInt().SetFrom(*it_start);
                    loc.SetInt().SetTo(*it_start + *it_len);
                    if ( denseg.IsSetStrands() ) {
                        loc.SetInt().SetStrand(*it_strand);
                        ++it_strand;
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
                        loc.SetInt().SetTo(*it_start + *it_len);
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


void CAnnotObject_Info::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CAnnotObject_Info");
    CObject::DebugDump( ddc, depth);

    string choice;
    switch (m_AnnotType) {
    default:                            choice="unknown";   break;
    case CSeq_annot::C_Data::e_not_set: choice="notset";    break;
    case CSeq_annot::C_Data::e_Ftable:  choice="feature";   break;
    case CSeq_annot::C_Data::e_Align:   choice="alignment"; break;
    case CSeq_annot::C_Data::e_Graph:   choice="graph";     break;
    case CSeq_annot::C_Data::e_Ids:     choice="ids";       break;
    case CSeq_annot::C_Data::e_Locs:    choice="locs";      break;
    }
    ddc.Log("m_AnnotType", choice);
//    ddc.Log("m_DataSource", m_DataSource,0);
    ddc.Log("m_FeatType", long(m_FeatType));
    ddc.Log("m_Object", m_Object.GetPointer(),0);
//    ddc.Log("m_Annot", m_Annot.GetPointer(),0);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
