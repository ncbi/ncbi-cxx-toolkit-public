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

#include <corelib/ncbiobj.hpp>

#include <objmgr/reader.hpp>
#include <objmgr/impl/snp_annot_info.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_point.hpp>
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
    "complex - has comment",
    "complex - location is not point",
    "complex - location is not gi",
    "complex - location gi is bad",
    "complex - location strand is bad",
    "complex - id count is too large",
    "complex - id count is not one",
    "complex - allele length is bad",
    "complex - allele count is too large",
    "complex - allele count is not two",
    "complex - weight has bad value",
    "complex - weight count is not one"
};


SSNP_Info::ESNP_Type SSNP_Info::ParseSeq_feat(const CSeq_feat& feat,
                                              CSeq_annot_SNP_Info& annot_info)
{
    const CSeq_loc& loc = feat.GetLocation();
    if ( loc.Which() != CSeq_loc::e_Pnt ) {
        return eSNP_Complex_LocationIsNotPoint;
    }

    const CSeq_point& point = loc.GetPnt();
    const CSeq_id& id = point.GetId();

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
    ITERATE ( CSeq_feat::TQual, it, qual ) {
        const CGb_qual& gb_qual = **it;
        const string& val = gb_qual.GetVal();
        if ( val.size() != 1 ) {
            return eSNP_Complex_AlleleLengthBad;
        }
        if ( alleles_count >= kMax_AllelesCount ) {
            return eSNP_Complex_AlleleCountTooLarge;
        }
        if ( (m_Alleles[alleles_count++] = val[0]) == 0 ) {
            return eSNP_Complex_AlleleLengthBad;
        }
    }
    if ( alleles_count != kMax_AllelesCount ) {
        return eSNP_Complex_AlleleCountIsNotTwo;
    }
    /*
    while ( alleles_count < kMax_AllelesCount ) {
        m_Alleles[alleles_count++] = 0;
    }
    */

    bool have_snp_id = false;
    ITERATE ( CSeq_feat::TDbxref, it, dbxref ) {
        if ( have_snp_id ) {
            return eSNP_Complex_IdCountTooLarge;
        }
        const CDbtag& dbtag = **it;
        if ( dbtag.GetDb() != "dbSNP" ) {
            return eSNP_Bad_WrongTextId;
        }
        const CObject_id& tag = dbtag.GetTag();
        if ( tag.Which() != CObject_id::e_Id ) {
            return eSNP_Bad_WrongMemberSet;
        }
        m_SNP_Id = tag.GetId();
        have_snp_id = true;
    }
    if ( !have_snp_id ) {
        return eSNP_Complex_IdCountIsNotOne;
    }
    
    if ( feat.IsSetId() || feat.IsSetPartial() || feat.IsSetExcept() ||
         feat.IsSetProduct() || feat.IsSetTitle() ||
         feat.IsSetCit() || feat.IsSetExp_ev() || feat.IsSetXref() ||
         feat.IsSetPseudo() || feat.IsSetExcept_text() ||
         imp_feat.IsSetLoc() || imp_feat.IsSetDescr() ||
         point.IsSetFuzz() || !point.IsSetStrand() ) {
        return eSNP_Bad_WrongMemberSet;
    }

    if ( imp_feat.GetKey() != "variation" ) {
        return eSNP_Bad_WrongTextId;
    }

    {{
        const CObject_id& id = ext.GetType();
        if ( id.Which() != CObject_id::e_Str ||
             id.GetStr() != "dbSnpSynonymyData" ) {
            return eSNP_Bad_WrongTextId;
        }
    }}

    bool have_weight = false;
    ITERATE ( CUser_object::TData, it, ext.GetData() ) {
        const CUser_field& field = **it;
        const CUser_field::TData& data = field.GetData();

        {{
            const CObject_id& id = field.GetLabel();
            if ( id.Which() != CObject_id::e_Str ||
                 id.GetStr() != "weight" ) {
                return eSNP_Bad_WrongTextId;
            }
        }}

        if ( have_weight ) {
            return eSNP_Complex_WeightCountIsNotOne;
        }
        if ( field.IsSetNum() || data.Which() != CUser_field::TData::e_Int ) {
            return eSNP_Complex_WeightBadValue;
        }
        int value = data.GetInt();
        if ( value < 0 || value > kMax_UI1 ) {
            return eSNP_Complex_WeightBadValue;
        }
        m_Weight = Uint8(value);
        have_weight = true;
    }
    if ( !have_weight ) {
        return eSNP_Complex_WeightCountIsNotOne;
    }

    m_DoublePosition = point.GetPoint() << 1;
    switch ( point.GetStrand() ) {
    case eNa_strand_plus:
        break;
    case eNa_strand_minus:
        m_DoublePosition |= 1;
        break;
    default:
        return eSNP_Complex_LocationStrandIsBad;
    }

    if ( feat.IsSetComment() ) {
        m_CommentIndex = annot_info.x_GetSNP_CommentIndex(feat.GetComment());
        if ( m_CommentIndex < 0 ) {
            return eSNP_Complex_HasComment;
        }
    }
    else {
        m_CommentIndex = -1;
    }

    if ( id.Which() != CSeq_id::e_Gi ) {
        return eSNP_Complex_LocationIsNotGi;
    }
    if ( !annot_info.x_SetSNP_Gi(id.GetGi()) ) {
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
            feat.SetData().SetImp().SetKey("variation");
        }
        { // allele
            CSeq_feat::TQual& qual = feat.SetQual();
            qual.resize(kMax_AllelesCount);
            for ( int i = 0; i < kMax_AllelesCount; ++i ) {
                qual[i].Reset(new CGb_qual);
                CGb_qual& gb_qual = *qual[i];
                gb_qual.SetQual("allele");
                gb_qual.SetVal(" ");
            }
        }
        { // weight
            CSeq_feat::TExt& ext = feat.SetExt();
            ext.SetType().SetStr("dbSnpSynonymyData");
            CSeq_feat::TExt::TData& data = ext.SetData();
            data.resize(1);
            data[0].Reset(new CUser_field);
            CUser_field& user_field = *data[0];
            user_field.SetLabel().SetStr("weight");
        }
        { // snpid
            CSeq_feat::TDbxref& dbxref = feat.SetDbxref();
            dbxref.resize(1);
            dbxref[0].Reset(new CDbtag);
            CDbtag& dbtag = *dbxref[0];
            dbtag.SetDb("dbSNP");
        }
    }
    return feat_ref;
}


