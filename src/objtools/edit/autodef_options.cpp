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
* Author:  Colleen Bollin
*
* File Description:
*   Generate unique definition lines for a set of sequences using organism
*   descriptions and feature clauses.
*/

#include <ncbi_pch.hpp>
#include <objtools/edit/autodef_options.hpp>
#include <corelib/ncbimisc.hpp>

#include <objects/seqfeat/BioSource.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

            
CAutoDefOptions::CAutoDefOptions()
{
    x_Reset();
}


CRef<CUser_object> CAutoDefOptions::MakeUserObject() const
{
    CRef<CUser_object> user(new CUser_object());

    user->SetObjectType(CUser_object::eObjectType_AutodefOptions);

    for (unsigned int i = 0; i < eOptionFieldMax; i++) {
        if (x_IsBoolean(i) && m_BooleanFlags[i]) {
            CRef<CUser_field> field = x_MakeBooleanField(i);
            user->SetData().push_back(field);
        }
    }

    user->SetData().push_back(x_MakeMaxMods());
    user->SetData().push_back(x_MakeFeatureListType());
    user->SetData().push_back(x_MakeMiscFeatRule());
    user->SetData().push_back(x_MakeHIVRule());
    user->SetData().push_back(x_MakeProductFlag());
    user->SetData().push_back(x_MakeSuppressedFeatures());
    user->SetData().push_back(x_MakeModifierList());

    return user;
}


#define INITENUMFIELD(Fieldname) \
    } else if (field_type == eOptionFieldType_##Fieldname) { \
        if ((*it)->IsSetData() && (*it)->GetData().IsStr()) { \
            m_##Fieldname = x_Get##Fieldname((*it)->GetData().GetStr()); \
        }


void CAutoDefOptions::InitFromUserObject(const CUser_object& obj)
{
    x_Reset();

    ITERATE(CUser_object::TData, it, obj.GetData()) {
        if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr()) {}
        TFieldType field_type = x_GetFieldType((*it)->GetLabel().GetStr());
        if (field_type != eOptionFieldType_Unknown) {
            if (x_IsBoolean(field_type)) {
                if ((*it)->IsSetData() && (*it)->GetData().IsBool() && (*it)->GetData().GetBool()) {
                    m_BooleanFlags[field_type] = true;
                }
            INITENUMFIELD(FeatureListType)
            INITENUMFIELD(MiscFeatRule)
            INITENUMFIELD(HIVRule)
            INITENUMFIELD(ProductFlag)
            }
            else if (field_type == eOptionFieldType_SuppressedFeatures) {
                x_SetSuppressedFeatures(**it);
            } else if (field_type == eOptionFieldType_MaxMods) {
                if ((*it)->IsSetData() && (*it)->GetData().IsInt()) {
                    m_MaxMods = (*it)->GetData().GetInt();
                }
            } else if (field_type == eOptionFieldType_ModifierList) {
                x_SetModifierList(**it);
            }
        }
    }
}


void CAutoDefOptions::x_Reset()
{
    m_FeatureListType = eListAllFeatures;
    m_MiscFeatRule = eDelete;
    m_ProductFlag = CBioSource::eGenome_unknown;
    m_HIVRule = ePreferClone;

    ClearSuppressedFeatures();
    ClearModifierList();

    for (unsigned int i = 0; i < eOptionFieldMax; i++) {
        m_BooleanFlags[i] = false;
    }
}


