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
 *  File Description: SNP Seq-annot info
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiobj.hpp>

#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/tse_info.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/objectio.hpp>
#include <serial/serial.hpp>
#include <serial/pack_string.hpp>

#include <algorithm>
#include <numeric>

// for debugging
#include <serial/objostrasn.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#undef EXACT_ALLELE_COUNT


/////////////////////////////////////////////////////////////////////////////
// SSNP_Info
/////////////////////////////////////////////////////////////////////////////

const char* const SSNP_Info::s_SNP_Type_Label[eSNP_Type_last] = {
    "simple",
    "bad - wrong member set",
    "bad - wrong text id",
    "complex - has comment",
    "complex - location is not point",
    "complex - location is not gi",
    "complex - location gi is bad",
    "complex - location strand is bad",
    "complex - id count is too large",
    "complex - id count is not one",
    "complex - allele length is bad",
    "complex - allele count is too large",
    "complex - allele count is non standard",
    "complex - weight has bad value",
    "complex - weight count is not one"
};


static const string kId_variation        ("variation");
static const string kId_allele           ("allele");
static const string kId_replace          ("replace");
static const string kId_dbSnpSynonymyData("dbSnpSynonymyData");
static const string kId_weight           ("weight");
static const string kId_dbSNP            ("dbSNP");