void SSNP_Info::x_UpdateSeq_feat(CSeq_feat& feat,
                               const CSeq_annot_SNP_Info& annot_info) const
{
    { // comment
        if ( m_CommentIndex < 0 ) {
            feat.ResetComment();
            _ASSERT(!feat.IsSetComment());
        }
        else {
            feat.SetComment(annot_info.x_GetComment(m_CommentIndex));
            _ASSERT(feat.IsSetComment());
            _ASSERT(feat.GetComment().size() > 0);
        }
    }
    { // location
        CSeq_point& point = feat.SetLocation().SetPnt();
        point.SetPoint(GetPosition());
        point.SetStrand(MinusStrand()? eNa_strand_minus: eNa_strand_plus);
        point.SetId().SetGi(annot_info.GetGi());
    }
    { // allele
        CSeq_feat::TQual& qual = feat.SetQual();
        _ASSERT(qual.size() == kMax_AllelesCount);
        for ( int i = 0; i < kMax_AllelesCount; ++i ) {
            CGb_qual& gb_qual = *qual[i];
            _ASSERT(gb_qual.SetVal().size() == 1);
            gb_qual.SetVal()[0] = m_Alleles[i];
        }
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


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * $Log$
 * Revision 1.1  2003/08/14 20:05:19  vasilche
 * Simple SNP features are stored as table internally.
 * They are recreated when needed using CFeat_CI.
 *
 */