typedef SStaticPair<const char *, unsigned int> TNameValPair;
const TNameValPair sc_FieldTypes[] = {
        { "AltSpliceFlag", CAutoDefOptions::eOptionFieldType_AltSpliceFlag },
        { "DoNotApplyToAff", CAutoDefOptions::eOptionFieldType_DoNotApplyToAff },
        { "DoNotApplyToCf", CAutoDefOptions::eOptionFieldType_DoNotApplyToCf },
        { "DoNotApplyToNr", CAutoDefOptions::eOptionFieldType_DoNotApplyToNr },
        { "DoNotApplyToSp", CAutoDefOptions::eOptionFieldType_DoNotApplyToSp },
        { "FeatureListType", CAutoDefOptions::eOptionFieldType_FeatureListType },
        { "GeneClusterOppStrand", CAutoDefOptions::eOptionFieldType_GeneClusterOppStrand },
        { "HIVRule", CAutoDefOptions::eOptionFieldType_HIVRule },
        { "IncludeCountryText", CAutoDefOptions::eOptionFieldType_IncludeCountryText },
        { "Keep3UTRs", CAutoDefOptions::eOptionFieldType_Keep3UTRs },
        { "Keep5UTRs", CAutoDefOptions::eOptionFieldType_Keep5UTRs },
        { "KeepAfterSemicolon", CAutoDefOptions::eOptionFieldType_KeepAfterSemicolon },
        { "KeepExons", CAutoDefOptions::eOptionFieldType_KeepExons },
        { "KeepIntrons", CAutoDefOptions::eOptionFieldType_KeepIntrons },
        { "KeepLTRs", CAutoDefOptions::eOptionFieldType_KeepLTRs },
        { "KeepPromoters", CAutoDefOptions::eOptionFieldType_KeepPromoters },
        { "LeaveParenthetical", CAutoDefOptions::eOptionFieldType_LeaveParenthetical },
        { "MaxMods", CAutoDefOptions::eOptionFieldType_MaxMods },
        { "MiscFeatRule", CAutoDefOptions::eOptionFieldType_MiscFeatRule },
        { "ModifierList", CAutoDefOptions::eOptionFieldType_ModifierList },
        { "ProductFlag", CAutoDefOptions::eOptionFieldType_ProductFlag },
        { "SpecifyNuclearProduct", CAutoDefOptions::eOptionFieldType_SpecifyNuclearProduct },
        { "SuppressedFeatures", CAutoDefOptions::eOptionFieldType_SuppressedFeatures },
        { "SuppressFeatureAltSplice", CAutoDefOptions::eOptionFieldType_SuppressFeatureAltSplice },
        { "SuppressLocusTags", CAutoDefOptions::eOptionFieldType_SuppressLocusTags },
        { "SuppressMobileElementSubfeatures", CAutoDefOptions::eOptionFieldType_SuppressMobileElementSubfeatures },
        { "UseFakePromoters", CAutoDefOptions::eOptionFieldType_UseFakePromoters },
        { "UseLabels", CAutoDefOptions::eOptionFieldType_UseLabels },
        { "UseNcRNAComment", CAutoDefOptions::eOptionFieldType_UseNcRNAComment }
};

typedef CStaticArrayMap<const char *, unsigned int, PNocase> TNameValPairMap;
DEFINE_STATIC_ARRAY_MAP_WITH_COPY(TNameValPairMap, sc_FieldTypeStrsMap, sc_FieldTypes);

const TNameValPair sc_FeatureListTypeStr[] = {
        { "Complete Genome", CAutoDefOptions::eCompleteGenome },
        { "Complete Sequence", CAutoDefOptions::eCompleteSequence },
        { "List All Features", CAutoDefOptions::eListAllFeatures },
        { "Partial Genome", CAutoDefOptions::ePartialGenome },
        { "Partial Sequence", CAutoDefOptions::ePartialSequence },
        { "Sequence", CAutoDefOptions::eSequence }
};
DEFINE_STATIC_ARRAY_MAP_WITH_COPY(TNameValPairMap, sc_FeatureListTypeStrsMap, sc_FeatureListTypeStr);

const TNameValPair sc_MiscFeatRuleStr[] = {
        { "CommentFeat", CAutoDefOptions::eCommentFeat },
        { "Delete", CAutoDefOptions::eDelete },
        { "NoncodingProductFeat", CAutoDefOptions::eNoncodingProductFeat }
};
DEFINE_STATIC_ARRAY_MAP_WITH_COPY(TNameValPairMap, sc_MiscFeatRuleStrsMap, sc_MiscFeatRuleStr);

