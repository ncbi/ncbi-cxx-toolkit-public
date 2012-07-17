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

#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqtable/Seq_table.hpp>

#include <objmgr/error_codes.hpp>
#include <objmgr/annot_selector.hpp>

#include <objmgr/split/asn_sizer.hpp>

#define NCBI_USE_ERRCODE_X   ObjMgr_ObjSplitInfo

#ifndef NCBI_ANNOT_TRACK_ZOOM_LEVEL_SUFFIX
# define NCBI_ANNOT_TRACK_ZOOM_LEVEL_SUFFIX "@@"
#endif

BEGIN_NCBI_SCOPE

NCBI_DEFINE_ERR_SUBCODE_X(10);

BEGIN_SCOPE(objects)

static CSafeStaticPtr<CAsnSizer> s_Sizer; // for size estimation

namespace {
    template<class C>
    string AsnText(const C& obj)
    {
        CNcbiOstrstream str;
        str << MSerial_AsnText << obj;
        return CNcbiOstrstreamToString(str);
    }

    template<class C>
    int AsnCompare(const C& obj1, const C& obj2)
    {
        string str1 = AsnText(obj1);
        string str2 = AsnText(obj2);
        return str1.compare(str2);
    }
}


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
    : m_TopPriority(eAnnotPriority_max),
      m_NamePriority(eAnnotPriority_max)
{
}


CAnnotName CSeq_annot_SplitInfo::GetName(const CSeq_annot& annot)
{
    string name;
    int version = -1;
    if ( annot.IsSetId() ) {
        const CSeq_annot::TId& ids = annot.GetId();
        ITERATE ( CSeq_annot::TId, it, ids ) {
            const CAnnot_id& id = **it;
            if ( id.IsOther() ) {
                const CTextannot_id& text_id = id.GetOther();
                if ( text_id.IsSetAccession() ) {
                    const string& acc = text_id.GetAccession();
                    if ( acc.empty() ) {
                        ERR_POST_X(1, "Empty named annot accession");
                    }
                    else if ( name.empty() ) {
                        name = acc;
                    }
                    else if ( name != acc ) {
                        ERR_POST_X(2, "Conflicting named annot accessions: "<<
                                   name<<" & "<<acc);
                    }
                }
                if ( text_id.IsSetVersion() ) {
                    int ver = text_id.GetVersion();
                    if ( ver < 0 ) {
                        ERR_POST_X(3, "Negative version: "<<ver);
                    }
                    else if ( version < 0 ) {
                        version = ver;
                    }
                    else if ( version != ver ) {
                        ERR_POST_X(4, "Conflicting named annot versions: "<<
                                   name<<": "<<version<<" & "<<ver);
                    }
                }
            }
        }
    }
    int zoom_level = -1;
    if ( annot.IsSetDesc() ) {
        const CSeq_annot::TDesc::Tdata& descs = annot.GetDesc().Get();
        ITERATE( CSeq_annot::TDesc::Tdata, it, descs ) {
            const CAnnotdesc& desc = **it;
            if ( desc.Which() == CAnnotdesc::e_Name ) {
                const string& s = desc.GetName();
                if ( s.empty() ) {
                    ERR_POST_X(5, "Empty annot name");
                }
                else if ( name.empty() ) {
                    name = s;
                }
                else if ( name != s ) {
                    ERR_POST_X(6, "Conflicting annot names: "<<
                               name<<" & "<<s);
                }
            }
            else if ( desc.Which() == CAnnotdesc::e_User ) {
                const CUser_object& user = desc.GetUser();
                const CObject_id& type = user.GetType();
                if ( !type.IsStr() || type.GetStr() != "AnnotationTrack" ) {
                    continue;
                }
                CConstRef<CUser_field> field = user.GetFieldRef("ZoomLevel");
                if ( field && field->GetData().IsInt() ) {
                    int level = field->GetData().GetInt();
                    if ( level < 0 ) {
                        ERR_POST_X(7, "Negative zoom level");
                    }
                    else if ( zoom_level < 0 ) {
                        zoom_level = level;
                    }
                    else if ( zoom_level != level ) {
                        ERR_POST_X(8, "Conflicting named annot zoom levels: "<<
                                   name<<": "<<zoom_level<<" & "<<level);
                    }
                }
            }
        }
    }
    if ( version >= 0 ) {
        if ( name.empty() ) {
            ERR_POST_X(9, "Named annot version with empty name");
        }
        else {
            name += "."+NStr::IntToString(version);
        }
    }
    if ( zoom_level >= 0 ) {
        if ( name.empty() ) {
            ERR_POST_X(10, "Named annot zoom level with empty name");
        }
        else {
            name += NCBI_ANNOT_TRACK_ZOOM_LEVEL_SUFFIX+NStr::IntToString(zoom_level);
        }
    }
    if ( name.empty() ) {
        return CAnnotName();
    }
    else {
        return CAnnotName(name);
    }
}