SSNP_Info::ESNP_Type SSNP_Info::ParseSeq_feat(const CSeq_feat& feat,
                                              CSeq_annot_SNP_Info& annot_info)
{
    m_Flags = 0;

    const CSeq_loc& loc = feat.GetLocation();
    const CSeqFeatData& data = feat.GetData();
    if ( data.Which() != CSeqFeatData::e_Imp ||
         !feat.IsSetQual() || !feat.IsSetExt() || !feat.IsSetDbxref() ) {
        return eSNP_Bad_WrongMemberSet;
    }
    
    const CImp_feat& imp_feat = data.GetImp();
    const CSeq_feat::TQual& qual = feat.GetQual();
    const CUser_object& ext = feat.GetExt();
    const CSeq_feat::TDbxref& dbxref = feat.GetDbxref();

    size_t alleles_count = 0;
    bool qual_allele = false;
    bool qual_replace = false;
    ITERATE ( CSeq_feat::TQual, it, qual ) {
        if ( alleles_count >= kMax_AllelesCount ) {
            return eSNP_Complex_AlleleCountTooLarge;
        }
        const CGb_qual& gb_qual = **it;
        const string& qual_str = gb_qual.GetQual();
        if ( qual_str == kId_allele ) {
            qual_allele = true;
        }
        else if ( qual_str == kId_replace ) {
            qual_replace = true;
        }
        else {
            return eSNP_Bad_WrongTextId;
        }
        TAlleleIndex allele_index =
            annot_info.x_GetAlleleIndex(gb_qual.GetVal());
        if ( allele_index == kNo_AlleleIndex ) {
            //NcbiCout << "Bad allele: \"" << gb_qual.GetVal() << "\"\n";
            return eSNP_Complex_AlleleLengthBad;
        }
        m_AllelesIndices[alleles_count++] = allele_index;
    }
    if ( qual_replace && qual_allele ) {
        return eSNP_Bad_WrongTextId;
    }
    if ( qual_replace ) {
        m_Flags |= fQualReplace;
    }
#ifdef EXACT_ALLELE_COUNT
    if ( alleles_count != kMax_AllelesCount ) {
        return eSNP_Complex_AlleleCountIsNonStandard;
    }
#else
    while ( alleles_count < kMax_AllelesCount ) {
        m_AllelesIndices[alleles_count++] = kNo_AlleleIndex;
    }
#endif

    bool have_snp_id = false;
    ITERATE ( CSeq_feat::TDbxref, it, dbxref ) {
        if ( have_snp_id ) {
            return eSNP_Complex_IdCountTooLarge;
        }
        const CDbtag& dbtag = **it;
        if ( dbtag.GetDb() != kId_dbSNP ) {
            return eSNP_Bad_WrongTextId;
        }
        const CObject_id& tag = dbtag.GetTag();
        switch ( tag.Which() ) {
        case CObject_id::e_Id:
            m_SNP_Id = tag.GetId();
            break;
        case CObject_id::e_Str:
            try {
                m_SNP_Id = NStr::StringToInt(tag.GetStr());
            }
            catch ( ... ) {
                return eSNP_Bad_WrongMemberSet;
            }
            break;
        default:
            return eSNP_Bad_WrongMemberSet;
        }
        have_snp_id = true;
    }
    if ( !have_snp_id ) {
        return eSNP_Complex_IdCountIsNotOne;
    }
    
    if ( feat.IsSetId() || feat.IsSetPartial() || feat.IsSetExcept() ||
         feat.IsSetProduct() || feat.IsSetTitle() ||
         feat.IsSetCit() || feat.IsSetExp_ev() || feat.IsSetXref() ||
         feat.IsSetPseudo() || feat.IsSetExcept_text() ||
         imp_feat.IsSetLoc() || imp_feat.IsSetDescr() ) {
        return eSNP_Bad_WrongMemberSet;
    }

    if ( imp_feat.GetKey() != kId_variation ) {
        return eSNP_Bad_WrongTextId;
    }

    {{
        const CObject_id& id = ext.GetType();
        if ( id.Which() != CObject_id::e_Str ||
             id.GetStr() != kId_dbSnpSynonymyData ) {
            return eSNP_Bad_WrongTextId;
        }
    }}

    bool have_weight = false;
    ITERATE ( CUser_object::TData, it, ext.GetData() ) {
        const CUser_field& field = **it;
        const CUser_field::TData& user_data = field.GetData();

        {{
            const CObject_id& id = field.GetLabel();
            if ( id.Which() != CObject_id::e_Str ||
                 id.GetStr() != kId_weight ) {
                return eSNP_Bad_WrongTextId;
            }
        }}

        if ( have_weight ) {
            return eSNP_Complex_WeightCountIsNotOne;
        }
        if ( field.IsSetNum() ||
             user_data.Which() != CUser_field::TData::e_Int ) {
            return eSNP_Complex_WeightBadValue;
        }
        int value = user_data.GetInt();
        if ( value < 0 || value > kMax_Weight ) {
            return eSNP_Complex_WeightBadValue;
        }
        m_Weight = TWeight(value);
        have_weight = true;
    }
    if ( !have_weight ) {
        return eSNP_Complex_WeightCountIsNotOne;
    }

    const CSeq_id* id;
    ENa_strand strand;
    switch ( loc.Which() ) {
    case CSeq_loc::e_Pnt:
    {
        const CSeq_point& point = loc.GetPnt();
        if ( point.IsSetFuzz() || !point.IsSetStrand() ) {
            return eSNP_Bad_WrongMemberSet;
        }
        id = &point.GetId();
        strand = point.GetStrand();
        m_ToPosition = point.GetPoint();
        m_PositionDelta = 0;
        break;
    }
    case CSeq_loc::e_Int:
    {
        const CSeq_interval& interval = loc.GetInt();
        if ( interval.IsSetFuzz_from() || interval.IsSetFuzz_to() ||
             !interval.IsSetStrand() ) {
            return eSNP_Bad_WrongMemberSet;
        }
        id = &interval.GetId();
        strand = interval.GetStrand();
        m_ToPosition = interval.GetTo();
        int delta = m_ToPosition - interval.GetFrom();
        if ( delta <= 0 || delta > kMax_PositionDelta ) {
            return eSNP_Complex_LocationIsNotPoint;
        }
        m_PositionDelta = TPositionDelta(delta);
        break;
    }
    default:
        return eSNP_Complex_LocationIsNotPoint;
    }

    switch ( strand ) {
    case eNa_strand_plus:
        break;
    case eNa_strand_minus:
        m_Flags |= fMinusStrand;
        break;
    default:
        return eSNP_Complex_LocationStrandIsBad;
    }

    if ( feat.IsSetComment() ) {
        m_CommentIndex = annot_info.x_GetCommentIndex(feat.GetComment());
        if ( m_CommentIndex == kNo_CommentIndex ) {
            return eSNP_Complex_HasComment;
        }
    }
    else {
        m_CommentIndex = kNo_CommentIndex;
    }

    if ( !id->IsGi() ) {
        return eSNP_Complex_LocationIsNotGi;
    }
    if ( !annot_info.x_CheckGi(id->GetGi()) ) {
        return eSNP_Complex_LocationGiIsBad;
    }

    return eSNP_Simple;
}


