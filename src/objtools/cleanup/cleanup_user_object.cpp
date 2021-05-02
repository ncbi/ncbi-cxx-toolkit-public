/* $Id$
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
 *   Code for cleaning up user objects
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>
#include <objects/general/User_object.hpp>
#include <objects/valid/Comment_rule.hpp>
#include <objects/valid/Comment_set.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include "cleanup_utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


bool CCleanup::CleanupUserObject( CUser_object &user_object )
{
    bool any_change = false;

    // clean type str
    if( user_object.IsSetType() && user_object.GetType().IsStr() ) {
        any_change |= CleanVisString (user_object.SetType().SetStr());
    }

    // clean fields
    if (user_object.IsSetData()) {
        for (auto field : user_object.SetData()) {
            any_change |= x_CleanupUserField(*field);
        }
    }

    any_change |= s_CleanupGeneOntology(user_object);
    any_change |= s_CleanupStructuredComment(user_object);
    any_change |= s_CleanupDBLink(user_object);

    return any_change;
}


bool CCleanup::x_CleanupUserField(CUser_field& field)
{
    bool any_change = false;

    if (field.IsSetLabel() && field.GetLabel().IsStr()) {
        any_change |= CleanVisString(field.SetLabel().SetStr());
    }

    if (field.IsSetData()) {
        any_change |= s_AddNumToUserField(field);
        switch (field.GetData().Which()) {
        case CUser_field::TData::e_Str:
            any_change |= Asn2gnbkCompressSpaces(field.SetData().SetStr());
            any_change |= CleanVisString(field.SetData().SetStr());
            break;
        case CUser_field::TData::e_Object:
            any_change |= CleanupUserObject(field.SetData().SetObject());
            break;
        case CUser_field::TData::e_Objects:
            for (auto sub_obj : field.SetData().SetObjects()) {
                any_change |= CleanupUserObject(*sub_obj);
            }
            break;
        case CUser_field::TData::e_Strs:
            // NOTE: for some reason, using the auto range
            // does not work here
            for (auto str = field.SetData().SetStrs().begin(); str != field.SetData().SetStrs().end(); str++) {
                any_change |= Asn2gnbkCompressSpaces(*str);
                any_change |= CleanVisString(*str);
            }
            break;
        case CUser_field::TData::e_Fields:
            for (auto sub_field : field.SetData().SetFields()) {
                any_change |= x_CleanupUserField(*sub_field);
            }
        default:
            break;
        }
    }
    return any_change;
}


typedef SStaticPair<const char*, const char*>  TOntologyCleanupElem;
static const TOntologyCleanupElem k_ontology_term_cleanup_map[] = {
    {"go id", "GO:"},
    {"go ref", "GO_REF:"}
};
typedef CStaticArrayMap<const char*, const char*, PNocase_CStr> TOntologyCleanupMap;
DEFINE_STATIC_ARRAY_MAP(TOntologyCleanupMap, sc_OntologyTermCleanupMap, k_ontology_term_cleanup_map);

bool CCleanup::s_CleanupGeneOntology(CUser_object& obj)
{
    bool any_change = false;

    // nothing to do if not GeneOntology object
    if (!obj.IsSetType() || !obj.GetType().IsStr() ||
        !NStr::Equal(obj.GetType().GetStr(), "GeneOntology")) {
        return any_change;
    }
    // nothing to do if no fields
    if (!obj.IsSetData()) {
        return any_change;
    }

    static const char * const sc_bsecGoQualType[] = {
        "", "Component", "Function", "Process"
    };
    typedef CStaticArraySet<const char*, PNocase_CStr> TGoQualTypeSet;
    DEFINE_STATIC_ARRAY_MAP( TGoQualTypeSet, sc_GoQualArray, sc_bsecGoQualType );

    for (auto outer_field : obj.SetData()) {
        if (outer_field->IsSetLabel() && outer_field->GetLabel().IsStr() &&
            outer_field->IsSetData() && outer_field->GetData().IsFields()
            && sc_GoQualArray.find(outer_field->GetLabel().GetStr().c_str()) != sc_GoQualArray.end()) {
            for (auto term : outer_field->SetData().SetFields()) {
                CUser_field &field = *term;
                if (field.IsSetData() && field.GetData().IsFields()) {
                    for (auto inner_term_iter : field.SetData().SetFields()) {
                        CUser_field &inner_field = *inner_term_iter;
                        if (inner_field.IsSetLabel() &&
                            inner_field.GetLabel().IsStr() &&
                            inner_field.IsSetData() &&
                            inner_field.GetData().IsStr()) {
                            const string &inner_label = inner_field.GetLabel().GetStr();
                            auto find_term = sc_OntologyTermCleanupMap.find(inner_label.c_str());
                            if (find_term != sc_OntologyTermCleanupMap.end() &&
                                NStr::StartsWith(inner_field.SetData().SetStr(), find_term->second, NStr::eNocase)) {
                                inner_field.SetData().SetStr().erase(0, strlen(find_term->second));
                                any_change = true;
                            }
                        }
                    }
                }
            }
        }
    }

    return any_change;
}



bool CCleanup::s_CleanupStructuredComment( CUser_object &obj )
{
    bool any_change = false;

    if (obj.GetObjectType() != CUser_object::eObjectType_StructuredComment) {
        return any_change;
    }

    any_change |= s_RemoveEmptyFields(obj);

    if (!obj.IsSetData()) {
        return any_change;
    }

    bool genome_assembly_data = false;
    bool ibol_data = false;

    const string kBarcode = "International Barcode of Life (iBOL)Data";

    for (auto field_i : obj.SetData()) {
        CUser_field &field = *field_i;
        if (field.IsSetLabel() && field.GetLabel().IsStr()
            && field.IsSetData() && field.GetData().IsStr()) {
            bool is_prefix = NStr::Equal(field.GetLabel().GetStr(), "StructuredCommentPrefix");
            bool is_suffix = NStr::Equal(field.GetLabel().GetStr(), "StructuredCommentSuffix");
            if (is_prefix || is_suffix) {
                string core = CUtf8::AsUTF8(field.GetData().GetStr(), eEncoding_Ascii);
                CComment_rule::NormalizePrefix(core);
                string new_val = is_prefix ? CComment_rule::MakePrefixFromRoot(core) : CComment_rule::MakeSuffixFromRoot(core);
                if (!NStr::Equal(new_val, field.GetData().GetStr())) {
                    field.SetData().SetStr(new_val);
                    any_change = true;
                }
                if (core == "Genome-Assembly-Data") {
                    genome_assembly_data = true;
                } else if( core == kBarcode ) {
                    ibol_data = true;
                }
            }
        }
    }

    if( genome_assembly_data ) {
        any_change |= s_CleanupGenomeAssembly(obj);
    }

    if( ibol_data ) {
        CConstRef<CComment_set> rules = CComment_set::GetCommentRules();
        if (rules) {
            CConstRef<CComment_rule> ruler = rules->FindCommentRuleEx(kBarcode);
            if (ruler) {
                const CComment_rule& rule = *ruler;
                any_change |= rule.ReorderFields(obj);
            }
        }
    }
    return any_change;
}


typedef SStaticPair<const char*, const char*>  TFinishingCleanupElem;
static const TFinishingCleanupElem k_finishing_cleanup_map[] = {
    {"Annotation Directed", "Annotation-Directed Improvement"},
    {"High Quality Draft", "High-Quality Draft"},
    {"Improved High Quality Draft", "Improved High-Quality Draft"},
    {"Non-contiguous Finished", "Noncontiguous Finished"}
};
typedef CStaticArrayMap<const char*, const char*, PNocase_CStr> TFinishingCleanupMap;
DEFINE_STATIC_ARRAY_MAP(TFinishingCleanupMap, sc_FinishingCleanupMap, k_finishing_cleanup_map);

bool CCleanup::s_CleanupGenomeAssembly(CUser_object& obj)
{
    bool any_change = false;
    for (auto field_i : obj.SetData()) {
        CUser_field &field = *field_i;
        if (!field.IsSetLabel() || !field.GetLabel().IsStr() ||
            !field.IsSetData() || !field.GetData().IsStr()) {
            continue;
        }
        if (field.GetLabel().GetStr() == "Finishing Goal" ||
            field.GetLabel().GetStr()  == "Current Finishing Status" ) {
            auto replace = sc_FinishingCleanupMap.find(field.GetData().GetStr().c_str());
            if (replace != sc_FinishingCleanupMap.end()) {
                field.SetData().SetStr(replace->second);
                any_change = true;
            }
        } else if( field.GetLabel().GetStr() == "Assembly Date" ) {
            string &field_str = field.SetData().SetStr();
            bool ambiguous = false;
            string altered = CSubSource::FixDateFormat (field_str, true, ambiguous);
            if (!NStr::IsBlank(altered)) {
                CRef<CDate> coll_date = CSubSource::DateFromCollectionDate (altered);
                if (coll_date && coll_date->IsStd() && coll_date->GetStd().IsSetYear()) {
                    string day;
                    string month;
                    string year;
                    string new_date;
                    if (!ambiguous && coll_date->GetStd().IsSetDay()) {
                        coll_date->GetDate(&day, "%2D");
                    }
                    if (!ambiguous && coll_date->GetStd().IsSetMonth()) {
                        coll_date->GetDate(&month, "%N");
                        month = month.substr(0, 3);
                        month = NStr::ToUpper(month);
                    }
                    coll_date->GetDate(&year, "%Y");
                    if (!NStr::IsBlank(day)) {
                        new_date += day + "-";
                    }
                    if (!NStr::IsBlank(month)) {
                        new_date += month + "-";
                    }
                    if (!NStr::IsBlank(year)) {
                        new_date += year;
                    }
                    if (!NStr::Equal(field_str, new_date)) {
                        field_str = new_date;
                        any_change = true;
                    }
                }
            }
        }
    }
    return any_change;
}


bool CCleanup::s_RemoveEmptyFields(CUser_object& obj)
{
    bool any_change = false;
    if (obj.GetObjectType() != CUser_object::eObjectType_StructuredComment) {
        return any_change;
    }
    if (!obj.IsSetData()) {
        return any_change;
    }

    CUser_object::TData::iterator it = obj.SetData().begin();
    while (it != obj.SetData().end()) {
        bool is_blank = false;
        if ((*it)->IsSetData()) {
            if ((*it)->GetData().IsStr()) {
                const string& val = (*it)->GetData().GetStr();
                if (NStr::IsBlank(val)) {
                    is_blank = true;
                }
            } else if ((*it)->GetData().Which() == CUser_field::TData::e_not_set) {
                is_blank = true;
            }
        } else {
            is_blank = true;
        }

        if (is_blank) {
            it = obj.SetData().erase(it);
            any_change = true;
        } else {
            ++it;
        }
    }
    return any_change;
}


bool CCleanup::s_CleanupDBLink(CUser_object& obj)
{
    bool changed = false;
    if (obj.GetObjectType() != CUser_object::eObjectType_DBLink) {
        return changed;
    }
    if (!obj.IsSetData()) {
        return changed;
    }
    for (auto& it : obj.SetData()) {
        if (it->IsSetData() && it->GetData().IsStr()) {
            string val = it->GetData().GetStr();
            it->SetData().SetStrs().push_back(val);
            changed = true;
        }
    }
    return changed;
}


bool CCleanup::s_AddNumToUserField(CUser_field &field)
{
    if (!field.IsSetData()) {
        return false;
    }
    bool any_change = false;
    switch (field.GetData().Which()) {
        case CUser_field::TData::e_Strs:
            if (!field.IsSetNum() || field.GetNum() != field.GetData().GetStrs().size()) {
                field.SetNum(field.GetData().GetStrs().size());
                any_change = true;
            }
            break;
         case CUser_field::TData::e_Ints:
            if (!field.IsSetNum() || field.GetNum() != field.GetData().GetInts().size()) {
                field.SetNum(field.GetData().GetInts().size());
                any_change = true;
            }
            break;
        case CUser_field::TData::e_Reals:
            if (!field.IsSetNum() || field.GetNum() != field.GetData().GetReals().size()) {
                field.SetNum(field.GetData().GetReals().size());
                any_change = true;
            }
            break;
        case CUser_field::TData::e_Oss:
            if (!field.IsSetNum() || field.GetNum() != field.GetData().GetOss().size()) {
                field.SetNum(field.GetData().GetOss().size());
                any_change = true;
            }
            break;
        default:
            if (field.IsSetNum() && field.GetNum() != 1) {
                field.SetNum(1);
                any_change = true;
            }
            break;
    }
    return any_change;
}


END_SCOPE(objects)
END_NCBI_SCOPE