TAnnotPriority CSeq_annot_SplitInfo::GetPriority(void) const
{
    if ( m_NamePriority != eAnnotPriority_max ) {
        return m_NamePriority;
    }
    else if ( m_TopPriority != eAnnotPriority_max ) {
        return m_TopPriority;
    }
    else {
        return eAnnotPriority_skeleton;
    }
}


TAnnotPriority
CSeq_annot_SplitInfo::GetPriority(const CAnnotObject_SplitInfo& obj) const
{
    if ( m_NamePriority != eAnnotPriority_max ) {
        return m_NamePriority;
    }
    else {
        return obj.GetPriority();
    }
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
    case CSeq_annot::C_Data::e_Seq_table:
        return 1;
    default:
        _ASSERT("bad annot type" && 0);
    }
    return 0;
}


void CSeq_annot_SplitInfo::SetSeq_annot(const CSeq_annot& annot,
                                        const SSplitterParams& params,
                                        const CBlobSplitterImpl& impl)
{
    s_Sizer->Set(annot, params);
    m_Size = CSize(*s_Sizer);

    double ratio = m_Size.GetRatio();
    _ASSERT(!m_Src_annot);
    m_Src_annot.Reset(&annot);
    _ASSERT(!m_Name.IsNamed());
    m_Name = GetName(annot);
    switch ( annot.GetData().Which() ) {
    case CSeq_annot::TData::e_Ftable:
        ITERATE(CSeq_annot::C_Data::TFtable, it, annot.GetData().GetFtable()) {
            Add(CAnnotObject_SplitInfo(**it, impl, ratio));
        }
        break;
    case CSeq_annot::TData::e_Align:
        ITERATE(CSeq_annot::C_Data::TAlign, it, annot.GetData().GetAlign()) {
            Add(CAnnotObject_SplitInfo(**it, impl, ratio));
        }
        break;
    case CSeq_annot::TData::e_Graph:
        ITERATE(CSeq_annot::C_Data::TGraph, it, annot.GetData().GetGraph()) {
            Add(CAnnotObject_SplitInfo(**it, impl, ratio));
        }
        break;
    case CSeq_annot::TData::e_Seq_table:
        Add(CAnnotObject_SplitInfo(annot.GetData().GetSeq_table(), impl, ratio));
        break;
    default:
        _ASSERT("bad annot type" && 0);
    }
    if ( m_Name.IsNamed() ) {
        // named annotation should have at most regular priority
        m_NamePriority = max(m_TopPriority,
                             TAnnotPriority(eAnnotPriority_regular));
        // zoomed annotation have fixed priority
        SIZE_TYPE p = m_Name.GetName().find(NCBI_ANNOT_TRACK_ZOOM_LEVEL_SUFFIX);
        if ( p != NPOS ) {
            SIZE_TYPE pl = p+strlen(NCBI_ANNOT_TRACK_ZOOM_LEVEL_SUFFIX);
            int zoom_level = NStr::StringToInt(m_Name.GetName().substr(pl));
            if ( zoom_level > 0 ) {
                m_NamePriority = eAnnotPriority_zoomed + zoom_level;
            }
        }
    }
}


void CSeq_annot_SplitInfo::Add(const CAnnotObject_SplitInfo& obj)
{
    TAnnotPriority index = obj.GetPriority();
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
                                               const CBlobSplitterImpl& impl,
                                               double ratio)
    : m_ObjectType(CSeq_annot::C_Data::e_Ftable),
      m_Object(&obj),
      m_Size(s_Sizer->GetAsnSize(obj), ratio)
{
    m_Location.Add(obj, impl);
}


CAnnotObject_SplitInfo::CAnnotObject_SplitInfo(const CSeq_graph& obj,
                                               const CBlobSplitterImpl& impl,
                                               double ratio)
    : m_ObjectType(CSeq_annot::C_Data::e_Graph),
      m_Object(&obj),
      m_Size(s_Sizer->GetAsnSize(obj), ratio)
{
    m_Location.Add(obj, impl);
}


CAnnotObject_SplitInfo::CAnnotObject_SplitInfo(const CSeq_align& obj,
                                               const CBlobSplitterImpl& impl,
                                               double ratio)
    : m_ObjectType(CSeq_annot::C_Data::e_Align),
      m_Object(&obj),
      m_Size(s_Sizer->GetAsnSize(obj), ratio)
{
    m_Location.Add(obj, impl);
}