CRef<CSeq_feat> SSNP_Info::x_CreateSeq_feat(void) const
{
    CRef<CSeq_feat> feat_ref(new CSeq_feat);
    {
        CSeq_feat& feat = *feat_ref;
        { // data
            CPackString::Assign(feat.SetData().SetImp().SetKey(),
                                kId_variation);
        }
        { // weight
            CSeq_feat::TExt& ext = feat.SetExt();
            CPackString::Assign(ext.SetType().SetStr(),
                                kId_dbSnpSynonymyData);
            CSeq_feat::TExt::TData& data = ext.SetData();
            data.resize(1);
            data[0].Reset(new CUser_field);
            CUser_field& user_field = *data[0];
            CPackString::Assign(user_field.SetLabel().SetStr(),
                                kId_weight);
        }
        { // snpid
            CSeq_feat::TDbxref& dbxref = feat.SetDbxref();
            dbxref.resize(1);
            dbxref[0].Reset(new CDbtag);
            CDbtag& dbtag = *dbxref[0];
            CPackString::Assign(dbtag.SetDb(),
                                kId_dbSNP);
        }
    }
    return feat_ref;
}


void SSNP_Info::x_UpdateSeq_featData(CSeq_feat& feat,
                                     const CSeq_annot_SNP_Info& annot_info) const
{
    { // comment
        if ( m_CommentIndex == kNo_CommentIndex ) {
            feat.ResetComment();
        }
        else {
            CPackString::Assign(feat.SetComment(),
                                annot_info.x_GetComment(m_CommentIndex));
        }
    }
    { // allele
        CSeq_feat::TQual& qual = feat.SetQual();
        size_t i;
        const string& qual_str =
            m_Flags & fQualReplace? kId_replace: kId_allele;
        for ( i = 0; i < kMax_AllelesCount; ++i ) {
            TAlleleIndex allele_index = m_AllelesIndices[i];
            if ( allele_index == kNo_AlleleIndex ) {
                break;
            }
            CGb_qual* gb_qual;
            if ( i < qual.size() ) {
                gb_qual = qual[i].GetPointer();
            }
            else {
                qual.push_back(CRef<CGb_qual>(gb_qual = new CGb_qual));
                CPackString::Assign(gb_qual->SetQual(), qual_str);
            }
            CPackString::Assign(gb_qual->SetVal(),
                                annot_info.x_GetAllele(allele_index));
        }
        qual.resize(i);
    }
    { // weight
        CSeq_feat::TExt::TData& data = feat.SetExt().SetData();
        _ASSERT(data.size() == 1);
        CUser_field& user_field = *data[0];
        user_field.SetData().SetInt(m_Weight);
    }
    { // snpid
        CSeq_feat::TDbxref& dbxref = feat.SetDbxref();
        _ASSERT(dbxref.size() == 1);
        CDbtag& dbtag = *dbxref[0];
        dbtag.SetTag().SetId(m_SNP_Id);
    }
}


void SSNP_Info::x_UpdateSeq_feat(CSeq_feat& feat,
                                 CRef<CSeq_point>& seq_point,
                                 CRef<CSeq_interval>& seq_interval,
                                 const CSeq_annot_SNP_Info& annot_info) const
{
    x_UpdateSeq_featData(feat, annot_info);
    { // location
        TSeqPos to_position = m_ToPosition;
        TPositionDelta position_delta = m_PositionDelta;
        int gi = annot_info.GetGi();
        ENa_strand strand = MinusStrand()? eNa_strand_minus: eNa_strand_plus;
        if ( position_delta == 0 ) {
            // point
            feat.SetLocation().Reset();
            if ( !seq_point || !seq_point->ReferencedOnlyOnce() ) {
                seq_point.Reset(new CSeq_point);
            }
            CSeq_point& point = *seq_point;
            feat.SetLocation().SetPnt(point);
            point.SetPoint(to_position);
            point.SetStrand(strand);
            point.SetId().SetGi(gi);
        }
        else {
            // interval
            feat.SetLocation().Reset();
            if ( !seq_interval || !seq_interval->ReferencedOnlyOnce() ) {
                seq_interval.Reset(new CSeq_interval);
            }
            CSeq_interval& interval = *seq_interval;
            feat.SetLocation().SetInt(interval);
            interval.SetFrom(to_position-position_delta);
            interval.SetTo(to_position);
            interval.SetStrand(strand);
            interval.SetId().SetGi(gi);
        }
    }
}


