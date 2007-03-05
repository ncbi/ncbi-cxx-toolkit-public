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
#include <objmgr/split/object_splitinfo.hpp>

#include <serial/serial.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_descr.hpp>

#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>

#include <objmgr/split/asn_sizer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static CAsnSizer s_Sizer; // for size estimation


/////////////////////////////////////////////////////////////////////////////
// CLocObjects_SplitInfo
/////////////////////////////////////////////////////////////////////////////


void CLocObjects_SplitInfo::Add(const CAnnotObject_SplitInfo& obj)
{
    m_Objects.push_back(obj);
    m_Location.Add(obj.m_Location);
    m_Size += obj.m_Size;
}


CNcbiOstream& CLocObjects_SplitInfo::Print(CNcbiOstream& out) const
{
    return out << m_Size;
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_SplitInfo
/////////////////////////////////////////////////////////////////////////////


CSeq_annot_SplitInfo::CSeq_annot_SplitInfo(void)
    : m_TopPriority(eAnnotPriority_max)
{
}


CSeq_annot_SplitInfo::CSeq_annot_SplitInfo(const CSeq_annot_SplitInfo& base,
                                           EAnnotPriority priority)
    : m_Src_annot(base.m_Src_annot),
      m_Name(base.m_Name),
      m_TopPriority(priority),
      m_Objects(priority+1)
{
    _ASSERT(unsigned(priority) < base.m_Objects.size());
    _ASSERT(base.m_Objects[priority]);
    const CLocObjects_SplitInfo& objs = *base.m_Objects[priority];
    m_Objects[priority] = new CLocObjects_SplitInfo(objs);
    m_Size = objs.m_Size;
    m_Location = objs.m_Location;
}


CAnnotName CSeq_annot_SplitInfo::GetName(const CSeq_annot& annot)
{
    CAnnotName ret;
    if ( annot.IsSetDesc() ) {
        string name;
        ITERATE ( CSeq_annot::TDesc::Tdata, it, annot.GetDesc().Get() ) {
            const CAnnotdesc& desc = **it;
            if ( desc.Which() == CAnnotdesc::e_Name ) {
                name = desc.GetName();
                break;
            }
        }
        ret.SetNamed(name);
    }
    return ret;
}


EAnnotPriority CSeq_annot_SplitInfo::GetPriority(void) const
{
    return m_TopPriority;
}


size_t CSeq_annot_SplitInfo::CountAnnotObjects(const CSeq_annot& annot)
{
    switch ( annot.GetData().Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
        return annot.GetData().GetFtable().size();
    case CSeq_annot::C_Data::e_Align:
        return annot.GetData().GetAlign().size();
    case CSeq_annot::C_Data::e_Graph:
        return annot.GetData().GetGraph().size();
    default:
        _ASSERT("bad annot type" && 0);
    }
    return 0;
}


void CSeq_annot_SplitInfo::SetSeq_annot(const CSeq_annot& annot,
                                        const SSplitterParams& params)
{
    s_Sizer.Set(annot, params);
    m_Size = CSize(s_Sizer);

    double ratio = m_Size.GetRatio();
    _ASSERT(!m_Src_annot);
    m_Src_annot.Reset(&annot);
    _ASSERT(!m_Name.IsNamed());
    m_Name = GetName(annot);
    switch ( annot.GetData().Which() ) {
    case CSeq_annot::TData::e_Ftable:
        ITERATE(CSeq_annot::C_Data::TFtable, it, annot.GetData().GetFtable()) {
            Add(CAnnotObject_SplitInfo(**it, ratio));
        }
        break;
    case CSeq_annot::TData::e_Align:
        ITERATE(CSeq_annot::C_Data::TAlign, it, annot.GetData().GetAlign()) {
            Add(CAnnotObject_SplitInfo(**it, ratio));
        }
        break;
    case CSeq_annot::TData::e_Graph:
        ITERATE(CSeq_annot::C_Data::TGraph, it, annot.GetData().GetGraph()) {
            Add(CAnnotObject_SplitInfo(**it, ratio));
        }
        break;
    default:
        _ASSERT("bad annot type" && 0);
    }
}


void CSeq_annot_SplitInfo::Add(const CAnnotObject_SplitInfo& obj)
{
    EAnnotPriority index = obj.GetPriority();
    m_TopPriority = min(m_TopPriority, index);
    m_Objects.resize(max(m_Objects.size(), index + size_t(1)));
    if ( !m_Objects[index] ) {
        m_Objects[index] = new CLocObjects_SplitInfo;
    }
    m_Objects[index]->Add(obj);
    m_Location.Add(obj.m_Location);
}


CNcbiOstream& CSeq_annot_SplitInfo::Print(CNcbiOstream& out) const
{
    string name;
    if ( m_Name.IsNamed() ) {
        name = " \"" + m_Name.GetName() + "\"";
    }
    out << "Seq-annot" << name << ":";

    size_t lines = 0;
    ITERATE ( TObjects, it, m_Objects ) {
        if ( !*it ) {
            continue;
        }
        out << "\nObjects" << (it-m_Objects.begin()) << ": " << **it;
        ++lines;
    }
    if ( lines > 1 ) {
        out << "\n   Total: " << m_Size;
    }
    return out << NcbiEndl;
}


/////////////////////////////////////////////////////////////////////////////
// CAnnotObject_SplitInfo
/////////////////////////////////////////////////////////////////////////////

CAnnotObject_SplitInfo::CAnnotObject_SplitInfo(const CSeq_feat& obj,
                                               double ratio)
    : m_ObjectType(CSeq_annot::C_Data::e_Ftable),
      m_Object(&obj),
      m_Size(s_Sizer.GetAsnSize(obj), ratio)
{
    m_Location.Add(obj);
}


CAnnotObject_SplitInfo::CAnnotObject_SplitInfo(const CSeq_graph& obj,
                                               double ratio)
    : m_ObjectType(CSeq_annot::C_Data::e_Graph),
      m_Object(&obj),
      m_Size(s_Sizer.GetAsnSize(obj), ratio)
{
    m_Location.Add(obj);
}


CAnnotObject_SplitInfo::CAnnotObject_SplitInfo(const CSeq_align& obj,
                                               double ratio)
    : m_ObjectType(CSeq_annot::C_Data::e_Align),
      m_Object(&obj),
      m_Size(s_Sizer.GetAsnSize(obj), ratio)
{
    m_Location.Add(obj);
}


EAnnotPriority CAnnotObject_SplitInfo::GetPriority(void) const
{
    if ( m_ObjectType != CSeq_annot::C_Data::e_Ftable ) {
        return eAnnotPriority_regular;
    }
    const CObject& annot = *m_Object;
    const CSeq_feat& feat = dynamic_cast<const CSeq_feat&>(annot);
    switch ( feat.GetData().GetSubtype() ) {
    case CSeqFeatData::eSubtype_gene:
    case CSeqFeatData::eSubtype_cdregion:
        return eAnnotPriority_landmark;
    case CSeqFeatData::eSubtype_variation:
        return eAnnotPriority_lowest;
    default:
        return eAnnotPriority_regular;
    }
}


/////////////////////////////////////////////////////////////////////////////
// CBioseq_SplitInfo
/////////////////////////////////////////////////////////////////////////////


CBioseq_SplitInfo::CBioseq_SplitInfo(const CBioseq& seq,
                                     const SSplitterParams& params)
    : m_Bioseq(&seq)
{
    m_Location.clear();
    ITERATE ( CBioseq::TId, it, seq.GetId() ) {
        m_Location.Add(CSeq_id_Handle::GetHandle(**it),
                       CSeqsRange::TRange::GetWhole());
    }
    s_Sizer.Set(seq, params);
    m_Size = CSize(s_Sizer);
    m_Priority = eAnnotPriority_regular;
}


EAnnotPriority CBioseq_SplitInfo::GetPriority(void) const
{
    return m_Priority;
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_SplitInfo
/////////////////////////////////////////////////////////////////////////////


CPlace_SplitInfo::CPlace_SplitInfo(void)
{
}


CPlace_SplitInfo::~CPlace_SplitInfo(void)
{
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_descr_SplitInfo
/////////////////////////////////////////////////////////////////////////////


CSeq_descr_SplitInfo::CSeq_descr_SplitInfo(const CPlaceId& place_id,
                                           TSeqPos seq_length,
                                           const CSeq_descr& descr,
                                           const SSplitterParams& params)
    : m_Descr(&descr)
{
    if ( place_id.IsBioseq() ) {
        m_Location.Add(place_id.GetBioseqId(), CRange<TSeqPos>::GetWhole());
    }
    else {
        _ASSERT(place_id.IsBioseq_set()); // it's either Bioseq or Bioseq_set
        // use dummy handle for Bioseq-sets
        m_Location.Add(CSeq_id_Handle(), CRange<TSeqPos>::GetWhole());
    }
    s_Sizer.Set(descr, params);
    m_Size = CSize(s_Sizer);
    m_Priority = eAnnotPriority_regular;
}


EAnnotPriority CSeq_descr_SplitInfo::GetPriority(void) const
{
    return m_Priority;
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_hist_SplitInfo
/////////////////////////////////////////////////////////////////////////////


CSeq_hist_SplitInfo::CSeq_hist_SplitInfo(const CPlaceId& place_id,
                                         const CSeq_hist& hist,
                                         const SSplitterParams& params)
{
    _ASSERT( hist.IsSetAssembly() );
    m_Assembly = hist.GetAssembly();
    _ASSERT( place_id.IsBioseq() );
    m_Location.Add(place_id.GetBioseqId(), CRange<TSeqPos>::GetWhole());
    s_Sizer.Set(hist, params);
    m_Size = CSize(s_Sizer);
    m_Priority = eAnnotPriority_low;
}


CSeq_hist_SplitInfo::CSeq_hist_SplitInfo(const CPlaceId& place_id,
                                         const CSeq_align& align,
                                         const SSplitterParams& params)
{
    CRef<CSeq_align> dst(&const_cast<CSeq_align&>(align));
    m_Assembly.push_back(dst);
    _ASSERT( place_id.IsBioseq() );
    m_Location.Add(place_id.GetBioseqId(), CRange<TSeqPos>::GetWhole());
    s_Sizer.Set(align, params);
    m_Size = CSize(s_Sizer);
    m_Priority = eAnnotPriority_low;
}


EAnnotPriority CSeq_hist_SplitInfo::GetPriority(void) const
{
    return m_Priority;
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_data_SplitInfo
/////////////////////////////////////////////////////////////////////////////


void CSeq_data_SplitInfo::SetSeq_data(const CPlaceId& place_id,
                                      const TRange& range,
                                      TSeqPos seq_length,
                                      const CSeq_data& data,
                                      const SSplitterParams& params)
{
    _ASSERT(place_id.IsBioseq()); // Seq-data is attribute of Bioseqs
    m_Location.clear();
    m_Location.Add(place_id.GetBioseqId(), range);
    m_Data.Reset(&data);
    s_Sizer.Set(data, params);
    m_Size = CSize(s_Sizer);
    m_Priority = eAnnotPriority_low;
    if ( seq_length <= 10000 ) {
        m_Priority = eAnnotPriority_regular;
    }
}


CSeq_data_SplitInfo::TRange CSeq_data_SplitInfo::GetRange(void) const
{
    _ASSERT(m_Location.size() == 1);
    return m_Location.begin()->second.GetTotalRange();
}


EAnnotPriority CSeq_data_SplitInfo::GetPriority(void) const
{
    return m_Priority;
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_inst_SplitInfo
/////////////////////////////////////////////////////////////////////////////


void CSeq_inst_SplitInfo::Add(const CSeq_data_SplitInfo& data)
{
    m_Seq_data.push_back(data);
}


END_SCOPE(objects)
END_NCBI_SCOPE