const TNameValPair sc_HIVRuleStr[] = {
        { "PreferClone", CAutoDefOptions::ePreferClone },
        { "PreferIsolate", CAutoDefOptions::ePreferIsolate },
        { "WantBoth", CAutoDefOptions::eWantBoth }
};
DEFINE_STATIC_ARRAY_MAP_WITH_COPY(TNameValPairMap, sc_HIVRuleStrsMap, sc_HIVRuleStr);



#define AUTODEFGETENUM(Fieldname, Map, Default) \
string CAutoDefOptions::x_Get##Fieldname(unsigned int value) const \
{ \
    TNameValPairMap::const_iterator it = Map.begin(); \
        while (it != Map.end()) { \
        if (value == it->second) { \
            return it->first; \
        } \
        ++it; \
    } \
    return kEmptyStr; \
} \
CAutoDefOptions::T##Fieldname CAutoDefOptions::x_Get##Fieldname(const string& value) const \
{ \
    TNameValPairMap::const_iterator it = Map.find(value.c_str()); \
    if (it != Map.end()) { \
        return it->second; \
        } \
    return Default; \
}

AUTODEFGETENUM(FieldType, sc_FieldTypeStrsMap, eOptionFieldType_Unknown)
AUTODEFGETENUM(FeatureListType, sc_FeatureListTypeStrsMap, eListAllFeatures)
AUTODEFGETENUM(MiscFeatRule, sc_MiscFeatRuleStrsMap, eDelete);
AUTODEFGETENUM(HIVRule, sc_HIVRuleStrsMap, ePreferClone);

string CAutoDefOptions::x_GetProductFlag(CBioSource::TGenome value) const
{
    return CBioSource::GetOrganelleByGenome(value);
}


CBioSource::TGenome CAutoDefOptions::x_GetProductFlag(const string& value) const
{
    return CBioSource::GetGenomeByOrganelle(value);
}


bool CAutoDefOptions::x_IsBoolean(TFieldType field_type) const
{
    bool rval = true;
    switch (field_type)
    {
        case eOptionFieldType_HIVRule:
        case eOptionFieldType_FeatureListType:
        case eOptionFieldType_MiscFeatRule:
        case eOptionFieldType_ProductFlag:
        case eOptionFieldType_SuppressedFeatures:
        case eOptionFieldType_ModifierList:
            rval = false;
            break;
        default:
            rval = true;
            break;
    }
    return rval;
}


CRef<CUser_field> CAutoDefOptions::x_MakeBooleanField(TFieldType field_type) const
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(x_GetFieldType(field_type));
    field->SetData().SetBool(true);
    return field;
}


void CAutoDefOptions::SuppressAllFeatures()
{
    ClearSuppressedFeatures();
    m_SuppressedFeatureSubtypes.push_back(CSeqFeatData::eSubtype_any);
}


bool CAutoDefOptions::IsFeatureSuppressed(CSeqFeatData::ESubtype subtype) const
{
    ITERATE(TSuppressedFeatureSubtypes, it, m_SuppressedFeatureSubtypes) {
        if (subtype == *it || *it == CSeqFeatData::eSubtype_any) {
            return true;
        }
    }
    return false;
}

void CAutoDefOptions::SuppressFeature(CSeqFeatData::ESubtype subtype)
{
    m_SuppressedFeatureSubtypes.push_back(subtype);
}


void CAutoDefOptions::ClearSuppressedFeatures()
{
    m_SuppressedFeatureSubtypes.clear();
}


CRef<CUser_field> CAutoDefOptions::x_MakeSuppressedFeatures() const
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(x_GetFieldType(eOptionFieldType_SuppressedFeatures));

    ITERATE(TSuppressedFeatureSubtypes, it, m_SuppressedFeatureSubtypes) {
        if (*it == CSeqFeatData::eSubtype_any) {
            field->SetData().SetStr("All");
            return field;
        }
        field->SetData().SetStrs().push_back(CSeqFeatData::SubtypeValueToName(*it));
    }

    return field;
}