void SSNP_Info::x_UpdateSeq_feat(CSeq_feat& feat,
                                 const CSeq_annot_SNP_Info& annot_info) const
{
    x_UpdateSeq_featData(feat, annot_info);
    { // location
        TSeqPos to_position = m_ToPosition;
        TPositionDelta position_delta = m_PositionDelta;
        int gi = annot_info.GetGi();
        ENa_strand strand = MinusStrand()? eNa_strand_minus: eNa_strand_plus;
        if ( position_delta == 0 ) {
            // point
            CSeq_point& point = feat.SetLocation().SetPnt();
            point.SetPoint(to_position);
            point.SetStrand(strand);
            point.SetId().SetGi(gi);
        }
        else {
            // interval
            CSeq_interval& interval = feat.SetLocation().SetInt();
            interval.SetFrom(to_position-position_delta);
            interval.SetTo(to_position);
            interval.SetStrand(strand);
            interval.SetId().SetGi(gi);
        }
    }
}


CRef<CSeq_feat>
SSNP_Info::CreateSeq_feat(const CSeq_annot_SNP_Info& annot_info) const
{
    CRef<CSeq_feat> feat_ref = x_CreateSeq_feat();
    x_UpdateSeq_feat(*feat_ref, annot_info);
    return feat_ref;
}


void SSNP_Info::UpdateSeq_feat(CRef<CSeq_feat>& feat_ref,
                               const CSeq_annot_SNP_Info& annot_info) const
{
    if ( !feat_ref || !feat_ref->ReferencedOnlyOnce() ) {
        feat_ref = x_CreateSeq_feat();
    }
    x_UpdateSeq_feat(*feat_ref, annot_info);
}


void SSNP_Info::UpdateSeq_feat(CRef<CSeq_feat>& feat_ref,
                               CRef<CSeq_point>& seq_point,
                               CRef<CSeq_interval>& seq_interval,
                               const CSeq_annot_SNP_Info& annot_info) const
{
    if ( !feat_ref || !feat_ref->ReferencedOnlyOnce() ) {
        feat_ref = x_CreateSeq_feat();
    }
    x_UpdateSeq_feat(*feat_ref, seq_point, seq_interval, annot_info);
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_SNP_Info
/////////////////////////////////////////////////////////////////////////////

CSeq_annot_SNP_Info::CSeq_annot_SNP_Info(void)
    : m_Gi(-1)
{
}


CSeq_annot_SNP_Info::CSeq_annot_SNP_Info(const CSeq_annot_SNP_Info& info)
    : m_Gi(info.m_Gi),
      m_SNP_Set(info.m_SNP_Set),
      m_Comments(info.m_Comments),
      m_Alleles(info.m_Alleles),
      m_Seq_annot(info.m_Seq_annot)
{
}


CSeq_annot_SNP_Info::~CSeq_annot_SNP_Info(void)
{
}


const CSeq_annot_Info& CSeq_annot_SNP_Info::GetParentSeq_annot_Info(void) const
{
    return static_cast<const CSeq_annot_Info&>(GetBaseParent_Info());
}


CSeq_annot_Info& CSeq_annot_SNP_Info::GetParentSeq_annot_Info(void)
{
    return static_cast<CSeq_annot_Info&>(GetBaseParent_Info());
}


const CSeq_entry_Info& CSeq_annot_SNP_Info::GetParentSeq_entry_Info(void) const
{
    return GetParentSeq_annot_Info().GetParentSeq_entry_Info();
}


CSeq_entry_Info& CSeq_annot_SNP_Info::GetParentSeq_entry_Info(void)
{
    return GetParentSeq_annot_Info().GetParentSeq_entry_Info();
}


void CSeq_annot_SNP_Info::x_ParentAttach(CSeq_annot_Info& parent)
{
    x_BaseParentAttach(parent);
}


void CSeq_annot_SNP_Info::x_ParentDetach(CSeq_annot_Info& parent)
{
    x_BaseParentDetach(parent);
}


void CSeq_annot_SNP_Info::x_UpdateAnnotIndexContents(CTSE_Info& tse)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetGiHandle(GetGi());
    tse.x_MapSNP_Table(GetParentSeq_annot_Info().GetName(), idh, *this);
    TParent::x_UpdateAnnotIndexContents(tse);
}


void CSeq_annot_SNP_Info::x_UnmapAnnotObjects(CTSE_Info& tse)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetGiHandle(GetGi());
    tse.x_UnmapSNP_Table(GetParentSeq_annot_Info().GetName(), idh, *this);
}


void CSeq_annot_SNP_Info::x_DropAnnotObjects(CTSE_Info& /*tse*/)
{
}


void CSeq_annot_SNP_Info::x_DoUpdateObject(void)
{
}