CAnnotObject_SplitInfo::CAnnotObject_SplitInfo(const CSeq_table& obj,
                                               const CBlobSplitterImpl& impl,
                                               double ratio)
    : m_ObjectType(CSeq_annot::C_Data::e_Seq_table),
      m_Object(&obj),
      m_Size(s_Sizer->GetAsnSize(obj), ratio)
{
    m_Location.Add(obj, impl);
}


TAnnotPriority CAnnotObject_SplitInfo::GetPriority(void) const
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


int CAnnotObject_SplitInfo::Compare(const CAnnotObject_SplitInfo& other) const
{
    if ( m_Object == other.m_Object ) {
        return 0;
    }
    if ( int cmp = m_ObjectType - other.m_ObjectType ) {
        return cmp;
    }
    if ( int cmp = m_Size.Compare(other.m_Size) ) {
        return cmp;
    }
    if ( m_ObjectType == CSeq_annot::C_Data::e_Ftable ) {
        const CSeq_feat& f1 = dynamic_cast<const CSeq_feat&>(*m_Object);
        const CSeq_feat& f2 = dynamic_cast<const CSeq_feat&>(*other.m_Object);
        if ( int cmp = f1.GetData().GetSubtype() - f2.GetData().GetSubtype() ) {
            return cmp;
        }
        if ( int cmp = AsnCompare(f1, f2) ) {
            return cmp;
        }
    }
    else if ( m_ObjectType == CSeq_annot::C_Data::e_Align ) {
        const CSeq_align& a1 = dynamic_cast<const CSeq_align&>(*m_Object);
        const CSeq_align& a2 = dynamic_cast<const CSeq_align&>(*other.m_Object);
        if ( int cmp = AsnCompare(a1, a2) ) {
            return cmp;
        }
    }
    else if ( m_ObjectType == CSeq_annot::C_Data::e_Graph ) {
        const CSeq_graph& g1 = dynamic_cast<const CSeq_graph&>(*m_Object);
        const CSeq_graph& g2 = dynamic_cast<const CSeq_graph&>(*other.m_Object);
        if ( int cmp = AsnCompare(g1, g2) ) {
            return cmp;
        }
    }
    else if ( m_ObjectType == CSeq_annot::C_Data::e_Seq_table ) {
        const CSeq_table& t1 = dynamic_cast<const CSeq_table&>(*m_Object);
        const CSeq_table& t2 = dynamic_cast<const CSeq_table&>(*other.m_Object);
        if ( int cmp = AsnCompare(t1, t2) ) {
            return cmp;
        }
    }
    return 0;
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
    s_Sizer->Set(seq, params);
    m_Size = CSize(*s_Sizer);
    m_Priority = eAnnotPriority_regular;
}


TAnnotPriority CBioseq_SplitInfo::GetPriority(void) const
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
    s_Sizer->Set(descr, params);
    m_Size = CSize(*s_Sizer);
    m_Priority = eAnnotPriority_regular;
}


TAnnotPriority CSeq_descr_SplitInfo::GetPriority(void) const
{
    return m_Priority;
}


int CSeq_descr_SplitInfo::Compare(const CSeq_descr_SplitInfo& other) const
{
    const CSeq_descr::Tdata& d1 = m_Descr->Get();
    const CSeq_descr::Tdata& d2 = other.m_Descr->Get();
    for ( CSeq_descr::Tdata::const_iterator i1(d1.begin()), i2(d2.begin());
          i1 != d1.end() || i2 != d2.end(); ++i1, ++i2 ) {
        if ( int cmp = (i1 != d1.end()) - (i2 != d2.end()) ) {
            return cmp;
        }
        if ( int cmp = (*i1)->Which() - (*i2)->Which() ) {
            return cmp;
        }
    }
    if ( int cmp = m_Size.Compare(other.m_Size) ) {
        return cmp;
    }
    if ( int cmp = AsnCompare(*m_Descr, *other.m_Descr) ) {
        return cmp;
    }
    return 0;
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
    s_Sizer->Set(hist, params);
    m_Size = CSize(*s_Sizer);
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
    s_Sizer->Set(align, params);
    m_Size = CSize(*s_Sizer);
    m_Priority = eAnnotPriority_low;
}


TAnnotPriority CSeq_hist_SplitInfo::GetPriority(void) const
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
    s_Sizer->Set(data, params);
    m_Size = CSize(*s_Sizer);
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


TAnnotPriority CSeq_data_SplitInfo::GetPriority(void) const
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