void CAutoDefOptions::x_SetSuppressedFeatures(const CUser_field& field)
{
    ClearSuppressedFeatures();

    if (!field.IsSetData()) {
        return;
    }
    if (field.GetData().IsStr() && NStr::EqualNocase(field.GetData().GetStr(), "All")) {
        m_SuppressedFeatureSubtypes.push_back(CSeqFeatData::eSubtype_any);
        return;
    }

    if (!field.GetData().IsStrs()){
        return;
    }

    ITERATE(CUser_field::TData::TStrs, v, field.GetData().GetStrs()) {
        CSeqFeatData::ESubtype val = CSeqFeatData::SubtypeNameToValue(*v);
        if (val != CSeqFeatData::eSubtype_bad) {
            m_SuppressedFeatureSubtypes.push_back(val);
        }
    }
}


void CAutoDefOptions::AddSubSource(CSubSource::TSubtype subtype)
{
    m_SubSources.push_back(subtype);
}


void CAutoDefOptions::AddOrgMod(COrgMod::TSubtype subtype)
{
    m_OrgMods.push_back(subtype);
}


void CAutoDefOptions::ClearModifierList()
{
    m_OrgMods.clear();
    m_SubSources.clear();
}


const string kSubSources = "SubSources";
const string kOrgMods = "OrgMods";

CRef<CUser_field> CAutoDefOptions::x_MakeModifierList() const
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(x_GetFieldType(eOptionFieldType_ModifierList));
    
    if (!m_SubSources.empty()) {
        CRef<CUser_field> subsources(new CUser_field());
        subsources->SetLabel().SetStr(kSubSources);
        ITERATE(TSubSources, it, m_SubSources) {
            string val = CSubSource::GetSubtypeName(*it);
            subsources->SetData().SetStrs().push_back(val);
        }
        field->SetData().SetFields().push_back(subsources);
    }

    if (!m_OrgMods.empty()) {
        CRef<CUser_field> orgmods(new CUser_field());
        orgmods->SetLabel().SetStr(kOrgMods);
        ITERATE(TOrgMods, it, m_OrgMods) {
            string val = COrgMod::GetSubtypeName(*it);
            orgmods->SetData().SetStrs().push_back(val);
        }
        field->SetData().SetFields().push_back(orgmods);
    }

    return field;
}


void CAutoDefOptions::x_SetModifierList(const CUser_field& field)
{
    ClearModifierList();

    if (!field.IsSetData() || !field.GetData().IsFields()) {
        return;
    }

    ITERATE(CUser_field::TData::TFields, it, field.GetData().GetFields()) {
        if ((*it)->IsSetLabel() && (*it)->GetLabel().IsStr() &&
            (*it)->IsSetData() && (*it)->GetData().IsStrs()) {
            if (NStr::EqualNocase((*it)->GetLabel().GetStr(), kSubSources)) {
                ITERATE(CUser_field::TData::TStrs, s, (*it)->GetData().GetStrs()) {
                    CSubSource::TSubtype subtype = CSubSource::GetSubtypeValue(*s);
                    m_SubSources.push_back(subtype);
                }
            } else if (NStr::EqualNocase((*it)->GetLabel().GetStr(), kOrgMods)) {
                ITERATE(CUser_field::TData::TStrs, s, (*it)->GetData().GetStrs()) {
                    COrgMod::TSubtype subtype = COrgMod::GetSubtypeValue(*s);
                    m_OrgMods.push_back(subtype);
                }
            }
        }
    }
}


CRef<CUser_field> CAutoDefOptions::x_MakeMaxMods() const
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr(x_GetFieldType(eOptionFieldType_MaxMods));
    field->SetData().SetInt(m_MaxMods);
    return field;
}


END_SCOPE(objects)
END_NCBI_SCOPE
