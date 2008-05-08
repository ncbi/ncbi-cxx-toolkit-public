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


/////////////////////////////////////////////////////////////////////////////
// SSNP_Info
/////////////////////////////////////////////////////////////////////////////

const char* const SSNP_Info::s_SNP_Type_Label[eSNP_Type_last] = {
    "simple",
    "bad - wrong member set",
    "bad - wrong text id",
    "complex - comment too big",
    "complex - comment index overflow",
    "complex - location is not point",
    "complex - location is not gi",
    "complex - location gi is bad",
    "complex - location strand is bad",
    "complex - id count is too large",
    "complex - id count is not one",
    "complex - allele length is too big",
    "complex - allele index overflow",
    "complex - allele count is too large",
    "complex - weight has bad value",
    "complex - weight count is not one",
    "complex - bad format of dbSnpQAdata",
    "complex - no place for dbSnpQAdata",
    "complex - dbSnpQAdata index overflow"
};


static const string kId_variation        ("variation");
static const string kId_allele           ("allele");
static const string kId_replace          ("replace");
static const string kId_dbSnpSynonymyData("dbSnpSynonymyData");
static const string kId_dbSnpQAdata      ("dbSnpQAdata");
static const string kId_weight           ("weight");
static const string kId_QualityCodes     ("QualityCodes");
static const string kVal_1               ("1");
static const string kId_dbSNP            ("dbSNP");

static const size_t kMax_CommentLength = 65530;
static const size_t kMax_AlleleLength  = 32;

size_t SSNP_Info::GetAllelesCount(void) const
{
    size_t count = 0;
    for (; count < kMax_AllelesCount; ++count) {
        if ( m_AllelesIndices[count] == kNo_AlleleIndex ) {
            break;
        }
    }
    if ( m_Flags & fQualityCodeMask ) {
        --count;
    }
    return count;
}


CUser_field::TData::E_Choice SSNP_Info::GetQualityCodeWhich(void) const
{
    if ( m_Flags & fQualityCodeStr ) {
        return CUser_field::TData::e_Str;
    }
    if ( m_Flags & fQualityCodeOs ) {
        return CUser_field::TData::e_Os;
    }
    return CUser_field::TData::e_not_set;
}