size_t CIndexedStrings::GetIndex(const string& s, size_t max_index)
{
    TIndices::iterator it = m_Indices.lower_bound(s);
    if ( it != m_Indices.end() && it->first == s ) {
        return it->second;
    }
    size_t index = m_Strings.size();
    if ( index <= max_index ) {
        //NcbiCout << "string["<<index<<"] = \"" << s << "\"\n";
        m_Strings.push_back(s);
        m_Indices.insert(it, TIndices::value_type(s, index));
    }
    else {
        _ASSERT(index == max_index + 1);
    }
    return index;
}


SSNP_Info::TAlleleIndex
CSeq_annot_SNP_Info::x_GetAlleleIndex(const string& allele)
{
    if ( allele.size() > SSNP_Info::kMax_AlleleLength )
        return SSNP_Info::kNo_AlleleIndex;
    if ( m_Alleles.IsEmpty() ) {
        // prefill by small alleles
        for ( const char* c = "-NACGT"; *c; ++c ) {
            m_Alleles.GetIndex(string(1, *c), SSNP_Info::kMax_AlleleIndex);
        }
        for ( const char* c1 = "ACGT"; *c1; ++c1 ) {
            string s(1, *c1);
            for ( const char* c2 = "ACGT"; *c2; ++c2 ) {
                m_Alleles.GetIndex(s+*c2, SSNP_Info::kMax_AlleleIndex);
            }
        }
    }
    return m_Alleles.GetIndex(allele, SSNP_Info::kMax_AlleleIndex);
}


void CSeq_annot_SNP_Info::x_SetGi(int gi)
{
    _ASSERT(m_Gi == -1);
    m_Gi = gi;
    _ASSERT(!m_Seq_id);
    m_Seq_id.Reset(new CSeq_id);
    m_Seq_id->SetGi(gi);
}


void CSeq_annot_SNP_Info::Reset(void)
{
    m_Gi = -1;
    m_Seq_id.Reset();
    m_Comments.Clear();
    m_Alleles.Clear();
    m_SNP_Set.clear();
    m_Seq_annot.Reset();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * $Log$
 * Revision 1.14  2004/06/30 20:53:33  vasilche
 * Added parsing string dbSNP id for SNP table.
 *
 * Revision 1.13  2004/05/21 21:42:13  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.12  2004/03/24 18:30:30  vasilche
 * Fixed edit API.
 * Every *_Info object has its own shallow copy of original object.
 *
 * Revision 1.11  2004/03/16 15:47:28  vasilche
 * Added CBioseq_set_Handle and set of EditHandles
 *
 * Revision 1.10  2004/02/06 16:13:20  vasilche
 * Added parsing "replace" as a synonym of "allele" in SNP qualifiers.
 * More compact format of SNP table in cache. SNP table version increased.
 * Fixed null pointer exception when SNP features are loaded from cache.
 *
 * Revision 1.9  2004/01/28 20:54:36  vasilche
 * Fixed mapping of annotations.
 *
 * Revision 1.8  2004/01/13 16:55:34  vasilche
 * CReader, CSeqref and some more classes moved from xobjmgr to separate lib.
 * Headers moved from include/objmgr to include/objtools/data_loaders/genbank.
 *
 * Revision 1.7  2003/11/26 17:56:00  vasilche
 * Implemented ID2 split in ID1 cache.
 * Fixed loading of splitted annotations.
 *
 * Revision 1.6  2003/10/21 14:27:35  vasilche
 * Added caching of gi -> sat,satkey,version resolution.
 * SNP blobs are stored in cache in preprocessed format (platform dependent).
 * Limit number of connections to GenBank servers.
 * Added collection of ID1 loader statistics.
 *
 * Revision 1.5  2003/09/30 16:22:04  vasilche
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
 * Revision 1.4  2003/08/27 14:29:53  vasilche
 * Reduce object allocations in feature iterator.
 *
 * Revision 1.3  2003/08/19 18:35:21  vasilche
 * CPackString classes were moved to SERIAL library.
 *
 * Revision 1.2  2003/08/15 19:19:16  vasilche
 * Fixed memory leak in string packing hooks.
 * Fixed processing of 'partial' flag of features.
 * Allow table packing of non-point SNP.
 * Allow table packing of SNP with long alleles.
 *
 * Revision 1.1  2003/08/14 20:05:19  vasilche
 * Simple SNP features are stored as table internally.
 * They are recreated when needed using CFeat_CI.
 *
 */