SSNP_Info::ESNP_Type SSNP_Info::ParseSeq_feat(const CSeq_feat& feat,
                                              CSeq_annot_SNP_Info& annot_info)
{
    m_Flags = 0;

    const CSeq_loc& loc = feat.GetLocation();
    const CSeqFeatData& data = feat.GetData();
    if ( data.Which() != CSeqFeatData::e_Imp ||
         !feat.IsSetQual() || !feat.IsSetDbxref() ) {
        return eSNP_Bad_WrongMemberSet;
    }
    
    const CImp_feat& imp_feat = data.GetImp();
    const CSeq_feat::TQual& qual = feat.GetQual();
    const CSeq_feat::TDbxref& dbxref = feat.GetDbxref();

    size_t alleles_count = 0;
    bool qual_allele = false;
    bool qual_replace = false;
    bool weight_qual = false;
    bool weight_ext = false;
    ITERATE ( CSeq_feat::TQual, it, qual ) {
        if ( alleles_count >= kMax_AllelesCount ) {
            return eSNP_Complex_AlleleCountTooLarge;
        }
        const CGb_qual& gb_qual = **it;
        const string& qual_id = gb_qual.GetQual();
        const string& qual_val = gb_qual.GetVal();
        if ( qual_id == kId_replace ) {
            qual_replace = true;
        }
        else if ( qual_id == kId_weight ) {
            if ( weight_qual ) {
                return eSNP_Complex_WeightCountIsNotOne;
            }
            weight_qual = true;
            m_Flags |= fWeightQual;
            if ( qual_val == kVal_1 ) {
                m_Weight = 1;
            }
            else {
                try {
                    int value = NStr::StringToInt(qual_val);
                    if ( value < 0 || value > kMax_Weight ) {
                        return eSNP_Complex_WeightBadValue;
                    }
                    m_Weight = TWeight(value);
                }
                catch ( ... ) {
                    return eSNP_Complex_WeightBadValue;
                }
            }
            continue;
        }
        else if ( qual_id == kId_allele ) {
            qual_allele = true;
        }
        else {
            return eSNP_Bad_WrongTextId;
        }
        if ( qual_val.size() > kMax_AlleleLength ) {
            return eSNP_Complex_AlleleTooBig;
        }
        TAlleleIndex allele_index = annot_info.x_GetAlleleIndex(qual_val);
        if ( allele_index == kNo_AlleleIndex ) {
            return eSNP_Complex_AlleleIndexOverflow;
        }
        m_AllelesIndices[alleles_count++] = allele_index;
    }
    if ( qual_replace && qual_allele ) {
        return eSNP_Bad_WrongTextId;
    }
    if ( qual_replace ) {
        m_Flags |= fQualReplace;
    }
    for ( size_t i = alleles_count; i < kMax_AllelesCount; ++i ) {
        m_AllelesIndices[i] = kNo_AlleleIndex;
    }

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

    if ( feat.IsSetExt() ) {
        const CUser_object& ext = feat.GetExt();
        const CObject_id& ext_id = ext.GetType();
        if ( ext_id.Which() != CObject_id::e_Str ) {
            return eSNP_Bad_WrongTextId;
        }
        const string& ext_id_str = ext_id.GetStr();
        if ( ext_id_str == kId_dbSnpSynonymyData ) {
            if ( weight_qual ) {
                return eSNP_Complex_WeightCountIsNotOne;
            }

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

                if ( weight_ext ) {
                    return eSNP_Complex_WeightCountIsNotOne;
                }
                weight_ext = true;
                m_Flags |= fWeightExt;
                if ( field.IsSetNum() ||
                     user_data.Which() != CUser_field::TData::e_Int ) {
                    return eSNP_Complex_WeightBadValue;
                }
                int value = user_data.GetInt();
                if ( value < 0 || value > kMax_Weight ) {
                    return eSNP_Complex_WeightBadValue;
                }
                m_Weight = TWeight(value);
            }
        }
        else if ( ext_id_str == kId_dbSnpQAdata ) {
            if ( alleles_count == kMax_AllelesCount ||
                 (m_Flags & fQualityCodeMask) ) {
                return eSNP_Complex_NoPlaceForQAdata;
            }
            
            const CUser_object::TData& data = ext.GetData();
            if ( data.empty() ) {
                return eSNP_Complex_BadQAdata;
            }
            CUser_object::TData::const_iterator it = data.begin();
            const CUser_field& field = **it;
            {{
                const CObject_id& id = field.GetLabel();
                if ( id.Which() != CObject_id::e_Str ||
                     id.GetStr() != kId_QualityCodes ) {
                    return eSNP_Bad_WrongTextId;
                }
            }}
            const CUser_field::TData& user_data = field.GetData();
            TFlags qaflag;
            TQualityIndex qaindex;
            switch ( user_data.Which() ) {
            case CUser_field::TData::e_Str:
                qaflag = fQualityCodeStr;
                qaindex = annot_info.x_GetQualityIndex(user_data.GetStr());
                break;
            case CUser_field::TData::e_Os:
                qaflag = fQualityCodeOs;
                qaindex = annot_info.x_GetQualityIndex(user_data.GetOs());
                break;
            default:
                return eSNP_Complex_BadQAdata;
            }
            if ( qaindex == kNo_QualityIndex || ++it != data.end() ) {
                return eSNP_Complex_QAdataIndexOverflow;
            }
            m_Flags |= qaflag;
            m_AllelesIndices[alleles_count++] = qaindex;
        }
        else {
            return eSNP_Bad_WrongTextId;
        }
    }
    const CSeq_id* id;
    ENa_strand strand;
    switch ( loc.Which() ) {
    case CSeq_loc::e_Pnt:
    {
        const CSeq_point& point = loc.GetPnt();
        if ( !point.IsSetStrand() ) {
            return eSNP_Bad_WrongMemberSet;
        }
        if ( point.IsSetFuzz() ) {
            const CInt_fuzz& fuzz = point.GetFuzz();
            if ( !fuzz.IsLim() || fuzz.GetLim() != CInt_fuzz::eLim_tr ) {
                return eSNP_Bad_WrongMemberSet;
            }
            m_Flags |= fFuzzLimTr;
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
        const string& comment = feat.GetComment();
        if ( comment.size() > kMax_CommentLength ) {
            return eSNP_Complex_CommentTooBig;
        }
        m_CommentIndex = annot_info.x_GetCommentIndex(feat.GetComment());
        if ( m_CommentIndex == kNo_CommentIndex ) {
            return eSNP_Complex_CommentIndexOverflow;
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
        if ( 0 ) { // data
            CPackString::Assign(feat.SetData().SetImp().SetKey(),
                                kId_variation);
        }
        if ( 0 ) { // weight - will create in x_UpdateSeq_featData
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
        if ( 0 ) { // snpid - will create in x_UpdateSeq_featData
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
    CPackString::Assign(feat.SetData().SetImp().SetKey(),
                        kId_variation);
    { // comment
        if ( m_CommentIndex == kNo_CommentIndex ) {
            feat.ResetComment();
        }
        else {
            CPackString::Assign(feat.SetComment(),
                                annot_info.x_GetComment(m_CommentIndex));
        }
    }
    size_t alleles_count = 0;
    while ( alleles_count < kMax_AllelesCount &&
            m_AllelesIndices[alleles_count] != kNo_AlleleIndex ) {
        ++alleles_count;
    }
    
    if ( m_Flags & fQualityCodeMask ) {
        _ASSERT(alleles_count > 0);
        --alleles_count;
    }
    
    { // allele
        CSeq_feat::TQual& qual = feat.SetQual();
        const string& qual_str =
            m_Flags & fQualReplace? kId_replace: kId_allele;
        
        size_t qual_index = 0;
        for ( size_t i = 0; i < alleles_count; ++i ) {
            TAlleleIndex allele_index = m_AllelesIndices[i];
            CGb_qual* gb_qual;
            if ( qual_index < qual.size() ) {
                gb_qual = qual[qual_index].GetPointer();
            }
            else {
                qual.push_back(CRef<CGb_qual>(gb_qual = new CGb_qual));
            }
            ++qual_index;
            CPackString::Assign(gb_qual->SetQual(), qual_str);
            CPackString::Assign(gb_qual->SetVal(),
                                annot_info.x_GetAllele(allele_index));
        }

        if ( m_Flags & fWeightQual ) { // weight in qual
            CGb_qual* gb_qual;
            if ( qual_index < qual.size() ) {
                gb_qual = qual[qual_index].GetPointer();
            }
            else {
                qual.push_back(CRef<CGb_qual>(gb_qual = new CGb_qual));
            }
            ++qual_index;
            CPackString::Assign(gb_qual->SetQual(), kId_weight);
            if ( m_Weight == 1 ) {
                CPackString::Assign(gb_qual->SetVal(), kVal_1);
            }
            else {
                gb_qual->SetVal(NStr::IntToString(m_Weight));
            }
        }
        qual.resize(qual_index);
    }

    if ( m_Flags & fWeightExt ) {
        // weight in ext
        CSeq_feat::TExt& ext = feat.SetExt();
        CPackString::Assign(ext.SetType().SetStr(), kId_dbSnpSynonymyData);
        CSeq_feat::TExt::TData& data = ext.SetData();
        data.resize(1);
        data[0].Reset(new CUser_field);
        CUser_field& user_field = *data[0];
        CPackString::Assign(user_field.SetLabel().SetStr(),
                            kId_weight);
        user_field.SetData().SetInt(m_Weight);
    }
    else if ( m_Flags & fQualityCodeStr ) {
        // qadata in ext
        CSeq_feat::TExt& ext = feat.SetExt();
        CPackString::Assign(ext.SetType().SetStr(), kId_dbSnpQAdata);
        CSeq_feat::TExt::TData& data = ext.SetData();
        data.resize(1);
        data[0].Reset(new CUser_field);
        CUser_field& user_field = *data[0];
        CPackString::Assign(user_field.SetLabel().SetStr(), kId_QualityCodes);
        TQualityIndex qadata_index = m_AllelesIndices[alleles_count];
        CPackString::Assign(user_field.SetData().SetStr(),
                            annot_info.x_GetQuality(qadata_index));
    }
    else if ( m_Flags & fQualityCodeOs ) {
        // qadata in ext
        CSeq_feat::TExt& ext = feat.SetExt();
        CPackString::Assign(ext.SetType().SetStr(), kId_dbSnpQAdata);
        CSeq_feat::TExt::TData& data = ext.SetData();
        data.resize(1);
        data[0].Reset(new CUser_field);
        CUser_field& user_field = *data[0];
        CPackString::Assign(user_field.SetLabel().SetStr(), kId_QualityCodes);
        TQualityIndex qadata_index = m_AllelesIndices[alleles_count];
        annot_info.x_GetQuality(qadata_index, user_field.SetData().SetOs());
    }
    else {
        feat.ResetExt();
    }

    { // snpid
        CSeq_feat::TDbxref& dbxref = feat.SetDbxref();
        dbxref.resize(1);
        if ( !dbxref.front() ) {
            dbxref.front().Reset(new CDbtag);
        }
        CDbtag& dbtag = *dbxref[0];
        CPackString::Assign(dbtag.SetDb(),
                            kId_dbSNP);
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
            if ( m_Flags & fFuzzLimTr ) {
                point.SetFuzz().SetLim(CInt_fuzz::eLim_tr);
            }
            else {
                point.ResetFuzz();
            }
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
            if ( m_Flags & fFuzzLimTr ) {
                point.SetFuzz().SetLim(CInt_fuzz::eLim_tr);
            }
            else {
                point.ResetFuzz();
            }
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


CSeq_annot_SNP_Info::CSeq_annot_SNP_Info(CSeq_annot& annot)
    : m_Gi(-1), m_Seq_annot(&annot)
{
}


CSeq_annot_SNP_Info::CSeq_annot_SNP_Info(const CSeq_annot_SNP_Info& info)
    : m_Gi(info.m_Gi),
      m_Seq_id(info.m_Seq_id),
      m_SNP_Set(info.m_SNP_Set),
      m_Comments(info.m_Comments),
      m_Alleles(info.m_Alleles),
      m_QualityStr(info.m_QualityStr),
      m_QualityOs(info.m_QualityOs),
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


void CSeq_annot_SNP_Info::x_DoUpdate(TNeedUpdateFlags flags)
{
    TParent::x_DoUpdate(flags);
}


CIndexedStrings::CIndexedStrings(void)
{
}


CIndexedStrings::CIndexedStrings(const CIndexedStrings& ss)
    : m_Strings(ss.m_Strings)
{
}


void CIndexedStrings::ClearIndices(void)
{
    m_Indices.reset();
}


void CIndexedStrings::Clear(void)
{
    ClearIndices();
    m_Strings.clear();
}


void CIndexedStrings::Resize(size_t new_size)
{
    m_Indices.reset();
    m_Strings.resize(new_size);
}


size_t CIndexedStrings::GetIndex(const string& s, size_t max_index)
{
    if ( !m_Indices.get() ) {
        m_Indices.reset(new TIndices);
        for ( size_t i = 0; i < m_Strings.size(); ++i ) {
            m_Indices->insert(TIndices::value_type(m_Strings[i], i));
        }
    }
    TIndices::iterator it = m_Indices->lower_bound(s);
    if ( it != m_Indices->end() && it->first == s ) {
        return it->second;
    }
    size_t index = m_Strings.size();
    if ( index <= max_index ) {
        m_Strings.push_back(s);
        m_Indices->insert(it, TIndices::value_type(m_Strings.back(), index));
    }
    return index;
}


CIndexedOctetStrings::CIndexedOctetStrings(const CIndexedOctetStrings& ss)
    : m_ElementSize(ss.m_ElementSize), m_Strings(ss.m_Strings)
{
}


CIndexedOctetStrings::CIndexedOctetStrings(void)
    : m_ElementSize(0)
{
}


void CIndexedOctetStrings::ClearIndices(void)
{
    m_Indices.reset();
    if ( m_Strings.capacity() > m_Strings.size() + 32 ) {
        TOctetString s(m_Strings);
        s.swap(m_Strings);
    }
}


void CIndexedOctetStrings::Clear(void)
{
    m_Indices.reset();
    m_Strings.clear();
}


size_t CIndexedOctetStrings::GetIndex(const TOctetString& os, size_t max_index)
{
    size_t size = os.size();
    if ( size == 0 ) {
        return max_index+1;
    }
    if ( size != m_ElementSize ) {
        if ( m_ElementSize != 0 ) {
            return max_index+1;
        }
        m_ElementSize = size;
    }
    if ( !m_Indices.get() ) {
        _ASSERT(m_Strings.size() % size == 0);
        m_Indices.reset(new TIndices);
        m_Strings.reserve(size*(max_index+1));
        for ( size_t i = 0; i*size < m_Strings.size(); ++i ) {
            m_Indices->insert(TIndices::value_type
                              (CTempString(&m_Strings[i*size], size), i));
        }
    }
    CTempString s(&os[0], size);
    TIndices::iterator it = m_Indices->lower_bound(s);
    if ( it != m_Indices->end() && it->first == s ) {
        return it->second;
    }
    size_t pos = m_Strings.size();
    if ( pos > max_index*size ) {
        return max_index+1;
    }
    size_t index = pos/size;
    m_Strings.insert(m_Strings.end(), os.begin(), os.end());
    m_Indices->insert(TIndices::value_type
                      (CTempString(&m_Strings[pos], size), index));
    return index;
}


void CIndexedOctetStrings::GetString(size_t index, TOctetString& s) const
{
    size_t size = m_ElementSize;
    size_t pos = index*size;
    s.assign(m_Strings.begin()+pos, m_Strings.begin()+(pos+size));
}


void CIndexedOctetStrings::SetTotalString(size_t element_size,
                                          TOctetString& s)
{
    m_Indices.reset();
    m_ElementSize = element_size;
    m_Strings.swap(s);
}


SSNP_Info::TAlleleIndex
CSeq_annot_SNP_Info::x_GetAlleleIndex(const string& allele)
{
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


void CSeq_annot_SNP_Info::x_FinishParsing(void)
{
    // we don't need index maps anymore
    m_Comments.ClearIndices();
    m_Alleles.ClearIndices();
    m_QualityStr.ClearIndices();
    m_QualityOs.ClearIndices();
    
    sort(m_SNP_Set.begin(), m_SNP_Set.end());
    
    x_SetDirtyAnnotIndex();
}


void CSeq_annot_SNP_Info::Reset(void)
{
    m_Gi = -1;
    m_Seq_id.Reset();
    m_Comments.Clear();
    m_Alleles.Clear();
    m_QualityStr.Clear();
    m_QualityOs.Clear();
    m_SNP_Set.clear();
    m_Seq_annot.Reset();
}


END_SCOPE(objects)
END_NCBI_SCOPE
